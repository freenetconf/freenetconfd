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
#include <string.h>
#include <roxml.h>
#include <stdint.h>

#include "freenetconfd/freenetconfd.h"
#include "freenetconfd/datastore.h"

#include "methods.h"
#include "messages.h"
#include "config.h"
#include "modules.h"
#include "netconf.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#endif

static int get(struct rpc_data *data, datastore_t *datastore);
static int method_handle_get(struct rpc_data *data);
static int method_handle_get_config(struct rpc_data *data);
static int method_handle_edit_config(struct rpc_data *data);
static int method_handle_copy_config(struct rpc_data *data);
static int method_handle_delete_config(struct rpc_data *data);
static int method_handle_lock(struct rpc_data *data);
static int method_handle_unlock(struct rpc_data *data);
static int method_handle_close_session(struct rpc_data *data);
static int method_handle_kill_session(struct rpc_data *data);
static int method_handle_get_schema(struct rpc_data *data);

const struct rpc_method rpc_methods[] =
{
	{ "get", method_handle_get },
	{ "get-config", method_handle_get_config },
	{ "get-schema", method_handle_get_schema },
	{ "edit-config", method_handle_edit_config },
	{ "copy-config", method_handle_copy_config },
	{ "delete-config", method_handle_delete_config },
	{ "lock", method_handle_lock },
	{ "unlock", method_handle_unlock },
	{ "close-session", method_handle_close_session },
	{ "kill-session", method_handle_kill_session },
};

/*
 * method_analyze_message_hello() - analyze rpc hello message
 *
 * @char*:	xml message for parsing
 * @int*:	netconf 'base' we deduce from message
 *
 * Checks if rpc message is a valid hello message and parse rcp base version
 * client supports.
 */
int method_analyze_message_hello(char *xml_in, int *base)
{
	int rc = -1, num_nodes = 0;
	node_t **nodes;
	int tbase = -1;

	node_t *root = roxml_load_buf(xml_in);

	if (!root) goto exit;

	node_t *hello = roxml_get_nodes(root, ROXML_ELM_NODE, "hello", 0);

	if (!hello) goto exit;

	/* rfc: must not have */
	node_t *session_id = roxml_get_nodes(hello, ROXML_ELM_NODE, "session-id", 0);

	if (session_id) goto exit;

	nodes = roxml_xpath(root, "//capabilities/capability", &num_nodes);

	for (int i = 0; i < num_nodes; i++)
	{
		if (!nodes[i]) continue;

		char *value = roxml_get_content(nodes[i], NULL, 0, NULL);

		if (strcmp(value, "urn:ietf:params:netconf:base:1.1") == 0)
		{
			tbase = 1;
		}
		else if (strcmp(value, "urn:ietf:params:netconf:base:1.0") == 0)
		{
			tbase = 0;
		}
	}

	if (tbase == -1)
		goto exit;

	*base = tbase;

	rc = 0;

exit:

	roxml_release(RELEASE_ALL);
	roxml_close(root);

	return rc;
}

int method_create_message_hello(char **xml_out)
{
	int rc = -1, len;
	char c_session_id[BUFSIZ];
	static uint32_t session_id = 0;

	/* prevent variable overflow */
	if (++session_id == 0)
		session_id = 1;

	node_t *root = roxml_load_buf(XML_NETCONF_HELLO);

	if (!root)
	{
		ERROR("unable to load 'netconf hello' message template\n");
		goto exit;
	}

	node_t *n_hello = roxml_get_chld(root, NULL, 0);

	if (!n_hello)
	{
		ERROR("unable to parse 'netconf hello' message template\n");
		goto exit;
	}

	len = snprintf(c_session_id, BUFSIZ, "%d", session_id);

	if (len <= 0)
	{
		ERROR("unable to convert session_id\n");
		goto exit;
	}

	node_t *n_session_id = roxml_add_node(n_hello, 0, ROXML_ELM_NODE, "session-id", c_session_id);

	if (!n_session_id)
	{
		ERROR("unable to add session id node\n");
		goto exit;
	}

	len = roxml_commit_changes(root, NULL, xml_out, 0);

	if (len <= 0)
	{
		ERROR("unable to create 'netconf hello' message\n");
		goto exit;
	}

	rc = 0;

exit:

	roxml_close(root);

	return rc;
}

