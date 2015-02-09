/*
 * Copyright (C) 2014 Sartura, Ltd.
 * Copyright (C) 2014 Cisco Systems, Inc.
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

#ifndef __FREENETCONFD_SRC_NETCONF_H__
#define __FREENETCONFD_SRC_NETCONF_H__

#include <roxml.h>

int netconf_capabilites_from_yang(char *yang_dir, node_t *root);

/* RFC: http://tools.ietf.org/html/rfc6241#appendix-A */

#endif /* __FREENETCONFD_SRC_NETCONF_H__ */
