#include <inttypes.h>
#include <time.h>
#include "dmconfig.h"

/* max integer length on 64bit system */
#define _INT_LEN 21

static DMCONTEXT *ctx;

/* Yang structures models */
struct list_key_t {
	const char *key;
};

enum LIST_KEY{
	IP,
	NAME,
	__LIST_KEY_COUNT
};

enum LEAF_LIST_KEY{
	HIGHER_LAYER_IF,
	LOWER_LAYER_IF,
	USER_NAME,
	SEARCH,
	USER_AUTHENTICATION_ORDER,
	BOOTORDER,
	__LEAF_LIST_KEY_COUNT
};

enum LIST_NODE{
	ADDRESS,
	FIRMWARE_JOB,
	FIRMWARE_SLOT,
	GROUP,
	INTERFACE,
	NEIGHBOR,
	RULE,
	RULE_LIST,
	SERVER,
	SSH_KEY,
	USER,
	__LIST_NODE_COUNT
};



const struct list_key_t list_keys[__LIST_KEY_COUNT] = {
	[IP] = { .key = "ip" },
	[NAME] = { .key = "name"}
};

const struct list_key_t leaf_list_keys[__LEAF_LIST_KEY_COUNT] = {
	[HIGHER_LAYER_IF] = {.key = "higher-layer-if"},
	[LOWER_LAYER_IF] = {.key = "lower-layer-if"},
	[USER_NAME] = {.key = "user-name"},
	[SEARCH] = {.key = "search"},
	[USER_AUTHENTICATION_ORDER] = {.key = "user-authentication-order"},
	[BOOTORDER] = {.key = "bootorder"}
};

const struct list_key_t list_nodes[__LIST_NODE_COUNT] = {
	[ADDRESS] = {.key = "address"},
	[FIRMWARE_JOB] = {.key = "firmware-job"},
	[FIRMWARE_SLOT] = {.key = "firmware-slot"},
	[GROUP] = {.key = "group"},
	[INTERFACE] = {.key = "interface"},
	[NEIGHBOR] = {.key = "neighbor"},
	[RULE] = {.key = "rule"},
	[RULE_LIST] = {.key = "rule-list"},
	[SERVER] = {.key = "server"},
	[SSH_KEY] = {.key = "ssh-key"},
	[USER] = {.key = "user"}
};

static int is_list_key(const char *node_name)
{
	for (int i=0; i<__LIST_KEY_COUNT; i++)
		if(!strcmp(node_name, list_keys[i].key))
			return 1;

	return 0;
}

static int is_leaf_list_key(const char *node_name)
{
	for (int i=0; i<__LEAF_LIST_KEY_COUNT; i++)
		if(!strcmp(node_name, leaf_list_keys[i].key))
			return 1;

	return 0;
}

static int is_list_node(const char *node_name)
{
	for (int i=0; i<__LIST_NODE_COUNT; i++)
		if(!strcmp(node_name, list_nodes[i].key))
			return 1;

	return 0;
}

/*
 * dm_init() - init libev and dmconfig parts
 *
 */
int dm_init()
{
	int rc = -1;

	if (!(ctx = dm_context_new()))
		goto exit;

	dm_context_init(ctx, EV_DEFAULT, AF_INET, NULL, NULL, NULL);

	/* connect */
	if ((dm_connect(ctx)) != RC_OK) {
		fprintf(stderr, "dmconfig: connect failed\n");
		goto exit;
	}

	if ((rpc_startsession(ctx, CMD_FLAG_READWRITE, 0, NULL)) != RC_OK) {
		fprintf(stderr, "dmconfig: send start session failed\n");
		goto exit;
	}

	rc = 0;
exit:
	return rc;
}

/*
 * dm_shutdown() - deinit libev and dmconfig parts
 *
 */
void dm_shutdown()
{
	rpc_endsession(ctx);
	dm_context_shutdown(ctx, DMCONFIG_OK);
	dm_context_release(ctx);
}

