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
#include "meshlink_internal.h"
#include "sptps.h"

// TODO: refactor logging code to use a meshlink_handle_t *.
void logger(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *format, ...) {
	if(mesh) {
		if(level < mesh->log_level || !mesh->log_cb)
			return;
	} else {
		if(level < global_log_level || !global_log_cb)
			return;
	}

	va_list ap;
	char message[1024] = "";

	va_start(ap, format);
	int len = vsnprintf(message, sizeof message, format, ap);
	va_end(ap);

	if(len > 0 && len < sizeof message && message[len - 1] == '\n')
		message[len - 1] = 0;

	if(mesh)
		mesh->log_cb(mesh, level, message);
	else
		global_log_cb(NULL, level, message);
}
