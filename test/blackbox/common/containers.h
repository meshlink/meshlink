/*
    containers.h -- Declarations for Container Management API
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>
                        Manav Kumar Mehta <manavkumarm@yahoo.com>

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

#ifndef CONTAINERS_H
#define CONTAINERS_H

#include <lxc/lxccontainer.h>

#define DAEMON_ARGV_LEN 200
#define CONTAINER_SHUTDOWN_TIMEOUT 5

extern char *lxc_path;

struct lxc_container *find_container(char *name);
void rename_container(char *old_name, char *new_name);
char *run_in_container(char *cmd, char *node, bool daemonize);
void container_wait_ip(int node);
void create_containers(char *node_names[], int num_nodes);
void setup_containers(void **state);
void destroy_containers(void);
void restart_all_containers(void);
char *invite_in_container(char *inviter, char *invitee);
void node_sim_in_container(char *node, char *device_class, char *invite_url);
void node_step_in_container(char *node, char *sig);
void change_ip(int node);

#endif // CONTAINERS_H
