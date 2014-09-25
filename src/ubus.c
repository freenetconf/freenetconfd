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

static struct ubus_context *ubus = NULL;
static struct ubus_object main_object;

static const struct ubus_method fnd_methods[] = {
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
