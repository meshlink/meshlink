/*
    meshlink.c -- Implementation of the MeshLink API.
    Copyright (C) 2014-2018 Guus Sliepen <guus@meshlink.io>

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

#include "crypto.h"
#include "ecdsagen.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "netutl.h"
#include "node.h"
#include "packmsg.h"
#include "prf.h"
#include "protocol.h"
#include "route.h"
#include "sockaddr.h"
#include "utils.h"
#include "xalloc.h"
#include "ed25519/sha512.h"
#include "discovery.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

__thread meshlink_errno_t meshlink_errno;
meshlink_log_cb_t global_log_cb;
meshlink_log_level_t global_log_level;

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

// Find out what local address a socket would use if we connect to the given address.
// We do this using connect() on a UDP socket, so the kernel has to resolve the address
// of both endpoints, but this will actually not send any UDP packet.
static bool getlocaladdr(char *destaddr, struct sockaddr *sn, socklen_t *sl) {
	struct addrinfo *rai = NULL;
	const struct addrinfo hint = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = IPPROTO_UDP,
	};

	if(getaddrinfo(destaddr, "80", &hint, &rai) || !rai) {
		return false;
	}

	int sock = socket(rai->ai_family, rai->ai_socktype, rai->ai_protocol);

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

	if(getsockname(sock, sn, sl)) {
		closesocket(sock);
		return false;
	}

	closesocket(sock);
	return true;
}

static bool getlocaladdrname(char *destaddr, char *host, socklen_t hostlen) {
	struct sockaddr_storage sn;
	socklen_t sl = sizeof(sn);

	if(!getlocaladdr(destaddr, (struct sockaddr *)&sn, &sl)) {
		return false;
	}

	if(getnameinfo((struct sockaddr *)&sn, sl, host, hostlen, NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV)) {
		return false;
	}

	return true;
}

char *meshlink_get_external_address(meshlink_handle_t *mesh) {
	return meshlink_get_external_address_for_family(mesh, AF_UNSPEC);
}

char *meshlink_get_external_address_for_family(meshlink_handle_t *mesh, int family) {
	char *hostname = NULL;

	logger(mesh, MESHLINK_DEBUG, "Trying to discover externally visible hostname...\n");
	struct addrinfo *ai = str2addrinfo("meshlink.io", "80", SOCK_STREAM);
	static const char request[] = "GET http://www.meshlink.io/host.cgi HTTP/1.0\r\n\r\n";
	char line[256];

	for(struct addrinfo *aip = ai; aip; aip = aip->ai_next) {
		if(family != AF_UNSPEC && aip->ai_family != family) {
			continue;
		}

		int s = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);

		if(s >= 0) {
			set_timeout(s, 5000);

			if(connect(s, aip->ai_addr, aip->ai_addrlen)) {
				closesocket(s);
				s = -1;
			}
		}

		if(s >= 0) {
			send(s, request, sizeof(request) - 1, 0);
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

char *meshlink_get_local_address_for_family(meshlink_handle_t *mesh, int family) {
	(void)mesh;

	// Determine address of the local interface used for outgoing connections.
	char localaddr[NI_MAXHOST];
	bool success = false;

	if(family == AF_INET) {
		success = getlocaladdrname("93.184.216.34", localaddr, sizeof(localaddr));
	} else if(family == AF_INET6) {
		success = getlocaladdrname("2606:2800:220:1:248:1893:25c8:1946", localaddr, sizeof(localaddr));
	}

	if(!success) {
		meshlink_errno = MESHLINK_ENETWORK;
		return NULL;
	}

	return xstrdup(localaddr);
}

void remove_duplicate_hostnames(char *host[], char *port[], int n) {
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

		if(found) {
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
	char *hostname[4] = {NULL};
	char *port[4] = {NULL};
	char *hostport = NULL;

	if(!(flags & (MESHLINK_INVITE_LOCAL | MESHLINK_INVITE_PUBLIC))) {
		flags |= MESHLINK_INVITE_LOCAL | MESHLINK_INVITE_PUBLIC;
	}

	if(!(flags & (MESHLINK_INVITE_IPV4 | MESHLINK_INVITE_IPV6))) {
		flags |= MESHLINK_INVITE_IPV4 | MESHLINK_INVITE_IPV6;
	}

	// Add local addresses if requested
	if(flags & MESHLINK_INVITE_LOCAL) {
		if(flags & MESHLINK_INVITE_IPV4) {
			hostname[0] = meshlink_get_local_address_for_family(mesh, AF_INET);
		}

		if(flags & MESHLINK_INVITE_IPV6) {
			hostname[1] = meshlink_get_local_address_for_family(mesh, AF_INET6);
		}
	}

	// Add public/canonical addresses if requested
	if(flags & MESHLINK_INVITE_PUBLIC) {
		// Try the CanonicalAddress first
		get_canonical_address(mesh->self, &hostname[2], &port[2]);

		if(!hostname[2]) {
			if(flags & MESHLINK_INVITE_IPV4) {
				hostname[2] = meshlink_get_external_address_for_family(mesh, AF_INET);
			}

			if(flags & MESHLINK_INVITE_IPV6) {
				hostname[3] = meshlink_get_external_address_for_family(mesh, AF_INET6);
			}
		}
	}

	for(int i = 0; i < 4; i++) {
		// Ensure we always have a port number
		if(hostname[i] && !port[i]) {
			port[i] = xstrdup(mesh->myport);
		}
	}

	remove_duplicate_hostnames(hostname, port, 4);

	if(!(flags & MESHLINK_INVITE_NUMERIC)) {
		for(int i = 0; i < 4; i++) {
			if(!hostname[i]) {
				continue;
			}

			// Convert what we have to a sockaddr
			struct addrinfo *ai_in, *ai_out;
			struct addrinfo hint = {
				.ai_family = AF_UNSPEC,
				.ai_flags = AI_NUMERICSERV,
				.ai_socktype = SOCK_STREAM,
			};
			int err = getaddrinfo(hostname[i], port[i], &hint, &ai_in);

			if(err || !ai_in) {
				continue;
			}

			// Convert it to a hostname
			char resolved_host[NI_MAXHOST];
			char resolved_port[NI_MAXSERV];
			err = getnameinfo(ai_in->ai_addr, ai_in->ai_addrlen, resolved_host, sizeof resolved_host, resolved_port, sizeof resolved_port, NI_NUMERICSERV);

			if(err) {
				freeaddrinfo(ai_in);
				continue;
			}

			// Convert the hostname back to a sockaddr
			hint.ai_family = ai_in->ai_family;
			err = getaddrinfo(resolved_host, resolved_port, &hint, &ai_out);

			if(err || !ai_out) {
				freeaddrinfo(ai_in);
				continue;
			}

			// Check if it's still the same sockaddr
			if(ai_in->ai_addrlen != ai_out->ai_addrlen || memcmp(ai_in->ai_addr, ai_out->ai_addr, ai_in->ai_addrlen)) {
				freeaddrinfo(ai_in);
				freeaddrinfo(ai_out);
				continue;
			}

			// Yes: replace the hostname with the resolved one
			free(hostname[i]);
			hostname[i] = xstrdup(resolved_host);

			freeaddrinfo(ai_in);
			freeaddrinfo(ai_out);
		}
	}

	// Remove duplicates again, since IPv4 and IPv6 addresses might map to the same hostname
	remove_duplicate_hostnames(hostname, port, 4);

	// Concatenate all unique address to the hostport string
	for(int i = 0; i < 4; i++) {
		if(!hostname[i]) {
			continue;
		}

		// Ensure we have the same addresses in our own host config file.
		char *tmphostport;
		xasprintf(&tmphostport, "%s %s", hostname[i], port[i]);
		/// TODO: FIX
		//config_add_string(&mesh->config, "Address", tmphostport);
		free(tmphostport);

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

static bool try_bind(int port) {
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

	while(ai) {
		int fd = socket(ai->ai_family, SOCK_STREAM, IPPROTO_TCP);

		if(!fd) {
			freeaddrinfo(ai);
			return false;
		}

		int result = bind(fd, ai->ai_addr, ai->ai_addrlen);
		closesocket(fd);

		if(result) {
			freeaddrinfo(ai);
			return false;
		}

		ai = ai->ai_next;
	}

	freeaddrinfo(ai);
	return true;
}

int check_port(meshlink_handle_t *mesh) {
	for(int i = 0; i < 1000; i++) {
		int port = 0x1000 + (rand() & 0x7fff);

		if(try_bind(port)) {
			free(mesh->myport);
			xasprintf(&mesh->myport, "%d", port);
			return port;
		}
	}

	meshlink_errno = MESHLINK_ENETWORK;
	logger(mesh, MESHLINK_DEBUG, "Could not find any available network port.\n");
	return 0;
}

static bool finalize_join(meshlink_handle_t *mesh, const void *buf, uint16_t len) {
	packmsg_input_t in = {buf, len};
	uint32_t version = packmsg_get_uint32(&in);

	if(version != MESHLINK_INVITATION_VERSION) {
		logger(mesh, MESHLINK_ERROR, "Invalid invitation version!\n");
		return false;
	}

	char *name = packmsg_get_str_dup(&in);
	int32_t devclass = packmsg_get_int32(&in);
	uint32_t count = packmsg_get_array(&in);

	if(!name) {
		logger(mesh, MESHLINK_DEBUG, "No Name found in invitation!\n");
		return false;
	}

	if(!check_id(name)) {
		logger(mesh, MESHLINK_DEBUG, "Invalid Name found in invitation: %s!\n", name);
		return false;
	}

	if(!count) {
		logger(mesh, MESHLINK_ERROR, "Incomplete invitation file!\n");
		return false;
	}

	// Initialize configuration directory
	if(!config_init(mesh)) {
		return false;
	}

	// Write main config file
	uint8_t outbuf[4096];
	packmsg_output_t out = {outbuf, sizeof(outbuf)};
	packmsg_add_uint32(&out, MESHLINK_CONFIG_VERSION);
	packmsg_add_str(&out, name);
	packmsg_add_bin(&out, ecdsa_get_private_key(mesh->private_key), 96);
	packmsg_add_uint16(&out, atoi(mesh->myport));

	config_t config = {outbuf, packmsg_output_size(&out, outbuf)};

	if(!main_config_write(mesh, &config)) {
		return false;
	}

	// Write our own host config file
	out.ptr = outbuf;
	out.len = sizeof(outbuf);
	packmsg_add_uint32(&out, MESHLINK_CONFIG_VERSION);
	packmsg_add_str(&out, name);
	packmsg_add_int32(&out, devclass);
	packmsg_add_bool(&out, false);
	packmsg_add_bin(&out, ecdsa_get_public_key(mesh->private_key), 32);
	packmsg_add_str(&out, ""); // TODO: copy existing canonical address, in case it was added before meshlink_join().
	packmsg_add_array(&out, 0);

	config.len = packmsg_output_size(&out, outbuf);

	if(!config_write(mesh, name, &config)) {
		return false;
	}

	// Write host config files
	while(count--) {
		const void *data;
		uint32_t len = packmsg_get_bin_raw(&in, &data);

		if(!len) {
			logger(mesh, MESHLINK_ERROR, "Incomplete invitation file!\n");
			return false;
		}

		config_t config = {data, len};
		node_t *n = new_node();

		if(!node_read_from_config(mesh, n, &config)) {
			free_node(n);
			logger(mesh, MESHLINK_ERROR, "Invalid host config file in invitation file!\n");
			meshlink_errno = MESHLINK_EPEER;
			return false;
		}

		if(!strcmp(n->name, name)) {
			logger(mesh, MESHLINK_DEBUG, "Secondary chunk would overwrite our own host config file.\n");
			free_node(n);
			meshlink_errno = MESHLINK_EPEER;
			return false;
		}

		node_add(mesh, n);

		if(!config_write(mesh, n->name, &config)) {
			return false;
		}
	}

	sptps_send_record(&(mesh->sptps), 1, ecdsa_get_public_key(mesh->private_key), 32);

	free(mesh->name);
	free(mesh->self->name);
	mesh->name = xstrdup(name);
	mesh->self->name = xstrdup(name);

	logger(mesh, MESHLINK_DEBUG, "Configuration stored in: %s\n", mesh->confbase);

	return true;
}

static bool invitation_send(void *handle, uint8_t type, const void *data, size_t len) {
	(void)type;
	meshlink_handle_t *mesh = handle;
	const char *ptr = data;

	while(len) {
		int result = send(mesh->sock, ptr, len, 0);

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
	meshlink_handle_t *mesh = handle;

	switch(type) {
	case SPTPS_HANDSHAKE:
		return sptps_send_record(&(mesh->sptps), 0, mesh->cookie, sizeof(mesh)->cookie);

	case 0:
		return finalize_join(mesh, msg, len);

	case 1:
		logger(mesh, MESHLINK_DEBUG, "Invitation succesfully accepted.\n");
		shutdown(mesh->sock, SHUT_RDWR);
		mesh->success = true;
		break;

	default:
		return false;
	}

	return true;
}

static bool recvline(meshlink_handle_t *mesh, size_t len) {
	char *newline = NULL;

	if(!mesh->sock) {
		abort();
	}

	while(!(newline = memchr(mesh->buffer, '\n', mesh->blen))) {
		int result = recv(mesh->sock, mesh->buffer + mesh->blen, sizeof(mesh)->buffer - mesh->blen, 0);

		if(result == -1 && errno == EINTR) {
			continue;
		} else if(result <= 0) {
			return false;
		}

		mesh->blen += result;
	}

	if((size_t)(newline - mesh->buffer) >= len) {
		return false;
	}

	len = newline - mesh->buffer;

	memcpy(mesh->line, mesh->buffer, len);
	mesh->line[len] = 0;
	memmove(mesh->buffer, newline + 1, mesh->blen - len - 1);
	mesh->blen -= len + 1;

	return true;
}
static bool sendline(int fd, char *format, ...) {
	static char buffer[4096];
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
		logger(mesh, MESHLINK_DEBUG, "Error during key generation!\n");
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	logger(mesh, MESHLINK_DEBUG, "Done.\n");

	return true;
}

static struct timeval idle(event_loop_t *loop, void *data) {
	(void)loop;
	meshlink_handle_t *mesh = data;
	struct timeval t, tmin = {3600, 0};

	for splay_each(node_t, n, mesh->nodes) {
		if(!n->utcp) {
			continue;
		}

		t = utcp_timeout(n->utcp);

		if(timercmp(&t, &tmin, <)) {
			tmin = t;
		}
	}

	return tmin;
}

// Get our local address(es) by simulating connecting to an Internet host.
static void add_local_addresses(meshlink_handle_t *mesh) {
	struct sockaddr_storage sn;
	socklen_t sl = sizeof(sn);

	// IPv4 example.org

	if(getlocaladdr("93.184.216.34", (struct sockaddr *)&sn, &sl)) {
		((struct sockaddr_in *)&sn)->sin_port = ntohs(atoi(mesh->myport));
		meshlink_hint_address(mesh, (meshlink_node_t *)mesh->self, (struct sockaddr *)&sn);
	}

	// IPv6 example.org

	sl = sizeof(sn);

	if(getlocaladdr("2606:2800:220:1:248:1893:25c8:1946", (struct sockaddr *)&sn, &sl)) {
		((struct sockaddr_in6 *)&sn)->sin6_port = ntohs(atoi(mesh->myport));
		meshlink_hint_address(mesh, (meshlink_node_t *)mesh->self, (struct sockaddr *)&sn);
	}
}

#if 0
static bool meshlink_write_config(meshlink_handle_t *mesh) {
	uint8_t buf[1024];
	packmsg_output_t out = {buf, sizeof buf};
	packmsg_add_str(&out, mesh->name);
	packmsg_add_uint32(&out, mesh->devclass);
	packmsg_add_uint16(&out, mesh->port);
	packmsg_add_bin(&out, ecdsa, sizeof(ecdsa));
	uint32_t len = packmsg_output_size(&out, buf);

	if(!len) {
		logger(mesh, MESHLINK_DEBUG, "Could not create configuration data\n",);
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}
}
#endif

static bool meshlink_setup(meshlink_handle_t *mesh) {
	if(!config_init(mesh)) {
		logger(mesh, MESHLINK_ERROR, "Could not set up configuration in %s: %s\n", mesh->confbase, strerror(errno));
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

	// Write the main config file
	uint8_t buf[4096];
	packmsg_output_t out = {buf, sizeof(buf)};

	packmsg_add_uint32(&out, MESHLINK_CONFIG_VERSION);
	packmsg_add_str(&out, mesh->name);
	packmsg_add_bin(&out, ecdsa_get_private_key(mesh->private_key), 96);
	packmsg_add_bin(&out, ecdsa_get_private_key(mesh->invitation_key), 96);
	packmsg_add_uint16(&out, atoi(mesh->myport));

	config_t config = {buf, packmsg_output_size(&out, buf)};

	if(!main_config_write(mesh, &config)) {
		return false;
	}

	// Write our own host config file
	out.ptr = buf;
	out.len = sizeof(buf);
	packmsg_add_uint32(&out, MESHLINK_CONFIG_VERSION);
	packmsg_add_str(&out, mesh->name);
	packmsg_add_int32(&out, mesh->devclass);
	packmsg_add_bool(&out, false);
	packmsg_add_bin(&out, ecdsa_get_public_key(mesh->private_key), 32);
	packmsg_add_str(&out, ""); // TODO: copy existing canonical address, in case it was added before meshlink_join().
	packmsg_add_array(&out, 0);

	config.len = packmsg_output_size(&out, buf);

	if(!config_write(mesh, mesh->name, &config)) {
		return false;
	}

	return true;
}

static bool meshlink_read_config(meshlink_handle_t *mesh) {
	// Open the configuration file and lock it
	if(!main_config_lock(mesh)) {
		logger(NULL, MESHLINK_ERROR, "Cannot lock main config file\n");
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	config_t config;

	if(!main_config_read(mesh, &config)) {
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

#if 0

	// TODO: check this?
	if(mesh->name && strcmp(mesh->name, name)) {
		logger(NULL, MESHLINK_ERROR, "Configuration is for a different name (%s)!", name);
		meshlink_errno = MESHLINK_ESTORAGE;
		free(name);
		config_free(&config);
		return false;
	}

#endif

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

	if(!node_read_public_key(mesh, mesh->self)) {
		logger(NULL, MESHLINK_ERROR, "Could not read our host configuration file!");
		free_node(mesh->self);
		mesh->self = NULL;
		return false;
	}

	return true;
}


static meshlink_handle_t *meshlink_open_internal(const char *confbase, const char *name, const char *appname, dev_class_t devclass, const void *key, size_t keylen) {
	// Validate arguments provided by the application
	bool usingname = false;

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

	if(name) {
		if(!check_id(name)) {
			logger(NULL, MESHLINK_ERROR, "Invalid name given!\n");
			meshlink_errno = MESHLINK_EINVAL;
			return NULL;
		}

		usingname = true;
	}

	if((int)devclass < 0 || devclass > _DEV_CLASS_MAX) {
		logger(NULL, MESHLINK_ERROR, "Invalid devclass given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	meshlink_handle_t *mesh = xzalloc(sizeof(meshlink_handle_t));

	if(confbase) {
		mesh->confbase = xstrdup(confbase);
	}

	mesh->appname = xstrdup(appname);
	mesh->devclass = devclass;
	mesh->discovery = true;
	mesh->invitation_timeout = 604800; // 1 week

	if(usingname) {
		mesh->name = xstrdup(name);
	}

	// Hash the key
	if(key) {
		mesh->config_key = xmalloc(CHACHA_POLY1305_KEYLEN);

		if(!prf(key, keylen, "MeshLink configuration key", 26, mesh->config_key, CHACHA_POLY1305_KEYLEN)) {
			logger(NULL, MESHLINK_ERROR, "Error creating configuration key!\n");
			meshlink_errno = MESHLINK_EINTERNAL;
			return NULL;
		}
	}

	// initialize mutex
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&(mesh->mesh_mutex), &attr);

	mesh->threadstarted = false;
	event_loop_init(&mesh->loop);
	mesh->loop.data = mesh;

	meshlink_queue_init(&mesh->outpacketqueue);

	// If no configuration exists yet, create it.

	if(!main_config_exists(mesh)) {
		if(!meshlink_setup(mesh)) {
			logger(NULL, MESHLINK_ERROR, "Cannot create initial configuration\n");
			meshlink_close(mesh);
			return NULL;
		}
	} else {
		if(!meshlink_read_config(mesh)) {
			logger(NULL, MESHLINK_ERROR, "Cannot read main configuration\n");
			meshlink_close(mesh);
			return NULL;
		}
	}

#ifdef HAVE_MINGW
	struct WSAData wsa_state;
	WSAStartup(MAKEWORD(2, 2), &wsa_state);
#endif

	// Setup up everything
	// TODO: we should not open listening sockets yet

	if(!setup_network(mesh)) {
		meshlink_close(mesh);
		meshlink_errno = MESHLINK_ENETWORK;
		return NULL;
	}

	add_local_addresses(mesh);
	node_write_config(mesh, mesh->self);

	idle_set(&mesh->loop, idle, mesh);

	logger(NULL, MESHLINK_DEBUG, "meshlink_open returning\n");
	return mesh;
}

meshlink_handle_t *meshlink_open(const char *confbase, const char *name, const char *appname, dev_class_t devclass) {
	if(!confbase || !*confbase) {
		logger(NULL, MESHLINK_ERROR, "No confbase given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	return meshlink_open_internal(confbase, name, appname, devclass, NULL, 0);
}

meshlink_handle_t *meshlink_open_encrypted(const char *confbase, const char *name, const char *appname, dev_class_t devclass, const void *key, size_t keylen) {
	if(!confbase || !*confbase) {
		logger(NULL, MESHLINK_ERROR, "No confbase given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(!key || !keylen) {
		logger(NULL, MESHLINK_ERROR, "No key given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	return meshlink_open_internal(confbase, name, appname, devclass, key, keylen);
}

meshlink_handle_t *meshlink_open_ephemeral(const char *name, const char *appname, dev_class_t devclass) {
	if(!name || !*name) {
		logger(NULL, MESHLINK_ERROR, "No name given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	return meshlink_open_internal(NULL, name, appname, devclass, NULL, 0);
}


static void *meshlink_main_loop(void *arg) {
	meshlink_handle_t *mesh = arg;

	pthread_mutex_lock(&(mesh->mesh_mutex));

	logger(mesh, MESHLINK_DEBUG, "Starting main_loop...\n");
	main_loop(mesh);
	logger(mesh, MESHLINK_DEBUG, "main_loop returned.\n");

	pthread_mutex_unlock(&(mesh->mesh_mutex));
	return NULL;
}

bool meshlink_start(meshlink_handle_t *mesh) {
	assert(mesh->self);
	assert(mesh->private_key);

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	logger(mesh, MESHLINK_DEBUG, "meshlink_start called\n");

	pthread_mutex_lock(&(mesh->mesh_mutex));

	if(mesh->threadstarted) {
		logger(mesh, MESHLINK_DEBUG, "thread was already running\n");
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return true;
	}

	if(mesh->listen_socket[0].tcp.fd < 0) {
		logger(mesh, MESHLINK_ERROR, "Listening socket not open\n");
		meshlink_errno = MESHLINK_ENETWORK;
		return false;
	}

	mesh->thedatalen = 0;

	// TODO: open listening sockets first

	//Check that a valid name is set
	if(!mesh->name) {
		logger(mesh, MESHLINK_DEBUG, "No name given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	init_outgoings(mesh);

	// Start the main thread

	event_loop_start(&mesh->loop);

	if(pthread_create(&mesh->thread, NULL, meshlink_main_loop, mesh) != 0) {
		logger(mesh, MESHLINK_DEBUG, "Could not start thread: %s\n", strerror(errno));
		memset(&mesh->thread, 0, sizeof(mesh)->thread);
		meshlink_errno = MESHLINK_EINTERNAL;
		event_loop_stop(&mesh->loop);
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	mesh->threadstarted = true;

#if HAVE_CATTA

	if(mesh->discovery) {
		discovery_start(mesh);
	}

#endif

	assert(mesh->self->ecdsa);
	assert(!memcmp((uint8_t *)mesh->self->ecdsa + 64, (uint8_t *)mesh->private_key + 64, 32));


	pthread_mutex_unlock(&(mesh->mesh_mutex));
	return true;
}

void meshlink_stop(meshlink_handle_t *mesh) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));
	logger(mesh, MESHLINK_DEBUG, "meshlink_stop called\n");

#if HAVE_CATTA

	// Stop discovery
	if(mesh->discovery) {
		discovery_stop(mesh);
	}

#endif

	// Shut down the main thread
	event_loop_stop(&mesh->loop);

	// Send ourselves a UDP packet to kick the event loop
	for(int i = 0; i < mesh->listen_sockets; i++) {
		sockaddr_t sa;
		socklen_t salen = sizeof(sa.sa);

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
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		pthread_join(mesh->thread, NULL);
		pthread_mutex_lock(&(mesh->mesh_mutex));

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

	exit_outgoings(mesh);

	pthread_mutex_unlock(&(mesh->mesh_mutex));
}

void meshlink_close(meshlink_handle_t *mesh) {
	if(!mesh || !mesh->confbase) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	// stop can be called even if mesh has not been started
	meshlink_stop(mesh);

	// lock is not released after this
	pthread_mutex_lock(&(mesh->mesh_mutex));

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

	free(mesh->name);
	free(mesh->appname);
	free(mesh->confbase);
	pthread_mutex_destroy(&(mesh->mesh_mutex));

	main_config_unlock(mesh);

	memset(mesh, 0, sizeof(*mesh));

	free(mesh);
}

bool meshlink_destroy(const char *confbase) {
	if(!confbase) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	return config_destroy(confbase);
}

void meshlink_set_receive_cb(meshlink_handle_t *mesh, meshlink_receive_cb_t cb) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));
	mesh->receive_cb = cb;
	pthread_mutex_unlock(&(mesh->mesh_mutex));
}

void meshlink_set_node_status_cb(meshlink_handle_t *mesh, meshlink_node_status_cb_t cb) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));
	mesh->node_status_cb = cb;
	pthread_mutex_unlock(&(mesh->mesh_mutex));
}

void meshlink_set_node_duplicate_cb(meshlink_handle_t *mesh, meshlink_node_duplicate_cb_t cb) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));
	mesh->node_duplicate_cb = cb;
	pthread_mutex_unlock(&(mesh->mesh_mutex));
}

void meshlink_set_log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, meshlink_log_cb_t cb) {
	if(mesh) {
		pthread_mutex_lock(&(mesh->mesh_mutex));
		mesh->log_cb = cb;
		mesh->log_level = cb ? level : 0;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
	} else {
		global_log_cb = cb;
		global_log_level = cb ? level : 0;
	}
}

bool meshlink_send(meshlink_handle_t *mesh, meshlink_node_t *destination, const void *data, size_t len) {
	meshlink_packethdr_t *hdr;

	// Validate arguments
	if(!mesh || !destination || len >= MAXSIZE - sizeof(*hdr)) {
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

	packet->probe = false;
	packet->tcp = false;
	packet->len = len + sizeof(*hdr);

	hdr = (meshlink_packethdr_t *)packet->data;
	memset(hdr, 0, sizeof(*hdr));
	// leave the last byte as 0 to make sure strings are always
	// null-terminated if they are longer than the buffer
	strncpy((char *)hdr->destination, destination->name, (sizeof(hdr)->destination) - 1);
	strncpy((char *)hdr->source, mesh->self->name, (sizeof(hdr)->source) - 1);

	memcpy(packet->data + sizeof(*hdr), data, len);

	// Queue it
	if(!meshlink_queue_push(&mesh->outpacketqueue, packet)) {
		free(packet);
		meshlink_errno = MESHLINK_ENOMEM;
		return false;
	}

	// Notify event loop
	signal_trigger(&(mesh->loop), &(mesh->datafromapp));

	return true;
}

void meshlink_send_from_queue(event_loop_t *loop, meshlink_handle_t *mesh) {
	(void)loop;
	vpn_packet_t *packet = meshlink_queue_pop(&mesh->outpacketqueue);

	if(!packet) {
		return;
	}

	mesh->self->in_packets++;
	mesh->self->in_bytes += packet->len;
	route(mesh, mesh->self, packet);
}

ssize_t meshlink_get_pmtu(meshlink_handle_t *mesh, meshlink_node_t *destination) {
	if(!mesh || !destination) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));

	node_t *n = (node_t *)destination;

	if(!n->status.reachable) {
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return 0;

	} else if(n->mtuprobes > 30 && n->minmtu) {
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return n->minmtu;
	} else {
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return MTU;
	}
}

char *meshlink_get_fingerprint(meshlink_handle_t *mesh, meshlink_node_t *node) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));

	node_t *n = (node_t *)node;

	if(!node_read_public_key(mesh, n) || !n->ecdsa) {
		meshlink_errno = MESHLINK_EINTERNAL;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	char *fingerprint = ecdsa_get_base64_public_key(n->ecdsa);

	if(!fingerprint) {
		meshlink_errno = MESHLINK_EINTERNAL;
	}

	pthread_mutex_unlock(&(mesh->mesh_mutex));
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

	meshlink_node_t *node = NULL;

	pthread_mutex_lock(&(mesh->mesh_mutex));
	node = (meshlink_node_t *)lookup_node(mesh, (char *)name); // TODO: make lookup_node() use const
	pthread_mutex_unlock(&(mesh->mesh_mutex));
	return node;
}

meshlink_node_t **meshlink_get_all_nodes(meshlink_handle_t *mesh, meshlink_node_t **nodes, size_t *nmemb) {
	if(!mesh || !nmemb || (*nmemb && !nodes)) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	meshlink_node_t **result;

	//lock mesh->nodes
	pthread_mutex_lock(&(mesh->mesh_mutex));

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

	pthread_mutex_unlock(&(mesh->mesh_mutex));

	return result;
}

bool meshlink_sign(meshlink_handle_t *mesh, const void *data, size_t len, void *signature, size_t *siglen) {
	if(!mesh || !data || !len || !signature || !siglen) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(*siglen < MESHLINK_SIGLEN) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));

	if(!ecdsa_sign(mesh->private_key, data, len, signature)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	*siglen = MESHLINK_SIGLEN;
	pthread_mutex_unlock(&(mesh->mesh_mutex));
	return true;
}

bool meshlink_verify(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len, const void *signature, size_t siglen) {
	if(!mesh || !data || !len || !signature) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(siglen != MESHLINK_SIGLEN) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));

	bool rval = false;

	struct node_t *n = (struct node_t *)source;

	if(!node_read_public_key(mesh, n)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		rval = false;
	} else {
		rval = ecdsa_verify(((struct node_t *)source)->ecdsa, data, len, signature);
	}

	pthread_mutex_unlock(&(mesh->mesh_mutex));
	return rval;
}

static bool refresh_invitation_key(meshlink_handle_t *mesh) {
	pthread_mutex_lock(&(mesh->mesh_mutex));

	size_t count = invitation_purge_old(mesh, time(NULL) - mesh->invitation_timeout);

	if(!count) {
		// TODO: Update invitation key if necessary?
	}

	pthread_mutex_unlock(&(mesh->mesh_mutex));

	return mesh->invitation_key;
}

bool meshlink_set_canonical_address(meshlink_handle_t *mesh, meshlink_node_t *node, const char *address, const char *port) {
	if(!mesh || !node || !address) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(!is_valid_hostname(address)) {
		logger(mesh, MESHLINK_DEBUG, "Invalid character in address: %s\n", address);
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(port && !is_valid_port(port)) {
		logger(mesh, MESHLINK_DEBUG, "Invalid character in port: %s\n", address);
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	char *canonical_address;

	if(port) {
		xasprintf(&canonical_address, "%s %s", address, port);
	} else {
		canonical_address = xstrdup(address);
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));

	node_t *n = (node_t *)node;
	free(n->canonical_address);
	n->canonical_address = canonical_address;
	n->status.dirty = true;

	pthread_mutex_unlock(&(mesh->mesh_mutex));

	return true;
}

bool meshlink_add_address(meshlink_handle_t *mesh, const char *address) {
	return meshlink_set_canonical_address(mesh, (meshlink_node_t *)mesh->self, address, NULL);
}

bool meshlink_add_external_address(meshlink_handle_t *mesh) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	char *address = meshlink_get_external_address(mesh);

	if(!address) {
		return false;
	}

	bool rval = meshlink_add_address(mesh, address);
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

	return atoi(mesh->myport);
}

bool meshlink_set_port(meshlink_handle_t *mesh, int port) {
	if(!mesh || port < 0 || port >= 65536 || mesh->threadstarted) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(mesh->myport && port == atoi(mesh->myport)) {
		return true;
	}

	if(!try_bind(port)) {
		meshlink_errno = MESHLINK_ENETWORK;
		return false;
	}

	bool rval = false;

	pthread_mutex_lock(&(mesh->mesh_mutex));

	if(mesh->threadstarted) {
		meshlink_errno = MESHLINK_EINVAL;
		goto done;
	}

	close_network_connections(mesh);

	// TODO: write meshlink.conf again

	if(!setup_network(mesh)) {
		meshlink_errno = MESHLINK_ENETWORK;
	} else {
		rval = true;
	}

done:
	pthread_mutex_unlock(&(mesh->mesh_mutex));

	return rval;
}

void meshlink_set_invitation_timeout(meshlink_handle_t *mesh, int timeout) {
	mesh->invitation_timeout = timeout;
}

char *meshlink_invite_ex(meshlink_handle_t *mesh, const char *name, uint32_t flags) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));

	// Check validity of the new node's name
	if(!check_id(name)) {
		logger(mesh, MESHLINK_DEBUG, "Invalid name for node.\n");
		meshlink_errno = MESHLINK_EINVAL;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return NULL;
	}

	// Ensure no host configuration file with that name exists
	if(config_exists(mesh, name)) {
		logger(mesh, MESHLINK_DEBUG, "A host config file for %s already exists!\n", name);
		meshlink_errno = MESHLINK_EEXIST;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return NULL;
	}

	// Ensure no other nodes know about this name
	if(meshlink_get_node(mesh, name)) {
		logger(mesh, MESHLINK_DEBUG, "A node with name %s is already known!\n", name);
		meshlink_errno = MESHLINK_EEXIST;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return NULL;
	}

	// Get the local address
	char *address = get_my_hostname(mesh, flags);

	if(!address) {
		logger(mesh, MESHLINK_DEBUG, "No Address known for ourselves!\n");
		meshlink_errno = MESHLINK_ERESOLV;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return NULL;
	}

	if(!refresh_invitation_key(mesh)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return NULL;
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
	packmsg_add_int32(&inv, DEV_CLASS_UNKNOWN); /* TODO: allow this to be set by inviter? */

	/* TODO: Add several host config files to bootstrap connections */
	config_t configs[5] = {NULL};
	int count = 0;

	if(config_read(mesh, mesh->self->name, &configs[count])) {
		count++;
	}

	/* Append host config files to the invitation file */
	packmsg_add_array(&inv, count);

	for(int i = 0; i < count; i++) {
		packmsg_add_bin(&inv, configs[i].buf, configs[i].len);
		config_free(&configs[i]);
	}

	config_t config = {outbuf, packmsg_output_size(&inv, outbuf)};

	if(!invitation_write(mesh, cookiehash, &config)) {
		logger(mesh, MESHLINK_DEBUG, "Could not create invitation file %s: %s\n", cookiehash, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return NULL;
	}

	// Create an URL from the local address, key hash and cookie
	char *url;
	xasprintf(&url, "%s/%s%s", address, hash, cookie);
	free(address);

	pthread_mutex_unlock(&(mesh->mesh_mutex));
	return url;
}

