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

#ifndef __FREENETCONFD_MODULES_H_
#define __FREENETCONFD_MODULES_H_

#include <freenetconfd/plugin.h>
#include <libubox/list.h>

int modules_load(char *modules_path, struct list_head *modules);
int modules_unload();
int modules_reload(char *module_name);

int modules_init();
struct list_head *get_modules();

#endif /* __FREENETCONFD_MODULES_H_ */
