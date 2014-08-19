#include <inttypes.h>
#include <time.h>
#include "dmconfig.h"
#include "freenetconfd.h"

/* max integer length on 64bit system */
#define _INT_LEN 21

/* netconf operations */
#define NETCONF_OP_NA 0
#define NETCONF_OP_DEL 1
#define NETCONF_OP_ADD 2

static DMCONTEXT *ctx;

int dm_set_parameters_from_xml_call(node_t *root, node_t *n);

/* parsed from yang models */

const char *list_keys[] = {
	"ip",
	"name"
};

const char *leafrefs[] = {
	"lower-layer-if",
	"higher-layer-if"
};

const char *leaf_list_nodes[] = {
	"higher-layer-if", "lower-layer-if",
	"user-name", "search",
	"user-authentication-order", "bootorder"
};

static int is_leafref(const char *leafref)
{
	if(leafref)
	for( int i=0, len = ARRAY_SIZE(leafrefs); i < len; i++)
		if(!strcmp(leafref, leafrefs[i]))
			return 1;

	return 0;
}
static int is_list_key(const char *node_name)
{
	if(node_name)
	for( int i=0, len = ARRAY_SIZE(list_keys); i < len; i++)
		if(!strcmp(node_name, list_keys[i]))
			return 1;

	return 0;
}

static int is_leaf_list_node(const char *node_name)
{
	if(node_name)
	for( int i=0, len = ARRAY_SIZE(leaf_list_nodes); i < len; i++)
		if(!strcmp(node_name, leaf_list_nodes[i]))
			return 1;

	return 0;
}

static void roxml_del_curr(node_t **node)
{
	node_t *t = roxml_get_parent(*node);
	roxml_del_node(*node);
	*node = t;
}

static void yang_del_prefix(char **s)
{
	char *prefixes[] = {"sys:", "ianaift:"};
	for( int i=0, len = ARRAY_SIZE(prefixes); i < len; i++)
		if (strstr(*s, prefixes[i]))
			memmove(*s, *s + strlen(prefixes[i]), strlen(*s));
}

static int netconf_operation(node_t *s)
{
	node_t *noperation = roxml_get_attr(s, "operation", 0);
	char *coperation = roxml_get_content(noperation, NULL, 0, NULL);
	if(!coperation)
		return NETCONF_OP_NA;

	if (!strcmp(coperation, "delete"))
		return NETCONF_OP_DEL;

	if (!strcmp(coperation, "create"))
		return NETCONF_OP_ADD;

	return NETCONF_OP_NA;
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
	ev_loop_destroy(EV_DEFAULT);
}

/*
 * dm_get_parameter() - get parameter from mand
 *
 * @char*:	key we are searching for
 *
 * Returns NULL on error or value if found. Must be freed.
 */
static char* dm_get_parameter(char *key, uint32_t *code)
{
	if (!key) return NULL;

	DM2_AVPGRP answer = DM2_AVPGRP_INITIALIZER;

	uint32_t vendor_id;
	void *data;
	size_t size;
	char *rp = NULL;

	printf("get_parameter: %s\n", key);

	if (rpc_db_get(ctx, 1, (const char **)&key, &answer) != RC_OK)
		goto exit;

	if (dm_expect_avp(&answer, code, &vendor_id, &data, &size) != RC_OK
		|| vendor_id != VP_TRAVELPING
		|| dm_expect_group_end(&answer) != RC_OK
		|| dm_decode_unknown_as_string(*code, data, size, &rp) != RC_OK)
		goto exit;

exit:
	dm_release_avpgrp(&answer);
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

	yang_del_prefix(&value);

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

	dm_release_avpgrp(&answer);
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
		instance = 0;
	}

	dm_release_avpgrp(&answer);

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

	dm_release_avpgrp(&answer);
	return rc;
}

/*
 * dm_set_array() - add/set array in mand
 *
 *
 *
 */
#define BLOCK_ALLOC 16

