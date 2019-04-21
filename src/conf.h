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

extern bool config_read_file(struct meshlink_handle *mesh, FILE *f, struct config_t *, void *key);
extern bool config_write_file(struct meshlink_handle *mesh, FILE *f, const struct config_t *, void *key);
extern void config_free(struct config_t *config);

extern bool rename_confbase_subdir(struct meshlink_handle *mesh, const char *old_conf_subdir, const char *new_conf_subdir);
extern bool meshlink_confbase_exists(struct meshlink_handle *mesh);

extern bool config_init(struct meshlink_handle *mesh, const char *conf_subdir);
extern bool config_destroy(const char *confbase, const char *conf_subdir);
extern bool config_subdir_destroy(const char *confbase, const char *conf_subdir);

extern bool main_config_exists(struct meshlink_handle *mesh, const char *conf_subdir);
extern bool main_config_lock(struct meshlink_handle *mesh);
extern void main_config_unlock(struct meshlink_handle *mesh);
extern bool main_config_read(struct meshlink_handle *mesh, const char *conf_subdir, struct config_t *, void *key);
extern bool main_config_write(struct meshlink_handle *mesh, const char *conf_subdir, const struct config_t *, void *key);

extern bool config_exists(struct meshlink_handle *mesh, const char *conf_subdir, const char *name);
extern bool config_read(struct meshlink_handle *mesh, const char *conf_subdir, const char *name, struct config_t *, void *key);
extern bool config_write(struct meshlink_handle *mesh, const char *conf_subdir, const char *name, const struct config_t *, void *key);
extern bool config_scan_all(struct meshlink_handle *mesh, const char *conf_subdir, const char *conf_type, config_scan_action_t action, void *arg);

extern bool invitation_read(struct meshlink_handle *mesh, const char *conf_subdir, const char *name, struct config_t *, void *key);
extern bool invitation_write(struct meshlink_handle *mesh, const char *conf_subdir, const char *name, const struct config_t *, void *key);
extern size_t invitation_purge_old(struct meshlink_handle *mesh, time_t deadline);

#endif
