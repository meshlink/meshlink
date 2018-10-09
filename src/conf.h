#ifndef MESHLINK_CONF_H
#define MESHLINK_CONF_H

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

#include "list.h"
#include "meshlink_internal.h"
#include "splay_tree.h"

typedef struct config_t {
	char *variable;
	char *value;
	int line;
} config_t;

extern void init_configuration(struct splay_tree_t **);
extern void exit_configuration(struct splay_tree_t **);
extern config_t *new_config(void) __attribute__((__malloc__));
extern void free_config(config_t *);
extern void config_add(struct splay_tree_t *, config_t *);
extern config_t *lookup_config(struct splay_tree_t *, char *);
extern config_t *lookup_config_next(struct splay_tree_t *, const config_t *);
extern bool get_config_bool(const config_t *, bool *);
extern bool get_config_int(const config_t *, int *);
extern bool set_config_int(config_t *, int);
extern bool get_config_string(const config_t *, char **);
extern bool set_config_string(config_t *, const char *);
extern bool get_config_address(const config_t *, struct addrinfo **);

extern bool read_server_config(struct meshlink_handle *mesh);
extern bool read_host_config(struct meshlink_handle *mesh, struct splay_tree_t *, const char *);
extern bool write_host_config(struct meshlink_handle *mesh, const struct splay_tree_t *, const char *);
extern bool modify_config_file(struct meshlink_handle *mesh, const char *name, const char *key, const char *value, int trim);
extern bool append_config_file(struct meshlink_handle *mesh, const char *name, const char *key, const char *value);

#endif
