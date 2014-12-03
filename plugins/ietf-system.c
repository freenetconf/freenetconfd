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

datastore_t root = {"root",NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,1,0,0};

char *get_hostname(datastore_t *node)
{
	char *rc = malloc(9);
	strncpy(rc, "HOSTNAME", 9);
	return rc;
}

// FIXME: this is only a prototype
void generic_update(datastore_t *node)
{
	if (node) printf("Datastore node UPDATE\t%s: %s\n", node->name, node->value);
}

int create_store()
{
	// ietf-system
	datastore_t *system = ds_add_child_create(&root, "system", NULL, ns, NULL, 0);
	system->update = generic_update;

	datastore_t *location = ds_add_child_create(system, "location", "Zagreb", NULL, NULL, 0); // string
	location->update = generic_update;
	datastore_t *hostname = ds_add_child_create(system, "hostname", "localhost", NULL, NULL, 0); // string
// 	hostname->get = get_hostname;
	hostname->update = generic_update;
	datastore_t *contact = ds_add_child_create(system, "contact", "yes, please", NULL, NULL, 0); // string
	datastore_t *clock = ds_add_child_create(system, "clock", NULL, NULL, NULL, 0);
	datastore_t *ntp = ds_add_child_create(system, "ntp", NULL, NULL, NULL, 0);

	// clock
	datastore_t *timezone_location = ds_add_child_create(clock, "timezone-location", "Europe/Zagreb", NULL, NULL, 0); // string
	timezone_location->update = generic_update;
	datastore_t *timezone_utc_offset = ds_add_child_create(clock, "timezone-utc-offset", "60", NULL, NULL, 0); // int16

	// ntp
	datastore_t *enabled = ds_add_child_create(ntp, "enabled", "false", NULL, NULL, 0); // bool

	// server list
	for (int i = 1; i < 3; i++) {
		//server
		datastore_t *server = ds_add_child_create(ntp, "server", NULL, NULL, NULL, 0);
		server->is_list = 1;
		char server_name[BUFSIZ];
		snprintf(server_name, BUFSIZ, "server%d", i);
		datastore_t *name = ds_add_child_create(server, "name", server_name, NULL, NULL, 0); // string
		name->is_key = 1;

		datastore_t *association_type = ds_add_child_create(server, "association-type", NULL, NULL, NULL, 0);
		ds_set_is_config(association_type, 0, 0);
		datastore_t *iburst = ds_add_child_create(server, "iburst", "false", NULL, NULL, 0);
		ds_set_is_config(iburst, 0, 0);
		datastore_t *prefer = ds_add_child_create(server, "prefer", "false", NULL, NULL, 0);
		ds_set_is_config(prefer, 0, 0);

		// udp
		datastore_t *udp = ds_add_child_create(server, "udp", NULL, NULL, NULL, 0);
		ds_add_child_create(udp, "address", "127.0.0.1", NULL, NULL, 0); // inet-addr
		ds_add_child_create(udp, "port", "8088", NULL, NULL, 0); // int16
	}

	datastore_t *dns_resolver = ds_add_child_create(system, "dns-resolver", NULL, NULL, NULL, 0);
	ds_add_child_create(dns_resolver, "search", "localhost1", NULL, NULL, 0)->is_list = 1;
	ds_add_child_create(dns_resolver, "search", "localhost2", NULL, NULL, 0)->is_list = 1;

	for (int i = 1; i < 3; i++) {
		datastore_t *server = ds_add_child_create(dns_resolver, "server", NULL, NULL, NULL, 0);
		char server_name[BUFSIZ];
		snprintf(server_name, BUFSIZ, "server%d", i);
		datastore_t *name = ds_add_child_create(server, "name", server_name, NULL, NULL, 0); // string
		name->is_key = 1;

		// udp
		datastore_t *udp_and_tcp = ds_add_child_create(server, "udp-and-tcp", NULL, NULL, NULL, 0);
		ds_add_child_create(udp_and_tcp, "address", "127.0.0.1", NULL, NULL, 0); // inet-addr
		ds_add_child_create(udp_and_tcp, "port", "8088", NULL, NULL, 0); // int16
	}

	datastore_t *options = ds_add_child_create(dns_resolver, "options", NULL, NULL, NULL, 0);
	ds_add_child_create(options, "timeout", "5", NULL, NULL, 0);
	ds_add_child_create(options, "attempts", "2", NULL, NULL, 0);


	// ietf-system-state
	datastore_t *system_state = ds_add_child_create(&root, "system-state", NULL, ns, NULL, 0);
	ds_set_is_config(system_state, 0, 0);

	datastore_t *platform  = ds_add_child_create(system_state, "platform", NULL, ns, NULL, 0);

	datastore_t *os_name = ds_add_child_create(platform, "os-name", "The awesome Linux", ns, NULL, 0);
	datastore_t *os_release = ds_add_child_create(platform, "os-release", "Top noch", ns, NULL, 0);
	datastore_t *os_version = ds_add_child_create(platform, "os-version", "latest", ns, NULL, 0);
	datastore_t *machine = ds_add_child_create(platform, "machine", "x86_64", ns, NULL, 0);

	datastore_t *clock_state = ds_add_child_create(system_state, "clock", NULL, ns, NULL, 0);

	datastore_t *current_datetime = ds_add_child_create(clock_state, "current_datetime", "now", ns, NULL, 0);
	datastore_t *boot_datetime = ds_add_child_create(clock_state, "boot-datetime", "before", ns, NULL, 0);
	return 0;
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

	DEBUG("rpc\n");
	
	roxml_add_node(data->out, 0, ROXML_ELM_NODE, "data", NULL);
	
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

	if( !WIFEXITED(reboot_status) || WEXITSTATUS(reboot_status) != 0)
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

	if( !WIFEXITED(shutdown_status) || WEXITSTATUS(shutdown_status) != 0)
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
	create_store();

	m.rpcs = rpc;
	m.rpc_count = (sizeof(rpc) / sizeof(*(rpc))); // to be filled directly by code generator
	m.ns = ns;
	m.datastore = &root;

	return &m;
}

void destroy()
{
	ds_free(root.child, 1);
	root.child = NULL;
}