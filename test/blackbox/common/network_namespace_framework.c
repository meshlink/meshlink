/*
    test_optimal_pmtu.c -- Execution of specific meshlink black box test cases
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
#include "network_namespace_framework.h"

#define DEFAULT_PUB_NET_ADDR "203.0.113.0/24"
#define DEFAULT_GATEWAY_NET_ADDR "203.0.113.254"
#define NS_PEER0  " ns_peer0 "
#define NS_ETH0   " ns_eth0 "
#define PEER_INDEX i ? 0 : 1
#define get_namespace_handle_by_index(state_ptr, index) index < state_ptr->num_namespaces ? &(state_ptr->namespaces[index]) : NULL
#define get_interface_handle_by_index(namespace, index) index < namespace->interfaces_no  ? &((namespace->interfaces)[index]) : NULL

static int ipv4_str_check_cidr(const char *ip_addr) {
	int cidr = 0;
	sscanf(ip_addr, "%*d.%*d.%*d.%*d/%d", &cidr);
	return cidr;
}

static char *ipv4_str_remove_cidr(const char *ipv4_addr) {
	char *ptr = strdup(ipv4_addr);
	assert(ptr);

	if(ipv4_str_check_cidr(ptr)) {
		char *end = strchr(ptr, '/');
		*end = '\0';
	}

	return ptr;
}

namespace_t *find_namespace(netns_state_t *state, const char *namespace_name) {
	int i;

	for(i = 0; i < state->num_namespaces; i++) {
		if(!strcmp((state->namespaces[i]).name, namespace_name)) {
			return &(state->namespaces[i]);
		}
	}

	return NULL;
}

static int netns_delete_namespace(namespace_t *namespace_handle) {
	char cmd[200];

	if(namespace_handle->type != BRIDGE) {
		assert(sprintf(cmd, "ip netns del %s 2>/dev/null", namespace_handle->name) >= 0);
	} else {
		assert(sprintf(cmd, "ip link del %s 2>/dev/null", namespace_handle->name) >= 0);
	}

	return system(cmd);
}

/* Create new network namespace using namespace handle */
static void netns_create_namespace(netns_state_t *test_state, namespace_t *namespace_handle) {
	char cmd[200];
	int cmd_ret;

	// Add the network namespace

	sprintf(cmd, "ip netns add %s", namespace_handle->name);
	assert(system(cmd) == 0);

	sprintf(cmd, "ip netns exec %s ip link set dev lo up", namespace_handle->name);
	assert(system(cmd) == 0);
}

static void netns_create_bridge(netns_state_t *test_state, namespace_t *namespace_handle) {
	char cmd[200];
	int cmd_ret;

	sprintf(cmd, "ip link add name %s type bridge", namespace_handle->name);
	assert(system(cmd) == 0);

	sprintf(cmd, "ip link set %s up", namespace_handle->name);
	assert(system(cmd) == 0);
}

interface_t *get_peer_interface_handle(netns_state_t *test_state, namespace_t *namespace, namespace_t *peer_namespace) {
	int i;
	interface_t *interfaces = namespace->interfaces;
	int if_no = namespace->interfaces_no;
	char *peer_name = peer_namespace->name;

	for(i = 0; i < if_no; i++) {
		if(!strcasecmp(interfaces[i].if_peer, peer_name)) {
			return &interfaces[i];
		}
	}

	return NULL;
}

interface_t *get_interface_handle_by_name(netns_state_t *test_state, namespace_t *namespace, const char *peer_name) {
	namespace_t *peer_ns;
	peer_ns = find_namespace(test_state, peer_name);
	assert(peer_ns);

	return get_peer_interface_handle(test_state, namespace, peer_ns);
}

bool check_interfaces_visited(netns_state_t *test_state, namespace_t *ns1, namespace_t *ns2) {
	interface_t *iface, *peer_iface;
	iface = get_peer_interface_handle(test_state, ns1, ns2);
	peer_iface = get_peer_interface_handle(test_state, ns2, ns1);
	assert(iface && peer_iface);

	return iface->priv || peer_iface->priv;
}

