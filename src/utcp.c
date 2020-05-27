/*
    utcp.c -- Userspace TCP
    Copyright (C) 2014-2017 Guus Sliepen <guus@tinc-vpn.org>

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

#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utcp_priv.h"

#ifndef EBADMSG
#define EBADMSG         104
#endif

#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

#ifdef poll
#undef poll
#endif

#ifndef UTCP_CLOCK
#if defined(CLOCK_MONOTONIC_RAW) && defined(__x86_64__)
#define UTCP_CLOCK CLOCK_MONOTONIC_RAW
#else
#define UTCP_CLOCK CLOCK_MONOTONIC
#endif
#endif

static void timespec_sub(const struct timespec *a, const struct timespec *b, struct timespec *r) {
	r->tv_sec = a->tv_sec - b->tv_sec;
	r->tv_nsec = a->tv_nsec - b->tv_nsec;

	if(r->tv_nsec < 0) {
		r->tv_sec--, r->tv_nsec += NSEC_PER_SEC;
	}
}

static int32_t timespec_diff_usec(const struct timespec *a, const struct timespec *b) {
	return (a->tv_sec - b->tv_sec) * 1000000 + (a->tv_nsec - b->tv_nsec) / 1000;
}

static bool timespec_lt(const struct timespec *a, const struct timespec *b) {
	if(a->tv_sec == b->tv_sec) {
		return a->tv_nsec < b->tv_nsec;
	} else {
		return a->tv_sec < b->tv_sec;
	}
}

static void timespec_clear(struct timespec *a) {
	a->tv_sec = 0;
	a->tv_nsec = 0;
}

static bool timespec_isset(const struct timespec *a) {
	return a->tv_sec;
}

static long CLOCK_GRANULARITY; // usec

static inline size_t min(size_t a, size_t b) {
	return a < b ? a : b;
}

static inline size_t max(size_t a, size_t b) {
	return a > b ? a : b;
}

#ifdef UTCP_DEBUG
#include <stdarg.h>

#ifndef UTCP_DEBUG_DATALEN
#define UTCP_DEBUG_DATALEN 20
#endif

static void debug(struct utcp_connection *c, const char *format, ...) {
	struct timespec tv;
	char buf[1024];
	int len;

	clock_gettime(CLOCK_REALTIME, &tv);
	len = snprintf(buf, sizeof(buf), "%ld.%06lu %u:%u ", (long)tv.tv_sec, tv.tv_nsec / 1000, c ? c->src : 0, c ? c->dst : 0);
	va_list ap;
	va_start(ap, format);
	len += vsnprintf(buf + len, sizeof(buf) - len, format, ap);
	va_end(ap);

	if(len > 0 && (size_t)len < sizeof(buf)) {
		fwrite(buf, len, 1, stderr);
	}
}

static void print_packet(struct utcp_connection *c, const char *dir, const void *pkt, size_t len) {
	struct hdr hdr;

	if(len < sizeof(hdr)) {
		debug(c, "%s: short packet (%lu bytes)\n", dir, (unsigned long)len);
		return;
	}

	memcpy(&hdr, pkt, sizeof(hdr));

	uint32_t datalen;

	if(len > sizeof(hdr)) {
		datalen = min(len - sizeof(hdr), UTCP_DEBUG_DATALEN);
	} else {
		datalen = 0;
	}


	const uint8_t *data = (uint8_t *)pkt + sizeof(hdr);
	char str[datalen * 2 + 1];
	char *p = str;

	for(uint32_t i = 0; i < datalen; i++) {
		*p++ = "0123456789ABCDEF"[data[i] >> 4];
		*p++ = "0123456789ABCDEF"[data[i] & 15];
	}

	*p = 0;

	debug(c, "%s: len %lu src %u dst %u seq %u ack %u wnd %u aux %x ctl %s%s%s%s%s data %s\n",
	      dir, (unsigned long)len, hdr.src, hdr.dst, hdr.seq, hdr.ack, hdr.wnd, hdr.aux,
	      hdr.ctl & SYN ? "SYN" : "",
	      hdr.ctl & RST ? "RST" : "",
	      hdr.ctl & FIN ? "FIN" : "",
	      hdr.ctl & ACK ? "ACK" : "",
	      hdr.ctl & MF ? "MF" : "",
	      str
	     );
}

static void debug_cwnd(struct utcp_connection *c) {
	debug(c, "snd.cwnd %u snd.ssthresh %u\n", c->snd.cwnd, ~c->snd.ssthresh ? c->snd.ssthresh : 0);
}
#else
#define debug(...) do {} while(0)
#define print_packet(...) do {} while(0)
#define debug_cwnd(...) do {} while(0)
#endif

static void set_state(struct utcp_connection *c, enum state state) {
	c->state = state;

	if(state == ESTABLISHED) {
		timespec_clear(&c->conn_timeout);
	}

	debug(c, "state %s\n", strstate[state]);
}

static bool fin_wanted(struct utcp_connection *c, uint32_t seq) {
	if(seq != c->snd.last) {
		return false;
	}

	switch(c->state) {
	case FIN_WAIT_1:
	case CLOSING:
	case LAST_ACK:
		return true;

	default:
		return false;
	}
}

static bool is_reliable(struct utcp_connection *c) {
	return c->flags & UTCP_RELIABLE;
}

static bool is_framed(struct utcp_connection *c) {
	return c->flags & UTCP_FRAMED;
}

static int32_t seqdiff(uint32_t a, uint32_t b) {
	return a - b;
}

// Buffer functions
static bool buffer_wraps(struct buffer *buf) {
	return buf->size - buf->offset < buf->used;
}

static bool buffer_resize(struct buffer *buf, uint32_t newsize) {
	char *newdata = realloc(buf->data, newsize);

	if(!newdata) {
		return false;
	}

	buf->data = newdata;

	if(buffer_wraps(buf)) {
		// Shift the right part of the buffer until it hits the end of the new buffer.
		// Old situation:
		// [345......012]
		// New situation:
		// [345.........|........012]
		uint32_t tailsize = buf->size - buf->offset;
		uint32_t newoffset = newsize - tailsize;
		memmove(buf->data + newoffset, buf->data + buf->offset, tailsize);
		buf->offset = newoffset;
	}

	buf->size = newsize;
	return true;
}

// Store data into the buffer
static ssize_t buffer_put_at(struct buffer *buf, size_t offset, const void *data, size_t len) {
	debug(NULL, "buffer_put_at %lu %lu %lu\n", (unsigned long)buf->used, (unsigned long)offset, (unsigned long)len);

	// Ensure we don't store more than maxsize bytes in total
	size_t required = offset + len;

	if(required > buf->maxsize) {
		if(offset >= buf->maxsize) {
			return 0;
		}

		len = buf->maxsize - offset;
		required = buf->maxsize;
	}

	// Check if we need to resize the buffer
	if(required > buf->size) {
		size_t newsize = buf->size;

		if(!newsize) {
			newsize = 4096;
		}

		do {
			newsize *= 2;
		} while(newsize < required);

		if(newsize > buf->maxsize) {
			newsize = buf->maxsize;
		}

		if(!buffer_resize(buf, newsize)) {
			return -1;
		}
	}

	uint32_t realoffset = buf->offset + offset;

	if(buf->size - buf->offset <= offset) {
		// The offset wrapped
		realoffset -= buf->size;
	}

	if(buf->size - realoffset < len) {
		// The new chunk of data must be wrapped
		memcpy(buf->data + realoffset, data, buf->size - realoffset);
		memcpy(buf->data, (char *)data + buf->size - realoffset, len - (buf->size - realoffset));
	} else {
		memcpy(buf->data + realoffset, data, len);
	}

	if(required > buf->used) {
		buf->used = required;
	}

	return len;
}

static ssize_t buffer_put(struct buffer *buf, const void *data, size_t len) {
	return buffer_put_at(buf, buf->used, data, len);
}

// Copy data from the buffer without removing it.
static ssize_t buffer_copy(struct buffer *buf, void *data, size_t offset, size_t len) {
	// Ensure we don't copy more than is actually stored in the buffer
	if(offset >= buf->used) {
		return 0;
	}

	if(buf->used - offset < len) {
		len = buf->used - offset;
	}

	uint32_t realoffset = buf->offset + offset;

	if(buf->size - buf->offset <= offset) {
		// The offset wrapped
		realoffset -= buf->size;
	}

	if(buf->size - realoffset < len) {
		// The data is wrapped
		memcpy(data, buf->data + realoffset, buf->size - realoffset);
		memcpy((char *)data + buf->size - realoffset, buf->data, len - (buf->size - realoffset));
	} else {
		memcpy(data, buf->data + realoffset, len);
	}

	return len;
}

// Copy data from the buffer without removing it.
static ssize_t buffer_call(struct utcp_connection *c, struct buffer *buf, size_t offset, size_t len) {
	if(!c->recv) {
		return len;
	}

	// Ensure we don't copy more than is actually stored in the buffer
	if(offset >= buf->used) {
		return 0;
	}

	if(buf->used - offset < len) {
		len = buf->used - offset;
	}

	uint32_t realoffset = buf->offset + offset;

	if(buf->size - buf->offset <= offset) {
		// The offset wrapped
		realoffset -= buf->size;
	}

	if(buf->size - realoffset < len) {
		// The data is wrapped
		ssize_t rx1 = c->recv(c, buf->data + realoffset, buf->size - realoffset);

		if(rx1 < buf->size - realoffset) {
			return rx1;
		}

		// The channel might have been closed by the previous callback
		if(!c->recv) {
			return len;
		}

		ssize_t rx2 = c->recv(c, buf->data, len - (buf->size - realoffset));

		if(rx2 < 0) {
			return rx2;
		} else {
			return rx1 + rx2;
		}
	} else {
		return c->recv(c, buf->data + realoffset, len);
	}
}

// Discard data from the buffer.
static ssize_t buffer_discard(struct buffer *buf, size_t len) {
	if(buf->used < len) {
		len = buf->used;
	}

	if(buf->size - buf->offset <= len) {
		buf->offset -= buf->size;
	}

	if(buf->used == len) {
		buf->offset = 0;
	} else {
		buf->offset += len;
	}

	buf->used -= len;

	return len;
}

static void buffer_clear(struct buffer *buf) {
	buf->used = 0;
	buf->offset = 0;
}

static bool buffer_set_size(struct buffer *buf, uint32_t minsize, uint32_t maxsize) {
	if(maxsize < minsize) {
		maxsize = minsize;
	}

	buf->maxsize = maxsize;

	return buf->size >= minsize || buffer_resize(buf, minsize);
}

static void buffer_exit(struct buffer *buf) {
	free(buf->data);
	memset(buf, 0, sizeof(*buf));
}

static uint32_t buffer_free(const struct buffer *buf) {
	return buf->maxsize > buf->used ? buf->maxsize - buf->used : 0;
}

// Connections are stored in a sorted list.
// This gives O(log(N)) lookup time, O(N log(N)) insertion time and O(N) deletion time.

static int compare(const void *va, const void *vb) {
	assert(va && vb);

	const struct utcp_connection *a = *(struct utcp_connection **)va;
	const struct utcp_connection *b = *(struct utcp_connection **)vb;

	assert(a && b);
	assert(a->src && b->src);

	int c = (int)a->src - (int)b->src;

	if(c) {
		return c;
	}

	c = (int)a->dst - (int)b->dst;
	return c;
}

static struct utcp_connection *find_connection(const struct utcp *utcp, uint16_t src, uint16_t dst) {
	if(!utcp->nconnections) {
		return NULL;
	}

	struct utcp_connection key = {
		.src = src,
		.dst = dst,
	}, *keyp = &key;
	struct utcp_connection **match = bsearch(&keyp, utcp->connections, utcp->nconnections, sizeof(*utcp->connections), compare);
	return match ? *match : NULL;
}

static void free_connection(struct utcp_connection *c) {
	struct utcp *utcp = c->utcp;
	struct utcp_connection **cp = bsearch(&c, utcp->connections, utcp->nconnections, sizeof(*utcp->connections), compare);

	assert(cp);

	int i = cp - utcp->connections;
	memmove(cp, cp + 1, (utcp->nconnections - i - 1) * sizeof(*cp));
	utcp->nconnections--;

	buffer_exit(&c->rcvbuf);
	buffer_exit(&c->sndbuf);
	free(c);
}

static struct utcp_connection *allocate_connection(struct utcp *utcp, uint16_t src, uint16_t dst) {
	// Check whether this combination of src and dst is free

	if(src) {
		if(find_connection(utcp, src, dst)) {
			errno = EADDRINUSE;
			return NULL;
		}
	} else { // If src == 0, generate a random port number with the high bit set
		if(utcp->nconnections >= 32767) {
			errno = ENOMEM;
			return NULL;
		}

		src = rand() | 0x8000;

		while(find_connection(utcp, src, dst)) {
			src++;
		}
	}

	// Allocate memory for the new connection

	if(utcp->nconnections >= utcp->nallocated) {
		if(!utcp->nallocated) {
			utcp->nallocated = 4;
		} else {
			utcp->nallocated *= 2;
		}

		struct utcp_connection **new_array = realloc(utcp->connections, utcp->nallocated * sizeof(*utcp->connections));

		if(!new_array) {
			return NULL;
		}

		utcp->connections = new_array;
	}

	struct utcp_connection *c = calloc(1, sizeof(*c));

	if(!c) {
		return NULL;
	}

	if(!buffer_set_size(&c->sndbuf, DEFAULT_SNDBUFSIZE, DEFAULT_MAXSNDBUFSIZE)) {
		free(c);
		return NULL;
	}

	if(!buffer_set_size(&c->rcvbuf, DEFAULT_RCVBUFSIZE, DEFAULT_MAXRCVBUFSIZE)) {
		buffer_exit(&c->sndbuf);
		free(c);
		return NULL;
	}

	// Fill in the details

	c->src = src;
	c->dst = dst;
#ifdef UTCP_DEBUG
	c->snd.iss = 0;
#else
	c->snd.iss = rand();
#endif
	c->snd.una = c->snd.iss;
	c->snd.nxt = c->snd.iss + 1;
	c->snd.last = c->snd.nxt;
	c->snd.cwnd = (utcp->mss > 2190 ? 2 : utcp->mss > 1095 ? 3 : 4) * utcp->mss;
	c->snd.ssthresh = ~0;
	debug_cwnd(c);
	c->srtt = 0;
	c->rttvar = 0;
	c->rto = START_RTO;
	c->utcp = utcp;

	// Add it to the sorted list of connections

	utcp->connections[utcp->nconnections++] = c;
	qsort(utcp->connections, utcp->nconnections, sizeof(*utcp->connections), compare);

	return c;
}

static inline uint32_t absdiff(uint32_t a, uint32_t b) {
	if(a > b) {
		return a - b;
	} else {
		return b - a;
	}
}

// Update RTT variables. See RFC 6298.
static void update_rtt(struct utcp_connection *c, uint32_t rtt) {
	if(!rtt) {
		debug(c, "invalid rtt\n");
		return;
	}

	if(!c->srtt) {
		c->srtt = rtt;
		c->rttvar = rtt / 2;
	} else {
		c->rttvar = (c->rttvar * 3 + absdiff(c->srtt, rtt)) / 4;
		c->srtt = (c->srtt * 7 + rtt) / 8;
	}

	c->rto = c->srtt + max(4 * c->rttvar, CLOCK_GRANULARITY);

	if(c->rto > MAX_RTO) {
		c->rto = MAX_RTO;
	}

	debug(c, "rtt %u srtt %u rttvar %u rto %u\n", rtt, c->srtt, c->rttvar, c->rto);
}

static void start_retransmit_timer(struct utcp_connection *c) {
	clock_gettime(UTCP_CLOCK, &c->rtrx_timeout);

	uint32_t rto = c->rto;

	while(rto > USEC_PER_SEC) {
		c->rtrx_timeout.tv_sec++;
		rto -= USEC_PER_SEC;
	}

	c->rtrx_timeout.tv_nsec += rto * 1000;

	if(c->rtrx_timeout.tv_nsec >= NSEC_PER_SEC) {
		c->rtrx_timeout.tv_nsec -= NSEC_PER_SEC;
		c->rtrx_timeout.tv_sec++;
	}

	debug(c, "rtrx_timeout %ld.%06lu\n", c->rtrx_timeout.tv_sec, c->rtrx_timeout.tv_nsec);
}

static void start_flush_timer(struct utcp_connection *c) {
	clock_gettime(UTCP_CLOCK, &c->rtrx_timeout);

	c->rtrx_timeout.tv_nsec += c->utcp->flush_timeout * 1000000;

	if(c->rtrx_timeout.tv_nsec >= NSEC_PER_SEC) {
		c->rtrx_timeout.tv_nsec -= NSEC_PER_SEC;
		c->rtrx_timeout.tv_sec++;
	}

	debug(c, "rtrx_timeout %ld.%06lu (flush)\n", c->rtrx_timeout.tv_sec, c->rtrx_timeout.tv_nsec);
}

static void stop_retransmit_timer(struct utcp_connection *c) {
	timespec_clear(&c->rtrx_timeout);
	debug(c, "rtrx_timeout cleared\n");
}

struct utcp_connection *utcp_connect_ex(struct utcp *utcp, uint16_t dst, utcp_recv_t recv, void *priv, uint32_t flags) {
	struct utcp_connection *c = allocate_connection(utcp, 0, dst);

	if(!c) {
		return NULL;
	}

	assert((flags & ~0x1f) == 0);

	c->flags = flags;
	c->recv = recv;
	c->priv = priv;

	struct {
		struct hdr hdr;
		uint8_t init[4];
	} pkt;

	pkt.hdr.src = c->src;
	pkt.hdr.dst = c->dst;
	pkt.hdr.seq = c->snd.iss;
	pkt.hdr.ack = 0;
	pkt.hdr.wnd = c->rcvbuf.maxsize;
	pkt.hdr.ctl = SYN;
	pkt.hdr.aux = 0x0101;
	pkt.init[0] = 1;
	pkt.init[1] = 0;
	pkt.init[2] = 0;
	pkt.init[3] = flags & 0x7;

	set_state(c, SYN_SENT);

	print_packet(c, "send", &pkt, sizeof(pkt));
	utcp->send(utcp, &pkt, sizeof(pkt));

	clock_gettime(UTCP_CLOCK, &c->conn_timeout);
	c->conn_timeout.tv_sec += utcp->timeout;

	start_retransmit_timer(c);

	return c;
}

struct utcp_connection *utcp_connect(struct utcp *utcp, uint16_t dst, utcp_recv_t recv, void *priv) {
	return utcp_connect_ex(utcp, dst, recv, priv, UTCP_TCP);
}

void utcp_accept(struct utcp_connection *c, utcp_recv_t recv, void *priv) {
	if(c->reapable || c->state != SYN_RECEIVED) {
		debug(c, "accept() called on invalid connection in state %s\n", c, strstate[c->state]);
		return;
	}

	debug(c, "accepted %p %p\n", c, recv, priv);
	c->recv = recv;
	c->priv = priv;
	c->do_poll = true;
	set_state(c, ESTABLISHED);
}

static void ack(struct utcp_connection *c, bool sendatleastone) {
	int32_t left = seqdiff(c->snd.last, c->snd.nxt);
	int32_t cwndleft = is_reliable(c) ? min(c->snd.cwnd, c->snd.wnd) - seqdiff(c->snd.nxt, c->snd.una) : MAX_UNRELIABLE_SIZE;

	assert(left >= 0);

	if(cwndleft <= 0) {
		left = 0;
	} else if(cwndleft < left) {
		left = cwndleft;

		if(!sendatleastone || cwndleft > c->utcp->mss) {
			left -= left % c->utcp->mss;
		}
	}

	debug(c, "cwndleft %d left %d\n", cwndleft, left);

	if(!left && !sendatleastone) {
		return;
	}

	struct {
		struct hdr hdr;
		uint8_t data[];
	} *pkt = c->utcp->pkt;

	pkt->hdr.src = c->src;
	pkt->hdr.dst = c->dst;
	pkt->hdr.ack = c->rcv.nxt;
	pkt->hdr.wnd = is_reliable(c) ? c->rcvbuf.maxsize : 0;
	pkt->hdr.ctl = ACK;
	pkt->hdr.aux = 0;

	do {
		uint32_t seglen = left > c->utcp->mss ? c->utcp->mss : left;
		pkt->hdr.seq = c->snd.nxt;

		buffer_copy(&c->sndbuf, pkt->data, seqdiff(c->snd.nxt, c->snd.una), seglen);

		c->snd.nxt += seglen;
		left -= seglen;

		if(!is_reliable(c)) {
			if(left) {
				pkt->hdr.ctl |= MF;
			} else {
				pkt->hdr.ctl &= ~MF;
			}
		}

		if(seglen && fin_wanted(c, c->snd.nxt)) {
			seglen--;
			pkt->hdr.ctl |= FIN;
		}

		if(!c->rtt_start.tv_sec && is_reliable(c)) {
			// Start RTT measurement
			clock_gettime(UTCP_CLOCK, &c->rtt_start);
			c->rtt_seq = pkt->hdr.seq + seglen;
			debug(c, "starting RTT measurement, expecting ack %u\n", c->rtt_seq);
		}

		print_packet(c, "send", pkt, sizeof(pkt->hdr) + seglen);
		c->utcp->send(c->utcp, pkt, sizeof(pkt->hdr) + seglen);

		if(left && !is_reliable(c)) {
			pkt->hdr.wnd += seglen;
		}
	} while(left);
}

static ssize_t utcp_send_reliable(struct utcp_connection *c, const void *data, size_t len) {
	size_t rlen = len + (is_framed(c) ? 2 : 0);

	if(!rlen) {
		return 0;
	}

	// Check if we need to be able to buffer all data

	if(c->flags & (UTCP_NO_PARTIAL | UTCP_FRAMED)) {
		if(rlen > c->sndbuf.maxsize) {
			errno = EMSGSIZE;
			return -1;
		}

		if((c->flags & UTCP_FRAMED) && len > MAX_UNRELIABLE_SIZE) {
			errno = EMSGSIZE;
			return -1;
		}

		if(rlen > buffer_free(&c->sndbuf)) {
			errno = EWOULDBLOCK;
			return 0;
		}
	}

	// Add data to the send buffer.

	if(is_framed(c)) {
		uint16_t len16 = len;
		buffer_put(&c->sndbuf, &len16, sizeof(len16));
		assert(buffer_put(&c->sndbuf, data, len) == (ssize_t)len);
	} else {
		len = buffer_put(&c->sndbuf, data, len);

		if(len <= 0) {
			errno = EWOULDBLOCK;
			return 0;
		}
	}

	c->snd.last += rlen;

	// Don't send anything yet if the connection has not fully established yet

	if(c->state == SYN_SENT || c->state == SYN_RECEIVED) {
		return len;
	}

	ack(c, false);

	if(!timespec_isset(&c->rtrx_timeout)) {
		start_retransmit_timer(c);
	}

	if(!timespec_isset(&c->conn_timeout)) {
		clock_gettime(UTCP_CLOCK, &c->conn_timeout);
		c->conn_timeout.tv_sec += c->utcp->timeout;
	}

	return len;
}


/* In the send buffer we can have multiple frames, each prefixed with their
   length. However, the first frame might already have been partially sent. The
   variable c->frame_offset tracks how much of a partial frame is left at the
   start. If it is 0, it means there is no partial frame, and the first two
   bytes in the send buffer are the length of the first frame.

   After sending an MSS sized packet, we need to calculate the new frame_offset
   value, since it is likely that the next packet will also have a partial frame
   at the start. We do this by scanning the previously sent packet for frame
   headers, to find out how many bytes of the last frame are left to send.
*/
static void ack_unreliable_framed(struct utcp_connection *c) {
	int32_t left = seqdiff(c->snd.last, c->snd.nxt);
	assert(left > 0);

	struct {
		struct hdr hdr;
		uint8_t data[];
	} *pkt = c->utcp->pkt;

	pkt->hdr.src = c->src;
	pkt->hdr.dst = c->dst;
	pkt->hdr.ack = c->rcv.nxt;
	pkt->hdr.ctl = ACK | MF;
	pkt->hdr.aux = 0;

	bool sent_packet = false;

	while(left >= c->utcp->mss) {
		pkt->hdr.wnd = c->frame_offset;
		uint32_t seglen = c->utcp->mss;

		pkt->hdr.seq = c->snd.nxt;

		buffer_copy(&c->sndbuf, pkt->data, seqdiff(c->snd.nxt, c->snd.una), seglen);

		c->snd.nxt += seglen;
		c->snd.una = c->snd.nxt;
		left -= seglen;

		print_packet(c, "send", pkt, sizeof(pkt->hdr) + seglen);
		c->utcp->send(c->utcp, pkt, sizeof(pkt->hdr) + seglen);
		sent_packet = true;

		// Calculate the new frame offset
		while(c->frame_offset < seglen) {
			uint16_t framelen;
			buffer_copy(&c->sndbuf, &framelen, c->frame_offset, sizeof(framelen));
			c->frame_offset += framelen + 2;
		}

		buffer_discard(&c->sndbuf, seglen);
		c->frame_offset -= seglen;
	};

	if(sent_packet) {
		if(left) {
			// We sent one packet but we have partial data left, (re)start the flush timer
			start_flush_timer(c);
		} else {
			// There is no partial data in the send buffer, so stop the flush timer
			stop_retransmit_timer(c);
		}
	}
}

