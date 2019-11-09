#ifndef MESHLINK_PROTOCOL_H
#define MESHLINK_PROTOCOL_H

/*
    protocol.h -- header for protocol.c
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

#include "ecdsa.h"

/* Protocol version. Different major versions are incompatible. */

#define PROT_MAJOR 17
#define PROT_MINOR 3 /* Should not exceed 255! */

/* Silly Windows */

#ifdef ERROR
#undef ERROR
#endif

/* Request numbers */

typedef enum request_t {
	ALL = -1,                                       /* Guardian for allow_request */
	ID = 0, METAKEY, CHALLENGE, CHAL_REPLY, ACK,
	STATUS, ERROR, TERMREQ,
	PING, PONG,
	ADD_SUBNET, DEL_SUBNET,
	ADD_EDGE, DEL_EDGE,
	KEY_CHANGED, REQ_KEY, ANS_KEY,
	PACKET,
	/* Tinc 1.1 requests */
	CONTROL,
	REQ_PUBKEY, ANS_PUBKEY,
	REQ_SPTPS,
	LAST                                            /* Guardian for the highest request number */
} request_t;

typedef struct past_request_t {
	const char *request;
	time_t firstseen;
} past_request_t;

/* Maximum size of strings in a request.
 * scanf terminates %2048s with a NUL character,
 * but the NUL character can be written after the 2048th non-NUL character.
 */

#define MAX_STRING_SIZE 2049
#define MAX_STRING "%2048s"

#include "edge.h"
#include "net.h"
#include "node.h"

/* Basic functions */

extern bool send_request(struct meshlink_handle *mesh, struct connection_t *, struct submesh_t *s, const char *, ...) __attribute__((__format__(printf, 4, 5)));
extern void forward_request(struct meshlink_handle *mesh, struct connection_t *, struct submesh_t *, const char *);
extern bool receive_request(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool check_id(const char *);

extern void init_requests(struct meshlink_handle *mesh);
extern void exit_requests(struct meshlink_handle *mesh);
extern bool seen_request(struct meshlink_handle *mesh, const char *);

/* Requests */

extern bool send_id(struct meshlink_handle *mesh, struct connection_t *);
extern bool send_ack(struct meshlink_handle *mesh, struct connection_t *);
extern bool send_ping(struct meshlink_handle *mesh, struct connection_t *);
extern bool send_pong(struct meshlink_handle *mesh, struct connection_t *);
extern bool send_add_edge(struct meshlink_handle *mesh, struct connection_t *, const struct edge_t *, int contradictions);
extern bool send_del_edge(struct meshlink_handle *mesh, struct connection_t *, const struct edge_t *, int contradictions);
extern bool send_req_key(struct meshlink_handle *mesh, struct node_t *);

/* Request handlers  */

extern bool id_h(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool ack_h(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool status_h(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool error_h(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool termreq_h(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool ping_h(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool pong_h(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool add_edge_h(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool del_edge_h(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool key_changed_h(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool req_key_h(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool ans_key_h(struct meshlink_handle *mesh, struct connection_t *, const char *);
extern bool tcppacket_h(struct meshlink_handle *mesh, struct connection_t *, const char *);

#endif
