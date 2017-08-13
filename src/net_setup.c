/*
    net_setup.c -- Setup.
    Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>

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
#include "protocol.h"
#include "route.h"
#include "utils.h"
#include "xalloc.h"

bool node_read_ecdsa_public_key(meshlink_handle_t *mesh, node_t *n) {
	if(ecdsa_active(n->ecdsa))
		return true;

	splay_tree_t *config_tree;
	char *p;

	init_configuration(&config_tree);
	if(!read_host_config(mesh, config_tree, n->name))
		goto exit;

	/* First, check for simple ECDSAPublicKey statement */

	if(get_config_string(lookup_config(config_tree, "ECDSAPublicKey"), &p)) {
		n->ecdsa = ecdsa_set_base64_public_key(p);
		free(p);
	}

exit:
	exit_configuration(&config_tree);
	return n->ecdsa;
}

bool read_ecdsa_public_key(meshlink_handle_t *mesh, connection_t *c) {
	if(ecdsa_active(c->ecdsa))
		return true;

	char *p;

	if(!c->config_tree) {
		init_configuration(&c->config_tree);
		if(!read_host_config(mesh, c->config_tree, c->name))
			return false;
	}

	/* First, check for simple ECDSAPublicKey statement */

	if(get_config_string(lookup_config(c->config_tree, "ECDSAPublicKey"), &p)) {
		c->ecdsa = ecdsa_set_base64_public_key(p);
		free(p);
		return c->ecdsa;
	}

	return false;
}

bool read_ecdsa_private_key(meshlink_handle_t *mesh) {
	FILE *fp;
	char filename[PATH_MAX];

	snprintf(filename, PATH_MAX, "%s" SLASH "ecdsa_key.priv", mesh->confbase);
	fp = fopen(filename, "rb");

	if(!fp) {
		logger(mesh, MESHLINK_ERROR, "Error reading ECDSA private key file: %s", strerror(errno));
		return false;
	}

	mesh->self->connection->ecdsa = ecdsa_read_pem_private_key(fp);
	fclose(fp);

	if(!mesh->self->connection->ecdsa)
		logger(mesh, MESHLINK_ERROR, "Reading ECDSA private key file failed: %s", strerror(errno));

	return mesh->self->connection->ecdsa;
}

static bool read_invitation_key(meshlink_handle_t *mesh) {
	FILE *fp;
	char filename[PATH_MAX];

	if(mesh->invitation_key) {
		ecdsa_free(mesh->invitation_key);
		mesh->invitation_key = NULL;
	}

	snprintf(filename, PATH_MAX, "%s" SLASH "invitations" SLASH "ecdsa_key.priv", mesh->confbase);

	fp = fopen(filename, "rb");

	if(fp) {
		mesh->invitation_key = ecdsa_read_pem_private_key(fp);
		fclose(fp);
		if(!mesh->invitation_key)
			logger(mesh, MESHLINK_ERROR, "Reading ECDSA private key file `%s' failed: %s", filename, strerror(errno));
	}

	return mesh->invitation_key;
}

bool node_read_devclass(meshlink_handle_t *mesh, node_t *n) {

	splay_tree_t *config_tree;
	char *p;

	init_configuration(&config_tree);
	if(!read_host_config(mesh, config_tree, n->name))
		goto exit;

	if(get_config_string(lookup_config(config_tree, "DeviceClass"), &p)) {
		n->devclass = atoi(p);
		free(p);
	}

	if(n->devclass < 0 || n->devclass > _DEV_CLASS_MAX)
		n->devclass = _DEV_CLASS_MAX;

exit:
	exit_configuration(&config_tree);
	return n->devclass != 0;
}

bool node_write_devclass(meshlink_handle_t *mesh, node_t *n) {

	if(n->devclass < 0 || n->devclass > _DEV_CLASS_MAX)
		return false;

	bool result = false;

	splay_tree_t *config_tree;
	init_configuration(&config_tree);

	// ignore read errors; in case the file does not exist we will create it
	read_host_config(mesh, config_tree, n->name);

	config_t *cnf = lookup_config(config_tree, "DeviceClass");

	if(!cnf) {
		cnf = new_config();
		cnf->variable = xstrdup("DeviceClass");
		config_add(config_tree, cnf);
	}

	set_config_int(cnf, n->devclass);

	if(!write_host_config(mesh, config_tree, n->name))
		goto fail;

	result = true;

fail:
	exit_configuration(&config_tree);
	return result;
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
		if(!check_id(ent->d_name))
			continue;

		node_t *n = lookup_node(mesh, ent->d_name);
		if(n)
			continue;

		n = new_node();
		n->name = xstrdup(ent->d_name);
		node_read_devclass(mesh, n);
		node_add(mesh, n);
	}

	closedir(dir);
}


char *get_name(meshlink_handle_t *mesh) {
	char *name = NULL;

	get_config_string(lookup_config(mesh->config, "Name"), &name);

	if(!name)
		return NULL;

	if(!check_id(name)) {
		logger(mesh, MESHLINK_ERROR, "Invalid name for mesh->self!");
		free(name);
		return NULL;
	}

	return name;
}

