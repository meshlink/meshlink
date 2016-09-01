/*
    net.h -- header for net.c
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

#ifndef __MESHLINK_NET_H__
#define __MESHLINK_NET_H__

#include "net_defines.h"
#include "event.h"
#include "sockaddr.h"
#include "conf.h"
#include "list.h"

typedef struct outgoing_t {
	char *name;
	int timeout;
	struct splay_tree_t *config_tree;
	struct config_t *cfg;
	struct addrinfo *ai; // addresses from config files
	struct addrinfo *aip;
	struct addrinfo *nai; // addresses known via other online nodes (use free_known_addresses())
	timeout_t ev;
	struct meshlink_handle *mesh;
} outgoing_t;

extern int maxoutbufsize;
extern int addressfamily;

extern int keylifetime;
extern int max_connection_burst;
extern bool do_prune;

/* Yes, very strange placement indeed, but otherwise the typedefs get all tangled up */
#include "connection.h"
#include "node.h"

extern void retry_outgoing(struct meshlink_handle *mesh, outgoing_t *);
extern bool handle_incoming_vpn_data(struct event_loop_t *loop, void *, int);
extern void finish_connecting(struct meshlink_handle *mesh, struct connection_t *);
extern bool do_outgoing_connection(struct meshlink_handle *mesh, struct outgoing_t *);
extern bool handle_new_meta_connection(struct event_loop_t *loop, void *, int);
extern int setup_listen_socket(struct meshlink_handle *mesh, const sockaddr_t *);
extern int setup_vpn_in_socket(struct meshlink_handle *mesh, const sockaddr_t *);
extern bool send_sptps_data(void *handle, uint8_t type, const void *data, size_t len);
extern bool receive_sptps_record(void *handle, uint8_t type, const void *data, uint16_t len);
extern bool send_packet(struct meshlink_handle *mesh, struct node_t *, struct vpn_packet_t *);
extern void receive_tcppacket(struct meshlink_handle *mesh, struct connection_t *, const char *, int);
extern void broadcast_packet(struct meshlink_handle *mesh, const struct node_t *, struct vpn_packet_t *);
extern char *get_name(struct meshlink_handle *mesh);
extern void load_all_nodes(struct meshlink_handle *mesh);
extern bool setup_myself_reloadable(struct meshlink_handle *mesh);
extern bool setup_network(struct meshlink_handle *mesh);
extern void setup_outgoing_connection(struct meshlink_handle *mesh, struct outgoing_t *);
extern void try_outgoing_connections(struct meshlink_handle *mesh);
extern void close_network_connections(struct meshlink_handle *mesh);
extern int main_loop(struct meshlink_handle *mesh);
extern void terminate_connection(struct meshlink_handle *mesh, struct connection_t *, bool);
extern bool node_read_ecdsa_public_key(struct meshlink_handle *mesh, struct node_t *);
extern bool read_ecdsa_public_key(struct meshlink_handle *mesh, struct connection_t *);
extern bool read_ecdsa_private_key(struct meshlink_handle *mesh);
extern void send_mtu_probe(struct meshlink_handle *mesh, struct node_t *);
extern void handle_meta_connection_data(struct meshlink_handle *mesh, struct connection_t *);
extern void retry(struct meshlink_handle *mesh);

#ifndef HAVE_MINGW
#define closesocket(s) close(s)
#else
extern CRITICAL_SECTION mutex;
#endif

#endif /* __MESHLINK_NET_H__ */
