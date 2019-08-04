#ifndef MESHLINK_INTERNAL_H
#define MESHLINK_INTERNAL_H

/*
    meshlink_internal.h -- Internal parts of the public API.
    Copyright (C) 2014-2019 Guus Sliepen <guus@meshlink.io>

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

#ifdef MESHLINK_H
#error You must not include both meshlink.h and meshlink_internal.h!
#endif

#include "system.h"

#include "event.h"
#include "hash.h"
#include "meshlink.h"
#include "meshlink_queue.h"
#include "sockaddr.h"
#include "sptps.h"

#include <pthread.h>

#define MAXSOCKETS 4    /* Probably overkill... */

static const char meshlink_invitation_label[] = "MeshLink invitation";
static const char meshlink_tcp_label[] = "MeshLink TCP";
static const char meshlink_udp_label[] = "MeshLink UDP";

#define MESHLINK_CONFIG_VERSION 1
#define MESHLINK_INVITATION_VERSION 1

struct CattaServer;
struct CattaSServiceBrowser;
struct CattaSimplePoll;
struct CattaSEntryGroup;

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
} proxytype_t;

struct meshlink_open_params {
	char *confbase;
	char *appname;
	char *name;
	dev_class_t devclass;

	int netns;

	const void *key;
	size_t keylen;
};

/// A handle for an instance of MeshLink.
struct meshlink_handle {
	// public members
	char *name;
	void *priv;

	// private members
	pthread_mutex_t mesh_mutex;
	event_loop_t loop;
	struct node_t *self;
	meshlink_log_cb_t log_cb;
	meshlink_log_level_t log_level;

	// The most important network-related members come first
	int listen_sockets;
	listen_socket_t listen_socket[MAXSOCKETS];

	meshlink_receive_cb_t receive_cb;
	meshlink_queue_t outpacketqueue;
	signal_t datafromapp;

	hash_t *node_udp_cache;

	struct splay_tree_t *nodes;
	struct splay_tree_t *edges;

	struct list_t *connections;
	struct list_t *outgoings;
	struct list_t *submeshes;

	// Meta-connection-related members
	struct splay_tree_t *past_request_tree;
	timeout_t past_request_timeout;

	int connection_burst;
	int contradicting_add_edge;
	int contradicting_del_edge;
	int sleeptime;
	time_t connection_burst_time;
	time_t last_config_check;
	time_t last_hard_try;
	timeout_t pingtimer;
	timeout_t periodictimer;

	struct connection_t *everyone;

	int next_pit;
	int pits[10];

	// Infrequently used callbacks
	meshlink_node_status_cb_t node_status_cb;
	meshlink_channel_accept_cb_t channel_accept_cb;
	meshlink_node_duplicate_cb_t node_duplicate_cb;
	meshlink_connection_try_cb_t connection_try_cb;

	// Mesh parameters
	char *appname;
	char *myport;

	struct ecdsa *private_key;
	struct ecdsa *invitation_key;

	int32_t devclass;

	int invitation_timeout;
	int pinginterval;       /* seconds between pings */
	int pingtimeout;        /* seconds to wait for response */
	int maxtimeout;
	int udp_choice;

	int netns;

	bool default_blacklist;
	bool discovery;         // Whether Catta is enabled or not


	// Configuration
	char *confbase;
	FILE *conffile;
	void *config_key;

	// Thread management
	pthread_t thread;
	bool threadstarted;
	bool discovery_threadstarted;

	// Catta
	pthread_t discovery_thread;
	struct CattaServer *catta_server;
	struct CattaSServiceBrowser *catta_browser;
	struct CattaSimplePoll *catta_poll;
	struct CattaSEntryGroup *catta_group;
	char *catta_servicetype;
	unsigned int catta_interfaces;

	// State used for meshlink_join()
	int sock;
	char cookie[18], hash[18];
	bool success;
	sptps_t sptps;
	char *data;
	size_t thedatalen;
	size_t blen;
	char line[4096];
	char buffer[4096];

	// Proxy configuration, currently not exposed.
	char *proxyhost;
	char *proxyport;
	char *proxyuser;
	char *proxypass;
	proxytype_t proxytype;
};

/// A handle for a MeshLink node.
struct meshlink_node {
	const char *name;
	void *priv;
};

/// A handle for a node Sub-Mesh.
struct meshlink_submesh {
	const char *name;
	void *priv;
};

/// A channel.
struct meshlink_channel {
	struct node_t *node;
	void *priv;

	struct utcp_connection *c;
	meshlink_channel_receive_cb_t receive_cb;
	meshlink_channel_poll_cb_t poll_cb;
};

/// Header for data packets routed between nodes
typedef struct meshlink_packethdr {
	uint8_t destination[16];
	uint8_t source[16];
} __attribute__((__packed__)) meshlink_packethdr_t;

extern void meshlink_send_from_queue(event_loop_t *el, meshlink_handle_t *mesh);
extern void update_node_status(meshlink_handle_t *mesh, struct node_t *n);
extern meshlink_log_level_t global_log_level;
extern meshlink_log_cb_t global_log_cb;
extern int check_port(meshlink_handle_t *mesh);
extern void handle_duplicate_node(meshlink_handle_t *mesh, struct node_t *n);
extern void handle_network_change(meshlink_handle_t *mesh, bool online);

/// Device class traits
typedef struct {
	unsigned int min_connects;
	unsigned int max_connects;
	int edge_weight;
} dev_class_traits_t;

extern const dev_class_traits_t dev_class_traits[];

#endif