char *meshlink_invite(meshlink_handle_t *mesh, const char *name) {
	return meshlink_invite_ex(mesh, name, 0);
}

bool meshlink_join(meshlink_handle_t *mesh, const char *invitation) {
	if(!mesh || !invitation) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));

	//Before doing meshlink_join make sure we are not connected to another mesh
	if(mesh->threadstarted) {
		logger(mesh, MESHLINK_DEBUG, "Already connected to a mesh\n");
		meshlink_errno = MESHLINK_EINVAL;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	//TODO: think of a better name for this variable, or of a different way to tokenize the invitation URL.
	char copy[strlen(invitation) + 1];
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

	if(!b64decode(slash, mesh->hash, 18) || !b64decode(slash + 24, mesh->cookie, 18)) {
		goto invalid;
	}

	// Generate a throw-away key for the invitation.
	ecdsa_t *key = ecdsa_generate();

	if(!key) {
		meshlink_errno = MESHLINK_EINTERNAL;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	char *b64key = ecdsa_get_base64_public_key(key);
	char *comma;
	mesh->sock = -1;

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
		struct addrinfo *ai = str2addrinfo(address, port, SOCK_STREAM);

		if(ai) {
			for(struct addrinfo *aip = ai; aip; aip = aip->ai_next) {
				mesh->sock = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);

				if(mesh->sock == -1) {
					logger(mesh, MESHLINK_DEBUG, "Could not open socket: %s\n", strerror(errno));
					meshlink_errno = MESHLINK_ENETWORK;
					continue;
				}

				set_timeout(mesh->sock, 5000);

				if(connect(mesh->sock, aip->ai_addr, aip->ai_addrlen)) {
					logger(mesh, MESHLINK_DEBUG, "Could not connect to %s port %s: %s\n", address, port, strerror(errno));
					meshlink_errno = MESHLINK_ENETWORK;
					closesocket(mesh->sock);
					mesh->sock = -1;
					continue;
				}
			}

			freeaddrinfo(ai);
		} else {
			meshlink_errno = MESHLINK_ERESOLV;
		}

		if(mesh->sock != -1 || !comma) {
			break;
		}

		address = comma;
	}

	if(mesh->sock == -1) {
		pthread_mutex_unlock(&mesh->mesh_mutex);
		return false;
	}

	logger(mesh, MESHLINK_DEBUG, "Connected to %s port %s...\n", address, port);

	// Tell him we have an invitation, and give him our throw-away key.

	mesh->blen = 0;

	if(!sendline(mesh->sock, "0 ?%s %d.%d %s", b64key, PROT_MAJOR, 1, mesh->appname)) {
		logger(mesh, MESHLINK_DEBUG, "Error sending request to %s port %s: %s\n", address, port, strerror(errno));
		closesocket(mesh->sock);
		meshlink_errno = MESHLINK_ENETWORK;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	free(b64key);

	char hisname[4096] = "";
	int code, hismajor, hisminor = 0;

	if(!recvline(mesh, sizeof(mesh)->line) || sscanf(mesh->line, "%d %s %d.%d", &code, hisname, &hismajor, &hisminor) < 3 || code != 0 || hismajor != PROT_MAJOR || !check_id(hisname) || !recvline(mesh, sizeof(mesh)->line) || !rstrip(mesh->line) || sscanf(mesh->line, "%d ", &code) != 1 || code != ACK || strlen(mesh->line) < 3) {
		logger(mesh, MESHLINK_DEBUG, "Cannot read greeting from peer\n");
		closesocket(mesh->sock);
		meshlink_errno = MESHLINK_ENETWORK;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	// Check if the hash of the key he gave us matches the hash in the URL.
	char *fingerprint = mesh->line + 2;
	char hishash[64];

	if(sha512(fingerprint, strlen(fingerprint), hishash)) {
		logger(mesh, MESHLINK_DEBUG, "Could not create hash\n%s\n", mesh->line + 2);
		meshlink_errno = MESHLINK_EINTERNAL;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	if(memcmp(hishash, mesh->hash, 18)) {
		logger(mesh, MESHLINK_DEBUG, "Peer has an invalid key!\n%s\n", mesh->line + 2);
		meshlink_errno = MESHLINK_EPEER;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;

	}

	ecdsa_t *hiskey = ecdsa_set_base64_public_key(fingerprint);

	if(!hiskey) {
		meshlink_errno = MESHLINK_EINTERNAL;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	// Start an SPTPS session
	if(!sptps_start(&mesh->sptps, mesh, true, false, key, hiskey, meshlink_invitation_label, sizeof(meshlink_invitation_label), invitation_send, invitation_receive)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	// Feed rest of input buffer to SPTPS
	if(!sptps_receive_data(&mesh->sptps, mesh->buffer, mesh->blen)) {
		meshlink_errno = MESHLINK_EPEER;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	int len;

	while((len = recv(mesh->sock, mesh->line, sizeof(mesh)->line, 0))) {
		if(len < 0) {
			if(errno == EINTR) {
				continue;
			}

			logger(mesh, MESHLINK_DEBUG, "Error reading data from %s port %s: %s\n", address, port, strerror(errno));
			meshlink_errno = MESHLINK_ENETWORK;
			pthread_mutex_unlock(&(mesh->mesh_mutex));
			return false;
		}

		if(!sptps_receive_data(&mesh->sptps, mesh->line, len)) {
			meshlink_errno = MESHLINK_EPEER;
			pthread_mutex_unlock(&(mesh->mesh_mutex));
			return false;
		}
	}

	sptps_stop(&mesh->sptps);
	ecdsa_free(hiskey);
	ecdsa_free(key);
	closesocket(mesh->sock);

	if(!mesh->success) {
		logger(mesh, MESHLINK_DEBUG, "Connection closed by peer, invitation cancelled.\n");
		meshlink_errno = MESHLINK_EPEER;
		pthread_mutex_unlock(&(mesh->mesh_mutex));
		return false;
	}

	pthread_mutex_unlock(&(mesh->mesh_mutex));
	return true;

invalid:
	logger(mesh, MESHLINK_DEBUG, "Invalid invitation URL\n");
	meshlink_errno = MESHLINK_EINVAL;
	pthread_mutex_unlock(&(mesh->mesh_mutex));
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

	pthread_mutex_lock(&(mesh->mesh_mutex));

	packmsg_add_int32(&out, mesh->self->devclass);
	packmsg_add_bool(&out, mesh->self->status.blacklisted);
	packmsg_add_bin(&out, ecdsa_get_public_key(mesh->private_key), 32);
	packmsg_add_str(&out, mesh->self->canonical_address ? mesh->self->canonical_address : "");

	uint32_t count = 0;

	for(uint32_t i = 0; i < 5; i++) {
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

	pthread_mutex_unlock(&(mesh->mesh_mutex));

	if(!packmsg_output_ok(&out)) {
		logger(mesh, MESHLINK_DEBUG, "Error creating export data\n");
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
		logger(mesh, MESHLINK_DEBUG, "Error creating export data\n");
		meshlink_errno = MESHLINK_EINTERNAL;
		free(buf2);
		return NULL;
	}

	b64encode_urlsafe(buf2, (char *)buf2, packmsg_output_size(&out2, buf2));

	return (char *)buf2;
}

bool meshlink_import(meshlink_handle_t *mesh, const char *data) {
	if(!mesh || !data) {
		abort();
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	size_t datalen = strlen(data);
	uint8_t *buf = xmalloc(datalen);
	int buflen = b64decode(data, buf, datalen);

	if(!buflen) {
		abort();
		logger(mesh, MESHLINK_DEBUG, "Invalid data\n");
		meshlink_errno = MESHLINK_EPEER;
		return false;
	}

	packmsg_input_t in = {buf, buflen};
	uint32_t count = packmsg_get_array(&in);

	if(!count) {
		abort();
		logger(mesh, MESHLINK_DEBUG, "Invalid data\n");
		meshlink_errno = MESHLINK_EPEER;
		return false;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));

	while(count--) {
		const void *data;
		uint32_t len = packmsg_get_bin_raw(&in, &data);

		if(!len) {
			break;
		}

		packmsg_input_t in2 = {data, len};
		uint32_t version = packmsg_get_uint32(&in2);
		char *name = packmsg_get_str_dup(&in2);

		if(!packmsg_input_ok(&in2) || version != MESHLINK_CONFIG_VERSION || !check_id(name)) {
			free(name);
			packmsg_input_invalidate(&in2);
			break;
		}

		if(!check_id(name)) {
			free(name);
			break;
		}

		node_t *n = lookup_node(mesh, name);

		if(n) {
			free(name);
			logger(mesh, MESHLINK_DEBUG, "Node %s already exists, not importing\n", name);
			continue;
		}

		n = new_node();
		n->name = name;
		n->devclass = packmsg_get_int32(&in2);
		n->status.blacklisted = packmsg_get_bool(&in2);
		const void *key;
		uint32_t keylen = packmsg_get_bin_raw(&in2, &key);

		if(keylen == 32) {
			n->ecdsa = ecdsa_set_public_key(key);
		}

		n->canonical_address = packmsg_get_str_dup(&in2);
		uint32_t count = packmsg_get_array(&in2);

		if(count > 5) {
			count = 5;
		}

		for(uint32_t i = 0; i < count; i++) {
			n->recent[i] = packmsg_get_sockaddr(&in2);
		}

		if(!packmsg_done(&in2) || keylen != 32) {
			abort();
			packmsg_input_invalidate(&in2);
			free_node(n);
		} else {
			config_t config = {data, len};
			config_write(mesh, n->name, &config);
			node_add(mesh, n);
		}
	}

	pthread_mutex_unlock(&(mesh->mesh_mutex));

	if(!packmsg_done(&in)) {
		abort();
		logger(mesh, MESHLINK_ERROR, "Invalid data\n");
		meshlink_errno = MESHLINK_EPEER;
		return false;
	}

	return true;
}

void meshlink_blacklist(meshlink_handle_t *mesh, meshlink_node_t *node) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));

	node_t *n;
	n = (node_t *)node;
	n->status.blacklisted = true;
	n->status.dirty = true;
	logger(mesh, MESHLINK_DEBUG, "Blacklisted %s.\n", node->name);

	//Immediately terminate any connections we have with the blacklisted node
	for list_each(connection_t, c, mesh->connections) {
		if(c->node == n) {
			terminate_connection(mesh, c, c->status.active);
		}
	}

	pthread_mutex_unlock(&(mesh->mesh_mutex));
}

void meshlink_whitelist(meshlink_handle_t *mesh, meshlink_node_t *node) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));

	node_t *n = (node_t *)node;
	n->status.blacklisted = false;
	n->status.dirty = true;

	pthread_mutex_unlock(&(mesh->mesh_mutex));
	return;
}

