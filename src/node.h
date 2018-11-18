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

typedef struct node_status_t {
	unsigned int unused_active: 1;          /* 1 if active (not used for nodes) */
	unsigned int validkey: 1;               /* 1 if we currently have a valid key for him */
	unsigned int waitingforkey: 1;          /* 1 if we already sent out a request */
	unsigned int visited: 1;                /* 1 if this node has been visited by one of the graph algorithms */
	unsigned int reachable: 1;              /* 1 if this node is reachable in the graph */
	unsigned int indirect: 1;               /* 1 if this node is not directly reachable by us */
	unsigned int unused_sptps: 1;           /* 1 if this node supports SPTPS */
	unsigned int udp_confirmed: 1;          /* 1 if the address is one that we received UDP traffic on */
	unsigned int broadcast: 1;              /* 1 if the next UDP packet should be broadcast to the local network */
	unsigned int blacklisted: 1;            /* 1 if the node is blacklist so we never want to speak with him anymore */
	unsigned int destroyed: 1;              /* 1 if the node is being destroyed, deallocate channels when any callback is triggered */
	unsigned int duplicate: 1;              /* 1 if the node is duplicate, ie. multiple nodes using the same Name are online */
	unsigned int unused: 20;
} node_status_t;

typedef struct node_t {
	char *name;                             /* name of this node */
	void *priv;

	uint32_t options;                       /* options turned on for this node */
	dev_class_t devclass;

	struct meshlink_handle *mesh;           /* The mesh this node belongs to */

	int sock;                               /* Socket to use for outgoing UDP packets */
	sockaddr_t address;                     /* his real (internet) ip to send UDP packets to */

	uint64_t id;                            /* Unique ID for this node */

	node_status_t status;
	time_t last_state_change;
	time_t last_req_key;

	struct ecdsa *ecdsa;                    /* His public ECDSA key */
	sptps_t sptps;

	int incompression;                      /* Compressionlevel, 0 = no compression */
	int outcompression;                     /* Compressionlevel, 0 = no compression */

	int distance;
	struct node_t *nexthop;                 /* nearest node from us to him */
	struct edge_t *prevedge;                /* nearest node from him to us */
	struct node_t *via;                     /* next hop for UDP packets */

	struct splay_tree_t *edge_tree;                /* Edges with this node as one of the endpoints */

	struct connection_t *connection;        /* Connection associated with this node (if a direct connection exists) */
	time_t last_connect_try;
	time_t last_successfull_connection;

	uint16_t mtu;                           /* Maximum size of packets to send to this node */
	uint16_t minmtu;                        /* Probed minimum MTU */
	uint16_t maxmtu;                        /* Probed maximum MTU */
	int mtuprobes;                          /* Number of probes */
	timeout_t mtutimeout;                   /* Probe event */

	struct utcp *utcp;

	uint64_t in_packets;
	uint64_t in_bytes;
	uint64_t out_packets;
	uint64_t out_bytes;
} node_t;

extern void init_nodes(struct meshlink_handle *mesh);
extern void exit_nodes(struct meshlink_handle *mesh);
extern node_t *new_node(void) __attribute__((__malloc__));
extern void free_node(node_t *);
extern void node_add(struct meshlink_handle *mesh, node_t *);
extern void node_del(struct meshlink_handle *mesh, node_t *);
extern node_t *lookup_node(struct meshlink_handle *mesh, const char *);
extern node_t *lookup_node_id(struct meshlink_handle *mesh, uint64_t id);
extern void update_node_udp(struct meshlink_handle *mesh, node_t *, const sockaddr_t *);
extern void update_node_id(struct meshlink_handle *mesh, node_t *);

#endif