/*
 * method_handle_message - handle all rpc messages
 *
 * @char*:	xml message for parsing
 * @char**:	xml message we create for response
 *
 * Get netconf method from rpc message and call apropriate rpc method which
 * will parse and return response message.
 */
int method_handle_message_rpc(char *xml_in, char **xml_out)
{
	int rc = -1;
	char *operation_name = NULL;
	char *ns = NULL;
	struct rpc_data data = { NULL, NULL, NULL, 0};

	node_t *root_in = roxml_load_buf(xml_in);

	if (!root_in) goto exit;

	node_t *rpc_in = roxml_get_chld(root_in, NULL, 0);

	if (!rpc_in) goto exit;

	node_t *operation = roxml_get_chld(rpc_in, NULL, 0);

	if (!operation) goto exit;

	node_t *n_ns = roxml_get_ns(operation);

	if (!n_ns) goto exit;

	operation_name = roxml_get_name(operation, NULL, 0);
	ns = roxml_get_content(n_ns, NULL, 0, NULL);

	if (!operation_name || !ns)
	{
		ERROR("unable to extract rpc and namespace\n");
		goto exit;
	}

	DEBUG("received rpc '%s' (%s)\n", operation_name, ns);

	data.out = roxml_load_buf(XML_NETCONF_REPLY_TEMPLATE);
	node_t *rpc_out = roxml_get_chld(data.out, NULL, 0);

	/* copy all arguments from rpc to rpc-reply */
	int args = roxml_get_attr_nb(rpc_in);

	for (int i = 0; i < args; i++)
	{

		int flags = ROXML_ATTR_NODE;
		node_t *n_arg = roxml_get_attr(rpc_in, NULL, i);

		char *name = roxml_get_name(n_arg, NULL, 0);

		// default namespace
		if (!strcmp(name, ""))
			flags |= ROXML_NS_NODE;

		char *value = roxml_get_content(n_arg, NULL, 0, NULL);

		roxml_add_node(rpc_out, 0, flags, name, value);
	}

	data.in = operation;
	data.out = rpc_out;

	const struct rpc_method *method = NULL;

	for (int i = 0; i < ARRAY_SIZE(rpc_methods); i++)
	{
		if (!strcmp(operation_name, rpc_methods[i].query))
		{
			method = &rpc_methods[i];
			break;
		}
	}

	/* process modules */
	if (!method)
	{
		int found = 0;
		struct list_head *modules = get_modules();
		struct module_list *elem;
		list_for_each_entry(elem, modules, list)
		{
			if (found) break;

			DEBUG("module: %s\n", elem->name);

			for (int i = 0; i < elem->m->rpc_count; i++)
			{
				if (!strcmp(elem->m->rpcs[i].query, operation_name) &&
					!strcmp(elem->m->ns, ns))
				{
					DEBUG("method found in module: %s (%s)\n", elem->m->rpcs[i].query, elem->m->ns);
					method = &elem->m->rpcs[i];
					found = 1;
					break;
				}
			}
		}
	}

	if (!method)
	{
		ERROR("method not supported\n");
		data.error = netconf_rpc_error("method not supported", RPC_ERROR_TAG_OPERATION_NOT_SUPPORTED, 0, 0);
		rc = RPC_ERROR;
	}
	else
	{
		rc = method->handler(&data);
	}

	switch (rc)
	{
		case RPC_OK:
			roxml_add_node(data.out, 0, ROXML_ELM_NODE, "ok", NULL);
			rc = 0;
			break;

		case RPC_OK_CLOSE:
			roxml_add_node(data.out, 0, ROXML_ELM_NODE, "ok", NULL);
			rc = 1;
			break;

		case RPC_DATA:
			rc = 0;
			break;

		case RPC_ERROR:
			if (!data.error)
				data.error = netconf_rpc_error("UNKNOWN ERROR", 0, 0, 0);

			roxml_add_node(data.out, 0, ROXML_ELM_NODE, "rpc_error", data.error);

			free(data.error);
			data.error = NULL;

			rc = 0;
			break;

		case RPC_DATA_EXISTS:
			if (!data.error)
				data.error = netconf_rpc_error("Data exists!", RPC_ERROR_TAG_DATA_EXISTS, RPC_ERROR_TYPE_RPC, RPC_ERROR_SEVERITY_ERROR);

			roxml_add_node(data.out, 0, ROXML_ELM_NODE, "rpc_error", data.error);

			free(data.error);
			data.error = NULL;

			rc = 0;
			break;

		case RPC_DATA_MISSING:
			if (!data.error)
				data.error = netconf_rpc_error("Data missing!", RPC_ERROR_TAG_DATA_MISSING, RPC_ERROR_TYPE_RPC, RPC_ERROR_SEVERITY_ERROR);

			roxml_add_node(data.out, 0, ROXML_ELM_NODE, "rpc_error", data.error);

			free(data.error);
			data.error = NULL;

			rc = 0;
			break;
	}

exit:

	if (data.out)
	{
		roxml_commit_changes(data.out, NULL, xml_out, 0);
		roxml_close(data.out);
	}

	roxml_release(RELEASE_ALL);
	roxml_close(root_in);

	return rc;
}

