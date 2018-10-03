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
void node_sim_in_container_event(char *node, char *device_class,
                           char *invite_url, char *clientId, char *import);
void node_step_in_container(char *node, char *sig);
void change_ip(int node);
char *get_container_ip(int node);

int create_bridge(char* bridgeName);
void add_interface(char* bridgeName, char* interfaceName);
void add_veth_pair(char* vethName1, char* vethName2);
void bring_if_up(char* bridgeName);
void replaceAll(char *str, const char *oldWord, const char *newWord);
void switch_bridge(char* containerName, char* currentBridge, char* newBridge);
void bring_if_down(char* bridgeName);
void del_interface(char* bridgeName, char* interfaceName);
int delete_bridge(char* bridgeName);
int create_container_on_bridge(char* containerName, char* bridgeName, char* ifName);
void config_dnsmasq(char* containerName, char* ifName, char* listenAddress, char* dhcpRange);
void config_nat(char* containerName, char* listenAddress);
int create_nat_layer(char* containerName, char* bridgeName, char* ifName, char* listenAddress, char* dhcpRange);
void destroy_nat_layer(char* containerName, char* bridgeName);
void incoming_firewall_ipv4(char* packetType, int portNumber);
void incoming_firewall_ipv6(char* packetType, int portNumber);
void outgoing_firewall_ipv4(char* packetType, int portNumber);
void outgoing_firewall_ipv6(char* packetType, int portNumber);

#endif // CONTAINERS_H
