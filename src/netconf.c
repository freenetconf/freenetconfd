/*
 * Copyright (C) 2014 Cisco Systems, Inc.
 * Copyright (C) 2014 Sartura, Ltd.
 *
 * Author: Zvonimir Fras <zvonimir.fras@sartura.hr>
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

#include "freenetconfd/freenetconfd.h"

#include "netconf.h"
#include "messages.h"

#include <dirent.h>
#include <string.h>
#include <stdlib.h>

int netconf_capabilites_from_yang(char *yang_dir, char ***capabilities)
{
	DIR *dir;
	struct dirent *file;
	int rc = 0;
	char *revision;

	if (!yang_dir) {
		ERROR("yang dir not specified\n");
		return 1;
	}

	if ((dir = opendir(yang_dir)) == NULL) {
		ERROR("openning yang directory failed:%s\n", yang_dir);
		return 1;
	}

	int i = 0;

	capabilities = NULL;

	while ((file = readdir (dir)) != NULL) {

		capabilities = realloc(capabilities, i+1);

		// list only yang files
		char *ext = strstr(file->d_name, ".yang");

		if (!ext)
			continue;

		DEBUG("yang module %s\n", file->d_name);

		// remove extension
		ext[0] = 0;

		revision = strstr(file->d_name,"@");

		if (!revision) {
			asprintf(capabilities[i],"<capability>%s:%s%s</capability>", YANG_NAMESPACE, file->d_name, "?module=");
		}
		else {
			asprintf(capabilities[i],"<capability>%s:%s%s&amp;revision=%s</capability>", YANG_NAMESPACE, file->d_name, "?module=", revision + 1);
		}
	}

	closedir(dir);
	free(file);

	return rc;
}

char *rpc_error_tags[__RPC_ERROR_TAG_COUNT] = {
	"operation-failed",
	"operation-not-supported",
	"in-use",
	"invalid-value",
	"data-missing"
};

char *rpc_error_types[__RPC_ERROR_TYPE_COUNT] = {
	"transport",
	"rpc",
	"protocol",
	"application"
};


char *rpc_error_severities[__RPC_ERROR_SEVERITY_COUNT] = {
	"error",
	"warning"
};

char* netconf_rpc_error(char *msg, rpc_error_tag_t rpc_error_tag, rpc_error_type_t rpc_error_type, rpc_error_severity_t rpc_error_severity )
{
	// defaults
	char *tag = "operation-failed";
	char *type = "rpc";
	char *severity = "error";

	char *rpc_error = NULL;

	// truncate too big messages
	if (!msg || strlen(msg) > 400)
		msg = "";

	if (rpc_error_tag > 0 && rpc_error_tag < __RPC_ERROR_TAG_COUNT)
		tag = rpc_error_tags[rpc_error_tag];

	if (rpc_error_type > 0 && rpc_error_type < __RPC_ERROR_TYPE_COUNT)
		type = rpc_error_types[rpc_error_type];

	if (rpc_error_severity > 0 && rpc_error_severity < __RPC_ERROR_SEVERITY_COUNT)
		severity = rpc_error_severities[rpc_error_severity];

	asprintf(&rpc_error, "<error_type>%s</error_type><error_tag>%s</error_tag>"
			 "<error_severity>%s</error_severity><error_message xml:lang=\"en\">%s</error_message>", type, tag, severity, msg);

	return rpc_error;
}
