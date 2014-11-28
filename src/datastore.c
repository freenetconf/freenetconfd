#include "datastore.h"
#include "freenetconfd.h"
#include "freenetconfd/plugin.h"


void ds_print_key(ds_key_t* key)
{
	printf("KEY:\n");
	for (ds_key_t *cur = key; cur != NULL; cur = cur->next) {
		printf("name: %s\nvalue: %s\n-----------------\n", cur->name, cur->value);
	}
}

void ds_free_key(ds_key_t* key)
{
	if (key && key->next) ds_free_key(key->next);
	free(key);
}


// makes key from xml data
ds_key_t *ds_get_key_from_xml(node_t *root, datastore_t *our_root)
{
	int child_count = roxml_get_chld_nb(root);
	if (!child_count) return NULL;

	ds_key_t *rc = NULL;
	ds_key_t *cur_key = NULL;

	for (int i = 0; i < child_count; i++) {
		node_t *child = roxml_get_chld(root, NULL, i);
		char *key_name = roxml_get_name(child, NULL, 0);
		char *key_value = roxml_get_content(child, NULL, 0, NULL);

		// if it seems to be actual key
		if (key_name && key_name[0] != '\0' && key_value && key_value[0] != '\0') {
			// if datastore provided see if tag with key_name has is_key flag
			if (our_root && !ds_element_has_key_part(our_root, key_name, NULL))
				continue;
			if (!rc) {
				rc = malloc(sizeof(ds_key_t));
				cur_key = rc;
			} else {
				cur_key->next = malloc(sizeof(ds_key_t));
				cur_key = cur_key->next;
			}
			cur_key->name = roxml_get_name(child, NULL, 0);
			cur_key->value = key_value;
			cur_key->next = NULL;
		}
	}

	return rc;
}

void ds_init(datastore_t *datastore, char *name, char *value, char *ns)
{
	datastore->name = name;
	datastore->value = value;
	datastore->ns = ns;
	datastore->parent = datastore->child = datastore->prev = datastore->next = NULL;
	datastore->get = NULL;
	datastore->set = NULL;
	datastore->set_multiple = NULL;
	datastore->del = datastore->create = NULL;
	datastore->update = NULL;
	datastore->is_config = 1;
	datastore->is_list = datastore->is_key = 0;
}

void ds_free(datastore_t *datastore, int free_siblings)
{
	if (!datastore) return;

	DEBUG("name: %s\tfree_siblings: %d\n", datastore->name, free_siblings);
	// make a clear end if we're not deleting root node
	if (datastore->parent && !datastore->prev) datastore->parent->child = datastore->next;

	free(datastore->name); datastore->name = NULL;
	free(datastore->value); datastore->value = NULL;
	free(datastore->ns); datastore->ns = NULL;
	datastore->is_config = 1;
	datastore->is_list = datastore->is_key = 0;

	if (free_siblings) {
		ds_free(datastore->next, 1);
		datastore->next = NULL;
	}

	// keep the datastore clean and running
	if (datastore->next) {
		if (datastore->prev) {
			datastore->prev->next = datastore->next;
			datastore->next->prev = datastore->prev;
		}
		else datastore->next->prev = NULL;
	} else if (datastore->prev) datastore->prev->next = NULL;

	ds_free(datastore->child, 1); datastore->child = NULL;
	DEBUG("prev: %s\tnext: %s\n", datastore->prev ? datastore->prev->name : "null", datastore->next ? datastore->next->name : "null");
	free(datastore);
}

datastore_t *ds_create(char *name, char *value, char *ns)
{
	datastore_t *datastore = malloc(sizeof(datastore_t));
	if (!datastore) return 0;

	   ds_init(datastore,
				name ? strdup(name) : NULL,
				value ? strdup(value) : NULL,
				ns ? strdup(ns) : NULL);

	return datastore;
}

int ds_set_value(datastore_t *datastore, char *value)
{
	if (!datastore || !value) return -1;

	free(datastore->value);
	datastore->value = strdup(value);

	if (!datastore->value) return -1;

	return 0;
}

