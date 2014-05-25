/*
    event.c -- I/O and timeout event handling
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

#include "system.h"

#include "dropin.h"
#include "event.h"
#include "net.h"
#include "splay_tree.h"
#include "utils.h"
#include "xalloc.h"

static int io_compare(const io_t *a, const io_t *b) {
	return a->fd - b->fd;
}

static int timeout_compare(const timeout_t *a, const timeout_t *b) {
	struct timeval diff;
	timersub(&a->tv, &b->tv, &diff);
	if(diff.tv_sec < 0)
		return -1;
	if(diff.tv_sec > 0)
		return 1;
	if(diff.tv_usec < 0)
		return -1;
	if(diff.tv_usec > 0)
		return 1;
	if(a < b)
		return -1;
	if(a > b)
		return 1;
	return 0;
}

void io_add(event_loop_t *loop, io_t *io, io_cb_t cb, void *data, int fd, int flags) {
	if(io->cb)
		return;

	io->fd = fd;
	io->cb = cb;
	io->data = data;
	io->node.data = io;

	io_set(loop, io, flags);

	if(!splay_insert_node(&loop->ios, &io->node))
		abort();
}

void io_set(event_loop_t *loop, io_t *io, int flags) {
	io->flags = flags;

	if(flags & IO_READ)
		FD_SET(io->fd, &loop->readfds);
	else
		FD_CLR(io->fd, &loop->readfds);

	if(flags & IO_WRITE)
		FD_SET(io->fd, &loop->writefds);
	else
		FD_CLR(io->fd, &loop->writefds);
}

void io_del(event_loop_t *loop, io_t *io) {
	if(!io->cb)
		return;

	io_set(loop, io, 0);

	splay_unlink_node(&loop->ios, &io->node);
	io->cb = NULL;
}

void timeout_add(event_loop_t *loop, timeout_t *timeout, timeout_cb_t cb, void *data, struct timeval *tv) {
	timeout->cb = cb;
	timeout->data = data;
	timeout->node.data = timeout;

	timeout_set(loop, timeout, tv);
}

void timeout_set(event_loop_t *loop, timeout_t *timeout, struct timeval *tv) {
	if(timerisset(&timeout->tv))
		splay_unlink_node(&loop->timeouts, &timeout->node);

	if(!loop->now.tv_sec)
		gettimeofday(&loop->now, NULL);

	timeradd(&loop->now, tv, &timeout->tv);

	if(!splay_insert_node(&loop->timeouts, &timeout->node))
		abort();
}

void timeout_del(event_loop_t *loop, timeout_t *timeout) {
	if(!timeout->cb)
		return;

	splay_unlink_node(&loop->timeouts, &timeout->node);
	timeout->cb = 0;
	timeout->tv = (struct timeval){0, 0};
}

bool event_loop_run(event_loop_t *loop) {
	loop->running = true;

	fd_set readable;
	fd_set writable;

	while(loop->running) {
		gettimeofday(&loop->now, NULL);
		struct timeval diff, *tv = NULL;

		while(loop->timeouts.head) {
			timeout_t *timeout = loop->timeouts.head->data;
			timersub(&timeout->tv, &loop->now, &diff);

			if(diff.tv_sec < 0) {
				timeout->cb(loop, timeout->data);
				if(timercmp(&timeout->tv, &loop->now, <))
					timeout_del(loop, timeout);
			} else {
				tv = &diff;
				break;
			}
		}

		memcpy(&readable, &loop->readfds, sizeof readable);
		memcpy(&writable, &loop->writefds, sizeof writable);

		int fds = 0;

		if(loop->ios.tail) {
			io_t *last = loop->ios.tail->data;
			fds = last->fd + 1;
		}

		int n = select(fds, &readable, &writable, NULL, tv);

		if(n < 0) {
			if(sockwouldblock(errno))
				continue;
			else
				return false;
		}

		if(!n)
			continue;

		for splay_each(io_t, io, &loop->ios) {
			if(FD_ISSET(io->fd, &writable))
				io->cb(loop, io->data, IO_WRITE);
			else if(FD_ISSET(io->fd, &readable))
				io->cb(loop, io->data, IO_READ);
		}
	}

	return true;
}

void event_flush_output(event_loop_t *loop) {
	for splay_each(io_t, io, &loop->ios)
		if(FD_ISSET(io->fd, &loop->writefds))
			io->cb(loop, io->data, IO_WRITE);
}

void event_loop_stop(event_loop_t *loop) {
	loop->running = false;
}

void event_loop_init(event_loop_t *loop) {
	loop->ios.compare = (splay_compare_t)io_compare;
	loop->timeouts.compare = (splay_compare_t)timeout_compare;
	gettimeofday(&loop->now, NULL);
}

void event_loop_exit(event_loop_t *loop) {
	for splay_each(io_t, io, &loop->ios)
		splay_free_node(&loop->ios, node);
	for splay_each(timeout_t, timeout, &loop->timeouts)
		splay_free_node(&loop->timeouts, node);
}
