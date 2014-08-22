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
#include "meshlink.h"
#include "meshlink_queue.h"
#include "sockaddr.h"
#include "sptps.h"

#include <pthread.h>

#define MAXSOCKETS 8    /* Probably overkill... */

struct AvahiServer;
struct AvahiSServiceBrowser;
struct AvahiSimplePoll;
struct AvahiSEntryGroup;

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

typedef struct outpacketqueue {
	meshlink_node_t *destination;
	const void *data;
	unsigned int len;
} outpacketqueue_t;

/// A handle for an instance of MeshLink.
struct meshlink_handle {
	char *name;
	char *appname;
	dev_class_t devclass;
	void *priv;

	char *confbase;

	meshlink_receive_cb_t receive_cb;
	meshlink_node_status_cb_t node_status_cb;
	meshlink_log_cb_t log_cb;
	meshlink_log_level_t log_level;

	meshlink_channel_accept_cb_t channel_accept_cb;

	pthread_t thread;
	bool threadstarted;
	pthread_mutex_t outpacketqueue_mutex;
	pthread_mutex_t mesh_mutex;
	event_loop_t loop;
	listen_socket_t listen_socket[MAXSOCKETS];
	int listen_sockets;
	signal_t datafromapp;

	struct node_t *self;

	struct splay_tree_t *config;
	struct splay_tree_t *edges;
	struct splay_tree_t *nodes;

	struct list_t *connections;
	struct list_t *outgoings;

	meshlink_queue_t outpacketqueue;

	struct splay_tree_t *past_request_tree;
	timeout_t past_request_timeout;

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

	pthread_t discovery_thread;
	bool discovery_threadstarted;
	struct AvahiServer *avahi_server;
	struct AvahiSServiceBrowser *avahi_browser;
	struct AvahiSimplePoll *avahi_poll;
	struct AvahiSEntryGroup *avahi_group;
	char* avahi_servicetype;
};

/// A handle for a MeshLink node.
struct meshlink_node {
	const char *name;
	void *priv;
};

/// A channel.
struct meshlink_channel {
	struct utcp_connection *c;
	struct node_t *node;
	meshlink_channel_receive_cb_t receive_cb;
};

/// Header for data packets routed between nodes
typedef struct meshlink_packethdr {
	uint8_t destination[16];
	uint8_t source[16];
} __attribute__ ((__packed__)) meshlink_packethdr_t;

extern void meshlink_send_from_queue(event_loop_t* el,meshlink_handle_t *mesh);
extern meshlink_log_level_t global_log_level;
extern meshlink_log_cb_t global_log_cb;

/// Device class traits
typedef struct {
	unsigned int min_connects;
	unsigned int max_connects;
	int edge_weight;
} dev_class_traits_t;

extern dev_class_traits_t dev_class_traits[];

#endif // MESHLINK_INTERNAL_H