/*
 * dm_get_parameter() - get parameter from mand
 *
 * @char*:	key we are searching for
 *
 * Returns NULL on error or value if found. Must be freed.
 */
static char* dm_get_parameter(char *key)
{
	if (!key) return NULL;

	DM2_AVPGRP answer = DM2_AVPGRP_INITIALIZER;

	uint32_t code, vendor_id;
	void *data;
	size_t size;
	char *rp = NULL;

	printf("get_parameter: %s\n", key);

	if (rpc_db_get(ctx, 1, (const char **)&key, &answer) != RC_OK)
		goto exit;

	if (dm_expect_avp(&answer, &code, &vendor_id, &data, &size) != RC_OK
		|| vendor_id != VP_TRAVELPING
		|| dm_expect_group_end(&answer) != RC_OK
		|| dm_decode_unknown_as_string(code, data, size, &rp) != RC_OK)
		goto exit;

exit:
	return rp;
}

/*
 * dm_set_parameter() - set parameter through mand
 *
 * @char*:	key we are setting
 * @char*:	value to be set
 *
 * Returns zero on success.
 */
static int dm_set_parameter(char *key, char *value)
{
	if(!key || !value) return -1;

	int rc = -1;
	struct rpc_db_set_path_value set_value = {
		.path = key,
		.value = {
			.code = AVP_UNKNOWN,
			.vendor_id = VP_TRAVELPING,
			.data = value,
			.size = strlen(value),
		},
	};

	if ((rc = rpc_db_set(ctx, 1, &set_value, NULL)) != RC_OK)
		goto exit;

	rc = 0;

exit:


	return rc;
}

/*
 * unused in R/W session
 * dm_commit() - commit pending changes
 *
 * Commits pending changes (set_parameter, add object...).
 * Returns zero on success.
 */
/*
static int dm_commit()
{
	if ((rpc_db_commit(ctx, NULL)) != RC_OK) {
		fprintf(stderr, "dmconfig: couldn't commit changes\n");
		return -1;
	}

	return 0;
}
*/

/*
 * dm_get_instance() - get instance from mand
 *
 * @char*:	NULL or path (for example "system.ntp")
 * @char*:	key we are searching for (for example "name")
 * @char*:	value we are searching for (for example "Server1")
 *
 * Returns instace if found.
 */
static uint16_t dm_get_instance(char *path, char *key, char *value)
{
	int rc = -1;
	uint16_t instance = 0;
	struct dm2_avp search = {
			.code = AVP_UNKNOWN,
			.vendor_id = VP_TRAVELPING,
			.data = value,
			.size = strlen(value),
	};
	DM2_AVPGRP answer = DM2_AVPGRP_INITIALIZER;

	if(!path) path = "";

	if ((rc = rpc_db_findinstance(ctx, path, key, &search, &answer)) != RC_OK) {
		fprintf(stderr, "dmconfig: couldn't get instance\n");
		goto exit;
	}

	if ((rc = dm_expect_uint16_type(&answer, AVP_UINT16, VP_TRAVELPING, &instance)) != RC_OK) {
		fprintf(stderr, "dmconfig: couldn't get instance\n");
		goto exit;
	}

exit:

	return instance;
}

/*
 * dm_add_instance() - add instance to mand
 *
 * @char*:	path with instance number (for example "system.ntp.server.1")
 *
 * Returns instance id on success, 0 on failure.
 */

static uint16_t dm_add_instance(char *path)
{
	DM2_AVPGRP answer = DM2_AVPGRP_INITIALIZER;

	uint16_t instance = DM_ADD_INSTANCE_AUTO;

	if (rpc_db_addinstance(ctx, path, instance, &answer) == RC_OK &&
			dm_expect_uint16_type(&answer, AVP_UINT16, VP_TRAVELPING, &instance) == RC_OK)
	{
		printf("added instance:%" PRIu16 "\n", instance);
	}
	else {
		fprintf(stderr, "unable to add instance\n");
		return 0;
	}

	return instance;
}

