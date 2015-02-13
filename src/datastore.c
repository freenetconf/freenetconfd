/*
 * Copyright (C) 2014 Cisco Systems, Inc.
 * Copyright (C) 2014 Sartura, Ltd.
 *
 * Author: Zvonimir Fras <zvonimir.fras@sartura.hr>
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

#include "freenetconfd/freenetconfd.h"
#include "freenetconfd/datastore.h"
#include "freenetconfd/plugin.h"

// nodes in processing implementation

static ds_nip_t *ds_nip_has_node(ds_nip_t *nip_list_head, node_t *node)
{
	if (!nip_list_head)
		return NULL;

	for (ds_nip_t *cur = nip_list_head->next; cur; cur = cur->next)
	{
		if (cur->node == node)
			return cur;
	}

	return NULL;
}

/**
 * ds_nip_add() adds node to nip_list_head
 *
 * @nip_list_head nip_list_head representing the list
 * @node node to add to the list
 *
 * Return: pointer to nip_list_head, NULL on error
 *
 * Creates new list if nip_list_head == NULL
 */
static ds_nip_t *ds_nip_add(ds_nip_t *nip_list_head, node_t *node)
{
	ds_nip_t *nip = malloc(sizeof(ds_nip_t));

	if (!nip)
	{
		ERROR("not enough memory\n");
		return NULL;
	}

	nip->node = node;
	nip->is_head = 0;

	if (!nip_list_head)
	{
		nip_list_head = malloc(sizeof(ds_nip_t));

		if (!nip_list_head)
		{
			ERROR("not enough memory\n");
			return NULL;
		}

		nip_list_head->is_head = 1;
		nip->next = NULL;
	}
	else
	{
		nip->next = nip_list_head->next;
	}

	nip_list_head->next = nip;

	return nip_list_head;
}

static ds_nip_t *ds_nip_add_unique(ds_nip_t *nip_list_head, node_t *node)
{
	if (!ds_nip_has_node(nip_list_head, node))
		return ds_nip_add(nip_list_head, node);

	return nip_list_head;
}

static void ds_free_nip(ds_nip_t *nip_list_head)
{
	if (!nip_list_head)
		return;

	ds_free_nip(nip_list_head->next);
	nip_list_head->next = NULL;
	free(nip_list_head);
}

/**
 * ds_nip_delete() deletes node from nip_list
 *
 * Return: 0 on success, -1 on not found
 */
static int ds_nip_delete(ds_nip_t *nip_list_head, node_t *node)
{
	if (!nip_list_head)
		return -1;

	ds_nip_t *cur, *prev;

	for (prev = nip_list_head, cur = nip_list_head->next; prev && cur; cur = cur->next, prev = prev->next)
	{
		if (cur->node == node)
		{
			prev->next = cur->next;
			free(cur);
			break;
		}
	}

	return 0;
}

enum ds_operation ds_get_operation(node_t *node)
{
	if (!node)
		return OPERATION_MERGE;

	node_t *operation_attr = roxml_get_attr(node, "operation", 0);
	char *operation = roxml_get_content(operation_attr, NULL, 0, NULL);

	if (!operation)
		return OPERATION_MERGE;

	if (!strcmp(operation, "delete"))
		return OPERATION_DELETE;

	if (!strcmp(operation, "remove"))
		return OPERATION_REMOVE;

	if (!strcmp(operation, "create"))
		return OPERATION_CREATE;

	if (!strcmp(operation, "merge"))
		return OPERATION_MERGE;

	if (!strcmp(operation, "replace"))
		return OPERATION_REPLACE;

	return OPERATION_MERGE;
}

void ds_print_key(ds_key_t *key)
{
	printf("KEY:\n");

	for (ds_key_t *cur = key; cur != NULL; cur = cur->next)
	{
		printf("name: %s\nvalue: %s\n-----------------\n", cur->name, cur->value);
	}
}

void ds_free_key(ds_key_t *key)
{
	if (key && key->next)
		ds_free_key(key->next);

	free(key);
}


