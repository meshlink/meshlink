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
#include "xoshiro.h"

#include <pthread.h>

#define MAXSOCKETS 4    /* Probably overkill... */

static const char meshlink_invitation_label[] = "MeshLink invitation";
static const char meshlink_tcp_label[] = "MeshLink TCP";
static const char meshlink_udp_label[] = "MeshLink UDP";

#define MESHLINK_CONFIG_VERSION 2
#define MESHLINK_INVITATION_VERSION 2

struct CattaServer;
struct CattaSServiceBrowser;
struct CattaSimplePoll;
struct CattaSEntryGroup;

typedef struct listen_socket_t {
	struct io_t tcp;
	struct io_t udp;
	sockaddr_t sa;
	sockaddr_t broadcast_sa;
} listen_socket_t;

struct meshlink_open_params {
	char *confbase;
	char *appname;
	char *name;
	dev_class_t devclass;

	int netns;

	const void *key;
	size_t keylen;
};

/// Device class traits
typedef struct {
	int pinginterval;
	int pingtimeout;
	int fast_retry_period;
	unsigned int min_connects;
	unsigned int max_connects;
	int edge_weight;
} dev_class_traits_t;

/// A handle for an instance of MeshLink.
struct meshlink_handle {
	// public members
	char *name;
	void *priv;

	// private members
	pthread_mutex_t mutex;
	event_loop_t loop;
	struct node_t *self;
	meshlink_log_cb_t log_cb;
	meshlink_log_level_t log_level;
	void *packet;

	// The most important network-related members come first
	int reachable;
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
	time_t last_hard_try;
	time_t last_unreachable;
	timeout_t pingtimer;
	timeout_t periodictimer;

	struct connection_t *everyone;
	uint64_t prng_state[4];
	uint32_t session_id;

	int next_pit;
	int pits[10];

	// Infrequently used callbacks
	meshlink_node_status_cb_t node_status_cb;
	meshlink_node_pmtu_cb_t node_pmtu_cb;
	meshlink_channel_accept_cb_t channel_accept_cb;
	meshlink_node_duplicate_cb_t node_duplicate_cb;
	meshlink_connection_try_cb_t connection_try_cb;
	meshlink_error_cb_t error_cb;

	// Mesh parameters
	char *appname;
	char *myport;

	struct ecdsa *private_key;
	struct ecdsa *invitation_key;

	dev_class_t devclass;

	int invitation_timeout;
	int maxtimeout;
	int udp_choice;

	dev_class_traits_t dev_class_traits[DEV_CLASS_COUNT];

	int netns;

	bool default_blacklist;
	bool discovery;         // Whether Catta is enabled or not
	bool inviter_commits_first;

	// Configuration
	char *confbase;
	FILE *lockfile;
	void *config_key;
	char *external_address_url;
	struct list_t *invitation_addresses;

	// Thread management
	pthread_t thread;
	pthread_cond_t cond;
	pthread_mutex_t discovery_mutex;
	pthread_cond_t discovery_cond;
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

	// PFROUTE
	io_t pfroute_io;

	// ADNS
	pthread_t adns_thread;
	pthread_cond_t adns_cond;
	meshlink_queue_t adns_queue;
	meshlink_queue_t adns_done_queue;
	signal_t adns_signal;
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

/// An AIO buffer.
typedef struct meshlink_aio_buffer {
	const void *data;
	int fd;
	size_t len;
	size_t done;
	union {
		meshlink_aio_cb_t buffer;
		meshlink_aio_fd_cb_t fd;
	} cb;
	void *priv;
	struct meshlink_aio_buffer *next;
} meshlink_aio_buffer_t;

/// A channel.
struct meshlink_channel {
	struct node_t *node;
	void *priv;
	bool in_callback;

	struct utcp_connection *c;
	meshlink_aio_buffer_t *aio_send;
	meshlink_aio_buffer_t *aio_receive;
	meshlink_channel_receive_cb_t receive_cb;
	meshlink_channel_poll_cb_t poll_cb;
};

/// Header for data packets routed between nodes
typedef struct meshlink_packethdr {
	uint8_t destination[16];
	uint8_t source[16];
} __attribute__((__packed__)) meshlink_packethdr_t;

void meshlink_send_from_queue(event_loop_t *loop, void *mesh);
void update_node_status(meshlink_handle_t *mesh, struct node_t *n);
void update_node_pmtu(meshlink_handle_t *mesh, struct node_t *n);
extern meshlink_log_level_t global_log_level;
extern meshlink_log_cb_t global_log_cb;
void handle_duplicate_node(meshlink_handle_t *mesh, struct node_t *n);
void handle_network_change(meshlink_handle_t *mesh, bool online);
void call_error_cb(meshlink_handle_t *mesh, meshlink_errno_t meshlink_errno);

/// Per-instance PRNG
static inline int prng(meshlink_handle_t *mesh, uint64_t max) {
	return xoshiro(mesh->prng_state) % max;
}

/// Fudge value of ~0.1 seconds, in microseconds.
static const unsigned int TIMER_FUDGE = 0x8000000;

#endif