static void flush_unreliable_framed(struct utcp_connection *c) {
	int32_t left = seqdiff(c->snd.last, c->snd.nxt);

	/* If the MSS dropped since last time ack_unreliable_frame() was called,
	  we might now have more than one segment worth of data left.
	*/
	if(left > c->utcp->mss) {
		ack_unreliable_framed(c);
		left = seqdiff(c->snd.last, c->snd.nxt);
		assert(left <= c->utcp->mss);
	}

	if(left) {
		struct {
			struct hdr hdr;
			uint8_t data[];
		} *pkt = c->utcp->pkt;

		pkt->hdr.src = c->src;
		pkt->hdr.dst = c->dst;
		pkt->hdr.seq = c->snd.nxt;
		pkt->hdr.ack = c->rcv.nxt;
		pkt->hdr.wnd = c->frame_offset;
		pkt->hdr.ctl = ACK | MF;
		pkt->hdr.aux = 0;

		uint32_t seglen = left;

		buffer_copy(&c->sndbuf, pkt->data, seqdiff(c->snd.nxt, c->snd.una), seglen);
		buffer_discard(&c->sndbuf, seglen);

		c->snd.nxt += seglen;
		c->snd.una = c->snd.nxt;

		print_packet(c, "send", pkt, sizeof(pkt->hdr) + seglen);
		c->utcp->send(c->utcp, pkt, sizeof(pkt->hdr) + seglen);
	}

	c->frame_offset = 0;
	stop_retransmit_timer(c);
}


