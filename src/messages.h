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
  "<capability>urn:ietf:params:netconf:capability:candidate:1.0</capability>" \
  "<capability>urn:ietf:params:netconf:capability:writable-running:1.0</capability>" \
  "<capability>urn:ietf:params:xml:ns:yang:iana-if-type?module=iana-if-type</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:iana-timezones?module=iana-timezones</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-inet-types?module=ietf-inet-types</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-interfaces?module=ietf-interfaces</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-ip?module=ietf-ip</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-netconf-acm?module=ietf-netconf-acm</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-system?module=ietf-system</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-yang-types?module=ietf-yang-types</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:opencpe-deviations?module=opencpe-deviations</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:opencpe-firmware-mgmt?module=opencpe-firmware-mgmt</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:opencpe-system?module=opencpe-system</capability>"\
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
