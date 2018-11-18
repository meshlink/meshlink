/*
    node.c -- node tree management
    Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>,

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

#include "hash.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "net.h"
#include "netutl.h"
#include "node.h"
#include "splay_tree.h"
#include "utils.h"
#include "xalloc.h"

static int node_compare(const node_t *a, const node_t *b) {
	return strcmp(a->name, b->name);
}

static int node_id_compare(const node_t *a, const node_t *b) {
	if(a->id < b->id) {
		return -1;
	} else if(a->id == b->id) {
		return 0;
	} else {
		return 1;
	}
}

void init_nodes(meshlink_handle_t *mesh) {
	mesh->nodes = splay_alloc_tree((splay_compare_t) node_compare, (splay_action_t) free_node);
	mesh->node_ids = splay_alloc_tree((splay_compare_t) node_id_compare, NULL);
}

void exit_nodes(meshlink_handle_t *mesh) {
	if(mesh->nodes) {
		splay_delete_tree(mesh->nodes);
	}

	if(mesh->node_ids) {
		splay_delete_tree(mesh->node_ids);
	}

	mesh->nodes = NULL;
	mesh->node_ids = NULL;
}

node_t *new_node(void) {
	node_t *n = xzalloc(sizeof(*n));

	n->edge_tree = new_edge_tree();
	n->mtu = MTU;
	n->maxmtu = MTU;
	n->devclass = _DEV_CLASS_MAX;

	return n;
}

void free_node(node_t *n) {
	n->status.destroyed = true;

	utcp_exit(n->utcp);

	if(n->edge_tree) {
		free_edge_tree(n->edge_tree);
	}

	sockaddrfree(&n->address);

	ecdsa_free(n->ecdsa);
	sptps_stop(&n->sptps);

	if(n->mtutimeout.cb) {
		abort();
	}

	free(n->name);

	free(n);
}

void node_add(meshlink_handle_t *mesh, node_t *n) {
	n->mesh = mesh;
	splay_insert(mesh->nodes, n);
	update_node_id(mesh, n);
}

void node_del(meshlink_handle_t *mesh, node_t *n) {
	timeout_del(&mesh->loop, &n->mtutimeout);

	for splay_each(edge_t, e, n->edge_tree) {
		edge_del(mesh, e);
	}

	splay_delete(mesh->nodes, n);
}

node_t *lookup_node(meshlink_handle_t *mesh, const char *name) {
	const node_t n = {.name = (char *)name};
	return splay_search(mesh->nodes, &n);
}

node_t *lookup_node_id(meshlink_handle_t *mesh, uint64_t id) {
	const node_t n = {.id = id};
	return splay_search(mesh->node_ids, &n);
}

void update_node_id(meshlink_handle_t *mesh, node_t *n) {
	if(n->id) {
		logger(mesh, LOG_WARNING, "Node %s already has id %"PRIu64"\n", n->name, n->id);
		return;
	}

	struct {
		uint8_t public[32];
		uint32_t gen;
	} input;

	uint8_t hash[64];
	uint64_t id;

	memset(&input, 0, sizeof input);

	strncpy(input.public, n->name, sizeof input.public);
	input.gen = 0;

	while(true) {
		sha512(&input, sizeof input, hash);
		memcpy(&id, hash, sizeof id);
		input.gen++;

		// ID 0 is reserved
		if(!id) {
			continue;
		}

		// Check if there is a conflict with an existing node
		node_t *other = lookup_node_id(mesh, id);
		int cmp = other ? strcmp(n->name, other->name) : 0;

		// If yes and we sort after the other, try again
		if(cmp > 0) {
			continue;
		}

		if(other) {
			splay_delete(mesh->node_ids, other);
		}

		n->id = id;
		splay_insert(mesh->node_ids, n);

		if(other) {
			update_node_id(mesh, other);
		}

		break;
	}
}

void update_node_udp(meshlink_handle_t *mesh, node_t *n, const sockaddr_t *sa) {
	if(n == mesh->self) {
		logger(mesh, MESHLINK_WARNING, "Trying to update UDP address of mesh->self!");
		return;
	}

	if(sa) {
		n->address = *sa;
		n->sock = 0;

		for(int i = 0; i < mesh->listen_sockets; i++) {
			if(mesh->listen_socket[i].sa.sa.sa_family == sa->sa.sa_family) {
				n->sock = i;
				break;
			}
		}

		meshlink_hint_address(mesh, (meshlink_node_t *)n, &sa->sa);

		if(mesh->log_level >= MESHLINK_DEBUG) {
			char *hostname = sockaddr2hostname(&n->address);
			logger(mesh, MESHLINK_DEBUG, "UDP address of %s set to %s", n->name, hostname);
			free(hostname);
		}
	}
}