static ssize_t utcp_send_unreliable(struct utcp_connection *c, const void *data, size_t len) {
	if(len > MAX_UNRELIABLE_SIZE) {
		errno = EMSGSIZE;
		return -1;
	}

	size_t rlen = len + (is_framed(c) ? 2 : 0);

	if(rlen > buffer_free(&c->sndbuf)) {
		if(rlen > c->sndbuf.maxsize) {
			errno = EMSGSIZE;
			return -1;
		} else {
			errno = EWOULDBLOCK;
			return 0;
		}
	}

	// Don't send anything yet if the connection has not fully established yet

	if(c->state == SYN_SENT || c->state == SYN_RECEIVED) {
		return len;
	}

	if(is_framed(c)) {
		uint16_t framelen = len;
		buffer_put(&c->sndbuf, &framelen, sizeof(framelen));
	}

	buffer_put(&c->sndbuf, data, len);

	c->snd.last += rlen;

	if(is_framed(c)) {
		ack_unreliable_framed(c);
	} else {
		ack(c, false);
		c->snd.una = c->snd.nxt = c->snd.last;
		buffer_discard(&c->sndbuf, c->sndbuf.used);
	}

	return len;
}

ssize_t utcp_send(struct utcp_connection *c, const void *data, size_t len) {
	if(c->reapable) {
		debug(c, "send() called on closed connection\n");
		errno = EBADF;
		return -1;
	}

	switch(c->state) {
	case CLOSED:
	case LISTEN:
		debug(c, "send() called on unconnected connection\n");
		errno = ENOTCONN;
		return -1;

	case SYN_SENT:
	case SYN_RECEIVED:
	case ESTABLISHED:
	case CLOSE_WAIT:
		break;

	case FIN_WAIT_1:
	case FIN_WAIT_2:
	case CLOSING:
	case LAST_ACK:
	case TIME_WAIT:
		debug(c, "send() called on closed connection\n");
		errno = EPIPE;
		return -1;
	}

	if(!data && len) {
		errno = EFAULT;
		return -1;
	}

	if(is_reliable(c)) {
		return utcp_send_reliable(c, data, len);
	} else {
		return utcp_send_unreliable(c, data, len);
	}
}