static int dm_set_array(char *path, node_t *s)
{
	int rc = -1;
	int cnt;

	struct rpc_db_set_path_value set_array = {
		.path  = path,
		.value = {
			.code = AVP_ARRAY,
			.vendor_id = VP_TRAVELPING,
		},
	};
	struct rpc_db_set_path_value *array = NULL;

	char *node_name = roxml_get_name(s, NULL, 0);

	for (cnt = 0; s != 0; ) {

		if (strcmp(node_name, roxml_get_name(s, NULL, 0)))
			break;

		if (netconf_operation(s) != NETCONF_OP_DEL) {

			char *value = roxml_get_content(s, NULL, 0, NULL);
			printf("leaf value:%s\n", value);
			yang_del_prefix(&value);

			if ((cnt % BLOCK_ALLOC) == 0)
				array = talloc_realloc(NULL, array, struct rpc_db_set_path_value, cnt + BLOCK_ALLOC);
			if (!array)
				return RC_ERR_ALLOC;
			memset(array + cnt, 0, sizeof(struct rpc_db_set_path_value));

			array[cnt].value.code = AVP_UNKNOWN;
			array[cnt].value.vendor_id = VP_TRAVELPING;
			array[cnt].value.data = value;
			array[cnt].value.size = strlen(value);

			printf("array[%d]=%s\n", cnt, value);

			cnt++;
		}

		node_t *next = roxml_get_next_sibling(s);
		roxml_del_curr(&s);
		s = next;
	}


	printf("elements:%d\n", cnt);

	set_array.value.data = array;
	set_array.value.size = cnt;

	DM2_AVPGRP answer = DM2_AVPGRP_INITIALIZER;
	if ((rc = rpc_db_set(ctx, 1, &set_array, &answer)) != RC_OK) {
		printf("set array failed \n");
		goto exit;
	}

	printf("set array succeded\n");

	rc = 0;

exit:
	dm_release_avpgrp(&answer);
	talloc_free(array);

	return rc;
}

/*
 * dm_set_parameters_from_xml_call() - recursive function to parse xml parameters
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
	int rc;

	rpc_switchsession(ctx, CMD_FLAG_CONFIGURE, 0, NULL);

	rc = dm_set_parameters_from_xml_call(root, n);

	rpc_db_commit(ctx, NULL);
	rpc_db_save(ctx, NULL);
	rpc_switchsession(ctx, CMD_FLAG_READWRITE, 0, NULL);

	return rc;
}

int dm_set_parameters_from_xml_call(node_t *root, node_t *n)
{
	if (!n || !root) return -1;

	static char *path = NULL;
	static uint16_t instance = 0;

	char *name = roxml_get_name(n, NULL, 0);
	if (!name) {
		fprintf(stderr, "unable to get node name\n");
		talloc_free(path); path = NULL;
		return -1;
	}

	/* if node exists and it's not a key node (lists) add node name to path */
	if (!is_list_key(name)) {
		path = talloc_asprintf_append(path, "%s.", name);
		if (!path) return -1;
	}

	/* leaf lists */
	if (is_leaf_list_node(name)) {
		printf("leaf list this one\n");
		path[strlen(path) -1] = 0;

		dm_set_array(path, n);

		talloc_free(path); path = NULL;

		return dm_set_parameters_from_xml_call(root, root);
	}

	/* lists */
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
		if (netconf_operation(n) == NETCONF_OP_DEL) {
				printf("deleting instance:%s\n", path);
				int rc = dm_del_instance(path);

				talloc_free(path); path = NULL;

				if (rc) {
					fprintf(stderr, "problem deleting instace\n");
					return -1;
				}

				roxml_del_node(n);
				return dm_set_parameters_from_xml_call(root, root);
		}

		/* add '.' after we got instance number, ie:'system.ntp.server.1.' */
		path = talloc_asprintf_append(path, ".");
		if (!path) return -1;
	}

	/* values */
	/* if this is value node, set paramater in mand, delete node from xml
 	 * and return */
	char *val = roxml_get_content(n, NULL, 0, NULL);
	if (val != NULL && strlen(val)) {

		/* remove last '.' */
		path[strlen(path) -1] = 0;

		/* if this is list, save key as attribute, we will use it to set all
 	 	 * children parameters this node has in mand */
		if (is_list_key(name)) {
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
		return dm_set_parameters_from_xml_call(root, root);
	}

	/* else if no children and no values (all processed) remove this child */
	node_t *child = roxml_get_chld(n, NULL, 0);
	if (!child) {
		talloc_free(path); path = NULL;

		/* xml processed */
		if (n == root) return 0;

		roxml_del_node(n);

		return dm_set_parameters_from_xml_call(root, root);
	}

	/* process next child */
	return dm_set_parameters_from_xml_call(root, child);
}