void netns_connect_namespaces(netns_state_t *test_state, namespace_t *ns1, namespace_t *ns2) {
	char buff[20], cmd[200], ns_eth0[20], ns_peer0[20];
	int cmd_ret, if_no, i;
	char eth_pairs[2][20];
	namespace_t *ns[2] = { ns1, ns2 };
	interface_t *interface;
	char *set = "set";

	// Check if visited already
	if(check_interfaces_visited(test_state, ns1, ns2)) {
		return;
	}

	assert(sprintf(eth_pairs[0], "%.9s_eth0", ns2->name) >= 0);
	assert(sprintf(eth_pairs[1], "%.9s_peer0", ns1->name) >= 0);

	// Delete veth pair if already exists
	for(i = 0; i < 2; i++) {
		assert(sprintf(cmd, "ip link del %s 2>/dev/null", eth_pairs[i]) >= 0);
		cmd_ret = system(cmd);
	}

	// Create veth pair
	assert(sprintf(cmd, "ip link add %s type veth peer name %s", eth_pairs[0], eth_pairs[1]) >= 0);
	assert(system(cmd) == 0);

	for(i = 0; i < 2; i++) {

		// Find interface handle that with it's peer interface
		interface =  get_peer_interface_handle(test_state, ns[i], ns[PEER_INDEX]);
		assert(interface);

		if(ns[i]->type != BRIDGE) {

			// Define interface name
			char *if_name;

			if(interface->if_name) {
				if_name = interface->if_name;
			} else {
				assert(sprintf(buff, "eth_%s", interface->if_peer) >= 0);
				if_name = buff;
			}

			interface->if_name = strdup(if_name);

			assert(interface->if_name);

			// Connect one end of the the veth pair to the namespace's interface
			assert(sprintf(cmd, "ip link set %s netns %s name %s", eth_pairs[i], ns[i]->name, interface->if_name) >= 0);

			assert(system(cmd) == 0);
		} else {

			// Connect one end of the the veth pair to the bridge
			assert(sprintf(cmd, "ip link set %s master %s up\n", eth_pairs[i], ns[i]->name) >= 0);
			assert(system(cmd) == 0);
		}

		// Mark interfaces as connected
		interface->priv = set;
		interface = get_peer_interface_handle(test_state, ns[PEER_INDEX], ns[i]);
		assert(interface);
		interface->priv = set;
	}
}

void netns_configure_ip_address(netns_state_t *test_state) {
	int i, if_no, cmd_ret;
	namespace_t *namespace;
	interface_t *if_handle;
	char cmd[200];

	for(i = 0; i < test_state->num_namespaces; i++) {
		namespace = get_namespace_handle_by_index(test_state, i);

		for(if_no = 0; if_no < namespace->interfaces_no; if_no++) {
			if_handle = get_interface_handle_by_index(namespace, if_no);
			assert(if_handle);

			if(if_handle->if_addr && namespace->type != BRIDGE) {
				assert(sprintf(cmd, "ip netns exec %s ip addr add %s dev %s", namespace->name, if_handle->if_addr, if_handle->if_name) >= 0);
				assert(system(cmd) == 0);
				assert(sprintf(cmd, "ip netns exec %s ip link set dev %s up", namespace->name, if_handle->if_name) >= 0);
				assert(system(cmd) == 0);

				if(if_handle->if_default_route_ip) {
					char *route_ip = ipv4_str_remove_cidr(if_handle->if_default_route_ip);
					assert(sprintf(cmd, "ip netns exec %s ip route add default via %s", namespace->name, route_ip) >= 0);
					assert(system(cmd) == 0);
					free(route_ip);
				}
			}
		}
	}
}

