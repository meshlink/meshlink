/*
    meshlink.c -- Implementation of the MeshLink API.
    Copyright (C) 2014-2021 Guus Sliepen <guus@meshlink.io>

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
#include <pthread.h>

#include "adns.h"
#include "crypto.h"
#include "ecdsagen.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "net.h"
#include "netutl.h"
#include "node.h"
#include "submesh.h"
#include "packmsg.h"
#include "prf.h"
#include "protocol.h"
#include "route.h"
#include "sockaddr.h"
#include "utils.h"
#include "xalloc.h"
#include "ed25519/sha512.h"
#include "discovery.h"
#include "devtools.h"
#include "graph.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
__thread meshlink_errno_t meshlink_errno;
meshlink_log_cb_t global_log_cb;
meshlink_log_level_t global_log_level;

typedef bool (*search_node_by_condition_t)(const node_t *, const void *);

static int rstrip(char *value) {
	int len = strlen(value);

	while(len && strchr("\t\r\n ", value[len - 1])) {
		value[--len] = 0;
	}

	return len;
}

static void get_canonical_address(node_t *n, char **hostname, char **port) {
	if(!n->canonical_address) {
		return;
	}

	*hostname = xstrdup(n->canonical_address);
	char *space = strchr(*hostname, ' ');

	if(space) {
		*space++ = 0;
		*port = xstrdup(space);
	}
}

static bool is_valid_hostname(const char *hostname) {
	if(!*hostname) {
		return false;
	}

	for(const char *p = hostname; *p; p++) {
		if(!(isalnum(*p) || *p == '-' || *p == '.' || *p == ':')) {
			return false;
		}
	}

	return true;
}

static bool is_valid_port(const char *port) {
	if(!*port) {
		return false;
	}

	if(isdigit(*port)) {
		char *end;
		unsigned long int result = strtoul(port, &end, 10);
		return result && result < 65536 && !*end;
	}

	for(const char *p = port; *p; p++) {
		if(!(isalnum(*p) || *p == '-')) {
			return false;
		}
	}

	return true;
}

static void set_timeout(int sock, int timeout) {
#ifdef _WIN32
	DWORD tv = timeout;
#else
	struct timeval tv;
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout - tv.tv_sec * 1000) * 1000;
#endif
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

struct socket_in_netns_params {
	int domain;
	int type;
	int protocol;
	int netns;
	int fd;
};

#ifdef HAVE_SETNS
static void *socket_in_netns_thread(void *arg) {
	struct socket_in_netns_params *params = arg;

	if(setns(params->netns, CLONE_NEWNET) == -1) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	params->fd = socket(params->domain, params->type, params->protocol);

	return NULL;
}
#endif // HAVE_SETNS

static int socket_in_netns(int domain, int type, int protocol, int netns) {
	if(netns == -1) {
		return socket(domain, type, protocol);
	}

#ifdef HAVE_SETNS
	struct socket_in_netns_params params = {domain, type, protocol, netns, -1};

	pthread_t thr;

	if(pthread_create(&thr, NULL, socket_in_netns_thread, &params) == 0) {
		if(pthread_join(thr, NULL) != 0) {
			abort();
		}
	}

	return params.fd;
#else
	return -1;
#endif // HAVE_SETNS

}

// Find out what local address a socket would use if we connect to the given address.
// We do this using connect() on a UDP socket, so the kernel has to resolve the address
// of both endpoints, but this will actually not send any UDP packet.
static bool getlocaladdr(const char *destaddr, sockaddr_t *sa, socklen_t *salen, int netns) {
	struct addrinfo *rai = NULL;
	const struct addrinfo hint = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = IPPROTO_UDP,
		.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV,
	};

	if(getaddrinfo(destaddr, "80", &hint, &rai) || !rai) {
		return false;
	}

	int sock = socket_in_netns(rai->ai_family, rai->ai_socktype, rai->ai_protocol, netns);

	if(sock == -1) {
		freeaddrinfo(rai);
		return false;
	}

	if(connect(sock, rai->ai_addr, rai->ai_addrlen) && !sockwouldblock(errno)) {
		closesocket(sock);
		freeaddrinfo(rai);
		return false;
	}

	freeaddrinfo(rai);

	if(getsockname(sock, &sa->sa, salen)) {
		closesocket(sock);
		return false;
	}

	closesocket(sock);
	return true;
}

static bool getlocaladdrname(const char *destaddr, char *host, socklen_t hostlen, int netns) {
	sockaddr_t sa;
	socklen_t salen = sizeof(sa);

	if(!getlocaladdr(destaddr, &sa, &salen, netns)) {
		return false;
	}

	if(getnameinfo(&sa.sa, salen, host, hostlen, NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV)) {
		return false;
	}

	return true;
}

char *meshlink_get_external_address(meshlink_handle_t *mesh) {
	return meshlink_get_external_address_for_family(mesh, AF_UNSPEC);
}

char *meshlink_get_external_address_for_family(meshlink_handle_t *mesh, int family) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_get_external_address_for_family(%d)", family);
	const char *url = mesh->external_address_url;

	if(!url) {
		url = "http://meshlink.io/host.cgi";
	}

	/* Find the hostname part between the slashes */
	if(strncmp(url, "http://", 7)) {
		abort();
		meshlink_errno = MESHLINK_EINTERNAL;
		return NULL;
	}

	const char *begin = url + 7;

	const char *end = strchr(begin, '/');

	if(!end) {
		end = begin + strlen(begin);
	}

	/* Make a copy */
	char host[end - begin + 1];
	strncpy(host, begin, end - begin);
	host[end - begin] = 0;

	char *port = strchr(host, ':');

	if(port) {
		*port++ = 0;
	}

	logger(mesh, MESHLINK_DEBUG, "Trying to discover externally visible hostname...\n");
	struct addrinfo *ai = adns_blocking_request(mesh, xstrdup(host), xstrdup(port ? port : "80"), SOCK_STREAM, 5);
	char line[256];
	char *hostname = NULL;

	for(struct addrinfo *aip = ai; aip; aip = aip->ai_next) {
		if(family != AF_UNSPEC && aip->ai_family != family) {
			continue;
		}

		int s = socket_in_netns(aip->ai_family, aip->ai_socktype, aip->ai_protocol, mesh->netns);

#ifdef SO_NOSIGPIPE
		int nosigpipe = 1;
		setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));
#endif

		if(s >= 0) {
			set_timeout(s, 5000);

			if(connect(s, aip->ai_addr, aip->ai_addrlen)) {
				closesocket(s);
				s = -1;
			}
		}

		if(s >= 0) {
			send(s, "GET ", 4, 0);
			send(s, url, strlen(url), 0);
			send(s, " HTTP/1.0\r\n\r\n", 13, 0);
			int len = recv(s, line, sizeof(line) - 1, MSG_WAITALL);

			if(len > 0) {
				line[len] = 0;

				if(line[len - 1] == '\n') {
					line[--len] = 0;
				}

				char *p = strrchr(line, '\n');

				if(p && p[1]) {
					hostname = xstrdup(p + 1);
				}
			}

			closesocket(s);

			if(hostname) {
				break;
			}
		}
	}

	if(ai) {
		freeaddrinfo(ai);
	}

	// Check that the hostname is reasonable
	if(hostname && !is_valid_hostname(hostname)) {
		free(hostname);
		hostname = NULL;
	}

	if(!hostname) {
		meshlink_errno = MESHLINK_ERESOLV;
	}

	return hostname;
}

static bool is_localaddr(sockaddr_t *sa) {
	switch(sa->sa.sa_family) {
	case AF_INET:
		return *(uint8_t *)(&sa->in.sin_addr.s_addr) == 127;

	case AF_INET6: {
		uint16_t first = sa->in6.sin6_addr.s6_addr[0] << 8 | sa->in6.sin6_addr.s6_addr[1];
		return first == 0 || (first & 0xffc0) == 0xfe80;
	}

	default:
		return false;
	}
}

#ifdef HAVE_GETIFADDRS
struct getifaddrs_in_netns_params {
	struct ifaddrs **ifa;
	int netns;
};

#ifdef HAVE_SETNS
static void *getifaddrs_in_netns_thread(void *arg) {
	struct getifaddrs_in_netns_params *params = arg;

	if(setns(params->netns, CLONE_NEWNET) == -1) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(getifaddrs(params->ifa) != 0) {
		*params->ifa = NULL;
	}

	return NULL;
}
#endif // HAVE_SETNS

static int getifaddrs_in_netns(struct ifaddrs **ifa, int netns) {
	if(netns == -1) {
		return getifaddrs(ifa);
	}

#ifdef HAVE_SETNS
	struct getifaddrs_in_netns_params params = {ifa, netns};
	pthread_t thr;

	if(pthread_create(&thr, NULL, getifaddrs_in_netns_thread, &params) == 0) {
		if(pthread_join(thr, NULL) != 0) {
			abort();
		}
	}

	return *params.ifa ? 0 : -1;
#else
	return -1;
#endif // HAVE_SETNS

}
#endif

char *meshlink_get_local_address_for_family(meshlink_handle_t *mesh, int family) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_get_local_address_for_family(%d)", family);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	// Determine address of the local interface used for outgoing connections.
	char localaddr[NI_MAXHOST];
	bool success = false;

	if(family == AF_INET) {
		success = getlocaladdrname("93.184.216.34", localaddr, sizeof(localaddr), mesh->netns);
	} else if(family == AF_INET6) {
		success = getlocaladdrname("2606:2800:220:1:248:1893:25c8:1946", localaddr, sizeof(localaddr), mesh->netns);
	}

#ifdef HAVE_GETIFADDRS

	if(!success) {
		struct ifaddrs *ifa = NULL;
		getifaddrs_in_netns(&ifa, mesh->netns);

		for(struct ifaddrs *ifap = ifa; ifap; ifap = ifap->ifa_next) {
			sockaddr_t *sa = (sockaddr_t *)ifap->ifa_addr;

			if(!sa || sa->sa.sa_family != family) {
				continue;
			}

			if(is_localaddr(sa)) {
				continue;
			}

			if(!getnameinfo(&sa->sa, SALEN(sa->sa), localaddr, sizeof(localaddr), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV)) {
				success = true;
				break;
			}
		}

		freeifaddrs(ifa);
	}

#endif

	if(!success) {
		meshlink_errno = MESHLINK_ENETWORK;
		return NULL;
	}

	return xstrdup(localaddr);
}

static void remove_duplicate_hostnames(char *host[], char *port[], int n) {
	for(int i = 0; i < n; i++) {
		if(!host[i]) {
			continue;
		}

		// Ignore duplicate hostnames
		bool found = false;

		for(int j = 0; j < i; j++) {
			if(!host[j]) {
				continue;
			}

			if(strcmp(host[i], host[j])) {
				continue;
			}

			if(strcmp(port[i], port[j])) {
				continue;
			}

			found = true;
			break;
		}

		if(found || !is_valid_hostname(host[i])) {
			free(host[i]);
			free(port[i]);
			host[i] = NULL;
			port[i] = NULL;
			continue;
		}
	}
}

// This gets the hostname part for use in invitation URLs
static char *get_my_hostname(meshlink_handle_t *mesh, uint32_t flags) {
	int count = 4 + (mesh->invitation_addresses ? mesh->invitation_addresses->count : 0);
	int n = 0;
	char *hostname[count];
	char *port[count];
	char *hostport = NULL;

	memset(hostname, 0, sizeof(hostname));
	memset(port, 0, sizeof(port));

	if(!(flags & (MESHLINK_INVITE_LOCAL | MESHLINK_INVITE_PUBLIC))) {
		flags |= MESHLINK_INVITE_LOCAL | MESHLINK_INVITE_PUBLIC;
	}

	if(!(flags & (MESHLINK_INVITE_IPV4 | MESHLINK_INVITE_IPV6))) {
		flags |= MESHLINK_INVITE_IPV4 | MESHLINK_INVITE_IPV6;
	}

	// Add all explicitly set invitation addresses
	if(mesh->invitation_addresses) {
		for list_each(char, combo, mesh->invitation_addresses) {
			hostname[n] = xstrdup(combo);
			char *slash = strrchr(hostname[n], '/');

			if(slash) {
				*slash = 0;
				port[n] = xstrdup(slash + 1);
			}

			n++;
		}
	}

	// Add local addresses if requested
	if(flags & MESHLINK_INVITE_LOCAL) {
		if(flags & MESHLINK_INVITE_IPV4) {
			hostname[n++] = meshlink_get_local_address_for_family(mesh, AF_INET);
		}

		if(flags & MESHLINK_INVITE_IPV6) {
			hostname[n++] = meshlink_get_local_address_for_family(mesh, AF_INET6);
		}
	}

	// Add public/canonical addresses if requested
	if(flags & MESHLINK_INVITE_PUBLIC) {
		// Try the CanonicalAddress first
		get_canonical_address(mesh->self, &hostname[n], &port[n]);

		if(!hostname[n] && count == 4) {
			if(flags & MESHLINK_INVITE_IPV4) {
				hostname[n++] = meshlink_get_external_address_for_family(mesh, AF_INET);
			}

			if(flags & MESHLINK_INVITE_IPV6) {
				hostname[n++] = meshlink_get_external_address_for_family(mesh, AF_INET6);
			}
		} else {
			n++;
		}
	}

	for(int i = 0; i < n; i++) {
		// Ensure we always have a port number
		if(hostname[i] && !port[i]) {
			port[i] = xstrdup(mesh->myport);
		}
	}

	remove_duplicate_hostnames(hostname, port, n);

	// Resolve the hostnames
	for(int i = 0; i < n; i++) {
		if(!hostname[i]) {
			continue;
		}

		// Convert what we have to a sockaddr
		struct addrinfo *ai_in = adns_blocking_request(mesh, xstrdup(hostname[i]), xstrdup(port[i]), SOCK_STREAM, 5);

		if(!ai_in) {
			continue;
		}

		// Remember the address(es)
		for(struct addrinfo *aip = ai_in; aip; aip = aip->ai_next) {
			node_add_recent_address(mesh, mesh->self, (sockaddr_t *)aip->ai_addr);
		}

		freeaddrinfo(ai_in);
		continue;
	}

	// Remove duplicates again, since IPv4 and IPv6 addresses might map to the same hostname
	remove_duplicate_hostnames(hostname, port, n);

	// Concatenate all unique address to the hostport string
	for(int i = 0; i < n; i++) {
		if(!hostname[i]) {
			continue;
		}

		// Append the address to the hostport string
		char *newhostport;
		xasprintf(&newhostport, (strchr(hostname[i], ':') ? "%s%s[%s]:%s" : "%s%s%s:%s"), hostport ? hostport : "", hostport ? "," : "", hostname[i], port[i]);
		free(hostport);
		hostport = newhostport;

		free(hostname[i]);
		free(port[i]);
	}

	return hostport;
}

static bool try_bind(meshlink_handle_t *mesh, int port) {
	struct addrinfo *ai = NULL;
	struct addrinfo hint = {
		.ai_flags = AI_PASSIVE,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP,
	};

	char portstr[16];
	snprintf(portstr, sizeof(portstr), "%d", port);

	if(getaddrinfo(NULL, portstr, &hint, &ai) || !ai) {
		return false;
	}

	bool success = false;

	for(struct addrinfo *aip = ai; aip; aip = aip->ai_next) {
		/* Try to bind to TCP. */

		int tcp_fd = setup_tcp_listen_socket(mesh, aip);

		if(tcp_fd == -1) {
			if(errno == EADDRINUSE) {
				/* If this port is in use for any address family, avoid it. */
				success = false;
				break;
			} else {
				continue;
			}
		}

		/* If TCP worked, then we require that UDP works as well. */

		int udp_fd = setup_udp_listen_socket(mesh, aip);

		if(udp_fd == -1) {
			closesocket(tcp_fd);
			success = false;
			break;
		}

		closesocket(tcp_fd);
		closesocket(udp_fd);
		success = true;
	}

	freeaddrinfo(ai);
	return success;
}