/*
 * dm_list_to_xml() - recursive function which creates XML from mand list
 *
 * @char*: path prefix which is created (system.ntp.)
 * @DM2_AVPGRP*: mand request context
 * @node_t*: XML root which we are creating
 *
 */
static uint32_t dm_list_to_xml(DM2_AVPGRP *grp, node_t **xml_out, int elem_node, char *parent_name, char *filter)
{
	uint32_t r;
	DM2_AVPGRP container;
	uint32_t code;
	uint32_t vendor_id;
	void *data;
	size_t size;

	char *name = NULL;

	if ((r = dm_expect_avp(grp, &code, &vendor_id, &data, &size)) != RC_OK)
		return r;

	dm_init_avpgrp(grp->ctx, data, size, &container);

	switch (code) {

	case AVP_ARRAY: {
			printf("AVP_ARRAY\n");

			uint32_t type;
			if ((r = dm_expect_string_type(&container, AVP_NAME, VP_TRAVELPING, &name)) != RC_OK
    			|| (r = dm_expect_uint32_type(&container, AVP_TYPE, VP_TRAVELPING, &type)) != RC_OK)
			return r;

			while (dm_expect_group_end(&container) != RC_OK) {
				char *value = NULL;
				if ((r = dm_expect_avp(&container, &code, &vendor_id, &data, &size)) != RC_OK
					|| (r = dm_decode_unknown_as_string(code, data, size, &value)) != RC_OK)
					return r;

				if (is_leafref(name) && value){
					uint32_t code;
					char *key = "name";
					char req_len = strlen(value) + strlen(key) + 2;
					char req[req_len];
					snprintf(req, req_len, "%s.%s", value, key);

					char *response = dm_get_parameter(req, &code);
					roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, name, response);

					free(response);
				}
				else  {
					roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, name, value);
				}
				free(value);
			}
		}
	break;

	case AVP_NAME:
			printf("AVP_NAME\n");
		break;

	/* has children instances */
	case AVP_TABLE:
			printf("AVP_TABLE\n");
		if ((r = dm_expect_string_type(&container, AVP_NAME, VP_TRAVELPING, &name)) != RC_OK) {
			fprintf(stderr, "invalid object\n");
			return r;
		}

		/* save node name for instances */
		parent_name = name;

		/* process all children */
		while (dm_list_to_xml(&container, xml_out, elem_node, parent_name, filter) == RC_OK);

		break;

	/* has children */
	case AVP_OBJECT:
			printf("AVP_OBJECT\n");

		/* skip first node */
		if (!elem_node) {
			++elem_node;
			while (dm_list_to_xml(&container, xml_out, elem_node, NULL, filter) == RC_OK);

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
		while (dm_list_to_xml(&container, &n, elem_node, parent_name, filter) == RC_OK);

		break;

	/* is one of cildren instances */
	case AVP_INSTANCE: {
			printf("AVP_INSTANCE\n");
			uint16_t id;
			if ((r = dm_expect_uint16_type(&container, AVP_NAME, VP_TRAVELPING, &id)) != RC_OK) {
				fprintf(stderr, "invalid instance \n");
				return r;
			}

			printf("instance:%d, parent:%s\n", id, parent_name);

			node_t *n = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, parent_name, NULL);
			if (!n) {
				fprintf(stderr, "dm_get_xml_config: unable to add parameter node\n");
				return -1;
			}

			while (dm_list_to_xml(&container, &n, elem_node, parent_name, filter) == RC_OK);

			break;
		}
	/* is one children element */
	case AVP_ELEMENT: {

		printf("AVP_ELEMENT\n");
		uint32_t type;
		char *value = NULL;
		int len = 0;
		node_t *n = NULL;

		if ((r = dm_expect_string_type(&container, AVP_NAME, VP_TRAVELPING, &name)) != RC_OK
			|| (r = dm_expect_uint32_type(&container, AVP_TYPE, VP_TRAVELPING, &type)) != RC_OK
			|| (r = dm_expect_avp(&container, &code, &vendor_id, &data, &size)) != RC_OK){
			return r;
		}

		printf("element:%s\n", name);

		if ((r = dm_decode_unknown_as_string(code, data, size, &value)) != RC_OK) {
			fprintf(stderr, "unable to decode value from mand\n");
			return r;
		}

		if (!strcmp(name, "type"))
			len = asprintf(&value, "%s:%s", "ianaift", value);
		if (len < 0) {
			fprintf(stderr, "dm_get_xml_config: unable to add value type\n");
			return -1;
		}

		//printf("value:%s\n", value);
		n = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, name, value);
		if (!n)
			fprintf(stderr, "dm_get_xml_config: unable to add parameter node\n");
		else {
			if (!strcmp(name, "type"))
				roxml_add_node(n, 0, ROXML_ATTR_NODE, "xmlns:ianaift", "urn:ietf:params:xml:ns:yang:iana-if-type");
		}

		free(value);

		if(filter && !strcmp(name, filter))
			return -1;

		break;
		}

		default: fprintf(stderr, "unknown code for type:%d\n", code);
	}

	return RC_OK;
}