static void swap_ports(struct hdr *hdr) {
	uint16_t tmp = hdr->src;
	hdr->src = hdr->dst;
	hdr->dst = tmp;
}

static void fast_retransmit(struct utcp_connection *c) {
	if(c->state == CLOSED || c->snd.last == c->snd.una) {
		debug(c, "fast_retransmit() called but nothing to retransmit!\n");
		return;
	}

	struct utcp *utcp = c->utcp;

	struct {
		struct hdr hdr;
		uint8_t data[];
	} *pkt = c->utcp->pkt;

	pkt->hdr.src = c->src;
	pkt->hdr.dst = c->dst;
	pkt->hdr.wnd = c->rcvbuf.maxsize;
	pkt->hdr.aux = 0;

	switch(c->state) {
	case ESTABLISHED:
	case FIN_WAIT_1:
	case CLOSE_WAIT:
	case CLOSING:
	case LAST_ACK:
		// Send unacked data again.
		pkt->hdr.seq = c->snd.una;
		pkt->hdr.ack = c->rcv.nxt;
		pkt->hdr.ctl = ACK;
		uint32_t len = min(seqdiff(c->snd.last, c->snd.una), utcp->mss);

		if(fin_wanted(c, c->snd.una + len)) {
			len--;
			pkt->hdr.ctl |= FIN;
		}

		buffer_copy(&c->sndbuf, pkt->data, 0, len);
		print_packet(c, "rtrx", pkt, sizeof(pkt->hdr) + len);
		utcp->send(utcp, pkt, sizeof(pkt->hdr) + len);
		break;

	default:
		break;
	}
}

static void retransmit(struct utcp_connection *c) {
	if(c->state == CLOSED || c->snd.last == c->snd.una) {
		debug(c, "retransmit() called but nothing to retransmit!\n");
		stop_retransmit_timer(c);
		return;
	}

	struct utcp *utcp = c->utcp;

	if(utcp->retransmit && is_reliable(c)) {
		utcp->retransmit(c);
	}

	struct {
		struct hdr hdr;
		uint8_t data[];
	} *pkt = c->utcp->pkt;

	pkt->hdr.src = c->src;
	pkt->hdr.dst = c->dst;
	pkt->hdr.wnd = c->rcvbuf.maxsize;
	pkt->hdr.aux = 0;

	switch(c->state) {
	case SYN_SENT:
		// Send our SYN again
		pkt->hdr.seq = c->snd.iss;
		pkt->hdr.ack = 0;
		pkt->hdr.ctl = SYN;
		pkt->hdr.aux = 0x0101;
		pkt->data[0] = 1;
		pkt->data[1] = 0;
		pkt->data[2] = 0;
		pkt->data[3] = c->flags & 0x7;
		print_packet(c, "rtrx", pkt, sizeof(pkt->hdr) + 4);
		utcp->send(utcp, pkt, sizeof(pkt->hdr) + 4);
		break;

	case SYN_RECEIVED:
		// Send SYNACK again
		pkt->hdr.seq = c->snd.nxt;
		pkt->hdr.ack = c->rcv.nxt;
		pkt->hdr.ctl = SYN | ACK;
		print_packet(c, "rtrx", pkt, sizeof(pkt->hdr));
		utcp->send(utcp, pkt, sizeof(pkt->hdr));
		break;

	case ESTABLISHED:
	case FIN_WAIT_1:
	case CLOSE_WAIT:
	case CLOSING:
	case LAST_ACK:
		if(!is_reliable(c) && is_framed(c) && c->sndbuf.used) {
			flush_unreliable_framed(c);
			return;
		}

		// Send unacked data again.
		pkt->hdr.seq = c->snd.una;
		pkt->hdr.ack = c->rcv.nxt;
		pkt->hdr.ctl = ACK;
		uint32_t len = min(seqdiff(c->snd.last, c->snd.una), utcp->mss);

		if(fin_wanted(c, c->snd.una + len)) {
			len--;
			pkt->hdr.ctl |= FIN;
		}

		// RFC 5681 slow start after timeout
		uint32_t flightsize = seqdiff(c->snd.nxt, c->snd.una);
		c->snd.ssthresh = max(flightsize / 2, utcp->mss * 2); // eq. 4
		c->snd.cwnd = utcp->mss;
		debug_cwnd(c);

		buffer_copy(&c->sndbuf, pkt->data, 0, len);
		print_packet(c, "rtrx", pkt, sizeof(pkt->hdr) + len);
		utcp->send(utcp, pkt, sizeof(pkt->hdr) + len);

		c->snd.nxt = c->snd.una + len;
		break;

	case CLOSED:
	case LISTEN:
	case TIME_WAIT:
	case FIN_WAIT_2:
		// We shouldn't need to retransmit anything in this state.
#ifdef UTCP_DEBUG
		abort();
#endif
		stop_retransmit_timer(c);
		goto cleanup;
	}

	start_retransmit_timer(c);
	c->rto *= 2;

	if(c->rto > MAX_RTO) {
		c->rto = MAX_RTO;
	}

	c->rtt_start.tv_sec = 0; // invalidate RTT timer
	c->dupack = 0; // cancel any ongoing fast recovery

cleanup:
	return;
}

/* Update receive buffer and SACK entries after consuming data.
 *
 * Situation:
 *
 * |.....0000..1111111111.....22222......3333|
 * |---------------^
 *
 * 0..3 represent the SACK entries. The ^ indicates up to which point we want
 * to remove data from the receive buffer. The idea is to substract "len"
 * from the offset of all the SACK entries, and then remove/cut down entries
 * that are shifted to before the start of the receive buffer.
 *
 * There are three cases:
 * - the SACK entry is after ^, in that case just change the offset.
 * - the SACK entry starts before and ends after ^, so we have to
 *   change both its offset and size.
 * - the SACK entry is completely before ^, in that case delete it.
 */
static void sack_consume(struct utcp_connection *c, size_t len) {
	debug(c, "sack_consume %lu\n", (unsigned long)len);

	if(len > c->rcvbuf.used) {
		debug(c, "all SACK entries consumed\n");
		c->sacks[0].len = 0;
		return;
	}

	buffer_discard(&c->rcvbuf, len);

	for(int i = 0; i < NSACKS && c->sacks[i].len;) {
		if(len < c->sacks[i].offset) {
			c->sacks[i].offset -= len;
			i++;
		} else if(len < c->sacks[i].offset + c->sacks[i].len) {
			c->sacks[i].len -= len - c->sacks[i].offset;
			c->sacks[i].offset = 0;
			i++;
		} else {
			if(i < NSACKS - 1) {
				memmove(&c->sacks[i], &c->sacks[i + 1], (NSACKS - 1 - i) * sizeof(c->sacks)[i]);
				c->sacks[NSACKS - 1].len = 0;
			} else {
				c->sacks[i].len = 0;
				break;
			}
		}
	}

	for(int i = 0; i < NSACKS && c->sacks[i].len; i++) {
		debug(c, "SACK[%d] offset %u len %u\n", i, c->sacks[i].offset, c->sacks[i].len);
	}
}

static void handle_out_of_order(struct utcp_connection *c, uint32_t offset, const void *data, size_t len) {
	debug(c, "out of order packet, offset %u\n", offset);
	// Packet loss or reordering occured. Store the data in the buffer.
	ssize_t rxd = buffer_put_at(&c->rcvbuf, offset, data, len);

	if(rxd <= 0) {
		debug(c, "packet outside receive buffer, dropping\n");
		return;
	}

	if((size_t)rxd < len) {
		debug(c, "packet partially outside receive buffer\n");
		len = rxd;
	}

	// Make note of where we put it.
	for(int i = 0; i < NSACKS; i++) {
		if(!c->sacks[i].len) { // nothing to merge, add new entry
			debug(c, "new SACK entry %d\n", i);
			c->sacks[i].offset = offset;
			c->sacks[i].len = rxd;
			break;
		} else if(offset < c->sacks[i].offset) {
			if(offset + rxd < c->sacks[i].offset) { // insert before
				if(!c->sacks[NSACKS - 1].len) { // only if room left
					debug(c, "insert SACK entry at %d\n", i);
					memmove(&c->sacks[i + 1], &c->sacks[i], (NSACKS - i - 1) * sizeof(c->sacks)[i]);
					c->sacks[i].offset = offset;
					c->sacks[i].len = rxd;
				} else {
					debug(c, "SACK entries full, dropping packet\n");
				}

				break;
			} else { // merge
				debug(c, "merge with start of SACK entry at %d\n", i);
				c->sacks[i].offset = offset;
				break;
			}
		} else if(offset <= c->sacks[i].offset + c->sacks[i].len) {
			if(offset + rxd > c->sacks[i].offset + c->sacks[i].len) { // merge
				debug(c, "merge with end of SACK entry at %d\n", i);
				c->sacks[i].len = offset + rxd - c->sacks[i].offset;
				// TODO: handle potential merge with next entry
			}

			break;
		}
	}

	for(int i = 0; i < NSACKS && c->sacks[i].len; i++) {
		debug(c, "SACK[%d] offset %u len %u\n", i, c->sacks[i].offset, c->sacks[i].len);
	}
}

static void handle_out_of_order_framed(struct utcp_connection *c, uint32_t offset, const void *data, size_t len) {
	uint32_t in_order_offset = (c->sacks[0].len && c->sacks[0].offset == 0) ? c->sacks[0].len : 0;

	// Put the data into the receive buffer
	handle_out_of_order(c, offset + in_order_offset, data, len);
}

