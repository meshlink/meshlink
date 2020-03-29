/*
    net_setup.c -- Setup.
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

#include "conf.h"
#include "connection.h"
#include "ecdsa.h"
#include "graph.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "net.h"
#include "netutl.h"
#include "packmsg.h"
#include "protocol.h"
#include "route.h"
#include "utils.h"
#include "xalloc.h"
#include "submesh.h"

/// Helper function to start parsing a host config file
static bool node_get_config(meshlink_handle_t *mesh, node_t *n, config_t *config, packmsg_input_t *in) {
	if(!config_read(mesh, "current", n->name, config, mesh->config_key)) {
		return false;
	}

	in->ptr = config->buf;
	in->len = config->len;

	uint32_t version = packmsg_get_uint32(in);

	if(version != MESHLINK_CONFIG_VERSION) {
		logger(mesh, MESHLINK_ERROR, "Invalid config file for node %s", n->name);
		config_free(config);
		return false;
	}

	const char *name;
	uint32_t len = packmsg_get_str_raw(in, &name);

	if(len != strlen(n->name) || !name || strncmp(name, n->name, len)) {
		logger(mesh, MESHLINK_ERROR, "Invalid config file for node %s", n->name);
		config_free(config);
		return false;
	}

	return true;
}

/// Read the public key from a host config file. Used whenever we need to start an SPTPS session.
bool node_read_public_key(meshlink_handle_t *mesh, node_t *n) {
	if(ecdsa_active(n->ecdsa)) {
		return true;
	}

	config_t config;
	packmsg_input_t in;

	if(!node_get_config(mesh, n, &config, &in)) {
		return false;
	}

	packmsg_skip_element(&in); /* submesh */
	packmsg_get_int32(&in); /* devclass */
	packmsg_get_bool(&in); /* blacklisted */

	const void *key;
	uint32_t len = packmsg_get_bin_raw(&in, &key);

	if(len != 32) {
		config_free(&config);
		return false;
	}

	n->ecdsa = ecdsa_set_public_key(key);

	// While we are at it, read known address information
	if(!n->canonical_address) {
		n->canonical_address = packmsg_get_str_dup(&in);
	} else {
		packmsg_skip_element(&in);
	}

	// Append any known addresses in the config file to the list we currently have
	uint32_t known_count = 0;

	for(uint32_t i = 0; i < MAX_RECENT; i++) {
		if(n->recent[i].sa.sa_family) {
			known_count++;
		}
	}

	uint32_t count = packmsg_get_array(&in);

	for(uint32_t i = 0; i < count; i++) {
		if(i < MAX_RECENT - known_count) {
			n->recent[i + known_count] = packmsg_get_sockaddr(&in);
		} else {
			packmsg_skip_element(&in);
		}
	}

	time_t last_reachable = packmsg_get_int64(&in);
	time_t last_unreachable = packmsg_get_int64(&in);

	if(!n->last_reachable) {
		n->last_reachable = last_reachable;
	}

	if(!n->last_unreachable) {
		n->last_unreachable = last_unreachable;
	}

	config_free(&config);
	return true;
}

/// Fill in node details from a config blob.
bool node_read_from_config(meshlink_handle_t *mesh, node_t *n, const config_t *config) {
	if(n->canonical_address) {
		return true;
	}

	packmsg_input_t in = {config->buf, config->len};
	uint32_t version = packmsg_get_uint32(&in);

	if(version != MESHLINK_CONFIG_VERSION) {
		return false;
	}

	char *name = packmsg_get_str_dup(&in);

	if(!name) {
		return false;
	}

	if(n->name) {
		if(strcmp(n->name, name)) {
			free(name);
			return false;
		}

		free(name);
	} else {
		n->name = name;
	}

	char *submesh_name = packmsg_get_str_dup(&in);

	if(!strcmp(submesh_name, CORE_MESH)) {
		free(submesh_name);
		n->submesh = NULL;
	} else {
		n->submesh = lookup_or_create_submesh(mesh, submesh_name);
		free(submesh_name);

		if(!n->submesh) {
			return false;
		}
	}

	n->devclass = packmsg_get_int32(&in);
	n->status.blacklisted = packmsg_get_bool(&in);
	const void *key;
	uint32_t len = packmsg_get_bin_raw(&in, &key);

	if(len) {
		if(len != 32) {
			return false;
		}

		if(!ecdsa_active(n->ecdsa)) {
			n->ecdsa = ecdsa_set_public_key(key);
		}
	}

	n->canonical_address = packmsg_get_str_dup(&in);
	uint32_t count = packmsg_get_array(&in);

	for(uint32_t i = 0; i < count; i++) {
		if(i < MAX_RECENT) {
			n->recent[i] = packmsg_get_sockaddr(&in);
		} else {
			packmsg_skip_element(&in);
		}
	}

	n->last_reachable = packmsg_get_int64(&in);
	n->last_unreachable = packmsg_get_int64(&in);

	return packmsg_done(&in);
}

