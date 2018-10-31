/*
    common_handlers.h -- Declarations of common callback handlers and signal handlers for
                            black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>

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

#ifndef COMMON_HANDLERS_H
#define COMMON_HANDLERS_H

#include "common_types.h"

#define PRINT_TEST_CASE_HEADER()        if(state_ptr) \
                                            fprintf(stderr, "[ %s ]\n", state_ptr->test_case_name)
#define PRINT_TEST_CASE_MSG(...)        if(state_ptr) \
                                            do { \
                                                fprintf(stderr, "[ %s ] ", \
                                                    state_ptr->test_case_name); \
                                                fprintf(stderr, __VA_ARGS__); \
                                            } while(0)

extern bool meta_conn_status[];
extern bool node_reachable_status[];
extern bool test_running;

char *get_ip(const char *if_name);
char *get_netmask(const char *if_name);
void stop_nw_intf(const char *if_name);
void start_nw_intf(const char *if_name);
void set_ip(const char *if_name, const char *new_ip);
void set_netmask(const char *if_name, const char *new_ip);
void mesh_close_signal_handler(int a);
void mesh_stop_start_signal_handler(int a);
void setup_signals(void);
void meshlink_callback_node_status(meshlink_handle_t *mesh, meshlink_node_t *node,
                                        bool reachable);
void meshlink_callback_logger(meshlink_handle_t *mesh, meshlink_log_level_t level,
                                      const char *text);

void change_ip(int node);
int create_bridge(const char *bridgeName);
void add_interface(const char *bridgeName, const char *interfaceName);
void add_veth_pair(const char *vethName1, const char *vethName2);
void bring_if_up(const char* bridgeName);
void replaceAll(char *str, const char *oldWord, const char *newWord);
void switch_bridge(const char *containerName, const char *currentBridge, const char *newBridge);
void bring_if_down(const char *bridgeName);
void del_interface(const char *bridgeName, const char *interfaceName);
int delete_bridge(const char *bridgeName);
int create_container_on_bridge(const char *containerName, const char *bridgeName, const char *ifName);
void config_dnsmasq(const char *containerName, const char *ifName, const char *listenAddress, const char *dhcpRange);
void config_nat(const char *containerName, const char *listenAddress);
int create_nat_layer(const char *containerName, const char *bridgeName, const char *ifName, const char *listenAddress, char* dhcpRange);
void destroy_nat_layer(const char *containerName, const char *bridgeName);
void incoming_firewall_ipv4(const char *packetType, int portNumber);
void incoming_firewall_ipv6(const char *packetType, int portNumber);
void outgoing_firewall_ipv4(const char *packetType, int portNumber);
void outgoing_firewall_ipv6(const char *packetType, int portNumber);

#endif // COMMON_HANDLERS_H
