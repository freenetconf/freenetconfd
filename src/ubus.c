/*
 * Copyright (C) 2014 Sartura, Ltd.
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
#include <unistd.h>
#include <libubus.h>

#include "ubus.h"

#include "freenetconfd.h"
#include "ssh.h"

static struct ubus_context *ubus = NULL;
static struct ubus_object main_object;

/* reverse ssh */
enum connect {
	CONNECT_HOST,
	CONNECT_PORT,
	__CONNECT_MAX
};

static const struct blobmsg_policy connect_policy[] = {
	[CONNECT_HOST] = { .name = "host", .type = BLOBMSG_TYPE_STRING },
	[CONNECT_PORT] = { .name = "port", .type = BLOBMSG_TYPE_STRING },
};

static int
fnd_handle_connect(struct ubus_context *ctx, struct ubus_object *obj,
		   struct ubus_request_data *req, const char *method,
		   struct blob_attr *msg)
{
	struct blob_attr *tb[__CONNECT_MAX];

	blobmsg_parse(connect_policy, ARRAY_SIZE(connect_policy), tb,
		      blob_data(msg), blob_len(msg));

	if (!tb[CONNECT_HOST])
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (!tb[CONNECT_PORT])
		return UBUS_STATUS_INVALID_ARGUMENT;

	ssh_reverse_connect(blobmsg_data(tb[CONNECT_HOST]), blobmsg_data(tb[CONNECT_PORT]));

	return 0;
}

static const struct ubus_method fnd_methods[] = {
	UBUS_METHOD("connect", fnd_handle_connect, connect_policy),
};

static struct ubus_object_type main_object_type =
	UBUS_OBJECT_TYPE("freenetconfd", fnd_methods);

static struct ubus_object main_object = {
	.name = "netconf",
	.type = &main_object_type,
	.methods = fnd_methods,
	.n_methods = ARRAY_SIZE(fnd_methods),
};

int
ubus_init(void)
{
	ubus = ubus_connect(NULL);
	if (!ubus) return -1;

	ubus_add_uloop(ubus);

	if (ubus_add_object(ubus, &main_object)) return -1;

	return 0;
}

void
ubus_exit(void)
{
	if (ubus) ubus_free(ubus);
}