// makes key from xml data
ds_key_t *ds_get_key_from_xml(node_t *root, datastore_t *our_root)
{
	int child_count = roxml_get_chld_nb(root);

	if (!child_count)
		return NULL;

	ds_key_t *rc = NULL;
	ds_key_t *cur_key = NULL;

	for (int i = 0; i < child_count; i++)
	{
		node_t *child = roxml_get_chld(root, NULL, i);
		char *key_name = roxml_get_name(child, NULL, 0);
		char *key_value = roxml_get_content(child, NULL, 0, NULL);

		// if it seems to be actual key
		if (key_name && key_name[0] != '\0' && key_value && key_value[0] != '\0')
		{
			// if datastore provided see if tag with key_name has is_key flag
			if (our_root && !ds_element_has_key_part(our_root, key_name, NULL))
				continue;

			// make sure key part is correctly allocated
			if (!rc)
			{
				rc = malloc(sizeof(ds_key_t));

				if (!rc)
				{
					ERROR("not enough memory\n");
					return NULL;
				}

				cur_key = rc;
			}
			else
			{
				cur_key->next = malloc(sizeof(ds_key_t));

				if (!cur_key->next)
				{
					ERROR("not enough memory\n");
					return NULL;
				}

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
	datastore->del = NULL;
	datastore->create_child = NULL;
	datastore->update = NULL;
	datastore->is_config = 1;
	datastore->is_list = datastore->is_key = 0;
	datastore->choice_group = 0;
}

void ds_free(datastore_t *datastore, int free_siblings)
{
	if (!datastore)
		return;

	// make a clear end if we're not deleting root node
	if (datastore->parent && !datastore->prev)
		datastore->parent->child = datastore->next;

	free(datastore->name);
	datastore->name = NULL;
	free(datastore->value);
	datastore->value = NULL;
	free(datastore->ns);
	datastore->ns = NULL;
	datastore->is_config = 1;
	datastore->is_list = datastore->is_key = 0;
	datastore->choice_group = 0;

	if (free_siblings)
	{
		ds_free(datastore->next, 1);
		datastore->next = NULL;
	}

	// keep the datastore clean and running
	if (datastore->next)
	{
		if (datastore->prev)
		{
			datastore->prev->next = datastore->next;
			datastore->next->prev = datastore->prev;
		}
		else
		{
			datastore->next->prev = NULL;
		}
	}
	else if (datastore->prev)
	{
		datastore->prev->next = NULL;
	}

	ds_free(datastore->child, 1);
	datastore->child = NULL;
	free(datastore);
}

datastore_t *ds_create(char *name, char *value, char *ns)
{
	datastore_t *datastore = malloc(sizeof(datastore_t));

	if (!datastore)
		return 0;

	ds_init(datastore,
			name ? strdup(name) : NULL,
			value ? strdup(value) : NULL,
			ns ? strdup(ns) : NULL);

	return datastore;
}

datastore_t *ds_create_path(datastore_t *root, node_t *path_endpoint)
{
	if (!root || !path_endpoint)
		return root;

	ds_nip_t *node_list = NULL;

	// get the list of nodes (path_endpoint's parent) in root-down order
	for (node_t *cur = path_endpoint; strcmp(roxml_get_name(cur, NULL, 0), "config"); cur = roxml_get_parent(cur))
	{
		DEBUG("post-nip.add( %s )\n", roxml_get_name(cur, NULL, 0));
		node_list = ds_nip_add(node_list, cur);
	}

	// get the real root of the plugin
	root = root->parent;

	// go down and create one by one
	for (ds_nip_t *cur = node_list->next; cur; cur = cur->next)
	{
		char *cur_name = roxml_get_name(cur->node, NULL, 0);
		char *cur_value = roxml_get_content(cur->node, NULL, 0, NULL);

		if (!strlen(cur_value)) cur_value = NULL;

		DEBUG("ds.create( %s, %s )\n", cur_name, cur_value);

		datastore_t *child = ds_find_child(root, cur_name, cur_value);

		if (!child)
		{
			root = root->create_child ? (datastore_t *) root->create_child(root, cur_name, cur_value, NULL, NULL, 0)
									  : ds_add_child_create(root, cur_name, cur_value, NULL, NULL, 0);
			if (root->set)
				root->set(root, cur_value);
		}
		else
		{
			root = child;
		}

		if (!child)
			DEBUG("\tcreated %s->%s\n", root->parent->name, root->name);
	}

	ds_free_nip(node_list);

	return root;
}

int ds_purge_choice_group(datastore_t *parent, int choice_group)
{
	if (!parent)
		return -1;

	if(!choice_group)
		return 0;

	for (datastore_t *child = parent->child; child; )
	{
		datastore_t *tmp = child;
		child = child->next;

		if (tmp->choice_group == choice_group)
			ds_free(tmp, 0);
	}

	return 0;
}

int ds_set_value(datastore_t *datastore, char *value)
{
	if (!datastore || !value)
		return -1;

	if (datastore->set)
	{
		int sr = datastore->set(datastore, value);

		if (sr)
			return RPC_ERROR; // TODO error-option
	}

	free(datastore->value);
	datastore->value = strdup(value);

	if (!datastore->value)
		return -1;

	DEBUG("ds_set_value( %s, %s )\n", datastore->name, value);

	return 0;
}

void ds_set_is_config(datastore_t *datastore, int is_config, int set_siblings)
{
	if (!datastore)
		return;

	datastore->is_config = is_config;

	if (is_config)
		return; // is_config is only recursively set if it's false

	if (set_siblings)
		ds_set_is_config(datastore->next, is_config, 1);

	ds_set_is_config(datastore->child, is_config, 1);
}


void ds_add_child(datastore_t *self, datastore_t *child, char *target_name, int target_position)
{
	if (self->child)
	{
		datastore_t *cur;
		int cur_pos = 0;

		if (target_name)
		{
			for (cur = self->child; cur->next != 0; cur = cur->next)
			{
				if (!strcmp(cur->name, target_name))
				{
					// if it's last of its name
					if (strcmp(cur->next->name, target_name))
						break;

					// if at desired position
					if (target_position && ++cur_pos >= target_position)
						break;
				}
			}
		}
		else
		{
			for (cur = self->child; cur->next != 0; cur = cur->next)
			{
				if (target_position && ++cur_pos >= target_position)
					break;
			}
		}

		child->next = cur->next;

		if (cur->next) cur->next->prev = child;

		cur->next = child;
		child->prev = cur;
	}
	else
	{
		self->child = child;
	}

	child->parent = self;

	ds_set_is_config(child, self->is_config, 0);
}

datastore_t *ds_add_child_create(datastore_t *datastore, char *name, char *value, char *ns, char *target_name, int target_position)
{
	datastore_t *child = ds_create(name, value, ns);

	if (!child)
		return 0;

	ds_add_child(datastore, child, target_name, target_position);

	return child;
}

datastore_t *ds_add_from_filter(datastore_t *datastore, node_t *filter_root, ds_nip_t *nip)
{
	if (!datastore || !filter_root)
		return NULL;

	char *name = roxml_get_name(filter_root, NULL, 0);
	DEBUG("add_from_filter( %s, %s )\n", name, roxml_get_content(filter_root, NULL, 0, NULL));
	DEBUG("\tadding_to %s->%s\n", datastore->parent->name, datastore->name);

	char *value = roxml_get_content(filter_root, NULL, 0, NULL);
	char *ns = roxml_get_content(roxml_get_ns(filter_root), NULL, 0, NULL);

	datastore_t *rc = datastore->create_child ? (datastore_t *) datastore->create_child(datastore, name, value, ns, name, 0)
											  : ds_add_child_create(datastore, name, value, ns, name, 0);
	if (rc->set)
		rc->set(rc, value);

	ds_nip_delete(nip, filter_root);

	int child_count = roxml_get_chld_nb(filter_root);

	for (int i = 0; i < child_count; i++)
	{
		node_t *child = roxml_get_chld(filter_root, NULL, i);

		ds_add_from_filter(rc, child, nip);
	}

	return rc;
}


datastore_t *ds_find_sibling(datastore_t *root, char *name, char *value)
{
	for (datastore_t *cur = root; cur != NULL; cur = cur->next)
	{
		// check name
		if (cur->name && !strcmp(cur->name, name))
		{
			// check value if requested
			if (value)
			{
				if (cur->value && !strcmp(cur->value, value))
					return cur;
			}
			else
			{
				return cur;
			}
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
	if (!name)
		return 0;

	for (datastore_t *cur = elem->child; cur != NULL; cur = cur->next)
	{
		if (cur->name && !strcmp(cur->name, name))
		{
			// names match
			if (!value || (cur->value && !strcmp(cur->value, value)))
			{
				// if we don't need value or values match
				return cur->is_key; // key found
			}
		}
	}

	return 0; // no key found
}

int ds_element_has_key(datastore_t *elem, ds_key_t *key)
{
	if (!elem || !key)
		return 0;

	for (ds_key_t *key_part = key; key_part != NULL; key_part = key_part->next)
	{
		if (!ds_element_has_key_part(elem, key_part->name, key_part->value))
			return 0; // doesn't have key if misses at least one key part
	}

	return 1; // found all key parts so has key
}

int ds_list_has_key(datastore_t *list)
{
	if (!list)
		return 0;

	for (datastore_t *cur = list->child; cur != NULL; cur = cur->next)
	{
		if (cur->is_key)
			return 1;
	}

	return 0;
}

datastore_t *ds_find_node_by_key(datastore_t *our_root, ds_key_t *key)
{
	for (datastore_t *cur = our_root; cur != NULL; cur = cur->next)
	{
		if (ds_element_has_key(cur, key))
			return cur;
	}

	return NULL;
}


void ds_get_all(datastore_t *our_root, node_t *out, int get_config, int check_siblings)
{
	if (!our_root)
		return;

	// skip non-configurable nodes if only configurable are requested
	if (get_config && !our_root->is_config)
	{
		// still have to check siblings, they may be configurable
		if (check_siblings)
			ds_get_all(our_root->next, out, get_config, 1);

		return;
	}

	if (our_root->update)
		our_root->update(our_root);

	// use get() if available
	char *value;

	if (our_root->get)
		value = our_root->get(our_root);
	else
		value = our_root->value;

	node_t *nn = roxml_add_node(out, 0, ROXML_ELM_NODE, our_root->name, value);

	if (our_root->ns)
		roxml_add_node(nn, 0, ROXML_ATTR_NODE, "xmlns", our_root->ns); // add namespace

	// free value if returned with get (get always allocates)
	if (our_root->get)
		free(value);

	if (check_siblings)
		ds_get_all(our_root->next, out, get_config, 1);

	ds_get_all(our_root->child, nn, get_config, 1);
}

void ds_get_all_keys(datastore_t *our_root, node_t *out, int get_config)
{
	if (!our_root || !out)
		return;

	// skip non-configurable nodes if only configurable are requested
	if (get_config && !our_root->is_config)
		return;

	if (our_root->update)
		our_root->update(our_root);

	for (datastore_t *parent_cur = our_root; parent_cur != NULL; parent_cur = parent_cur->next)
	{
		if (get_config && !parent_cur->is_config)
			continue; // skip non-configurable nodes if only configurable are requested

		node_t *parent_xml = roxml_add_node(out, 0, ROXML_ELM_NODE, parent_cur->name, NULL);

		for (datastore_t *cur = parent_cur->child; cur != NULL; cur = cur->next)
		{
			if (get_config && !cur->is_config)
				continue; // skip non-configurable nodes if only configurable are requested

			if (cur->is_key)
			{
				char *value;

				if (cur->get)
					value = cur->get(cur); // use get() if available
				else
					value = cur->value;

				roxml_add_node(parent_xml, 0, ROXML_ELM_NODE, cur->name, value);

				if (cur->get)
					free(value); // free value if returned with get (get always allocates)
			}
		}
	}
}

void ds_get_list_data(node_t *filter_root, datastore_t *node, node_t *out, int get_config)
{
	// skip non-configurable nodes if only configurable are requested
	if (get_config && !node->is_config)
		return;

	int child_count = roxml_get_chld_nb(filter_root);

	for (int i = 0; i < child_count; i++)
	{
		node_t *cur = roxml_get_chld(filter_root, NULL, i);

		char *name = roxml_get_name(cur, NULL, 0);
		char *value = roxml_get_content(cur, NULL, 0, NULL);

		if (value && strlen(value))
			continue; // skip if key has value

		datastore_t *our_cur = ds_find_child(node, name, NULL);
		ds_get_all(our_cur, out, get_config, 0);
	}
}

void ds_get_filtered(node_t *filter_root, datastore_t *our_root, node_t *out, int get_config)
{
	if (!our_root)
		return;

	// recursively check siblings
	node_t *filter_root_sibling = roxml_get_next_sibling(filter_root);

	if (filter_root_sibling && our_root->next)
	{
		ds_get_filtered(filter_root_sibling, our_root->next, out, get_config);
	}

	// skip non-configurable nodes if only configurable are requested
	if (get_config && !our_root->is_config)
		return;

	node_t *filter_root_child = roxml_get_chld(filter_root, NULL, 0);

	if (our_root->is_list && filter_root_child)
	{
		// handle list filtering
		if (!ds_list_has_key(our_root))
			return; // this shouldn't happen

		ds_key_t *key = ds_get_key_from_xml(filter_root, NULL);
		ds_print_key(key);

		if (!key)
		{
			ds_get_all_keys(our_root, out, get_config);
			return;
		}

		datastore_t *node = ds_find_node_by_key(our_root, key);
		ds_free_key(key);

		if (!node)
			DEBUG("node IS NULL\n");
		else
			DEBUG("node name: %s\nfilter_root name: %s\n", node->name, roxml_get_name(filter_root, NULL, 0));

		ds_get_list_data(filter_root, node, out, get_config);
	}
	else if (filter_root_child)
	{
		// we're not calling update() sooner because ds_get_all and ds_get_all_keys
		// will call it too and we don't want to call it twice in the same get
		if (our_root->update)
			our_root->update(our_root);

		out = roxml_add_node(out, 0, ROXML_ELM_NODE, our_root->name, NULL);

		if (our_root->ns)
			roxml_add_node(out, 0, ROXML_ATTR_NODE, "xmlns", our_root->ns); // add namespace

		datastore_t *our_child = ds_find_child(our_root, roxml_get_name(filter_root_child, NULL, 0), NULL);
		ds_get_filtered(filter_root_child, our_child, out, get_config);
	}
	else if (our_root->is_list)
	{
		// leaf list

		// we're not calling update() sooner because ds_get_all and ds_get_all_keys
		// will call it too and we don't want to call it twice in the same get
		if (our_root->update)
			our_root->update(our_root);

		for (datastore_t *cur = our_root; cur != NULL; cur = cur->next)
		{
			if (!strcmp(cur->name, our_root->name))
			{
				char *value;

				if (cur->get)
					value = cur->get(cur); // use get() if available
				else
					value = cur->value;

				roxml_add_node(out, 0, ROXML_ELM_NODE, our_root->name, value);

				if (cur->get)
					free(value); // free value if returned with get (get always allocates)
			}
		}
	}
	else
	{
		ds_get_all(our_root, out, get_config, 0);
	}
}

int ds_edit_config(node_t *filter_root, datastore_t *our_root, ds_nip_t *nodes_in_processing)
{
	if (!filter_root)
		return 0;

	// always add nodes to nip if they exist
	ds_nip_t *nip = ds_nip_add_unique(nodes_in_processing, filter_root);

	if(!our_root)
		return 0;

	// finding match
	char *filter_name = roxml_get_name(filter_root, NULL, 0);

	if (!filter_name || !our_root->name)
		return -1;

	int rc = 0;

	DEBUG("\t\tfilter: %s\t our: %s\n", filter_name, our_root->name);

	// names differ
	if (strcmp(filter_name, our_root->name))
	{
		// search in next or child element
		rc = ds_edit_config(filter_root, our_root->next ? our_root->next : our_root->child, nip);
	}
	else
	{
		// names match

		// check operation delete or remove,
		// no need to check other operations here since we can't do anything about it
		// and have to go in anyway
		enum ds_operation operation = ds_get_operation(filter_root);

		if (operation == OPERATION_DELETE || operation == OPERATION_REMOVE)
		{
			datastore_t *child = our_root;

			if (our_root->is_list)
			{
				if (ds_list_has_key(our_root))
				{
					ds_key_t *key = ds_get_key_from_xml(filter_root, our_root);
					child = ds_find_node_by_key(our_root, key);
					ds_free_key(key);
				}
				else
				{
					child = ds_find_child(our_root->parent,
										  roxml_get_name(filter_root, NULL, 0),
										  roxml_get_content(filter_root, NULL, 0, NULL)
										 );
				}
			}

			if (!child)
			{
				if (operation == OPERATION_DELETE)
					return RPC_DATA_MISSING;

				return 0;
			}

			DEBUG("delete( %s, %s )\n", child->name, child->value);

			if (child->del)
				child->del(child, NULL); // TODO figure out what del() does and what it needs to take as arguments

			ds_free(child, 0);
			ds_nip_delete(nip, filter_root);

			return 0;
		}
		else if (operation == OPERATION_CREATE)
		{
			DEBUG("%s exists and cannot be created\n", roxml_get_name(filter_root, NULL, 0));
			return RPC_DATA_EXISTS;
		}

		if (our_root->set_multiple)
		{
			int smr = our_root->set_multiple(our_root, filter_root);
			DEBUG("set_multiple( %s, %s )\n", our_root->name, roxml_get_name(filter_root, NULL, 0));

			if (smr)
				return RPC_ERROR; // TODO error-option
		}

		if (our_root->is_list)
		{
			if (ds_list_has_key(our_root))
			{
				// list
				ds_key_t *key = ds_get_key_from_xml(filter_root, our_root);
				datastore_t *node = ds_find_node_by_key(our_root, key);

				if (!node)
				{
					// if we're here, operation is merge
					// if the node with specified key doesn't exist,
					// we should create it
					// since filter_root is already in nip, we gracefully exit
					// node will be created
					ds_free_key(key);
					return 0;
				}

				// replace values in datastore for all the values in filter
				// remove key from xml
				for (ds_key_t *key_part = key; key_part; key_part = key_part->next)
				{
					roxml_del_node(roxml_get_nodes(filter_root, ROXML_ELM_NODE, key_part->name, 0));
				}

				// foreach elem in xml first layer
				int child_count = roxml_get_chld_nb(filter_root);

				for (int i = 0; i < child_count; i++)
				{
					node_t *elem = roxml_get_chld(filter_root, NULL, i);

					// recursive call to edit configs based on filter
					rc = ds_edit_config(elem, node->child, nip);

					if (rc)
					{
						DEBUG("!!!!!!!!!!!rc\n");
						ds_free_key(key);
						goto exit_edit; // immediatelly return on error // TODO error-option
					}
				}

				ds_free_key(key);
			}
			else
			{
				// leaf list (actually list without a key)
				ds_add_from_filter(our_root->parent, filter_root, nip);
			}

			ds_nip_delete(nip, filter_root);
		}
		else
		{
			int child_count = roxml_get_chld_nb(filter_root);

			if (child_count)
			{
				for (int i = 0; i < child_count; i++)
				{
					node_t *elem = roxml_get_chld(filter_root, NULL, i);

					// recursive call to edit configs based on filter
					rc = ds_edit_config(elem, our_root->child, nip);

					if (rc)
						goto exit_edit; // immediatelly return on error // TODO error-option
				}
			}
			else
			{
				// "normal"
				char *value = roxml_get_content(filter_root, NULL, 0, NULL);

				DEBUG("set( %s, %s )\n", our_root->name, value);

				ds_set_value(our_root, value);
			}

			ds_nip_delete(nip, filter_root);
		}
	}

exit_edit:

	if (!nodes_in_processing) // original call, recursion is done!
	{
		for (ds_nip_t *cur = nip->next; cur; cur = cur->next)
		{
			DEBUG("processing %s->%s\n", roxml_get_name(roxml_get_parent(cur->node), NULL, 0), roxml_get_name(cur->node, NULL, 0));
			enum ds_operation cur_operation = ds_get_operation(cur->node);

			if (cur_operation == OPERATION_DELETE)
			{
				// we should have deleted this but it doesn't exist in datastore
				ds_free_nip(nip);
				return RPC_DATA_MISSING; // TODO handle error-option
			}
			else if (cur_operation == OPERATION_REMOVE)
			{
				continue;
			}
			else // create or merge or replace but needs to create the node
			{
				datastore_t *nn = ds_create_path(our_root, cur->node);
				ds_set_value(nn, roxml_get_content(cur->node, NULL, 0, NULL));

				// add whole trees if they are missing
				int child_count = roxml_get_chld_nb(cur->node);
				for (int i = 0; i < child_count; i++)
				{
					ds_add_from_filter(nn, roxml_get_chld(cur->node, NULL, i), nip);
				}
			}
		}

		ds_free_nip(nip);
	}

	return rc;
}