/*
 * dm_del_instance() - delete instance from mand
 *
 * @char*:	path to instance (for example "system.ntp.server.1")
 * Returns non zero on failure.
 */

static int dm_del_instance(char *path)
{
	DM2_AVPGRP answer = DM2_AVPGRP_INITIALIZER;

	int rc = rpc_db_delinstance(ctx, path, &answer);
	rc = (rc == RC_OK ? 0 : 1);

	return rc;
}

/*
 * dm_set_parameters_from_xml() - recursive function to parse xml parameters
 *
 * @node_t:	xml node that we have to process
 * @node_t:	current node that we are processing
 *
 * Parse xml structure and create 'cli' dm compatible structure for sending to
 * mand. We recursively go through XML nodes creating mand 'path'
 * (system.ntp.server...) until we find a value node which we use to set
 * parameter (path we created plus value we got), delete that node and restart
 * from root node.
 *
 * <system><ntp><enabled></ntp></system> ... system.ntp=enabled
 *
 * TODO: delete whole node if no more data in it after parsing
 */

int dm_set_parameters_from_xml(node_t *root, node_t *n)
{
	if (!n || !root) return -1;

	static char *path = NULL;
	static uint16_t instance = 0;

	char *name = roxml_get_name(n, NULL, 0);

	/* if node exists and it's not a key node (lists) add node name to path */
	if (name && !is_list_key(name)) {
		path = talloc_asprintf_append(path, "%s.", name);
		if (!path) return -1;
	}

	/* use attribute instance if we saved one */
	node_t *ninstance = roxml_get_attr(n, "instance", 0);
	if (ninstance) {
		char *instance = roxml_get_content(ninstance, NULL, 0, NULL);
		if (!instance) {
			fprintf(stderr, "unable to get node instance\n");
			talloc_free(path); path = NULL;
			return -1;
		}

		/* add instance to path: system.ntp.server.1*/
		path = talloc_asprintf_append(path, "%s", instance);
		if (!path) return -1;

		/* if we got delete operation, delete instance from mand, delete this
		* node from this xml and start again from root */
		node_t *noperation = roxml_get_attr(n, "operation", 0);
		if (noperation) {
			char *coperation = roxml_get_content(noperation, NULL, 0, NULL);
			if (!coperation) {
				fprintf(stderr, "unable to get operation value\n");
				talloc_free(path); path = NULL;
				return -1;
			}
			if (!strcmp(coperation, "delete")) {
				printf("deleting instance:%s", path);
				int rc = dm_del_instance(path);

				talloc_free(path); path = NULL;

				if (rc) {
					fprintf(stderr, "problem deleting instace\n");
					return -1;
				}

				roxml_del_node(n);
				return dm_set_parameters_from_xml(root, root);
			}
		}

		/* add '.' after we got instance number, ie:'system.ntp.server.1.' */
		path = talloc_asprintf_append(path, ".");
		if (!path) return -1;
	}

	/* else if this is value node, set paramater in mand, delete node from xml
 	 * and return */
	char *val = roxml_get_content(n, NULL, 0, NULL);
	if (val != NULL && strlen(val)) {

		/* remove last '.' */
		path[strlen(path) -1] = 0;

		/* if this is list, save key as attribute, we will use it to set all
 	 	 * children parameters this node has in mand */
		if (name && is_list_key(name)) {
			char cinstance[_INT_LEN];

			instance = dm_get_instance(path, name, val);

			/* if there is such instance we will just add it as attribute on
 	 	 	 * parent node */
			if (instance) {
				snprintf(cinstance, _INT_LEN, "%d", instance);
			}

			/* else add new instance to mand first */
			else {
				printf("dm_set_parameters_from_xml: no such instance:%s, adding \n", name);

				instance = dm_add_instance(path);
				if (!instance) {
					printf("unable to add instance\n");
					return -1;
				}
				snprintf(cinstance, _INT_LEN, "%d", instance);

				/* also set key value for new instance */
				path = talloc_asprintf_append(path, ".%s.%s", cinstance, name);
				if (!path) return -1;

				if (dm_set_parameter(path, val)) {
					fprintf(stderr, "unable to add instance key for:%s, %s\n", path, val);
					return -1;
				}
			}

			/* add instance to parent node */
			node_t *parent = roxml_get_parent(n);
			if(!parent) {
				fprintf(stderr, "dm_set_parameters_from_xml: unable to get parent node\n");
				return -1;
			}
			roxml_add_node(parent, 0, ROXML_ATTR_NODE, "instance", cinstance);
		}

		else {
			if (dm_set_parameter(path, val)) {
				fprintf(stderr, "unable to set parameter:%s", path);
				return -1;
			}
			printf("parameter:%s=\"%s\"\n", path, val);
		}

		talloc_free(path); path = NULL;

		roxml_del_node(n);
		return dm_set_parameters_from_xml(root, root);
	}

	/* else if no children and no values (all processed) remove this child */
	node_t *child = roxml_get_chld(n, NULL, 0);
	if (!child) {
		talloc_free(path); path = NULL;

		/* xml processed */
		if (n == root) return 0;

		roxml_del_node(n);

		return dm_set_parameters_from_xml(root, root);
	}

	/* process next child */
	return dm_set_parameters_from_xml(root, child);
}