void meshlink_set_default_blacklist(meshlink_handle_t *mesh, bool blacklist) {
	mesh->default_blacklist = blacklist;
}

/* Hint that a hostname may be found at an address
 * See header file for detailed comment.
 */
void meshlink_hint_address(meshlink_handle_t *mesh, meshlink_node_t *node, const struct sockaddr *addr) {
	if(!mesh || !node || !addr) {
		meshlink_errno = EINVAL;
		return;
	}

	pthread_mutex_lock(&(mesh->mesh_mutex));

	node_t *n = (node_t *)node;
	memmove(n->recent + 1, n->recent, 4 * sizeof(*n->recent));
	memcpy(n->recent, addr, SALEN(*addr));
	n->status.dirty = true;

	pthread_mutex_unlock(&(mesh->mesh_mutex));
	// @TODO do we want to fire off a connection attempt right away?
}

static bool channel_pre_accept(struct utcp *utcp, uint16_t port) {
	(void)port;
	node_t *n = utcp->priv;
	meshlink_handle_t *mesh = n->mesh;
	return mesh->channel_accept_cb;
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
	} else if(channel->receive_cb) {
		channel->receive_cb(mesh, channel, data, len);
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

static ssize_t channel_send(struct utcp *utcp, const void *data, size_t len) {
	node_t *n = utcp->priv;

	if(n->status.destroyed) {
		return -1;
	}

	meshlink_handle_t *mesh = n->mesh;
	return meshlink_send(mesh, (meshlink_node_t *)n, data, len) ? (ssize_t)len : -1;
}

void meshlink_set_channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, meshlink_channel_receive_cb_t cb) {
	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	channel->receive_cb = cb;
}

