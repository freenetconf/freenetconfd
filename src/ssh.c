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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libssh/libssh.h>
#include <libssh/server.h>

#include <libubox/uloop.h>
#include <libubox/usock.h>

#include "xml.h"
#include "freenetconfd.h"
#include "ssh.h"
#include "messages.h"
#include "config.h"

#define CHUNK_LEN 256

static void ssh_handle_connection(struct uloop_fd *u_fd, __unused unsigned int events);
static void ssh_del_client(struct uloop_process *uproc, int ret);

static struct uloop_fd ssh_srv = { .cb = ssh_handle_connection };

static char *read_buffer = NULL;
static int read_buffer_len = 0;

struct ssh {
	ssh_bind ssh_bind;

	struct uloop_fd ssh_fd;
};

static struct ssh s;
static int netconf_base = 1;

static int
ssh_check_key(ssh_key key_client)
{
	FILE *fd = fopen(config.authorized_keys_file, "r");
	if (!fd) {
		ERROR("unable to open authorized_keys_file: '%s'\n", config.authorized_keys_file);
		return 1;
	}

	char *line = NULL;
	size_t line_len = 0;
	int is_found = 0;
	int rc;

	while (!is_found && getline(&line, &line_len, fd) != -1) {
		char type[32], key[2048];
		sscanf(line, "%s %s ", type, key);

		ssh_key key_server = NULL;
		rc = ssh_pki_import_pubkey_base64(key, ssh_key_type_from_name(type), &key_server);
		if (rc != SSH_OK) continue;

		rc = ssh_key_cmp(key_client, key_server, SSH_KEY_CMP_PUBLIC);
		if (!rc) is_found = 1;

		ssh_key_free(key_server);
	}

	free(line);
	fclose(fd);

	return !is_found;
}

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

		if (bytes_read <= 0) {
			DEBUG("ssh_channel_read: %d\n", (int)bytes_read);
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
 * @int:	uloop flags (unused)
 *
 * Called on every new connection. Creates new ssh session and processes
 * messages if everything is valid.
 */
static void ssh_cb(struct uloop_fd *fd, unsigned int flags)
{
	ssh_session ssh_session;
	ssh_message ssh_message = NULL;
	ssh_channel ssh_channel = NULL;
	int rc;

	DEBUG("initializing session\n");
	ssh_session = ssh_new();
	if (!ssh_session) {
		uloop_end();
		return;
	}

	/* TODO: review this again */
	static int session_id = 0;
	session_id++;

	/* configure timeout */
	ssh_options_set(ssh_session, SSH_OPTIONS_TIMEOUT, &config.ssh_timeout_socket);

	DEBUG("accepting connection\n");
	rc = ssh_bind_accept_fd(s.ssh_bind, ssh_session, ssh_bind_get_fd(s.ssh_bind));
	if (rc != SSH_OK) goto free_ssh;

	DEBUG("exchanging keys\n");
	rc = ssh_handle_key_exchange(ssh_session);
	if (rc != SSH_OK) goto free_ssh;

	/* configure authentication methods */
	int auth_method = SSH_AUTH_METHOD_UNKNOWN;

	if (config.username && config.password)
		auth_method |= SSH_AUTH_METHOD_PASSWORD;

	if (config.authorized_keys_file)
		auth_method |= SSH_AUTH_METHOD_PUBLICKEY;

	ssh_set_auth_methods(ssh_session, auth_method);

	DEBUG("trying to authenticate\n");
	int is_authenticated = 0;
	do {
		ssh_message = ssh_message_get(ssh_session);
		if (!ssh_message) goto free_ssh;

		rc = SSH_ERROR;

		if (config.authorized_keys_file
		    && ssh_message_subtype(ssh_message) == SSH_AUTH_METHOD_PUBLICKEY)
		{
			ssh_key key_client = NULL;
			key_client = ssh_message_auth_pubkey(ssh_message);
			if (!key_client) goto free_ssh;

			if (ssh_check_key(key_client) == 0) {
				is_authenticated = 1;
				rc = ssh_message_auth_reply_success(ssh_message, 0);
			} else
				rc = ssh_message_reply_default(ssh_message);
		} else if (config.username && config.password
			   && ssh_message_subtype(ssh_message) == SSH_AUTH_METHOD_PASSWORD
			   && !strcmp(ssh_message_auth_user(ssh_message), config.username)
			   && !strcmp(ssh_message_auth_password(ssh_message), config.password))
		{
			is_authenticated = 1;
			rc = ssh_message_auth_reply_success(ssh_message, 0);
		} else
			rc = ssh_message_reply_default(ssh_message);

		ssh_message_free(ssh_message);
		ssh_message = NULL;

		if (rc == SSH_ERROR) goto free_ssh;
	} while (!is_authenticated);

	if (!is_authenticated) goto free_ssh;
	DEBUG("authentication completed\n");

	DEBUG("trying to setup channel\n");
	do {
		ssh_message = ssh_message_get(ssh_session);
		if (!ssh_message) goto free_ssh;

		if (ssh_message_subtype(ssh_message) == SSH_CHANNEL_SESSION)
			ssh_channel = ssh_message_channel_request_open_reply_accept(ssh_message);

		if (!ssh_channel) {
			/* no reason to check error message here */
			ssh_message_reply_default(ssh_message);
			goto free_ssh;
		}

		ssh_message_free(ssh_message);
		ssh_message = NULL;
	} while (!ssh_channel);

	if (!ssh_channel) goto free_ssh;
	DEBUG("channel setup completed\n");

	DEBUG("requesting netconf subsystem\n");
	int is_netconf = 0;
	do {
		ssh_message = ssh_message_get(ssh_session);
		if (!ssh_message) goto free_ssh;

		if ((ssh_message_subtype(ssh_message) == SSH_CHANNEL_REQUEST_SUBSYSTEM)
			&& !strcmp(ssh_message_channel_request_subsystem(ssh_message), "netconf"))
		{
			is_netconf = 1;
			rc = ssh_message_channel_request_reply_success(ssh_message);
		} else
			rc = ssh_message_reply_default(ssh_message);

		ssh_message_free(ssh_message);
		ssh_message = NULL;

		if (rc == SSH_ERROR) goto free_ssh;
	} while (!is_netconf);

	if (!is_netconf) goto free_ssh;
	DEBUG("netconf subsystem received\n");

	char *xml_msg_in = NULL;
	char *xml_msg_out = NULL;

	DEBUG("reading <hello>\n");
	rc = netconf_read(&ssh_channel, &xml_msg_in, 1);
	if (rc) goto free_xml;

	rc = xml_analyze_message_hello(xml_msg_in, &netconf_base);
	if (rc) goto free_xml;

	DEBUG("sending <hello>\n");
	rc = netconf_write(&ssh_channel, XML_NETCONF_HELLO , 1);
	if (rc) goto free_xml;

	int rp = 0;
	while(!rp && !rc) {
		free(xml_msg_in);
		free(xml_msg_out);
		xml_msg_in = NULL;
		xml_msg_out = NULL;

		DEBUG("reading message\n");
		rc = netconf_read(&ssh_channel, &xml_msg_in, 0);
		if (rc) break;

		printf("'%s'\n",xml_msg_in);

		rp = xml_handle_message_rpc(xml_msg_in, &xml_msg_out);

		DEBUG("sending message\n");
		rc = netconf_write(&ssh_channel, xml_msg_out, 0);
	}

free_xml:
	free(read_buffer);
	read_buffer = NULL;
	read_buffer_len = 0;
	free(xml_msg_in);
	free(xml_msg_out);

free_ssh:
	if (ssh_message) ssh_message_free(ssh_message);
	ssh_disconnect(ssh_session);
	if (ssh_session) ssh_free(ssh_session);

	DEBUG("session closed\n");
	uloop_end();
}

static void
ssh_handle_connection(struct uloop_fd *u_fd, __unused unsigned int events)
{
	int client_fd;
	while ((client_fd = accept(u_fd->fd, NULL, NULL)) >= 0 || errno != EWOULDBLOCK) {
		if (client_fd < 0)
			continue;

		LOG("received incoming connection\n");

		struct uloop_process *uproc = calloc(1, sizeof(*uproc));
		if (!uproc || (uproc->pid = fork()) == -1) {
			free(uproc);
			close(client_fd);
		}

		if (uproc->pid != 0) { /* parent */
			/* register an event handler for when the child terminates */
			uproc->cb = ssh_del_client;
			uloop_process_add(uproc);
			close(client_fd);
		} else { /* child */
			s.ssh_bind = ssh_bind_new();
			if (!s.ssh_bind) {
				ERROR("not enough memory\n");
				uloop_end();
				return;
			}

			ssh_bind_options_set(s.ssh_bind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &config.log_level);
			ssh_bind_options_set(s.ssh_bind, SSH_BIND_OPTIONS_HOSTKEY, config.host_rsa_key);
			ssh_bind_options_set(s.ssh_bind, SSH_BIND_OPTIONS_HOSTKEY, config.host_dsa_key);

			ssh_bind_set_blocking(s.ssh_bind, 1);
			ssh_bind_set_fd(s.ssh_bind, client_fd);

			s.ssh_fd.cb = ssh_cb;
			s.ssh_fd.fd = client_fd;

			uloop_fd_add(&s.ssh_fd, ULOOP_READ | ULOOP_WRITE | ULOOP_EDGE_TRIGGER);
		}
	}
}

void
ssh_reverse_connect(char *user, char *fingerprint, char *host, char *port)
{
	LOG("initiating reverse ssh connection\n");

	struct uloop_process *uproc = calloc(1, sizeof(*uproc));
	if (!uproc || (uproc->pid = fork()) == -1) {
		free(uproc);
		return;
	}

	if (uproc->pid != 0) { /* parent */
		/* register an event handler for when the child terminates */
		uproc->cb = ssh_del_client;
		uloop_process_add(uproc);
		return;
	}

	/* child */

	int fd = 0;

	if ((fd = usock(USOCK_TCP, host, port)) < 0) {
		ERROR("connect failed %d\n", fd);
		uloop_end();
		return;
	}

	s.ssh_bind = ssh_bind_new();
	if (!s.ssh_bind) {
		ERROR("not enough memory\n");
		uloop_end();
		return;
	}

	ssh_bind_options_set(s.ssh_bind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &config.log_level);
	ssh_bind_options_set(s.ssh_bind, SSH_BIND_OPTIONS_HOSTKEY, config.host_rsa_key);
	ssh_bind_options_set(s.ssh_bind, SSH_BIND_OPTIONS_HOSTKEY, config.host_dsa_key);

	ssh_bind_set_blocking(s.ssh_bind, 1);
	ssh_bind_set_fd(s.ssh_bind, fd);

	s.ssh_fd.cb = ssh_cb;
	s.ssh_fd.fd = fd;

	uloop_fd_add(&s.ssh_fd, ULOOP_READ | ULOOP_WRITE | ULOOP_EDGE_TRIGGER);
}

static void
ssh_del_client(struct uloop_process *uproc, int ret)
{
	free(uproc);
	LOG("child processing ssh request has died\n");
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

	/* generate host keys they do not exist */
	if (config.host_rsa_key && access(config.host_rsa_key, F_OK) == -1) {
		LOG("key doesn't exist: creating %s...\n", config.host_rsa_key);
		ssh_key host_rsa_key;
		rc = ssh_pki_generate(SSH_KEYTYPE_RSA, 4096, &host_rsa_key);
		if (rc == SSH_OK) {
			rc = ssh_pki_export_privkey_file(host_rsa_key, "", NULL, NULL, config.host_rsa_key);
			if (rc != SSH_OK) ERROR("unable to save key to file\n");
			free(host_rsa_key);
		}
	}

	if (config.host_dsa_key && access(config.host_dsa_key, F_OK) == -1) {
		LOG("key doesn't exist: creating %s...\n", config.host_dsa_key);
		ssh_key host_dsa_key;
		rc = ssh_pki_generate(SSH_KEYTYPE_DSS, 4096, &host_dsa_key);
		if (rc == SSH_OK) {
			rc = ssh_pki_export_privkey_file(host_dsa_key, "", NULL, NULL, config.host_dsa_key);
			if (rc != SSH_OK) ERROR("unable to save key to file\n");
			free(host_dsa_key);
		}
	}

	ssh_srv.fd = usock(USOCK_TCP | USOCK_SERVER | USOCK_NONBLOCK, config.addr, config.port);
	if (ssh_srv.fd < 0) {
		ERROR("Unable to bind socket: %s", strerror(errno));
		return -1;
	}

	uloop_fd_add(&ssh_srv, ULOOP_READ | ULOOP_EDGE_TRIGGER);

	return 0;
}

void
ssh_netconf_exit(void)
{
	ssh_bind_free(s.ssh_bind);
	ssh_finalize();

	return;
}
