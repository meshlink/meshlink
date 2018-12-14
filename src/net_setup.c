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

/// Helper function to start parsing a host config file
static bool node_get_config(meshlink_handle_t *mesh, node_t *n, config_t *config, packmsg_input_t *in) {
	if(!config_read(mesh, n->name, config)) {
		return false;
	}

	in->ptr = config->buf;
	in->len = config->len;

	uint32_t version = packmsg_get_uint32(in);

	if(version != MESHLINK_CONFIG_VERSION) {
		config_free(config);
		return false;
	}

	const char *name;
	uint32_t len = packmsg_get_str_raw(in, &name);

	if(len != strlen(n->name) || strncmp(name, n->name, len)) {
		config_free(config);
		return false;
	}

	return true;
}

/// Read just the devclass from a host config file. Used at startup when reading all host config files.
bool node_read_devclass(meshlink_handle_t *mesh, node_t *n) {
	config_t config;
	packmsg_input_t in;

	if(!node_get_config(mesh, n, &config, &in)) {
		return false;
	}

	int32_t devclass = packmsg_get_int32(&in);
	bool blacklisted = packmsg_get_bool(&in);
	config_free(&config);

	if(!packmsg_input_ok(&in) || devclass < 0 || devclass > _DEV_CLASS_MAX) {
		return false;
	}

	n->devclass = devclass;
	n->status.blacklisted = blacklisted;
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

	packmsg_get_int32(&in); /* devclass */
	packmsg_get_bool(&in); /* blacklisted */

	const void *key;
	uint32_t len = packmsg_get_bin_raw(&in, &key);

	if(len != 32) {
		config_free(&config);
		return false;
	}

	n->ecdsa = ecdsa_set_public_key(key);
	config_free(&config);
	return true;
}

/// Read the full host config file. Used whenever we need to start an SPTPS session.
bool node_read_full(meshlink_handle_t *mesh, node_t *n) {
	if(n->canonical_address) {
		return true;
	}

	config_t config;
	packmsg_input_t in;

	if(!node_get_config(mesh, n, &config, &in)) {
		return false;
	}

	packmsg_get_int32(&in); /* devclass */
	packmsg_get_bool(&in); /* blacklisted */
	const void *key;
	uint32_t len = packmsg_get_bin_raw(&in, &key); /* public key */

	if(len != 32) {
		return false;
	}

	if(!ecdsa_active(n->ecdsa)) {
		n->ecdsa = ecdsa_set_public_key(key);
	}

	n->canonical_address = packmsg_get_str_dup(&in);

	uint32_t count = packmsg_get_array(&in);

	if(count > 5) {
		count = 5;
	}

	for(uint32_t i = 0; i < count; i++) {
		n->recent[i] = packmsg_get_sockaddr(&in);
	}

	config_free(&config);

	return packmsg_done(&in);
}

