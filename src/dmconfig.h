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

int dm_rpc_restart();
int dm_rpc_shutdown();
int dm_rpc_set_bootorder(node_t *node);
int dm_rpc_firmware_download(node_t *node);
int dm_rpc_firmware_commit(int32_t job_id);

int dm_set_parameters_from_xml(node_t *root, node_t *n);
int dm_get_xml_config(node_t *filter_root, node_t *filter_node, node_t **xml_out);
int dm_set_current_datetime(char *value);
#endif /* __CONFIG_H__ */