void netns_enable_all_nats(netns_state_t *test_state) {
	int i, j;
	namespace_t *namespace, *peer_namespace;
	interface_t *interface_handle;
	char cmd[200];
	char *ip_addr;

	for(i = 0; i < test_state->num_namespaces; i++) {
		namespace = get_namespace_handle_by_index(test_state, i);

		if(namespace->type == FULL_CONE) {
			assert(namespace->nat_arg);
			netns_fullcone_handle_t **nat_rules = namespace->nat_arg;
			char *eth0;

			for(j = 0; nat_rules[j]; j++) {
				assert(nat_rules[j]->snat_to_source && nat_rules[j]->dnat_to_destination);

				interface_handle = get_interface_handle_by_name(test_state, namespace, nat_rules[j]->snat_to_source);
				assert(interface_handle);
				eth0 = interface_handle->if_name;
				ip_addr = ipv4_str_remove_cidr(interface_handle->if_addr);
				assert(sprintf(cmd, "ip netns exec %s iptables -t nat -A POSTROUTING -o %s -j SNAT --to-source %s", namespace->name, eth0, ip_addr) >= 0);
				assert(system(cmd) == 0);
				free(ip_addr);

				peer_namespace = find_namespace(test_state, nat_rules[j]->dnat_to_destination);
				interface_handle = get_interface_handle_by_name(test_state, peer_namespace, namespace->name);
				assert(interface_handle);

				ip_addr = ipv4_str_remove_cidr(interface_handle->if_addr);
				assert(sprintf(cmd, "ip netns exec %s iptables -t nat -A PREROUTING  -i %s -j DNAT --to-destination %s", namespace->name, eth0, ip_addr) >= 0);
				assert(system(cmd) == 0);
				free(ip_addr);
			}
		}
	}
}

void netns_create_all_namespaces(netns_state_t *test_state) {
	int i, j;
	namespace_t *namespace, *peer_namespace;
	interface_t *interfaces;

	for(i = 0; i < test_state->num_namespaces; i++) {
		namespace = get_namespace_handle_by_index(test_state, i);

		// Delete the namespace if already exists
		netns_delete_namespace(namespace);

		// Create namespace

		if(namespace->type != BRIDGE) {
			netns_create_namespace(test_state, namespace);
		} else {
			netns_create_bridge(test_state, namespace);
		}
	}
}

void netns_connect_all_namespaces(netns_state_t *test_state) {
	int i, j;
	namespace_t *namespace, *peer_namespace;
	interface_t *interfaces;
	interface_t *interface_handle;

	for(i = 0; i < test_state->num_namespaces; i++) {
		namespace = get_namespace_handle_by_index(test_state, i);
		assert(namespace->interfaces);
		interfaces = namespace->interfaces;

		for(j = 0; j < namespace->interfaces_no; j++) {
			peer_namespace = find_namespace(test_state, interfaces[j].if_peer);
			assert(peer_namespace);
			netns_connect_namespaces(test_state, namespace, peer_namespace);
		}
	}

	// Reset all priv members of the interfaces

	for(i = 0; i < test_state->num_namespaces; i++) {
		namespace = get_namespace_handle_by_index(test_state, i);
		assert(namespace->interfaces);

		for(j = 0; j < namespace->interfaces_no; j++) {
			interface_handle = get_interface_handle_by_index(namespace, j);
			assert(interface_handle);
			interface_handle->priv = NULL;
		}
	}
}

void increment_ipv4_str(char *ip_addr, int ip_addr_size) {
	uint32_t addr_int_n, addr_int_h;

	assert(inet_pton(AF_INET, ip_addr, &addr_int_n) > 0);
	addr_int_h = ntohl(addr_int_n);
	addr_int_h = addr_int_h + 1;
	addr_int_n = htonl(addr_int_h);
	assert(inet_ntop(AF_INET, &addr_int_n, ip_addr, ip_addr_size));
}

void increment_ipv4_cidr_str(char *ip) {
	int subnet;
	assert(sscanf(ip, "%*d.%*d.%*d.%*d/%d", &subnet) >= 0);
	char *ptr = strchr(ip, '/');
	*ptr = '\0';
	increment_ipv4_str(ip, INET6_ADDRSTRLEN);
	sprintf(ip, "%s/%d", ip, subnet);
}

