/*
    common_handlers.c -- Implementation of common callback handling and signal handling
                            functions for black box tests
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
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <ctype.h>
#include "test_step.h"
#include "common_handlers.h"

char *lxc_bridge = NULL;
black_box_state_t *state_ptr = NULL;

bool meta_conn_status[10];

bool test_running;

static int meshlink_get_node_in_container(const char *name) {
  int i;
  for(i = 0; i < state_ptr->num_nodes; i++) {
    if(!strcasecmp(state_ptr->node_names[i], name)) {
      return i;
      break;
    }
  }
  return -1;
}

void mesh_close_signal_handler(int a) {
	test_running = false;

	exit(EXIT_SUCCESS);
}

void setup_signals(void) {
	test_running = true;
	signal(SIGTERM, mesh_close_signal_handler);
}

/* Return the IP Address of the Interface 'if_name'
    The caller is responsible for freeing the dynamically allocated string that is returned */
char *get_ip(const char *if_name) {
	struct ifaddrs *ifaddr, *ifa;
	char *ip;
	int family;

	ip = malloc(NI_MAXHOST);
	assert(ip);
	assert(getifaddrs(&ifaddr) != -1);

	for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if(ifa->ifa_addr == NULL) {
			continue;
		}

		family = ifa->ifa_addr->sa_family;

		if(family == AF_INET && !strcmp(ifa->ifa_name , if_name)) {
			assert(!getnameinfo(ifa->ifa_addr, (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6), ip, NI_MAXHOST, NULL, 0, NI_NUMERICHOST));
			break;
		}
	}

	return ip;
}

/* Return the IP Address of the Interface 'if_name'
    The caller is responsible for freeing the dynamically allocated string that is returned */
char *get_netmask(const char *if_name) {
	struct ifaddrs *ifaddr, *ifa;
	char *ip;
	int family;

	ip = malloc(NI_MAXHOST);
	assert(ip);
	assert(getifaddrs(&ifaddr) != -1);

	for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if(ifa->ifa_addr == NULL) {
			continue;
		}

		family = ifa->ifa_addr->sa_family;

		if(family == AF_INET && !strcmp(ifa->ifa_name , if_name)) {
			assert(!getnameinfo(ifa->ifa_netmask, (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6), ip, NI_MAXHOST, NULL, 0, NI_NUMERICHOST));
			break;
		}
	}

	return ip;
}

/* Change the IP Address of an interface */
void set_ip(const char *if_name, const char *new_ip) {
	char set_ip_cmd[100];
	assert(snprintf(set_ip_cmd, sizeof(set_ip_cmd), "ifconfig %s %s", if_name, new_ip) >= 0);
	assert(system(set_ip_cmd) == 0);
}

/* Change the Netmask of an interface */
void set_netmask(const char *if_name, const char *new_netmask) {
	char set_mask_cmd[100];
	assert(snprintf(set_mask_cmd, sizeof(set_mask_cmd), "ifconfig %s netmask %s", if_name, new_netmask) >= 0);
	assert(system(set_mask_cmd) == 0);
}

/* Bring a network interface down (before making changes such as the IP Address) */
void stop_nw_intf(const char *if_name) {
	char nw_down_cmd[100];
	assert(snprintf(nw_down_cmd, sizeof(nw_down_cmd), "ifconfig %s down", if_name) >= 0);
	assert(system(nw_down_cmd) == 0);
}

/* Bring a network interface up (after bringing it down and making changes such as
    the IP Address) */
void start_nw_intf(const char *if_name) {
	char nw_up_cmd[100];
	assert(snprintf(nw_up_cmd, sizeof(nw_up_cmd), "ifconfig %s up", if_name) >= 0);
	assert(system(nw_up_cmd) == 0);
}

void meshlink_callback_node_status(meshlink_handle_t *mesh, meshlink_node_t *node,
                                   bool reachable) {
  (void)mesh;
	fprintf(stderr, "Node %s became %s\n", node->name, (reachable) ? "reachable" : "unreachable");
}

void meshlink_callback_logger(meshlink_handle_t *mesh, meshlink_log_level_t level,
                              const char *text) {
  (void)mesh;
  (void)level;

	fprintf(stderr, "meshlink>> %s\n", text);

	if(state_ptr) {
    bool status;
    char name[100];
    if(sscanf(text, "Connection with %s activated", name) == 1) {
      status = true;
    } else if (sscanf(text, "Already connected to %s", name) == 1) {
      status = true;
    } else if(sscanf(text, "Connection closed by %s", name) == 1) {
      status = false;
    } else if(sscanf(text, "Closing connection with %s", name) == 1) {
      status = false;
    } else {
      return;
    }
    int i = meshlink_get_node_in_container(name);
    assert(i != -1);
    meta_conn_status[i] = status;
	}
}
