#ifndef MESHLINK_BUFFER_H
#define MESHLINK_BUFFER_H

/*
    conf.h -- header for conf.c
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

typedef struct buffer_t {
	char *data;
	size_t maxlen;
	size_t len;
	size_t offset;
} buffer_t;

extern void buffer_compact(buffer_t *buffer, size_t maxsize);
extern char *buffer_prepare(buffer_t *buffer, size_t size);
extern void buffer_add(buffer_t *buffer, const char *data, size_t size);
extern char *buffer_readline(buffer_t *buffer);
extern char *buffer_read(buffer_t *buffer, size_t size);
extern void buffer_clear(buffer_t *buffer);

#endif
