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

typedef struct config_t {
	const uint8_t *buf;
	size_t len;
} config_t;

typedef bool (*config_scan_action_t)(struct meshlink_handle *mesh, const char *name, void *arg);

bool config_read_file(struct meshlink_handle *mesh, FILE *f, struct config_t *, const void *key) __attribute__((__warn_unused_result__));
bool config_write_file(struct meshlink_handle *mesh, FILE *f, const struct config_t *, const void *key) __attribute__((__warn_unused_result__));
void config_free(struct config_t *config);

bool meshlink_confbase_exists(struct meshlink_handle *mesh) __attribute__((__warn_unused_result__));

bool config_init(struct meshlink_handle *mesh, const char *conf_subdir) __attribute__((__warn_unused_result__));
bool config_destroy(const char *confbase, const char *conf_subdir) __attribute__((__warn_unused_result__));
bool config_copy(struct meshlink_handle *mesh, const char *src_dir_name, const void *src_key, const char *dst_dir_name, const void *dst_key) __attribute__((__warn_unused_result__));
bool config_rename(struct meshlink_handle *mesh, const char *old_conf_subdir, const char *new_conf_subdir) __attribute__((__warn_unused_result__));
bool config_sync(struct meshlink_handle *mesh, const char *conf_subdir) __attribute__((__warn_unused_result__));
bool sync_path(const char *path) __attribute__((__warn_unused_result__));

bool main_config_exists(struct meshlink_handle *mesh, const char *conf_subdir) __attribute__((__warn_unused_result__));
bool main_config_lock(struct meshlink_handle *mesh, const char *lock_filename) __attribute__((__warn_unused_result__));
void main_config_unlock(struct meshlink_handle *mesh);
bool main_config_read(struct meshlink_handle *mesh, const char *conf_subdir, struct config_t *, void *key) __attribute__((__warn_unused_result__));
bool main_config_write(struct meshlink_handle *mesh, const char *conf_subdir, const struct config_t *, void *key) __attribute__((__warn_unused_result__));

bool config_exists(struct meshlink_handle *mesh, const char *conf_subdir, const char *name) __attribute__((__warn_unused_result__));
bool config_read(struct meshlink_handle *mesh, const char *conf_subdir, const char *name, struct config_t *, void *key) __attribute__((__warn_unused_result__));
bool config_write(struct meshlink_handle *mesh, const char *conf_subdir, const char *name, const struct config_t *, void *key) __attribute__((__warn_unused_result__));
bool config_delete(struct meshlink_handle *mesh, const char *conf_subdir, const char *name) __attribute__((__warn_unused_result__));
bool config_scan_all(struct meshlink_handle *mesh, const char *conf_subdir, const char *conf_type, config_scan_action_t action, void *arg) __attribute__((__warn_unused_result__));

bool invitation_read(struct meshlink_handle *mesh, const char *conf_subdir, const char *name, struct config_t *, void *key) __attribute__((__warn_unused_result__));
bool invitation_write(struct meshlink_handle *mesh, const char *conf_subdir, const char *name, const struct config_t *, void *key) __attribute__((__warn_unused_result__));
size_t invitation_purge_old(struct meshlink_handle *mesh, time_t deadline);
size_t invitation_purge_node(struct meshlink_handle *mesh, const char *node_name);

#endif