int check_port(meshlink_handle_t *mesh) {
	for(int i = 0; i < 1000; i++) {
		int port = 0x1000 + prng(mesh, 0x7000);

		if(try_bind(mesh, port)) {
			free(mesh->myport);
			xasprintf(&mesh->myport, "%d", port);
			return port;
		}
	}

	meshlink_errno = MESHLINK_ENETWORK;
	logger(mesh, MESHLINK_DEBUG, "Could not find any available network port.\n");
	return 0;
}

static bool write_main_config_files(meshlink_handle_t *mesh) {
	if(!mesh->confbase) {
		return true;
	}

	uint8_t buf[4096];

	/* Write the main config file */
	packmsg_output_t out = {buf, sizeof buf};

	packmsg_add_uint32(&out, MESHLINK_CONFIG_VERSION);
	packmsg_add_str(&out, mesh->name);
	packmsg_add_bin(&out, ecdsa_get_private_key(mesh->private_key), 96);
	packmsg_add_bin(&out, ecdsa_get_private_key(mesh->invitation_key), 96);
	packmsg_add_uint16(&out, atoi(mesh->myport));

	if(!packmsg_output_ok(&out)) {
		return false;
	}

	config_t config = {buf, packmsg_output_size(&out, buf)};

	if(!main_config_write(mesh, "current", &config, mesh->config_key)) {
		return false;
	}

	/* Write our own host config file */
	if(!node_write_config(mesh, mesh->self, true)) {
		return false;
	}

	return true;
}

typedef struct {
	meshlink_handle_t *mesh;
	int sock;
	char cookie[18 + 32];
	char hash[18];
	bool success;
	sptps_t sptps;
	char *data;
	size_t thedatalen;
	size_t blen;
	char line[4096];
	char buffer[4096];
} join_state_t;

static bool finalize_join(join_state_t *state, const void *buf, uint16_t len) {
	meshlink_handle_t *mesh = state->mesh;
	packmsg_input_t in = {buf, len};
	uint32_t version = packmsg_get_uint32(&in);

	if(version != MESHLINK_INVITATION_VERSION) {
		logger(mesh, MESHLINK_ERROR, "Invalid invitation version!\n");
		return false;
	}

	char *name = packmsg_get_str_dup(&in);
	char *submesh_name = packmsg_get_str_dup(&in);
	dev_class_t devclass = packmsg_get_int32(&in);
	uint32_t count = packmsg_get_array(&in);

	if(!name || !check_id(name)) {
		logger(mesh, MESHLINK_DEBUG, "No valid Name found in invitation!\n");
		free(name);
		free(submesh_name);
		return false;
	}

	if(!submesh_name || (strcmp(submesh_name, CORE_MESH) && !check_id(submesh_name))) {
		logger(mesh, MESHLINK_DEBUG, "No valid Submesh found in invitation!\n");
		free(name);
		free(submesh_name);
		return false;
	}

	if(!count) {
		logger(mesh, MESHLINK_ERROR, "Incomplete invitation file!\n");
		free(name);
		free(submesh_name);
		return false;
	}

	free(mesh->name);
	free(mesh->self->name);
	mesh->name = name;
	mesh->self->name = xstrdup(name);
	mesh->self->submesh = strcmp(submesh_name, CORE_MESH) ? lookup_or_create_submesh(mesh, submesh_name) : NULL;
	free(submesh_name);
	mesh->self->devclass = devclass == DEV_CLASS_UNKNOWN ? mesh->devclass : devclass;

	// Initialize configuration directory
	if(!config_init(mesh, "current")) {
		return false;
	}

	if(!write_main_config_files(mesh)) {
		return false;
	}

	// Write host config files
	for(uint32_t i = 0; i < count; i++) {
		const void *data;
		uint32_t data_len = packmsg_get_bin_raw(&in, &data);

		if(!data_len) {
			logger(mesh, MESHLINK_ERROR, "Incomplete invitation file!\n");
			return false;
		}

		packmsg_input_t in2 = {data, data_len};
		uint32_t version2 = packmsg_get_uint32(&in2);
		char *name2 = packmsg_get_str_dup(&in2);

		if(!packmsg_input_ok(&in2) || version2 != MESHLINK_CONFIG_VERSION || !check_id(name2)) {
			free(name2);
			packmsg_input_invalidate(&in);
			break;
		}

		if(!check_id(name2)) {
			free(name2);
			break;
		}

		if(!strcmp(name2, mesh->name)) {
			logger(mesh, MESHLINK_ERROR, "Secondary chunk would overwrite our own host config file.\n");
			free(name2);
			meshlink_errno = MESHLINK_EPEER;
			return false;
		}

		node_t *n = new_node();
		n->name = name2;

		config_t config = {data, data_len};

		if(!node_read_from_config(mesh, n, &config)) {
			free_node(n);
			logger(mesh, MESHLINK_ERROR, "Invalid host config file in invitation file!\n");
			meshlink_errno = MESHLINK_EPEER;
			return false;
		}

		if(i == 0) {
			/* The first host config file is of the inviter itself;
			 * remember the address we are currently using for the invitation connection.
			 */
			sockaddr_t sa;
			socklen_t salen = sizeof(sa);

			if(getpeername(state->sock, &sa.sa, &salen) == 0) {
				node_add_recent_address(mesh, n, &sa);
			}
		}

		/* Clear the reachability times, since we ourself have never seen these nodes yet */
		n->last_reachable = 0;
		n->last_unreachable = 0;

		if(!node_write_config(mesh, n, true)) {
			free_node(n);
			return false;
		}

		node_add(mesh, n);
	}

	/* Ensure the configuration directory metadata is on disk */
	if(!config_sync(mesh, "current") || (mesh->confbase && !sync_path(mesh->confbase))) {
		return false;
	}

	if(!mesh->inviter_commits_first) {
		devtool_set_inviter_commits_first(false);
	}

	sptps_send_record(&state->sptps, 1, ecdsa_get_public_key(mesh->private_key), 32);

	logger(mesh, MESHLINK_DEBUG, "Configuration stored in: %s\n", mesh->confbase);

	return true;
}

static bool invitation_send(void *handle, uint8_t type, const void *data, size_t len) {
	(void)type;
	join_state_t *state = handle;
	const char *ptr = data;

	while(len) {
		int result = send(state->sock, ptr, len, 0);

		if(result == -1 && errno == EINTR) {
			continue;
		} else if(result <= 0) {
			return false;
		}

		ptr += result;
		len -= result;
	}

	return true;
}

static bool invitation_receive(void *handle, uint8_t type, const void *msg, uint16_t len) {
	join_state_t *state = handle;
	meshlink_handle_t *mesh = state->mesh;

	if(mesh->inviter_commits_first) {
		switch(type) {
		case SPTPS_HANDSHAKE:
			return sptps_send_record(&state->sptps, 2, state->cookie, 18 + 32);

		case 1:
			break;

		case 0:
			if(!finalize_join(state, msg, len)) {
				return false;
			}

			logger(mesh, MESHLINK_DEBUG, "Invitation successfully accepted.\n");
			shutdown(state->sock, SHUT_RDWR);
			state->success = true;
			break;

		default:
			return false;
		}
	} else {
		switch(type) {
		case SPTPS_HANDSHAKE:
			return sptps_send_record(&state->sptps, 0, state->cookie, 18);

		case 0:
			return finalize_join(state, msg, len);

		case 1:
			logger(mesh, MESHLINK_DEBUG, "Invitation successfully accepted.\n");
			shutdown(state->sock, SHUT_RDWR);
			state->success = true;
			break;

		default:
			return false;
		}
	}

	return true;
}

static bool recvline(join_state_t *state) {
	char *newline = NULL;

	while(!(newline = memchr(state->buffer, '\n', state->blen))) {
		int result = recv(state->sock, state->buffer + state->blen, sizeof(state)->buffer - state->blen, 0);

		if(result == -1 && errno == EINTR) {
			continue;
		} else if(result <= 0) {
			return false;
		}

		state->blen += result;
	}

	if((size_t)(newline - state->buffer) >= sizeof(state->line)) {
		return false;
	}

	size_t len = newline - state->buffer;

	memcpy(state->line, state->buffer, len);
	state->line[len] = 0;
	memmove(state->buffer, newline + 1, state->blen - len - 1);
	state->blen -= len + 1;

	return true;
}

static bool sendline(int fd, const char *format, ...) {
	char buffer[4096];
	char *p = buffer;
	int blen = 0;
	va_list ap;

	va_start(ap, format);
	blen = vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);

	if(blen < 1 || (size_t)blen >= sizeof(buffer)) {
		return false;
	}

	buffer[blen] = '\n';
	blen++;

	while(blen) {
		int result = send(fd, p, blen, MSG_NOSIGNAL);

		if(result == -1 && errno == EINTR) {
			continue;
		} else if(result <= 0) {
			return false;
		}

		p += result;
		blen -= result;
	}

	return true;
}

static const char *errstr[] = {
	[MESHLINK_OK] = "No error",
	[MESHLINK_EINVAL] = "Invalid argument",
	[MESHLINK_ENOMEM] = "Out of memory",
	[MESHLINK_ENOENT] = "No such node",
	[MESHLINK_EEXIST] = "Node already exists",
	[MESHLINK_EINTERNAL] = "Internal error",
	[MESHLINK_ERESOLV] = "Could not resolve hostname",
	[MESHLINK_ESTORAGE] = "Storage error",
	[MESHLINK_ENETWORK] = "Network error",
	[MESHLINK_EPEER] = "Error communicating with peer",
	[MESHLINK_ENOTSUP] = "Operation not supported",
	[MESHLINK_EBUSY] = "MeshLink instance already in use",
	[MESHLINK_EBLACKLISTED] = "Node is blacklisted",
};

const char *meshlink_strerror(meshlink_errno_t err) {
	if((int)err < 0 || err >= sizeof(errstr) / sizeof(*errstr)) {
		return "Invalid error code";
	}

	return errstr[err];
}