/*
 * dm_list_to_xml() - recursive function which creates XML from mand list
 *
 * @char*: path prefix which is created (system.ntp.)
 * @DM2_AVPGRP*: mand request context
 * @node_t*: XML root which we are creating
 *
 * <system><dns-resolver><server><name>test2</name><udp-and-tcp><port>0</port><address>1.2.3.4</address></udp-and-tcp></server><search>test</search></dns-resolver></system></config></edit-config>
 * <dns-resolver><search><search>.local</search></search>
 */
static uint32_t dm_list_to_xml(DM2_AVPGRP *grp, node_t **xml_out, int elem_node, char *parent_name)
{
	uint32_t r;
	DM2_AVPGRP container;
	uint32_t code;
	uint32_t vendor_id;
	void *data;
	size_t size;

	char *name = NULL;

	static int is_leaf = 0;

	if ((r = dm_expect_avp(grp, &code, &vendor_id, &data, &size)) != RC_OK)
		return r;

	if (vendor_id != VP_TRAVELPING)
		return RC_ERR_MISC;

	dm_init_avpgrp(grp->ctx, data, size, &container);

	switch (code) {

	/* has children instances */
	case AVP_TABLE:
		if ((r = dm_expect_string_type(&container, AVP_NAME, VP_TRAVELPING, &name)) != RC_OK) {
			fprintf(stderr, "invalid object\n");
			return r;
		}

		/* test if leaf list */
		is_leaf = is_leaf_list_key(name) ? 1 : 0;

		/* save node name for instances */
		parent_name = name;

		/* process all children */
		while (dm_list_to_xml(&container, xml_out, elem_node, parent_name) == RC_OK);

		break;

	/* has children */
	case AVP_OBJECT:

		/* skip first node */
		if (!elem_node++) {
			while (dm_list_to_xml(&container, xml_out, elem_node, NULL) == RC_OK);

			return RC_OK;
		}

		if ((r = dm_expect_string_type(&container, AVP_NAME, VP_TRAVELPING, &name)) != RC_OK) {
			fprintf(stderr, "invalid object\n");
			return r;
		}

		printf("object:%s\n", name);

		node_t *n = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, name, NULL);
		if (!n) {
			fprintf(stderr, "dm_get_xml_config: unable to add parameter node\n");
			return -1;
		}

		if(!strcmp(name, "ipv4") || !strcmp(name, "ipv6"))
			roxml_add_node(n, 0, ROXML_ATTR_NODE, "xmlns", "urn:ietf:params:xml:ns:yang:ietf-ip");

		/* process all children */
		while (dm_list_to_xml(&container, &n, elem_node, parent_name) == RC_OK);

		break;

	/* is one of cildren instances */
	case AVP_INSTANCE: {
			uint16_t id;
			if ((r = dm_expect_uint16_type(&container, AVP_NAME, VP_TRAVELPING, &id)) != RC_OK) {
				fprintf(stderr, "invalid instance \n");
				return r;
			}

			/* mand workaround: if this is leaf list skip parent tag */
			if (is_leaf || !parent_name) {
				while (dm_list_to_xml(&container, xml_out, elem_node, NULL) == RC_OK);
				is_leaf = 0;
				break;
			}

			/* else normal list */
			printf("instance:%d, parent:%s\n", id, parent_name);

			node_t *n = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, parent_name, NULL);
			if (!n) {
				fprintf(stderr, "dm_get_xml_config: unable to add parameter node\n");
				return -1;
			}

			while (dm_list_to_xml(&container, &n, elem_node, parent_name) == RC_OK);

			break;
		}
	/* is one children element */
	case AVP_ELEMENT: {

		if ((r = dm_expect_string_type(&container, AVP_NAME, VP_TRAVELPING, &name)) != RC_OK) {
			fprintf(stderr, "invalid element\n");
			return r;
		}

		printf("element:%s\n", name);

		uint32_t type;
		char *value = NULL;

		if ((r = dm_expect_uint32_type(&container, AVP_TYPE, VP_TRAVELPING, &type)) != RC_OK
			|| (r = dm_expect_avp(&container, &code, &vendor_id, &data, &size)) != RC_OK)
			return r;

		switch(type) {
		case AVP_UINT32:
		case AVP_ENUMID:
			value = calloc(_INT_LEN, 1);
			snprintf(value, _INT_LEN, "%" PRIu32, dm_get_uint32_avp(data));
			break;

		case AVP_UINT64:
			value = calloc(_INT_LEN, 1);
			snprintf(value, _INT_LEN, "%" PRIu64, dm_get_uint64_avp(data));
			break;

		case AVP_INT32:
		case AVP_COUNTER:
			value = calloc(_INT_LEN, 1);
			snprintf(value, _INT_LEN, "%" PRId32, dm_get_int32_avp(data));
			break;

		case AVP_INT64:
		case AVP_TICKS:
			value = calloc(_INT_LEN, 1);
			snprintf(value, _INT_LEN, "%" PRId64, dm_get_int64_avp(data));
			break;

		case AVP_BOOL:
			value = strdup((dm_get_uint8_avp(data) ? "true" : "false"));
			break;

		case AVP_STRING:
		case AVP_PATH:
		case AVP_ENUM:
			if (size) {
				value = calloc(size + 1, 1);
				snprintf(value, size + 1, "%s", (char *) data);
			}
			break;

		case AVP_ADDRESS: {
			char buf[INET6_ADDRSTRLEN];
			int af;
			union {
				struct in_addr in;
				struct in6_addr in6;
			} addr;

			if (dm_get_address_avp(&af, &addr, sizeof(addr), data, size)) {
				inet_ntop(af, &addr, buf, sizeof(buf));
				value = strdup(buf);
			}
			break;
		}

		case AVP_BINARY: {
			value = malloc(((size + 3) * 4) / 3);
			dm_to64(data, size, value);
			break;
		}

		default:
			printf("unknown type:%d\n", code);
		}

		int len = 0;
		if (!strcmp(name, "type"))
			len = asprintf(&value, "%s:%s", "ianaift", value);
		if (len < 0) {
			fprintf(stderr, "dm_get_xml_config: unable to add value type\n");
			return -1;
		}

		//printf("value:%s\n", value);
		node_t *n = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, name, value);
		if (!n)
			fprintf(stderr, "dm_get_xml_config: unable to add parameter node\n");
		else {
			if (!strcmp(name, "type"))
				roxml_add_node(n, 0, ROXML_ATTR_NODE, "xmlns:ianaift", "urn:ietf:params:xml:ns:yang:iana-if-type");
		}

		free(value);
		break;
	}
	}

	return RC_OK;
}

