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
#include "utcp.h"
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
	uint16_t want_udp: 1;               /* 1 if we want working UDP because we have data to send */
	uint16_t tiny: 1;                   /* 1 if this is a tiny node */
} node_status_t;

#define MAX_RECENT 5

typedef struct node_t {
	// Public member variables
	char *name;                             /* name of this node */
	void *priv;

	// Private member variables
	node_status_t status;
	uint16_t minmtu;                        /* Probed minimum MTU */
	dev_class_t devclass;

	// Used for packet I/O
	int sock;                               /* Socket to use for outgoing UDP packets */
	uint32_t session_id;                    /* Unique ID for this node's currently running process */
	sptps_t sptps;
	sockaddr_t address;                     /* his real (internet) ip to send UDP packets to */

	struct utcp *utcp;

	// Traffic counters
	uint64_t in_data;                       /* Bytes received from channels */
	uint64_t out_data;                      /* Bytes sent via channels */
	uint64_t in_forward;                    /* Bytes received for channels that need to be forwarded to other nodes */
	uint64_t out_forward;                   /* Bytes forwarded from channel from other nodes */
	uint64_t in_meta;                       /* Bytes received from meta-connections, heartbeat packets etc. */
	uint64_t out_meta;                      /* Bytes sent on meta-connections, heartbeat packets etc. */

	// MTU probes
	timeout_t mtutimeout;                   /* Probe event */
	int mtuprobes;                          /* Number of probes */
	uint16_t mtu;                           /* Maximum size of packets to send to this node */
	uint16_t maxmtu;                        /* Probed maximum MTU */

	// Used for meta-connection I/O, timeouts
	struct meshlink_handle *mesh;           /* The mesh this node belongs to */
	struct submesh_t *submesh;              /* Nodes Sub-Mesh Handle*/

	time_t last_req_key;

	struct ecdsa *ecdsa;                    /* His public ECDSA key */

	struct connection_t *connection;        /* Connection associated with this node (if a direct connection exists) */
	time_t last_connect_try;
	time_t last_successfull_connection;

	char *canonical_address;                /* The canonical address of this node, if known */
	char *external_ip_address;              /* The external IP address of this node, if known */
	sockaddr_t recent[MAX_RECENT];          /* Recently seen addresses */
	sockaddr_t catta_address;               /* Latest address seen by Catta */

	// Graph-related member variables
	time_t last_reachable;
	time_t last_unreachable;

	int distance;
	struct node_t *nexthop;                 /* nearest node from us to him */
	struct edge_t *prevedge;                /* nearest node from him to us */

	struct splay_tree_t *edge_tree;         /* Edges with this node as one of the endpoints */
} node_t;

void init_nodes(struct meshlink_handle *mesh);
void exit_nodes(struct meshlink_handle *mesh);
node_t *new_node(void) __attribute__((__malloc__));
void free_node(node_t *n);
void node_add(struct meshlink_handle *mesh, node_t *n);
void node_del(struct meshlink_handle *mesh, node_t *n);
node_t *lookup_node(struct meshlink_handle *mesh, const char *name) __attribute__((__warn_unused_result__));
node_t *lookup_node_udp(struct meshlink_handle *mesh, const sockaddr_t *sa) __attribute__((__warn_unused_result__));
void update_node_udp(struct meshlink_handle *mesh, node_t *n, const sockaddr_t *sa);
bool node_add_recent_address(struct meshlink_handle *mesh, node_t *n, const sockaddr_t *addr);

#endif