bool node_write_config(meshlink_handle_t *mesh, node_t *n) {
	uint8_t buf[4096];
	packmsg_output_t out = {buf, sizeof(buf)};

	packmsg_add_uint32(&out, MESHLINK_CONFIG_VERSION);
	packmsg_add_str(&out, n->name);
	packmsg_add_int32(&out, n->devclass);
	assert(n->devclass != 3);
	packmsg_add_bool(&out, n->status.blacklisted);

	if(ecdsa_active(n->ecdsa)) {
		packmsg_add_bin(&out, ecdsa_get_public_key(n->ecdsa), 32);
	} else {
		packmsg_add_bin(&out, "", 0);
	}

	packmsg_add_str(&out, n->canonical_address ? n->canonical_address : "");

	uint32_t count = 0;

	for(uint32_t i = 0; i < 5; i++) {
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

	if(!packmsg_output_ok(&out)) {
		return false;
	}

	config_t config = {buf, packmsg_output_size(&out, buf)};
	return config_write(mesh, n->name, &config);
}

void load_all_nodes(meshlink_handle_t *mesh) {
	DIR *dir;
	struct dirent *ent;
	char dname[PATH_MAX];

	snprintf(dname, PATH_MAX, "%s" SLASH "hosts", mesh->confbase);
	dir = opendir(dname);

	if(!dir) {
		logger(mesh, MESHLINK_ERROR, "Could not open %s: %s", dname, strerror(errno));
		return;
	}

	while((ent = readdir(dir))) {
		if(!check_id(ent->d_name)) {
			continue;
		}

		node_t *n = lookup_node(mesh, ent->d_name);

		if(n) {
			continue;
		}

		n = new_node();
		n->name = xstrdup(ent->d_name);
		node_read_devclass(mesh, n);
		node_add(mesh, n);
	}

	closedir(dir);
}

/*
  Add listening sockets.
*/
static bool add_listen_address(meshlink_handle_t *mesh, char *address, bool bindto) {
	char *port = mesh->myport;

	if(address) {
		char *space = strchr(address, ' ');

		if(space) {
			*space++ = 0;
			port = space;
		}

		if(!strcmp(address, "*")) {
			*address = 0;
		}
	}

	struct addrinfo *ai;

	struct addrinfo hint = {
		.ai_family = addressfamily,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP,
		.ai_flags = AI_PASSIVE,
	};

	int err = getaddrinfo(address && *address ? address : NULL, port, &hint, &ai);

	free(address);

	if(err || !ai) {
		logger(mesh, MESHLINK_ERROR, "System call `%s' failed: %s", "getaddrinfo", err == EAI_SYSTEM ? strerror(err) : gai_strerror(err));
		return false;
	}

	bool success = false;

	for(struct addrinfo *aip = ai; aip; aip = aip->ai_next) {
		// Ignore duplicate addresses
		bool found = false;

		for(int i = 0; i < mesh->listen_sockets; i++)
			if(!memcmp(&mesh->listen_socket[i].sa, aip->ai_addr, aip->ai_addrlen)) {
				found = true;
				break;
			}

		if(found) {
			continue;
		}

		if(mesh->listen_sockets >= MAXSOCKETS) {
			logger(mesh, MESHLINK_ERROR, "Too many listening sockets");
			return false;
		}

		int tcp_fd = setup_listen_socket((sockaddr_t *) aip->ai_addr);

		if(tcp_fd < 0) {
			continue;
		}

		int udp_fd = setup_vpn_in_socket(mesh, (sockaddr_t *) aip->ai_addr);

		if(udp_fd < 0) {
			close(tcp_fd);
			continue;
		}

		io_add(&mesh->loop, &mesh->listen_socket[mesh->listen_sockets].tcp, handle_new_meta_connection, &mesh->listen_socket[mesh->listen_sockets], tcp_fd, IO_READ);
		io_add(&mesh->loop, &mesh->listen_socket[mesh->listen_sockets].udp, handle_incoming_vpn_data, &mesh->listen_socket[mesh->listen_sockets], udp_fd, IO_READ);

		if(mesh->log_level >= MESHLINK_INFO) {
			char *hostname = sockaddr2hostname((sockaddr_t *) aip->ai_addr);
			logger(mesh, MESHLINK_INFO, "Listening on %s", hostname);
			free(hostname);
		}

		mesh->listen_socket[mesh->listen_sockets].bindto = bindto;
		memcpy(&mesh->listen_socket[mesh->listen_sockets].sa, aip->ai_addr, aip->ai_addrlen);
		mesh->listen_sockets++;
		success = true;
	}

	freeaddrinfo(ai);
	return success;
}

/*
  Configure node_t mesh->self and set up the local sockets (listen only)
*/
bool setup_myself(meshlink_handle_t *mesh) {
	/* Set some defaults */

	mesh->localdiscovery = true;
	keylifetime = 3600; // TODO: check if this can be removed as well
	mesh->maxtimeout = 900;
	mesh->self->options |= OPTION_PMTU_DISCOVERY;

	/* Done */

	mesh->self->nexthop = mesh->self;
	mesh->self->via = mesh->self;
	mesh->self->status.reachable = true;
	mesh->self->last_state_change = mesh->loop.now.tv_sec;

	node_add(mesh, mesh->self);

	graph(mesh);

	load_all_nodes(mesh);

	/* Open sockets */

	mesh->listen_sockets = 0;

	if(!add_listen_address(mesh, NULL, NULL)) {
		if(strcmp(mesh->myport, "0")) {
			logger(mesh, MESHLINK_INFO, "Could not bind to port %s, asking OS to choose one for us", mesh->myport);
			free(mesh->myport);
			mesh->myport = strdup("0");

			if(!mesh->myport) {
				return false;
			}

			if(!add_listen_address(mesh, NULL, NULL)) {
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

	mesh->last_config_check = mesh->loop.now.tv_sec;

	return true;
}

/*
  initialize network
*/
bool setup_network(meshlink_handle_t *mesh) {
	init_connections(mesh);
	init_nodes(mesh);
	init_edges(mesh);
	init_requests(mesh);

	mesh->pinginterval = 60;
	mesh->pingtimeout = 5;
	maxoutbufsize = 10 * MTU;

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
	exit_connections(mesh);

	if(mesh->myport) {
		free(mesh->myport);
	}

	return;
}
