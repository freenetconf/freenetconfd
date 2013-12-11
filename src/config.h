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

#ifndef __CONFIG_H__
#define __CONFIG_H__

int config_load();
void config_exit();

struct config_t {
	char *addr;
	char *port;
	char *username;
	char *password;
	char *host_dsa_key;
	char *host_rsa_key;
	char *client_key_pub;
	uint32_t log_level;
	uint32_t ssh_timeout_socket;
	uint32_t ssh_timeout_read;
	bool ssh_pcap_enable;
	char *ssh_pcap_file;
} config;

#endif /* __CONFIG_H__ */
