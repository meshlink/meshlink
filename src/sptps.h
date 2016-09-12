/*
    sptps.h -- Simple Peer-to-Peer Security
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

#ifndef __SPTPS_H__
#define __SPTPS_H__

#include "system.h"

#include "chacha-poly1305/chacha-poly1305.h"
#include "ecdh.h"
#include "ecdsa.h"
#include "net_defines.h"

#define SPTPS_VERSION 0

// Record types
#define SPTPS_HANDSHAKE 128   // Key exchange and authentication
#define SPTPS_ALERT 129       // Warning or error messages
#define SPTPS_CLOSE 130       // Application closed the connection

// Key exchange states
#define SPTPS_KEX 1           // Waiting for the first Key EXchange record
#define SPTPS_SECONDARY_KEX 2 // Ready to receive a secondary Key EXchange record
#define SPTPS_SIG 3           // Waiting for a SIGnature record
#define SPTPS_ACK 4           // Waiting for an ACKnowledgement record

// max payload size when using SPTPS
// PAYLOAD_MTU
// - 19 to 21 bytes encryption (sptps.c send_record_priv / send_record_priv_datagram)
#define SPTPS_OVERHEAD 19
#define SPTPS_DATAGRAM_OVERHEAD 21
#define SPTPS_MTU (PAYLOAD_MTU - SPTPS_OVERHEAD)
#define SPTPS_DATAGRAM_MTU (PAYLOAD_MTU - SPTPS_DATAGRAM_OVERHEAD)

typedef bool (*send_data_t)(void *handle, uint8_t type, const void *data, size_t len);
typedef bool (*receive_record_t)(void *handle, uint8_t type, const void *data, uint16_t len);

typedef struct sptps {
	bool initiator;
	bool datagram;
	int state;

	char *inbuf;
	size_t buflen;
	uint16_t reclen;

	bool instate;
	chacha_poly1305_ctx_t *incipher;
	uint32_t inseqno;
	uint32_t received;
	unsigned int replaywin;
	char *late;

	bool outstate;
	chacha_poly1305_ctx_t *outcipher;
	uint32_t outseqno;

	ecdsa_t *mykey;
	ecdsa_t *hiskey;
	ecdh_t *ecdh;

	char *mykex;
	char *hiskex;
	char *key;
	char *label;
	size_t labellen;

	void *handle;
	send_data_t send_data;
	receive_record_t receive_record;
} sptps_t;

extern unsigned int sptps_replaywin;
extern void sptps_log_quiet(sptps_t *s, int s_errno, const char *format, va_list ap);
extern void sptps_log_stderr(sptps_t *s, int s_errno, const char *format, va_list ap);
extern void (*sptps_log)(sptps_t *s, int s_errno, const char *format, va_list ap);
extern bool sptps_start(sptps_t *s, void *handle, bool initiator, bool datagram, ecdsa_t *mykey, ecdsa_t *hiskey, const char *label, size_t labellen, send_data_t send_data, receive_record_t receive_record);
extern bool sptps_stop(sptps_t *s);
extern bool sptps_send_record(sptps_t *s, uint8_t type, const void *data, uint16_t len);
extern bool sptps_receive_data(sptps_t *s, const void *data, size_t len);
extern bool sptps_force_kex(sptps_t *s);
extern bool sptps_verify_datagram(sptps_t *s, const void *data, size_t len);
// max pre-encryption payload size
extern uint16_t sptps_maxmtu(sptps_t *s);
// number of bytes of packet needed by sptps for encryption and compression
extern uint16_t sptps_overhead(sptps_t *s);

#endif
