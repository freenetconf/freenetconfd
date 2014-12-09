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

#include <stdlib.h>
#include <uci.h>
#include <string.h>
#include <libubox/blobmsg.h>
#include <uci_blob.h>

#include <libgen.h>
#include <sys/stat.h>

#include "freenetconfd/freenetconfd.h"

#include "config.h"

enum
{
	ADDR,
	PORT,
	YANG_DIR,
	MODULES_DIR,
	__OPTIONS_COUNT
};

const struct blobmsg_policy config_policy[__OPTIONS_COUNT] =
{
	[ADDR] = { .name = "addr", .type = BLOBMSG_TYPE_STRING },
	[PORT] = { .name = "port", .type = BLOBMSG_TYPE_STRING },
	[YANG_DIR] = { .name = "yang_dir", .type = BLOBMSG_TYPE_STRING },
	[MODULES_DIR] = { .name = "modules_dir", .type = BLOBMSG_TYPE_STRING }
};
const struct uci_blob_param_list config_attr_list =
{
	.n_params = __OPTIONS_COUNT,
	.params = config_policy
};

/*
 * config_load() - load uci config file
 *
 * Load and parse uci config file. If config file is found, parse configs to
 * internal structure defined in config.h.
 */
int config_load(void)
{
	struct uci_context *uci = uci_alloc_context();
	struct uci_package *conf = NULL;
	struct blob_attr *tb[__OPTIONS_COUNT], *c;
	static struct blob_buf buf;

	if (uci_load(uci, "freenetconfd", &conf))
	{
		uci_free_context(uci);
		return -1;
	}

	blob_buf_init(&buf, 0);

	struct uci_element *section_elem;
	uci_foreach_element(&conf->sections, section_elem)
	{
		struct uci_section *s = uci_to_section(section_elem);
		uci_to_blob(&buf, s, &config_attr_list);
	}

	blobmsg_parse(config_policy, __OPTIONS_COUNT, tb, blob_data(buf.head), blob_len(buf.head));

	/* defaults */
	config.addr = NULL;
	config.port = NULL;
	config.yang_dir = NULL;
	config.modules_dir = NULL;

	if ((c = tb[ADDR]))
		config.addr = strdup(blobmsg_get_string(c));

	if ((c = tb[PORT]))
		config.port = strdup(blobmsg_get_string(c));

	if ((c = tb[YANG_DIR]))
		config.yang_dir = strdup(blobmsg_get_string(c));

	if ((c = tb[MODULES_DIR]))
		config.modules_dir = strdup(blobmsg_get_string(c));

	if (!(config.modules_dir))
	{
		ERROR("modules directory must be set\n");
		uci_unload(uci, conf);
		uci_free_context(uci);
		return -1;
	}

	blob_buf_free(&buf);
	uci_unload(uci, conf);
	uci_free_context(uci);

	return 0;
}

void config_exit(void)
{
	free(config.addr);
	free(config.port);
	free(config.yang_dir);
	free(config.modules_dir);
}
