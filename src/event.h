#ifndef MESHLINK_EVENT_H
#define MESHLINK_EVENT_H

/*
    event.h -- I/O, timeout and signal event handling
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

#include "splay_tree.h"
#include "system.h"
#include <pthread.h>

#define IO_READ 1
#define IO_WRITE 2

typedef struct event_loop_t event_loop_t;

typedef void (*io_cb_t)(event_loop_t *loop, void *data, int flags);
typedef void (*timeout_cb_t)(event_loop_t *loop, void *data);
typedef void (*signal_cb_t)(event_loop_t *loop, void *data);
typedef struct timespec(*idle_cb_t)(event_loop_t *loop, void *data);

typedef struct io_t {
	struct splay_node_t node;
	int fd;
	int flags;
	io_cb_t cb;
	void *data;
} io_t;

typedef struct timeout_t {
	struct splay_node_t node;
	struct timespec tv;
	timeout_cb_t cb;
	void *data;
} timeout_t;

typedef struct signal_t {
	struct splay_node_t node;
	int signum;
#ifdef HAVE_STDATOMIC_H
	volatile atomic_flag set;
#endif
	signal_cb_t cb;
	void *data;
} signal_t;

struct event_loop_t {
	void *data;

	volatile bool running;
	bool deletion;

	struct timespec now;

	splay_tree_t timeouts;
	idle_cb_t idle_cb;
	void *idle_data;
	splay_tree_t ios;
	splay_tree_t signals;

	fd_set readfds;
	fd_set writefds;

	io_t signalio;
	int pipefd[2];
};

void io_add(event_loop_t *loop, io_t *io, io_cb_t cb, void *data, int fd, int flags);
void io_del(event_loop_t *loop, io_t *io);
void io_set(event_loop_t *loop, io_t *io, int flags);

void timeout_add(event_loop_t *loop, timeout_t *timeout, timeout_cb_t cb, void *data, struct timespec *tv);
void timeout_del(event_loop_t *loop, timeout_t *timeout);
void timeout_set(event_loop_t *loop, timeout_t *timeout, struct timespec *tv);

void signal_add(event_loop_t *loop, signal_t *sig, signal_cb_t cb, void *data, uint8_t signum);
void signal_trigger(event_loop_t *loop, signal_t *sig);
void signal_del(event_loop_t *loop, signal_t *sig);

void idle_set(event_loop_t *loop, idle_cb_t cb, void *data);

void event_loop_init(event_loop_t *loop);
void event_loop_exit(event_loop_t *loop);
bool event_loop_run(event_loop_t *loop, pthread_mutex_t *mutex) __attribute__((__warn_unused_result__));
void event_loop_flush_output(event_loop_t *loop);
void event_loop_start(event_loop_t *loop);
void event_loop_stop(event_loop_t *loop);

#endif