/* <get-config<filter></filter></get-config>
 * <get-config<filter><system></system><interfaces></interfaces></filter></get-config></rpc>
 * <get><filter><system><clock><timezone-location/><timezone-utc-offset/></clock></system></filter></get></rpc>
 * <get><filter><system><contact></contact><hostname/><location/></system></filter></get></rpc>
 * <get><filter><system><location></location></system></filter></get></rpc>
 * <get><filter><system><location></location><contact/><hostname/></system></filter></get></rpc>
 * <get><filter><system><contact></contact></system></filter></get></rpc>
 * <get><filter><system><ntp></ntp></system></filter></get></rpc>
 */

int dm_get_xml_config(node_t *filter_root, node_t *filter_node, node_t **xml_out, char *path)
{
	int rc = -1;

	if (!filter_root || !filter_node) {
		printf("invalid filter or node\n");
	}

	node_t *next_node = *xml_out;
	char *current_path = talloc_strdup(NULL, path);
	char *node_name = roxml_get_name(filter_node, NULL, 0);
	int is_key = is_list_key(node_name);

	/* if not root node or key node add to path */
	if (strcmp(node_name, "filter") && !is_key && !is_leaf_list_key(node_name) && !is_list_node(node_name)) {
		path = talloc_asprintf_append(path, "%s%s", path ? "." : "", node_name);
		if (!path || !strlen(path)) {
			fprintf(stderr, "unable to reallocate\n");
			return -1;
		}
		next_node = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, node_name, NULL);
		/*  ietf attributes */
		char *attr_type = NULL;

		if (!strcmp(node_name, "system") || !strcmp(node_name, "system-state"))
			attr_type = "urn:ietf:params:xml:ns:yang:ietf-system";
		else if (!strcmp(node_name, "interfaces") || !strcmp(node_name, "interfaces-state"))
			attr_type = "urn:ietf:params:xml:ns:yang:ietf-interfaces";
		else if (!strcmp(node_name, "firmware-slot"))
			attr_type = "urn:opencpe:firmware-mgmt";

		if (attr_type)
			roxml_add_node(next_node, 0, ROXML_ATTR_NODE, "xmlns", attr_type);
	}

	//printf("name:%s\n", node_name);
	printf("path:%s\n", path);

	/* we are always looking for the last child node for getting data */
	node_t *child = roxml_get_chld(filter_node, NULL, 0);
	if (child) {
		rc = dm_get_xml_config(filter_root, child, &next_node, path);
		return rc;
	}

	/* check if content match node */
	char *node_content = roxml_get_content(child, NULL, 0, NULL);
	if (node_content && strlen(node_content)) {
		printf("content match node:%s\n", node_content);
		//return dm_get_xml_config(filter_root, child, &n, NULL);
	}

	/* check if key match node */
	if (is_key) {
		printf("key match node:%s\n", node_name);
	}

	/* get paramater/list from mand */
	DM2_AVPGRP answer = DM2_AVPGRP_INITIALIZER;
	if ((rc = rpc_db_list(ctx, 0, path, &answer)) != RC_OK) {
		fprintf(stderr, "dmconfig: couldn't get list for path:%s\n", path);
		talloc_free(path); path = NULL;
		return -1;
	}

	uint32_t code, vendor_id; void *data; size_t size;
	rc = dm_expect_avp(&answer, &code, &vendor_id, &data, &size);
	if (rc != RC_OK) {
		fprintf(stderr, "unable to get type from mand\n");
		talloc_free(path); path = NULL;
		return -1;
	}

	/* what type of data did we got from mand */
	switch (code) {
		case AVP_ELEMENT: {
				char *param_value = dm_get_parameter(path);
				/* emtpy value is allowed */

				printf("got parameter back:%s\n", param_value);
				node_t *node_value = roxml_add_node(next_node, 0, ROXML_TXT_NODE, NULL, param_value);
				if (!node_value) {
					fprintf(stderr, "unable to set node value\n");
					talloc_free(path); path = NULL;
					return -1;
				}
			}
		break;

		case AVP_INSTANCE:
		case AVP_OBJECT:
		case AVP_TABLE:

			rpc_db_list(ctx, 0, path, &answer);

			/* got list, add to response */
			while (dm_list_to_xml(&answer, &next_node, 0, NULL) == RC_OK);
		break;
	}

	/* move to the next node */
	node_t *s = roxml_get_next_sibling(filter_node);
	if (s) {
		printf("got sibling:%s\n", roxml_get_name(s, NULL, 0));
		rc = dm_get_xml_config(filter_root, s, xml_out, current_path);
		return rc;
	}

	return 0;
}
/*
 * dm_set_current_datetime() - set datetime
 *
 * @char*: value to be set
 *
 * 	Sets datetime if ntp is not enabled.
 * 	Returns 0 on success, 1 on ntp enabled, -1 on error.
 */