static int
method_handle_get(struct rpc_data *data)
{
	node_t *n_data = roxml_add_node(data->out, 0, ROXML_ELM_NODE, "data", NULL);

	int nb = 0;
	node_t **n_filter, *n;

	struct list_head *modules = get_modules();
	struct module_list *elem;

	if ((n_filter = roxml_xpath(data->in, "//filter", &nb)))
	{
		nb = roxml_get_chld_nb(n_filter[0]);

		/* empty filter */
		if (!nb)
			return RPC_DATA;

		while (--nb >= 0)
		{
			n = roxml_get_chld(n_filter[0], NULL, nb);
			char *module = roxml_get_name(n, NULL, 0);
			char *ns = roxml_get_content(roxml_get_ns(n), NULL, 0, NULL);
			DEBUG("filter for module: %s (%s)\n", module, ns);

			list_for_each_entry(elem, modules, list)
			{
				DEBUG("module: %s\n", elem->name);

				if (!strcmp(ns, elem->m->ns))
				{
					DEBUG("calling module: %s (%s) \n", module, ns);
					struct rpc_data d = {n, n_data, NULL, 0};

					get(&d, elem->m->datastore);

					break;
				}
			}
		}
	}
	else
	{
		DEBUG("no filter requested, processing all modules\n");
		list_for_each_entry(elem, modules, list)
		{
			DEBUG("calling module: %s\n", elem->name);
			n = data->in;
			struct rpc_data d = {n, n_data, NULL, 0};
			get(&d, elem->m->datastore);
		}
	}

	return RPC_DATA;
}

static int get(struct rpc_data *data, datastore_t *datastore)
{
	node_t *ro_root = data->in;
	char *ro_root_name = roxml_get_name(ro_root, NULL, 0);

	// client requested get all
	if (ro_root_name && !strcmp("get", ro_root_name))
	{
		ds_get_all(datastore->child, data->out, data->get_config, 1);

		return RPC_DATA;
	}

	// client requested filtered get
	datastore_t *our_root = ds_find_child(datastore, ro_root_name, NULL);
	ds_get_filtered(ro_root, our_root, data->out, data->get_config);

	return RPC_DATA;
}


static int
method_handle_get_config(struct rpc_data *data)
{
	// TODO: merge with get
	return method_handle_get(data);
}

static int
method_handle_edit_config(struct rpc_data *data)
{
	node_t *config = roxml_get_chld(data->in, "config", 0);

	if (!config) return RPC_ERROR;

	struct list_head *modules = get_modules();
	struct module_list *elem;

	int rc = RPC_OK;

	int child_count = roxml_get_chld_nb(config);

	for (int i = 0; i < child_count; i++)
	{
		node_t *cur = roxml_get_chld(config, NULL, i);

		char *module = roxml_get_name(cur, NULL, 0);
		char *ns = roxml_get_content(roxml_get_ns(cur), NULL, 0, NULL);

		DEBUG("edit_config for module: %s (%s)\n", module, ns);

		list_for_each_entry(elem, modules, list)
		{
			DEBUG("module: %s\n", elem->name);

			if (!strcmp(ns, elem->m->ns))
			{
				DEBUG("calling module: %s (%s) \n", module, ns);
				rc = ds_edit_config(cur, elem->m->datastore->child, NULL);
				break;
			}
		}
	}

	return rc;
}

