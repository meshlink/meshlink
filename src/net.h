#ifndef MESHLINK_NET_H
#define MESHLINK_NET_H

/*
    net.h -- header for net.c
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

/* Maximum size of SPTPS payload */
#ifdef ENABLE_JUMBOGRAMS
#define MTU 8951        /* 9000 bytes payload - 28 bytes IP+UDP header - 21 bytes SPTPS header+MAC */
#else
#define MTU 1451        /* 1500 bytes payload - 28 bytes IP+UDP - 21 bytes SPTPS header+MAC */
#endif

#define MINMTU 527      /* 576 minimum recommended Internet MTU - 28 bytes IP+UDP - 21 bytes SPTPS header+MAC */

/* MAXSIZE is the maximum size of an encapsulated packet */
#define MAXSIZE (MTU + 64)

/* MAXBUFSIZE is the maximum size of a request: enough for a base64 encoded MAXSIZEd packet plus request header */
#define MAXBUFSIZE ((MAXSIZE * 8) / 6 + 128)

typedef struct vpn_packet_t {
	uint16_t probe: 1;
	int16_t tcp: 1;
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
	struct node_t *node;
	enum {
		OUTGOING_START,
		OUTGOING_CANONICAL_RESOLVE,
		OUTGOING_CANONICAL,
		OUTGOING_RECENT,
		OUTGOING_KNOWN,
		OUTGOING_END,
		OUTGOING_NO_KNOWN_ADDRESSES,
	} state;
	int timeout;
	timeout_t ev;
	struct addrinfo *ai;
	struct addrinfo *aip;
} outgoing_t;

/* Yes, very strange placement indeed, but otherwise the typedefs get all tangled up */
#include "connection.h"
#include "node.h"

extern void init_outgoings(struct meshlink_handle *mesh);
extern void exit_outgoings(struct meshlink_handle *mesh);
#if defined(HAVE_RECVMMSG) || defined(HAVE_SENDMMSG)
extern void init_mmsg(struct meshlink_handle *mesh);
extern void exit_mmsg(struct meshlink_handle *mesh);
#endif
#ifdef HAVE_SENDMMSG
extern void flush_mmsg(struct meshlink_handle *mesh);
#endif

extern void retry_outgoing(struct meshlink_handle *mesh, outgoing_t *);
extern void handle_incoming_vpn_data(struct event_loop_t *loop, void *, int);
extern void finish_connecting(struct meshlink_handle *mesh, struct connection_t *);
extern void do_outgoing_connection(struct meshlink_handle *mesh, struct outgoing_t *);
extern void handle_new_meta_connection(struct event_loop_t *loop, void *, int);
extern int setup_tcp_listen_socket(struct meshlink_handle *mesh, const struct addrinfo *aip) __attribute__((__warn_unused_result__));
extern int setup_udp_listen_socket(struct meshlink_handle *mesh, const struct addrinfo *aip) __attribute__((__warn_unused_result__));
extern bool send_sptps_data(void *handle, uint8_t type, const void *data, size_t len);
extern bool receive_sptps_record(void *handle, uint8_t type, const void *data, uint16_t len) __attribute__((__warn_unused_result__));
extern void send_packet(struct meshlink_handle *mesh, struct node_t *, struct vpn_packet_t *);
extern char *get_name(struct meshlink_handle *mesh) __attribute__((__warn_unused_result__));
extern void load_all_nodes(struct meshlink_handle *mesh);
extern bool setup_myself_reloadable(struct meshlink_handle *mesh) __attribute__((__warn_unused_result__));
extern bool setup_network(struct meshlink_handle *mesh) __attribute__((__warn_unused_result__));
extern void reset_outgoing(struct outgoing_t *);
extern void setup_outgoing_connection(struct meshlink_handle *mesh, struct outgoing_t *);
extern void close_network_connections(struct meshlink_handle *mesh);
extern void main_loop(struct meshlink_handle *mesh);
extern void terminate_connection(struct meshlink_handle *mesh, struct connection_t *, bool);
extern bool node_read_public_key(struct meshlink_handle *mesh, struct node_t *) __attribute__((__warn_unused_result__));
extern bool node_read_from_config(struct meshlink_handle *mesh, struct node_t *, const config_t *config) __attribute__((__warn_unused_result__));
extern bool read_ecdsa_public_key(struct meshlink_handle *mesh, struct connection_t *) __attribute__((__warn_unused_result__));
extern bool read_ecdsa_private_key(struct meshlink_handle *mesh) __attribute__((__warn_unused_result__));
extern bool node_write_config(struct meshlink_handle *mesh, struct node_t *) __attribute__((__warn_unused_result__));
extern void send_mtu_probe(struct meshlink_handle *mesh, struct node_t *);
extern void handle_meta_connection_data(struct meshlink_handle *mesh, struct connection_t *);
extern void retry(struct meshlink_handle *mesh);
extern int check_port(struct meshlink_handle *mesh);

#ifndef HAVE_MINGW
#define closesocket(s) close(s)
#endif

#endif