bool node_write_config(meshlink_handle_t *mesh, node_t *n) {
	if(!mesh->confbase) {
		return true;
	}

	uint8_t buf[4096];
	packmsg_output_t out = {buf, sizeof(buf)};

	packmsg_add_uint32(&out, MESHLINK_CONFIG_VERSION);
	packmsg_add_str(&out, n->name);
	packmsg_add_str(&out, n->submesh ? n->submesh->name : CORE_MESH);
	packmsg_add_int32(&out, n->devclass);
	packmsg_add_bool(&out, n->status.blacklisted);

	if(ecdsa_active(n->ecdsa)) {
		packmsg_add_bin(&out, ecdsa_get_public_key(n->ecdsa), 32);
	} else {
		packmsg_add_bin(&out, "", 0);
	}

	packmsg_add_str(&out, n->canonical_address ? n->canonical_address : "");

	uint32_t count = 0;

	for(uint32_t i = 0; i < MAX_RECENT; i++) {
		if(n->recent[i].sa.sa_family) {
			count++;
		} else {
			break;
		}
	}

	packmsg_add_array(&out, count);

	for(uint32_t i = 0; i < count; i++) {
		packmsg_add_sockaddr(&out, &n->recent[i]);
	}

	packmsg_add_int64(&out, n->last_reachable);
	packmsg_add_int64(&out, n->last_unreachable);

	if(!packmsg_output_ok(&out)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	config_t config = {buf, packmsg_output_size(&out, buf)};

	if(!config_write(mesh, "current", n->name, &config, mesh->config_key)) {
		call_error_cb(mesh, MESHLINK_ESTORAGE);
		return false;
	}

	return true;
}

static bool load_node(meshlink_handle_t *mesh, const char *name, void *priv) {
	(void)priv;

	if(!check_id(name)) {
		return true;
	}

	node_t *n = lookup_node(mesh, name);

	if(n) {
		return true;
	}

	n = new_node();
	n->name = xstrdup(name);

	config_t config;
	packmsg_input_t in;

	if(!node_get_config(mesh, n, &config, &in)) {
		free_node(n);
		return false;
	}

	if(!node_read_from_config(mesh, n, &config)) {
		logger(mesh, MESHLINK_ERROR, "Invalid config file for node %s", n->name);
		config_free(&config);
		free_node(n);
		return false;
	}

	config_free(&config);

	node_add(mesh, n);

	return true;
}

int setup_tcp_listen_socket(meshlink_handle_t *mesh, const struct addrinfo *aip) {
	int nfd = socket(aip->ai_family, SOCK_STREAM, IPPROTO_TCP);

	if(nfd == -1) {
		return -1;
	}

#ifdef FD_CLOEXEC
	fcntl(nfd, F_SETFD, FD_CLOEXEC);
#endif

	int option = 1;
	setsockopt(nfd, SOL_SOCKET, SO_REUSEADDR, (void *)&option, sizeof(option));

#if defined(IPV6_V6ONLY)

	if(aip->ai_family == AF_INET6) {
		setsockopt(nfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&option, sizeof(option));
	}

#else
#warning IPV6_V6ONLY not defined
#endif

	if(bind(nfd, aip->ai_addr, aip->ai_addrlen)) {
		closesocket(nfd);
		return -1;
	}

	if(listen(nfd, 3)) {
		logger(mesh, MESHLINK_ERROR, "System call `%s' failed: %s", "listen", sockstrerror(sockerrno));
		closesocket(nfd);
		return -1;
	}

	return nfd;
}

int setup_udp_listen_socket(meshlink_handle_t *mesh, const struct addrinfo *aip) {
	int nfd = socket(aip->ai_family, SOCK_DGRAM, IPPROTO_UDP);

	if(nfd == -1) {
		return -1;
	}

#ifdef FD_CLOEXEC
	fcntl(nfd, F_SETFD, FD_CLOEXEC);
#endif

#ifdef O_NONBLOCK
	int flags = fcntl(nfd, F_GETFL);

	if(fcntl(nfd, F_SETFL, flags | O_NONBLOCK) < 0) {
		closesocket(nfd);
		logger(mesh, MESHLINK_ERROR, "System call `%s' failed: %s", "fcntl", strerror(errno));
		return -1;
	}

#elif defined(WIN32)
	unsigned long arg = 1;

	if(ioctlsocket(nfd, FIONBIO, &arg) != 0) {
		closesocket(nfd);
		logger(mesh, MESHLINK_ERROR, "Call to `%s' failed: %s", "ioctlsocket", sockstrerror(sockerrno));
		return -1;
	}

#endif

	int option = 1;
	setsockopt(nfd, SOL_SOCKET, SO_REUSEADDR, (void *)&option, sizeof(option));
	setsockopt(nfd, SOL_SOCKET, SO_BROADCAST, (void *)&option, sizeof(option));

#if defined(IPV6_V6ONLY)

	if(aip->ai_family == AF_INET6) {
		setsockopt(nfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&option, sizeof(option));
	}

#endif

#if defined(IP_DONTFRAG) && !defined(IP_DONTFRAGMENT)
#define IP_DONTFRAGMENT IP_DONTFRAG
#endif

#if defined(IP_MTU_DISCOVER) && defined(IP_PMTUDISC_DO)
	option = IP_PMTUDISC_DO;
	setsockopt(nfd, IPPROTO_IP, IP_MTU_DISCOVER, (void *)&option, sizeof(option));
#elif defined(IP_DONTFRAGMENT)
	option = 1;
	setsockopt(nfd, IPPROTO_IP, IP_DONTFRAGMENT, (void *)&option, sizeof(option));
#endif

	if(aip->ai_family == AF_INET6) {
#if defined(IPV6_MTU_DISCOVER) && defined(IPV6_PMTUDISC_DO)
		option = IPV6_PMTUDISC_DO;
		setsockopt(nfd, IPPROTO_IPV6, IPV6_MTU_DISCOVER, (void *)&option, sizeof(option));
#elif defined(IPV6_DONTFRAG)
		option = 1;
		setsockopt(nfd, IPPROTO_IPV6, IPV6_DONTFRAG, (void *)&option, sizeof(option));
#endif
	}

	if(bind(nfd, aip->ai_addr, aip->ai_addrlen)) {
		closesocket(nfd);
		return -1;
	}

	return nfd;
}

/*
  Add listening sockets.
*/
static bool add_listen_sockets(meshlink_handle_t *mesh) {
	struct addrinfo *ai;

	struct addrinfo hint = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP,
		.ai_flags = AI_PASSIVE,
	};

	int err = getaddrinfo(NULL, mesh->myport, &hint, &ai);

	if(err || !ai) {
		logger(mesh, MESHLINK_ERROR, "System call `%s' failed: %s", "getaddrinfo", err == EAI_SYSTEM ? strerror(err) : gai_strerror(err));
		return false;
	}

	bool success = false;

	for(struct addrinfo *aip = ai; aip; aip = aip->ai_next) {
		// Ignore duplicate addresses
		bool found = false;

		for(int i = 0; i < mesh->listen_sockets; i++) {
			if(!memcmp(&mesh->listen_socket[i].sa, aip->ai_addr, aip->ai_addrlen)) {
				found = true;
				break;
			}
		}

		if(found) {
			continue;
		}

		if(mesh->listen_sockets >= MAXSOCKETS) {
			logger(mesh, MESHLINK_ERROR, "Too many listening sockets");
			return false;
		}

		/* Try to bind to TCP */

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

		io_add(&mesh->loop, &mesh->listen_socket[mesh->listen_sockets].tcp, handle_new_meta_connection, &mesh->listen_socket[mesh->listen_sockets], tcp_fd, IO_READ);
		io_add(&mesh->loop, &mesh->listen_socket[mesh->listen_sockets].udp, handle_incoming_vpn_data, &mesh->listen_socket[mesh->listen_sockets], udp_fd, IO_READ);

		if(mesh->log_level <= MESHLINK_INFO) {
			char *hostname = sockaddr2hostname((sockaddr_t *) aip->ai_addr);
			logger(mesh, MESHLINK_INFO, "Listening on %s", hostname);
			free(hostname);
		}

		memcpy(&mesh->listen_socket[mesh->listen_sockets].sa, aip->ai_addr, aip->ai_addrlen);
		memcpy(&mesh->listen_socket[mesh->listen_sockets].broadcast_sa, aip->ai_addr, aip->ai_addrlen);

		if(aip->ai_family == AF_INET6) {
			mesh->listen_socket[mesh->listen_sockets].broadcast_sa.in6.sin6_addr.s6_addr[0x0] = 0xff;
			mesh->listen_socket[mesh->listen_sockets].broadcast_sa.in6.sin6_addr.s6_addr[0x1] = 0x02;
			mesh->listen_socket[mesh->listen_sockets].broadcast_sa.in6.sin6_addr.s6_addr[0xf] = 0x01;
		} else {
			mesh->listen_socket[mesh->listen_sockets].broadcast_sa.in.sin_addr.s_addr = 0xffffffff;
		}

		mesh->listen_sockets++;
		success = true;
	}

	freeaddrinfo(ai);

	if(!success) {
		for(int i = 0; i < mesh->listen_sockets; i++) {
			io_del(&mesh->loop, &mesh->listen_socket[i].tcp);
			io_del(&mesh->loop, &mesh->listen_socket[i].udp);
			close(mesh->listen_socket[i].tcp.fd);
			close(mesh->listen_socket[i].udp.fd);
		}

		mesh->listen_sockets = 0;
	}

	return success;
}

