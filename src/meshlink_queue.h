#ifndef MESHLINK_QUEUE_H
#define MESHLINK_QUEUE_H

/*
    queue.h -- Thread-safe queue
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

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>

typedef struct meshlink_queue {
	struct meshlink_queue_item *head;
	struct meshlink_queue_item *tail;
	pthread_mutex_t mutex;
} meshlink_queue_t;

typedef struct meshlink_queue_item {
	void *data;
	struct meshlink_queue_item *next;
} meshlink_queue_item_t;

static inline void meshlink_queue_init(meshlink_queue_t *queue) {
	queue->head = NULL;
	queue->tail = NULL;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);
	pthread_mutex_init(&queue->mutex, &attr);
}

static inline void meshlink_queue_exit(meshlink_queue_t *queue) {
	pthread_mutex_destroy(&queue->mutex);
}

static inline bool meshlink_queue_push(meshlink_queue_t *queue, void *data) {
	meshlink_queue_item_t *item = malloc(sizeof(*item));

	if(!item) {
		return false;
	}

	item->data = data;
	item->next = NULL;
	pthread_mutex_lock(&queue->mutex);

	if(!queue->tail) {
		queue->head = queue->tail = item;
	} else {
		queue->tail = queue->tail->next = item;
	}

	pthread_mutex_unlock(&queue->mutex);
	return true;
}

static inline void *meshlink_queue_pop(meshlink_queue_t *queue) {
	meshlink_queue_item_t *item;
	void *data;
	pthread_mutex_lock(&queue->mutex);

	if((item = queue->head)) {
		queue->head = item->next;

		if(!queue->head) {
			queue->tail = NULL;
		}
	}

	pthread_mutex_unlock(&queue->mutex);
	data = item ? item->data : NULL;
	free(item);
	return data;
}

#endif
