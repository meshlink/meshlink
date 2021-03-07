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
#include "devtools.h"
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
			devtool_adns_resolve_probe();
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

void init_adns(meshlink_handle_t *mesh) {
	meshlink_queue_init(&mesh->adns_queue);
	meshlink_queue_init(&mesh->adns_done_queue);
	signal_add(&mesh->loop, &mesh->adns_signal, adns_cb_handler, mesh, 1);
	pthread_create(&mesh->adns_thread, NULL, adns_loop, mesh);
}

void exit_adns(meshlink_handle_t *mesh) {
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

void adns_queue(meshlink_handle_t *mesh, char *host, char *serv, adns_cb_t cb, void *data, int timeout) {
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

struct adns_blocking_info {
	meshlink_handle_t *mesh;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	char *host;
	char *serv;
	struct addrinfo *ai;
	int socktype;
	bool done;
};

static void *adns_blocking_handler(void *data) {
	struct adns_blocking_info *info = data;

	logger(info->mesh, MESHLINK_WARNING, "Resolving %s port %s", info->host, info->serv);
	devtool_adns_resolve_probe();

	struct addrinfo hint = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = info->socktype,
	};

	int result = getaddrinfo(info->host, info->serv, &hint, &info->ai);

	if(result) {
		logger(info->mesh, MESHLINK_ERROR, "getaddrinfo(%s, %s) returned an error: %s", info->host, info->serv, gai_strerror(result));
		info->ai = NULL;
	}

	if(pthread_mutex_lock(&info->mutex) != 0) {
		abort();
	}

	bool cleanup = info->done;

	if(!info->done) {
		logger(info->mesh, MESHLINK_WARNING, "getaddrinfo(%s, %s) returned before waiter timed out", info->host, info->serv);
		info->done = true;
		pthread_cond_signal(&info->cond);
	} else {
		logger(info->mesh, MESHLINK_WARNING, "getaddrinfo(%s, %s) returned after waiter timed out", info->host, info->serv);
	}

	pthread_mutex_unlock(&info->mutex);

	if(cleanup) {
		free(info->host);
		free(info->serv);
		free(info);
	}

	return NULL;
}

struct addrinfo *adns_blocking_request(meshlink_handle_t *mesh, char *host, char *serv, int socktype, int timeout) {
	struct adns_blocking_info *info = xzalloc(sizeof(*info));

	info->mesh = mesh;
	info->host = host;
	info->serv = serv;
	info->socktype = socktype;
	pthread_mutex_init(&info->mutex, NULL);
	pthread_cond_init(&info->cond, NULL);

	struct timespec deadline;
	clock_gettime(CLOCK_REALTIME, &deadline);
	deadline.tv_sec += timeout;

	logger(mesh, MESHLINK_WARNING, "Starting blocking DNS request for %s port %s", host, serv);

	pthread_t thread;

	if(pthread_create(&thread, NULL, adns_blocking_handler, info)) {
		free(info->host);
		free(info->serv);
		free(info);
		return NULL;
	} else {
		pthread_detach(thread);
	}

	if(pthread_mutex_lock(&info->mutex) != 0) {
		abort();
	}

	pthread_cond_timedwait(&info->cond, &info->mutex, &deadline);

	struct addrinfo *result = NULL;
	bool cleanup = info->done;

	if(info->done) {
		logger(mesh, MESHLINK_WARNING, "DNS request for %s port %s fulfilled in time, ai = %p", host, serv, (void *)info->ai);
		result = info->ai;
	} else {
		logger(mesh, MESHLINK_ERROR, "Deadline passed for DNS request %s port %s, ai = %p", host, serv, (void *)info->ai);
		info->done = true;
	}

	pthread_mutex_unlock(&info->mutex);

	if(cleanup) {
		free(info->host);
		free(info->serv);
		free(info);
	}

	return result;
}