void ds_set_is_config(datastore_t *datastore, int is_config, int set_siblings)
{
	if (!datastore) return;

	datastore->is_config = is_config;
	if (is_config) return; // is_config is only recursively set if it's false

	if (set_siblings)
		ds_set_is_config(datastore->next, is_config, 1);

	ds_set_is_config(datastore->child, is_config, 1);
}


void ds_add_child(datastore_t *self, datastore_t *child, char *target_name, int target_position)
{
	if (self->child) {
		datastore_t *cur;
		int cur_pos = 0;
		if (target_name) {
			for(cur = self->child; cur->next != 0; cur = cur->next) {
				if (!strcmp(cur->name, target_name)) {
					// if it's last of its name
					if (strcmp(cur->next->name, target_name)) break;
					// if at desired position
					if (target_position && ++cur_pos >= target_position) break;
				}
			}
		}
		else {
			for(cur = self->child; cur->next != 0; cur = cur->next) {
				if (target_position && ++cur_pos >= target_position) break;
			}
		}
		child->next = cur->next;
		if (cur->next) cur->next->prev = child;
		cur->next = child;
		child->prev = cur;
	} else {
		self->child = child;
	}

	child->parent = self;

	ds_set_is_config(child, self->is_config, 0);
}

datastore_t *ds_add_child_create(datastore_t *datastore, char *name, char *value, char *ns, char *target_name, int target_position)
{
	datastore_t *child = ds_create(name, value, ns);
	if (!child) return 0;

	ds_add_child(datastore, child, target_name, target_position);

	return child;
}

datastore_t *ds_add_from_filter(datastore_t *datastore, node_t *filter_root)
{
	if (!datastore || !filter_root) return NULL;

	char *name = roxml_get_name(filter_root, NULL, 0);
	datastore_t *rc = ds_add_child_create(datastore,
										  name,
										  roxml_get_content(filter_root, NULL, 0, NULL),
										  roxml_get_content(roxml_get_ns(filter_root), NULL, 0, NULL),
										  name, 0
										 );

	int child_count = roxml_get_chld_nb(filter_root);
	for (int i = 0; i < child_count; i++) {
		node_t *child = roxml_get_chld(filter_root, NULL, i);

		ds_add_from_filter(rc, child);
	}

	return rc;
}


datastore_t *ds_find_sibling(datastore_t *root, char *name, char *value)
{
	for(datastore_t *cur = root; cur != NULL; cur = cur->next) {
		// check name
		if (cur->name && !strcmp(cur->name, name)) {
			// check value if requested
			if (value) {
				if (cur->value && !strcmp(cur->value, value)) return cur;
			}
			else return cur;
		}
	}
	return NULL;
}

datastore_t *ds_find_child(datastore_t *root, char *name, char *value)
{
	return ds_find_sibling(root->child, name, value);
}

int ds_element_has_key_part(datastore_t *elem, char *name, char *value)
{
	if (!name) return 0;

	for(datastore_t *cur = elem->child; cur != NULL; cur = cur->next) {
		if (cur->name && !strcmp(cur->name, name)) {
			// names match
			if (!value || (cur->value && !strcmp(cur->value, value))) {
				// if we don't need value or values match
				return 1; // key found
			}
		}
	}

	return 0; // no key found
}

int ds_element_has_key(datastore_t *elem, ds_key_t *key)
{
	if (!elem || !key) return 0;

	for(ds_key_t *key_part = key; key_part != NULL; key_part = key_part->next) {
		if (!ds_element_has_key_part(elem, key_part->name, key_part->value)) {
			return 0; // doesn't have key if misses at least one key part
		}
	}

	return 1; // found all key parts so has key
}

int ds_list_has_key(datastore_t *list)
{
	if (!list) return 0;

	for (datastore_t *cur = list->child; cur != NULL; cur = cur->next) {
		if (cur->is_key) return 1;
	}

	return 0;
}

datastore_t *ds_find_node_by_key(datastore_t *our_root, ds_key_t *key)
{
	for(datastore_t *cur = our_root; cur != NULL; cur = cur->next) {
		if (ds_element_has_key(cur, key)) return cur;
	}

	return NULL;
}

