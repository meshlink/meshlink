/*
    autoconnect.c -- automatic connection establishment
    Copyright (C) 2019 Guus Sliepen <guus@meshlink.io>

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

#include "connection.h"
#include "list.h"
#include "logger.h"
#include "net.h"
#include "node.h"
#include "xalloc.h"

/* Make an outgoing connection if possible */
static bool make_outgoing(meshlink_handle_t *mesh, node_t *n) {
	if(!n || n->connection) {
		return false;
	}

	n->last_connect_try = mesh->loop.now.tv_sec;
	logger(mesh, MESHLINK_DEBUG, "Autoconnect trying to connect to %s", n->name);

	/* check if there is already a connection attempt to this node */
	for list_each(outgoing_t, outgoing, mesh->outgoings) {
		if(outgoing->node == n) {
			logger(mesh, MESHLINK_DEBUG, "* skip autoconnect since it is an outgoing connection already");
			return false;
		}
	}

	if(!n->status.reachable && !node_read_public_key(mesh, n)) {
		logger(mesh, MESHLINK_DEBUG, "* skip autoconnect since we don't know this node's public key");
		return false;
	}

	logger(mesh, MESHLINK_DEBUG, "Autoconnecting to %s", n->name);
	outgoing_t *outgoing = xzalloc(sizeof(outgoing_t));
	outgoing->node = n;
	list_insert_tail(mesh->outgoings, outgoing);
	setup_outgoing_connection(mesh, outgoing);
	return true;
}

/* Determine if node n is a better candidate for making an early connection to than node m. */
static bool compare_candidates(node_t *n, node_t *m) {
	/* Check if the last connection attempt was successful */
	bool n_successful = n->last_successful_connection > n->last_connect_try;
	bool m_successful = n->last_successful_connection > n->last_connect_try;

	if(n_successful != m_successful) {
		return n_successful;
	} else {
		if(n_successful) {
			/* If both were successfully connected to, prefer the most recent one */
			return n->last_successful_connection > m->last_successful_connection;
		} else {
			/* If the last connections were not successful, prefer the one we least recently tried to connect to. */
			return n->last_connect_try < m->last_connect_try;
		}
	}
}

/* Try to connect to any candidate in the same or better device class. Prefer recently connected-to nodes first. */
static bool make_eager_connection(meshlink_handle_t *mesh) {
	node_t *candidate = NULL;

	for splay_each(node_t, n, mesh->nodes) {
		if(n == mesh->self || n->devclass > mesh->devclass || n->connection || n->status.blacklisted) {
			continue;
		}

		if(!candidate) {
			candidate = n;
			continue;
		}

		if(compare_candidates(n, candidate)) {
			candidate = n;
		}
	}

	return make_outgoing(mesh, candidate);
}

/* Try to connect to balance connections to different device classes. Prefer recently connected-to nodes first. */
static bool make_better_connection(meshlink_handle_t *mesh) {
	const unsigned int min_connects = mesh->dev_class_traits[mesh->devclass].min_connects;

	for(dev_class_t devclass = 0; devclass <= mesh->devclass; ++devclass) {
		unsigned int connects = 0;

		for list_each(connection_t, c, mesh->connections) {
			if(c->status.active && c->node && c->node->devclass == devclass) {
				connects += 1;

				if(connects >= min_connects) {
					break;
				}
			}
		}

		if(connects >= min_connects) {
			continue;
		}

		node_t *candidate = NULL;

		for splay_each(node_t, n, mesh->nodes) {
			if(n == mesh->self || n->devclass != devclass || n->connection || n->status.blacklisted) {
				continue;
			}

			if(!candidate) {
				candidate = n;
				continue;
			}

			if(compare_candidates(n, candidate)) {
				candidate = n;
			}
		}

		if(make_outgoing(mesh, candidate)) {
			return true;
		}
	}

	return false;
}

/* Disconnect from a random node that doesn't weaken the graph, and cancel redundant outgoings */
static void disconnect_redundant(meshlink_handle_t *mesh) {
	int count = 0;

	for list_each(connection_t, c, mesh->connections) {
		if(!c->status.active || !c->outgoing || !c->node) {
			continue;
		}

		if(c->node->edge_tree->count < 2) {
			continue;
		}

		count++;
	}

	if(!count) {
		return;
	}

	int r = rand() % count;

	for list_each(connection_t, c, mesh->connections) {
		if(!c->status.active || !c->outgoing || !c->node) {
			continue;
		}

		if(c->node->edge_tree->count < 2) {
			continue;
		}

		if(r--) {
			continue;
		}

		logger(mesh, MESHLINK_DEBUG, "Autodisconnecting from %s", c->name);
		list_delete(mesh->outgoings, c->outgoing);
		c->outgoing = NULL;
		terminate_connection(mesh, c, c->status.active);
		break;
	}

	for list_each(outgoing_t, o, mesh->outgoings) {
		if(!o->node->connection) {
			list_delete_node(mesh->outgoings, node);
		}
	}
}

static void heal_partitions(meshlink_handle_t *mesh) {
	/* Select a random known node. The rationale is that if there are many
	 * reachable nodes, and only a few unreachable nodes, we don't want all
	 * reachable nodes to try to connect to the unreachable ones at the
	 * same time. This way, we back off automatically. Conversely, if there
	 * are only a few reachable nodes, and many unreachable ones, we're
	 * going to try harder to connect to them. */

	int r = rand() % mesh->nodes->count;

	for splay_each(node_t, n, mesh->nodes) {
		if(r--) {
			continue;
		}

		if(n == mesh->self || n->connection || n->status.reachable || n->status.blacklisted) {
			return;
		}

		/* Are we already trying to make an outgoing connection to it? If so, return. */
		for list_each(outgoing_t, outgoing, mesh->outgoings) {
			if(outgoing->node == n) {
				return;
			}
		}

		make_outgoing(mesh, n);
		return;
	}

}

unsigned int do_autoconnect(meshlink_handle_t *mesh) {
	/* Count the number of active connections. */

	unsigned int cur_connects = 0;

	for list_each(connection_t, c, mesh->connections) {
		if(c->status.active) {
			cur_connects += 1;
		}
	}

	/* We don't have the minimum number of connections? Eagerly try to make a new one. */

	const unsigned int max_connects = mesh->dev_class_traits[mesh->devclass].max_connects;
	const unsigned int min_connects = mesh->dev_class_traits[mesh->devclass].min_connects;

	logger(mesh, MESHLINK_DEBUG, "do_autoconnect() %d %d %d\n", cur_connects, min_connects, max_connects);

	if(cur_connects < min_connects) {
		make_eager_connection(mesh);
	} else if(cur_connects < max_connects) {
		/* Otherwise, try to improve. */
		make_better_connection(mesh);
	}

	if (cur_connects >= max_connects) {
		disconnect_redundant(mesh);
	}

	heal_partitions(mesh);

	return cur_connects;
}
