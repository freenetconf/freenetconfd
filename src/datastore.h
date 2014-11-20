#ifndef __DATASTORE_H_
#define __DATASTORE_H_

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
	int (*set) (struct datastore *self, void *data);
	int (*del) (struct datastore *self, void *data);
	int (*create) (struct datastore *self, void *data);
	void (*update) (struct datastore *self);
	int read_only;
	int is_list;
	int is_key;
} datastore_t;


typedef struct datastore_key {
	char *name;
	char *value;
	struct datastore_key *next;
} datastore_key_t;

/**
 * datastore_print_key() - prints all key names and values on standard output
 * in a human readable format
 *
 * @key key to print
 */
void ds_print_key(datastore_key_t *key);

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
void ds_free_key(datastore_key_t *key);

/**
 * datastore_get_key_from_xml() - locates nodes with values to be used as keys
 *
 * @root xml node_t where the search starts
 *
 * Return: datastore_key_t created from found xml data
 *
 * locates nodes with values to be used as keys,
 * creates the datastore_key_t and returns the pointer to it. You're responsible
 * for freeing it (using datastore_free_key())
 */
datastore_key_t *ds_get_key_from_xml(node_t *root);

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
 *
 * set the datastore to NULL after this call to be on the safe side
 */
void ds_free(datastore_t *datastore);

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
 * datastore_add_child() - adds a child to datastore node
 *
 * @self datastore to add to
 * @child child you want to add
 *
 * You should use this function whenever you want to add a child.
 * It handles all the inner working of the datastore.
 */
void ds_add_child(datastore_t *self, datastore_t *child);

datastore_t *ds_add_child_create(datastore_t *datastore, char *name, char *value, char *ns);

datastore_t *ds_find_sibling(datastore_t *root, char *name);

datastore_t *ds_find_child(datastore_t *root, char *name);

/**
 * element_has_key_part() - Checks if an element of a list has a key part with "name" and "value"
 * @elem element of a list
 * @name name of the key tag
 * @value value of the key tag
 *
 * Return: 1 on key part found, 0 on key part not found
 */
int ds_element_has_key_part(datastore_t *elem, char *name, char *value);

/**
 * element_has_key() - Checks if the element of a list has a key
 * @elem element of a list
 * @key key to check for
 *
 * Return: 1 on has key, 0 on doesn't have key
 */
int ds_element_has_key(datastore_t *elem, datastore_key_t *key);

/**
 * list_has_key() - Checks if list has key
 * @list list to check
 *
 * Return: 1 on has key, 0 on doesn't have at least one key part
 */
int ds_list_has_key(datastore_t *list);


datastore_t *ds_find_node_by_key(datastore_t *our_root, datastore_key_t *key);

void ds_get_all(datastore_t *our_root, node_t *out, int check_siblings);

void ds_get_all_keys(datastore_t *our_root, node_t *out);

void ds_get_list_data(node_t *filter_root, datastore_t *node, node_t *out);

void ds_get_filtered(node_t *filter_root, datastore_t *our_root, node_t *out);

#endif /* __DATASTORE_H_ */