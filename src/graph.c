/*
    graph.c -- graph algorithms
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

/* We need to generate two trees from the graph:

   1. A minimum spanning tree for broadcasts,
   2. A single-source shortest path tree for unicasts.

   Actually, the first one alone would suffice but would make unicast packets
   take longer routes than necessary.

   For the MST algorithm we can choose from Prim's or Kruskal's. I personally
   favour Kruskal's, because we make an extra AVL tree of edges sorted on
   weights (metric). That tree only has to be updated when an edge is added or
   removed, and during the MST algorithm we just have go linearly through that
   tree, adding safe edges until #edges = #nodes - 1. The implementation here
   however is not so fast, because I tried to avoid having to make a forest and
   merge trees.

   For the SSSP algorithm Dijkstra's seems to be a nice choice. Currently a
   simple breadth-first search is presented here.

   The SSSP algorithm will also be used to determine whether nodes are directly,
   indirectly or not reachable from the source. It will also set the correct
   destination address and port of a node if possible.
*/

#include "system.h"

#include "connection.h"
#include "edge.h"
#include "graph.h"
#include "list.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "netutl.h"
#include "node.h"
#include "protocol.h"
#include "utils.h"
#include "xalloc.h"
#include "graph.h"

/* Implementation of a simple breadth-first search algorithm.
   Running time: O(E)
*/

static void sssp_bfs(meshlink_handle_t *mesh) {
	list_t *todo_list = list_alloc(NULL);

	/* Clear visited status on nodes */

	for splay_each(node_t, n, mesh->nodes) {
		n->status.visited = false;
		n->status.indirect = true;
		n->distance = -1;
	}

	/* Begin with mesh->self */

	mesh->self->status.visited = true;
	mesh->self->status.indirect = false;
	mesh->self->nexthop = mesh->self;
	mesh->self->prevedge = NULL;
	mesh->self->via = mesh->self;
	mesh->self->distance = 0;
	list_insert_head(todo_list, mesh->self);

	/* Loop while todo_list is filled */

	for list_each(node_t, n, todo_list) {                   /* "n" is the node from which we start */
		logger(mesh, MESHLINK_DEBUG, " Examining edges from %s", n->name);

		if(n->distance < 0) {
			abort();
		}

		for splay_each(edge_t, e, n->edge_tree) {       /* "e" is the edge connected to "from" */
			if(!e->reverse) {
				continue;
			}

			/* Situation:

			           /
			          /
			   ----->(n)---e-->(e->to)
			          \
			           \

			   Where e is an edge, (n) and (e->to) are nodes.
			   n->address is set to the e->address of the edge left of n to n.
			   We are currently examining the edge e right of n from n:

			   - If edge e provides for better reachability of e->to, update
			     e->to and (re)add it to the todo_list to (re)examine the reachability
			     of nodes behind it.
			 */

			bool indirect = n->status.indirect || e->options & OPTION_INDIRECT;

			if(e->to->status.visited
			                && (!e->to->status.indirect || indirect)
			                && (e->to->distance != n->distance + 1 || e->weight >= e->to->prevedge->weight)) {
				continue;
			}

			e->to->status.visited = true;
			e->to->status.indirect = indirect;
			e->to->nexthop = (n->nexthop == mesh->self) ? e->to : n->nexthop;
			e->to->prevedge = e;
			e->to->via = indirect ? n->via : e->to;
			e->to->options = e->options;
			e->to->distance = n->distance + 1;

			if(!e->to->status.reachable || (e->to->address.sa.sa_family == AF_UNSPEC && e->address.sa.sa_family != AF_UNKNOWN)) {
				update_node_udp(mesh, e->to, &e->address);
			}

			list_insert_tail(todo_list, e->to);
		}

		next = node->next; /* Because the list_insert_tail() above could have added something extra for us! */
		list_delete_node(todo_list, node);
	}

	list_free(todo_list);
}

static void check_reachability(meshlink_handle_t *mesh) {
	/* Check reachability status. */

	for splay_each(node_t, n, mesh->nodes) {
		if(n->status.visited != n->status.reachable) {
			n->status.reachable = !n->status.reachable;
			n->last_state_change = mesh->loop.now.tv_sec;

			if(n->status.reachable) {
				logger(mesh, MESHLINK_DEBUG, "Node %s became reachable", n->name);
			} else {
				logger(mesh, MESHLINK_DEBUG, "Node %s became unreachable", n->name);
			}

			/* TODO: only clear status.validkey if node is unreachable? */

			n->status.validkey = false;
			sptps_stop(&n->sptps);
			n->status.waitingforkey = false;
			n->last_req_key = 0;

			n->status.udp_confirmed = false;
			n->maxmtu = MTU;
			n->minmtu = 0;
			n->mtuprobes = 0;

			timeout_del(&mesh->loop, &n->mtutimeout);

			if(!n->status.blacklisted) {
				update_node_status(mesh, n);
			}

			if(!n->status.reachable) {
				update_node_udp(mesh, n, NULL);
				n->status.broadcast = false;
				n->options = 0;
			} else if(n->connection) {
				if(n->connection->outgoing) {
					send_req_key(mesh, n);
				}
			}
		}
	}
}

void graph(meshlink_handle_t *mesh) {
	sssp_bfs(mesh);
	check_reachability(mesh);
}