static void handle_in_order(struct utcp_connection *c, const void *data, size_t len) {
	if(c->recv) {
		ssize_t rxd = c->recv(c, data, len);

		if(rxd != (ssize_t)len) {
			// TODO: handle the application not accepting all data.
			abort();
		}
	}

	// Check if we can process out-of-order data now.
	if(c->sacks[0].len && len >= c->sacks[0].offset) {
		debug(c, "incoming packet len %lu connected with SACK at %u\n", (unsigned long)len, c->sacks[0].offset);

		if(len < c->sacks[0].offset + c->sacks[0].len) {
			size_t offset = len;
			len = c->sacks[0].offset + c->sacks[0].len;
			size_t remainder = len - offset;

			ssize_t rxd = buffer_call(c, &c->rcvbuf, offset, remainder);

			if(rxd != (ssize_t)remainder) {
				// TODO: handle the application not accepting all data.
				abort();
			}
		}
	}

	if(c->rcvbuf.used) {
		sack_consume(c, len);
	}

	c->rcv.nxt += len;
}

static void handle_in_order_framed(struct utcp_connection *c, const void *data, size_t len) {
	// Treat it as out of order, since it is unlikely the start of this packet contains the start of a frame.
	uint32_t in_order_offset = (c->sacks[0].len && c->sacks[0].offset == 0) ? c->sacks[0].len : 0;
	handle_out_of_order(c, in_order_offset, data, len);

	// While we have full frames at the start, give them to the application
	while(c->sacks[0].len >= 2 && c->sacks[0].offset == 0) {
		uint16_t framelen;
		buffer_copy(&c->rcvbuf, &framelen, 0, sizeof(&framelen));

		if(framelen > c->sacks[0].len - 2) {
			break;
		}

		if(c->recv) {
			ssize_t rxd;
			uint32_t realoffset = c->rcvbuf.offset + 2;

			if(c->rcvbuf.size - c->rcvbuf.offset <= 2) {
				realoffset -= c->rcvbuf.size;
			}

			if(realoffset > c->rcvbuf.size - framelen) {
				// The buffer wraps, we need to copy
				uint8_t buf[framelen];
				buffer_copy(&c->rcvbuf, buf, 2, framelen);
				rxd = c->recv(c, buf, framelen);
			} else {
				// The frame is contiguous in the receive buffer
				rxd = c->recv(c, c->rcvbuf.data + realoffset, framelen);
			}

			if(rxd != (ssize_t)framelen) {
				// TODO: handle the application not accepting all data.
				abort();
			}
		}

		sack_consume(c, framelen + 2);
	}

	c->rcv.nxt += len;
}

static void handle_unreliable(struct utcp_connection *c, const struct hdr *hdr, const void *data, size_t len) {
	// Fast path for unfragmented packets
	if(!hdr->wnd && !(hdr->ctl & MF)) {
		if(c->recv) {
			c->recv(c, data, len);
		}

		c->rcv.nxt = hdr->seq + len;
		return;
	}

	// Ensure reassembled packet are not larger than 64 kiB
	if(hdr->wnd > MAX_UNRELIABLE_SIZE || hdr->wnd + len > MAX_UNRELIABLE_SIZE) {
		return;
	}

	// Don't accept out of order fragments
	if(hdr->wnd && hdr->seq != c->rcv.nxt) {
		return;
	}

	// Reset the receive buffer for the first fragment
	if(!hdr->wnd) {
		buffer_clear(&c->rcvbuf);
	}

	ssize_t rxd = buffer_put_at(&c->rcvbuf, hdr->wnd, data, len);

	if(rxd != (ssize_t)len) {
		return;
	}

	// Send the packet if it's the final fragment
	if(!(hdr->ctl & MF)) {
		buffer_call(c, &c->rcvbuf, 0, hdr->wnd + len);
	}

	c->rcv.nxt = hdr->seq + len;
}

static void handle_unreliable_framed(struct utcp_connection *c, const struct hdr *hdr, const void *data, size_t len) {
	bool in_order = hdr->seq == c->rcv.nxt;
	c->rcv.nxt = hdr->seq + len;

	const uint8_t *ptr = data;
	size_t left = len;

	// Does it start with a partial frame?
	if(hdr->wnd) {
		// Only accept the data if it is in order
		if(in_order && c->rcvbuf.used) {
			// In order, append it to the receive buffer
			buffer_put(&c->rcvbuf, data, min(hdr->wnd, len));

			if(hdr->wnd <= len) {
				// We have a full frame
				c->recv(c, (uint8_t *)c->rcvbuf.data + 2, c->rcvbuf.used - 2);
			}
		}

		// Exit early if there is other data in this frame
		if(hdr->wnd > len) {
			if(!in_order) {
				buffer_clear(&c->rcvbuf);
			}

			return;
		}

		ptr += hdr->wnd;
		left -= hdr->wnd;
	}

	// We now start with new frames, so clear any data in the receive buffer
	buffer_clear(&c->rcvbuf);

	// Handle whole frames
	while(left > 2) {
		uint16_t framelen;
		memcpy(&framelen, ptr, sizeof(framelen));

		if(left <= (size_t)framelen + 2) {
			break;
		}

		c->recv(c, ptr + 2, framelen);
		ptr += framelen + 2;
		left -= framelen + 2;
	}

	// Handle partial last frame
	if(left) {
		buffer_put(&c->rcvbuf, ptr, left);
	}
}

static void handle_incoming_data(struct utcp_connection *c, const struct hdr *hdr, const void *data, size_t len) {
	if(!is_reliable(c)) {
		if(is_framed(c)) {
			handle_unreliable_framed(c, hdr, data, len);
		} else {
			handle_unreliable(c, hdr, data, len);
		}

		return;
	}

	uint32_t offset = seqdiff(hdr->seq, c->rcv.nxt);

	if(is_framed(c)) {
		if(offset) {
			handle_out_of_order_framed(c, offset, data, len);
		} else {
			handle_in_order_framed(c, data, len);
		}
	} else {
		if(offset) {
			handle_out_of_order(c, offset, data, len);
		} else {
			handle_in_order(c, data, len);
		}
	}
}


