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

#ifndef __FREENETCONFD_NETCONF_H__
#define __FREENETCONFD_NETCONF_H__

/* RFC: http://tools.ietf.org/html/rfc6241#appendix-A */
typedef enum rpc_error_tag
{
	RPC_ERROR_TAG_OPERATION_FAILED,
	RPC_ERROR_TAG_OPERATION_NOT_SUPPORTED,
	RPC_ERROR_TAG_IN_USE,
	RPC_ERROR_TAG_INVALID_VALUE,
	RPC_ERROR_TAG_DATA_MISSING,
	RPC_ERROR_TAG_DATA_EXISTS,
	__RPC_ERROR_TAG_COUNT
} rpc_error_tag_t;

typedef enum rpc_error_type
{
	RPC_ERROR_TYPE_TRANSPORT,
	RPC_ERROR_TYPE_RPC,
	RPC_ERROR_TYPE_PROTOCOL,
	RPC_ERROR_TYPE_APPLICATION,
	__RPC_ERROR_TYPE_COUNT
} rpc_error_type_t;

typedef enum rpc_error_severity
{
	RPC_ERROR_SEVERITY_ERROR,
	RPC_ERROR_SEVERITY_WARN,
	__RPC_ERROR_SEVERITY_COUNT
} rpc_error_severity_t;
char *netconf_rpc_error(char *msg, rpc_error_tag_t rpc_error_tag, rpc_error_type_t rpc_error_type, rpc_error_severity_t rpc_error_severity, char *error_app_tag);


#endif /* __FREENETCONFD_NETCONF_H__ */
