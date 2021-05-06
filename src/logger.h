#ifndef MESHLINK_LOGGER_H
#define MESHLINK_LOGGER_H

/*
    logger.h -- header file for logger.c
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

#include "meshlink_internal.h"

#ifdef MESHLINK_NO_LOG
#define logger(mesh, level, ...) do {(void)(mesh);} while(0)
#else
#define logger(mesh, level, ...) logger2(__FILE__, __LINE__, (mesh), (level), __VA_ARGS__)
void logger2(const char *file, int line, meshlink_handle_t *mesh, meshlink_log_level_t level, const char *format, ...) __attribute__((__format__(printf, 5, 6)));
#endif

#endif
