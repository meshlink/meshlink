#ifndef MESHLINK_NODE_H
#define MESHLINK_NODE_H

/*
    node.h -- header for node.c
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

#include "event.h"
#include "sockaddr.h"
#include "sptps.h"
#include "utcp/utcp.h"
#include "submesh.h"

typedef struct node_status_t {
	uint16_t validkey: 1;               /* 1 if we currently have a valid key for him */
	uint16_t waitingforkey: 1;          /* 1 if we already sent out a request */
	uint16_t visited: 1;                /* 1 if this node has been visited by one of the graph algorithms */
	uint16_t reachable: 1;              /* 1 if this node is reachable in the graph */
	uint16_t udp_confirmed: 1;          /* 1 if the address is one that we received UDP traffic on */
	uint16_t broadcast: 1;              /* 1 if the next UDP packet should be broadcast to the local network */
	uint16_t blacklisted: 1;            /* 1 if the node is blacklist so we never want to speak with him anymore */
	uint16_t destroyed: 1;              /* 1 if the node is being destroyed, deallocate channels when any callback is triggered */
	uint16_t duplicate: 1;              /* 1 if the node is duplicate, ie. multiple nodes using the same Name are online */
	uint16_t dirty: 1;                  /* 1 if the configuration of the node is dirty and needs to be written out */
} node_status_t;

typedef struct node_t {
	// Public member variables
	char *name;                             /* name of this node */
	void *priv;

	// Private member variables
	node_status_t status;
	uint16_t minmtu;                        /* Probed minimum MTU */
	dev_class_t devclass;

	// Used for packet I/O
	sptps_t sptps;
	int sock;                               /* Socket to use for outgoing UDP packets */
	sockaddr_t address;                     /* his real (internet) ip to send UDP packets to */

	struct utcp *utcp;

	// Traffic counters
	uint64_t in_packets;
	uint64_t in_bytes;
	uint64_t out_packets;
	uint64_t out_bytes;

	// MTU probes
	timeout_t mtutimeout;                   /* Probe event */
	int mtuprobes;                          /* Number of probes */
	uint16_t mtu;                           /* Maximum size of packets to send to this node */
	uint16_t maxmtu;                        /* Probed maximum MTU */

	// Used for meta-connection I/O, timeouts
	struct meshlink_handle *mesh;           /* The mesh this node belongs to */
	struct submesh_t *submesh;              /* Nodes Sub-Mesh Handle*/

	time_t last_state_change;
	time_t last_req_key;

	struct ecdsa *ecdsa;                    /* His public ECDSA key */

	struct connection_t *connection;        /* Connection associated with this node (if a direct connection exists) */
	time_t last_connect_try;
	time_t last_successful_connection;

	char *canonical_address;                /* The canonical address of this node, if known */
	sockaddr_t recent[5];                   /* Recently seen addresses */

	// Graph-related member variables
	int distance;
	struct node_t *nexthop;                 /* nearest node from us to him */
	struct edge_t *prevedge;                /* nearest node from him to us */

	struct splay_tree_t *edge_tree;         /* Edges with this node as one of the endpoints */
} node_t;

extern void init_nodes(struct meshlink_handle *mesh);
extern void exit_nodes(struct meshlink_handle *mesh);
extern node_t *new_node(void) __attribute__((__malloc__));
extern void free_node(node_t *n);
extern void node_add(struct meshlink_handle *mesh, node_t *n);
extern void node_del(struct meshlink_handle *mesh, node_t *n);
extern node_t *lookup_node(struct meshlink_handle *mesh, const char *name);
extern node_t *lookup_node_udp(struct meshlink_handle *mesh, const sockaddr_t *sa);
extern void update_node_udp(struct meshlink_handle *mesh, node_t *n, const sockaddr_t *sa);

#endif
