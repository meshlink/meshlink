/*
    dns.c -- hostname resolving functions
    Copyright (C) 2019 Guus Sliepen <guus@meshlink.io>

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

#include <pthread.h>

#include "adns.h"
#include "logger.h"
#include "xalloc.h"

typedef struct adns_item {
	adns_cb_t cb;
	void *data;
	time_t deadline;
	struct addrinfo *ai;
	int err;
	char *host;
	char *serv;
} adns_item_t;

static void *adns_loop(void *data) {
	meshlink_handle_t *mesh = data;

	while(true) {
		adns_item_t *item = meshlink_queue_pop_cond(&mesh->adns_queue, &mesh->adns_cond);

		if(!item) {
			break;
		}

		if(time(NULL) < item->deadline) {
			logger(mesh, MESHLINK_DEBUG, "Resolving %s port %s", item->host, item->serv);
			int result = getaddrinfo(item->host, item->serv, NULL, &item->ai);

			if(result) {
				item->ai = NULL;
				item->err = errno;
			}
		} else {
			logger(mesh, MESHLINK_WARNING, "Deadline passed for DNS request %s port %s", item->host, item->serv);
			item->ai = NULL;
			item->err = ETIMEDOUT;
		}

		if(meshlink_queue_push(&mesh->adns_done_queue, item)) {
			signal_trigger(&mesh->loop, &mesh->adns_signal);
		} else {
			free(item->host);
			free(item->serv);
			free(item);
		}
	}

	return NULL;
}

static void adns_cb_handler(event_loop_t *loop, void *data) {
	(void)loop;
	meshlink_handle_t *mesh = data;

	for(adns_item_t *item; (item = meshlink_queue_pop(&mesh->adns_done_queue));) {
		item->cb(mesh, item->host, item->serv, item->data, item->ai, item->err);
		free(item);
	}
}

extern void init_adns(meshlink_handle_t *mesh) {
	signal_add(&mesh->loop, &mesh->adns_signal, adns_cb_handler, mesh, 1);
	meshlink_queue_init(&mesh->adns_queue);
	pthread_create(&mesh->adns_thread, NULL, adns_loop, mesh);
}

extern void exit_adns(meshlink_handle_t *mesh) {
	if(!mesh->adns_signal.cb) {
		return;
	}

	/* Drain the queue of any pending ADNS requests */
	for(adns_item_t *item; (item = meshlink_queue_pop(&mesh->adns_queue));) {
		free(item->host);
		free(item->serv);
		free(item);
	}

	/* Signal the ADNS thread to stop */
	if(!meshlink_queue_push(&mesh->adns_queue, NULL)) {
		abort();
	}

	pthread_cond_signal(&mesh->adns_cond);

	pthread_join(mesh->adns_thread, NULL);
	meshlink_queue_exit(&mesh->adns_queue);
	signal_del(&mesh->loop, &mesh->adns_signal);
}

extern void adns_queue(meshlink_handle_t *mesh, char *host, char *serv, adns_cb_t cb, void *data, int timeout) {
	adns_item_t *item = xmalloc(sizeof(*item));
	item->cb = cb;
	item->data = data;
	item->deadline = time(NULL) + timeout;
	item->host = host;
	item->serv = serv;

	logger(mesh, MESHLINK_DEBUG, "Enqueueing DNS request for %s port %s", item->host, item->serv);

	if(!meshlink_queue_push(&mesh->adns_queue, item)) {
		abort();
	}

	pthread_cond_signal(&mesh->adns_cond);
}
