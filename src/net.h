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

#ifndef __TINC_NET_H__
#define __TINC_NET_H__

#include "event.h"
#include "sockaddr.h"

#ifdef ENABLE_JUMBOGRAMS
#define MTU 9018        /* 9000 bytes payload + 14 bytes ethernet header + 4 bytes VLAN tag */
#else
#define MTU 1518        /* 1500 bytes payload + 14 bytes ethernet header + 4 bytes VLAN tag */
#endif

/* MAXSIZE is the maximum size of an encapsulated packet: MTU + seqno + HMAC + compressor overhead */
#define MAXSIZE (MTU + 4 + 32 + MTU/64 + 20)

/* MAXBUFSIZE is the maximum size of a request: enough for a MAXSIZEd packet or a 8192 bits RSA key */
#define MAXBUFSIZE ((MAXSIZE > 2048 ? MAXSIZE : 2048) + 128)

typedef struct vpn_packet_t {
	struct {
		unsigned int probe:1;
		unsigned int tcp:1;
	};
	uint16_t len;           /* the actual number of bytes in the `data' field */
	uint8_t data[MAXSIZE];
} vpn_packet_t;

/* Packet types when using SPTPS */

#define PKT_COMPRESSED 1
#define PKT_PROBE 4

typedef enum packet_type_t {
	PACKET_NORMAL,
	PACKET_COMPRESSED,
	PACKET_PROBE
} packet_type_t;

#include "conf.h"
#include "list.h"

typedef struct outgoing_t {
	char *name;
	int timeout;
	struct splay_tree_t *config_tree;
	struct config_t *cfg;
	struct addrinfo *ai;
	struct addrinfo *aip;
	timeout_t ev;
} outgoing_t;

extern int maxoutbufsize;
extern int addressfamily;
extern unsigned replaywin;

extern int keylifetime;
extern int max_connection_burst;
extern bool do_prune;
extern int autoconnect;

/* Yes, very strange placement indeed, but otherwise the typedefs get all tangled up */
#include "connection.h"
#include "node.h"

extern void retry_outgoing(outgoing_t *);
extern void handle_incoming_vpn_data(struct event_loop_t *loop, void *, int);
extern void finish_connecting(struct connection_t *);
extern bool do_outgoing_connection(struct outgoing_t *);
extern void handle_new_meta_connection(struct event_loop_t *loop, void *, int);
extern int setup_listen_socket(const sockaddr_t *);
extern int setup_vpn_in_socket(const sockaddr_t *);
extern bool send_sptps_data(void *handle, uint8_t type, const char *data, size_t len);
extern bool receive_sptps_record(void *handle, uint8_t type, const char *data, uint16_t len);
extern void send_packet(struct node_t *, struct vpn_packet_t *);
extern void receive_tcppacket(struct connection_t *, const char *, int);
extern void broadcast_packet(const struct node_t *, struct vpn_packet_t *);
extern char *get_name(struct meshlink_handle *mesh);
extern bool setup_myself_reloadable(struct meshlink_handle *mesh);
extern bool setup_network(struct meshlink_handle *mesh);
extern void setup_outgoing_connection(struct outgoing_t *);
extern void try_outgoing_connections(void);
extern void close_network_connections(void);
extern int main_loop(void);
extern void terminate_connection(struct connection_t *, bool);
extern bool node_read_ecdsa_public_key(struct meshlink_handle *mesh, struct node_t *);
extern bool read_ecdsa_public_key(struct meshlink_handle *mesh, struct connection_t *);
extern void send_mtu_probe(struct node_t *);
extern void handle_meta_connection_data(struct connection_t *);
extern void regenerate_key(void);
extern void purge(void);
extern void retry(void);
extern int reload_configuration(void);

#ifndef HAVE_MINGW
#define closesocket(s) close(s)
#else
extern CRITICAL_SECTION mutex;
#endif

#endif /* __TINC_NET_H__ */
