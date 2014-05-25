/*
   xalloc.h -- malloc and related fuctions with out of memory checking
   Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc., Foundation,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef __MESHLINK_XALLOC_H__
#define __MESHLINK_XALLOC_H__

static inline void *xmalloc(size_t n) __attribute__ ((__malloc__));
static inline void *xmalloc(size_t n) {
	void *p = malloc(n);
	if(!p)
		abort();
	return p;
}

static inline void *xzalloc(size_t n) __attribute__ ((__malloc__));
static inline void *xzalloc(size_t n) {
	void *p = calloc(1, n);
	if(!p)
		abort();
	return p;
}

static inline void *xrealloc(void *p, size_t n) {
	p = realloc(p, n);
	if(!p)
		abort();
	return p;
}

static inline char *xstrdup(const char *s) __attribute__ ((__malloc__));
static inline char *xstrdup(const char *s) {
	char *p = strdup(s);
	if(!p)
		abort();
	return p;
}

static inline int xvasprintf(char **strp, const char *fmt, va_list ap) {
#ifdef HAVE_MINGW
	char buf[1024];
	int result = vsnprintf(buf, sizeof buf, fmt, ap);
	if(result < 0)
		abort();
	*strp = xstrdup(buf);
#else
	int result = vasprintf(strp, fmt, ap);
	if(result < 0)
		abort();
#endif
	return result;
}

static inline int xasprintf(char **strp, const char *fmt, ...) __attribute__ ((__format__(printf, 2, 3)));
static inline int xasprintf(char **strp, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int result = xvasprintf(strp, fmt, ap);
	va_end(ap);
	return result;
}

#endif
