/*
    utcp.h -- Userspace TCP
    Copyright (C) 2014 Guus Sliepen <guus@tinc-vpn.org>

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

#ifndef UTCP_PRIV_H
#define UTCP_PRIV_H

#define UTCP_INTERNAL
#include "utcp.h"

#define PREP(l) char pkt[(l) + sizeof struct hdr]; struct hdr *hdr = &pkt;

#define SYN 1
#define ACK 2
#define FIN 4
#define RST 8
#define MF 16

#define AUX_INIT 1
#define AUX_FRAME 2
#define AUX_SAK 3
#define AUX_TIMESTAMP 4

#define NSACKS 4
#define DEFAULT_SNDBUFSIZE 4096
#define DEFAULT_MAXSNDBUFSIZE 131072
#define DEFAULT_RCVBUFSIZE 0
#define DEFAULT_MAXRCVBUFSIZE 131072

#define MAX_UNRELIABLE_SIZE 65536
#define DEFAULT_MTU 1000

#define USEC_PER_SEC 1000000L
#define NSEC_PER_SEC 1000000000L
#define DEFAULT_USER_TIMEOUT 60
#define START_RTO (1 * USEC_PER_SEC)
#define MAX_RTO (3 * USEC_PER_SEC)

struct hdr {
	uint16_t src; // Source port
	uint16_t dst; // Destination port
	uint32_t seq; // Sequence number
	uint32_t ack; // Acknowledgement number
	uint32_t wnd; // Window size
	uint16_t ctl; // Flags (SYN, ACK, FIN, RST)
	uint16_t aux; // other stuff
};

enum state {
	CLOSED,
	LISTEN,
	SYN_SENT,
	SYN_RECEIVED,
	ESTABLISHED,
	FIN_WAIT_1,
	FIN_WAIT_2,
	CLOSE_WAIT,
	CLOSING,
	LAST_ACK,
	TIME_WAIT
};

static const char *strstate[] __attribute__((unused)) = {
	[CLOSED] = "CLOSED",
	[LISTEN] = "LISTEN",
	[SYN_SENT] = "SYN_SENT",
	[SYN_RECEIVED] = "SYN_RECEIVED",
	[ESTABLISHED] = "ESTABLISHED",
	[FIN_WAIT_1] = "FIN_WAIT_1",
	[FIN_WAIT_2] = "FIN_WAIT_2",
	[CLOSE_WAIT] = "CLOSE_WAIT",
	[CLOSING] = "CLOSING",
	[LAST_ACK] = "LAST_ACK",
	[TIME_WAIT] = "TIME_WAIT"
};

struct buffer {
	char *data;
	uint32_t offset;
	uint32_t used;
	uint32_t size;
	uint32_t maxsize;
	bool external;
};

struct sack {
	uint32_t offset;
	uint32_t len;
};

struct utcp_connection {
	void *priv;
	struct utcp *utcp;
	uint32_t flags;

	bool reapable;
	bool do_poll;

	// Callbacks

	utcp_recv_t recv;
	utcp_poll_t poll;

	// TCP State

	uint16_t src;
	uint16_t dst;
	enum state state;

	struct {
		uint32_t una;
		uint32_t nxt;
		uint32_t wnd;
		uint32_t iss;

		uint32_t last;
		uint32_t cwnd;
		uint32_t ssthresh;
	} snd;

	struct {
		uint32_t nxt;
		uint32_t irs;
	} rcv;

	int dupack;

	// Timers

	struct timespec conn_timeout;
	struct timespec rtrx_timeout;
	struct timespec rtt_start;
	uint32_t rtt_seq;

	// RTT variables

	uint32_t srtt; // usec
	uint32_t rttvar; // usec
	uint32_t rto; // usec

	// Buffers

	uint32_t prev_free;
	struct buffer sndbuf;
	struct buffer rcvbuf;
	struct sack sacks[NSACKS];

	// Per-socket options

	bool nodelay;
	bool keepalive;
	bool shut_wr;

	// Congestion avoidance state

	struct timespec tlast;
	uint64_t bandwidth;
};

struct utcp {
	void *priv;

	// Callbacks

	utcp_accept_t accept;
	utcp_listen_t listen;
	utcp_retransmit_t retransmit;
	utcp_send_t send;

	// Packet buffer

	void *pkt;

	// Global socket options

	uint16_t mtu; // The maximum size of a UTCP packet, including headers.
	uint16_t mss; // The maximum size of the payload of a UTCP packet.
	int timeout; // sec

	// Connection management

	struct utcp_connection **connections;
	int nconnections;
	int nallocated;
};

#endif
