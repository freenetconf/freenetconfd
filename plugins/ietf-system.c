/*
 * Copyright (C) 2014 Cisco Systems, Inc.
 *
 * Author: Petar Koretic <petar.koretic@sartura.hr>
 * Author: Zvonimir Fras <zvonimir.fras@sartura.hr>
 * Author: Luka Perkov <luka.perkov@sartura.hr>
 *
 * freenetconfd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with freenetconfd. If not, see <http://www.gnu.org/licenses/>.
 */

#include <freenetconfd/plugin.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "../src/datastore.h"
#include "../src/freenetconfd.h"

struct module m;
char *ns = "urn:ietf:params:xml:ns:yang:ietf-system";

datastore_t root = {"root",NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,0};

int create_store()
{
	datastore_t *system = ds_add_child_create(&root, "system", NULL, ns);

	datastore_t *location = ds_add_child_create(system, "location", "Zagreb", NULL); // string
	datastore_t *hostname = ds_add_child_create(system, "hostname", "localhost", NULL); // string
	datastore_t *contact = ds_add_child_create(system, "contact", "yes, please", NULL); // string
	datastore_t *clock = ds_add_child_create(system, "clock", NULL, NULL);
	datastore_t *ntp = ds_add_child_create(system, "ntp", NULL, NULL);

	// clock
	datastore_t *timezone_location = ds_add_child_create(clock, "timezone-location", "Europe/Zagreb", NULL); // string
	datastore_t *timezone_utc_offset = ds_add_child_create(clock, "timezone-utc-offset", "60", NULL); // int16

	// ntp
	datastore_t *enabled = ds_add_child_create(ntp, "enabled", "true", NULL); // bool

	// server list
	for (int i = 1; i < 3; i++) {
		//server
		datastore_t *server = ds_add_child_create(ntp, "server", NULL, NULL);
		server->is_list = 1;
		char server_name[BUFSIZ];
		snprintf(server_name, BUFSIZ, "server%d", i);
		datastore_t *name = ds_add_child_create(server, "name", server_name, NULL); // string
		name->is_key = 1;

		ds_add_child_create(server, "association-type", NULL, NULL);
		ds_add_child_create(server, "iburst", "false", NULL);
		ds_add_child_create(server, "prefer", "false", NULL);

		// udp
		datastore_t *udp = ds_add_child_create(server, "udp", NULL, NULL);
		ds_add_child_create(udp, "address", "127.0.0.1", NULL); // inet-addr
		ds_add_child_create(udp, "port", "8088", NULL); // int16
	}

	datastore_t *dns_resolver = ds_add_child_create(system, "dns-resolver", NULL, NULL);
	for (int i = 1; i < 2; i++) {
		ds_add_child_create(dns_resolver, "search", "localhost", NULL);
	}

	for (int i = 1; i < 3; i++) {
		datastore_t *server = ds_add_child_create(dns_resolver, "server", NULL, NULL);
		char server_name[BUFSIZ];
		snprintf(server_name, BUFSIZ, "server%d", i);
		datastore_t *name = ds_add_child_create(server, "name", server_name, NULL); // string
		name->is_key = 1;

		// udp
		datastore_t *udp_and_tcp = ds_add_child_create(server, "udp-and-tcp", NULL, NULL);
		ds_add_child_create(udp_and_tcp, "address", "127.0.0.1", NULL); // inet-addr
		ds_add_child_create(udp_and_tcp, "port", "8088", NULL); // int16
	}

	datastore_t *options = ds_add_child_create(dns_resolver, "options", NULL, NULL);
	ds_add_child_create(options, "timeout", "5", NULL);
	ds_add_child_create(options, "attempts", "2", NULL);

	return 0;
}

// get
int get(struct rpc_data *data)
{
	create_store();

	node_t *ro_root = data->in;
	char *ro_root_name = roxml_get_name(ro_root, NULL, 0);

	// client requested get all
	if (ro_root_name && !strcmp("get", ro_root_name)) {
		ds_get_all(root.child, data->out, 1);

		ds_free(root.child);
		root.child = NULL;

		return RPC_DATA;
	}

	// client requested filtered get
	datastore_t *our_root = ds_find_child(&root, ro_root_name);
	ds_get_filtered(ro_root, our_root, data->out);

	ds_free(root.child);
	root.child = NULL;

	return RPC_DATA;
}

// get-config
int get_config(struct rpc_data *data)
{
	return RPC_DATA;
}

// edit-config
int edit_config(struct rpc_data *data)
{
	return RPC_OK;
}

// RPC
int rpc_set_current_datetime(struct rpc_data *data)
{
	/* TODO: use settimeofday */

	char cmd[BUFSIZ];
	char *date = roxml_get_content(roxml_get_chld(data->in, "current-datetime", 0), NULL, 0, NULL);

	snprintf(cmd, BUFSIZ, "date -s %s", date);

	if (system(cmd) == -1)
		return RPC_ERROR;

	return RPC_DATA;
}

int rpc_system_restart(struct rpc_data *data)
{
	pid_t reboot_pid;
	if( 0 == (reboot_pid = fork()) ) {
		reboot(LINUX_REBOOT_CMD_CAD_OFF);
		exit(1); /* never reached if reboot cmd succeeds */
	}

	if(reboot_pid < 0)
		return RPC_ERROR;

	int reboot_status;
	waitpid(reboot_pid, &reboot_status, 0);

	if( !WIFEXITED(reboot_status) || WIFEXITSTATUS(reboot_status) != 0)
		return RPC_ERROR;

	return RPC_DATA;
}

int rpc_system_shutdown(struct rpc_data *data)
{
	pid_t shutdown_pid;
	if( 0 == (shutdown_pid = fork()) ) {
		sync();
		reboot(LINUX_REBOOT_CMD_HALT);
		exit(1); /* never reached if reboot cmd succeeds */
	}

	if(shutdown_pid < 0)
		return RPC_ERROR;

	int shutdown_status;
	waitpid(shutdown_pid, &shutdown_status, 0);

	if( !WIFEXITED(shutdown_status) || WIFEXITSTATUS(shutdown_status) != 0)
		return RPC_ERROR;

	return RPC_DATA;
}

struct rpc_method rpc[] = {
	{"set-current-datetime", rpc_set_current_datetime},
	{"system-restart", rpc_system_restart},
	{"system-shutdown", rpc_system_shutdown},
};

struct module *init()
{
	m.get = get;
	m.get_config = get_config;
	m.edit_config = edit_config;
	m.rpcs = rpc;
	m.rpc_count = (sizeof(rpc) / sizeof(*(rpc))); // to be filled directly by code generator
	m.namespace = ns;

	return &m;
}