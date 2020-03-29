/*
    dropin.c -- a set of drop-in replacements for libc functions
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

#include "xalloc.h"

#ifndef HAVE_ASPRINTF
int asprintf(char **buf, const char *fmt, ...) {
	int result;
	va_list ap;
	va_start(ap, fmt);
	result = vasprintf(buf, fmt, ap);
	va_end(ap);
	return result;
}

int vasprintf(char **buf, const char *fmt, va_list ap) {
	int status;
	va_list aq;
	int len;

	len = 4096;
	*buf = xmalloc(len);

	va_copy(aq, ap);
	status = vsnprintf(*buf, len, fmt, aq);
	va_end(aq);

	if(status >= 0) {
		*buf = xrealloc(*buf, status + 1);
	}

	if(status > len - 1) {
		len = status;
		va_copy(aq, ap);
		status = vsnprintf(*buf, len, fmt, aq);
		va_end(aq);
	}

	return status;
}
#endif