void ds_get_all(datastore_t *our_root, node_t *out, int get_config, int check_siblings)
{
	if (!our_root) return;

	// skip non-configurable nodes if only configurable are requested
	if (get_config && !our_root->is_config) {
		// still have to check siblings, they may be configurable
		if (check_siblings) {
			ds_get_all(our_root->next, out, get_config, 1);
		}
		return;
	}

	if (our_root->update) our_root->update(our_root);

	// use get() if available
	char *value;
	if (our_root->get)
		value = our_root->get(our_root);
	else
		value = our_root->value;

	node_t *nn = roxml_add_node(out, 0, ROXML_ELM_NODE, our_root->name, value);
	if (our_root->ns) roxml_add_node(nn, 0, ROXML_ATTR_NODE, "xmlns", our_root->ns); // add namespace

	// free value if returned with get (get always allocates)
	if (our_root->get) free(value);

	if (check_siblings) {
		ds_get_all(our_root->next, out, get_config, 1);
	}

	ds_get_all(our_root->child, nn, get_config, 1);
}

void ds_get_all_keys(datastore_t *our_root, node_t *out, int get_config)
{
	if (!our_root || !out) return;

	// skip non-configurable nodes if only configurable are requested
	if (get_config && !our_root->is_config) return;

	if (our_root->update) our_root->update(our_root);

	for (datastore_t *parent_cur = our_root; parent_cur != NULL; parent_cur = parent_cur->next) {
		if (get_config && !parent_cur->is_config) continue; // skip non-configurable nodes if only configurable are requested
		node_t *parent_xml = roxml_add_node(out, 0, ROXML_ELM_NODE, parent_cur->name, NULL);
		for (datastore_t *cur = parent_cur->child; cur != NULL; cur = cur->next) {
			if (get_config && !cur->is_config) continue; // skip non-configurable nodes if only configurable are requested
			if (cur->is_key) {
				char *value;
				if (cur->get)
					value = cur->get(cur); // use get() if available
				else
					value = cur->value;
				roxml_add_node(parent_xml, 0, ROXML_ELM_NODE, cur->name, value);
				if (cur->get) free(value); // free value if returned with get (get always allocates)
			}
		}
	}
}

void ds_get_list_data(node_t *filter_root, datastore_t *node, node_t *out, int get_config)
{
	// skip non-configurable nodes if only configurable are requested
	if (get_config && !node->is_config) return;

	int child_count = roxml_get_chld_nb(filter_root);

	for (int i = 0; i < child_count; i++) {
		node_t *cur = roxml_get_chld(filter_root, NULL, i);

		char *name = roxml_get_name(cur, NULL, 0);
		char *value = roxml_get_content(cur, NULL, 0, NULL);
		if (value && strlen(value)) continue; // skip if key has value
		datastore_t *our_cur = ds_find_child(node, name, NULL);
		ds_get_all(our_cur, out, get_config, 0);
	}
}

void ds_get_filtered(node_t *filter_root, datastore_t *our_root, node_t *out, int get_config)
{
	if (!our_root) return;

	// recursively check siblings
	node_t *filter_root_sibling = roxml_get_next_sibling(filter_root);
	if (filter_root_sibling && our_root->next) {
		ds_get_filtered(filter_root_sibling, our_root->next, out, get_config);
	}

	// skip non-configurable nodes if only configurable are requested
	if (get_config && !our_root->is_config) return;

	node_t *filter_root_child = roxml_get_chld(filter_root, NULL, 0);

	if (our_root->is_list && filter_root_child) {
		// handle list filtering
		if (!ds_list_has_key(our_root)) return; // this shouldn't happen

		ds_key_t *key = ds_get_key_from_xml(filter_root, NULL);
		ds_print_key(key);
		if (!key) {
			ds_get_all_keys(our_root, out, get_config);
			return;
		}

		datastore_t *node = ds_find_node_by_key(our_root, key);
		ds_free_key(key);

		if (!node) DEBUG("node IS NULL\n");
		else DEBUG("node name: %s\nfilter_root name: %s\n", node->name, roxml_get_name(filter_root, NULL, 0));

		ds_get_list_data(filter_root, node, out, get_config);
	} else if (filter_root_child) {
		// we're not calling update() sooner because ds_get_all and ds_get_all_keys
		// will call it too and we don't want to call it twice in the same get
		if (our_root->update) our_root->update(our_root);

		out = roxml_add_node(out, 0, ROXML_ELM_NODE, our_root->name, NULL);
		if (our_root->ns) roxml_add_node(out, 0, ROXML_ATTR_NODE, "xmlns", our_root->ns); // add namespace

		datastore_t *our_child = ds_find_child(our_root, roxml_get_name(filter_root_child, NULL, 0), NULL);
		ds_get_filtered(filter_root_child, our_child, out, get_config);
	} else if (our_root->is_list) {
		// leaf list

		// we're not calling update() sooner because ds_get_all and ds_get_all_keys
		// will call it too and we don't want to call it twice in the same get
		if (our_root->update) our_root->update(our_root);

		for (datastore_t *cur = our_root; cur != NULL; cur = cur->next) {
			if (!strcmp(cur->name, our_root->name)) {
				char *value;
				if (cur->get)
					value = cur->get(cur); // use get() if available
				else
					value = cur->value;
				roxml_add_node(out, 0, ROXML_ELM_NODE, our_root->name, value);
				if (cur->get) free(value); // free value if returned with get (get always allocates)
			}
		}
	} else {
		ds_get_all(our_root, out, get_config, 0);
	}
}

