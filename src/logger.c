/*
    logger.c -- logging code
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

#include "logger.h"

debug_t debug_level = DEBUG_NOTHING;

void logger(int level, int priority, const char *format, ...) {
	if(level > debug_level)
		return;

	va_list ap;
	char message[1024] = "";

	va_start(ap, format);
	int len = vsnprintf(message, sizeof message, format, ap);
	va_end(ap);

	if(len > 0 && len < sizeof message && message[len - 1] == '\n')
		message[len - 1] = 0;

	fprintf(stderr, "%s\n", message);
}

// TODO: make sure this gets used somewhere
static void sptps_logger(struct sptps *s, int s_errno, const char *format, va_list ap) {
	char message[1024] = "";
	int len = vsnprintf(message, sizeof message, format, ap);
	if(len > 0 && len < sizeof message && message[len - 1] == '\n')
		message[len - 1] = 0;

	fprintf(stderr, "%s\n", message);
}
