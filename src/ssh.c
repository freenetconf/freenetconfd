/*
 * Copyright (C) 2014 Sartura, Ltd.
 *
 * Author: Luka Perkov <luka.perkov@sartura.hr>
 * Author: Petar Koretic <petar.koretic@sartura.hr>
 *
 * freenetconfd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with freenetconfd. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libssh/libssh.h>
#include <libssh/server.h>

#include <libubox/uloop.h>

#include "xml.h"
#include "freenetconfd.h"
#include "ssh.h"
#include "messages.h"
#include "config.h"

#define CHUNK_LEN 256

static char *read_buffer = NULL;
static int read_buffer_len = 0;

struct ssh {
	ssh_bind ssh_bind;

	struct uloop_fd ssh_fd;
};

static struct ssh s;
static int netconf_base = 1;

/*
 * netconf_read() - read and return netconf rpc messages
 *
 * @ssh_channel:	ssh_channel from which we read data
 * @char**:		message which we return
 * @int:		is it hello mesage or rpc
 *
 * Check if there is one complete message in buffer and return it if there is,
 * otherwise read new chunks from channel to global buffer until there is end
 * of message. Message is removed from buffer when returned. Hello message
 * always uses base 1.0 ending.
 */
static int
netconf_read(ssh_channel *ssh_channel, char **buffer, int hello_message)
{
	int rc = 1;
	ssize_t bytes_read;
	size_t read = 0;
	char *end_tag = NULL;
	char *end_tag_str = (netconf_base && !hello_message) ? XML_NETCONF_BASE_1_1_END : XML_NETCONF_BASE_1_0_END;

	if (read_buffer)
		end_tag = strstr(read_buffer, end_tag_str);

	while(!end_tag) {
		read_buffer = realloc(read_buffer, read_buffer_len + read + CHUNK_LEN + 1);
		if (!read_buffer) goto exit;
		memset(read_buffer + read_buffer_len + read, 0, CHUNK_LEN + 1);

		bytes_read = ssh_channel_read_timeout(*ssh_channel, read_buffer + read_buffer_len + read, CHUNK_LEN, 0, config.ssh_timeout_read);

		if (bytes_read < 0) {
			fprintf(stderr, "ssh_channel_read: %d\n", (int)bytes_read);
			goto exit;
		}

		read += bytes_read;

		end_tag = strstr(read_buffer, end_tag_str);
	};

	read = end_tag - read_buffer;
	*buffer = calloc(read + 1, 1);
	strncpy(*buffer, read_buffer, read);

	char *rest = read_buffer + read + strlen(end_tag_str);
	read_buffer_len = strlen(rest);

	memmove(read_buffer, rest, read_buffer_len);
	read_buffer[read_buffer_len] = 0;

	rc = 0;

exit:
	return rc;
}

/*
 * netconf_write() - write netconf rpc message to ssh channel
 *
 * @ssh_channel:	ssh_channel where to write
 * @char**:		message to write
 * @int:		is it hello mesage or rpc
 *
 * If message is valid, append proper ending and write to socket.
 */
static int
netconf_write(ssh_channel *ssh_channel, const char *buf, int hello_message)
{
	if(!buf) return 0;

	int rc = 1;
	size_t completed = 0;
	int msg_len = 0;
	char *msg = NULL;

	/* base 1.1 */
	if (netconf_base && !hello_message) {
		msg_len = asprintf(&msg, "\n#%zu\n%s%s", strlen(buf), buf, XML_NETCONF_BASE_1_1_END);
	}
	/* base 1_0 */
	else {
		msg_len = asprintf(&msg, "%s%s", buf, XML_NETCONF_BASE_1_0_END);
	}

	if (msg_len < 0) goto exit;

	do {
		rc = ssh_channel_write(*ssh_channel, msg, msg_len);

		if (rc == SSH_ERROR) {
			fprintf(stderr, "ssh_channel_write: %d\n", rc);
			goto exit;
		}

		completed += rc;

	} while (rc > 0 && completed < msg_len);

	rc = 0;

exit:
	free(msg);
	return rc;
}

/*
 * ssh_cb() - ssh uloop callback
 *
 * @uloop_fd*:	uloop socket
 * @int:		uloop flags (unused)
 *
 * Called on every new connection. Creates new ssh session and processes
 * messages if everything is valid.
 */
