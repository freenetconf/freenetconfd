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

#ifndef __FREENETCONFD_PLUGIN_H__
#define __FREENETCONFD_PLUGIN_H__

#include <freenetconfd/datastore.h>

#include <libubox/list.h>
#include <roxml.h>

enum response {RPC_OK, RPC_OK_CLOSE, RPC_DATA, RPC_ERROR, RPC_DATA_EXISTS, RPC_DATA_MISSING};

struct rpc_data
{
	node_t *in;
	node_t *out;
	char *error;
	int get_config;
};

struct rpc_method
{
	// rpc name for rpc_methods, xpath otherwise
	const char *query;
	int (*handler) (struct rpc_data *data);
};

struct module
{
	const struct rpc_method *rpcs;
	int rpc_count;
	char *ns;
	struct datastore *datastore;
};

struct module_list
{
	struct list_head list;
	char *name;
	void *lib;
	const struct module *m;
};

#endif /* __FREENETCONFD_PLUGIN_H__ */
