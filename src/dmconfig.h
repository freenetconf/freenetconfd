/*
 * Copyright (C) 2014 Sartura, Ltd.
 *
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

#ifndef __DMCONFIG_H__
#define __DMCONFIG_H__
#include <libdmconfig/dmconfig.h>
#include <roxml.h>

int dm_init(struct event_base **base, DMCONTEXT *ctx, DM_AVPGRP **grp);
void dm_shutdown(struct event_base **base, DMCONTEXT *ctx, DM_AVPGRP **grp);
char* dm_get_parameter(char *key);
int dm_set_parameter(char *key, char *value);
int dm_commit();
uint16_t dm_get_instance(char *path, char *key, char *value);
int dm_set_parameters_from_xml(node_t *root, node_t *n);
char *dm_get_xml_config(char *filter);
#endif /* __CONFIG_H__ */
