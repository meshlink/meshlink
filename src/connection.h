#ifndef MESHLINK_CONNECTION_H
#define MESHLINK_CONNECTION_H

/*
    connection.h -- header for connection.c
    Copyright (C) 2000-2013, 2017 Guus Sliepen <guus@meshlink.io>

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

#include "buffer.h"
#include "list.h"
#include "sptps.h"

#define OPTION_INDIRECT         0x0001
#define OPTION_TCPONLY          0x0002
#define OPTION_PMTU_DISCOVERY   0x0004
#define OPTION_CLAMP_MSS        0x0008
#define OPTION_VERSION(x) ((x) >> 24) /* Top 8 bits are for protocol minor version */

typedef struct connection_status_t {
	uint16_t pinged: 1;                 /* sent ping */
	uint16_t active: 1;                 /* 1 if active.. */
	uint16_t connecting: 1;             /* 1 if we are waiting for a non-blocking connect() to finish */
	uint16_t unused: 1;
	uint16_t control: 1;                /* 1 if this is a control connection */
	uint16_t pcap: 1;                   /* 1 if this is a control connection requesting packet capture */
	uint16_t log: 1;                    /* 1 if this is a control connection requesting log dump */
	uint16_t invitation: 1;             /* 1 if this is an invitation */
	uint16_t invitation_used: 1;        /* 1 if the invitation has been consumed */
	uint16_t initiator: 1;              /* 1 if we initiated this connection */
} connection_status_t;

#include "ecdsa.h"
#include "edge.h"
#include "net.h"
#include "node.h"
#include "submesh.h"

typedef struct connection_t {
	char *name;                     /* name he claims to have */
	struct node_t *node;            /* node associated with the other end */

	connection_status_t status;     /* status info */
	int socket;                     /* socket used for this connection */
	union sockaddr_t address;       /* his real (internet) ip */

	struct meshlink_handle *mesh;   /* the mesh this connection belongs to */

	// I/O
	sptps_t sptps;
	struct buffer_t inbuf;
	struct buffer_t outbuf;
	io_t io;                        /* input/output event on this metadata connection */
	int tcplen;                     /* length of incoming TCPpacket */
	int allow_request;              /* defined if there's only one request possible */
	time_t last_ping_time;          /* last time we saw some activity from the other end or pinged them */
	time_t last_key_renewal;        /* last time we renewed the SPTPS key */

	struct outgoing_t *outgoing;    /* used to keep track of outgoing connections */

	struct edge_t *edge;            /* edge associated with this connection */
	struct submesh_t *submesh;      /* his submesh handle if available in invitation file */

	// Only used during authentication
	ecdsa_t *ecdsa;                 /* his public ECDSA key */
	int protocol_major;             /* used protocol */
	int protocol_minor;             /* used protocol */
} connection_t;

extern void init_connections(struct meshlink_handle *mesh);
extern void exit_connections(struct meshlink_handle *mesh);
extern connection_t *new_connection(void) __attribute__((__malloc__));
extern void free_connection(connection_t *);
extern void connection_add(struct meshlink_handle *mesh, connection_t *);
extern void connection_del(struct meshlink_handle *mesh, connection_t *);

#endif
