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
  "<capability>urn:ietf:params:xml:ns:yang:iana-if-type?module=iana-if-type</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:iana-timezones?module=iana-timezones&amp;revision=2012-07-09</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-inet-types?module=ietf-inet-types&amp;revision=2013-07-15</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-interfaces?module=ietf-interfaces&amp;revision=2013-07-04</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-ip?module=ietf-ip&amp;revision=2013-10-18</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-netconf-acm?module=ietf-netconf-acm&amp;revision=2012-02-22</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-system?module=ietf-system&amp;revision=2013-11-07</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:ietf-yang-types?module=ietf-yang-types&amp;revision=2013-07-15</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:opencpe-deviations?module=opencpe-deviations&amp;revision=2014-01-12</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:opencpe-firmware-mgmt?module=opencpe-firmware-mgmt&amp;revision=2014-02-06</capability>"\
  "<capability>urn:ietf:params:xml:ns:yang:opencpe-system?module=opencpe-system&amp;revision=2013-11-07</capability>"\
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

#define XML_NETCONF_REPLY_ERROR_TEMPLATE \
"<rpc-reply xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">"\
 "<rpc-error>" \
 "</rpc-error>" \
"</rpc-reply>"

#endif /* _FREENETCONFD_MESSAGES_H__ */
