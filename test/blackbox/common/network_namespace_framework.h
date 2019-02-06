/*
    network_namespace_framework.h -- Declarations for Individual Test Case implementation functions
    Copyright (C) 2019  Guus Sliepen <guus@meshlink.io>

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
#define _GNU_SOURCE 1
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <fcntl.h>
#include <time.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <spawn.h>

#define PUB_IF  0
#define PRIV_IF 1

typedef enum {
	HOST,
	FULL_CONE,
	PORT_REST,
	ADDR_REST,
	SYMMERTRIC,
	BRIDGE,
} namespace_type_t;

typedef void *pthread_fun_ptr_t(void *arg);

typedef struct {
	char *if_name;
	int   if_type;
	char *if_peer;
	char *if_addr;
	char *if_route;
	char *addr_host;
	char *fetch_ip_netns_name;
	char *if_default_route_ip;
	void *priv;
} interface_t;

typedef struct {
	char *snat_to_source;
	char *dnat_to_destination;
} netns_fullcone_handle_t;

typedef struct {
	char *name;
	namespace_type_t type;
	void *nat_arg;
	char static_config_net_addr[INET6_ADDRSTRLEN];   // Buffer should be of length INET_ADDRSTRLEN or INET6_ADDRSTRLEN
	interface_t *interfaces;
	int interfaces_no;
	pid_t *pids;
	int pid_nos;
	void *priv;
} namespace_t;

typedef struct {
	char *test_case_name;
	namespace_t *namespaces;
	int num_namespaces;
	char *public_net_addr;
	pthread_t **threads;
	bool test_result;
} netns_state_t;

typedef struct {
	char *namespace_name;
	pthread_fun_ptr_t *netns_thread;
	pthread_t thread_handle;
	void *arg;
} netns_thread_t;

typedef struct {
	char *node_name;
	char *confbase;
	char *app_name;
	int dev_class;
	char *join_invitation;
} mesh_arg_t;

typedef struct {
	mesh_arg_t *mesh_arg;
	char *invitee_name;
	char *invite_str;
} mesh_invite_arg_t;

extern bool netns_create_topology(netns_state_t *state);
extern void netns_destroy_topology(netns_state_t *test_state);
extern void run_node_in_namespace_thread(netns_thread_t *netns_arg);
extern pid_t run_cmd_in_netns(netns_state_t *test_state, char *namespace_name, char *cmd_str);
