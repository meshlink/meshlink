#ifndef MESHLINK_HASH_H
#define MESHLINK_HASH_H

/*
    hash.h -- header file for hash.c
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

typedef struct hash_t {
	size_t n;
	size_t size;
	char *keys;
	const void **values;
} hash_t;

hash_t *hash_alloc(size_t n, size_t size) __attribute__((__malloc__));
void hash_free(hash_t *);

void hash_insert(hash_t *, const void *key, const void *value);

void *hash_search(const hash_t *, const void *key);
void *hash_search_or_insert(hash_t *, const void *key, const void *value);

void hash_clear(hash_t *);
void hash_resize(hash_t *, size_t n);

#endif
