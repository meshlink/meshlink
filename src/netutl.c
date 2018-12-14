/*
    netutl.c -- some supporting network utility code
    Copyright (C) 2014-2017 Guus Sliepen <guus@meshlink.io>

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

#include "system.h"

#include "net.h"
#include "netutl.h"
#include "logger.h"
#include "utils.h"
#include "xalloc.h"

bool hostnames = false;

/*
  Turn a string into a struct addrinfo.
  Return NULL on failure.
*/
struct addrinfo *str2addrinfo(const char *address, const char *service, int socktype) {
	struct addrinfo *ai;
	int err;

	struct addrinfo hint = {
		.ai_family = addressfamily,
		.ai_socktype = socktype,
	};

	err = getaddrinfo(address, service, &hint, &ai);

	if(err) {
		logger(NULL, MESHLINK_WARNING, "Error looking up %s port %s: %s", address, service, err == EAI_SYSTEM ? strerror(errno) : gai_strerror(err));
		return NULL;
	}

	return ai;
}

sockaddr_t str2sockaddr(const char *address, const char *port) {
	struct addrinfo *ai;
	sockaddr_t result = {0};
	int err;

	struct addrinfo hint = {
		.ai_family = AF_UNSPEC,
		.ai_flags = AI_NUMERICHOST,
		.ai_socktype = SOCK_STREAM,
	};

	err = getaddrinfo(address, port, &hint, &ai);

	if(err || !ai) {
		logger(NULL, MESHLINK_DEBUG, "Unknown type address %s port %s", address, port);
		result.sa.sa_family = AF_UNKNOWN;
		result.unknown.address = xstrdup(address);
		result.unknown.port = xstrdup(port);
		return result;
	}

	memcpy(&result, ai->ai_addr, ai->ai_addrlen);
	freeaddrinfo(ai);

	return result;
}

void sockaddr2str(const sockaddr_t *sa, char **addrstr, char **portstr) {
	char address[NI_MAXHOST];
	char port[NI_MAXSERV];
	char *scopeid;
	int err;

	if(sa->sa.sa_family == AF_UNKNOWN) {
		if(addrstr) {
			*addrstr = xstrdup(sa->unknown.address);
		}

		if(portstr) {
			*portstr = xstrdup(sa->unknown.port);
		}

		return;
	}

	err = getnameinfo(&sa->sa, SALEN(sa->sa), address, sizeof(address), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);

	if(err) {
		logger(NULL, MESHLINK_ERROR, "Error while translating addresses: %s", err == EAI_SYSTEM ? strerror(errno) : gai_strerror(err));
		abort();
	}

	scopeid = strchr(address, '%');

	if(scopeid) {
		*scopeid = '\0';        /* Descope. */
	}

	if(addrstr) {
		*addrstr = xstrdup(address);
	}

	if(portstr) {
		*portstr = xstrdup(port);
	}
}

char *sockaddr2hostname(const sockaddr_t *sa) {
	char *str;
	char address[NI_MAXHOST] = "unknown";
	char port[NI_MAXSERV] = "unknown";
	int err;

	if(sa->sa.sa_family == AF_UNKNOWN) {
		xasprintf(&str, "%s port %s", sa->unknown.address, sa->unknown.port);
		return str;
	}

	err = getnameinfo(&sa->sa, SALEN(sa->sa), address, sizeof(address), port, sizeof(port),
	                  hostnames ? 0 : (NI_NUMERICHOST | NI_NUMERICSERV));

	if(err) {
		logger(NULL, MESHLINK_ERROR, "Error while looking up hostname: %s", err == EAI_SYSTEM ? strerror(errno) : gai_strerror(err));
		abort();
	}

	xasprintf(&str, "%s port %s", address, port);

	return str;
}

int sockaddrcmp_noport(const sockaddr_t *a, const sockaddr_t *b) {
	int result;

	result = a->sa.sa_family - b->sa.sa_family;

	if(result) {
		return result;
	}

	switch(a->sa.sa_family) {
	case AF_UNSPEC:
		return 0;

	case AF_UNKNOWN:
		return strcmp(a->unknown.address, b->unknown.address);

	case AF_INET:
		return memcmp(&a->in.sin_addr, &b->in.sin_addr, sizeof(a->in.sin_addr));

	case AF_INET6:
		return memcmp(&a->in6.sin6_addr, &b->in6.sin6_addr, sizeof(a->in6.sin6_addr));

	default:
		logger(NULL, MESHLINK_ERROR, "sockaddrcmp() was called with unknown address family %d, exitting!",
		       a->sa.sa_family);
		abort();
	}
}

