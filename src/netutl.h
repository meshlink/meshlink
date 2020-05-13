#ifndef MESHLINK_NETUTL_H
#define MESHLINK_NETUTL_H

/*
    netutl.h -- header file for netutl.c
    Copyright (C) 2014, 2017 Guus Sliepen <guus@meshlink.io>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "net.h"
#include "packmsg.h"

struct addrinfo *str2addrinfo(const char *, const char *, int) __attribute__((__malloc__));
sockaddr_t str2sockaddr(const char *, const char *);
void sockaddr2str(const sockaddr_t *, char **, char **);
char *sockaddr2hostname(const sockaddr_t *) __attribute__((__malloc__));
int sockaddrcmp(const sockaddr_t *, const sockaddr_t *) __attribute__((__warn_unused_result__));
int sockaddrcmp_noport(const sockaddr_t *, const sockaddr_t *) __attribute__((__warn_unused_result__));
void sockaddrunmap(sockaddr_t *);
void sockaddrfree(sockaddr_t *);
void sockaddrcpy(sockaddr_t *, const sockaddr_t *);
void sockaddrcpy_setport(sockaddr_t *, const sockaddr_t *, uint16_t port);

void packmsg_add_sockaddr(struct packmsg_output *out, const sockaddr_t *);
sockaddr_t packmsg_get_sockaddr(struct packmsg_input *in) __attribute__((__warn_unused_result__));

#endif