int ds_edit_config(node_t* filter_root, datastore_t* our_root)
{
	if (!filter_root || !our_root)
		return 0;

	// finding match
	char *filter_name = roxml_get_name(filter_root, NULL, 0);
	if (!filter_name || !our_root->name)
		return -1;

	int rc = 0;
	DEBUG("filter_name: %s\tour_root->name: %s\tour_root->is_list: %d\n", filter_name, our_root->name, our_root->is_list);
	// names differ
	if (strcmp(filter_name, our_root->name)) {
		// search in next or child element
		rc = ds_edit_config(filter_root, our_root->next ? our_root->next : our_root->child);
	} else {
		// names match
		if (our_root->set_multiple) {
			int smr = our_root->set_multiple(our_root, filter_root);
			if (smr) return RPC_ERROR;
		}
		if (our_root->is_list) {
			if (ds_list_has_key(our_root)) {
				// list
				ds_key_t *key = ds_get_key_from_xml(filter_root, our_root);
				datastore_t *node = ds_find_node_by_key(our_root, key);

				// we should be able to find the node with that key
				if (!node) return -1;

				// replace values in datastore for all the values in filter
				// remove key from xml
				for (ds_key_t *key_part = key; key_part; key_part = key_part->next) {
					roxml_del_node(roxml_get_nodes(filter_root, ROXML_ELM_NODE, key_part->name, 0));
				}

				// foreach elem in xml first layer
				int child_count = roxml_get_chld_nb(filter_root);
				for (int i = 0; i < child_count; i++)
				{
					node_t *elem = roxml_get_chld(filter_root, NULL, i);

					// recursive call to edit configs based on filter
					rc = ds_edit_config(elem, node->child);
					if (!rc) return rc; // immediatelly return on error
				}

				ds_free_key(key);
			} else {
				// leaf list (actually list without a key)
				// only delete those with operation="delete" attribute
				node_t *attr = roxml_get_attr(filter_root, "operation", 0);
				char *attr_value = roxml_get_content(attr, NULL, 0, NULL);
				if (attr_value && !strcmp(attr_value, "delete")) {
					// find element based on name and value
					datastore_t *child = ds_find_child(our_root->parent,
														roxml_get_name(filter_root, NULL, 0),
														roxml_get_content(filter_root, NULL, 0, NULL)
														);
					ds_free(child, 0);
				} else {
				// add all others
					ds_add_from_filter(our_root->parent, filter_root);
				}
			}
		} else {
			int child_count = roxml_get_chld_nb(filter_root);
			if (child_count) {
				for (int i = 0; i < child_count; i++)
				{
					node_t *elem = roxml_get_chld(filter_root, NULL, i);

					// recursive call to edit configs based on filter
					rc = ds_edit_config(elem, our_root->child);
					if (rc) return rc; // immediatelly return on error
				}
			} else {
				// "normal"
				char *value = roxml_get_content(filter_root, NULL, 0, NULL);
				if (our_root->set) {
					int sr = our_root->set(value);
					if (sr) return RPC_ERROR;
				}
				ds_set_value(our_root, value);
			}
		}
	}

	return rc;
}



