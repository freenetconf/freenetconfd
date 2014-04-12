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
#include <string.h>
#include <roxml.h>
#include <unistd.h>
#include "messages.h"
#include "xml.h"
#include "dmconfig.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#endif

static int xml_handle_get(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_get_config(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_edit_config(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_copy_config(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_commit(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_cancel_commit(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_discard_changes(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_delete_config(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_lock(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_unlock(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_close_session(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_kill_session(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_system_restart(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_system_shutdown(char *message_id, node_t *xml_in, char **xml_out);
static int xml_handle_set_current_datetime(char *message_id, node_t *xml_in, char **xml_out);

struct rpc_method {
	const char *name;
	int (*handler) (char *message_id, node_t *xml_in, char **xml_out);
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
	{ "system-restart", xml_handle_system_restart },
	{ "system-shutdown", xml_handle_system_shutdown },
	{ "set-current-datetime", xml_handle_set_current_datetime }
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

	printf(":: got rpc %s\n", operation_name);

	const struct rpc_method *method = NULL;
	for (int i = 0; i < ARRAY_SIZE(rpc_methods); i++) {
		if (!strcmp(operation_name, rpc_methods[i].name)) {
			method = &rpc_methods[i];
			break;
		}
	}

	if (!method) goto exit;

	rc = method->handler(message_id, operation, xml_out);

exit:
	roxml_release(RELEASE_ALL);
	roxml_close(root);

	return rc;
}

static int
xml_handle_get(char *message_id, node_t *xml_in, char **xml_out)
{
	return xml_handle_get_config(message_id, xml_in, xml_out);
}

static int
xml_handle_get_config(char *message_id, node_t *xml_in, char **xml_out)
{
	int rc = -1;
	char *config = NULL;
	node_t *doc_out = NULL;


	/* construct message with mand returned data */
	doc_out = roxml_load_buf(XML_NETCONF_REPLY_TEMPLATE);
	if (!doc_out) goto exit;

	node_t *root = roxml_get_chld(doc_out, NULL, 0);
	if (!root) goto exit;

	node_t *attr = roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", message_id);
	if (!attr) goto exit;

	node_t *data = roxml_add_node(root, 0, ROXML_ELM_NODE, "data", NULL);
	if (!data) goto exit;

	/* if no filter return all */
	/* if filter but empty - return nothing */
	node_t *nfilter = roxml_get_chld(xml_in, "filter", 0);
	node_t *ns = roxml_get_chld(nfilter, NULL, 0);

	dm_get_xml_config(nfilter, nfilter, &data);

	rc = roxml_commit_changes(doc_out, NULL, xml_out, 0);
	if (rc) rc = 0;

exit:

	free(config);
	roxml_close(doc_out);

	return rc;
}

static int
xml_handle_edit_config(char *message_id, node_t *xml_in, char **xml_out)
{
	char *category_name  = NULL;
	node_t *doc_out = NULL;

	node_t *target = roxml_get_chld(xml_in, "target", 0);
	if (!target) goto exit;

	node_t *config = roxml_get_chld(xml_in, "config", 0);
	if(!config) goto exit;

	node_t *category = roxml_get_chld(config, NULL, 0);
	if (!category) goto exit;

	category_name = roxml_get_name(category, NULL, 0);

	printf(":: got %s\n", category_name);

	if (dm_set_parameters_from_xml(category, category))
		goto exit;

	doc_out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	if (!doc_out) goto exit;

	node_t *root = roxml_get_chld(doc_out, NULL, 0);
	if (!root) goto exit;

	node_t *attr = roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", message_id);
	if (!attr) goto exit;

	roxml_commit_changes(doc_out, NULL, xml_out, 0);

exit:
	roxml_close(doc_out);
	return 0;
}

static int
xml_handle_copy_config(char *message_id, node_t *xml_in, char **xml_out)
{
	return 0;
}

static int
xml_handle_delete_config(char *message_id, node_t *xml_in, char **xml_out)
{
	return 0;
}

static int
xml_handle_lock(char *message_id, node_t *xml_in, char **xml_out)
{
	node_t *doc_out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	if (!doc_out) goto exit;

	node_t *root = roxml_get_chld(doc_out, NULL, 0);
	if (!root) goto exit;

	node_t *attr = roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", message_id);
	if (!attr) goto exit;

	roxml_commit_changes(doc_out, NULL, xml_out, 0);

exit:
	roxml_close(doc_out);
	return 0;
}

static int
xml_handle_unlock(char *message_id, node_t *xml_in, char **xml_out)
{
	node_t *doc_out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	if (!doc_out) goto exit;

	node_t *root = roxml_get_chld(doc_out, NULL, 0);
	if (!root) goto exit;

	node_t *attr = roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", message_id);
	if (!attr) goto exit;

	roxml_commit_changes(doc_out, NULL, xml_out, 0);

exit:
	roxml_close(doc_out);
	return 0;
}

static int
xml_handle_close_session(char *message_id, node_t *xml_in, char **xml_out)
{
	node_t *doc_out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	if (!doc_out) goto exit;

	node_t *root = roxml_get_chld(doc_out, NULL, 0);
	if (!root) goto exit;

	node_t *attr = roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", message_id);
	if (!attr) goto exit;

	roxml_commit_changes(doc_out, NULL, xml_out, 0);

exit:
	roxml_close(doc_out);

	return 1;
}

static int
xml_handle_kill_session(char *message_id, node_t *xml_in, char **xml_out)
{
	node_t *doc_out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	if (!doc_out) goto exit;

	node_t *root = roxml_get_chld(doc_out, NULL, 0);
	if (!root) goto exit;

	node_t *attr = roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", message_id);
	if (!attr) goto exit;

	roxml_commit_changes(doc_out, NULL, xml_out, 0);

exit:
	roxml_close(doc_out);

	return 1;
}

static int xml_handle_commit(char *message_id, node_t *xml_in, char **xml_out)
{
	return 0;
}

static int xml_handle_cancel_commit(char *message_id, node_t *xml_in, char **xml_out)
{
	return 0;
}

static int xml_handle_discard_changes(char *message_id, node_t *xml_in, char **xml_out)
{
	node_t *doc_out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	if (!doc_out) goto exit;

	node_t *root = roxml_get_chld(doc_out, NULL, 0);
	if (!root) goto exit;

	node_t *attr = roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", message_id);
	if (!attr) goto exit;

	roxml_commit_changes(doc_out, NULL, xml_out, 0);

exit:
	roxml_close(doc_out);
	return 0;
}

/* TODO: send reply first */
static int xml_handle_system_restart(char *message_id, node_t *xml_in, char **xml_out)
{
	node_t *doc_out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	if (!doc_out) goto exit;

	node_t *root = roxml_get_chld(doc_out, NULL, 0);
	if (!root) goto exit;

	node_t *attr = roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", message_id);
	if (!attr) goto exit;

	roxml_commit_changes(doc_out, NULL, xml_out, 0);


exit:
	roxml_close(doc_out);

	char* const argv[1] = {NULL};
	execvp("/sbin/reboot", argv);
	return 0;
}

static int xml_handle_system_shutdown(char *message_id, node_t *xml_in, char **xml_out)
{
	node_t *doc_out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
	if (!doc_out) goto exit;

	node_t *root = roxml_get_chld(doc_out, NULL, 0);
	if (!root) goto exit;

	node_t *attr = roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", message_id);
	if (!attr) goto exit;

	roxml_commit_changes(doc_out, NULL, xml_out, 0);

	char* const argv[1] = {NULL};
	execvp("/sbin/poweroff", argv);

exit:
	roxml_close(doc_out);
	return 0;
}

static int xml_handle_set_current_datetime(char *message_id, node_t *xml_in, char **xml_out)
{
	char *value = "";
	node_t *doc_out = NULL;

	int rc = dm_set_current_datetime(value);

	/* ntp is active */
	if (rc) {
		doc_out = roxml_load_buf(XML_NETCONF_REPLY_ERROR_TEMPLATE);
		if (!doc_out) goto exit;

		node_t *root = roxml_get_chld(doc_out, NULL, 0);
		if (!root) goto exit;

		node_t *attr = roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", message_id);
		if (!attr) goto exit;

		node_t *ntype = roxml_add_node(root, 0, ROXML_ELM_NODE, "error-type", "application");
		node_t *ntag = roxml_add_node(root, 0, ROXML_ELM_NODE, "error-tag", "operation-failed");
		node_t *napptag = roxml_add_node(root, 0, ROXML_ELM_NODE, "error-app-tag", "ntp-active");
		if (!ntype || !ntag || !napptag) goto exit;
	}
	/* ok */
	else {
		doc_out = roxml_load_buf(XML_NETCONF_REPLY_OK_TEMPLATE);
		if (!doc_out) goto exit;

		node_t *root = roxml_get_chld(doc_out, NULL, 0);
		if (!root) goto exit;

		node_t *attr = roxml_add_node(root, 0, ROXML_ATTR_NODE, "message-id", message_id);
		if (!attr) goto exit;
	}

	roxml_commit_changes(doc_out, NULL, xml_out, 0);

exit:
	roxml_close(doc_out);
	return 0;
}
