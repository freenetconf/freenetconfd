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
#include <libdmconfig/dmmsg.h>
#include <libdmconfig/dmconfig.h>
#include <libdmconfig/dm_dmconfig_rpc_stub.h>
#include <roxml.h>

int dm_init();
void dm_shutdown();

char* dm_get_parameter(char *key);
int dm_set_parameter(char *key, char *value);
int dm_commit();
uint16_t dm_get_instance(char *path, char *key, char *value);
int dm_set_parameters_from_xml(node_t *root, node_t *n);
int dm_get_xml_config(node_t *filter_root, node_t *filter_node, node_t **xml_out);
uint32_t dm_list_to_xml(const char *prefix, DM2_AVPGRP *grp, node_t **xml_out);
#endif /* __CONFIG_H__ */
