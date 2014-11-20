#include "datastore.h"
#include "freenetconfd.h"


void ds_print_key(datastore_key_t* key)
{
	printf("KEY:\n");
	for (datastore_key_t *cur = key; cur != NULL; cur = cur->next) {
		printf("name: %s\nvalue: %s\n-----------------\n", cur->name, cur->value);
	}
}

void ds_free_key(datastore_key_t* key)
{
	if (key && key->next) ds_free_key(key->next);
	free(key);
}


// makes key from xml data
datastore_key_t *ds_get_key_from_xml(node_t *root)
{
	int child_count = roxml_get_chld_nb(root);
	if (!child_count) return NULL;

	datastore_key_t *rc = NULL;
	datastore_key_t *cur_key = NULL;

	for (int i = 0; i < child_count; i++) {
		node_t *child = roxml_get_chld(root, NULL, i);
		char *key_value = roxml_get_content(child, NULL, 0, NULL);

		// if is actual key
		if (key_value && key_value[0] != '\0') {
			if (!rc) {
				rc = malloc(sizeof(datastore_key_t));
				cur_key = rc;
			} else {
				cur_key->next = malloc(sizeof(datastore_key_t));
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
	datastore->set = datastore->del = datastore->create = NULL;
	datastore->update = NULL;
	datastore->read_only = datastore->is_list = datastore->is_key = 0;
}

void ds_free(datastore_t *datastore)
{
	if (!datastore) return;
	free(datastore->name); datastore->name = NULL;
	free(datastore->value); datastore->value = NULL;
	free(datastore->ns); datastore->ns = NULL;
	datastore->read_only = datastore->is_list = datastore->is_key = 0;
	ds_free(datastore->next); datastore->next = NULL;
	ds_free(datastore->child); datastore->child = NULL;
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

void ds_add_child(datastore_t *self, datastore_t *child)
{
	if (self->child) {
		datastore_t *cur;
		for(cur = self->child; cur->next != 0; cur = cur->next) continue; // goto last child in the list
		cur->next = child;
		child->prev = cur;
	} else {
		self->child = child;
	}
	child->parent = self;
}

datastore_t *ds_add_child_create(datastore_t *datastore, char *name, char *value, char *ns)
{
	datastore_t *child = ds_create(name, value, ns);
	if (!child) return 0;

	ds_add_child(datastore, child);

	return child;
}

datastore_t *ds_find_sibling(datastore_t *root, char *name)
{
	for(datastore_t *cur = root; cur != NULL; cur = cur->next) {
		if (cur->name && !strcmp(cur->name, name)) return cur;
	}
	return NULL;
}

datastore_t *ds_find_child(datastore_t *root, char *name)
{
	return ds_find_sibling(root->child, name);
}

int ds_element_has_key_part(datastore_t *elem, char *name, char *value)
{
	if (!name || !value) return 0;

	DEBUG("elem: %s, %s %s\n", elem->name, name, value);

	for(datastore_t *cur = elem->child; cur != NULL; cur = cur->next) {
		if (cur->name && cur->value &&
			!strcmp(cur->name, name) && !strcmp(cur->value, value))
		{
			return 1; // key found
		}
	}

	return 0; // no key found
}

int ds_element_has_key(datastore_t *elem, datastore_key_t *key)
{
	if (!elem || !key) return 0;

	DEBUG("elem: %s key: %s\n", elem->name, key->name);

	for(datastore_key_t *key_part = key; key_part != NULL; key_part = key_part->next) {
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

datastore_t *ds_find_node_by_key(datastore_t *our_root, datastore_key_t *key)
{
	for(datastore_t *cur = our_root; cur != NULL; cur = cur->next) {
		if (ds_element_has_key(cur, key)) return cur;
	}

	return NULL;
}

void ds_get_all(datastore_t *our_root, node_t *out, int check_siblings)
{
	if (!our_root) return;

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
		ds_get_all(our_root->next, out, 1);
	}

	ds_get_all(our_root->child, nn, 1);
}

void ds_get_all_keys(datastore_t *our_root, node_t *out)
{
	if (!our_root || !out) return;

	if (our_root->update) our_root->update(our_root);

	for (datastore_t *parent_cur = our_root; parent_cur != NULL; parent_cur = parent_cur->next) {
		node_t *parent_xml = roxml_add_node(out, 0, ROXML_ELM_NODE, parent_cur->name, NULL);
		for (datastore_t *cur = parent_cur->child; cur != NULL; cur = cur->next) {
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

void ds_get_list_data(node_t *filter_root, datastore_t *node, node_t *out)
{
	int child_count = roxml_get_chld_nb(filter_root);

	for (int i = 0; i < child_count; i++) {
		node_t *cur = roxml_get_chld(filter_root, NULL, i);

		char *name = roxml_get_name(cur, NULL, 0);
		char *value = roxml_get_content(cur, NULL, 0, NULL);
		if (value && strlen(value)) continue; // skip if key has value
		datastore_t *our_cur = ds_find_child(node, name);
		ds_get_all(our_cur, out, 0);
	}
}

void ds_get_filtered(node_t *filter_root, datastore_t *our_root, node_t *out)
{
	if (!our_root) return;

	// recursively check siblings
	node_t *filter_root_sibling = roxml_get_next_sibling(filter_root);
	if (filter_root_sibling && our_root->next) {
		ds_get_filtered(filter_root_sibling, our_root->next, out);
	}

	node_t *filter_root_child = roxml_get_chld(filter_root, NULL, 0);

	if (our_root->is_list && filter_root_child) {
		// handle list filtering
		if (!ds_list_has_key(our_root)) return; // this shouldn't happen

		datastore_key_t *key = ds_get_key_from_xml(filter_root);
		ds_print_key(key);
		if (!key) {
			ds_get_all_keys(our_root, out);
			return;
		}

		datastore_t *node = ds_find_node_by_key(our_root, key);
		ds_free_key(key);

		if (!node) DEBUG("node IS NULL\n");
		else DEBUG("node name: %s\nfilter_root name: %s\n", node->name, roxml_get_name(filter_root, NULL, 0));

		ds_get_list_data(filter_root, node, out);
	} else if (filter_root_child) {
		// we're not calling update() sooner because ds_get_all and ds_get_all_keys
		// will call it too and we don't want to call it twice in the same get
		if (our_root->update) our_root->update(our_root);

		out = roxml_add_node(out, 0, ROXML_ELM_NODE, our_root->name, NULL);
		if (our_root->ns) roxml_add_node(out, 0, ROXML_ATTR_NODE, "xmlns", our_root->ns); // add namespace

		datastore_t *our_child = ds_find_child(our_root, roxml_get_name(filter_root_child, NULL, 0));
		ds_get_filtered(filter_root_child, our_child, out);
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
		ds_get_all(our_root, out, 0);
	}
}


