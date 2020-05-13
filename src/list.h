#ifndef MESHLINK_LIST_H
#define MESHLINK_LIST_H

/*
    list.h -- header file for list.c
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

typedef struct list_node_t {
	struct list_node_t *prev;
	struct list_node_t *next;

	/* Payload */

	void *data;
} list_node_t;

typedef void (*list_action_t)(const void *);
typedef void (*list_action_node_t)(const list_node_t *);

typedef struct list_t {
	list_node_t *head;
	list_node_t *tail;
	unsigned int count;

	/* Callbacks */

	list_action_t delete;
} list_t;

/* (De)constructors */

list_t *list_alloc(list_action_t) __attribute__((__malloc__));
void list_free(list_t *);

/* Insertion and deletion */

list_node_t *list_insert_head(list_t *, void *);
list_node_t *list_insert_tail(list_t *, void *);

void list_delete(list_t *, const void *);

void list_delete_node(list_t *, list_node_t *);

void list_delete_head(list_t *);
void list_delete_tail(list_t *);

/* Head/tail lookup */

void *list_get_head(list_t *);
void *list_get_tail(list_t *);

/* Fast list deletion */

void list_delete_list(list_t *);

/* Traversing */

void list_foreach(list_t *, list_action_t);
void list_foreach_node(list_t *, list_action_node_t);

#define list_each(type, item, list) (type *item = (type *)1; item; item = NULL) for(list_node_t *list_node = (list)->head, *list_next; item = list_node ? list_node->data : NULL, list_next = list_node ? list_node->next : NULL, list_node; list_node = list_next)

#endif
