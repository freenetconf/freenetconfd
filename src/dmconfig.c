#include "dmconfig.h"
#include <inttypes.h>

/* max integer length on 64bit system */
#define _INT_LEN 21

/*
 * dm_init() - init libev and dmconfig parts
 *
 * @event_base*:
 * @DMCONTEXT:
 * DM_AVPGRP*:
 *
 * NOTE: init this only once
 */
int dm_init(struct event_base **base, DMCONTEXT *ctx, DM_AVPGRP **grp)
{
	int rc = -1;

	if (!(*base = event_init())) {
		fprintf(stderr,"dmconfig: event init failed\n");
		return rc;
	}

	dm_context_init(ctx, *base);

	rc = dm_init_socket(ctx, AF_INET);
	if (rc) {
		fprintf(stderr, "dmconfig: init socket failed\n");
		goto exit;
	}

	rc = dm_send_start_session(ctx, CMD_FLAG_READWRITE, NULL, NULL);
	if (rc) {
		fprintf(stderr, "dmconfig: send start session failed\n");
		goto exit;
	}

	*grp = dm_grp_new();
	if (*grp) rc = 0;

exit:
	return rc;
}

/*
 * dm_shutdown() - deinit libev and dmconfig parts
 *
 * @event_base*:
 * @DMCONTEXT:
 * DM_AVPGRP*:
 *
 */
void dm_shutdown(struct event_base **base, DMCONTEXT *ctx, DM_AVPGRP **grp)
{
	if (*grp) dm_grp_free(*grp);
	if (dm_context_get_sessionid(ctx))
		dm_send_end_session(ctx);
	dm_shutdown_socket(ctx);
	event_base_free(*base);
}
/*
 * dm_get_parameter() - get parameter from mand
 *
 * @char*:	key we are searching for
 *
 * Returns NULL on error or value if found. Must be freed.
 */