ssize_t utcp_recv(struct utcp *utcp, const void *data, size_t len) {
	const uint8_t *ptr = data;

	if(!utcp) {
		errno = EFAULT;
		return -1;
	}

	if(!len) {
		return 0;
	}

	if(!data) {
		errno = EFAULT;
		return -1;
	}

	// Drop packets smaller than the header

	struct hdr hdr;

	if(len < sizeof(hdr)) {
		print_packet(NULL, "recv", data, len);
		errno = EBADMSG;
		return -1;
	}

	// Make a copy from the potentially unaligned data to a struct hdr

	memcpy(&hdr, ptr, sizeof(hdr));

	// Try to match the packet to an existing connection

	struct utcp_connection *c = find_connection(utcp, hdr.dst, hdr.src);
	print_packet(c, "recv", data, len);

	// Process the header

	ptr += sizeof(hdr);
	len -= sizeof(hdr);

	// Drop packets with an unknown CTL flag

	if(hdr.ctl & ~(SYN | ACK | RST | FIN | MF)) {
		print_packet(NULL, "recv", data, len);
		errno = EBADMSG;
		return -1;
	}

	// Check for auxiliary headers

	const uint8_t *init = NULL;

	uint16_t aux = hdr.aux;

	while(aux) {
		size_t auxlen = 4 * (aux >> 8) & 0xf;
		uint8_t auxtype = aux & 0xff;

		if(len < auxlen) {
			errno = EBADMSG;
			return -1;
		}

		switch(auxtype) {
		case AUX_INIT:
			if(!(hdr.ctl & SYN) || auxlen != 4) {
				errno = EBADMSG;
				return -1;
			}

			init = ptr;
			break;

		default:
			errno = EBADMSG;
			return -1;
		}

		len -= auxlen;
		ptr += auxlen;

		if(!(aux & 0x800)) {
			break;
		}

		if(len < 2) {
			errno = EBADMSG;
			return -1;
		}

		memcpy(&aux, ptr, 2);
		len -= 2;
		ptr += 2;
	}

	bool has_data = len || (hdr.ctl & (SYN | FIN));

	// Is it for a new connection?

	if(!c) {
		// Ignore RST packets

		if(hdr.ctl & RST) {
			return 0;
		}

		// Is it a SYN packet and are we LISTENing?

		if(hdr.ctl & SYN && !(hdr.ctl & ACK) && utcp->accept) {
			// If we don't want to accept it, send a RST back
			if((utcp->pre_accept && !utcp->pre_accept(utcp, hdr.dst))) {
				len = 1;
				goto reset;
			}

			// Try to allocate memory, otherwise send a RST back
			c = allocate_connection(utcp, hdr.dst, hdr.src);

			if(!c) {
				len = 1;
				goto reset;
			}

			// Parse auxilliary information
			if(init) {
				if(init[0] < 1) {
					len = 1;
					goto reset;
				}

				c->flags = init[3] & 0x7;
			} else {
				c->flags = UTCP_TCP;
			}

synack:
			// Return SYN+ACK, go to SYN_RECEIVED state
			c->snd.wnd = hdr.wnd;
			c->rcv.irs = hdr.seq;
			c->rcv.nxt = c->rcv.irs + 1;
			set_state(c, SYN_RECEIVED);

			struct {
				struct hdr hdr;
				uint8_t data[4];
			} pkt;

			pkt.hdr.src = c->src;
			pkt.hdr.dst = c->dst;
			pkt.hdr.ack = c->rcv.irs + 1;
			pkt.hdr.seq = c->snd.iss;
			pkt.hdr.wnd = c->rcvbuf.maxsize;
			pkt.hdr.ctl = SYN | ACK;

			if(init) {
				pkt.hdr.aux = 0x0101;
				pkt.data[0] = 1;
				pkt.data[1] = 0;
				pkt.data[2] = 0;
				pkt.data[3] = c->flags & 0x7;
				print_packet(c, "send", &pkt, sizeof(hdr) + 4);
				utcp->send(utcp, &pkt, sizeof(hdr) + 4);
			} else {
				pkt.hdr.aux = 0;
				print_packet(c, "send", &pkt, sizeof(hdr));
				utcp->send(utcp, &pkt, sizeof(hdr));
			}

			start_retransmit_timer(c);
		} else {
			// No, we don't want your packets, send a RST back
			len = 1;
			goto reset;
		}

		return 0;
	}

	debug(c, "state %s\n", strstate[c->state]);

	// In case this is for a CLOSED connection, ignore the packet.
	// TODO: make it so incoming packets can never match a CLOSED connection.

	if(c->state == CLOSED) {
		debug(c, "got packet for closed connection\n");
		return 0;
	}

	// It is for an existing connection.

	// 1. Drop invalid packets.

	// 1a. Drop packets that should not happen in our current state.

	switch(c->state) {
	case SYN_SENT:
	case SYN_RECEIVED:
	case ESTABLISHED:
	case FIN_WAIT_1:
	case FIN_WAIT_2:
	case CLOSE_WAIT:
	case CLOSING:
	case LAST_ACK:
	case TIME_WAIT:
		break;

	default:
#ifdef UTCP_DEBUG
		abort();
#endif
		break;
	}

	// 1b. Discard data that is not in our receive window.

	if(is_reliable(c)) {
		bool acceptable;

		if(c->state == SYN_SENT) {
			acceptable = true;
		} else if(len == 0) {
			acceptable = seqdiff(hdr.seq, c->rcv.nxt) >= 0;
		} else {
			int32_t rcv_offset = seqdiff(hdr.seq, c->rcv.nxt);

			// cut already accepted front overlapping
			if(rcv_offset < 0) {
				acceptable = len > (size_t) - rcv_offset;

				if(acceptable) {
					ptr -= rcv_offset;
					len += rcv_offset;
					hdr.seq -= rcv_offset;
				}
			} else {
				acceptable = seqdiff(hdr.seq, c->rcv.nxt) >= 0 && seqdiff(hdr.seq, c->rcv.nxt) + len <= c->rcvbuf.maxsize;
			}
		}

		if(!acceptable) {
			debug(c, "packet not acceptable, %u <= %u + %lu < %u\n", c->rcv.nxt, hdr.seq, (unsigned long)len, c->rcv.nxt + c->rcvbuf.maxsize);

			// Ignore unacceptable RST packets.
			if(hdr.ctl & RST) {
				return 0;
			}

			// Otherwise, continue processing.
			len = 0;
		}
	} else {
#if UTCP_DEBUG
		int32_t rcv_offset = seqdiff(hdr.seq, c->rcv.nxt);

		if(rcv_offset) {
			debug(c, "packet out of order, offset %u bytes\n", rcv_offset);
		}

#endif
	}

	c->snd.wnd = hdr.wnd; // TODO: move below

	// 1c. Drop packets with an invalid ACK.
	// ackno should not roll back, and it should also not be bigger than what we ever could have sent
	// (= snd.una + c->sndbuf.used).

	if(!is_reliable(c)) {
		if(hdr.ack != c->snd.last && c->state >= ESTABLISHED) {
			hdr.ack = c->snd.una;
		}
	}

	if(hdr.ctl & ACK && (seqdiff(hdr.ack, c->snd.last) > 0 || seqdiff(hdr.ack, c->snd.una) < 0)) {
		debug(c, "packet ack seqno out of range, %u <= %u < %u\n", c->snd.una, hdr.ack, c->snd.una + c->sndbuf.used);

		// Ignore unacceptable RST packets.
		if(hdr.ctl & RST) {
			return 0;
		}

		goto reset;
	}

	// 2. Handle RST packets

	if(hdr.ctl & RST) {
		switch(c->state) {
		case SYN_SENT:
			if(!(hdr.ctl & ACK)) {
				return 0;
			}

			// The peer has refused our connection.
			set_state(c, CLOSED);
			errno = ECONNREFUSED;

			if(c->recv) {
				c->recv(c, NULL, 0);
			}

			if(c->poll && !c->reapable) {
				c->poll(c, 0);
			}

			return 0;

		case SYN_RECEIVED:
			if(hdr.ctl & ACK) {
				return 0;
			}

			// We haven't told the application about this connection yet. Silently delete.
			free_connection(c);
			return 0;

		case ESTABLISHED:
		case FIN_WAIT_1:
		case FIN_WAIT_2:
		case CLOSE_WAIT:
			if(hdr.ctl & ACK) {
				return 0;
			}

			// The peer has aborted our connection.
			set_state(c, CLOSED);
			errno = ECONNRESET;

			if(c->recv) {
				c->recv(c, NULL, 0);
			}

			if(c->poll && !c->reapable) {
				c->poll(c, 0);
			}

			return 0;

		case CLOSING:
		case LAST_ACK:
		case TIME_WAIT:
			if(hdr.ctl & ACK) {
				return 0;
			}

			// As far as the application is concerned, the connection has already been closed.
			// If it has called utcp_close() already, we can immediately free this connection.
			if(c->reapable) {
				free_connection(c);
				return 0;
			}

			// Otherwise, immediately move to the CLOSED state.
			set_state(c, CLOSED);
			return 0;

		default:
#ifdef UTCP_DEBUG
			abort();
#endif
			break;
		}
	}

	uint32_t advanced;

	if(!(hdr.ctl & ACK)) {
		advanced = 0;
		goto skip_ack;
	}

	// 3. Advance snd.una

	advanced = seqdiff(hdr.ack, c->snd.una);

	if(advanced) {
		// RTT measurement
		if(c->rtt_start.tv_sec) {
			if(c->rtt_seq == hdr.ack) {
				struct timespec now;
				clock_gettime(UTCP_CLOCK, &now);
				int32_t diff = timespec_diff_usec(&now, &c->rtt_start);
				update_rtt(c, diff);
				c->rtt_start.tv_sec = 0;
			} else if(c->rtt_seq < hdr.ack) {
				debug(c, "cancelling RTT measurement: %u < %u\n", c->rtt_seq, hdr.ack);
				c->rtt_start.tv_sec = 0;
			}
		}

		int32_t data_acked = advanced;

		switch(c->state) {
		case SYN_SENT:
		case SYN_RECEIVED:
			data_acked--;
			break;

		// TODO: handle FIN as well.
		default:
			break;
		}

		assert(data_acked >= 0);

#ifndef NDEBUG
		int32_t bufused = seqdiff(c->snd.last, c->snd.una);
		assert(data_acked <= bufused);
#endif

		if(data_acked) {
			buffer_discard(&c->sndbuf, data_acked);

			if(is_reliable(c)) {
				c->do_poll = true;
			}
		}

		// Also advance snd.nxt if possible
		if(seqdiff(c->snd.nxt, hdr.ack) < 0) {
			c->snd.nxt = hdr.ack;
		}

		c->snd.una = hdr.ack;

		if(c->dupack) {
			if(c->dupack >= 3) {
				debug(c, "fast recovery ended\n");
				c->snd.cwnd = c->snd.ssthresh;
			}

			c->dupack = 0;
		}

		// Increase the congestion window according to RFC 5681
		if(c->snd.cwnd < c->snd.ssthresh) {
			c->snd.cwnd += min(advanced, utcp->mss); // eq. 2
		} else {
			c->snd.cwnd += max(1, (utcp->mss * utcp->mss) / c->snd.cwnd); // eq. 3
		}

		if(c->snd.cwnd > c->sndbuf.maxsize) {
			c->snd.cwnd = c->sndbuf.maxsize;
		}

		debug_cwnd(c);

		// Check if we have sent a FIN that is now ACKed.
		switch(c->state) {
		case FIN_WAIT_1:
			if(c->snd.una == c->snd.last) {
				set_state(c, FIN_WAIT_2);
			}

			break;

		case CLOSING:
			if(c->snd.una == c->snd.last) {
				clock_gettime(UTCP_CLOCK, &c->conn_timeout);
				c->conn_timeout.tv_sec += utcp->timeout;
				set_state(c, TIME_WAIT);
			}

			break;

		default:
			break;
		}
	} else {
		if(!len && is_reliable(c) && c->snd.una != c->snd.last) {
			c->dupack++;
			debug(c, "duplicate ACK %d\n", c->dupack);

			if(c->dupack == 3) {
				// RFC 5681 fast recovery
				debug(c, "fast recovery started\n", c->dupack);
				uint32_t flightsize = seqdiff(c->snd.nxt, c->snd.una);
				c->snd.ssthresh = max(flightsize / 2, utcp->mss * 2); // eq. 4
				c->snd.cwnd = min(c->snd.ssthresh + 3 * utcp->mss, c->sndbuf.maxsize);

				if(c->snd.cwnd > c->sndbuf.maxsize) {
					c->snd.cwnd = c->sndbuf.maxsize;
				}

				debug_cwnd(c);

				fast_retransmit(c);
			} else if(c->dupack > 3) {
				c->snd.cwnd += utcp->mss;

				if(c->snd.cwnd > c->sndbuf.maxsize) {
					c->snd.cwnd = c->sndbuf.maxsize;
				}

				debug_cwnd(c);
			}

			// We got an ACK which indicates the other side did get one of our packets.
			// Reset the retransmission timer to avoid going to slow start,
			// but don't touch the connection timeout.
			start_retransmit_timer(c);
		}
	}

	// 4. Update timers

	if(advanced) {
		if(c->snd.una == c->snd.last) {
			stop_retransmit_timer(c);
			timespec_clear(&c->conn_timeout);
		} else if(is_reliable(c)) {
			start_retransmit_timer(c);
			clock_gettime(UTCP_CLOCK, &c->conn_timeout);
			c->conn_timeout.tv_sec += utcp->timeout;
		}
	}

skip_ack:
	// 5. Process SYN stuff

	if(hdr.ctl & SYN) {
		switch(c->state) {
		case SYN_SENT:

			// This is a SYNACK. It should always have ACKed the SYN.
			if(!advanced) {
				goto reset;
			}

			c->rcv.irs = hdr.seq;
			c->rcv.nxt = hdr.seq + 1;

			if(c->shut_wr) {
				c->snd.last++;
				set_state(c, FIN_WAIT_1);
			} else {
				c->do_poll = true;
				set_state(c, ESTABLISHED);
			}

			break;

		case SYN_RECEIVED:
			// This is a retransmit of a SYN, send back the SYNACK.
			goto synack;

		case ESTABLISHED:
		case FIN_WAIT_1:
		case FIN_WAIT_2:
		case CLOSE_WAIT:
		case CLOSING:
		case LAST_ACK:
		case TIME_WAIT:
			// This could be a retransmission. Ignore the SYN flag, but send an ACK back.
			break;

		default:
#ifdef UTCP_DEBUG
			abort();
#endif
			return 0;
		}
	}

	// 6. Process new data

	if(c->state == SYN_RECEIVED) {
		// This is the ACK after the SYNACK. It should always have ACKed the SYNACK.
		if(!advanced) {
			goto reset;
		}

		// Are we still LISTENing?
		if(utcp->accept) {
			utcp->accept(c, c->src);
		}

		if(c->state != ESTABLISHED) {
			set_state(c, CLOSED);
			c->reapable = true;
			goto reset;
		}
	}

	if(len) {
		switch(c->state) {
		case SYN_SENT:
		case SYN_RECEIVED:
			// This should never happen.
#ifdef UTCP_DEBUG
			abort();
#endif
			return 0;

		case ESTABLISHED:
		case FIN_WAIT_1:
		case FIN_WAIT_2:
			break;

		case CLOSE_WAIT:
		case CLOSING:
		case LAST_ACK:
		case TIME_WAIT:
			// Ehm no, We should never receive more data after a FIN.
			goto reset;

		default:
#ifdef UTCP_DEBUG
			abort();
#endif
			return 0;
		}

		handle_incoming_data(c, &hdr, ptr, len);
	}

	// 7. Process FIN stuff

	if((hdr.ctl & FIN) && (!is_reliable(c) || hdr.seq + len == c->rcv.nxt)) {
		switch(c->state) {
		case SYN_SENT:
		case SYN_RECEIVED:
			// This should never happen.
#ifdef UTCP_DEBUG
			abort();
#endif
			break;

		case ESTABLISHED:
			set_state(c, CLOSE_WAIT);
			break;

		case FIN_WAIT_1:
			set_state(c, CLOSING);
			break;

		case FIN_WAIT_2:
			clock_gettime(UTCP_CLOCK, &c->conn_timeout);
			c->conn_timeout.tv_sec += utcp->timeout;
			set_state(c, TIME_WAIT);
			break;

		case CLOSE_WAIT:
		case CLOSING:
		case LAST_ACK:
		case TIME_WAIT:
			// Ehm, no. We should never receive a second FIN.
			goto reset;

		default:
#ifdef UTCP_DEBUG
			abort();
#endif
			break;
		}

		// FIN counts as one sequence number
		c->rcv.nxt++;
		len++;

		// Inform the application that the peer closed its end of the connection.
		if(c->recv) {
			errno = 0;
			c->recv(c, NULL, 0);
		}
	}

	// Now we send something back if:
	// - we received data, so we have to send back an ACK
	//   -> sendatleastone = true
	// - or we got an ack, so we should maybe send a bit more data
	//   -> sendatleastone = false

	if(is_reliable(c) || hdr.ctl & SYN || hdr.ctl & FIN) {
		ack(c, has_data);
	}

	return 0;

reset:
	swap_ports(&hdr);
	hdr.wnd = 0;
	hdr.aux = 0;

	if(hdr.ctl & ACK) {
		hdr.seq = hdr.ack;
		hdr.ctl = RST;
	} else {
		hdr.ack = hdr.seq + len;
		hdr.seq = 0;
		hdr.ctl = RST | ACK;
	}

	print_packet(c, "send", &hdr, sizeof(hdr));
	utcp->send(utcp, &hdr, sizeof(hdr));
	return 0;

}