int dm_set_current_datetime(char *value)
{
	if (!value) return -1;

	printf("got date:%s\n", value);

	int rc = -1;
	char *ntp_enabled = NULL;

	struct tm tm;
	time_t t;
	if ( strptime(value, "%FT%T%z", &tm) == NULL ) {
		fprintf(stderr, "unable to convert time value\n");
		goto exit;
	}

	t = mktime(&tm);

	printf("got timestamp :%ld\n", (long) t);

	ntp_enabled = dm_get_parameter("system.ntp.enabled");
	if (!ntp_enabled) {
		fprintf(stderr, "unable to get system ntp state\n");
		goto exit;
	}

	printf("ntp state:%s\n", ntp_enabled);

	if (!strcmp(ntp_enabled, "1")) {
		rc = 1;
		goto exit;
	}

	if (!strcmp(ntp_enabled, "0")) {
		rc = dm_set_parameter("system-state.clock.current-datetime", value);
		if (rc) {
			fprintf(stderr, "unable to set current datetime\n");
			rc = -1;
			goto exit;
		}
		rc = 0;
	}
	else {
		fprintf(stderr, "unknown ntp value\n");
	}

exit:

	free(ntp_enabled);
	return rc;
}

int dm_rpc_restart()
{
	int rc = rpc_system_restart(ctx);
	return rc == RC_OK ? 0 : 1;
}

