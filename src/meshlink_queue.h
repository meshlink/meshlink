/*
    queue.h -- Thread-safe queue
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

#ifndef MESHLINK_QUEUE_H
#define MESHLINK_QUEUE_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>

#define MESHLINK_MUTEX_LOCK(mutex)   { if(pthread_mutex_lock(mutex) != 0)   { fprintf(stderr, "%s:%d - could not lock mutex!!!\n",   __FILE__, __LINE__); abort(); } }
#define MESHLINK_MUTEX_UNLOCK(mutex) { if(pthread_mutex_unlock(mutex) != 0) { fprintf(stderr, "%s:%d - could not unlock mutex!!!\n", __FILE__, __LINE__); abort(); } }

typedef void (*meshlink_queue_action_t)(const void *);

typedef struct meshlink_queue {
	struct meshlink_queue_item *head;	
	struct meshlink_queue_item *tail;	
	// pthread_mutex_t mutex;
} meshlink_queue_t;

typedef struct meshlink_queue_item {
	void *data;
	struct meshlink_queue_item *next;
} meshlink_queue_item_t;

/**
 * Insert data into first-in-first-out queue.
 * Caller of meshlink_queue_pop is respsonsible for free-ing data.
 */
static inline bool meshlink_queue_push(meshlink_queue_t *queue, void *data) {
	meshlink_queue_item_t *item = malloc(sizeof *item);
	if(!item)
		return false;
	item->data = data;
	item->next = NULL;
	//MESHLINK_MUTEX_LOCK(&queue->mutex);
	if(!queue->tail)
		queue->head = queue->tail = item;
	else
		queue->tail = queue->tail->next = item;
	//MESHLINK_MUTEX_UNLOCK(&queue->mutex);
	return true;
}

/**
 * Internal function to get next meshlink_queue_item from queue.
 * Caller must free() returned meshlink_queue_item->data (or do something with it).
 *
 * Use meshlink_queue_pop(queue) if you just want the data, not the queue_item.
 */
static inline void *meshlink_queue_pop(meshlink_queue_t *queue) {
	meshlink_queue_item_t *item;
	void *data;
	//MESHLINK_MUTEX_LOCK(&queue->mutex);
	if((item = queue->head)) {
		queue->head = item->next;
		if(!queue->head)
			queue->tail = NULL;
	}
	//MESHLINK_MUTEX_UNLOCK(&queue->mutex);
	data = item ? item->data : NULL;
	free(item);
	return data;
}

/**
 * Internal function to peek next meshlink_queue_item from queue.
 * Caller must NOT free() returned meshlink_queue_item->data!
 *
 * Use meshlink_queue_peek(queue) if you just want the data, not the queue_item.
 */
static inline void *meshlink_queue_peek(meshlink_queue_t *queue) {
    return queue->head ? queue->head->data : NULL;
}

/**
 * Internal function to check whether there's something in the queue.
 */
static inline bool meshlink_queue_empty(meshlink_queue_t *queue) {
    return queue->head == NULL;
}

/**
 * Deallocate all data in queue using given deleter function.
 */
static inline void exit_meshlink_queue(meshlink_queue_t *queue, meshlink_queue_action_t deleter) {
	void* data;
	do {
		data = meshlink_queue_pop(queue);
		if(data)
			deleter(data);
	} while (data);
}

#endif