int utcp_shutdown(struct utcp_connection *c, int dir) {
	debug(c, "shutdown %d at %u\n", dir, c ? c->snd.last : 0);

	if(!c) {
		errno = EFAULT;
		return -1;
	}

	if(c->reapable) {
		debug(c, "shutdown() called on closed connection\n");
		errno = EBADF;
		return -1;
	}

	if(!(dir == UTCP_SHUT_RD || dir == UTCP_SHUT_WR || dir == UTCP_SHUT_RDWR)) {
		errno = EINVAL;
		return -1;
	}

	// TCP does not have a provision for stopping incoming packets.
	// The best we can do is to just ignore them.
	if(dir == UTCP_SHUT_RD || dir == UTCP_SHUT_RDWR) {
		c->recv = NULL;
	}

	// The rest of the code deals with shutting down writes.
	if(dir == UTCP_SHUT_RD) {
		return 0;
	}

	// Only process shutting down writes once.
	if(c->shut_wr) {
		return 0;
	}

	c->shut_wr = true;

	switch(c->state) {
	case CLOSED:
	case LISTEN:
		errno = ENOTCONN;
		return -1;

	case SYN_SENT:
		return 0;

	case SYN_RECEIVED:
	case ESTABLISHED:
		if(!is_reliable(c) && is_framed(c)) {
			flush_unreliable_framed(c);
		}

		set_state(c, FIN_WAIT_1);
		break;

	case FIN_WAIT_1:
	case FIN_WAIT_2:
		return 0;

	case CLOSE_WAIT:
		set_state(c, CLOSING);
		break;

	case CLOSING:
	case LAST_ACK:
	case TIME_WAIT:
		return 0;
	}

	c->snd.last++;

	ack(c, !is_reliable(c));

	if(!timespec_isset(&c->rtrx_timeout)) {
		start_retransmit_timer(c);
	}

	return 0;
}

static bool reset_connection(struct utcp_connection *c) {
	if(!c) {
		errno = EFAULT;
		return false;
	}

	if(c->reapable) {
		debug(c, "abort() called on closed connection\n");
		errno = EBADF;
		return false;
	}

	c->recv = NULL;
	c->poll = NULL;

	switch(c->state) {
	case CLOSED:
		return true;

	case LISTEN:
	case SYN_SENT:
	case CLOSING:
	case LAST_ACK:
	case TIME_WAIT:
		set_state(c, CLOSED);
		return true;

	case SYN_RECEIVED:
	case ESTABLISHED:
	case FIN_WAIT_1:
	case FIN_WAIT_2:
	case CLOSE_WAIT:
		set_state(c, CLOSED);
		break;
	}

	// Send RST

	struct hdr hdr;

	hdr.src = c->src;
	hdr.dst = c->dst;
	hdr.seq = c->snd.nxt;
	hdr.ack = 0;
	hdr.wnd = 0;
	hdr.ctl = RST;

	print_packet(c, "send", &hdr, sizeof(hdr));
	c->utcp->send(c->utcp, &hdr, sizeof(hdr));
	return true;
}

// Closes all the opened connections
void utcp_abort_all_connections(struct utcp *utcp) {
	if(!utcp) {
		errno = EINVAL;
		return;
	}

	for(int i = 0; i < utcp->nconnections; i++) {
		struct utcp_connection *c = utcp->connections[i];

		if(c->reapable || c->state == CLOSED) {
			continue;
		}

		utcp_recv_t old_recv = c->recv;
		utcp_poll_t old_poll = c->poll;

		reset_connection(c);

		if(old_recv) {
			errno = 0;
			old_recv(c, NULL, 0);
		}

		if(old_poll && !c->reapable) {
			errno = 0;
			old_poll(c, 0);
		}
	}

	return;
}

int utcp_close(struct utcp_connection *c) {
	debug(c, "closing\n");

	if(c->rcvbuf.used) {
		debug(c, "receive buffer not empty, resetting\n");
		return reset_connection(c) ? 0 : -1;
	}

	if(utcp_shutdown(c, SHUT_RDWR) && errno != ENOTCONN) {
		return -1;
	}

	c->recv = NULL;
	c->poll = NULL;
	c->reapable = true;
	return 0;
}

int utcp_abort(struct utcp_connection *c) {
	if(!reset_connection(c)) {
		return -1;
	}

	c->reapable = true;
	return 0;
}

/* Handle timeouts.
 * One call to this function will loop through all connections,
 * checking if something needs to be resent or not.
 * The return value is the time to the next timeout in milliseconds,
 * or maybe a negative value if the timeout is infinite.
 */
struct timespec utcp_timeout(struct utcp *utcp) {
	struct timespec now;
	clock_gettime(UTCP_CLOCK, &now);
	struct timespec next = {now.tv_sec + 3600, now.tv_nsec};