interface_t *netns_get_priv_addr(netns_state_t *test_state, const char *namespace_name) {
	namespace_t *namespace_handle;
	interface_t *interface_handle;
	int if_no;

	namespace_handle = find_namespace(test_state, namespace_name);
	assert(namespace_handle);

	for(if_no = 0; if_no < namespace_handle->interfaces_no; if_no++) {
		interface_handle = get_interface_handle_by_index(namespace_handle, if_no);

		if(!strcmp(namespace_handle->name, interface_handle->fetch_ip_netns_name)) {
			return interface_handle;
		}
	}

	return NULL;
}

void netns_add_default_route_addr(netns_state_t *test_state) {
	int ns, if_no;
	namespace_t *namespace_handle;
	interface_t *interface_handle, *peer_interface_handle;

	for(ns = 0; ns < test_state->num_namespaces; ns++) {
		namespace_handle = get_namespace_handle_by_index(test_state, ns);
		assert(namespace_handle);

		if(namespace_handle->type != HOST) {
			continue;
		}

		for(if_no = 0; if_no < namespace_handle->interfaces_no; if_no++) {
			interface_handle = get_interface_handle_by_index(namespace_handle, if_no);

			if(interface_handle->if_default_route_ip == NULL) {
				peer_interface_handle = netns_get_priv_addr(test_state, interface_handle->fetch_ip_netns_name);
				assert(peer_interface_handle);
				interface_handle->if_default_route_ip  = ipv4_str_remove_cidr(peer_interface_handle->if_addr);
			} else {
				char *dup = strdup(interface_handle->if_default_route_ip);
				assert(dup);
				interface_handle->if_default_route_ip = dup;
			}
		}
	}
}

void netns_assign_ip_addresses(netns_state_t *test_state) {
	int ns, j;
	namespace_t *namespace_handle;
	interface_t *interface_handle, *peer_interface_handle;
	int sub_net;


	char *addr = malloc(INET6_ADDRSTRLEN);
	assert(addr);

	if(test_state->public_net_addr) {
		assert(strncpy(addr, test_state->public_net_addr, INET6_ADDRSTRLEN));
	} else {
		assert(strncpy(addr, DEFAULT_PUB_NET_ADDR, INET6_ADDRSTRLEN));
	}

	test_state->public_net_addr = addr;

	for(ns = 0; ns < test_state->num_namespaces; ns++) {
		namespace_handle = get_namespace_handle_by_index(test_state, ns);
		assert(namespace_handle);

		if(namespace_handle->type == BRIDGE) {
			continue;
		}

		for(j = 0; j < namespace_handle->interfaces_no; j++) {
			interface_handle = get_interface_handle_by_index(namespace_handle, j);
			assert(interface_handle);

			if(interface_handle->if_addr) {
				continue;
			}

			// If fetch ip net namespace name is given get IP address from it, else get a public IP address

			if(interface_handle->fetch_ip_netns_name) {
				namespace_t *gw_netns_handle = find_namespace(test_state, interface_handle->fetch_ip_netns_name);
				assert(gw_netns_handle);
				assert(gw_netns_handle->static_config_net_addr);

				increment_ipv4_cidr_str(gw_netns_handle->static_config_net_addr);
				interface_handle->if_addr = strdup(gw_netns_handle->static_config_net_addr);
			} else {
				increment_ipv4_cidr_str(test_state->public_net_addr);
				interface_handle->if_addr = strdup(test_state->public_net_addr);

				if(namespace_handle->type == HOST) {
					if(interface_handle->if_default_route_ip) {
						char *dup = strdup(interface_handle->if_default_route_ip);
						assert(dup);
						interface_handle->if_default_route_ip = dup;
					} else {
						interface_handle->if_default_route_ip = strdup(DEFAULT_GATEWAY_NET_ADDR);
					}
				}
			}
		}
	}

	netns_add_default_route_addr(test_state);
}

static void netns_namespace_init_pids(netns_state_t *test_state) {
	int if_no;
	namespace_t *namespace_handle;

	for(if_no = 0; if_no < test_state->num_namespaces; if_no++) {
		namespace_handle = get_namespace_handle_by_index(test_state, if_no);
		assert(namespace_handle);
		namespace_handle->pid_nos = 0;
		namespace_handle->pids = NULL;
	}
}