static void ssh_cb(struct uloop_fd *fd, unsigned int flags)
{
	ssh_session ssh_session;
	ssh_message ssh_message;
	ssh_channel ssh_channel = 0;
	static int session_id = 0;

	int rc;

	ssh_session = ssh_new();
	if (!ssh_session) {
		fprintf(stderr, "ssh_cb: not enough memory (ssh_new)\n");
		return;
	}
	session_id++;

	ssh_options_set(ssh_session, SSH_OPTIONS_TIMEOUT, &config.ssh_timeout_socket);

	rc = ssh_bind_accept(s.ssh_bind, ssh_session);
	if (rc != SSH_OK) {
		fprintf(stderr, "ssh_cb: error accepting a connection (ssh_bind_accept: %s)\n", ssh_get_error(s.ssh_bind));
		ssh_free(ssh_session);
		return;
	}

	rc = ssh_handle_key_exchange(ssh_session);
	if (rc != SSH_OK) {
		fprintf(stderr, "ssh_cb: error exchanging keys (ssh_handle_key_exchange: %s)\n", ssh_get_error(s.ssh_bind));
		ssh_disconnect(ssh_session);
		ssh_free(ssh_session);
		return;
	}

	ssh_set_auth_methods(ssh_session, SSH_AUTH_METHOD_PUBLICKEY | SSH_AUTH_METHOD_PASSWORD);

	int is_authenticated = 0;
	ssh_key key_client = NULL,
	key_server = NULL;

	do {
		ssh_message = ssh_message_get(ssh_session);

		if (!ssh_message) break;

		if (ssh_message_subtype(ssh_message) == SSH_AUTH_METHOD_PUBLICKEY) {
			key_client = ssh_message_auth_pubkey(ssh_message);
			if (!key_client) break;

			ssh_pki_import_pubkey_file(config.client_key_pub, &key_server);
			if (!key_server)break;

			rc = ssh_key_cmp(key_client,key_server, SSH_KEY_CMP_PUBLIC);
			if (rc) break;

			ssh_key_free(key_server);

			is_authenticated = 1;
			rc = ssh_message_auth_reply_success(ssh_message, 0);
		} else if (ssh_message_subtype(ssh_message) == SSH_AUTH_METHOD_PASSWORD
		    && !strcmp(ssh_message_auth_user(ssh_message), config.username)
		    && !strcmp(ssh_message_auth_password(ssh_message), config.password))
		{
			is_authenticated = 1;
			rc = ssh_message_auth_reply_success(ssh_message, 0);
		} else 
			rc = ssh_message_reply_default(ssh_message);

		ssh_message_free(ssh_message);

		if (rc == SSH_ERROR) {
			fprintf(stderr, "error sending message\n");
			ssh_key_free(key_server);
			ssh_free(ssh_session);
			ssh_disconnect(ssh_session);
			return;
		}

	} while (ssh_message && !is_authenticated);

	if (!is_authenticated) {
		fprintf(stderr, "error authenticating\n");
		ssh_key_free(key_server);
		ssh_free(ssh_session);
		ssh_disconnect(ssh_session);
		return;
	}

	printf(":: got authenticated\n");

	do {
		ssh_message = ssh_message_get(ssh_session);

		if (!ssh_message) break;

		if (ssh_message_subtype(ssh_message) == SSH_CHANNEL_SESSION) {
			ssh_channel = ssh_message_channel_request_open_reply_accept(ssh_message);
		}

		if (!ssh_channel) {
			rc = ssh_message_reply_default(ssh_message);
		}

		ssh_message_free(ssh_message);

		if (rc == SSH_ERROR) {
			fprintf(stderr, "error sending message\n");
			ssh_free(ssh_session);
			ssh_disconnect(ssh_session);
			return;
		}

	} while (ssh_message && !ssh_channel);

	if (!ssh_channel) {
		fprintf(stderr, "error in channel\n");
		ssh_free(ssh_session);
		ssh_disconnect(ssh_session);
		return;
	}

	printf(":: got channel\n");

	int is_netconf = 0;
	do {
		ssh_message = ssh_message_get(ssh_session);

		if (!ssh_message) break;

		if ((ssh_message_subtype(ssh_message) == SSH_CHANNEL_REQUEST_SUBSYSTEM)
		    && !strcmp(ssh_message_channel_request_subsystem(ssh_message), "netconf"))
		{
			is_netconf = 1;
			rc = ssh_message_channel_request_reply_success(ssh_message);
		} else {
			rc = ssh_message_reply_default(ssh_message);
		}

		ssh_message_free(ssh_message);

		if (rc == SSH_ERROR) {
			fprintf(stderr, "error sending message\n");
			ssh_free(ssh_session);
			ssh_disconnect(ssh_session);
			return;
		}

	} while (ssh_message && !is_netconf);

	if (!is_netconf) {
		fprintf(stderr, "not in netconf subsystem\n");
		ssh_free(ssh_session);
		ssh_disconnect(ssh_session);
		return;
	}

	printf(":: got netconf\n");

	printf("-- reading <hello>\n");
	char *xml_message_in = NULL,
		*xml_message_out = NULL;

	rc = netconf_read(&ssh_channel, &xml_message_in, 1);
	if (rc) goto shutdown;

	rc = xml_analyze_message_hello(xml_message_in, &netconf_base);
	if (rc) goto shutdown;

	printf("-- sending <hello>\n");
	rc = netconf_write(&ssh_channel, XML_NETCONF_HELLO , 1);
	if (rc) goto shutdown;

	int rp = 0;

	while(!rp && !rc) {
		free(xml_message_in);
		free(xml_message_out);
		xml_message_in = NULL;
		xml_message_out = NULL;

		printf("-- reading message\n");

		rc = netconf_read(&ssh_channel, &xml_message_in, 0);
		if (rc) break;

		printf("'%s'\n",xml_message_in);

		rp = xml_handle_message_rpc(xml_message_in, &xml_message_out);

		printf("-- sending message\n");
		rc = netconf_write(&ssh_channel, xml_message_out, 0);
	}

shutdown:
	printf(":: session closed\n");
	free(read_buffer);
	read_buffer = NULL;
	read_buffer_len = 0;
	free(xml_message_in);
	free(xml_message_out);
	ssh_channel_free(ssh_channel);
	ssh_free(ssh_session);
	ssh_disconnect(ssh_session);

	return;
}

