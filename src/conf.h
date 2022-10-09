#ifndef MESHLINK_CONF_H
#define MESHLINK_CONF_H

/*
    econf.h -- header for econf.c
    Copyright (C) 2018 Guus Sliepen <guus@meshlink.io>

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

struct meshlink_handle;
struct meshlink_open_params;

typedef struct config_t {
	const uint8_t *buf;
	size_t len;
} config_t;

typedef bool (*config_scan_action_t)(struct meshlink_handle *mesh, const char *name, size_t len);

void config_free(struct config_t *config);

bool config_init(struct meshlink_handle *mesh, const struct meshlink_open_params *params) __attribute__((__warn_unused_result__));
void config_exit(struct meshlink_handle *mesh);
bool config_destroy(const struct meshlink_open_params *params) __attribute__((__warn_unused_result__));

bool config_load(struct meshlink_handle *mesh, const char *name, struct config_t *) __attribute__((__warn_unused_result__));
bool config_store(struct meshlink_handle *mesh, const char *name, const struct config_t *) __attribute__((__warn_unused_result__));
bool config_ls(struct meshlink_handle *mesh, config_scan_action_t action) __attribute__((__warn_unused_result__));

bool config_exists(struct meshlink_handle *mesh, const char *name) __attribute__((__warn_unused_result__));

bool config_change_key(struct meshlink_handle *mesh, void *new_key) __attribute__((__warn_unused_result__));
bool config_cleanup_old_files(struct meshlink_handle *mesh);

bool invitation_read(struct meshlink_handle *mesh, const char *name, struct config_t *) __attribute__((__warn_unused_result__));
bool invitation_write(struct meshlink_handle *mesh, const char *name, const struct config_t *) __attribute__((__warn_unused_result__));
size_t invitation_purge_old(struct meshlink_handle *mesh, time_t deadline);
size_t invitation_purge_node(struct meshlink_handle *mesh, const char *name);

#endif