static bool ecdsa_keygen(meshlink_handle_t *mesh) {
	logger(mesh, MESHLINK_DEBUG, "Generating ECDSA keypairs:\n");

	mesh->private_key = ecdsa_generate();
	mesh->invitation_key = ecdsa_generate();

	if(!mesh->private_key || !mesh->invitation_key) {
		logger(mesh, MESHLINK_ERROR, "Error during key generation!\n");
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	logger(mesh, MESHLINK_DEBUG, "Done.\n");

	return true;
}

static bool timespec_lt(const struct timespec *a, const struct timespec *b) {
	if(a->tv_sec == b->tv_sec) {
		return a->tv_nsec < b->tv_nsec;
	} else {
		return a->tv_sec < b->tv_sec;
	}
}

static struct timespec idle(event_loop_t *loop, void *data) {
	(void)loop;
	meshlink_handle_t *mesh = data;
	struct timespec t, tmin = {3600, 0};

	for splay_each(node_t, n, mesh->nodes) {
		if(!n->utcp) {
			continue;
		}

		t = utcp_timeout(n->utcp);

		if(timespec_lt(&t, &tmin)) {
			tmin = t;
		}
	}

	return tmin;
}

// Get our local address(es) by simulating connecting to an Internet host.
static void add_local_addresses(meshlink_handle_t *mesh) {
	sockaddr_t sa;
	sa.storage.ss_family = AF_UNKNOWN;
	socklen_t salen = sizeof(sa);

	// IPv4 example.org

	if(getlocaladdr("93.184.216.34", &sa, &salen, mesh->netns)) {
		sa.in.sin_port = ntohs(atoi(mesh->myport));
		node_add_recent_address(mesh, mesh->self, &sa);
	}

	// IPv6 example.org

	salen = sizeof(sa);

	if(getlocaladdr("2606:2800:220:1:248:1893:25c8:1946", &sa, &salen, mesh->netns)) {
		sa.in6.sin6_port = ntohs(atoi(mesh->myport));
		node_add_recent_address(mesh, mesh->self, &sa);
	}
}

static bool meshlink_setup(meshlink_handle_t *mesh) {
	if(!config_destroy(mesh->confbase, "new")) {
		logger(mesh, MESHLINK_ERROR, "Could not delete configuration in %s/new: %s\n", mesh->confbase, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	if(!config_destroy(mesh->confbase, "old")) {
		logger(mesh, MESHLINK_ERROR, "Could not delete configuration in %s/old: %s\n", mesh->confbase, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	if(!config_init(mesh, "current")) {
		logger(mesh, MESHLINK_ERROR, "Could not set up configuration in %s/current: %s\n", mesh->confbase, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	if(!ecdsa_keygen(mesh)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	if(check_port(mesh) == 0) {
		meshlink_errno = MESHLINK_ENETWORK;
		return false;
	}

	/* Create a node for ourself */

	mesh->self = new_node();
	mesh->self->name = xstrdup(mesh->name);
	mesh->self->devclass = mesh->devclass;
	mesh->self->ecdsa = ecdsa_set_public_key(ecdsa_get_public_key(mesh->private_key));
	mesh->self->session_id = mesh->session_id;

	if(!write_main_config_files(mesh)) {
		logger(mesh, MESHLINK_ERROR, "Could not write main config files into %s/current: %s\n", mesh->confbase, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	/* Ensure the configuration directory metadata is on disk */
	if(!config_sync(mesh, "current")) {
		return false;
	}

	return true;
}

static bool meshlink_read_config(meshlink_handle_t *mesh) {
	config_t config;

	if(!main_config_read(mesh, "current", &config, mesh->config_key)) {
		logger(NULL, MESHLINK_ERROR, "Could not read main configuration file!");
		return false;
	}

	packmsg_input_t in = {config.buf, config.len};
	const void *private_key;
	const void *invitation_key;

	uint32_t version = packmsg_get_uint32(&in);
	char *name = packmsg_get_str_dup(&in);
	uint32_t private_key_len = packmsg_get_bin_raw(&in, &private_key);
	uint32_t invitation_key_len = packmsg_get_bin_raw(&in, &invitation_key);
	uint16_t myport = packmsg_get_uint16(&in);

	if(!packmsg_done(&in) || version != MESHLINK_CONFIG_VERSION || private_key_len != 96 || invitation_key_len != 96) {
		logger(NULL, MESHLINK_ERROR, "Error parsing main configuration file!");
		free(name);
		config_free(&config);
		return false;
	}

	if(mesh->name && strcmp(mesh->name, name)) {
		logger(NULL, MESHLINK_ERROR, "Configuration is for a different name (%s)!", name);
		meshlink_errno = MESHLINK_ESTORAGE;
		free(name);
		config_free(&config);
		return false;
	}

	free(mesh->name);
	mesh->name = name;
	xasprintf(&mesh->myport, "%u", myport);
	mesh->private_key = ecdsa_set_private_key(private_key);
	mesh->invitation_key = ecdsa_set_private_key(invitation_key);
	config_free(&config);

	/* Create a node for ourself and read our host configuration file */

	mesh->self = new_node();
	mesh->self->name = xstrdup(name);
	mesh->self->devclass = mesh->devclass;
	mesh->self->session_id = mesh->session_id;

	if(!node_read_public_key(mesh, mesh->self)) {
		logger(NULL, MESHLINK_ERROR, "Could not read our host configuration file!");
		meshlink_errno = MESHLINK_ESTORAGE;
		free_node(mesh->self);
		mesh->self = NULL;
		return false;
	}

	return true;
}

#ifdef HAVE_SETNS
static void *setup_network_in_netns_thread(void *arg) {
	meshlink_handle_t *mesh = arg;

	if(setns(mesh->netns, CLONE_NEWNET) != 0) {
		return NULL;
	}

	bool success = setup_network(mesh);
	return success ? arg : NULL;
}
#endif // HAVE_SETNS

meshlink_open_params_t *meshlink_open_params_init(const char *confbase, const char *name, const char *appname, dev_class_t devclass) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_open_params_init(%s, %s, %s, %d)", confbase, name, appname, devclass);

	if(!confbase || !*confbase) {
		logger(NULL, MESHLINK_ERROR, "No confbase given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(!appname || !*appname) {
		logger(NULL, MESHLINK_ERROR, "No appname given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(strchr(appname, ' ')) {
		logger(NULL, MESHLINK_ERROR, "Invalid appname given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(name && !check_id(name)) {
		logger(NULL, MESHLINK_ERROR, "Invalid name given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(devclass < 0 || devclass >= DEV_CLASS_COUNT) {
		logger(NULL, MESHLINK_ERROR, "Invalid devclass given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	meshlink_open_params_t *params = xzalloc(sizeof * params);

	params->confbase = xstrdup(confbase);
	params->name = name ? xstrdup(name) : NULL;
	params->appname = xstrdup(appname);
	params->devclass = devclass;
	params->netns = -1;

	xasprintf(&params->lock_filename, "%s" SLASH "meshlink.lock", confbase);

	return params;
}

bool meshlink_open_params_set_netns(meshlink_open_params_t *params, int netns) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_open_params_set_netnst(%d)", netns);

	if(!params) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	params->netns = netns;

	return true;
}

bool meshlink_open_params_set_storage_key(meshlink_open_params_t *params, const void *key, size_t keylen) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_open_params_set_storage_key(%p, %zu)", key, keylen);

	if(!params) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if((!key && keylen) || (key && !keylen)) {
		logger(NULL, MESHLINK_ERROR, "Invalid key length!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	params->key = key;
	params->keylen = keylen;

	return true;
}

bool meshlink_open_params_set_storage_policy(meshlink_open_params_t *params, meshlink_storage_policy_t policy) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_open_params_set_storage_policy(%d)", policy);

	if(!params) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	params->storage_policy = policy;

	return true;
}

bool meshlink_open_params_set_lock_filename(meshlink_open_params_t *params, const char *filename) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_open_params_set_lock_filename(%s)", filename);

	if(!params || !filename) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	free(params->lock_filename);
	params->lock_filename = xstrdup(filename);

	return true;
}

bool meshlink_encrypted_key_rotate(meshlink_handle_t *mesh, const void *new_key, size_t new_keylen) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_encrypted_key_rotate(%p, %zu)", new_key, new_keylen);

	if(!mesh || !new_key || !new_keylen) {
		logger(mesh, MESHLINK_ERROR, "Invalid arguments given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	// Create hash for the new key
	void *new_config_key;
	new_config_key = xmalloc(CHACHA_POLY1305_KEYLEN);

	if(!prf(new_key, new_keylen, "MeshLink configuration key", 26, new_config_key, CHACHA_POLY1305_KEYLEN)) {
		logger(mesh, MESHLINK_ERROR, "Error creating new configuration key!\n");
		meshlink_errno = MESHLINK_EINTERNAL;
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	// Copy contents of the "current" confbase sub-directory to "new" confbase sub-directory with the new key

	if(!config_copy(mesh, "current", mesh->config_key, "new", new_config_key)) {
		logger(mesh, MESHLINK_ERROR, "Could not set up configuration in %s/old: %s\n", mesh->confbase, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	devtool_keyrotate_probe(1);

	// Rename confbase/current/ to confbase/old

	if(!config_rename(mesh, "current", "old")) {
		logger(mesh, MESHLINK_ERROR, "Cannot rename %s/current to %s/old\n", mesh->confbase, mesh->confbase);
		meshlink_errno = MESHLINK_ESTORAGE;
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	devtool_keyrotate_probe(2);

	// Rename confbase/new/ to confbase/current

	if(!config_rename(mesh, "new", "current")) {
		logger(mesh, MESHLINK_ERROR, "Cannot rename %s/new to %s/current\n", mesh->confbase, mesh->confbase);
		meshlink_errno = MESHLINK_ESTORAGE;
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	devtool_keyrotate_probe(3);

	// Cleanup the "old" confbase sub-directory

	if(!config_destroy(mesh->confbase, "old")) {
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	// Change the mesh handle key with new key

	free(mesh->config_key);
	mesh->config_key = new_config_key;

	pthread_mutex_unlock(&mesh->mutex);

	return true;
}

void meshlink_open_params_free(meshlink_open_params_t *params) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_open_params_free()");

	if(!params) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	free(params->confbase);
	free(params->name);
	free(params->appname);
	free(params->lock_filename);

	free(params);
}

/// Device class traits
static const dev_class_traits_t default_class_traits[DEV_CLASS_COUNT] = {
	{ .pingtimeout = 5, .pinginterval = 60, .maxtimeout = 900, .min_connects = 3, .max_connects = 10000, .edge_weight = 1 }, // DEV_CLASS_BACKBONE
	{ .pingtimeout = 5, .pinginterval = 60, .maxtimeout = 900, .min_connects = 3, .max_connects = 100, .edge_weight = 3 },   // DEV_CLASS_STATIONARY
	{ .pingtimeout = 5, .pinginterval = 60, .maxtimeout = 900, .min_connects = 3, .max_connects = 3, .edge_weight = 6 },     // DEV_CLASS_PORTABLE
	{ .pingtimeout = 5, .pinginterval = 60, .maxtimeout = 900, .min_connects = 1, .max_connects = 1, .edge_weight = 9 },     // DEV_CLASS_UNKNOWN
};

meshlink_handle_t *meshlink_open(const char *confbase, const char *name, const char *appname, dev_class_t devclass) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_open(%s, %s, %s, %d)", confbase, name, appname, devclass);

	if(!confbase || !*confbase) {
		logger(NULL, MESHLINK_ERROR, "No confbase given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	char lock_filename[PATH_MAX];
	snprintf(lock_filename, sizeof(lock_filename), "%s" SLASH "meshlink.lock", confbase);

	/* Create a temporary struct on the stack, to avoid allocating and freeing one. */
	meshlink_open_params_t params = {
		.confbase = (char *)confbase,
		.lock_filename = lock_filename,
		.name = (char *)name,
		.appname = (char *)appname,
		.devclass = devclass,
		.netns = -1,
	};

	return meshlink_open_ex(&params);
}

meshlink_handle_t *meshlink_open_encrypted(const char *confbase, const char *name, const char *appname, dev_class_t devclass, const void *key, size_t keylen) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_open_encrypted(%s, %s, %s, %d, %p, %zu)", confbase, name, appname, devclass, key, keylen);

	if(!confbase || !*confbase) {
		logger(NULL, MESHLINK_ERROR, "No confbase given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	char lock_filename[PATH_MAX];
	snprintf(lock_filename, sizeof(lock_filename), "%s" SLASH "meshlink.lock", confbase);

	/* Create a temporary struct on the stack, to avoid allocating and freeing one. */
	meshlink_open_params_t params = {
		.confbase = (char *)confbase,
		.lock_filename = lock_filename,
		.name = (char *)name,
		.appname = (char *)appname,
		.devclass = devclass,
		.netns = -1,
	};

	if(!meshlink_open_params_set_storage_key(&params, key, keylen)) {
		return false;
	}

	return meshlink_open_ex(&params);
}

meshlink_handle_t *meshlink_open_ephemeral(const char *name, const char *appname, dev_class_t devclass) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_open_ephemeral(%s, %s, %d)", name, appname, devclass);

	if(!name) {
		logger(NULL, MESHLINK_ERROR, "No name given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(!check_id(name)) {
		logger(NULL, MESHLINK_ERROR, "Invalid name given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(!appname || !*appname) {
		logger(NULL, MESHLINK_ERROR, "No appname given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(strchr(appname, ' ')) {
		logger(NULL, MESHLINK_ERROR, "Invalid appname given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(devclass < 0 || devclass >= DEV_CLASS_COUNT) {
		logger(NULL, MESHLINK_ERROR, "Invalid devclass given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	/* Create a temporary struct on the stack, to avoid allocating and freeing one. */
	meshlink_open_params_t params = {
		.name = (char *)name,
		.appname = (char *)appname,
		.devclass = devclass,
		.netns = -1,
	};

	return meshlink_open_ex(&params);
}

meshlink_handle_t *meshlink_open_ex(const meshlink_open_params_t *params) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_open_ex()");

	// Validate arguments provided by the application
	if(!params->appname || !*params->appname) {
		logger(NULL, MESHLINK_ERROR, "No appname given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(strchr(params->appname, ' ')) {
		logger(NULL, MESHLINK_ERROR, "Invalid appname given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(params->name && !check_id(params->name)) {
		logger(NULL, MESHLINK_ERROR, "Invalid name given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(params->devclass < 0 || params->devclass >= DEV_CLASS_COUNT) {
		logger(NULL, MESHLINK_ERROR, "Invalid devclass given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if((params->key && !params->keylen) || (!params->key && params->keylen)) {
		logger(NULL, MESHLINK_ERROR, "Invalid key length!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	meshlink_handle_t *mesh = xzalloc(sizeof(meshlink_handle_t));

	if(params->confbase) {
		mesh->confbase = xstrdup(params->confbase);
	}

	mesh->appname = xstrdup(params->appname);
	mesh->devclass = params->devclass;
	mesh->discovery.enabled = true;
	mesh->invitation_timeout = 604800; // 1 week
	mesh->netns = params->netns;
	mesh->submeshes = NULL;
	mesh->log_cb = global_log_cb;
	mesh->log_level = global_log_level;
	mesh->packet = xmalloc(sizeof(vpn_packet_t));

	randomize(&mesh->prng_state, sizeof(mesh->prng_state));

	do {
		randomize(&mesh->session_id, sizeof(mesh->session_id));
	} while(mesh->session_id == 0);

	memcpy(mesh->dev_class_traits, default_class_traits, sizeof(default_class_traits));

	mesh->name = params->name ? xstrdup(params->name) : NULL;

	// Hash the key
	if(params->key) {
		mesh->config_key = xmalloc(CHACHA_POLY1305_KEYLEN);

		if(!prf(params->key, params->keylen, "MeshLink configuration key", 26, mesh->config_key, CHACHA_POLY1305_KEYLEN)) {
			logger(NULL, MESHLINK_ERROR, "Error creating configuration key!\n");
			meshlink_close(mesh);
			meshlink_errno = MESHLINK_EINTERNAL;
			return NULL;
		}
	}

	// initialize mutexes and conds
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);

	if(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
		abort();
	}

	pthread_mutex_init(&mesh->mutex, &attr);
	pthread_cond_init(&mesh->cond, NULL);

	pthread_cond_init(&mesh->adns_cond, NULL);

	mesh->threadstarted = false;
	event_loop_init(&mesh->loop);
	mesh->loop.data = mesh;

	meshlink_queue_init(&mesh->outpacketqueue);

	// Atomically lock the configuration directory.
	if(!main_config_lock(mesh, params->lock_filename)) {
		meshlink_close(mesh);
		return NULL;
	}

	// If no configuration exists yet, create it.

	bool new_configuration = false;

	if(!meshlink_confbase_exists(mesh)) {
		if(!mesh->name) {
			logger(NULL, MESHLINK_ERROR, "No configuration files found!\n");
			meshlink_close(mesh);
			meshlink_errno = MESHLINK_ESTORAGE;
			return NULL;
		}

		if(!meshlink_setup(mesh)) {
			logger(NULL, MESHLINK_ERROR, "Cannot create initial configuration\n");
			meshlink_close(mesh);
			return NULL;
		}

		new_configuration = true;
	} else {
		if(!meshlink_read_config(mesh)) {
			logger(NULL, MESHLINK_ERROR, "Cannot read main configuration\n");
			meshlink_close(mesh);
			return NULL;
		}
	}

	mesh->storage_policy = params->storage_policy;

#ifdef HAVE_MINGW
	struct WSAData wsa_state;
	WSAStartup(MAKEWORD(2, 2), &wsa_state);
#endif

	// Setup up everything
	// TODO: we should not open listening sockets yet

	bool success = false;

	if(mesh->netns != -1) {
#ifdef HAVE_SETNS
		pthread_t thr;

		if(pthread_create(&thr, NULL, setup_network_in_netns_thread, mesh) == 0) {
			void *retval = NULL;
			success = pthread_join(thr, &retval) == 0 && retval;
		}

#else
		meshlink_errno = MESHLINK_EINTERNAL;
		return NULL;

#endif // HAVE_SETNS
	} else {
		success = setup_network(mesh);
	}

	if(!success) {
		meshlink_close(mesh);
		meshlink_errno = MESHLINK_ENETWORK;
		return NULL;
	}

	if(mesh->devclass == DEV_CLASS_BACKBONE) {
		logger(NULL, MESHLINK_DEBUG, "Resolving external IP address as we are a backbone node\n");

		mesh->self->external_ip_address = meshlink_get_external_address(mesh);

		// if(meshlink_errno == MESHLINK_ERESOLV) {
		if(!mesh->self->external_ip_address) {
			logger(NULL, MESHLINK_WARNING, "Couldn't resolve external IP address, continuing without it...\n");
		} else {
			logger(NULL, MESHLINK_INFO, "Found external IP address: %s\n", mesh->self->external_ip_address);
		}
	}

	add_local_addresses(mesh);

	if(!node_write_config(mesh, mesh->self, new_configuration)) {
		logger(NULL, MESHLINK_ERROR, "Cannot update configuration\n");
		return NULL;
	}

	idle_set(&mesh->loop, idle, mesh);

	logger(NULL, MESHLINK_DEBUG, "meshlink_open returning\n");
	return mesh;
}

meshlink_submesh_t *meshlink_submesh_open(meshlink_handle_t *mesh, const char *submesh) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_submesh_open(%s)", submesh);

	meshlink_submesh_t *s = NULL;

	if(!mesh) {
		logger(NULL, MESHLINK_ERROR, "No mesh handle given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(!submesh || !*submesh) {
		logger(NULL, MESHLINK_ERROR, "No submesh name given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	//lock mesh->nodes
	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	s = (meshlink_submesh_t *)create_submesh(mesh, submesh);

	pthread_mutex_unlock(&mesh->mutex);

	return s;
}

static void *meshlink_main_loop(void *arg) {
	meshlink_handle_t *mesh = arg;

	if(mesh->netns != -1) {
#ifdef HAVE_SETNS

		if(setns(mesh->netns, CLONE_NEWNET) != 0) {
			pthread_cond_signal(&mesh->cond);
			return NULL;
		}

#else
		pthread_cond_signal(&mesh->cond);
		return NULL;
#endif // HAVE_SETNS
	}

	if(mesh->discovery.enabled) {
		discovery_start(mesh);
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(mesh->thread_status_cb) {
		mesh->thread_status_cb(mesh, true);
	}

	logger(mesh, MESHLINK_DEBUG, "Starting main_loop...\n");
	pthread_cond_broadcast(&mesh->cond);
	main_loop(mesh);
	logger(mesh, MESHLINK_DEBUG, "main_loop returned.\n");

	if(mesh->thread_status_cb) {
		mesh->thread_status_cb(mesh, false);
	}

	pthread_mutex_unlock(&mesh->mutex);

	// Stop discovery
	if(mesh->discovery.enabled) {
		discovery_stop(mesh);
	}

	return NULL;
}

bool meshlink_start(meshlink_handle_t *mesh) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	logger(mesh, MESHLINK_DEBUG, "meshlink_start called\n");

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	assert(mesh->self);
	assert(mesh->private_key);
	assert(mesh->self->ecdsa);
	assert(!memcmp((uint8_t *)mesh->self->ecdsa + 64, (uint8_t *)mesh->private_key + 64, 32));

	if(mesh->threadstarted) {
		logger(mesh, MESHLINK_DEBUG, "thread was already running\n");
		pthread_mutex_unlock(&mesh->mutex);
		return true;
	}

	if(mesh->listen_socket[0].tcp.fd < 0) {
		logger(mesh, MESHLINK_ERROR, "Listening socket not open\n");
		meshlink_errno = MESHLINK_ENETWORK;
		return false;
	}

	// Reset node connection timers
	for splay_each(node_t, n, mesh->nodes) {
		n->last_connect_try = 0;
	}

	// TODO: open listening sockets first

	//Check that a valid name is set
	if(!mesh->name) {
		logger(mesh, MESHLINK_ERROR, "No name given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	init_outgoings(mesh);
	init_adns(mesh);

	// Start the main thread

	event_loop_start(&mesh->loop);

	// Ensure we have a decent amount of stack space. Musl's default of 80 kB is too small.
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024);

	if(pthread_create(&mesh->thread, &attr, meshlink_main_loop, mesh) != 0) {
		logger(mesh, MESHLINK_ERROR, "Could not start thread: %s\n", strerror(errno));
		memset(&mesh->thread, 0, sizeof(mesh)->thread);
		meshlink_errno = MESHLINK_EINTERNAL;
		event_loop_stop(&mesh->loop);
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	pthread_cond_wait(&mesh->cond, &mesh->mutex);
	mesh->threadstarted = true;

	// Ensure we are considered reachable
	graph(mesh);

	pthread_mutex_unlock(&mesh->mutex);
	return true;
}

void meshlink_stop(meshlink_handle_t *mesh) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_stop()\n");

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	// Shut down the main thread
	event_loop_stop(&mesh->loop);

	// Send ourselves a UDP packet to kick the event loop
	for(int i = 0; i < mesh->listen_sockets; i++) {
		sockaddr_t sa;
		socklen_t salen = sizeof(sa);

		if(getsockname(mesh->listen_socket[i].udp.fd, &sa.sa, &salen) == -1) {
			logger(mesh, MESHLINK_ERROR, "System call `%s' failed: %s", "getsockname", sockstrerror(sockerrno));
			continue;
		}

		if(sendto(mesh->listen_socket[i].udp.fd, "", 1, MSG_NOSIGNAL, &sa.sa, salen) == -1) {
			logger(mesh, MESHLINK_ERROR, "Could not send a UDP packet to ourself: %s", sockstrerror(sockerrno));
		}
	}

	if(mesh->threadstarted) {
		// Wait for the main thread to finish
		pthread_mutex_unlock(&mesh->mutex);

		if(pthread_join(mesh->thread, NULL) != 0) {
			abort();
		}

		if(pthread_mutex_lock(&mesh->mutex) != 0) {
			abort();
		}

		mesh->threadstarted = false;
	}

	// Close all metaconnections
	if(mesh->connections) {
		for(list_node_t *node = mesh->connections->head, *next; node; node = next) {
			next = node->next;
			connection_t *c = node->data;
			c->outgoing = NULL;
			terminate_connection(mesh, c, false);
		}
	}

	exit_adns(mesh);
	exit_outgoings(mesh);

	// Ensure we are considered unreachable
	if(mesh->nodes) {
		graph(mesh);
	}

	// Try to write out any changed node config files, ignore errors at this point.
	if(mesh->nodes) {
		for splay_each(node_t, n, mesh->nodes) {
			if(n->status.dirty) {
				if(!node_write_config(mesh, n, false)) {
					// ignore
				}
			}
		}
	}

	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_close(meshlink_handle_t *mesh) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_close()\n");

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	// stop can be called even if mesh has not been started
	meshlink_stop(mesh);

	// lock is not released after this
	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	// Close and free all resources used.

	close_network_connections(mesh);

	logger(mesh, MESHLINK_INFO, "Terminating");

	event_loop_exit(&mesh->loop);

#ifdef HAVE_MINGW

	if(mesh->confbase) {
		WSACleanup();
	}

#endif

	ecdsa_free(mesh->invitation_key);

	if(mesh->netns != -1) {
		close(mesh->netns);
	}

	for(vpn_packet_t *packet; (packet = meshlink_queue_pop(&mesh->outpacketqueue));) {
		free(packet);
	}

	meshlink_queue_exit(&mesh->outpacketqueue);

	free(mesh->name);
	free(mesh->appname);
	free(mesh->confbase);
	free(mesh->config_key);
	free(mesh->external_address_url);
	free(mesh->packet);
	ecdsa_free(mesh->private_key);

	if(mesh->invitation_addresses) {
		list_delete_list(mesh->invitation_addresses);
	}

	main_config_unlock(mesh);

	pthread_mutex_unlock(&mesh->mutex);
	pthread_mutex_destroy(&mesh->mutex);

	memset(mesh, 0, sizeof(*mesh));

	free(mesh);
}

bool meshlink_destroy_ex(const meshlink_open_params_t *params) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_destroy_ex()\n");

	if(!params) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(!params->confbase) {
		/* Ephemeral instances */
		return true;
	}

	/* Exit early if the confbase directory itself doesn't exist */
	if(access(params->confbase, F_OK) && errno == ENOENT) {
		return true;
	}

	/* Take the lock the same way meshlink_open() would. */
	FILE *lockfile = fopen(params->lock_filename, "w+");

	if(!lockfile) {
		logger(NULL, MESHLINK_ERROR, "Could not open lock file %s: %s", params->lock_filename, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

#ifdef FD_CLOEXEC
	fcntl(fileno(lockfile), F_SETFD, FD_CLOEXEC);
#endif

#ifdef HAVE_MINGW
	// TODO: use _locking()?
#else

	if(flock(fileno(lockfile), LOCK_EX | LOCK_NB) != 0) {
		logger(NULL, MESHLINK_ERROR, "Configuration directory %s still in use\n", params->lock_filename);
		fclose(lockfile);
		meshlink_errno = MESHLINK_EBUSY;
		return false;
	}

#endif

	if(!config_destroy(params->confbase, "current") || !config_destroy(params->confbase, "new") || !config_destroy(params->confbase, "old")) {
		logger(NULL, MESHLINK_ERROR, "Cannot remove sub-directories in %s: %s\n", params->confbase, strerror(errno));
		return false;
	}

	if(unlink(params->lock_filename)) {
		logger(NULL, MESHLINK_ERROR, "Cannot remove lock file %s: %s\n", params->lock_filename, strerror(errno));
		fclose(lockfile);
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	fclose(lockfile);

	if(!sync_path(params->confbase)) {
		logger(NULL, MESHLINK_ERROR, "Cannot sync directory %s: %s\n", params->confbase, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	return true;
}

bool meshlink_destroy(const char *confbase) {
	logger(NULL, MESHLINK_DEBUG, "meshlink_destroy(%s)", confbase);

	char lock_filename[PATH_MAX];
	snprintf(lock_filename, sizeof(lock_filename), "%s" SLASH "meshlink.lock", confbase);

	meshlink_open_params_t params = {
		.confbase = (char *)confbase,
		.lock_filename = lock_filename,
	};

	return meshlink_destroy_ex(&params);
}

void meshlink_set_receive_cb(meshlink_handle_t *mesh, meshlink_receive_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_receive_cb(%p)", (void *)(intptr_t)cb);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->receive_cb = cb;
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_connection_try_cb(meshlink_handle_t *mesh, meshlink_connection_try_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_connection_try_cb(%p)", (void *)(intptr_t)cb);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->connection_try_cb = cb;
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_node_status_cb(meshlink_handle_t *mesh, meshlink_node_status_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_node_status_cb(%p)", (void *)(intptr_t)cb);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->node_status_cb = cb;
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_node_pmtu_cb(meshlink_handle_t *mesh, meshlink_node_pmtu_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_node_pmtu_cb(%p)", (void *)(intptr_t)cb);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->node_pmtu_cb = cb;
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_node_duplicate_cb(meshlink_handle_t *mesh, meshlink_node_duplicate_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_node_duplicate_cb(%p)", (void *)(intptr_t)cb);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->node_duplicate_cb = cb;
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, meshlink_log_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_log_cb(%p)", (void *)(intptr_t)cb);

	if(mesh) {
		if(pthread_mutex_lock(&mesh->mutex) != 0) {
			abort();
		}

		mesh->log_cb = cb;
		mesh->log_level = cb ? level : 0;
		pthread_mutex_unlock(&mesh->mutex);
	} else {
		global_log_cb = cb;
		global_log_level = cb ? level : 0;
	}
}

void meshlink_set_error_cb(struct meshlink_handle *mesh, meshlink_error_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_error_cb(%p)", (void *)(intptr_t)cb);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->error_cb = cb;
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_blacklisted_cb(struct meshlink_handle *mesh, meshlink_blacklisted_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_blacklisted_cb(%p)", (void *)(intptr_t)cb);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->blacklisted_cb = cb;
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_thread_status_cb(struct meshlink_handle *mesh, meshlink_thread_status_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_thread_status_cb(%p)", (void *)(intptr_t)cb);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->thread_status_cb = cb;
	pthread_mutex_unlock(&mesh->mutex);
}

static bool prepare_packet(meshlink_handle_t *mesh, meshlink_node_t *destination, const void *data, size_t len, vpn_packet_t *packet) {
	meshlink_packethdr_t *hdr;

	if(len > MAXSIZE - sizeof(*hdr)) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	node_t *n = (node_t *)destination;

	if(n->status.blacklisted) {
		logger(mesh, MESHLINK_ERROR, "Node %s blacklisted, dropping packet\n", n->name);
		meshlink_errno = MESHLINK_EBLACKLISTED;
		return false;
	}

	// Prepare the packet
	packet->probe = false;
	packet->tcp = false;
	packet->len = len + sizeof(*hdr);

	hdr = (meshlink_packethdr_t *)packet->data;
	memset(hdr, 0, sizeof(*hdr));
	// leave the last byte as 0 to make sure strings are always
	// null-terminated if they are longer than the buffer
	strncpy((char *)hdr->destination, destination->name, sizeof(hdr->destination) - 1);
	strncpy((char *)hdr->source, mesh->self->name, sizeof(hdr->source) - 1);

	memcpy(packet->data + sizeof(*hdr), data, len);

	return true;
}

static bool meshlink_send_immediate(meshlink_handle_t *mesh, meshlink_node_t *destination, const void *data, size_t len) {
	assert(mesh);
	assert(destination);
	assert(data);
	assert(len);

	// Prepare the packet
	if(!prepare_packet(mesh, destination, data, len, mesh->packet)) {
		return false;
	}

	// Send it immediately
	route(mesh, mesh->self, mesh->packet);

	return true;
}

bool meshlink_send(meshlink_handle_t *mesh, meshlink_node_t *destination, const void *data, size_t len) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_send(%s, %p, %zu)", destination ? destination->name : "(null)", data, len);

	// Validate arguments
	if(!mesh || !destination) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(!len) {
		return true;
	}

	if(!data) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	// Prepare the packet
	vpn_packet_t *packet = malloc(sizeof(*packet));

	if(!packet) {
		meshlink_errno = MESHLINK_ENOMEM;
		return false;
	}

	if(!prepare_packet(mesh, destination, data, len, packet)) {
		free(packet);
		return false;
	}

	// Queue it
	if(!meshlink_queue_push(&mesh->outpacketqueue, packet)) {
		free(packet);
		meshlink_errno = MESHLINK_ENOMEM;
		return false;
	}

	logger(mesh, MESHLINK_DEBUG, "Adding packet of %zu bytes to packet queue", len);

	// Notify event loop
	signal_trigger(&mesh->loop, &mesh->datafromapp);

	return true;
}

void meshlink_send_from_queue(event_loop_t *loop, void *data) {
	(void)loop;
	meshlink_handle_t *mesh = data;

	logger(mesh, MESHLINK_DEBUG, "Flushing the packet queue");

	for(vpn_packet_t *packet; (packet = meshlink_queue_pop(&mesh->outpacketqueue));) {
		logger(mesh, MESHLINK_DEBUG, "Removing packet of %d bytes from packet queue", packet->len);
		route(mesh, mesh->self, packet);
		free(packet);
	}
}

ssize_t meshlink_get_pmtu(meshlink_handle_t *mesh, meshlink_node_t *destination) {
	if(!mesh || !destination) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	node_t *n = (node_t *)destination;

	if(!n->status.reachable) {
		pthread_mutex_unlock(&mesh->mutex);
		return 0;

	} else if(n->mtuprobes > 30 && n->minmtu) {
		pthread_mutex_unlock(&mesh->mutex);
		return n->minmtu;
	} else {
		pthread_mutex_unlock(&mesh->mutex);
		return MTU;
	}
}

char *meshlink_get_fingerprint(meshlink_handle_t *mesh, meshlink_node_t *node) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	node_t *n = (node_t *)node;

	if(!node_read_public_key(mesh, n) || !n->ecdsa) {
		meshlink_errno = MESHLINK_EINTERNAL;
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	char *fingerprint = ecdsa_get_base64_public_key(n->ecdsa);

	if(!fingerprint) {
		meshlink_errno = MESHLINK_EINTERNAL;
	}

	pthread_mutex_unlock(&mesh->mutex);
	return fingerprint;
}

meshlink_node_t *meshlink_get_self(meshlink_handle_t *mesh) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	return (meshlink_node_t *)mesh->self;
}

meshlink_node_t *meshlink_get_node(meshlink_handle_t *mesh, const char *name) {
	if(!mesh || !name) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	node_t *n = NULL;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	n = lookup_node(mesh, (char *)name); // TODO: make lookup_node() use const
	pthread_mutex_unlock(&mesh->mutex);

	if(!n) {
		meshlink_errno = MESHLINK_ENOENT;
	}

	return (meshlink_node_t *)n;
}

meshlink_submesh_t *meshlink_get_submesh(meshlink_handle_t *mesh, const char *name) {
	if(!mesh || !name) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	meshlink_submesh_t *submesh = NULL;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	submesh = (meshlink_submesh_t *)lookup_submesh(mesh, name);
	pthread_mutex_unlock(&mesh->mutex);

	if(!submesh) {
		meshlink_errno = MESHLINK_ENOENT;
	}

	return submesh;
}

meshlink_node_t **meshlink_get_all_nodes(meshlink_handle_t *mesh, meshlink_node_t **nodes, size_t *nmemb) {
	if(!mesh || !nmemb || (*nmemb && !nodes)) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	meshlink_node_t **result;

	//lock mesh->nodes
	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	*nmemb = mesh->nodes->count;
	result = realloc(nodes, *nmemb * sizeof(*nodes));

	if(result) {
		meshlink_node_t **p = result;

		for splay_each(node_t, n, mesh->nodes) {
			*p++ = (meshlink_node_t *)n;
		}
	} else {
		*nmemb = 0;
		free(nodes);
		meshlink_errno = MESHLINK_ENOMEM;
	}

	pthread_mutex_unlock(&mesh->mutex);

	return result;
}

static meshlink_node_t **meshlink_get_all_nodes_by_condition(meshlink_handle_t *mesh, const void *condition, meshlink_node_t **nodes, size_t *nmemb, search_node_by_condition_t search_node) {
	meshlink_node_t **result;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	*nmemb = 0;

	for splay_each(node_t, n, mesh->nodes) {
		if(search_node(n, condition)) {
			++*nmemb;
		}
	}

	if(*nmemb == 0) {
		free(nodes);
		pthread_mutex_unlock(&mesh->mutex);
		return NULL;
	}

	result = realloc(nodes, *nmemb * sizeof(*nodes));

	if(result) {
		meshlink_node_t **p = result;

		for splay_each(node_t, n, mesh->nodes) {
			if(search_node(n, condition)) {
				*p++ = (meshlink_node_t *)n;
			}
		}
	} else {
		*nmemb = 0;
		free(nodes);
		meshlink_errno = MESHLINK_ENOMEM;
	}

	pthread_mutex_unlock(&mesh->mutex);

	return result;
}

static bool search_node_by_dev_class(const node_t *node, const void *condition) {
	dev_class_t *devclass = (dev_class_t *)condition;

	if(*devclass == (dev_class_t)node->devclass) {
		return true;
	}

	return false;
}

static bool search_node_by_blacklisted(const node_t *node, const void *condition) {
	return *(bool *)condition == node->status.blacklisted;
}

static bool search_node_by_submesh(const node_t *node, const void *condition) {
	if(condition == node->submesh) {
		return true;
	}

	return false;
}

struct time_range {
	time_t start;
	time_t end;
};

static bool search_node_by_last_reachable(const node_t *node, const void *condition) {
	const struct time_range *range = condition;
	time_t start = node->last_reachable;
	time_t end = node->last_unreachable;

	if(end < start) {
		end = time(NULL);

		if(end < start) {
			start = end;
		}
	}

	if(range->end >= range->start) {
		return start <= range->end && end >= range->start;
	} else {
		return start > range->start || end < range->end;
	}
}

meshlink_node_t **meshlink_get_all_nodes_by_dev_class(meshlink_handle_t *mesh, dev_class_t devclass, meshlink_node_t **nodes, size_t *nmemb) {
	if(!mesh || devclass < 0 || devclass >= DEV_CLASS_COUNT || !nmemb) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	return meshlink_get_all_nodes_by_condition(mesh, &devclass, nodes, nmemb, search_node_by_dev_class);
}

meshlink_node_t **meshlink_get_all_nodes_by_submesh(meshlink_handle_t *mesh, meshlink_submesh_t *submesh, meshlink_node_t **nodes, size_t *nmemb) {
	if(!mesh || !submesh || !nmemb) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	return meshlink_get_all_nodes_by_condition(mesh, submesh, nodes, nmemb, search_node_by_submesh);
}

meshlink_node_t **meshlink_get_all_nodes_by_last_reachable(meshlink_handle_t *mesh, time_t start, time_t end, meshlink_node_t **nodes, size_t *nmemb) {
	if(!mesh || !nmemb) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	struct time_range range = {start, end};

	return meshlink_get_all_nodes_by_condition(mesh, &range, nodes, nmemb, search_node_by_last_reachable);
}

meshlink_node_t **meshlink_get_all_nodes_by_blacklisted(meshlink_handle_t *mesh, bool blacklisted, meshlink_node_t **nodes, size_t *nmemb) {
	if(!mesh || !nmemb) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	return meshlink_get_all_nodes_by_condition(mesh, &blacklisted, nodes, nmemb, search_node_by_blacklisted);
}

dev_class_t meshlink_get_node_dev_class(meshlink_handle_t *mesh, meshlink_node_t *node) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	dev_class_t devclass;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	devclass = ((node_t *)node)->devclass;

	pthread_mutex_unlock(&mesh->mutex);

	return devclass;
}

bool meshlink_get_node_tiny(meshlink_handle_t *mesh, meshlink_node_t *node) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	bool tiny;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	tiny = ((node_t *)node)->status.tiny;

	pthread_mutex_unlock(&mesh->mutex);

	return tiny;
}

bool meshlink_get_node_blacklisted(meshlink_handle_t *mesh, meshlink_node_t *node) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
	}

	if(!node) {
		return mesh->default_blacklist;
	}

	bool blacklisted;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	blacklisted = ((node_t *)node)->status.blacklisted;

	pthread_mutex_unlock(&mesh->mutex);

	return blacklisted;
}

meshlink_submesh_t *meshlink_get_node_submesh(meshlink_handle_t *mesh, meshlink_node_t *node) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	node_t *n = (node_t *)node;

	meshlink_submesh_t *s;

	s = (meshlink_submesh_t *)n->submesh;

	return s;
}

bool meshlink_get_node_reachability(struct meshlink_handle *mesh, struct meshlink_node *node, time_t *last_reachable, time_t *last_unreachable) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	node_t *n = (node_t *)node;
	bool reachable;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	reachable = n->status.reachable && !n->status.blacklisted;

	if(last_reachable) {
		*last_reachable = n->last_reachable;
	}

	if(last_unreachable) {
		*last_unreachable = n->last_unreachable;
	}

	pthread_mutex_unlock(&mesh->mutex);

	return reachable;
}

bool meshlink_sign(meshlink_handle_t *mesh, const void *data, size_t len, void *signature, size_t *siglen) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_sign(%p, %zu, %p, %p)", data, len, signature, (void *)siglen);

	if(!mesh || !data || !len || !signature || !siglen) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(*siglen < MESHLINK_SIGLEN) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(!ecdsa_sign(mesh->private_key, data, len, signature)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	*siglen = MESHLINK_SIGLEN;
	pthread_mutex_unlock(&mesh->mutex);
	return true;
}

bool meshlink_verify(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len, const void *signature, size_t siglen) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_verify(%p, %zu, %p, %zu)", data, len, signature, siglen);

	if(!mesh || !source || !data || !len || !signature) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(siglen != MESHLINK_SIGLEN) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	bool rval = false;

	struct node_t *n = (struct node_t *)source;

	if(!node_read_public_key(mesh, n)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		rval = false;
	} else {
		rval = ecdsa_verify(((struct node_t *)source)->ecdsa, data, len, signature);
	}

	pthread_mutex_unlock(&mesh->mutex);
	return rval;
}

static bool refresh_invitation_key(meshlink_handle_t *mesh) {
	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	size_t count = invitation_purge_old(mesh, time(NULL) - mesh->invitation_timeout);

	if(!count) {
		// TODO: Update invitation key if necessary?
	}

	pthread_mutex_unlock(&mesh->mutex);

	return mesh->invitation_key;
}

bool meshlink_set_canonical_address(meshlink_handle_t *mesh, meshlink_node_t *node, const char *address, const char *port) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_canonical_address(%s, %s, %s)", node ? node->name : "(null)", address ? address : "(null)", port ? port : "(null)");

	if(!mesh || !node || !address) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(!is_valid_hostname(address)) {
		logger(mesh, MESHLINK_ERROR, "Invalid character in address: %s", address);
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if((node_t *)node != mesh->self && !port) {
		logger(mesh, MESHLINK_ERROR, "Missing port number!");
		meshlink_errno = MESHLINK_EINVAL;
		return false;

	}

	if(port && !is_valid_port(port)) {
		logger(mesh, MESHLINK_ERROR, "Invalid character in port: %s", address);
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	char *canonical_address;

	xasprintf(&canonical_address, "%s %s", address, port ? port : mesh->myport);

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	node_t *n = (node_t *)node;
	free(n->canonical_address);
	n->canonical_address = canonical_address;

	if(!node_write_config(mesh, n, false)) {
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	pthread_mutex_unlock(&mesh->mutex);

	return config_sync(mesh, "current");
}

bool meshlink_clear_canonical_address(meshlink_handle_t *mesh, meshlink_node_t *node) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_clear_canonical_address(%s)", node ? node->name : "(null)");

	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	node_t *n = (node_t *)node;
	free(n->canonical_address);
	n->canonical_address = NULL;

	if(!node_write_config(mesh, n, false)) {
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	pthread_mutex_unlock(&mesh->mutex);

	return config_sync(mesh, "current");
}

bool meshlink_add_invitation_address(struct meshlink_handle *mesh, const char *address, const char *port) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_add_invitation_address(%s, %s)", address ? address : "(null)", port ? port : "(null)");

	if(!mesh || !address) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(!is_valid_hostname(address)) {
		logger(mesh, MESHLINK_ERROR, "Invalid character in address: %s\n", address);
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(port && !is_valid_port(port)) {
		logger(mesh, MESHLINK_ERROR, "Invalid character in port: %s\n", address);
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	char *combo;

	if(port) {
		xasprintf(&combo, "%s/%s", address, port);
	} else {
		combo = xstrdup(address);
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(!mesh->invitation_addresses) {
		mesh->invitation_addresses = list_alloc((list_action_t)free);
	}

	list_insert_tail(mesh->invitation_addresses, combo);
	pthread_mutex_unlock(&mesh->mutex);

	return true;
}

void meshlink_clear_invitation_addresses(struct meshlink_handle *mesh) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_clear_invitation_addresses()");

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(mesh->invitation_addresses) {
		list_delete_list(mesh->invitation_addresses);
		mesh->invitation_addresses = NULL;
	}

	pthread_mutex_unlock(&mesh->mutex);
}

bool meshlink_add_address(meshlink_handle_t *mesh, const char *address) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_add_address(%s)", address ? address : "(null)");

	return meshlink_set_canonical_address(mesh, (meshlink_node_t *)mesh->self, address, NULL);
}

bool meshlink_add_external_address(meshlink_handle_t *mesh) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_add_external_address()");

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	char *address = meshlink_get_external_address(mesh);

	if(!address) {
		return false;
	}

	bool rval = meshlink_set_canonical_address(mesh, (meshlink_node_t *)mesh->self, address, NULL);
	free(address);

	return rval;
}

int meshlink_get_port(meshlink_handle_t *mesh) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	if(!mesh->myport) {
		meshlink_errno = MESHLINK_EINTERNAL;
		return -1;
	}

	int port;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	port = atoi(mesh->myport);
	pthread_mutex_unlock(&mesh->mutex);

	return port;
}

bool meshlink_set_port(meshlink_handle_t *mesh, int port) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_port(%d)", port);

	if(!mesh || port < 0 || port >= 65536 || mesh->threadstarted) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(mesh->myport && port == atoi(mesh->myport)) {
		return true;
	}

	if(!try_bind(mesh, port)) {
		meshlink_errno = MESHLINK_ENETWORK;
		return false;
	}

	devtool_trybind_probe();

	bool rval = false;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(mesh->threadstarted) {
		meshlink_errno = MESHLINK_EINVAL;
		goto done;
	}

	free(mesh->myport);
	xasprintf(&mesh->myport, "%d", port);

	/* Close down the network. This also deletes mesh->self. */
	close_network_connections(mesh);

	/* Recreate mesh->self. */
	mesh->self = new_node();
	mesh->self->name = xstrdup(mesh->name);
	mesh->self->devclass = mesh->devclass;
	mesh->self->session_id = mesh->session_id;
	xasprintf(&mesh->myport, "%d", port);

	if(!node_read_public_key(mesh, mesh->self)) {
		logger(NULL, MESHLINK_ERROR, "Could not read our host configuration file!");
		meshlink_errno = MESHLINK_ESTORAGE;
		free_node(mesh->self);
		mesh->self = NULL;
		goto done;
	} else if(!setup_network(mesh)) {
		meshlink_errno = MESHLINK_ENETWORK;
		goto done;
	}

	/* Rebuild our own list of recent addresses */
	memset(mesh->self->recent, 0, sizeof(mesh->self->recent));
	add_local_addresses(mesh);

	/* Write meshlink.conf with the updated port number */
	write_main_config_files(mesh);

	rval = config_sync(mesh, "current");

done:
	pthread_mutex_unlock(&mesh->mutex);

	return rval && meshlink_get_port(mesh) == port;
}

void meshlink_set_invitation_timeout(meshlink_handle_t *mesh, int timeout) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_invitation_timeout(%d)", timeout);

	mesh->invitation_timeout = timeout;
}

char *meshlink_invite_ex(meshlink_handle_t *mesh, meshlink_submesh_t *submesh, const char *name, uint32_t flags) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_invite_ex(%s, %s, %u)", submesh ? submesh->name : "(null)", name ? name : "(null)", flags);

	meshlink_submesh_t *s = NULL;

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(submesh) {
		s = (meshlink_submesh_t *)lookup_submesh(mesh, submesh->name);

		if(s != submesh) {
			logger(mesh, MESHLINK_ERROR, "Invalid submesh handle.\n");
			meshlink_errno = MESHLINK_EINVAL;
			return NULL;
		}
	} else {
		s = (meshlink_submesh_t *)mesh->self->submesh;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	// Check validity of the new node's name
	if(!check_id(name)) {
		logger(mesh, MESHLINK_ERROR, "Invalid name for node.\n");
		meshlink_errno = MESHLINK_EINVAL;
		pthread_mutex_unlock(&mesh->mutex);
		return NULL;
	}

	// Ensure no host configuration file with that name exists
	if(config_exists(mesh, "current", name)) {
		logger(mesh, MESHLINK_ERROR, "A host config file for %s already exists!\n", name);
		meshlink_errno = MESHLINK_EEXIST;
		pthread_mutex_unlock(&mesh->mutex);
		return NULL;
	}

	// Ensure no other nodes know about this name
	if(lookup_node(mesh, name)) {
		logger(mesh, MESHLINK_ERROR, "A node with name %s is already known!\n", name);
		meshlink_errno = MESHLINK_EEXIST;
		pthread_mutex_unlock(&mesh->mutex);
		return NULL;
	}

	// Get the local address
	char *address = get_my_hostname(mesh, flags);

	if(!address) {
		logger(mesh, MESHLINK_ERROR, "No Address known for ourselves!\n");
		meshlink_errno = MESHLINK_ERESOLV;
		pthread_mutex_unlock(&mesh->mutex);
		return NULL;
	}

	if(!refresh_invitation_key(mesh)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		pthread_mutex_unlock(&mesh->mutex);
		return NULL;
	}

	// If we changed our own host config file, write it out now
	if(mesh->self->status.dirty) {
		if(!node_write_config(mesh, mesh->self, false)) {
			logger(mesh, MESHLINK_ERROR, "Could not write our own host config file!\n");
			pthread_mutex_unlock(&mesh->mutex);
			return NULL;
		}
	}

	char hash[64];

	// Create a hash of the key.
	char *fingerprint = ecdsa_get_base64_public_key(mesh->invitation_key);
	sha512(fingerprint, strlen(fingerprint), hash);
	b64encode_urlsafe(hash, hash, 18);

	// Create a random cookie for this invitation.
	char cookie[25];
	randomize(cookie, 18);

	// Create a filename that doesn't reveal the cookie itself
	char buf[18 + strlen(fingerprint)];
	char cookiehash[64];
	memcpy(buf, cookie, 18);
	memcpy(buf + 18, fingerprint, sizeof(buf) - 18);
	sha512(buf, sizeof(buf), cookiehash);
	b64encode_urlsafe(cookiehash, cookiehash, 18);

	b64encode_urlsafe(cookie, cookie, 18);

	free(fingerprint);

	/* Construct the invitation file */
	uint8_t outbuf[4096];
	packmsg_output_t inv = {outbuf, sizeof(outbuf)};

	packmsg_add_uint32(&inv, MESHLINK_INVITATION_VERSION);
	packmsg_add_str(&inv, name);
	packmsg_add_str(&inv, s ? s->name : CORE_MESH);
	packmsg_add_int32(&inv, DEV_CLASS_UNKNOWN); /* TODO: allow this to be set by inviter? */

	/* TODO: Add several host config files to bootstrap connections.
	 * Note: make sure we only add config files of nodes that are in the core mesh or the same submesh,
	 * and are not blacklisted.
	 */
	config_t configs[5];
	memset(configs, 0, sizeof(configs));
	int count = 0;

	if(config_read(mesh, "current", mesh->self->name, &configs[count], mesh->config_key)) {
		count++;
	}

	/* Append host config files to the invitation file */
	packmsg_add_array(&inv, count);

	for(int i = 0; i < count; i++) {
		packmsg_add_bin(&inv, configs[i].buf, configs[i].len);
		config_free(&configs[i]);
	}

	config_t config = {outbuf, packmsg_output_size(&inv, outbuf)};

	if(!invitation_write(mesh, "current", cookiehash, &config, mesh->config_key)) {
		logger(mesh, MESHLINK_DEBUG, "Could not create invitation file %s: %s\n", cookiehash, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		pthread_mutex_unlock(&mesh->mutex);
		return NULL;
	}

	// Create an URL from the local address, key hash and cookie
	char *url;
	xasprintf(&url, "%s/%s%s", address, hash, cookie);
	free(address);

	pthread_mutex_unlock(&mesh->mutex);
	return url;
}

char *meshlink_invite(meshlink_handle_t *mesh, meshlink_submesh_t *submesh, const char *name) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_invite_ex(%s, %s)", submesh ? submesh->name : "(null)", name ? name : "(null)");

	return meshlink_invite_ex(mesh, submesh, name, 0);
}

bool meshlink_join(meshlink_handle_t *mesh, const char *invitation) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_join(%s)", invitation ? invitation : "(null)");

	if(!mesh || !invitation) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(mesh->storage_policy == MESHLINK_STORAGE_DISABLED) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	join_state_t state = {
		.mesh = mesh,
		.sock = -1,
	};

	ecdsa_t *key = NULL;
	ecdsa_t *hiskey = NULL;

	//TODO: think of a better name for this variable, or of a different way to tokenize the invitation URL.
	char copy[strlen(invitation) + 1];

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	//Before doing meshlink_join make sure we are not connected to another mesh
	if(mesh->threadstarted) {
		logger(mesh, MESHLINK_ERROR, "Cannot join while started\n");
		meshlink_errno = MESHLINK_EINVAL;
		goto exit;
	}

	// Refuse to join a mesh if we are already part of one. We are part of one if we know at least one other node.
	if(mesh->nodes->count > 1) {
		logger(mesh, MESHLINK_ERROR, "Already part of an existing mesh\n");
		meshlink_errno = MESHLINK_EINVAL;
		goto exit;
	}

	strcpy(copy, invitation);

	// Split the invitation URL into a list of hostname/port tuples, a key hash and a cookie.

	char *slash = strchr(copy, '/');

	if(!slash) {
		goto invalid;
	}

	*slash++ = 0;

	if(strlen(slash) != 48) {
		goto invalid;
	}

	char *address = copy;
	char *port = NULL;

	if(!b64decode(slash, state.hash, 18) || !b64decode(slash + 24, state.cookie, 18)) {
		goto invalid;
	}

	if(mesh->inviter_commits_first) {
		memcpy(state.cookie + 18, ecdsa_get_public_key(mesh->private_key), 32);
	}

	// Generate a throw-away key for the invitation.
	key = ecdsa_generate();

	if(!key) {
		meshlink_errno = MESHLINK_EINTERNAL;
		goto exit;
	}

	char *b64key = ecdsa_get_base64_public_key(key);
	char *comma;

	while(address && *address) {
		// We allow commas in the address part to support multiple addresses in one invitation URL.
		comma = strchr(address, ',');

		if(comma) {
			*comma++ = 0;
		}

		// Split of the port
		port = strrchr(address, ':');

		if(!port) {
			goto invalid;
		}

		*port++ = 0;

		// IPv6 address are enclosed in brackets, per RFC 3986
		if(*address == '[') {
			address++;
			char *bracket = strchr(address, ']');

			if(!bracket) {
				goto invalid;
			}

			*bracket++ = 0;

			if(*bracket) {
				goto invalid;
			}
		}

		// Connect to the meshlink daemon mentioned in the URL.
		struct addrinfo *ai = adns_blocking_request(mesh, xstrdup(address), xstrdup(port), SOCK_STREAM, 30);

		if(ai) {
			for(struct addrinfo *aip = ai; aip; aip = aip->ai_next) {
				state.sock = socket_in_netns(aip->ai_family, SOCK_STREAM, IPPROTO_TCP, mesh->netns);

				if(state.sock == -1) {
					logger(mesh, MESHLINK_DEBUG, "Could not open socket: %s\n", strerror(errno));
					meshlink_errno = MESHLINK_ENETWORK;
					continue;
				}

#ifdef SO_NOSIGPIPE
				int nosigpipe = 1;
				setsockopt(state.sock, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));
#endif

				set_timeout(state.sock, 5000);

				if(connect(state.sock, aip->ai_addr, aip->ai_addrlen)) {
					logger(mesh, MESHLINK_DEBUG, "Could not connect to %s port %s: %s\n", address, port, strerror(errno));
					meshlink_errno = MESHLINK_ENETWORK;
					closesocket(state.sock);
					state.sock = -1;
					continue;
				}

				break;
			}

			freeaddrinfo(ai);
		} else {
			meshlink_errno = MESHLINK_ERESOLV;
		}

		if(state.sock != -1 || !comma) {
			break;
		}

		address = comma;
	}

	if(state.sock == -1) {
		goto exit;
	}

	logger(mesh, MESHLINK_DEBUG, "Connected to %s port %s...\n", address, port);

	// Tell him we have an invitation, and give him our throw-away key.

	state.blen = 0;

	if(!sendline(state.sock, "0 ?%s %d.%d %s", b64key, PROT_MAJOR, PROT_MINOR, mesh->appname)) {
		logger(mesh, MESHLINK_ERROR, "Error sending request to %s port %s: %s\n", address, port, strerror(errno));
		meshlink_errno = MESHLINK_ENETWORK;
		goto exit;
	}

	free(b64key);

	char hisname[4096] = "";
	int code, hismajor, hisminor = 0;

	if(!recvline(&state) || sscanf(state.line, "%d %s %d.%d", &code, hisname, &hismajor, &hisminor) < 3 || code != 0 || hismajor != PROT_MAJOR || !check_id(hisname) || !recvline(&state) || !rstrip(state.line) || sscanf(state.line, "%d ", &code) != 1 || code != ACK || strlen(state.line) < 3) {
		logger(mesh, MESHLINK_ERROR, "Cannot read greeting from peer\n");
		meshlink_errno = MESHLINK_ENETWORK;
		goto exit;
	}

	// Check if the hash of the key he gave us matches the hash in the URL.
	char *fingerprint = state.line + 2;
	char hishash[64];

	if(sha512(fingerprint, strlen(fingerprint), hishash)) {
		logger(mesh, MESHLINK_ERROR, "Could not create hash\n%s\n", state.line + 2);
		meshlink_errno = MESHLINK_EINTERNAL;
		goto exit;
	}

	if(memcmp(hishash, state.hash, 18)) {
		logger(mesh, MESHLINK_ERROR, "Peer has an invalid key!\n%s\n", state.line + 2);
		meshlink_errno = MESHLINK_EPEER;
		goto exit;
	}

	hiskey = ecdsa_set_base64_public_key(fingerprint);

	if(!hiskey) {
		meshlink_errno = MESHLINK_EINTERNAL;
		goto exit;
	}

	// Start an SPTPS session
	if(!sptps_start(&state.sptps, &state, true, false, key, hiskey, meshlink_invitation_label, sizeof(meshlink_invitation_label), invitation_send, invitation_receive)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		goto exit;
	}

	// Feed rest of input buffer to SPTPS
	if(!sptps_receive_data(&state.sptps, state.buffer, state.blen)) {
		meshlink_errno = MESHLINK_EPEER;
		goto exit;
	}

	ssize_t len;
	logger(mesh, MESHLINK_DEBUG, "Starting invitation recv loop: %d %zu\n", state.sock, sizeof(state.line));

	while((len = recv(state.sock, state.line, sizeof(state.line), 0))) {
		if(len < 0) {
			if(errno == EINTR) {
				continue;
			}

			logger(mesh, MESHLINK_ERROR, "Error reading data from %s port %s: %s\n", address, port, strerror(errno));
			meshlink_errno = MESHLINK_ENETWORK;
			goto exit;
		}

		if(!sptps_receive_data(&state.sptps, state.line, len)) {
			meshlink_errno = MESHLINK_EPEER;
			goto exit;
		}
	}

	if(!state.success) {
		logger(mesh, MESHLINK_ERROR, "Connection closed by peer, invitation cancelled.\n");
		meshlink_errno = MESHLINK_EPEER;
		goto exit;
	}

	sptps_stop(&state.sptps);
	ecdsa_free(hiskey);
	ecdsa_free(key);
	closesocket(state.sock);

	pthread_mutex_unlock(&mesh->mutex);
	return true;

invalid:
	logger(mesh, MESHLINK_ERROR, "Invalid invitation URL\n");
	meshlink_errno = MESHLINK_EINVAL;
exit:
	sptps_stop(&state.sptps);
	ecdsa_free(hiskey);
	ecdsa_free(key);

	if(state.sock != -1) {
		closesocket(state.sock);
	}

	pthread_mutex_unlock(&mesh->mutex);
	return false;
}

char *meshlink_export(meshlink_handle_t *mesh) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	// Create a config file on the fly.

	uint8_t buf[4096];
	packmsg_output_t out = {buf, sizeof(buf)};
	packmsg_add_uint32(&out, MESHLINK_CONFIG_VERSION);
	packmsg_add_str(&out, mesh->name);
	packmsg_add_str(&out, CORE_MESH);

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	packmsg_add_int32(&out, mesh->self->devclass);
	packmsg_add_bool(&out, mesh->self->status.blacklisted);
	packmsg_add_bin(&out, ecdsa_get_public_key(mesh->private_key), 32);

	if(mesh->self->canonical_address && !strchr(mesh->self->canonical_address, ' ')) {
		char *canonical_address = NULL;
		xasprintf(&canonical_address, "%s %s", mesh->self->canonical_address, mesh->myport);
		packmsg_add_str(&out, canonical_address);
		free(canonical_address);
	} else {
		packmsg_add_str(&out, mesh->self->canonical_address ? mesh->self->canonical_address : "");
	}

	uint32_t count = 0;

	for(uint32_t i = 0; i < MAX_RECENT; i++) {
		if(mesh->self->recent[i].sa.sa_family) {
			count++;
		} else {
			break;
		}
	}

	packmsg_add_array(&out, count);

	for(uint32_t i = 0; i < count; i++) {
		packmsg_add_sockaddr(&out, &mesh->self->recent[i]);
	}

	packmsg_add_int64(&out, 0);
	packmsg_add_int64(&out, 0);

	pthread_mutex_unlock(&mesh->mutex);

	if(!packmsg_output_ok(&out)) {
		logger(mesh, MESHLINK_ERROR, "Error creating export data\n");
		meshlink_errno = MESHLINK_EINTERNAL;
		return NULL;
	}

	// Prepare a base64-encoded packmsg array containing our config file

	uint32_t len = packmsg_output_size(&out, buf);
	uint32_t len2 = ((len + 4) * 4) / 3 + 4;
	uint8_t *buf2 = xmalloc(len2);
	packmsg_output_t out2 = {buf2, len2};
	packmsg_add_array(&out2, 1);
	packmsg_add_bin(&out2, buf, packmsg_output_size(&out, buf));

	if(!packmsg_output_ok(&out2)) {
		logger(mesh, MESHLINK_ERROR, "Error creating export data\n");
		meshlink_errno = MESHLINK_EINTERNAL;
		free(buf2);
		return NULL;
	}

	b64encode_urlsafe(buf2, (char *)buf2, packmsg_output_size(&out2, buf2));

	return (char *)buf2;
}

bool meshlink_import(meshlink_handle_t *mesh, const char *data) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_import(%p)", (const void *)data);

	if(!mesh || !data) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	size_t datalen = strlen(data);
	uint8_t *buf = xmalloc(datalen);
	int buflen = b64decode(data, buf, datalen);

	if(!buflen) {
		logger(mesh, MESHLINK_ERROR, "Invalid data\n");
		free(buf);
		meshlink_errno = MESHLINK_EPEER;
		return false;
	}

	packmsg_input_t in = {buf, buflen};
	uint32_t count = packmsg_get_array(&in);

	if(!count) {
		logger(mesh, MESHLINK_ERROR, "Invalid data\n");
		free(buf);
		meshlink_errno = MESHLINK_EPEER;
		return false;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	while(count--) {
		const void *data2;
		uint32_t len2 = packmsg_get_bin_raw(&in, &data2);

		if(!len2) {
			break;
		}

		packmsg_input_t in2 = {data2, len2};
		uint32_t version = packmsg_get_uint32(&in2);
		char *name = packmsg_get_str_dup(&in2);

		if(!packmsg_input_ok(&in2) || version != MESHLINK_CONFIG_VERSION || !check_id(name)) {
			free(name);
			packmsg_input_invalidate(&in);
			break;
		}

		if(!check_id(name)) {
			free(name);
			break;
		}

		node_t *n = lookup_node(mesh, name);

		if(n) {
			logger(mesh, MESHLINK_DEBUG, "Node %s already exists, not importing\n", name);
			free(name);
			continue;
		}

		n = new_node();
		n->name = name;

		config_t config = {data2, len2};

		if(!node_read_from_config(mesh, n, &config)) {
			free_node(n);
			packmsg_input_invalidate(&in);
			break;
		}

		/* Clear the reachability times, since we ourself have never seen these nodes yet */
		n->last_reachable = 0;
		n->last_unreachable = 0;

		if(!node_write_config(mesh, n, true)) {
			free_node(n);
			free(buf);
			return false;
		}

		node_add(mesh, n);
	}

	pthread_mutex_unlock(&mesh->mutex);

	free(buf);

	if(!packmsg_done(&in)) {
		logger(mesh, MESHLINK_ERROR, "Invalid data\n");
		meshlink_errno = MESHLINK_EPEER;
		return false;
	}

	if(!config_sync(mesh, "current")) {
		return false;
	}

	return true;
}

static bool blacklist(meshlink_handle_t *mesh, node_t *n) {
	if(n == mesh->self) {
		logger(mesh, MESHLINK_ERROR, "%s blacklisting itself?\n", n->name);
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(n->status.blacklisted) {
		logger(mesh, MESHLINK_DEBUG, "Node %s already blacklisted\n", n->name);
		return true;
	}

	n->status.blacklisted = true;

	/* Immediately shut down any connections we have with the blacklisted node.
	 * We can't call terminate_connection(), because we might be called from a callback function.
	 */
	for list_each(connection_t, c, mesh->connections) {
		if(c->node == n) {
			if(c->status.active) {
				send_error(mesh, c, BLACKLISTED, "blacklisted");
			}

			shutdown(c->socket, SHUT_RDWR);
		}
	}

	utcp_reset_all_connections(n->utcp);

	n->mtu = 0;
	n->minmtu = 0;
	n->maxmtu = MTU;
	n->mtuprobes = 0;
	n->status.udp_confirmed = false;

	if(n->status.reachable) {
		n->last_unreachable = time(NULL);
	}

	/* Graph updates will suppress status updates for blacklisted nodes, so we need to
	 * manually call the status callback if necessary.
	 */
	if(n->status.reachable && mesh->node_status_cb) {
		mesh->node_status_cb(mesh, (meshlink_node_t *)n, false);
	}

	/* Remove any outstanding invitations */
	invitation_purge_node(mesh, n->name);

	return node_write_config(mesh, n, true) && config_sync(mesh, "current");
}

bool meshlink_blacklist(meshlink_handle_t *mesh, meshlink_node_t *node) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_blacklist(%s)", node ? node->name : "(null)");

	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(!blacklist(mesh, (node_t *)node)) {
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	pthread_mutex_unlock(&mesh->mutex);

	logger(mesh, MESHLINK_DEBUG, "Blacklisted %s.\n", node->name);
	return true;
}

bool meshlink_blacklist_by_name(meshlink_handle_t *mesh, const char *name) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_blacklist_by_name(%s)", name ? name : "(null)");

	if(!mesh || !name) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	node_t *n = lookup_node(mesh, (char *)name);

	if(!n) {
		n = new_node();
		n->name = xstrdup(name);
		node_add(mesh, n);
	}

	if(!blacklist(mesh, (node_t *)n)) {
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	pthread_mutex_unlock(&mesh->mutex);

	logger(mesh, MESHLINK_DEBUG, "Blacklisted %s.\n", name);
	return true;
}

static bool whitelist(meshlink_handle_t *mesh, node_t *n) {
	if(n == mesh->self) {
		logger(mesh, MESHLINK_ERROR, "%s whitelisting itself?\n", n->name);
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(!n->status.blacklisted) {
		logger(mesh, MESHLINK_DEBUG, "Node %s was already whitelisted\n", n->name);
		return true;
	}

	n->status.blacklisted = false;

	if(n->status.reachable) {
		n->last_reachable = time(NULL);
		update_node_status(mesh, n);
	}

	return node_write_config(mesh, n, true) && config_sync(mesh, "current");
}

bool meshlink_whitelist(meshlink_handle_t *mesh, meshlink_node_t *node) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_whitelist(%s)", node ? node->name : "(null)");

	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(!whitelist(mesh, (node_t *)node)) {
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	pthread_mutex_unlock(&mesh->mutex);

	logger(mesh, MESHLINK_DEBUG, "Whitelisted %s.\n", node->name);
	return true;
}

bool meshlink_whitelist_by_name(meshlink_handle_t *mesh, const char *name) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_whitelist_by_name(%s)", name ? name : "(null)");

	if(!mesh || !name) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	node_t *n = lookup_node(mesh, (char *)name);

	if(!n) {
		n = new_node();
		n->name = xstrdup(name);
		node_add(mesh, n);
	}

	if(!whitelist(mesh, (node_t *)n)) {
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	pthread_mutex_unlock(&mesh->mutex);

	logger(mesh, MESHLINK_DEBUG, "Whitelisted %s.\n", name);
	return true;
}

void meshlink_set_default_blacklist(meshlink_handle_t *mesh, bool blacklist) {
	mesh->default_blacklist = blacklist;
}

bool meshlink_forget_node(meshlink_handle_t *mesh, meshlink_node_t *node) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_forget_node(%s)", node ? node->name : "(null)");

	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	node_t *n = (node_t *)node;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	/* Check that the node is not reachable */
	if(n->status.reachable || n->connection) {
		pthread_mutex_unlock(&mesh->mutex);
		logger(mesh, MESHLINK_WARNING, "Could not forget %s: still reachable", n->name);
		return false;
	}

	/* Check that we don't have any active UTCP connections */
	if(n->utcp && utcp_is_active(n->utcp)) {
		pthread_mutex_unlock(&mesh->mutex);
		logger(mesh, MESHLINK_WARNING, "Could not forget %s: active UTCP connections", n->name);
		return false;
	}

	/* Check that we have no active connections to this node */
	for list_each(connection_t, c, mesh->connections) {
		if(c->node == n) {
			pthread_mutex_unlock(&mesh->mutex);
			logger(mesh, MESHLINK_WARNING, "Could not forget %s: active connection", n->name);
			return false;
		}
	}

	/* Remove any pending outgoings to this node */
	if(mesh->outgoings) {
		for list_each(outgoing_t, outgoing, mesh->outgoings) {
			if(outgoing->node == n) {
				list_delete_node(mesh->outgoings, list_node);
			}
		}
	}

	/* Delete the config file for this node */
	if(!config_delete(mesh, "current", n->name)) {
		pthread_mutex_unlock(&mesh->mutex);
		return false;
	}

	/* Delete any pending invitations */
	invitation_purge_node(mesh, n->name);

	/* Delete the node struct and any remaining edges referencing this node */
	node_del(mesh, n);

	pthread_mutex_unlock(&mesh->mutex);

	return config_sync(mesh, "current");
}

/* Hint that a hostname may be found at an address
 * See header file for detailed comment.
 */
void meshlink_hint_address(meshlink_handle_t *mesh, meshlink_node_t *node, const struct sockaddr *addr) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_hint_address(%s, %p)", node ? node->name : "(null)", (const void *)addr);

	if(!mesh || !node || !addr) {
		meshlink_errno = EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	node_t *n = (node_t *)node;

	if(node_add_recent_address(mesh, n, (sockaddr_t *)addr)) {
		if(!node_write_config(mesh, n, false)) {
			logger(mesh, MESHLINK_DEBUG, "Could not update %s\n", n->name);
		}
	}

	pthread_mutex_unlock(&mesh->mutex);
	// @TODO do we want to fire off a connection attempt right away?
}

static bool channel_pre_accept(struct utcp *utcp, uint16_t port) {
	(void)port;
	node_t *n = utcp->priv;
	meshlink_handle_t *mesh = n->mesh;

	if(mesh->channel_accept_cb && mesh->channel_listen_cb) {
		return mesh->channel_listen_cb(mesh, (meshlink_node_t *)n, port);
	} else {
		return mesh->channel_accept_cb;
	}
}

/* Finish one AIO buffer, return true if the channel is still open. */
static bool aio_finish_one(meshlink_handle_t *mesh, meshlink_channel_t *channel, meshlink_aio_buffer_t **head) {
	meshlink_aio_buffer_t *aio = *head;
	*head = aio->next;

	if(channel->c) {
		channel->in_callback = true;

		if(aio->data) {
			if(aio->cb.buffer) {
				aio->cb.buffer(mesh, channel, aio->data, aio->done, aio->priv);
			}
		} else {
			if(aio->cb.fd) {
				aio->cb.fd(mesh, channel, aio->fd, aio->done, aio->priv);
			}
		}

		channel->in_callback = false;

		if(!channel->c) {
			free(aio);
			free(channel);
			return false;
		}
	}

	free(aio);
	return true;
}

/* Finish all AIO buffers, return true if the channel is still open. */
static bool aio_abort(meshlink_handle_t *mesh, meshlink_channel_t *channel, meshlink_aio_buffer_t **head) {
	while(*head) {
		if(!aio_finish_one(mesh, channel, head)) {
			return false;
		}
	}

	return true;
}

static ssize_t channel_recv(struct utcp_connection *connection, const void *data, size_t len) {
	meshlink_channel_t *channel = connection->priv;

	if(!channel) {
		abort();
	}

	node_t *n = channel->node;
	meshlink_handle_t *mesh = n->mesh;

	if(n->status.destroyed) {
		meshlink_channel_close(mesh, channel);
		return len;
	}

	const char *p = data;
	size_t left = len;

	while(channel->aio_receive) {
		if(!len) {
			/* This receive callback signalled an error, abort all outstanding AIO buffers. */
			if(!aio_abort(mesh, channel, &channel->aio_receive)) {
				return len;
			}

			break;
		}

		meshlink_aio_buffer_t *aio = channel->aio_receive;
		size_t todo = aio->len - aio->done;

		if(todo > left) {
			todo = left;
		}

		if(aio->data) {
			memcpy((char *)aio->data + aio->done, p, todo);
		} else {
			ssize_t result = write(aio->fd, p, todo);

			if(result <= 0) {
				if(result < 0 && errno == EINTR) {
					continue;
				}

				/* Writing to fd failed, cancel just this AIO buffer. */
				logger(mesh, MESHLINK_ERROR, "Writing to AIO fd %d failed: %s", aio->fd, strerror(errno));

				if(!aio_finish_one(mesh, channel, &channel->aio_receive)) {
					return len;
				}

				continue;
			}

			todo = result;
		}

		aio->done += todo;
		p += todo;
		left -= todo;

		if(aio->done == aio->len) {
			if(!aio_finish_one(mesh, channel, &channel->aio_receive)) {
				return len;
			}
		}

		if(!left) {
			return len;
		}
	}

	if(channel->receive_cb) {
		channel->receive_cb(mesh, channel, p, left);
	}

	return len;
}

static void channel_accept(struct utcp_connection *utcp_connection, uint16_t port) {
	node_t *n = utcp_connection->utcp->priv;

	if(!n) {
		abort();
	}

	meshlink_handle_t *mesh = n->mesh;

	if(!mesh->channel_accept_cb) {
		return;
	}

	meshlink_channel_t *channel = xzalloc(sizeof(*channel));
	channel->node = n;
	channel->c = utcp_connection;

	if(mesh->channel_accept_cb(mesh, channel, port, NULL, 0)) {
		utcp_accept(utcp_connection, channel_recv, channel);
	} else {
		free(channel);
	}
}

static void channel_retransmit(struct utcp_connection *utcp_connection) {
	node_t *n = utcp_connection->utcp->priv;
	meshlink_handle_t *mesh = n->mesh;

	if(n->mtuprobes == 31 && n->mtutimeout.cb) {
		timeout_set(&mesh->loop, &n->mtutimeout, &(struct timespec) {
			0, 0
		});
	}
}

static ssize_t channel_send(struct utcp *utcp, const void *data, size_t len) {
	node_t *n = utcp->priv;

	if(n->status.destroyed) {
		return -1;
	}

	meshlink_handle_t *mesh = n->mesh;
	return meshlink_send_immediate(mesh, (meshlink_node_t *)n, data, len) ? (ssize_t)len : -1;
}

void meshlink_set_channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, meshlink_channel_receive_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_channel_receive_cb(%p, %p)", (void *)channel, (void *)(intptr_t)cb);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	channel->receive_cb = cb;
}

void channel_receive(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len) {
	(void)mesh;
	node_t *n = (node_t *)source;

	if(!n->utcp) {
		abort();
	}

	utcp_recv(n->utcp, data, len);
}

static void channel_poll(struct utcp_connection *connection, size_t len) {
	meshlink_channel_t *channel = connection->priv;

	if(!channel) {
		abort();
	}

	node_t *n = channel->node;
	meshlink_handle_t *mesh = n->mesh;

	while(channel->aio_send) {
		if(!len) {
			/* This poll callback signalled an error, abort all outstanding AIO buffers. */
			if(!aio_abort(mesh, channel, &channel->aio_send)) {
				return;
			}

			break;
		}

		/* We have at least one AIO buffer. Send as much as possible from the buffers. */
		meshlink_aio_buffer_t *aio = channel->aio_send;
		size_t todo = aio->len - aio->done;
		ssize_t sent;

		if(todo > len) {
			todo = len;
		}

		if(aio->data) {
			sent = utcp_send(connection, (char *)aio->data + aio->done, todo);
		} else {
			/* Limit the amount we read at once to avoid stack overflows */
			if(todo > 65536) {
				todo = 65536;
			}

			char buf[todo];
			ssize_t result = read(aio->fd, buf, todo);

			if(result > 0) {
				todo = result;
				sent = utcp_send(connection, buf, todo);
			} else {
				if(result < 0 && errno == EINTR) {
					continue;
				}

				/* Reading from fd failed, cancel just this AIO buffer. */
				if(result != 0) {
					logger(mesh, MESHLINK_ERROR, "Reading from AIO fd %d failed: %s", aio->fd, strerror(errno));
				}

				if(!aio_finish_one(mesh, channel, &channel->aio_send)) {
					return;
				}

				continue;
			}
		}

		if(sent != (ssize_t)todo) {
			/* Sending failed, abort all outstanding AIO buffers and send a poll callback. */
			if(!aio_abort(mesh, channel, &channel->aio_send)) {
				return;
			}

			len = 0;
			break;
		}

		aio->done += sent;
		len -= sent;

		/* If we didn't finish this buffer, exit early. */
		if(aio->done < aio->len) {
			return;
		}

		/* Signal completion of this buffer, and go to the next one. */
		if(!aio_finish_one(mesh, channel, &channel->aio_send)) {
			return;
		}

		if(!len) {
			return;
		}
	}

	if(channel->poll_cb) {
		channel->poll_cb(mesh, channel, len);
	} else {
		utcp_set_poll_cb(connection, NULL);
	}
}

void meshlink_set_channel_poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, meshlink_channel_poll_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_channel_poll_cb(%p, %p)", (void *)channel, (void *)(intptr_t)cb);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	channel->poll_cb = cb;
	utcp_set_poll_cb(channel->c, (cb || channel->aio_send) ? channel_poll : NULL);
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_channel_listen_cb(meshlink_handle_t *mesh, meshlink_channel_listen_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_channel_listen_cb(%p)", (void *)(intptr_t)cb);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->channel_listen_cb = cb;

	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_channel_accept_cb(meshlink_handle_t *mesh, meshlink_channel_accept_cb_t cb) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_channel_accept_cb(%p)", (void *)(intptr_t)cb);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->channel_accept_cb = cb;

	for splay_each(node_t, n, mesh->nodes) {
		if(!n->utcp && n != mesh->self) {
			n->utcp = utcp_init(channel_accept, channel_pre_accept, channel_send, n);
			utcp_set_mtu(n->utcp, n->mtu - sizeof(meshlink_packethdr_t));
			utcp_set_retransmit_cb(n->utcp, channel_retransmit);
		}
	}

	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_channel_sndbuf(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t size) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_channel_sndbuf(%p, %zu)", (void *)channel, size);

	meshlink_set_channel_sndbuf_storage(mesh, channel, NULL, size);
}

void meshlink_set_channel_rcvbuf(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t size) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_channel_rcvbuf(%p, %zu)", (void *)channel, size);

	meshlink_set_channel_rcvbuf_storage(mesh, channel, NULL, size);
}

void meshlink_set_channel_sndbuf_storage(meshlink_handle_t *mesh, meshlink_channel_t *channel, void *buf, size_t size) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_channel_sndbuf_storage(%p, %p, %zu)", (void *)channel, buf, size);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	utcp_set_sndbuf(channel->c, buf, size);
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_channel_rcvbuf_storage(meshlink_handle_t *mesh, meshlink_channel_t *channel, void *buf, size_t size) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_channel_rcvbuf_storage(%p, %p, %zu)", (void *)channel, buf, size);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	utcp_set_rcvbuf(channel->c, buf, size);
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_channel_flags(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint32_t flags) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_channel_flags(%p, %u)", (void *)channel, flags);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	utcp_set_flags(channel->c, flags);
	pthread_mutex_unlock(&mesh->mutex);
}

meshlink_channel_t *meshlink_channel_open_ex(meshlink_handle_t *mesh, meshlink_node_t *node, uint16_t port, meshlink_channel_receive_cb_t cb, const void *data, size_t len, uint32_t flags) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_channel_open_ex(%s, %u, %p, %p, %zu, %u)", node ? node->name : "(null)", port, (void *)(intptr_t)cb, data, len, flags);

	if(data && len) {
		abort();        // TODO: handle non-NULL data
	}

	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	node_t *n = (node_t *)node;

	if(!n->utcp) {
		n->utcp = utcp_init(channel_accept, channel_pre_accept, channel_send, n);
		utcp_set_mtu(n->utcp, n->mtu - sizeof(meshlink_packethdr_t));
		utcp_set_retransmit_cb(n->utcp, channel_retransmit);

		if(!n->utcp) {
			meshlink_errno = errno == ENOMEM ? MESHLINK_ENOMEM : MESHLINK_EINTERNAL;
			pthread_mutex_unlock(&mesh->mutex);
			return NULL;
		}
	}

	if(n->status.blacklisted) {
		logger(mesh, MESHLINK_ERROR, "Cannot open a channel with blacklisted node\n");
		meshlink_errno = MESHLINK_EBLACKLISTED;
		pthread_mutex_unlock(&mesh->mutex);
		return NULL;
	}

	meshlink_channel_t *channel = xzalloc(sizeof(*channel));
	channel->node = n;
	channel->receive_cb = cb;

	if(data && !len) {
		channel->priv = (void *)data;
	}

	channel->c = utcp_connect_ex(n->utcp, port, channel_recv, channel, flags);

	pthread_mutex_unlock(&mesh->mutex);

	if(!channel->c) {
		meshlink_errno = errno == ENOMEM ? MESHLINK_ENOMEM : MESHLINK_EINTERNAL;
		free(channel);
		return NULL;
	}

	return channel;
}

meshlink_channel_t *meshlink_channel_open(meshlink_handle_t *mesh, meshlink_node_t *node, uint16_t port, meshlink_channel_receive_cb_t cb, const void *data, size_t len) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_channel_open_ex(%s, %u, %p, %p, %zu)", node ? node->name : "(null)", port, (void *)(intptr_t)cb, data, len);

	return meshlink_channel_open_ex(mesh, node, port, cb, data, len, MESHLINK_CHANNEL_TCP);
}

void meshlink_channel_shutdown(meshlink_handle_t *mesh, meshlink_channel_t *channel, int direction) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_channel_shutdown(%p, %d)", (void *)channel, direction);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	utcp_shutdown(channel->c, direction);
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_channel_close(meshlink_handle_t *mesh, meshlink_channel_t *channel) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_channel_close(%p)", (void *)channel);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(channel->c) {
		utcp_close(channel->c);
		channel->c = NULL;

		/* Clean up any outstanding AIO buffers. */
		aio_abort(mesh, channel, &channel->aio_send);
		aio_abort(mesh, channel, &channel->aio_receive);
	}

	if(!channel->in_callback) {
		free(channel);
	}

	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_channel_abort(meshlink_handle_t *mesh, meshlink_channel_t *channel) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_channel_abort(%p)", (void *)channel);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(channel->c) {
		utcp_abort(channel->c);
		channel->c = NULL;

		/* Clean up any outstanding AIO buffers. */
		aio_abort(mesh, channel, &channel->aio_send);
		aio_abort(mesh, channel, &channel->aio_receive);
	}

	if(!channel->in_callback) {
		free(channel);
	}

	pthread_mutex_unlock(&mesh->mutex);
}

ssize_t meshlink_channel_send(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_channel_send(%p, %p, %zu)", (void *)channel, data, len);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	if(!len) {
		return 0;
	}

	if(!data) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	// TODO: more finegrained locking.
	// Ideally we want to put the data into the UTCP connection's send buffer.
	// Then, preferably only if there is room in the receiver window,
	// kick the meshlink thread to go send packets.

	ssize_t retval;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	/* Disallow direct calls to utcp_send() while we still have AIO active. */
	if(channel->aio_send) {
		retval = 0;
	} else {
		retval = utcp_send(channel->c, data, len);
	}

	pthread_mutex_unlock(&mesh->mutex);

	if(retval < 0) {
		meshlink_errno = MESHLINK_ENETWORK;
	}

	return retval;
}

bool meshlink_channel_aio_send(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, meshlink_aio_cb_t cb, void *priv) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_channel_aio_send(%p, %p, %zu, %p, %p)", (void *)channel, data, len, (void *)(intptr_t)cb, priv);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(!len || !data) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	meshlink_aio_buffer_t *aio = xzalloc(sizeof(*aio));
	aio->data = data;
	aio->len = len;
	aio->cb.buffer = cb;
	aio->priv = priv;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	/* Append the AIO buffer descriptor to the end of the chain */
	meshlink_aio_buffer_t **p = &channel->aio_send;

	while(*p) {
		p = &(*p)->next;
	}

	*p = aio;

	/* Ensure the poll callback is set, and call it right now to push data if possible */
	utcp_set_poll_cb(channel->c, channel_poll);
	size_t todo = MIN(len, utcp_get_sndbuf_free(channel->c));

	if(todo) {
		channel_poll(channel->c, todo);
	}

	pthread_mutex_unlock(&mesh->mutex);

	return true;
}

bool meshlink_channel_aio_fd_send(meshlink_handle_t *mesh, meshlink_channel_t *channel, int fd, size_t len, meshlink_aio_fd_cb_t cb, void *priv) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_channel_aio_fd_send(%p, %d, %zu, %p, %p)", (void *)channel, fd, len, (void *)(intptr_t)cb, priv);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(!len || fd == -1) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	meshlink_aio_buffer_t *aio = xzalloc(sizeof(*aio));
	aio->fd = fd;
	aio->len = len;
	aio->cb.fd = cb;
	aio->priv = priv;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	/* Append the AIO buffer descriptor to the end of the chain */
	meshlink_aio_buffer_t **p = &channel->aio_send;

	while(*p) {
		p = &(*p)->next;
	}

	*p = aio;

	/* Ensure the poll callback is set, and call it right now to push data if possible */
	utcp_set_poll_cb(channel->c, channel_poll);
	size_t left = utcp_get_sndbuf_free(channel->c);

	if(left) {
		channel_poll(channel->c, left);
	}

	pthread_mutex_unlock(&mesh->mutex);

	return true;
}

bool meshlink_channel_aio_receive(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, meshlink_aio_cb_t cb, void *priv) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_channel_aio_receive(%p, %p, %zu, %p, %p)", (void *)channel, data, len, (void *)(intptr_t)cb, priv);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(!len || !data) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	meshlink_aio_buffer_t *aio = xzalloc(sizeof(*aio));
	aio->data = data;
	aio->len = len;
	aio->cb.buffer = cb;
	aio->priv = priv;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	/* Append the AIO buffer descriptor to the end of the chain */
	meshlink_aio_buffer_t **p = &channel->aio_receive;

	while(*p) {
		p = &(*p)->next;
	}

	*p = aio;

	pthread_mutex_unlock(&mesh->mutex);

	return true;
}

bool meshlink_channel_aio_fd_receive(meshlink_handle_t *mesh, meshlink_channel_t *channel, int fd, size_t len, meshlink_aio_fd_cb_t cb, void *priv) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_channel_aio_fd_receive(%p, %d, %zu, %p, %p)", (void *)channel, fd, len, (void *)(intptr_t)cb, priv);

	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(!len || fd == -1) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	meshlink_aio_buffer_t *aio = xzalloc(sizeof(*aio));
	aio->fd = fd;
	aio->len = len;
	aio->cb.fd = cb;
	aio->priv = priv;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	/* Append the AIO buffer descriptor to the end of the chain */
	meshlink_aio_buffer_t **p = &channel->aio_receive;

	while(*p) {
		p = &(*p)->next;
	}

	*p = aio;

	pthread_mutex_unlock(&mesh->mutex);

	return true;
}

uint32_t meshlink_channel_get_flags(meshlink_handle_t *mesh, meshlink_channel_t *channel) {
	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	return channel->c->flags;
}

size_t meshlink_channel_get_sendq(meshlink_handle_t *mesh, meshlink_channel_t *channel) {
	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	return utcp_get_sendq(channel->c);
}

size_t meshlink_channel_get_recvq(meshlink_handle_t *mesh, meshlink_channel_t *channel) {
	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	return utcp_get_recvq(channel->c);
}

size_t meshlink_channel_get_mss(meshlink_handle_t *mesh, meshlink_channel_t *channel) {
	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	return utcp_get_mss(channel->node->utcp);
}

void meshlink_set_node_channel_timeout(meshlink_handle_t *mesh, meshlink_node_t *node, int timeout) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_node_channel_timeout(%s, %d)", node ? node->name : "(null)", timeout);

	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	node_t *n = (node_t *)node;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(!n->utcp) {
		n->utcp = utcp_init(channel_accept, channel_pre_accept, channel_send, n);
		utcp_set_mtu(n->utcp, n->mtu - sizeof(meshlink_packethdr_t));
		utcp_set_retransmit_cb(n->utcp, channel_retransmit);
	}

	utcp_set_user_timeout(n->utcp, timeout);

	pthread_mutex_unlock(&mesh->mutex);
}

void update_node_status(meshlink_handle_t *mesh, node_t *n) {
	if(n->status.reachable && mesh->channel_accept_cb && !n->utcp) {
		n->utcp = utcp_init(channel_accept, channel_pre_accept, channel_send, n);
		utcp_set_mtu(n->utcp, n->mtu - sizeof(meshlink_packethdr_t));
		utcp_set_retransmit_cb(n->utcp, channel_retransmit);
	}

	if(mesh->node_status_cb) {
		mesh->node_status_cb(mesh, (meshlink_node_t *)n, n->status.reachable && !n->status.blacklisted);
	}

	if(mesh->node_pmtu_cb) {
		mesh->node_pmtu_cb(mesh, (meshlink_node_t *)n, n->minmtu);
	}
}

void update_node_pmtu(meshlink_handle_t *mesh, node_t *n) {
	utcp_set_mtu(n->utcp, (n->minmtu > MINMTU ? n->minmtu : MINMTU) - sizeof(meshlink_packethdr_t));

	if(mesh->node_pmtu_cb && !n->status.blacklisted) {
		mesh->node_pmtu_cb(mesh, (meshlink_node_t *)n, n->minmtu);
	}
}

void handle_duplicate_node(meshlink_handle_t *mesh, node_t *n) {
	if(!mesh->node_duplicate_cb || n->status.duplicate) {
		return;
	}

	n->status.duplicate = true;
	mesh->node_duplicate_cb(mesh, (meshlink_node_t *)n);
}

void meshlink_enable_discovery(meshlink_handle_t *mesh, bool enable) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_enable_discovery(%d)", enable);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(mesh->discovery.enabled == enable) {
		goto end;
	}

	if(mesh->threadstarted) {
		if(enable) {
			discovery_start(mesh);
		} else {
			discovery_stop(mesh);
		}
	}

	mesh->discovery.enabled = enable;

end:
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_hint_network_change(struct meshlink_handle *mesh) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_hint_network_change()");

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(mesh->discovery.enabled) {
		scan_ifaddrs(mesh);
	}

	if(mesh->loop.now.tv_sec > mesh->discovery.last_update + 5) {
		mesh->discovery.last_update = mesh->loop.now.tv_sec;
		handle_network_change(mesh, 1);
	}

	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_dev_class_timeouts(meshlink_handle_t *mesh, dev_class_t devclass, int pinginterval, int pingtimeout) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_dev_class_timeouts(%d, %d, %d)", devclass, pinginterval, pingtimeout);

	if(!mesh || devclass < 0 || devclass >= DEV_CLASS_COUNT) {
		meshlink_errno = EINVAL;
		return;
	}

	if(pinginterval < 1 || pingtimeout < 1 || pingtimeout > pinginterval) {
		meshlink_errno = EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->dev_class_traits[devclass].pinginterval = pinginterval;
	mesh->dev_class_traits[devclass].pingtimeout = pingtimeout;
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_dev_class_fast_retry_period(meshlink_handle_t *mesh, dev_class_t devclass, int fast_retry_period) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_dev_class_fast_retry_period(%d, %d)", devclass, fast_retry_period);

	if(!mesh || devclass < 0 || devclass >= DEV_CLASS_COUNT) {
		meshlink_errno = EINVAL;
		return;
	}

	if(fast_retry_period < 0) {
		meshlink_errno = EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->dev_class_traits[devclass].fast_retry_period = fast_retry_period;
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_dev_class_maxtimeout(struct meshlink_handle *mesh, dev_class_t devclass, int maxtimeout) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_dev_class_fast_maxtimeout(%d, %d)", devclass, maxtimeout);

	if(!mesh || devclass < 0 || devclass >= DEV_CLASS_COUNT) {
		meshlink_errno = EINVAL;
		return;
	}

	if(maxtimeout < 0) {
		meshlink_errno = EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->dev_class_traits[devclass].maxtimeout = maxtimeout;
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_reset_timers(struct meshlink_handle *mesh) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_reset_timers()");

	if(!mesh) {
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	handle_network_change(mesh, true);

	if(mesh->discovery.enabled) {
		discovery_refresh(mesh);
	}

	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_inviter_commits_first(struct meshlink_handle *mesh, bool inviter_commits_first) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_inviter_commits_first(%d)", inviter_commits_first);

	if(!mesh) {
		meshlink_errno = EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->inviter_commits_first = inviter_commits_first;
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_external_address_discovery_url(struct meshlink_handle *mesh, const char *url) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_external_address_discovery_url(%s)", url ? url : "(null)");

	if(!mesh) {
		meshlink_errno = EINVAL;
		return;
	}

	if(url && (strncmp(url, "http://", 7) || strchr(url, ' '))) {
		meshlink_errno = EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	free(mesh->external_address_url);
	mesh->external_address_url = url ? xstrdup(url) : NULL;
	pthread_mutex_unlock(&mesh->mutex);
}

void meshlink_set_scheduling_granularity(struct meshlink_handle *mesh, long granularity) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_scheduling_granularity(%ld)", granularity);

	if(!mesh || granularity < 0) {
		meshlink_errno = EINVAL;
		return;
	}

	utcp_set_clock_granularity(granularity);
}

void meshlink_set_storage_policy(struct meshlink_handle *mesh, meshlink_storage_policy_t policy) {
	logger(mesh, MESHLINK_DEBUG, "meshlink_set_storage_policy(%d)", policy);

	if(!mesh) {
		meshlink_errno = EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->storage_policy = policy;
	pthread_mutex_unlock(&mesh->mutex);
}

void handle_network_change(meshlink_handle_t *mesh, bool online) {
	(void)online;

	if(!mesh->connections || !mesh->loop.running) {
		return;
	}

	retry(mesh);
	signal_trigger(&mesh->loop, &mesh->datafromapp);
}

void call_error_cb(meshlink_handle_t *mesh, meshlink_errno_t cb_errno) {
	// We should only call the callback function if we are in the background thread.
	if(!mesh->error_cb) {
		return;
	}

	if(!mesh->threadstarted) {
		return;
	}

	if(mesh->thread == pthread_self()) {
		mesh->error_cb(mesh, cb_errno);
	}
}

static void __attribute__((constructor)) meshlink_init(void) {
	crypto_init();
	utcp_set_clock_granularity(10000);
}

static void __attribute__((destructor)) meshlink_exit(void) {
	crypto_exit();
}
