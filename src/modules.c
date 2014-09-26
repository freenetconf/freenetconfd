/*
 * Copyright (C) 2014 Cisco Systems, Inc.
 *
 * Author: Petar Koretic <petar.koretic@sartura.hr>
 * Author: Luka Perkov <luka.perkov@sartura.hr>
 * Author: Zvonimir Fras <zvonimir.fras@sartura.hr>
 *
 * freenetconfd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with freenetconfd. If not, see <http://www.gnu.org/licenses/>.
 */

#include "modules.h"
#include "config.h"
#include "freenetconfd.h"

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include <string.h>

LIST_HEAD(module_list);

/* handle our internal list above */
struct list_head *get_modules()
{
	return &module_list;
}

int modules_init()
{
	return modules_load(config.modules_dir, &module_list);
}

static int module_load(char *modules_path, char *name, struct module_list **e)
{
	if (!name)
		return 1;

	struct module* (*init)();
	void *lib;

	*e = malloc(sizeof (struct module_list));

	char module_path[BUFSIZ];
	snprintf(module_path, BUFSIZ, "%s/%s", modules_path, name);

	lib = dlopen(module_path, RTLD_LAZY);
	if (!lib) {
		ERROR("%s\n", dlerror());

		return 1;
	}

	init = dlsym(lib,"init");
	if (!init) {
		dlclose(lib);

		return 2;
	}

	// load module data
	(*e)->m = init();
	if (!(*e)->m) {
		dlclose(lib);
		free(*e);

		return 3;
	}

	(*e)->name = strdup(name);
	(*e)->lib = lib;

	return 0;
}

static void module_unload(struct module_list **elem)
{
	free((*elem)->name);
	dlclose((*elem)->lib);
	list_del(&(*elem)->list);
	free(*elem);
}

int modules_load(char *modules_path, struct list_head *module_list)
{
	DIR *dir;
	struct dirent *file;
	int rc = 0;

	if ((dir = opendir(modules_path)) == NULL) {
		ERROR("unable to open modules dir: %s\n", modules_path);
		return 1;
	}

	while ((file = readdir(dir)) != NULL) {
		if(!strstr(file->d_name, ".so"))
			continue;

		struct module_list *e;
		rc = module_load(modules_path, file->d_name, &e);
		if (rc) {
			ERROR("unable to load module: '%s' (%d)\n", file->d_name, rc);
			closedir (dir);
			return 1;
		}

		list_add(&e->list, module_list);

		DEBUG("loaded module: '%s'\n", file->d_name);
	}

	closedir(dir);

	return 0;
}

int modules_unload()
{
	struct module_list *elem, *next;
	list_for_each_entry_safe(elem, next, &module_list, list) {
		DEBUG("unloading module: '%s'\n", elem->name);
		module_unload(&elem);
	}

	return 0;
}

int modules_reload(char *module_name)
{
	struct module_list *elem, *next;
	list_for_each_entry_safe(elem, next, &module_list, list) {
		// reload all modules
		if (!module_name) {
			module_unload(&elem);
		}
		// reload specific module if exists
		else if (!strcmp(module_name, elem->name)) {
			module_unload(&elem);

			return module_load(config.modules_dir, module_name, &elem);
		}
	}

	return 1;
}