	for(int i = 0; i < utcp->nconnections; i++) {
		struct utcp_connection *c = utcp->connections[i];

		if(!c) {
			continue;
		}

		// delete connections that have been utcp_close()d.
		if(c->state == CLOSED) {
			if(c->reapable) {
				debug(c, "reaping\n");
				free_connection(c);
				i--;
			}

			continue;
		}

		if(timespec_isset(&c->conn_timeout) && timespec_lt(&c->conn_timeout, &now)) {
			errno = ETIMEDOUT;
			c->state = CLOSED;

			if(c->recv) {
				c->recv(c, NULL, 0);
			}

			if(c->poll && !c->reapable) {
				c->poll(c, 0);
			}

			continue;
		}

		if(timespec_isset(&c->rtrx_timeout) && timespec_lt(&c->rtrx_timeout, &now)) {
			debug(c, "retransmitting after timeout\n");
			retransmit(c);
		}

		if(c->poll) {
			if((c->state == ESTABLISHED || c->state == CLOSE_WAIT) && c->do_poll) {
				c->do_poll = false;
				uint32_t len = buffer_free(&c->sndbuf);

				if(len) {
					c->poll(c, len);
				}
			} else if(c->state == CLOSED) {
				c->poll(c, 0);
			}
		}

		if(timespec_isset(&c->conn_timeout) && timespec_lt(&c->conn_timeout, &next)) {
			next = c->conn_timeout;
		}

		if(timespec_isset(&c->rtrx_timeout) && timespec_lt(&c->rtrx_timeout, &next)) {
			next = c->rtrx_timeout;
		}
	}

	struct timespec diff;

	timespec_sub(&next, &now, &diff);

	return diff;
}

bool utcp_is_active(struct utcp *utcp) {
	if(!utcp) {
		return false;
	}

	for(int i = 0; i < utcp->nconnections; i++)
		if(utcp->connections[i]->state != CLOSED && utcp->connections[i]->state != TIME_WAIT) {
			return true;
		}

	return false;
}

struct utcp *utcp_init(utcp_accept_t accept, utcp_pre_accept_t pre_accept, utcp_send_t send, void *priv) {
	if(!send) {
		errno = EFAULT;
		return NULL;
	}

	struct utcp *utcp = calloc(1, sizeof(*utcp));

	if(!utcp) {
		return NULL;
	}

	utcp_set_mtu(utcp, DEFAULT_MTU);

	if(!utcp->pkt) {
		free(utcp);
		return NULL;
	}

	if(!CLOCK_GRANULARITY) {
		struct timespec res;
		clock_getres(UTCP_CLOCK, &res);
		CLOCK_GRANULARITY = res.tv_sec * USEC_PER_SEC + res.tv_nsec / 1000;
	}

	utcp->accept = accept;
	utcp->pre_accept = pre_accept;
	utcp->send = send;
	utcp->priv = priv;
	utcp->timeout = DEFAULT_USER_TIMEOUT; // sec

	return utcp;
}

void utcp_exit(struct utcp *utcp) {
	if(!utcp) {
		return;
	}

	for(int i = 0; i < utcp->nconnections; i++) {
		struct utcp_connection *c = utcp->connections[i];

		if(!c->reapable) {
			if(c->recv) {
				c->recv(c, NULL, 0);
			}

			if(c->poll && !c->reapable) {
				c->poll(c, 0);
			}
		}

		buffer_exit(&c->rcvbuf);
		buffer_exit(&c->sndbuf);
		free(c);
	}

	free(utcp->connections);
	free(utcp->pkt);
	free(utcp);
}

uint16_t utcp_get_mtu(struct utcp *utcp) {
	return utcp ? utcp->mtu : 0;
}

uint16_t utcp_get_mss(struct utcp *utcp) {
	return utcp ? utcp->mss : 0;
}

void utcp_set_mtu(struct utcp *utcp, uint16_t mtu) {
	if(!utcp) {
		return;
	}

	if(mtu <= sizeof(struct hdr)) {
		return;
	}

	if(mtu > utcp->mtu) {
		char *new = realloc(utcp->pkt, mtu + sizeof(struct hdr));

		if(!new) {
			return;
		}

		utcp->pkt = new;
	}

	utcp->mtu = mtu;
	utcp->mss = mtu - sizeof(struct hdr);
}

void utcp_reset_timers(struct utcp *utcp) {
	if(!utcp) {
		return;
	}

	struct timespec now, then;

	clock_gettime(UTCP_CLOCK, &now);

	then = now;

	then.tv_sec += utcp->timeout;

	for(int i = 0; i < utcp->nconnections; i++) {
		struct utcp_connection *c = utcp->connections[i];

		if(c->reapable) {
			continue;
		}

		if(timespec_isset(&c->rtrx_timeout)) {
			c->rtrx_timeout = now;
		}

		if(timespec_isset(&c->conn_timeout)) {
			c->conn_timeout = then;
		}

		c->rtt_start.tv_sec = 0;

		if(c->rto > START_RTO) {
			c->rto = START_RTO;
		}
	}
}

int utcp_get_user_timeout(struct utcp *u) {
	return u ? u->timeout : 0;
}

void utcp_set_user_timeout(struct utcp *u, int timeout) {
	if(u) {
		u->timeout = timeout;
	}
}

size_t utcp_get_sndbuf(struct utcp_connection *c) {
	return c ? c->sndbuf.maxsize : 0;
}

size_t utcp_get_sndbuf_free(struct utcp_connection *c) {
	if(!c) {
		return 0;
	}

	switch(c->state) {
	case SYN_SENT:
	case SYN_RECEIVED:
	case ESTABLISHED:
	case CLOSE_WAIT:
		return buffer_free(&c->sndbuf);

	default:
		return 0;
	}
}

void utcp_set_sndbuf(struct utcp_connection *c, size_t size) {
	if(!c) {
		return;
	}

	c->sndbuf.maxsize = size;

	if(c->sndbuf.maxsize != size) {
		c->sndbuf.maxsize = -1;
	}

	c->do_poll = is_reliable(c) && buffer_free(&c->sndbuf);
}

size_t utcp_get_rcvbuf(struct utcp_connection *c) {
	return c ? c->rcvbuf.maxsize : 0;
}

size_t utcp_get_rcvbuf_free(struct utcp_connection *c) {
	if(c && (c->state == ESTABLISHED || c->state == CLOSE_WAIT)) {
		return buffer_free(&c->rcvbuf);
	} else {
		return 0;
	}
}

void utcp_set_rcvbuf(struct utcp_connection *c, size_t size) {
	if(!c) {
		return;
	}

	c->rcvbuf.maxsize = size;

	if(c->rcvbuf.maxsize != size) {
		c->rcvbuf.maxsize = -1;
	}
}

size_t utcp_get_sendq(struct utcp_connection *c) {
	return c->sndbuf.used;
}

size_t utcp_get_recvq(struct utcp_connection *c) {
	return c->rcvbuf.used;
}

bool utcp_get_nodelay(struct utcp_connection *c) {
	return c ? c->nodelay : false;
}

void utcp_set_nodelay(struct utcp_connection *c, bool nodelay) {
	if(c) {
		c->nodelay = nodelay;
	}
}

bool utcp_get_keepalive(struct utcp_connection *c) {
	return c ? c->keepalive : false;
}

void utcp_set_keepalive(struct utcp_connection *c, bool keepalive) {
	if(c) {
		c->keepalive = keepalive;
	}
}

size_t utcp_get_outq(struct utcp_connection *c) {
	return c ? seqdiff(c->snd.nxt, c->snd.una) : 0;
}

void utcp_set_recv_cb(struct utcp_connection *c, utcp_recv_t recv) {
	if(c) {
		c->recv = recv;
	}
}

void utcp_set_poll_cb(struct utcp_connection *c, utcp_poll_t poll) {
	if(c) {
		c->poll = poll;
		c->do_poll = is_reliable(c) && buffer_free(&c->sndbuf);
	}
}

void utcp_set_accept_cb(struct utcp *utcp, utcp_accept_t accept, utcp_pre_accept_t pre_accept) {
	if(utcp) {
		utcp->accept = accept;
		utcp->pre_accept = pre_accept;
	}
}

void utcp_expect_data(struct utcp_connection *c, bool expect) {
	if(!c || c->reapable) {
		return;
	}

	if(!(c->state == ESTABLISHED || c->state == FIN_WAIT_1 || c->state == FIN_WAIT_2)) {
		return;
	}

	if(expect) {
		// If we expect data, start the connection timer.
		if(!timespec_isset(&c->conn_timeout)) {
			clock_gettime(UTCP_CLOCK, &c->conn_timeout);
			c->conn_timeout.tv_sec += c->utcp->timeout;
		}
	} else {
		// If we want to cancel expecting data, only clear the timer when there is no unACKed data.
		if(c->snd.una == c->snd.last) {
			timespec_clear(&c->conn_timeout);
		}
	}
}

void utcp_offline(struct utcp *utcp, bool offline) {
	struct timespec now;
	clock_gettime(UTCP_CLOCK, &now);

	for(int i = 0; i < utcp->nconnections; i++) {
		struct utcp_connection *c = utcp->connections[i];

		if(c->reapable) {
			continue;
		}

		utcp_expect_data(c, offline);

		if(!offline) {
			if(timespec_isset(&c->rtrx_timeout)) {
				c->rtrx_timeout = now;
			}

			utcp->connections[i]->rtt_start.tv_sec = 0;

			if(c->rto > START_RTO) {
				c->rto = START_RTO;
			}
		}
	}
}

void utcp_set_retransmit_cb(struct utcp *utcp, utcp_retransmit_t cb) {
	utcp->retransmit = cb;
}

void utcp_set_clock_granularity(long granularity) {
	CLOCK_GRANULARITY = granularity;
}

int utcp_get_flush_timeout(struct utcp *utcp) {
	return utcp->flush_timeout;
}

void utcp_set_flush_timeout(struct utcp *utcp, int milliseconds) {
	utcp->flush_timeout = milliseconds;
}
