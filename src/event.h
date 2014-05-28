/*
    event.h -- I/O, timeout and signal event handling
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

#ifndef __MESHLINK_EVENT_H__
#define __MESHLINK_EVENT_H__

#include "splay_tree.h"

#define IO_READ 1
#define IO_WRITE 2

typedef struct event_loop_t event_loop_t;

typedef void (*io_cb_t)(event_loop_t *loop, void *data, int flags);
typedef void (*timeout_cb_t)(event_loop_t *loop, void *data);
typedef void (*signal_cb_t)(event_loop_t *loop, void *data);

typedef struct io_t {
	int fd;
	int flags;
	io_cb_t cb;
	void *data;
	struct splay_node_t node;
} io_t;

typedef struct timeout_t {
	struct timeval tv;
	timeout_cb_t cb;
	void *data;
	struct splay_node_t node;
} timeout_t;

typedef struct signal_t {
	int signum;
	signal_cb_t cb;
	void *data;
	struct splay_node_t node;
} signal_t;

struct event_loop_t {
	fd_set readfds;
	fd_set writefds;

	volatile bool running;
	struct timeval now;
	
	splay_tree_t ios;
	splay_tree_t timeouts;
	splay_tree_t signals;

	io_t signalio;
	int pipefd[2];

	void *data;
};

extern void io_add(event_loop_t *loop, io_t *io, io_cb_t cb, void *data, int fd, int flags);
extern void io_del(event_loop_t *loop, io_t *io);
extern void io_set(event_loop_t *loop, io_t *io, int flags);

extern void timeout_add(event_loop_t *loop, timeout_t *timeout, timeout_cb_t cb, void *data, struct timeval *tv);
extern void timeout_del(event_loop_t *loop, timeout_t *timeout);
extern void timeout_set(event_loop_t *loop, timeout_t *timeout, struct timeval *tv);

extern void signal_add(event_loop_t *loop, signal_t *sig, signal_cb_t cb, void *data, uint8_t signum);
extern void signal_trigger(event_loop_t *loop, signal_t *sig);
extern void signal_del(event_loop_t *loop, signal_t *sig);

extern void event_loop_init(event_loop_t *loop);
extern void event_loop_exit(event_loop_t *loop);
extern bool event_loop_run(event_loop_t *loop);
extern void event_loop_flush_output(event_loop_t *loop);
extern void event_loop_stop(event_loop_t *loop);

#endif
