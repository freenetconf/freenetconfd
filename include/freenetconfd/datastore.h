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

#ifndef __FREENETCONFD_DATASTORE_H__
#define __FREENETCONFD_DATASTORE_H__

#include <freenetconfd/plugin.h>

#include <roxml.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#endif

typedef struct datastore {
	char *name;
	char *value;
	char *ns;
	struct datastore *parent;
	struct datastore *child; // first child
	struct datastore *prev; // previous in the list
	struct datastore *next; // next in the list
	char *(*get) (struct datastore *self);
	void (*update) (struct datastore *self);
	/**
	 * set() - sets appropriate system value
	 *
	 * @value new value of appropriate system setting
	 *
	 * Return: you should return 0 on success, if you want to let
	 * freenetconf to update datastore value for you,
	 * -1 on error
	 */
	int (*set) (char *value);
	int (*set_multiple) (struct datastore *self, node_t *filter);
	int (*del) (struct datastore *self, void *data);
	int (*create) (struct datastore *self, void *data);
	int is_config;
	int is_list;
	int is_key;
} datastore_t;


typedef struct ds_key {
	char *name;
	char *value;
	struct ds_key *next;
} ds_key_t;

typedef struct ds_nip {
	node_t *node;
	struct ds_nip *next;
	int is_head;
} ds_nip_t;

enum ds_operation {OPERATION_MERGE, OPERATION_REPLACE = 0, OPERATION_CREATE, OPERATION_DELETE, OPERATION_REMOVE};

enum ds_operation ds_get_operation(node_t *node);

/**
 * datastore_print_key() - prints all key names and values on standard output
 * in a human readable format
 *
 * @key key to print
 */
void ds_print_key(ds_key_t *key);

/**
 * datastore_free_key() - recursively frees memory for all key parts
 *
 * @key key to free
 *
 * Requires next to be correctly initialized to NULL to mark the list end.
 * Only frees datastore_key_t structures, does NOT free strings,
 * you're going to have to free those yourself if you dynamically allocated
 * them manually.
 * Normally, roxml and datastore_free should handle this.
 */
void ds_free_key(ds_key_t *key);

/**
 * datastore_get_key_from_xml() - locates nodes with values to be used as keys
 *
 * @root xml node_t where the search starts
 * @our_root datastore_t node to compare what key values are
 *
 * Return: datastore_key_t created from found xml data
 *
 * locates nodes with values to be used as keys,
 * creates the datastore_key_t and returns the pointer to it. You're responsible
 * for freeing it (using datastore_free_key())
 *
 * our_root is useful for edit-config requests where we can't destinguish
 * between key values and values to set, if set to NULL, function will
 * use only root node_t for key extraction (this is ok for get funtions)
 */
ds_key_t *ds_get_key_from_xml(node_t *root, datastore_t *our_root);

/**
 * datastore_init() - inits datastore with name, value and namespace
 *
 * @datastore pointer to a datastore you'd like to init
 * @name string you'd like to use for a name
 * @value value it has
 * @ns namespace it belongs to
 *
 * Inits datastore with name, value and namespace you'd like to use and
 * puts all other values to default values so you don't have to worry about
 * them.
 */
void ds_init(datastore_t *datastore, char *name, char *value, char *ns);

/**
 * datastore_free() - frees the datastore you created with datastore_create()
 *
 * @datastore datastore you'd like to free
 * @free_siblings set to 1 when freeing the whole datastore, 0 for freeing just that node
 *
 * set the datastore to NULL after this call to be on the safe side
 */
void ds_free( datastore_t *datastore, int free_siblings );

/**
 * datastore_create() - creates datastore and inits it
 *
 * @name string you'd like to use for a name
 * @value value it has
 * @ns namespace it belongs to
 *
 * Return: pointer to the requested datastore
 */
datastore_t *ds_create(char *name, char *value, char *ns);

/**
 * ds_create_path() creates the same path in root, as that of path_endpoint
 *
 * Return: node in datastore that matches that of path_endpoint
 */
datastore_t *ds_create_path(datastore_t *root, node_t *path_endpoint);

/**
 * ds_set_value() - sets value to datastore
 *
 * @datastore datastore node to set value to
 * @value value to set
 *
 * Return: 0 on success, -1 on error
 *
 * Takes care of previous value and allocates memory for
 * new value. ds_free() takes care of freeing it so you're ok
 */
int ds_set_value(datastore_t *datastore, char *value);

/**
 * ds_set_is_config() - sets datastore is_donfig property
 * @datastore datastore node you're setting is_config on
 * @is_config 1 for true, 0 for false
 * @set_siblings set it to 0 when calling manually
 *
 * You should use this to set is_config instead of setting it
 * manually. It takes care of setting is_config to false on
 * children.
 */
void ds_set_is_config(datastore_t *datastore, int is_config, int set_siblings);

/**
 * datastore_add_child() - adds a child to datastore node
 *
 * @self datastore to add to
 * @child child you want to add
 * @target_name name to start counting from, can be NULL
 * @target_position position in the list of children, 0 to add on end
 *
 * You should use this function whenever you want to add a child.
 * It handles all the inner working of the datastore.
 */
void ds_add_child(datastore_t *self, datastore_t *child, char *target_name, int target_position);

datastore_t *ds_add_child_create(datastore_t *datastore, char *name, char *value, char *ns, char *target_name, int target_position);

datastore_t *ds_add_from_filter(datastore_t *datastore, node_t *filter_root, ds_nip_t *nip);

datastore_t *ds_find_sibling( datastore_t *root, char *name, char *value );

/**
 * ds_find_child() - finds roots child by name and value
 *
 * @root datastore node to start your search
 * @name name of the node you're searching for
 * @value value node contains, if NULL, finds first child with requested name
 */
datastore_t *ds_find_child(datastore_t *root, char *name, char *value);

/**
 * element_has_key_part() - Checks if an element of a list has a key part with "name" and "value"
 * @elem element of a list
 * @name name of the key tag
 * @value value of the key tag
 *
 * Return: 1 on key part found, 0 on key part not found
 * if value set to NULL, check if name tag is_key
 */
int ds_element_has_key_part(datastore_t *elem, char *name, char *value);

/**
 * element_has_key() - Checks if the element of a list has a key
 * @elem element of a list
 * @key key to check for
 *
 * Return: 1 on has key, 0 on doesn't have key
 */
int ds_element_has_key(datastore_t *elem, ds_key_t *key);

/**
 * list_has_key() - Checks if list has key
 * @list list to check
 *
 * Return: 1 on has key, 0 on doesn't have at least one key part
 */
int ds_list_has_key(datastore_t *list);


datastore_t *ds_find_node_by_key(datastore_t *our_root, ds_key_t *key);

void ds_get_all(datastore_t *our_root, node_t *out, int get_config, int check_siblings);

void ds_get_all_keys(datastore_t *our_root, node_t *out, int get_config);

void ds_get_list_data(node_t* filter_root, datastore_t* node, node_t* out, int get_config);

void ds_get_filtered(node_t *filter_root, datastore_t *our_root, node_t *out, int get_config);

/**
 * ds_edit_config()
 *
 * @nodes_in_processing set it to NULL when manually calling ds_edit_config
 */
int ds_edit_config(node_t *filter_root, datastore_t *our_root, ds_nip_t *nodes_in_processing);

#endif /* __FREENETCONFD_DATASTORE_H__ */
