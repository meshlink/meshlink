/*
    meshlink_internal.h -- Internal parts of the public API.
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

#ifndef MESHLINK_INTERNAL_H
#define MESHLINK_INTERNAL_H

#include "system.h"

#include "event.h"
#include "hash.h"
#include "logger.h"
#include "meshlink.h"
#include "sockaddr.h"
#include "sptps.h"

#define MAXSOCKETS 8    /* Probably overkill... */

typedef struct listen_socket_t {
	struct io_t tcp;
	struct io_t udp;
	sockaddr_t sa;
	bool bindto;
} listen_socket_t;

typedef enum proxytype_t {
	PROXY_NONE = 0,
	PROXY_SOCKS4,
	PROXY_SOCKS4A,
	PROXY_SOCKS5,
	PROXY_HTTP,
	PROXY_EXEC,
} proxytype_t;

/// A handle for an instance of MeshLink.
struct meshlink_handle {
	char *confbase;
	char *name;

	meshlink_receive_cb_t receive_cb;
	meshlink_node_status_cb_t node_status_cb;
	meshlink_log_cb_t log_cb;
	meshlink_log_level_t log_level;

	pthread_t thread;
	event_loop_t loop;
	listen_socket_t listen_socket[MAXSOCKETS];
	int listen_sockets;

	struct node_t *self;

	struct splay_tree_t *config;
	struct splay_tree_t *edges;
	struct splay_tree_t *nodes;

	struct list_t *connections;
	struct list_t *outgoings;

	int contradicting_add_edge;
	int contradicting_del_edge;
	int sleeptime;
	time_t last_config_check;
	timeout_t pingtimer;
	timeout_t periodictimer;

	char *myport;

	char *proxyhost;
	char *proxyport;
	char *proxyuser;
	char *proxypass;
	proxytype_t proxytype;

	bool localdiscovery;
	sockaddr_t localdiscovery_address;

	hash_t *node_udp_cache;
	struct connection_t *everyone;
	struct ecdsa *invitation_key;

	debug_t debug_level;
	int pinginterval;	/* seconds between pings */
	int pingtimeout;	/* seconds to wait for response */
	int maxtimeout;

	int sock;
	sptps_t sptps;
	char cookie[18], hash[18];
	char *data;
	size_t thedatalen;
	bool success;
	char line[4096];
	char buffer[4096];
	size_t blen;
};

/// A handle for a MeshLink node.
struct meshlink_node {
	const char *name;
	void *priv;
};

/// Header for data packets routed between nodes
typedef struct meshlink_packethdr {
	uint8_t destination[16];
	uint8_t source[16];
} __attribute__ ((__packed__)) meshlink_packethdr_t;

#endif // MESHLINK_INTERNAL_H
