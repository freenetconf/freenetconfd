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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libubox/uloop.h>

#include "freenetconfd.h"
#include "config.h"
#include "ssh.h"
#include "ubus.h"

int
main(int argc, char **argv)
{
	int rc = 0;

	rc = config_load();
	if (rc) {
		ERROR("configuration loading failed\n");
		goto exit;
	}

	rc = uloop_init();
	if (rc) {
		ERROR("uloop init failed\n");
		goto exit;
	}

	rc = ssh_netconf_init();
	if (rc) {
		ERROR("ssh init failed\n");
		goto exit;
	}

	rc = ubus_init();
	if (rc) {
		ERROR("ubus init failed\n");
		goto exit;
	}

	LOG("%s is accepting connections on '%s:%s'\n", PROJECT_NAME, config.addr, config.port);

	/* main loop */
	uloop_run();

	rc = EXIT_SUCCESS;
exit:
	ssh_netconf_exit();

	uloop_done();

	ubus_exit();

	config_exit();

	return rc;
}