static int
method_handle_copy_config(struct rpc_data *data)
{
	return RPC_OK;
}

static int
method_handle_delete_config(struct rpc_data *data)
{
	return RPC_OK;
}

static int
method_handle_lock(struct rpc_data *data)
{
	return RPC_OK;
}

static int
method_handle_unlock(struct rpc_data *data)
{
	return RPC_OK;
}

static int
method_handle_close_session(struct rpc_data *data)
{
	return RPC_OK_CLOSE;
}

static int
method_handle_kill_session(struct rpc_data *data)
{
	return RPC_OK_CLOSE;
}

static int method_handle_get_schema(struct rpc_data *data)
{
	FILE *yang_module = NULL;
	char yang_module_filename[BUFSIZ];
	char *yang_module_content = NULL;
	long yang_module_size;
	node_t *n_identifier, *n_version, *n_format;
	char *c_identifier, *c_version, *c_format;
	char *xml_entities[5][2] = {{"&", "&amp;"},
		{"\"", "&quot;"},
		{"\'", "&apos;"},
		{"<", "&lt;"},
		{">", "&gt;"}
	};

	if (!config.yang_dir)
	{
		ERROR ("yang dir not specified\n");
		goto exit;
	}

	LOG("yang directory: %s\n", config.yang_dir);

	n_identifier = roxml_get_chld(data->in, "identifier", 0);
	c_identifier = roxml_get_content(n_identifier, NULL, 0, NULL);

	if (!n_identifier || !c_identifier)
	{
		ERROR("yang module identifier not specified\n");
		goto exit;
	}

	n_version = roxml_get_chld(data->in, "version", 0);
	c_version = roxml_get_content(n_version, NULL, 0, NULL);

	/* 'yang' format if ommited */
	n_format = roxml_get_chld(data->in, "format", 0);
	c_format = roxml_get_content(n_format, NULL, 0, NULL);

	DEBUG("yang format:%s\n", c_format);

	/* TODO: return rpc-error */
	if (n_format && c_format && !strstr(c_format, "yang"))
	{
		ERROR("yang format not valid or supported\n");
		goto exit;
	}

	snprintf(yang_module_filename, BUFSIZ, "%s/%s", config.yang_dir, c_identifier);

	if (c_version)
	{
		snprintf(yang_module_filename + strlen(yang_module_filename), BUFSIZ, "@%s", c_version);
	}

	strncat(yang_module_filename, ".yang", 5);

	DEBUG("yang filename:%s\n", yang_module_filename);

	yang_module = fopen (yang_module_filename, "rb");

	if (!yang_module)
	{
		ERROR("yang module:%s not found\n", yang_module_filename);
		goto exit;
	}

	fseek (yang_module, 0, SEEK_END);
	yang_module_size = ftell (yang_module);
	fseek (yang_module, 0, SEEK_SET);
	yang_module_content = malloc (yang_module_size);

	if (!yang_module_content)
	{
		ERROR("unable to load yang module\n");
		goto exit;
	}

	/* escape xml from yang module */
	int c, pos = 0;

	while ((c = fgetc(yang_module)) != EOF)
	{
		char ch = (char) c;
		int escape_found = 0;

		for (int i = 0; i < 5; i++)
		{
			char r = xml_entities[i][0][0];
			char *s = xml_entities[i][1];
			int len = strlen(s);

			if (r == ch)
			{
				yang_module_size += len;
				yang_module_content = realloc(yang_module_content, yang_module_size);
				strncpy(yang_module_content + pos, s, len);
				pos += len;
				escape_found = 1;
				break;
			}
		}

		if (!escape_found)
			yang_module_content[pos++] = ch;
	}

	yang_module_content[pos] = 0;

	node_t *n_schema = roxml_add_node(data->out, 0, ROXML_ELM_NODE, "data", yang_module_content);

	if (!n_schema)
	{
		ERROR("unable to add data node\n");
		goto exit;
	}

	node_t *n_attr = roxml_add_node(n_schema, 0, ROXML_ATTR_NODE, "xmlns", "urn:ietf:params:xml:ns:yang:ietf-netconf-monitoring");

	if (!n_attr)
	{
		ERROR("unable to set attribute\n");
		goto exit;
	}

exit:

	if (yang_module)
		fclose(yang_module);

	free(yang_module_content);

	return RPC_DATA;
}