int sockaddrcmp(const sockaddr_t *a, const sockaddr_t *b) {
	int result;

	result = a->sa.sa_family - b->sa.sa_family;

	if(result) {
		return result;
	}

	switch(a->sa.sa_family) {
	case AF_UNSPEC:
		return 0;

	case AF_UNKNOWN:
		result = strcmp(a->unknown.address, b->unknown.address);

		if(result) {
			return result;
		}

		return strcmp(a->unknown.port, b->unknown.port);

	case AF_INET:
		result = memcmp(&a->in.sin_addr, &b->in.sin_addr, sizeof(a)->in.sin_addr);

		if(result) {
			return result;
		}

		return memcmp(&a->in.sin_port, &b->in.sin_port, sizeof(a)->in.sin_port);

	case AF_INET6:
		result = memcmp(&a->in6.sin6_addr, &b->in6.sin6_addr, sizeof(a)->in6.sin6_addr);

		if(result) {
			return result;
		}

		return memcmp(&a->in6.sin6_port, &b->in6.sin6_port, sizeof(a)->in6.sin6_port);

	default:
		logger(NULL, MESHLINK_ERROR, "sockaddrcmp() was called with unknown address family %d, exitting!",
		       a->sa.sa_family);
		abort();
	}
}

void sockaddrcpy(sockaddr_t *a, const sockaddr_t *b) {
	if(b->sa.sa_family != AF_UNKNOWN) {
		*a = *b;
	} else {
		a->unknown.family = AF_UNKNOWN;
		a->unknown.address = xstrdup(b->unknown.address);
		a->unknown.port = xstrdup(b->unknown.port);
	}
}

void sockaddrcpy_setport(sockaddr_t *a, const sockaddr_t *b, uint16_t port) {
	sockaddrcpy(a, b);

	switch(b->sa.sa_family) {
	case AF_INET:
		a->in.sin_port = port;
		break;

	case AF_INET6:
		a->in6.sin6_port = port;
		break;

	default:
		break;
	}
}

void sockaddrfree(sockaddr_t *a) {
	if(a->sa.sa_family == AF_UNKNOWN) {
		free(a->unknown.address);
		free(a->unknown.port);
	}
}

void sockaddrunmap(sockaddr_t *sa) {
	if(sa->sa.sa_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(&sa->in6.sin6_addr)) {
		sa->in.sin_addr.s_addr = ((uint32_t *) & sa->in6.sin6_addr)[3];
		sa->in.sin_family = AF_INET;
	}
}

void packmsg_add_sockaddr(packmsg_output_t *out, const sockaddr_t *sa) {
	switch(sa->sa.sa_family) {
	case AF_INET: {
		uint8_t buf[6];
		memcpy(buf + 0, &sa->in.sin_port, 2);
		memcpy(buf + 2, &sa->in.sin_addr, 4);
		packmsg_add_ext(out, 4, buf, sizeof(buf));
		break;
	}

	case AF_INET6: {
		uint8_t buf[18];
		memcpy(buf + 0, &sa->in6.sin6_port, 2);
		memcpy(buf + 2, &sa->in6.sin6_addr, 16);
		packmsg_add_ext(out, 6, buf, sizeof(buf));
		break;
	}

	default:
		packmsg_output_invalidate(out);
		break;
	}
}

sockaddr_t packmsg_get_sockaddr(packmsg_input_t *in) {
	sockaddr_t sa = {0};

	int8_t type;
	const void *data;
	uint32_t len = packmsg_get_ext_raw(in, &type, &data);

	switch(type) {
	case 4:
		if(len != 6) {
			packmsg_input_invalidate(in);
			return sa;
		}

		sa.sa.sa_family = AF_INET;
		memcpy(&sa.in.sin_port, (uint8_t *)data + 0, 2);
		memcpy(&sa.in.sin_addr, (uint8_t *)data + 2, 4);
		break;

	case 6:
		if(len != 18) {
			packmsg_input_invalidate(in);
			return sa;
		}

		sa.sa.sa_family = AF_INET6;
		memcpy(&sa.in6.sin6_port, (uint8_t *)data + 0, 2);
		memcpy(&sa.in6.sin6_addr, (uint8_t *)data + 2, 16);
		break;

	default:
		packmsg_input_invalidate(in);
		return sa;
	}

	return sa;
}