/*
 * ssh_netconf_init() - init ssh uloop
 *
 * Start ssh connection and bind callback to uloop.
 */
int
ssh_netconf_init(void)
{
	int rc = 0;

	s.ssh_bind = ssh_bind_new();
	if (!s.ssh_bind) {
		fprintf(stderr, "ssh_netconf_init: not enough memory (ssh_bind_new)\n");
		return -1;
	}

	ssh_bind_options_set(s.ssh_bind, SSH_BIND_OPTIONS_BINDADDR, config.addr);
	ssh_bind_options_set(s.ssh_bind, SSH_BIND_OPTIONS_BINDPORT_STR, config.port);
	ssh_bind_options_set(s.ssh_bind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &config.log_level);
	ssh_bind_options_set(s.ssh_bind, SSH_BIND_OPTIONS_RSAKEY, config.host_rsa_key);
	ssh_bind_options_set(s.ssh_bind, SSH_BIND_OPTIONS_DSAKEY, config.host_dsa_key);

	ssh_bind_set_blocking(s.ssh_bind, 0);

	rc = ssh_bind_listen(s.ssh_bind);
	if (rc < 0) {
		fprintf(stderr, "ssh_netconf_init: error listening on socket (ssh_bind_listen: %s)\n", ssh_get_error(s.ssh_bind));
		ssh_bind_free(s.ssh_bind);
		ssh_finalize();
		return -1;
	}

	rc = ssh_bind_get_fd(s.ssh_bind);
	if (!rc) {
		fprintf(stderr, "ssh_netconf_init: error listening on socket (ssh_bind_get_fd: %s)\n", ssh_get_error(s.ssh_bind));
		ssh_bind_free(s.ssh_bind);
		ssh_finalize();
		return -1;
	}

	s.ssh_fd.cb = ssh_cb;
	s.ssh_fd.fd = ssh_bind_get_fd(s.ssh_bind);

	uloop_fd_add(&s.ssh_fd, ULOOP_READ | ULOOP_EDGE_TRIGGER);
	return 0;
}

void
ssh_netconf_exit(void)
{
	ssh_bind_free(s.ssh_bind);
	ssh_finalize();

	return;
}