static void channel_receive(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len) {
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

	if(channel->poll_cb) {
		channel->poll_cb(mesh, channel, len);
	}
}

void meshlink_set_channel_poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, meshlink_channel_poll_cb_t cb) {
	(void)mesh;
	channel->poll_cb = cb;
	utcp_set_poll_cb(channel->c, cb ? channel_poll : NULL);
}

void meshlink_set_channel_accept_cb(meshlink_handle_t *mesh, meshlink_channel_accept_cb_t cb) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	pthread_mutex_lock(&mesh->mesh_mutex);
	mesh->channel_accept_cb = cb;
	mesh->receive_cb = channel_receive;

	for splay_each(node_t, n, mesh->nodes) {
		if(!n->utcp && n != mesh->self) {
			n->utcp = utcp_init(channel_accept, channel_pre_accept, channel_send, n);
		}
	}

	pthread_mutex_unlock(&mesh->mesh_mutex);
}

meshlink_channel_t *meshlink_channel_open_ex(meshlink_handle_t *mesh, meshlink_node_t *node, uint16_t port, meshlink_channel_receive_cb_t cb, const void *data, size_t len, uint32_t flags) {
	if(data || len) {
		abort();        // TODO: handle non-NULL data
	}

	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	node_t *n = (node_t *)node;

	if(!n->utcp) {
		n->utcp = utcp_init(channel_accept, channel_pre_accept, channel_send, n);
		mesh->receive_cb = channel_receive;

		if(!n->utcp) {
			meshlink_errno = errno == ENOMEM ? MESHLINK_ENOMEM : MESHLINK_EINTERNAL;
			return NULL;
		}
	}

	meshlink_channel_t *channel = xzalloc(sizeof(*channel));
	channel->node = n;
	channel->receive_cb = cb;
	channel->c = utcp_connect_ex(n->utcp, port, channel_recv, channel, flags);

	if(!channel->c) {
		meshlink_errno = errno == ENOMEM ? MESHLINK_ENOMEM : MESHLINK_EINTERNAL;
		free(channel);
		return NULL;
	}

	return channel;
}

