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

#ifndef __FREENETCONFD_H__
#define __FREENETCONFD_H__

#define PROJECT_NAME "freenetconfd"

#include <syslog.h>
#include <stdio.h>

#define DEBUG(fmt, ...) do { \
		fprintf(stderr, "freenetconfd: %s(%d): " fmt, __FILE__, __LINE__, ## __VA_ARGS__); \
	} while (0)

#define LOG(fmt, ...) do { \
		syslog(0, fmt, ## __VA_ARGS__); \
		fprintf(stderr, "freenetconfd: "fmt, ## __VA_ARGS__); \
	} while (0)

#define ERROR(fmt, ...) do { \
		syslog(0, fmt, ## __VA_ARGS__); \
		fprintf(stderr, "freenetconfd: "fmt, ## __VA_ARGS__); \
	} while (0)

#ifndef typeof
#define typeof __typeof
#endif

#ifndef container_of
#define container_of(ptr, type, member) ( \
	(type *)( (char *)ptr - offsetof(type,member) ))
#endif

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#endif /* __FREENETCONFD_H__ */
