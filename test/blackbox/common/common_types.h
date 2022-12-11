/*
    common_types.h -- Declarations of common types used in Black Box Testing
    Copyright (C) 2018  Guus Sliepen <guus@meshlink.io>

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

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdbool.h>
#include "meshlink.h"

#define NUT_NODE_NAME "nut"

#define LXC_UTIL_REL_PATH "test/blackbox/util"
#define LXC_RENAME_SCRIPT "lxc_rename.sh"
#define LXC_RUN_SCRIPT "lxc_run.sh"
#define LXC_COPY_SCRIPT "lxc_copy_file.sh"
#define LXC_BUILD_SCRIPT "build_container.sh"
#define LXC_NAT_BUILD "nat.sh"
#define LXC_NAT_FULL_CONE "full_cone.sh"
#define LXC_NAT_DESTROY "nat_destroy.sh"

typedef struct black_box_state {
	char *test_case_name;
	char **node_names;
	int num_nodes;
	bool test_result;
} black_box_state_t;

extern char *lxc_bridge;

extern char *eth_if_name;

extern black_box_state_t *state_ptr;

extern char *meshlink_root_path;

/* Meshlink Mesh Handle */
extern meshlink_handle_t *mesh_handle;

/* Flag to indicate if Mesh is running */
extern bool mesh_started;

#endif // COMMON_TYPES_H
