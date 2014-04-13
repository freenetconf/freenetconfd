#include "dmconfig.h"
#include <inttypes.h>

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

const struct list_key_t list_keys[__LIST_KEY_COUNT] = {
	[IP] = { .key = "ip" },
	[NAME] = { .key = "name"}
};

const struct list_key_t leaf_list_keys[__LEAF_LIST_KEY_COUNT] = {
	[HIGHER_LAYER_IF] = {.key = ""},
	[LOWER_LAYER_IF] = {.key = ""},
	[USER_NAME] = {.key = ""},
	[SEARCH] = {.key = ""},
	[USER_AUTHENTICATION_ORDER] = {.key = ""},
	[BOOTORDER] = {.key = ""}
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

	if ((rpc_startsession(ctx, CMD_FLAG_READWRITE, 10, NULL)) != RC_OK) {
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

	printf("GET DB: %s\n", key);

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
		.path  = key,
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
 * dm_commit() - commit pending changes
 *
 * Commits pending changes (set_parameter, add object...).
 * Returns zero on success.
 */
static int dm_commit()
{
	if ((rpc_db_commit(ctx, NULL)) != RC_OK) {
		fprintf(stderr, "dmconfig: couldn't commit changes\n");
		return -1;
	}

	return 0;
}

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

	/* use attribute instance if we saved one  */
	node_t *ninstance = roxml_get_attr(n, "instance", 0);
	if (ninstance) {
		char *instance = roxml_get_content(ninstance, NULL, 0, NULL);
		if (!instance) {
			fprintf(stderr, "unable to get node instance\n");
			return -1;
		}

		/* add instance to path: system.ntp.server.1*/
		path = talloc_asprintf_append(path, "%s", instance);
		if (!path) return -1;

		/* if we got delete operation delete instance from mand, delete this
 	 	 * node from xml and return  */
		node_t *noperation = roxml_get_attr(n, "operation", 0);
		if (noperation) {
			char *coperation = roxml_get_content(noperation, NULL, 0, NULL);
			if (!coperation) {
				fprintf(stderr, "unable to get operation value\n");
				return -1;
			}
			if (!strcmp(coperation, "delete")) {
				printf("deleting instance:%s", path);
				int rc = dm_del_instance(path);

				talloc_free(path);
				path = NULL;

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

		talloc_free(path);
		path = NULL;

		roxml_del_node(n);
		return dm_set_parameters_from_xml(root, root);
	}

	/* else if no children and no values (all processed) remove this child */
	node_t *child = roxml_get_chld(n, NULL, 0);
	if (!child) {
		talloc_free(path);
		path = NULL;

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
 */
static uint32_t dm_list_to_xml(const char *prefix, DM2_AVPGRP *grp, node_t **xml_out, int elem_node)
{
	//printf("prefix:%s\n", prefix);

	uint32_t r;
	DM2_AVPGRP container;
	uint32_t code;
	uint32_t vendor_id;
	void *data;
	size_t size;

	char *name, *path;
	uint16_t id;

	if ((r = dm_expect_avp(grp, &code, &vendor_id, &data, &size)) != RC_OK)
		return r;

	if (vendor_id != VP_TRAVELPING)
		return RC_ERR_MISC;

	dm_init_avpgrp(grp->ctx, data, size, &container);

	switch (code) {
	case AVP_TABLE:
	case AVP_OBJECT:
	case AVP_ELEMENT:
		if ((r = dm_expect_string_type(&container, AVP_NAME, VP_TRAVELPING, &name)) != RC_OK)
			return r;

		if (!(path = talloc_asprintf(container.ctx, "%s.%s", prefix, name)))
			return RC_ERR_ALLOC;
		break;

	case AVP_INSTANCE:
		if ((r = dm_expect_uint16_type(&container, AVP_NAME, VP_TRAVELPING, &id)) != RC_OK)
			return r;

		if (!(path = talloc_asprintf(container.ctx, "%s.%d", prefix, id)))
			return RC_ERR_ALLOC;
		break;

	default:
		printf("unknown object: %s, type: %d\n", prefix, code);
		return RC_ERR_MISC;
	}

//	printf("path:%s\n", path);
//	printf("type:%d for name:%s\n", code, name);

	switch (code) {
	case AVP_ELEMENT: {
		uint32_t type;
		char *value = NULL;
		//printf("%s:", name);

		if ((r = dm_expect_uint32_type(&container, AVP_TYPE, VP_TRAVELPING, &type)) != RC_OK
		    || (r = dm_expect_avp(&container, &code, &vendor_id, &data, &size)) != RC_OK)
			return r;

		//printf("------------pathnside:%s:\n", path);

		switch(type) {
		case AVP_UINT32:
			value = calloc(_INT_LEN, 1);
			snprintf(value, _INT_LEN, "%" PRIu32, dm_get_uint32_avp(data));
			break;

		case AVP_UINT64:
			value = calloc(_INT_LEN, 1);
			snprintf(value, _INT_LEN, "%" PRIu64, dm_get_uint64_avp(data));
			break;

		case AVP_INT32:
			value = calloc(_INT_LEN, 1);
			snprintf(value, _INT_LEN, "%" PRId32, dm_get_int32_avp(data));
			break;

		case AVP_INT64:
			value = calloc(_INT_LEN, 1);
			snprintf(value, _INT_LEN, "%" PRId64, dm_get_int64_avp(data));
			break;

		case AVP_BOOL:
			value = strdup((dm_get_uint8_avp(data) ? "true" : "false"));
			break;

		case AVP_STRING:
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
				struct in_addr  in;
				struct in6_addr in6;
			} addr;

			if (dm_get_address_avp(&af, &addr, sizeof(addr), data, size)) {
				inet_ntop(af, &addr, buf, sizeof(buf));
				value = strdup(buf);
			}
			break;
		}

		default:
			printf("unknown type:%d", code);
		}

		//printf("\n");
		//printf("name:%s, val:%s\n", name, value);

		//if (!strcmp(name, "timezone-utc-offset")) break;

		node_t *n = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, name, value);
		if (!n)
			fprintf(stderr, "dm_get_xml_config: unable to add parameter node\n");
		else {
			if (!strcmp(name, "type"))
				roxml_add_node(n, 0, ROXML_ATTR_NODE, "xmlns:ianaift", "urn:ietf:params:xml:ns:yang:iana-if-type");
			else if (!strcmp(name, "system"))
				roxml_add_node(n, 0, ROXML_ATTR_NODE, "xmlns", "urn:ietf:params:xml:ns:yang:ietf-system");
			else if (!strcmp(name, "interfaces"))
				roxml_add_node(n, 0, ROXML_ATTR_NODE, "xmlns", "urn:ietf:params:xml:ns:yang:ietf-interfaces");
		}

		free(value);
		break;
	}


	case AVP_INSTANCE:
		printf("instance\n");
		while (dm_list_to_xml(path, &container, xml_out, elem_node) == RC_OK);
		break;

	case AVP_TABLE:
	case AVP_OBJECT:
		printf("node: %s, type:%d\n", name, code);

		// if this is instance node, skip it
		/*
		  if (!strcmp(name, "server")) {
		  while (dm_list_to_xml(path, obj, xml_out, elem_node) == RC_OK);
		  }
		  else */

		if (!strcmp(name, "search") || !strcmp(name, "user-authentication-order") || !strcmp(name, "user") || !strcmp(name, "authentication") || !elem_node++) {
			//node_t *n = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, "server", NULL);
			while (dm_list_to_xml(path, &container, xml_out, elem_node) == RC_OK);

			return RC_OK;
		}

		node_t *n = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, name, NULL);
		if (!n) {
			fprintf(stderr, "dm_get_xml_config: unable to add parameter node\n");
			return -1;
		}

		if(!strcmp(name, "ipv4") || !strcmp(name, "ipv6"))
			roxml_add_node(n, 0, ROXML_ATTR_NODE, "xmlns", "urn:ietf:params:xml:ns:yang:ietf-ip");

		while (dm_list_to_xml(path, &container, &n, elem_node) == RC_OK);

		break;
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

int dm_get_xml_config(node_t *filter_root, node_t *filter_node, node_t **xml_out)
{
	if (!filter_root || !(*xml_out) || !filter_node) {
		printf("invalid filter or node\n");
	}

	static char *path = NULL;

	node_t *child = roxml_get_chld(filter_node, NULL, 0);
	if (!child) {

		free(path);
		path = NULL;

		while(1){
			if(filter_node == filter_root)
				return 1;

			/* if this node has siblings, delete only this node */
			node_t *s = roxml_get_next_sibling(filter_node);
			if (s) {
				printf("got sibling:%s\n", roxml_get_name(s, NULL, 0));
				roxml_del_node(filter_node);
				break;
			}

			/* else delete parent node (since it doesn't have any children
 	 	 	 * except our node) if not main node, and start from parent of parent */
			node_t *p = roxml_get_parent(filter_node);
			if (p != filter_root) {
				filter_node = roxml_get_parent(p);
				roxml_del_node(p);
				continue;
			}

			/* if this is parent node without children, delete it */
			if (roxml_get_chld_nb(filter_node) == 0) {
				roxml_del_node(filter_node);
				break;
			}

		};

		return dm_get_xml_config(filter_root, filter_root, xml_out);
	}

	DM2_AVPGRP answer = DM2_AVPGRP_INITIALIZER;
	int rc = -1;

	char *name = roxml_get_name(child, NULL, 0);
	if (name) {

		int path_len = path ? strlen(path) : 0;
		int name_len = strlen(name);

		path = realloc(path, path_len + name_len + 2); /* '.' and '\0' */
		if (!path) {
			fprintf(stderr, "dm_set_parameters_from_xml: realloc failed\n");
			return -1;
		}

		memset(path + path_len, 0, name_len + 2);

		if(path_len)
			strncat(path, ".", 1);
		strncat(path, name, name_len);

	}

	printf("list path: %s\n", path);
	if ((rc = rpc_db_list(ctx, 0, path, &answer)) != RC_OK) {
		fprintf(stderr, "dmconfig: couldn't get list for path:%s\n", path);
		goto exit;
	}

	node_t *c = roxml_get_chld(child, NULL, 0);
	if (!c) {
		printf("path:%s\n", path);
		printf("name:%s\n", name);

		node_t *n = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, name, NULL);
		if (!strcmp(name, "system")) roxml_add_node(n, 0, ROXML_ATTR_NODE, "xmlns",  "urn:ietf:params:xml:ns:yang:ietf-system");
		if (!strcmp(name, "interfaces")) roxml_add_node(n, 0, ROXML_ATTR_NODE, "xmlns",  "urn:ietf:params:xml:ns:yang:ietf-interfaces");

		while (dm_list_to_xml(path, &answer, &n, 0) == RC_OK);
	}

	dm_get_xml_config(filter_root, child, xml_out);

exit:
	return rc;
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
	int rc = -1;
	char *ntp_enabled = NULL;

	ntp_enabled = dm_get_parameter("system.ntp.enabled");
	if (!ntp_enabled) {
		fprintf(stderr, "unable to get system ntp state\n");
		goto exit;
	}

	if (!strcmp(ntp_enabled, "true")) {
		rc = 1;
		goto exit;
	}

	if (!strcmp(ntp_enabled, "false")) {
		rc = dm_set_parameter("system-state.clock.current-datetime", value);
		if (rc || dm_commit()) {
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