pid_t run_cmd_in_netns(netns_state_t *test_state, char *namespace_name, char *cmd_str) {
	pid_t pid;
	namespace_t *namespace_handle;
	char cmd[1000];

	assert(namespace_name && cmd_str);
	namespace_handle = find_namespace(test_state, namespace_name);
	assert(namespace_handle);

	if((pid = fork()) == 0) {
		assert(daemon(1, 0) != -1);
		assert(sprintf(cmd, "ip netns exec %s %s", namespace_name, cmd_str) >= 0);
		assert(system(cmd) == 0);
		exit(0);
	}

	pid_t *pid_ptr;
	pid_ptr = realloc(namespace_handle->pids, (namespace_handle->pid_nos + 1) * sizeof(pid_t));
	assert(pid_ptr);
	namespace_handle->pids = pid_ptr;
	(namespace_handle->pids)[namespace_handle->pid_nos] = pid;
	namespace_handle->pid_nos = namespace_handle->pid_nos + 1;

	return pid;
}

static void *pthread_fun(void *arg) {
	netns_thread_t *netns_arg = (netns_thread_t *)arg;
	char namespace_path[100];
	void *ret;
	assert(sprintf(namespace_path, "/var/run/netns/%s", netns_arg->namespace_name) >= 0);
	int fd = open(namespace_path, O_RDONLY);
	assert(fd != -1);
	assert(setns(fd, CLONE_NEWNET) != -1);

	ret = (netns_arg->netns_thread)(netns_arg->arg);
	pthread_detach(netns_arg->thread_handle);
	pthread_exit(ret);
}

void run_node_in_namespace_thread(netns_thread_t *netns_arg) {
	assert(netns_arg->namespace_name && netns_arg->netns_thread);
	assert(!pthread_create(&(netns_arg->thread_handle), NULL, pthread_fun, netns_arg));
}

void netns_destroy_topology(netns_state_t *test_state) {
	namespace_t *namespace_handle;
	interface_t *interface_handle;
	int if_no, j, i;
	pid_t pid, pid_ret;

	for(if_no = 0; if_no < test_state->num_namespaces; if_no++) {
		namespace_handle = get_namespace_handle_by_index(test_state, if_no);
		assert(namespace_handle->interfaces);

		for(i = 0; i < namespace_handle->pid_nos; i++) {
			pid = (namespace_handle->pids)[i];
			assert(kill(pid, SIGINT) != -1);
			pid_ret = waitpid(pid, NULL, WNOHANG);
			assert(pid_ret != -1);

			if(pid_ret == 0) {
				fprintf(stderr, "pid: %d, is still running\n", pid);
			}
		}

		// Free interface name, interface address, interface default address etc.,
		// which are dynamically allocated and set the values to NULL

		for(j = 0; j < namespace_handle->interfaces_no; j++) {
			interface_handle = get_interface_handle_by_index(namespace_handle, j);
			assert(interface_handle);

			free(interface_handle->if_name);
			interface_handle->if_name = NULL;
			free(interface_handle->if_addr);
			interface_handle->if_addr = NULL;
			free(interface_handle->if_default_route_ip);
			interface_handle->if_default_route_ip = NULL;
		}

		// Delete namespace
		assert(netns_delete_namespace(namespace_handle) == 0);
	}

	free(test_state->public_net_addr);
	test_state->public_net_addr = NULL;
}

bool netns_create_topology(netns_state_t *test_state) {

	// (Re)create name-spaces and bridges
	netns_create_all_namespaces(test_state);

	// Connect namespaces and bridges(if any) with their interfaces
	netns_connect_all_namespaces(test_state);

	// Assign IP addresses for the interfaces in namespaces
	netns_assign_ip_addresses(test_state);

	// Configure assigned IP addresses with the interfaces in netns
	netns_configure_ip_address(test_state);

	// Enable all NATs
	netns_enable_all_nats(test_state);

	netns_namespace_init_pids(test_state);

	return true;
}