int dm_rpc_shutdown()
{
	int rc = rpc_system_shutdown(ctx);
	return rc == RC_OK ? 0 : 1;

}

char* dm_rpc_firmware_download(char *address, char *install_target,
								char *credential, uint8_t credentialstype,
								uint32_t timeframe, uint8_t retry_count,
								uint32_t retry_interval, uint32_t retry_interval_increment)
{
	int rc;
	DM2_AVPGRP answer = DM2_AVPGRP_INITIALIZER;

	return "1"; // mand RPC API is not yet implemented

	rc = rpc_firmware_download(ctx, address, credentialstype, credential,
									install_target, timeframe, retry_count,
									retry_interval, retry_interval_increment, &answer);
	if (rc) {
		fprintf(stderr, "mand firmware download error\n");
		return NULL;
	}

	uint32_t code, vendor_id;
	void *data;
	size_t size;
	char *rp = NULL;

	rc = dm_expect_avp(&answer, &code, &vendor_id, &data, &size) != RC_OK
		|| vendor_id != VP_TRAVELPING
		|| dm_expect_group_end(&answer) != RC_OK
		|| dm_decode_unknown_as_string(code, data, size, &rp) != RC_OK;
	if (rc) {
		fprintf(stderr, "mand firmware download unable to get result\n");
		return NULL;
	}

	return rp;

}
int dm_rpc_firmware_commit(int32_t job_id)
{
	int rc = rpc_firmware_commit(ctx, job_id);
	return rc == RC_OK ? 0 : 1;

}

int dm_rpc_set_bootorder(const char **boot_order, int count)
{
	int rc = rpc_set_boot_order(ctx, count, boot_order);
	return rc == RC_OK ? 0 : 1;
}
