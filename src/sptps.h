#ifndef MESHLINK_SPTPS_H
#define MESHLINK_SPTPS_H

/*
    sptps.h -- Simple Peer-to-Peer Security
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

#include "system.h"

#include "chacha-poly1305/chacha-poly1305.h"
#include "ecdh.h"
#include "ecdsa.h"

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

typedef bool (*send_data_t)(void *handle, uint8_t type, const void *data, size_t len);
typedef bool (*receive_record_t)(void *handle, uint8_t type, const void *data, uint16_t len);

typedef struct sptps {
	// State
	bool initiator;
	bool datagram;
	bool instate;
	bool outstate;

	int state;

	// Main member variables
	char *inbuf;
	size_t inbuflen;

	chacha_poly1305_ctx_t *incipher;
	uint32_t replaywin;
	uint32_t inseqno;
	uint32_t received;
	uint16_t reclen;

	char *outbuf;
	size_t outbuflen;

	chacha_poly1305_ctx_t *outcipher;
	uint32_t outseqno;

	char *late;

	char *decrypted_buffer;
	size_t decrypted_buffer_len;

	// Callbacks
	void *handle;
	send_data_t send_data;
	receive_record_t receive_record;

	// Variables used for the authentication phase
	ecdsa_t *mykey;
	ecdsa_t *hiskey;
	ecdh_t *ecdh;

	char *mykex;
	char *hiskex;
	char *key;
	char *label;
	size_t labellen;

} sptps_t;

extern void sptps_log_quiet(sptps_t *s, int s_errno, const char *format, va_list ap);
extern void sptps_log_stderr(sptps_t *s, int s_errno, const char *format, va_list ap);
extern void (*sptps_log)(sptps_t *s, int s_errno, const char *format, va_list ap);
extern bool sptps_start(sptps_t *s, void *handle, bool initiator, bool datagram, ecdsa_t *mykey, ecdsa_t *hiskey, const char *label, size_t labellen, send_data_t send_data, receive_record_t receive_record) __attribute__((__warn_unused_result__));
extern bool sptps_stop(sptps_t *s);
extern bool sptps_send_record(sptps_t *s, uint8_t type, const void *data, uint16_t len);
extern bool sptps_receive_data(sptps_t *s, const void *data, size_t len) __attribute__((__warn_unused_result__));
extern bool sptps_force_kex(sptps_t *s) __attribute__((__warn_unused_result__));
extern bool sptps_verify_datagram(sptps_t *s, const void *data, size_t len) __attribute__((__warn_unused_result__));
extern void sptps_set_send_buffer(sptps_t *s, void *data, size_t len);

#endif
