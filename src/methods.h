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

#ifndef __FREENETCONFD_METHODS_H__
#define __FREENETCONFD_METHODS_H__

int method_analyze_message_hello(char *method_in, int *base);
int method_create_message_hello(char **method_out);
int method_handle_message_rpc(char *method_in, char **method_out);

#endif /* __FREENETCONFD_METHODS_H__ */
