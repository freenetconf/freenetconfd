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

#include <libubox/uloop.h>

#include "freenetconfd.h"
#include "ssh.h"
#include "config.h"
#include "dmconfig.h"

int
main(int argc, char **argv)
{
	int rc = 0;

	rc = config_load();
	if (rc) {
		printf("configuration loading failed\n");
		return -1;
	}

	rc = uloop_init();
	if (rc) {
		printf("uloop init failed\n");
		return -1;
	}

	rc = ssh_netconf_init();
	if (rc) {
		printf("ssh init failed\n");
		return -1;
	}

	rc = dm_init();
	if (rc) {
		fprintf(stderr, "mand init failed\n");
		return -1;
	}

	uloop_run();

	dm_shutdown();

	ssh_netconf_exit();

	uloop_done();

	config_exit();

	return EXIT_SUCCESS;
}
