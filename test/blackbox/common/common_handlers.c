/*
    common_handlers.c -- Implementation of common callback handling and signal handling
                            functions for black box tests
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
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include "test_step.h"
#include "common_handlers.h"

char *lxc_bridge = NULL;
black_box_state_t *state_ptr = NULL;

bool meta_conn_status[10];
bool node_reachable_status[10];

void mesh_close_signal_handler(int a) {
    execute_close();

    exit(EXIT_SUCCESS);
}

void mesh_stop_start_signal_handler(int a) {
    /* Stop the Mesh if it is running, otherwise start it again */
    (mesh_started) ? execute_stop() : execute_start();

    return;
}

void setup_signals(void) {
    signal(SIGTERM, mesh_close_signal_handler);
    signal(SIGINT, mesh_stop_start_signal_handler);

    return;
}

/* Return the IP Address of the Interface 'if_name'
    The caller is responsible for freeing the dynamically allocated string that is returned */
char *get_ip(const char *if_name) {
    int get_ip_fd;
    struct ifreq req_if;
    struct sockaddr_in *resp_if_addr;
    char *ip;

    /* Get IP Address of LXC Bridge Interface - this will be set up as the Gateway Address
        of the Static IP assigned to the Container */
    memset(&req_if, 0, sizeof(req_if));
    req_if.ifr_addr.sa_family = AF_INET;
    strncpy(req_if.ifr_name, if_name, IFNAMSIZ - 1);

    assert((get_ip_fd = socket(AF_INET, SOCK_DGRAM, 0)) != -1);
    ioctl(get_ip_fd, SIOCGIFADDR, &req_if);

    resp_if_addr = (struct sockaddr_in *) &(req_if.ifr_addr);
    assert(ip = malloc(20));
    strncpy(ip, inet_ntoa(resp_if_addr->sin_addr), 20);

    assert(close(get_ip_fd) != -1);

    return ip;
}

/* Return the IP Address of the Interface 'if_name'
    The caller is responsible for freeing the dynamically allocated string that is returned */
char *get_netmask(const char *if_name) {
    int get_nm_fd;
    struct ifreq req_if;
    struct sockaddr_in *resp_if_netmask;
    char *netmask;

    /* Get IP Address of LXC Bridge Interface - this will be set up as the Gateway Address
        of the Static IP assigned to the Container */
    memset(&req_if, 0, sizeof(req_if));
    req_if.ifr_addr.sa_family = AF_INET;
    strncpy(req_if.ifr_name, if_name, IFNAMSIZ - 1);

    assert((get_nm_fd = socket(AF_INET, SOCK_DGRAM, 0)) != -1);
    assert(ioctl(get_nm_fd, SIOCGIFNETMASK, &req_if) != -1);

    resp_if_netmask = (struct sockaddr_in *) &(req_if.ifr_addr);
    assert(netmask = malloc(20));
    strncpy(netmask, inet_ntoa(resp_if_netmask->sin_addr), 20);

    assert(close(get_nm_fd) != -1);

    return netmask;
}

/* Change the IP Address of an interface */
void set_ip(const char *if_name, const char *new_ip) {
    int set_ip_fd;
    struct ifreq req_if;
    struct sockaddr_in new_if_addr;

    /* Get IP Address of LXC Bridge Interface - this will be set up as the Gateway Address
        of the Static IP assigned to the Container */
    memset(&new_if_addr, 0, sizeof(new_if_addr));
    new_if_addr.sin_family = AF_INET;
    assert(inet_aton(new_ip, &new_if_addr.sin_addr) != 0);

    memset(&req_if, 0, sizeof(req_if));
    strncpy(req_if.ifr_name, if_name, IFNAMSIZ - 1);
    memcpy(&req_if.ifr_addr, &new_if_addr, sizeof(req_if.ifr_addr));

    assert((set_ip_fd = socket(AF_INET, SOCK_STREAM, 0)) != -1);
    ioctl(set_ip_fd, SIOCSIFADDR, &req_if);
    assert(close(set_ip_fd) != -1);
    /* TO DO: Get the original netmask and set it again, in case the IP change affects the
        netmask */

    return;
}