meshlink_channel_t *meshlink_channel_open(meshlink_handle_t *mesh, meshlink_node_t *node, uint16_t port, meshlink_channel_receive_cb_t cb, const void *data, size_t len) {
	return meshlink_channel_open_ex(mesh, node, port, cb, data, len, MESHLINK_CHANNEL_TCP);
}

void meshlink_channel_shutdown(meshlink_handle_t *mesh, meshlink_channel_t *channel, int direction) {
	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	utcp_shutdown(channel->c, direction);
}

void meshlink_channel_close(meshlink_handle_t *mesh, meshlink_channel_t *channel) {
	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	utcp_close(channel->c);
	free(channel);
}

ssize_t meshlink_channel_send(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
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
	// Then, preferrably only if there is room in the receiver window,
	// kick the meshlink thread to go send packets.

	pthread_mutex_lock(&mesh->mesh_mutex);
	ssize_t retval = utcp_send(channel->c, data, len);
	pthread_mutex_unlock(&mesh->mesh_mutex);

	if(retval < 0) {
		meshlink_errno = MESHLINK_ENETWORK;
	}

	return retval;
}

uint32_t meshlink_channel_get_flags(meshlink_handle_t *mesh, meshlink_channel_t *channel) {
	if(!mesh || !channel) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	return channel->c->flags;
}

