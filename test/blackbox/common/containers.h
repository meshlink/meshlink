/*
    containers.h -- Declarations for Container Management API
    Copyright (C) 2018  Guus Sliepen <guus@meshlink.io>
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

#define DAEMON_ARGV_LEN 2000
#define CONTAINER_SHUTDOWN_TIMEOUT 5

#define DHCP_RANGE "172.16.0.2,172.16.255.254,12h"
#define PUB_INTERFACE "eth0"
#define PRIV_INTERFACE "eth1"
#define LISTEN_ADDRESS "172.16.0.1"
#define NET_MASK "255.255.255.0"
#define SUBNET_MASK "172.16.0.0/24"

#define FULLCONE_NAT 1
#define ADDRESS_RESTRICTED_NAT 2
#define PORT_RESTRICTED_NAT 3
#define SYMMERTIC_NAT 4

extern char *lxc_path;

extern struct lxc_container *find_container(const char *name);
extern void rename_container(const char *old_name, const char *new_name);
extern char *run_in_container(const char *cmd, const char *node, bool daemonize);
extern void container_wait_ip(int node);
extern void create_containers(const char *node_names[], int num_nodes);
extern void setup_containers(void **state);
extern void destroy_containers(void);
extern void restart_all_containers(void);
extern char *invite_in_container(const char *inviter, const char *invitee);
extern void node_sim_in_container(const char *node, const char *device_class, const char *invite_url);
extern void node_sim_in_container_event(const char *node, const char *device_class,
                                        const char *invite_url, const char *clientId, const char *import);
extern void node_step_in_container(const char *node, const char *sig);
extern void change_ip(int node);

extern char *get_container_ip(const char *node_name);
extern void install_in_container(const char *node, const char *app);
extern void unblock_node_ip(const char *node);
extern void block_node_ip(const char *node);
extern void accept_port_rule(const char *node, const char *chain, const char *protocol, int port);
extern void nat_create(const char *nat_name, const char *nat_bridge, int nat_type);
extern void container_switch_bridge(const char *container_name, char *lxc_conf_path, const char *current_bridge, const char *new_bridge);
extern void bridge_add(const char *bridge_name);
extern void bridge_delete(const char *bridge_name);
extern void bridge_add_interface(const char *bridge_name, const char *interface_name);

extern void nat_destroy(const char *nat_name);
extern char *run_in_container_ex(const char *cmd, struct lxc_container *container, bool daemonize);
extern char *execute_in_container(const char *cmd, const char *container_name, bool daemonize);
extern char *block_icmp(const char *container_name);
extern char *unblock_icmp(const char *container_name);
extern char *change_container_mtu(const char *container_name, const char *interface_name, int mtu);
extern char *flush_conntrack(const char *container_name);

extern char **get_container_interface_ips(const char *container_name, const char *interface_name);
extern void flush_nat_rules(const char *container_name, const char *chain);
extern void add_full_cone_nat_rules(const char *container_name, const char *pub_interface, const char *priv_interface_listen_address);
extern void add_port_rest_nat_rules(const char *container_name, const char *pub_interface);
extern char *container_wait_ip_ex(const char *container_name);

#endif // CONTAINERS_H
