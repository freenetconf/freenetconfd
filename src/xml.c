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

#include "xml.h"
#include "freenetconfd.h"
#include "messages.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#endif

struct rpc_data {
	char *message_id;
	node_t *in;
	node_t *out;
};

static int xml_handle_get(struct rpc_data *data);
static int xml_handle_get_config(struct rpc_data *data);
static int xml_handle_edit_config(struct rpc_data *data);
static int xml_handle_copy_config(struct rpc_data *data);
static int xml_handle_commit(struct rpc_data *data);
static int xml_handle_cancel_commit(struct rpc_data *data);
static int xml_handle_discard_changes(struct rpc_data *data);
static int xml_handle_delete_config(struct rpc_data *data);
static int xml_handle_lock(struct rpc_data *data);
static int xml_handle_unlock(struct rpc_data *data);
static int xml_handle_close_session(struct rpc_data *data);
static int xml_handle_kill_session(struct rpc_data *data);

struct rpc_method {
	const char *name;
	int (*handler) (struct rpc_data *data);
};

const struct rpc_method rpc_methods[] = {
	{ "get", xml_handle_get },
	{ "get-config", xml_handle_get_config },
	{ "edit-config", xml_handle_edit_config },
	{ "copy-config", xml_handle_copy_config },
	{ "delete-config", xml_handle_delete_config },
	{ "commit", xml_handle_commit },
	{ "cancel-commit", xml_handle_cancel_commit },
	{ "discard-changes", xml_handle_discard_changes },
	{ "lock", xml_handle_lock },
	{ "unlock", xml_handle_unlock },
	{ "close-session", xml_handle_close_session },
	{ "kill-session", xml_handle_kill_session },
};

/*
 * xml_analyze_message_hello() - analyze rpc hello message
 *
 * @char*:	xml message for parsing
 * @int:	rpc 'base' of the message we deduce from message
 *
 * Checks if rpc message is a valid hello message and parse rcp base version
 * client supports.
 */
int xml_analyze_message_hello(char *xml_in, int *base)
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
	for (int i = 0; i< num_nodes; i++) {
		if (!nodes[i]) continue;
		char *value = roxml_get_content(nodes[i], NULL, 0, NULL);
		if (strcmp(value, "urn:ietf:params:netconf:base:1.1") == 0) {
			tbase = 1;
		}
		else if(strcmp(value, "urn:ietf:params:netconf:base:1.0") == 0) {
			tbase = 0;
		}
	}

	if(tbase == -1)
		goto exit;

	*base = tbase;

	rc = 0;
exit:
	roxml_release(RELEASE_ALL);
	roxml_close(root);

	return rc;
}

/*
 * xml_handle_message - handle all rpc messages
 *
 * @char*:	xml message for parsing
 * @char**:	xml message we create for response
 *
 * Get netconf method from rpc message and call apropriate rpc method which
 * will parse and return response message.
 */
int xml_handle_message_rpc(char *xml_in, char **xml_out)
{
	int rc = -1;
	char *message_id = 0;
	char *operation_name = 0;
	struct rpc_data data = {};

	node_t *root = roxml_load_buf(xml_in);
	if (!root) goto exit;

	node_t *rpc = roxml_get_chld(root, NULL, 0);
	if (!rpc) goto exit;

	node_t *rpc_message_id = roxml_get_attr(rpc, "message-id", 0);
	if (!rpc_message_id) goto exit;

	message_id = roxml_get_content(rpc_message_id, NULL, 0, NULL);
	if (!message_id) goto exit;

	node_t *operation = roxml_get_chld(rpc, NULL, 0);
	if (!operation) goto exit;

	operation_name = roxml_get_name(operation, NULL, 0);

	DEBUG("received rpc '%s'\n", operation_name);

	const struct rpc_method *method = NULL;
	for (int i = 0; i < ARRAY_SIZE(rpc_methods); i++) {
		if (!strcmp(operation_name, rpc_methods[i].name)) {
			method = &rpc_methods[i];
			break;
		}
	}

	if (!method) goto exit;

	data.message_id = message_id;
	data.in = operation;
	data.out = NULL;

	rc = method->handler(&data);

exit:
	if (data.out) {
		roxml_commit_changes(data.out, NULL, xml_out, 0);
		roxml_close(data.out);
	}

	roxml_release(RELEASE_ALL);
	roxml_close(root);

	return rc;
}

static int
xml_handle_get(struct rpc_data *data)
{
	data->out = roxml_load_buf(XML_NETCONF_REPLY_TEMPLATE);
	node_t *root = roxml_get_chld(data->out, NULL, 0);
	roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", data->message_id);
	roxml_add_node(root, 0, ROXML_ELM_NODE, "data", NULL);

	return 0;
}

static int
xml_handle_get_config(struct rpc_data *data)
{
	data->out = roxml_load_buf(XML_NETCONF_REPLY_TEMPLATE);
	node_t *root = roxml_get_chld(data->out, NULL, 0);
	roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", data->message_id);
	roxml_add_node(root, 0, ROXML_ELM_NODE, "data", NULL);

	return 0;
}

static int
xml_handle_edit_config(struct rpc_data *data)
{
	data->out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	node_t *root = roxml_get_chld(data->out, NULL, 0);
	roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", data->message_id);

	return 0;
}

static int
xml_handle_copy_config(struct rpc_data *data)
{
	data->out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	node_t *root = roxml_get_chld(data->out, NULL, 0);
	roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", data->message_id);

	return 0;
}

static int
xml_handle_delete_config(struct rpc_data *data)
{
	data->out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	node_t *root = roxml_get_chld(data->out, NULL, 0);
	roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", data->message_id);

	return 0;
}

static int
xml_handle_lock(struct rpc_data *data)
{
	data->out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	node_t *root = roxml_get_chld(data->out, NULL, 0);
	roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", data->message_id);

	return 0;
}

static int
xml_handle_unlock(struct rpc_data *data)
{
	data->out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	node_t *root = roxml_get_chld(data->out, NULL, 0);
	roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", data->message_id);

	return 0;
}

static int
xml_handle_close_session(struct rpc_data *data)
{
	data->out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	node_t *root = roxml_get_chld(data->out, NULL, 0);
	roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", data->message_id);

	return 1;
}

static int
xml_handle_kill_session(struct rpc_data *data)
{
	data->out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	node_t *root = roxml_get_chld(data->out, NULL, 0);
	roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", data->message_id);

	return 1;
}

static int xml_handle_commit(struct rpc_data *data)
{
	return 0;
}

static int xml_handle_cancel_commit(struct rpc_data *data)
{
	return 0;
}

static int xml_handle_discard_changes(struct rpc_data *data)
{
	return 0;
}