void update_node_status(meshlink_handle_t *mesh, node_t *n) {
	if(n->status.reachable && mesh->channel_accept_cb && !n->utcp) {
		n->utcp = utcp_init(channel_accept, channel_pre_accept, channel_send, n);
	}

	if(mesh->node_status_cb) {
		mesh->node_status_cb(mesh, (meshlink_node_t *)n, n->status.reachable);
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
#if HAVE_CATTA

	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	pthread_mutex_lock(&mesh->mesh_mutex);

	if(mesh->discovery == enable) {
		goto end;
	}

	if(mesh->threadstarted) {
		if(enable) {
			discovery_start(mesh);
		} else {
			discovery_stop(mesh);
		}
	}

	mesh->discovery = enable;

end:
	pthread_mutex_unlock(&mesh->mesh_mutex);
#else
	(void)mesh;
	(void)enable;
	meshlink_errno = MESHLINK_ENOTSUP;
#endif
}

static void __attribute__((constructor)) meshlink_init(void) {
	crypto_init();
	unsigned int seed;
	randomize(&seed, sizeof(seed));
	srand(seed);
}

static void __attribute__((destructor)) meshlink_exit(void) {
	crypto_exit();
}

/// Device class traits
dev_class_traits_t dev_class_traits[_DEV_CLASS_MAX + 1] = {
	{ .min_connects = 3, .max_connects = 10000, .edge_weight = 1 }, // DEV_CLASS_BACKBONE
	{ .min_connects = 3, .max_connects = 100, .edge_weight = 3 },   // DEV_CLASS_STATIONARY
	{ .min_connects = 3, .max_connects = 3, .edge_weight = 6 },             // DEV_CLASS_PORTABLE
	{ .min_connects = 1, .max_connects = 1, .edge_weight = 9 },             // DEV_CLASS_UNKNOWN
};