int dm_get_xml_config(node_t *filter_root, node_t *filter_node, node_t **xml_out, char *path)
{
	int rc = -1;

	if (!filter_root || !filter_node) {
		printf("invalid filter or node\n");
		return rc;
	}

	node_t *next_node = *xml_out;
	char *current_path = talloc_strdup(NULL, path ? : "");
	char *node_name = roxml_get_name(filter_node, NULL, 0);
	int is_key = is_list_key(node_name);

	/* if not root node or key node add to path */
	if (strcmp(node_name, "filter") && !is_key) {
		current_path = talloc_asprintf_append(current_path, "%s%s", strlen(path) ? "." : "", node_name);
		if (!current_path || !strlen(current_path)) {
			fprintf(stderr, "unable to reallocate\n");
			goto out;
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

	printf("path:%s\n", current_path);

	/* we are always looking for the last child node for getting data */
	node_t *child = roxml_get_chld(filter_node, NULL, 0);
	if (child) {
		rc = dm_get_xml_config(filter_root, child, &next_node, current_path);
		goto out;
	}

	/* check if content match node */
	char *node_content = roxml_get_content(filter_node, NULL, 0, NULL);
	if (node_content && strlen(node_content)) {
		printf("content match node:%s\n", node_content);

		int instance = dm_get_instance(current_path, node_name, node_content);
		if(!instance)
			goto out;

		current_path = talloc_asprintf_append(current_path, ".%d", instance);

		printf("path is now:%s\n", current_path);

		node_t *s = filter_node;
		while ((s = roxml_get_next_sibling(s))) {

			char *n = roxml_get_name(s, NULL, 0);
			if(!n) {
				fprintf(stderr, "unable to get node name\n");
				goto out;
			}

			node_t *c = roxml_get_chld(s, NULL, 0);
			if(c) {
				printf("this node:%s has child\n",n);
				char *p = talloc_asprintf_append(current_path, ".%s", n);
				next_node = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, n, NULL);
				if(!strcmp(n, "ipv4") || !strcmp(n, "ipv6"))
					roxml_add_node(next_node, 0, ROXML_ATTR_NODE, "xmlns", "urn:ietf:params:xml:ns:yang:ietf-ip");
				dm_get_xml_config(filter_root, c, &next_node, p);
				continue;
			}

			int req_size = strlen(current_path) + strlen(n) + 200;
			char request[req_size];

			snprintf(request, req_size, "%s.%s", current_path, n);
			printf("request:%s\n", request);

			uint32_t code;
			char *v = dm_get_parameter(request, &code);

			printf("match got name:%s - val:%s\n", n, v);

			printf("code is :%d\n", code);
			if((v && (code == AVP_ARRAY || code == AVP_TABLE || code == AVP_OBJECT || code == AVP_INSTANCE || code == AVP_TYPE)) || code == AVP_ARRAY) {
				printf("table element\n");
				DM2_AVPGRP a = DM2_AVPGRP_INITIALIZER;
				if (rpc_db_list(ctx, 0, request, &a) != RC_OK) {
					fprintf(stderr, "unable to get list from mand\n");
					dm_release_avpgrp(&a);
					free(v);
					goto out;
				}
				while (dm_list_to_xml(&a, xml_out, 1, NULL, NULL) == RC_OK);
				dm_release_avpgrp(&a);
			}
			else {
				roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, n, v);
			}
			free(v);
		};

		printf("all done\n");
		rc = 0;
		goto out;
	}


	/* get paramater/list from mand */
	DM2_AVPGRP answer = DM2_AVPGRP_INITIALIZER;
	if ((rc = rpc_db_list(ctx, 0, current_path, &answer)) != RC_OK) {
		fprintf(stderr, "dmconfig: couldn't get list for path:%s\n", current_path);
		dm_release_avpgrp(&answer);
		rc = -1;
		goto out;
	}

	uint32_t code, vendor_id; void *data; size_t size;
	rc = dm_expect_avp(&answer, &code, &vendor_id, &data, &size);
	if (rc != RC_OK) {
		fprintf(stderr, "unable to get type from mand\n");
		dm_release_avpgrp(&answer);
		rc = -1;
		goto out;
	}

	/* what type of data did we got from mand */
	switch (code) {
		case AVP_ELEMENT: {
				uint32_t code;
				char *param_value = dm_get_parameter(current_path, &code);
				/* emtpy value is allowed */

				printf("got parameter back:%s\n", param_value);
				node_t *node_value = roxml_add_node(next_node, 0, ROXML_TXT_NODE, NULL, param_value);
				free(param_value);
				if (!node_value) {
					fprintf(stderr, "unable to set node value\n");
					dm_release_avpgrp(&answer);
					rc = -1;
					goto out;
				}
			}
		break;

		case AVP_ARRAY:
				roxml_del_curr(&next_node);

		case AVP_INSTANCE:
		case AVP_OBJECT:
		case AVP_TABLE:

			rpc_db_list(ctx, 0, current_path, &answer);

			/* check if key match node */
			if (is_key) {
				printf("key match node:%s\n", node_name);

				roxml_del_curr(&next_node);

				while (dm_list_to_xml(&answer, &next_node, 0, NULL, node_name) == RC_OK);
			}

			/* got list, add to response */
			else while (dm_list_to_xml(&answer, &next_node, 0, NULL, NULL) == RC_OK);
		break;
	}

	dm_release_avpgrp(&answer);

	/* move to the next node */
	node_t *s = roxml_get_next_sibling(filter_node);
	if (s) {
		printf("got sibling:%s\n", roxml_get_name(s, NULL, 0));
		rc = dm_get_xml_config(filter_root, s, xml_out, path);
	}

out:
	talloc_free(current_path);
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

	uint32_t code;
	ntp_enabled = dm_get_parameter("system.ntp.enabled", &code);
	if (!ntp_enabled) {
		fprintf(stderr, "unable to get system ntp state\n");
		goto exit;
	}

	printf("ntp state:%s\n", ntp_enabled);

	if (!strcmp(ntp_enabled, "1") || !strcmp(ntp_enabled, "true")) {
		rc = 1;
		goto exit;
	}

	if (!strcmp(ntp_enabled, "0") || !strcmp(ntp_enabled, "false")) {
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
		rp = NULL;
	}

	dm_release_avpgrp(&answer);
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