bool setup_myself_reloadable(meshlink_handle_t *mesh) {
	mesh->localdiscovery = true;
	keylifetime = 3600; // TODO: check if this can be removed as well
	mesh->maxtimeout = 900;
	mesh->self->options |= OPTION_PMTU_DISCOVERY;

	read_invitation_key(mesh);

	return true;
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

		if(!strcmp(address, "*"))
			*address = 0;
	}

	struct addrinfo *ai, hint = {0};
	hint.ai_family = addressfamily;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = IPPROTO_TCP;
	hint.ai_flags = AI_PASSIVE;

	int err = getaddrinfo(address && *address ? address : NULL, port, &hint, &ai);
	free(address);

	if(err || !ai) {
		logger(mesh, MESHLINK_ERROR, "System call `%s' failed: %s", "getaddrinfo", err == EAI_SYSTEM ? strerror(err) : gai_strerror(err));
		return false;
	}

	for(struct addrinfo *aip = ai; aip; aip = aip->ai_next) {
		// Ignore duplicate addresses
		bool found = false;

		for(int i = 0; i < mesh->listen_sockets; i++)
			if(!memcmp(&mesh->listen_socket[i].sa, aip->ai_addr, aip->ai_addrlen)) {
				found = true;
				break;
			}

		if(found)
			continue;

		if(mesh->listen_sockets >= MAXSOCKETS) {
			logger(mesh, MESHLINK_ERROR, "Too many listening sockets");
			return false;
		}

		int tcp_fd = setup_listen_socket((sockaddr_t *) aip->ai_addr);

		if(tcp_fd < 0)
			continue;

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
	}

	freeaddrinfo(ai);
	return true;
}

/*
  Configure node_t mesh->self and set up the local sockets (listen only)
*/
bool setup_myself(meshlink_handle_t *mesh) {
	char *name;
	char *address = NULL;

	if(!(name = get_name(mesh))) {
		logger(mesh, MESHLINK_ERROR, "Name for MeshLink instance required!");
		return false;
	}

	mesh->self = new_node();
	mesh->self->connection = new_connection();
	mesh->self->name = name;
	mesh->self->devclass = mesh->devclass;
	mesh->self->connection->name = xstrdup(name);
	read_host_config(mesh, mesh->config, name);

	if(!get_config_string(lookup_config(mesh->config, "Port"), &mesh->myport)) {
		logger(mesh, MESHLINK_ERROR, "Port for MeshLink instance required!");
		return false;
	}

	mesh->self->connection->options = 0;
	mesh->self->connection->protocol_major = PROT_MAJOR;
	mesh->self->connection->protocol_minor = PROT_MINOR;

	mesh->self->options |= PROT_MINOR << 24;

	if(!read_ecdsa_private_key(mesh))
		return false;

	/* Ensure mesh->myport is numeric */

	if(!atoi(mesh->myport)) {
		struct addrinfo *ai = str2addrinfo("localhost", mesh->myport, SOCK_DGRAM);
		sockaddr_t sa;
		if(!ai || !ai->ai_addr)
			return false;
		free(mesh->myport);
		memcpy(&sa, ai->ai_addr, ai->ai_addrlen);
		sockaddr2str(&sa, NULL, &mesh->myport);
	}

	/* Check some options */

	if(!setup_myself_reloadable(mesh))
		return false;

	/* Compression */

	// TODO: drop compression in the packet layer?
	mesh->self->incompression = 0;
	mesh->self->connection->outcompression = 0;

	/* Done */

	mesh->self->nexthop = mesh->self;
	mesh->self->via = mesh->self;
	mesh->self->status.reachable = true;
	mesh->self->last_state_change = mesh->loop.now.tv_sec;

	node_write_devclass(mesh, mesh->self);
	node_add(mesh, mesh->self);

	graph(mesh);

	load_all_nodes(mesh);

	/* Open sockets */

	mesh->listen_sockets = 0;

	if(!add_listen_address(mesh, address, NULL)) {
		if(!strcmp(mesh->myport, "0")) {
			logger(mesh, MESHLINK_WARNING, "Could not bind to port %s, asking OS to choose one for us", mesh->myport);
			free(mesh->myport);
			mesh->myport = strdup("0");
			if(!mesh->myport)
				return false;
			if(!add_listen_address(mesh, address, NULL))
				return false;
		} else
			return false;
	}

	if(!mesh->listen_sockets) {
		logger(mesh, MESHLINK_ERROR, "Unable to create any listening socket!");
		return false;
	}

	xasprintf(&mesh->self->hostname, "MYSELF port %s", mesh->myport);
	mesh->self->connection->hostname = xstrdup(mesh->self->hostname);

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

	if(!setup_myself(mesh))
		return false;

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

	if(mesh->outgoings)
		list_delete_list(mesh->outgoings);

	if(mesh->self && mesh->self->connection) {
		terminate_connection(mesh, mesh->self->connection, false);
		free_connection(mesh->self->connection);
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

	if(mesh->myport) free(mesh->myport);

	return;
}