char* dm_get_parameter(char *key)
{
	if (!key) return NULL;

	DMCONTEXT ctx;
	DM_AVPGRP *grp = NULL, *ret_grp;
	struct event_base *base;

	uint32_t type, vendor_id;
	uint8_t	flags;
	void *data;
	size_t len;
	char *rp = NULL;
	int rc;

	rc = dm_init(&base, &ctx, &grp);
	if (rc) goto exit;

	rc = dm_grp_get_unknown(&grp, key);
	if (rc) goto exit;

	rc = dm_send_packet_get(&ctx, grp, &ret_grp);
	if (rc) goto exit;

	rc = dm_avpgrp_get_avp(ret_grp, &type, &flags, &vendor_id, &data, &len);
	if (rc) goto exit;

	dm_decode_unknown_as_string(type, data, len, &rp);

exit:
	dm_shutdown(&base, &ctx, &grp);
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
int dm_set_parameter(char *key, char *value)
{
	if(!key || !value) return -1;

	DMCONTEXT ctx;
	DM_AVPGRP *grp = NULL;
	struct event_base *base;
	int rc = -1;

	rc = dm_init(&base, &ctx, &grp);
	if (rc) goto exit;

	rc = dm_grp_set_unknown(&grp, key, value);
	if (rc) goto exit;

	rc = dm_send_packet_set(&ctx, grp);
	if (rc) goto exit;

	rc = 0;

exit:
	dm_shutdown(&base, &ctx, &grp);

	return rc;
}

/*
 * dm_commit() - commit pending changes
 *
 * Commits pending changes (set_parameter, add object...).
 * Returns zero on success.
 */
int dm_commit()
{
	DMCONTEXT ctx;
	DM_AVPGRP *grp = NULL;
	struct event_base *base;
	int rc = -1;

	rc = dm_init(&base, &ctx, &grp);
	if (rc) goto exit;

	rc = dm_send_commit(&ctx);
	if (rc) {
		fprintf(stderr, "dmconfig: couldn't commit changes\n");
		goto exit;
	}

exit:
	dm_shutdown(&base, &ctx, &grp);

	return rc;
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
uint16_t dm_get_instance(char *path, char *key, char *value)
{
	DMCONTEXT ctx;
	DM_AVPGRP *grp = NULL;
	struct event_base *base;
	int rc = -1;
	uint16_t instance = 0;

	if(!path) path = "";

	rc = dm_init(&base, &ctx, &grp);
	if (rc) goto exit;

	dm_grp_set_string(&grp, key, value);
	rc = dm_send_find_instance(&ctx, path, grp, &instance);

	if (rc) {
		fprintf(stderr, "dmconfig: couldn't get instance\n");
		goto exit;
	}

exit:
	dm_shutdown(&base, &ctx, &grp);

	return instance;
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
 */

int dm_set_parameters_from_xml(node_t *root, node_t *n)
{
	if (!n || !root) return -1;

	static char *path = NULL;
	static uint16_t instance = 0;

	char *name = roxml_get_name(n, NULL, 0);

	/* if node exists and it's not a key node (lists) */
	if (name && strcmp(name, "name") && strcmp(name, "ip")) {

		int path_len = path ? strlen(path) : 0;
		int name_len = strlen(name);
		path = realloc(path, path_len + name_len + 2);	/* '.' and '\0' */
		if (!path) {
			fprintf(stderr, "dm_set_parameters_from_xml: realloc failed\n");
			return -1;
		}

		memset(path + path_len, 0, name_len + 2);
		strncat(path, name, name_len);
		strncat(path, ".", 1);
	}

	/* use attribute instance if we saved one  */
	node_t *ninstance = roxml_get_attr(n, "instance", 0);
	if (ninstance) {
		char *instance = roxml_get_content(ninstance, NULL, 0, NULL);
		if (instance) {
			int path_len = path ? strlen(path) : 0;
			int instance_len = strlen(instance);
			path = realloc(path, path_len + instance_len + 2);
			if (!path) {
				fprintf(stderr, "dm_set_parameters_from_xml: realloc failed\n");
				return -1;
			}

			memset(path + path_len, 0, instance_len + 2);
			strncat(path, instance, instance_len);
			strncat(path, ".", 1);
		}
	}

	/* proccess value node and remove it after */
	char *val = roxml_get_content(n, NULL, 0, NULL);
	if (val != NULL && strlen(val)) {

		/* remove last '.' */
		path[strlen(path) -1] = 0;

		/* save key as attribue on parent */
		if (name && (!strcmp(name,"name") || !strcmp(name, "ip"))) {
			instance = dm_get_instance(path, name, val);
			if (!instance) {
				fprintf(stderr, "dm_set_parameters_from_xml: unable to get instance, mand bug?\n");
				/* if this happens it should be mand bug or wrong parsing
				 * try to workaround */
				instance = 1;
			}

			char cinstance[_INT_LEN];
			snprintf(cinstance, _INT_LEN, "%d", instance);

			node_t *parent = roxml_get_parent(n);
			if(!parent) {
				fprintf(stderr, "dm_set_parameters_from_xml: unable to get parent node\n");
				return -1;
			}
			roxml_add_node(parent, 0, ROXML_ATTR_NODE, "instance", cinstance);
		}

		/* remove 'else' for ability of changing 'key' value */
		/* I don't see that specified in YANG/NCS */
		else {
			dm_set_parameter(path, val);
			printf("parameter:%s=\"%s\"\n", path, val);
		}

		free(path);
		path = NULL;

		roxml_del_node(n);
		return dm_set_parameters_from_xml(root, root);
	}
	/* if no children anymore (all processed) remove this child */
	node_t *child = roxml_get_chld(n, NULL, 0);
	if (!child) {

		free(path);
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
 * @DM_AVPGRP*: mand request context
 * @node_t*: XML root which we are creating
 *
 */
uint32_t dm_list_to_xml(const char *prefix, DM_AVPGRP *grp, node_t **xml_out)
{
	//printf("prefix:%s\n", prefix);

	uint32_t r;
	DM_OBJ *container;
	char *name, *path;
	uint32_t type;

	if ((r = dm_expect_object(grp, &container)) != RC_OK) {
		return r;
	}

	if ((r = dm_expect_string_type(container, AVP_NODE_NAME, VP_TRAVELPING, &name)) != RC_OK) {
		return r;
	}

	if ((r = dm_expect_uint32_type(container, AVP_NODE_TYPE, VP_TRAVELPING, &type)) != RC_OK) {
		return r;
	}

	if (!(path = talloc_asprintf(container, "%s.%s", prefix, name)))
		return RC_ERR_ALLOC;

//	printf("path:%s\n", path);

		//printf("type:%d for name:%s\n", type, name);
	switch (type) {
		case NODE_PARAMETER:
			{
				uint32_t code;
				uint32_t vendor_id;
				void *data;
				size_t size;

				//printf("%s:", name);
				if (dm_expect_any(container, &code, &vendor_id, &data, &size) == RC_OK) {
					//printf("------------pathnside:%s:\n", path);
					char *value = NULL;

					switch(code) {
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
						}
						break;

						default:
							printf("unknown type:%d", code);
					}

					//printf("\n");
					printf("name:%s, val:%s\n", name, value);

					//if (!strcmp(name, "timezone-utc-offset")) break;

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
		case NODE_TABLE:
		case NODE_OBJECT:
			{
				printf("node:%s\n", name);
				DM_OBJ *obj;

				if ((r = dm_expect_object(container, &obj)) != RC_OK)
					return RC_OK;

				// if this is instance node, skip it
				/*
				if (!strcmp(name, "server")) {
					while (dm_list_to_xml(path, obj, xml_out) == RC_OK);
				}
				else */

				if (atol(name) || !strcmp(name, "search") || !strcmp(name, "user-authentication-order") || !strcmp(name, "user") || !strcmp(name, "authentication")) {
					//node_t *n = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, "server", NULL);
					while (dm_list_to_xml(path, obj, xml_out) == RC_OK);
				}
				else {
					node_t *n = roxml_add_node(*xml_out, 0, ROXML_ELM_NODE, name, NULL);
					if (!n)
						fprintf(stderr, "dm_get_xml_config: unable to add parameter node\n");
					else{
						if(!strcmp(name, "ipv4") || !strcmp(name, "ipv6"))
							roxml_add_node(n, 0, ROXML_ATTR_NODE, "xmlns", "urn:ietf:params:xml:ns:yang:ietf-ip");

						while (dm_list_to_xml(path, obj, &n) == RC_OK);
					}
				}

				break;
			}
		default:
			printf("unknown object: %s, type: %d\n", path ,type);

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

	DMCONTEXT ctx;
	DM_AVPGRP *grp = NULL;
	struct event_base *base;
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

	rc = dm_init(&base, &ctx, &grp);
	if (rc) goto exit;

	rc = dm_send_list(&ctx, path, 0, &grp);
	if (rc) {
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

		while (dm_list_to_xml(path, grp, &n) == RC_OK);
		char *param = dm_get_parameter(path);
		if (param) {
		}
	}

	dm_get_xml_config(filter_root, child, xml_out);

exit:
	//dm_shutdown(&base, &ctx, &grp);

	return rc;
}