/* Change the Netmask of an interface */
void set_netmask(const char *if_name, const char *new_netmask) {
    int set_nm_fd;
    struct ifreq req_if;
    struct sockaddr_in new_if_netmask;

    /* Get IP Address of LXC Bridge Interface - this will be set up as the Gateway Address
        of the Static IP assigned to the Container */
    memset(&new_if_netmask, 0, sizeof(new_if_netmask));
    new_if_netmask.sin_family = AF_INET;
    assert(inet_aton(new_netmask, &new_if_netmask.sin_addr) != 0);

    memset(&req_if, 0, sizeof(req_if));
    strncpy(req_if.ifr_name, if_name, IFNAMSIZ - 1);
    memcpy(&req_if.ifr_netmask, &new_if_netmask, sizeof(req_if.ifr_netmask));

    assert((set_nm_fd = socket(AF_INET, SOCK_STREAM, 0)) != -1);
    ioctl(set_nm_fd, SIOCSIFNETMASK, &req_if);
    assert(close(set_nm_fd) != -1);
    /* TO DO: Get the original netmask and set it again, in case the IP change affects the
        netmask */

    return;
}

/* Bring a network interface down (before making changes such as the IP Address) */
void stop_nw_intf(const char *if_name) {
    int set_flags_fd;
    struct ifreq req_if_set_flags;

    /* Set the flags on the Interface to bring it down */
    memset(&req_if_set_flags, 0, sizeof(req_if_set_flags));
    strncpy(req_if_set_flags.ifr_name, if_name, IFNAMSIZ - 1);
    req_if_set_flags.ifr_flags &= (~IFF_UP);

    assert((set_flags_fd = socket(AF_INET, SOCK_STREAM, 0)) != -1);
    assert(ioctl(set_flags_fd, SIOCSIFFLAGS, &req_if_set_flags) != -1);
    assert(close(set_flags_fd) != -1);

    return;
}

/* Bring a network interface up (after bringing it down and making changes such as
    the IP Address) */
void start_nw_intf(const char *if_name) {
    int set_flags_fd;
    struct ifreq req_if_set_flags;

    /* Set the flags on the Interface to bring it up */
    memset(&req_if_set_flags, 0, sizeof(req_if_set_flags));
    strncpy(req_if_set_flags.ifr_name, if_name, IFNAMSIZ - 1);
    req_if_set_flags.ifr_flags |= IFF_UP;

    assert((set_flags_fd = socket(AF_INET, SOCK_STREAM, 0)) != -1);
    assert(ioctl(set_flags_fd, SIOCSIFFLAGS, &req_if_set_flags) != -1);
    assert(close(set_flags_fd) != -1);

    return;
}

void meshlink_callback_node_status(meshlink_handle_t *mesh, meshlink_node_t *node,
                                        bool reachable) {
    int i;

    fprintf(stderr, "Node %s became %s\n", node->name, (reachable) ? "reachable" : "unreachable");

    if(state_ptr)
        for(i = 0; i < state_ptr->num_nodes; i++)
            if(strcmp(node->name, state_ptr->node_names[i]) == 0)
                node_reachable_status[i] = reachable;

    return;
}

void meshlink_callback_logger(meshlink_handle_t *mesh, meshlink_log_level_t level,
                                      const char *text) {
    int i;
    char connection_match_msg[100];

    fprintf(stderr, "meshlink>> %s\n", text);
    if(state_ptr && (strstr(text, "Connection") || strstr(text, "connection"))) {
        for(i = 0; i < state_ptr->num_nodes; i++) {
            assert(snprintf(connection_match_msg, sizeof(connection_match_msg),
                "Connection with %s", state_ptr->node_names[i]) >= 0);

            if(strstr(text, connection_match_msg) && strstr(text, "activated")) {
                meta_conn_status[i] = true;
                continue;
            }

            assert(snprintf(connection_match_msg, sizeof(connection_match_msg),
                "Already connected to %s", state_ptr->node_names[i]) >= 0);
            if(strstr(text, connection_match_msg)) {
                meta_conn_status[i] = true;
                continue;
            }

            assert(snprintf(connection_match_msg, sizeof(connection_match_msg),
                "Connection closed by %s", state_ptr->node_names[i]) >= 0);
            if(strstr(text, connection_match_msg)) {
                meta_conn_status[i] = false;
                continue;
            }

            assert(snprintf(connection_match_msg, sizeof(connection_match_msg),
                "Closing connection with %s", state_ptr->node_names[i]) >= 0);
            if(strstr(text, connection_match_msg)) {
                meta_conn_status[i] = false;
                continue;
            }
        }
    }

    return;
}
