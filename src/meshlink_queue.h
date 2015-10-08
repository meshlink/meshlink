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

typedef struct meshlink_queue {
	struct meshlink_queue_item *head;	
	struct meshlink_queue_item *tail;	
	// pthread_mutex_t mutex;
} meshlink_queue_t;

typedef struct meshlink_queue_item {
	void *data;
	struct meshlink_queue_item *next;
} meshlink_queue_item_t;

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

static inline meshlink_queue_item_t* meshlink_queue_pop_item(meshlink_queue_t* queue)
{
	meshlink_queue_item_t *item;
	void *data;
	//MESHLINK_MUTEX_LOCK(&queue->mutex);
	if((item = queue->head)) {
		queue->head = item->next;
		if(!queue->head)
			queue->tail = NULL;
	}
	//MESHLINK_MUTEX_UNLOCK(&queue->mutex);
	return item;
}

static inline void *meshlink_queue_pop(meshlink_queue_t *queue) {
	meshlink_queue_item_t *item;
	void *data;

	item = meshlink_queue_pop_item(queue);
	data = item ? item->data : NULL;
	free(item);
	return data;
}

static inline void exit_meshlink_queue(meshlink_queue_t *queue, void(*deleter)(void*)) {
	void* data;
	do {
		data = meshlink_queue_pop(queue);
		if(data)
			deleter(data);
	} while (data);
}

#endif