/*
  Configure node_t mesh->self and set up the local sockets (listen only)
*/
bool setup_myself(meshlink_handle_t *mesh) {
	/* Set some defaults */

	mesh->maxtimeout = 900;

	/* Done */

	mesh->self->nexthop = mesh->self;

	node_add(mesh, mesh->self);

	if(!config_scan_all(mesh, "current", "hosts", load_node, NULL)) {
		logger(mesh, MESHLINK_WARNING, "Could not scan all host config files");
	}

	/* Open sockets */

	mesh->listen_sockets = 0;

	if(!add_listen_sockets(mesh)) {
		if(strcmp(mesh->myport, "0")) {
			logger(mesh, MESHLINK_WARNING, "Could not bind to port %s, trying to find an alternative port", mesh->myport);

			if(!check_port(mesh)) {
				logger(mesh, MESHLINK_WARNING, "Could not bind to any port, trying to bind to port 0");
				free(mesh->myport);
				mesh->myport = xstrdup("0");
			}

			if(!add_listen_sockets(mesh)) {
				return false;
			}
		} else {
			return false;
		}
	}

	if(!mesh->listen_sockets) {
		logger(mesh, MESHLINK_ERROR, "Unable to create any listening socket!");
		return false;
	}

	/* Done. */

	mesh->last_unreachable = mesh->loop.now.tv_sec;

	return true;
}

/*
  initialize network
*/
bool setup_network(meshlink_handle_t *mesh) {
	init_connections(mesh);
	init_submeshes(mesh);
	init_nodes(mesh);
	init_edges(mesh);
	init_requests(mesh);

	if(!setup_myself(mesh)) {
		return false;
	}

	return true;
}

/*
  close all open network connections
*/
void close_network_connections(meshlink_handle_t *mesh) {
	if(mesh->connections) {
		for(list_node_t *node = mesh->connections->head, *next; node; node = next) {
			next = node->next;
			connection_t *c = node->data;
			c->outgoing = NULL;
			terminate_connection(mesh, c, false);
		}
	}

	for(int i = 0; i < mesh->listen_sockets; i++) {
		io_del(&mesh->loop, &mesh->listen_socket[i].tcp);
		io_del(&mesh->loop, &mesh->listen_socket[i].udp);
		close(mesh->listen_socket[i].tcp.fd);
		close(mesh->listen_socket[i].udp.fd);
	}

	exit_requests(mesh);
	exit_edges(mesh);
	exit_nodes(mesh);
	exit_submeshes(mesh);
	exit_connections(mesh);

	free(mesh->myport);
	mesh->myport = NULL;

	mesh->self = NULL;

	return;
}
