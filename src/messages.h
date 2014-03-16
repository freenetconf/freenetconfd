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

#ifndef _FREENETCONFD_MESSAGES_H__
#define _FREENETCONFD_MESSAGES_H__

#define XML_NETCONF_BASE_1_0_END "]]>]]>"
#define XML_NETCONF_BASE_1_1_END "\n##\n"

#define XML_PROLOG \
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>" \

#define XML_NETCONF_HELLO \
XML_PROLOG \
"<hello xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">" \
 "<capabilities>" \
  "<capability>urn:ietf:params:netconf:base:1.1</capability>" \
  "<capability>urn:ietf:params:netconf:capability:writable-running:1.0</capability>" \
  "<capability>urn:ietf:params:xml:ns:yang:ietf-system?module=ietf-system</capability>"\
 "</capabilities>" \
 "<session-id>1</session-id>" \
"</hello>"

#define XML_NETCONF_REPLY_OK_TEMPLATE \
"<rpc-reply xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">" \
 "<ok/>" \
"</rpc-reply>"

#define XML_NETCONF_REPLY_TEMPLATE \
"<rpc-reply xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">" \
"</rpc-reply>"

#define XML_NETCONF_RPC_ERROR_TEMPLATE \
"<rpc-reply xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"\
 "<rpc-error>" \
  "<error-type>error</error-type>" \
  "<error-tag>missing-attribute</error-tag>" \
  "<error-severity>error</error-severity>" \
  "<error-info>" \
   "<bad-attribute>message-id</bad-attribute>" \
   "<bad-element>rpc</bad-element>" \
  "</error-info>" \
 "</rpc-error>" \
"</rpc-reply>"

#endif /* _FREENETCONFD_MESSAGES_H__ */
