/*
 * Copyright (C) 2014 Cisco Systems, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#include <libubox/uloop.h>
#include <libubox/usock.h>
#include <libubox/ustream.h>

#include "config.h"
#include "freenetconfd.h"
#include "netconf.h"

static void server_cb(struct uloop_fd *fd, unsigned int events);

static struct uloop_fd server = { .cb = server_cb };
struct connection *next_connection = NULL;

struct connection {
	struct sockaddr_in sin;
	struct ustream_fd us;
};

static void notify_state(struct ustream *s)
{
	struct connection *c = container_of(s, struct connection, us.stream);

	ustream_free(&c->us.stream);
	close(c->us.fd.fd);

	free(c);

	LOG("connection closed\n");
}

static void notify_read(struct ustream *s, int bytes)
{
	// struct connection *c = container_of(s, struct connection, us.stream);

	char *data;
	int len;

	do {
		data = ustream_get_read_buf(s, &len);
		if (!data)
			break;

		printf("%.*s", len, data);

		ustream_consume(s, len);
	} while (1);
}

static void server_cb(struct uloop_fd *fd, unsigned int events)
{
	struct connection *c;
	unsigned int sl = sizeof(struct sockaddr_in);
	int fd;

	if (!next_connection)
		next_connection = calloc(1, sizeof(*next_connection));

	c = next_connection;
	fd = accept(server.fd, (struct sockaddr *) &c->sin, &sl);
	if (fd < 0) {
		LOG("failed accepting connection\n");
		return;
	}

	c->us.stream.string_data = true;
	c->us.stream.notify_read = notify_read;
	c->us.stream.notify_state = notify_state;

	ustream_fd_init(&c->us, fd);
	next_connection = NULL;

	LOG("received new connection\n");
}

int
netconf_init()
{
	server.fd = usock(USOCK_TCP | USOCK_SERVER, config.addr, config.port);
	if (server.fd < 0) {
		ERROR("unable to open socket %s:%s\n", config.addr, config.port);
		return -1;
	}

	uloop_fd_add(&server, ULOOP_READ);

	return 0;
}
