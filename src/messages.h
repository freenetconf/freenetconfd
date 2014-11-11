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

#ifndef _FREENETCONFD_MESSAGES_H__
#define _FREENETCONFD_MESSAGES_H__

#define XML_NETCONF_BASE_1_0_END "]]>]]>"
#define XML_NETCONF_BASE_1_1_END "\n##\n"

#define XML_NETCONF_HELLO \
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>" \
"<hello xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">" \
 "<capabilities>" \
  "<capability>urn:ietf:params:netconf:base:1.0</capability>" \
  "<capability>urn:ietf:params:netconf:base:1.1</capability>" \
  "<capability>urn:ietf:params:netconf:capability:writable-running:1.0</capability>" \
  "<capability>urn:ietf:params:xml:ns:yang:ietf-yang-types?module=ietf-yang-types&amp;revision=2013-07-15</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-inet-types?module=ietf-inet-types&amp;revision=2013-07-15</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-netconf-acm?module=ietf-netconf-acm&amp;revision=2012-02-22</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:iana-timezones?module=iana-timezones&amp;revision=2012-07-09</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-system?revision=2013-11-07&amp;module=ietf-system&amp;features=radius</capability>"\
 "</capabilities>" \
"</hello>"

#define XML_NETCONF_REPLY_TEMPLATE \
"<rpc-reply>" \
"</rpc-reply>"

#define YANG_NAMESPACE "urn:ietf:params:xml:ns:yang"

#endif /* _FREENETCONFD_MESSAGES_H__ */
