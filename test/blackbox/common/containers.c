/*
    containers.h -- Container Management API
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

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "containers.h"
#include "common_handlers.h"

char *lxc_path = NULL;
char *choose_arch;
static char container_ips[10][100];

/* Return the handle to an existing container after finding it by container name */
struct lxc_container *find_container(const char *name) {
	struct lxc_container **test_containers;
	char **container_names;
	int num_containers, i;

	assert((num_containers = list_all_containers(lxc_path, &container_names,
	                         &test_containers)) != -1);

	for(i = 0; i < num_containers; i++) {
		if(strcmp(container_names[i], name) == 0) {
			return test_containers[i];
		}
	}

	return NULL;
}

/* Rename a Container */
void rename_container(const char *old_name, const char *new_name) {
	char rename_command[200];
	int rename_status;
	struct lxc_container *old_container;

	/* Stop the old container if its still running */
	assert(old_container = find_container(old_name));
	old_container->shutdown(old_container, CONTAINER_SHUTDOWN_TIMEOUT);
	/* Call stop() in case shutdown() fails - one of these two will always succeed */
	old_container->stop(old_container);
	/* Rename the Container */
	/* TO DO: Perform this operation using the LXC API - currently does not work via the API,
	    need to investigate and determine why it doesn't work, and make it work */
	assert(snprintf(rename_command, sizeof(rename_command),
	                "%s/" LXC_UTIL_REL_PATH "/" LXC_RENAME_SCRIPT " %s %s %s", meshlink_root_path, lxc_path,
	                old_name, new_name) >= 0);
	rename_status = system(rename_command);
	PRINT_TEST_CASE_MSG("Container '%s' rename status: %d\n", old_name, rename_status);
	assert(rename_status == 0);
}

/* Run 'cmd' inside the Container created for 'node' and return the first line of the output
    or NULL if there is no output - useful when, for example, a meshlink invite is generated
    by a node running inside a Container
    'cmd' is run as a daemon if 'daemonize' is true - this mode is useful for running node
    simulations in Containers
    The caller is responsible for freeing the returned string */
char *run_in_container(const char *cmd, const char *container_name, bool daemonize) {
	char container_find_name[100];
	struct lxc_container *container;
	char *output = NULL;
	size_t output_len = 0;

	assert(cmd);
	assert(container_name);
	assert(snprintf(container_find_name, sizeof(container_find_name), "%s_%s",
	                state_ptr->test_case_name, container_name) >= 0);
	assert(container = find_container(container_find_name));

	/* Run the command within the Container, either as a daemon or foreground process */
	/* TO DO: Perform this operation using the LXC API - currently does not work using the API
	    Need to determine why it doesn't work, and make it work */
	if(daemonize) {
		char run_script_path[100];
		char *attach_argv[4];

		assert(snprintf(run_script_path, sizeof(run_script_path), "%s/" LXC_UTIL_REL_PATH "/" LXC_RUN_SCRIPT,
		                meshlink_root_path) >= 0);
		attach_argv[0] = run_script_path;
		attach_argv[1] = (char *)cmd;
		attach_argv[2] = container->name;
		attach_argv[3] = NULL;

		/* To daemonize, create a child process and detach it from its parent (this program) */
		if(fork() == 0) {
			assert(daemon(1, 0) != -1);    // Detach from the parent process
			assert(execv(attach_argv[0], attach_argv) != -1);   // Run exec() in the child process
		}
	} else {
		char *attach_command;
		size_t attach_command_len;
		int i;
		attach_command_len = strlen(meshlink_root_path) + strlen(LXC_UTIL_REL_PATH) + strlen(LXC_RUN_SCRIPT) + strlen(cmd) + strlen(container->name) + 10;
		attach_command = malloc(attach_command_len);
		assert(attach_command);

		assert(snprintf(attach_command, attach_command_len,
		                "%s/" LXC_UTIL_REL_PATH "/" LXC_RUN_SCRIPT " \"%s\" %s", meshlink_root_path, cmd,
		                container->name) >= 0);
		FILE *attach_fp;
		assert(attach_fp = popen(attach_command, "r"));
		free(attach_command);
		/* If the command has an output, strip out any trailing carriage returns or newlines and
		    return it, otherwise return NULL */
		output = NULL;
		output_len = 0;

		if(getline(&output, &output_len, attach_fp) != -1) {
			i = strlen(output) - 1;

			while(output[i] == '\n' || output[i] == '\r') {
				i--;
			}

			output[i + 1] = '\0';
		} else {
			free(output);
			output = NULL;
		}

		assert(pclose(attach_fp) != -1);
	}

	return output;
}

/* Wait for a starting Container to obtain an IP Address, then save that IP for future use */
void container_wait_ip(int node) {
	char container_name[100], lxcls_command[200];
	struct lxc_container *test_container;
	char *ip;
	size_t ip_len;
	int i;
	bool ip_found;
	FILE *lxcls_fp;

	assert(snprintf(container_name, sizeof(container_name), "%s_%s", state_ptr->test_case_name,
	                state_ptr->node_names[node]) >= 0);
	assert(test_container = find_container(container_name));
	assert(snprintf(lxcls_command, sizeof(lxcls_command),
	                "lxc-ls -f | grep %s | tr -s ' ' | cut -d ' ' -f 5", test_container->name) >= 0);
	PRINT_TEST_CASE_MSG("Waiting for Container '%s' to acquire IP\n", test_container->name);
	assert(ip = malloc(20));
	ip_len = sizeof(ip);
	ip_found = false;

	while(!ip_found) {
		assert(lxcls_fp = popen(lxcls_command, "r"));   // Run command
		assert(getline((char **)&ip, &ip_len, lxcls_fp) != -1); // Read its output
		/* Strip newlines and carriage returns from output */
		i = strlen(ip) - 1;

		while(ip[i] == '\n' || ip[i] == '\r') {
			i--;
		}

		ip[i + 1] = '\0';
		ip_found = (strcmp(ip, "-") != 0);  // If the output is not "-", IP has been acquired
		assert(pclose(lxcls_fp) != -1);
		sleep(1);
	}

	strncpy(container_ips[node], ip, sizeof(container_ips[node])); // Save the IP for future use
	PRINT_TEST_CASE_MSG("Node '%s' has IP Address %s\n", state_ptr->node_names[node],
	                    container_ips[node]);

	free(ip);
}

/* Create all required test containers */
void create_containers(const char *node_names[], int num_nodes) {
	int i;
	char container_name[100];
	int create_status, snapshot_status, snap_restore_status;
	struct lxc_container *first_container = NULL;

	for(i = 0; i < num_nodes; i++) {
		assert(snprintf(container_name, sizeof(container_name), "run_%s", node_names[i]) >= 0);

		/* If this is the first Container, create it otherwise restore the snapshot saved
		    for the first Container to create an additional Container */
		if(i == 0) {
			assert(first_container = lxc_container_new(container_name, NULL));
			assert(!first_container->is_defined(first_container));
			create_status = first_container->createl(first_container, "download", NULL, NULL,
			                LXC_CREATE_QUIET, "-d", "ubuntu", "-r", "trusty", "-a", choose_arch, NULL);
			fprintf(stderr, "Container '%s' create status: %d - %s\n", container_name,
			        first_container->error_num, first_container->error_string);
			assert(create_status);
			snapshot_status = first_container->snapshot(first_container, NULL);
			fprintf(stderr, "Container '%s' snapshot status: %d - %s\n", container_name,
			        first_container->error_num, first_container->error_string);
			assert(snapshot_status != -1);
		} else {
			assert(first_container);
			snap_restore_status = first_container->snapshot_restore(first_container, "snap0",
			                      container_name);
			fprintf(stderr, "Snapshot restore to Container '%s' status: %d - %s\n", container_name,
			        first_container->error_num, first_container->error_string);
			assert(snap_restore_status);
		}
	}
}

/* Setup Containers required for a test
    This function should always be invoked in a CMocka context
    after setting the state of the test case to an instance of black_box_state_t */
void setup_containers(void **state) {
	black_box_state_t *test_state = (black_box_state_t *)(*state);
	int i, confbase_del_status;
	char build_command[200];
	struct lxc_container *test_container, *new_container;
	char container_find_name[100];
	char container_new_name[100];
	int create_status, build_status;

	PRINT_TEST_CASE_HEADER();

	for(i = 0; i < test_state->num_nodes; i++) {
		/* Find the run_<node-name> Container or create it if it doesn't exist */
		assert(snprintf(container_find_name, sizeof(container_find_name), "run_%s",
		                test_state->node_names[i]) >= 0);

		if(!(test_container = find_container(container_find_name))) {
			assert(test_container = lxc_container_new(container_find_name, NULL));
			assert(!test_container->is_defined(test_container));
			create_status = test_container->createl(test_container, "download", NULL, NULL,
			                                        LXC_CREATE_QUIET, "-d", "ubuntu", "-r", "trusty", "-a", choose_arch, NULL);
			PRINT_TEST_CASE_MSG("Container '%s' create status: %d - %s\n", container_find_name,
			                    test_container->error_num, test_container->error_string);
			assert(create_status);
		}

		/* Stop the Container if it's running */
		test_container->shutdown(test_container, CONTAINER_SHUTDOWN_TIMEOUT);
		/* Call stop() in case shutdown() fails
		    One of these two calls will always succeed */
		test_container->stop(test_container);
		/* Rename the Container to make it specific to this test case,
		    if a Container with the target name already exists, skip this step */
		assert(snprintf(container_new_name, sizeof(container_new_name), "%s_%s",
		                test_state->test_case_name, test_state->node_names[i]) >= 0);

		if(!(new_container = find_container(container_new_name))) {
			rename_container(test_container->name, container_new_name);
			assert(new_container = find_container(container_new_name));
		}

		/* Start the Container */
		assert(new_container->start(new_container, 0, NULL));
		/* Build the Container by copying required files into it */
		assert(snprintf(build_command, sizeof(build_command),
		                "%s/" LXC_UTIL_REL_PATH "/" LXC_BUILD_SCRIPT " %s %s %s +x >/dev/null",
		                meshlink_root_path, test_state->test_case_name, test_state->node_names[i],
		                meshlink_root_path) >= 0);
		build_status = system(build_command);
		PRINT_TEST_CASE_MSG("Container '%s' build Status: %d\n", new_container->name,
		                    build_status);
		assert(build_status == 0);
		/* Restart the Container after building it and wait for it to acquire an IP */
		new_container->shutdown(new_container, CONTAINER_SHUTDOWN_TIMEOUT);
		new_container->stop(new_container);
		new_container->start(new_container, 0, NULL);
		container_wait_ip(i);
	}
}

/* Destroy all Containers with names containing 'run_' - Containers saved for debugging will
    have names beginning with test_case_ ; 'run_' is reserved for temporary Containers
    intended to be re-used for the next test */
void destroy_containers(void) {
	struct lxc_container **test_containers;
	char **container_names;
	int num_containers, i;

	assert((num_containers = list_all_containers(lxc_path, &container_names,
	                         &test_containers)) != -1);

	for(i = 0; i < num_containers; i++) {
		if(strstr(container_names[i], "run_")) {
			fprintf(stderr, "Destroying Container '%s'\n", container_names[i]);
			/* Stop the Container - it cannot be destroyed till it is stopped */
			test_containers[i]->shutdown(test_containers[i], CONTAINER_SHUTDOWN_TIMEOUT);
			/* Call stop() in case shutdown() fails
			    One of these two calls will always succeed */
			test_containers[i]->stop(test_containers[i]);
			/* Destroy the Container */
			test_containers[i]->destroy(test_containers[i]);
			/* Call destroy_with_snapshots() in case destroy() fails
			    one of these two calls will always succeed */
			test_containers[i]->destroy_with_snapshots(test_containers[i]);
		}
	}
}

/* Restart all the Containers being used in the current test case i.e. Containers with
    names beginning with <test-case-name>_<node-name> */
void restart_all_containers(void) {
	char container_name[100];
	struct lxc_container *test_container;
	int i;

	for(i = 0; i < state_ptr->num_nodes; i++) {
		/* Shutdown, then start the Container, then wait for it to acquire an IP Address */
		assert(snprintf(container_name, sizeof(container_name), "%s_%s", state_ptr->test_case_name,
		                state_ptr->node_names[i]) >= 0);
		assert(test_container = find_container(container_name));
		test_container->shutdown(test_container, CONTAINER_SHUTDOWN_TIMEOUT);
		test_container->stop(test_container);
		test_container->start(test_container, 0, NULL);
		container_wait_ip(i);
	}
}

/* Run the gen_invite command inside the 'inviter' container to generate an invite
    for 'invitee', and return the generated invite which is output on the terminal */
char *invite_in_container(const char *inviter, const char *invitee) {
	char invite_command[200];
	char *invite_url;

	assert(snprintf(invite_command, sizeof(invite_command),
	                "LD_LIBRARY_PATH=/home/ubuntu/test/.libs /home/ubuntu/test/gen_invite %s %s "
	                "2> gen_invite.log", inviter, invitee) >= 0);
	assert(invite_url = run_in_container(invite_command, inviter, false));
	PRINT_TEST_CASE_MSG("Invite Generated from '%s' to '%s': %s\n", inviter,
	                    invitee, invite_url);

	return invite_url;
}

/* Run the node_sim_<nodename> program inside the 'node''s container */
void node_sim_in_container(const char *node, const char *device_class, const char *invite_url) {
	char *node_sim_command;
	size_t node_sim_command_len;

	node_sim_command_len = 500 + (invite_url ? strlen(invite_url) : 0);
	node_sim_command = calloc(1, node_sim_command_len);
	assert(node_sim_command);
	assert(snprintf(node_sim_command, node_sim_command_len,
	                "LD_LIBRARY_PATH=/home/ubuntu/test/.libs /home/ubuntu/test/node_sim_%s %s %s %s "
	                "1>&2 2>> node_sim_%s.log", node, node, device_class,
	                (invite_url) ? invite_url : "", node) >= 0);
	run_in_container(node_sim_command, node, true);
	PRINT_TEST_CASE_MSG("node_sim_%s started in Container\n", node);

	free(node_sim_command);
}

/* Run the node_sim_<nodename> program inside the 'node''s container with event handling capable */
void node_sim_in_container_event(const char *node, const char *device_class,
                                 const char *invite_url, const char *clientId, const char *import) {
	char *node_sim_command;
	size_t node_sim_command_len;

	PRINT_TEST_CASE_MSG("Before launch\n");
	node_sim_command_len = 500 + (invite_url ? strlen(invite_url) : 0);
	node_sim_command = calloc(1, node_sim_command_len);
	assert(node_sim_command);
	PRINT_TEST_CASE_MSG("Before launch\n");
	assert(snprintf(node_sim_command, node_sim_command_len,
	                "LD_LIBRARY_PATH=/home/ubuntu/test/.libs /home/ubuntu/test/node_sim_%s %s %s %s %s %s "
	                "1>&2 2>> node_sim_%s.log", node, node, device_class,
	                clientId, import, (invite_url) ? invite_url : "", node) >= 0);
	run_in_container(node_sim_command, node, true);
	PRINT_TEST_CASE_MSG("node_sim_%s(Client Id :%s) started in Container with event handling\n",
	                    node, clientId);

	free(node_sim_command);
}

/* Run the node_step.sh script inside the 'node''s container to send the 'sig' signal to the
    node_sim program in the container */
void node_step_in_container(const char *node, const char *sig) {
	char node_step_command[200];

	assert(snprintf(node_step_command, sizeof(node_step_command),
	                "/home/ubuntu/test/node_step.sh lt-node_sim_%s %s 1>&2 2> node_step.log",
	                node, sig) >= 0);
	run_in_container(node_step_command, node, false);
	PRINT_TEST_CASE_MSG("Signal %s sent to node_sim_%s\n", sig, node);
}

/* Change the IP Address of the Container running 'node'
    Changes begin from X.X.X.254 and continue iteratively till an available address is found */
void change_ip(int node) {
	char *gateway_addr;
	char new_ip[20];
	char *netmask;
	char *last_dot_in_ip;
	int last_ip_byte = 254;
	FILE *if_fp;
	char copy_command[200];
	char container_name[100];
	struct lxc_container *container;
	int copy_file_stat;

	/* Get IP Address of LXC Bridge Interface - this will be set up as the Gateway Address
	    of the Static IP assigned to the Container */
	assert(gateway_addr = get_ip(lxc_bridge));
	/* Get Netmask of LXC Brdige Interface */
	assert(netmask = get_netmask(lxc_bridge));

	/* Replace last byte of Container's IP with 254 to form the new Container IP */
	assert(container_ips[node]);
	strncpy(new_ip, container_ips[node], sizeof(new_ip));
	assert(last_dot_in_ip = strrchr(new_ip, '.'));
	assert(snprintf(last_dot_in_ip + 1, 4, "%d", last_ip_byte) >= 0);

	/* Check that the new IP does not match the Container's existing IP
	    if it does, iterate till it doesn't */
	/* TO DO: Make sure the IP does not conflict with any other running Container */
	while(strcmp(new_ip, container_ips[node]) == 0) {
		last_ip_byte--;
		assert(snprintf(last_dot_in_ip + 1, 4, "%d", last_ip_byte) >= 0);
	}

	/* Create new 'interfaces' file for Container */
	assert(if_fp = fopen("interfaces", "w"));
	fprintf(if_fp, "auto lo\n");
	fprintf(if_fp, "iface lo inet loopback\n");
	fprintf(if_fp, "\n");
	fprintf(if_fp, "auto eth0\n");
	fprintf(if_fp, "iface eth0 inet static\n");
	fprintf(if_fp, "\taddress %s\n", new_ip);
	fprintf(if_fp, "\tnetmask %s\n", netmask);
	fprintf(if_fp, "\tgateway %s\n", gateway_addr);
	assert(fclose(if_fp) != EOF);

	/* Copy 'interfaces' file into Container's /etc/network path */
	assert(snprintf(copy_command, sizeof(copy_command),
	                "%s/" LXC_UTIL_REL_PATH "/" LXC_COPY_SCRIPT " interfaces %s_%s /etc/network/interfaces",
	                meshlink_root_path, state_ptr->test_case_name, state_ptr->node_names[node]) >= 0);
	copy_file_stat = system(copy_command);
	PRINT_TEST_CASE_MSG("Container '%s_%s' 'interfaces' file copy status: %d\n",
	                    state_ptr->test_case_name, state_ptr->node_names[node], copy_file_stat);
	assert(copy_file_stat == 0);

	/* Restart Container to apply new IP Address */
	assert(snprintf(container_name, sizeof(container_name), "%s_%s", state_ptr->test_case_name,
	                state_ptr->node_names[node]) >= 0);
	assert(container = find_container(container_name));
	container->shutdown(container, CONTAINER_SHUTDOWN_TIMEOUT);
	/* Call stop() in case shutdown() fails
	    One of these two calls with always succeed */
	container->stop(container);
	assert(container->start(container, 0, NULL));

	strncpy(container_ips[node], new_ip, sizeof(new_ip));   // Save the new IP Addres
	PRINT_TEST_CASE_MSG("Node '%s' IP Address changed to %s\n", state_ptr->node_names[node],
	                    container_ips[node]);
}

/* Return container's IP address */
char *get_container_ip(const char *node_name) {
	char *ip;
	int n, node = -1, i;

	for(i = 0; i < state_ptr->num_nodes; i++) {
		if(!strcasecmp(state_ptr->node_names[i], node_name)) {
			node = i;
			break;
		}
	}

	if(i == state_ptr->num_nodes) {
		return NULL;
	}

	n = strlen(container_ips[node]) + 1;
	ip = malloc(n);
	assert(ip);
	strncpy(ip, container_ips[node], n);

	return ip;
}

/* Install an app in a container */
void install_in_container(const char *node, const char *app) {
	char install_cmd[100];

	assert(snprintf(install_cmd, sizeof(install_cmd),
	                "apt-get install %s -y >> /dev/null", app) >= 0);
	char *ret = run_in_container(install_cmd, node, false);
	// TODO: Check in container whether app has installed or not with a timeout
	sleep(10);
}

/* Simulate a network failure by adding NAT rule in the container with it's IP address */
void block_node_ip(const char *node) {
	char block_cmd[100];
	char *node_ip;

	node_ip = get_container_ip(node);
	assert(node_ip);
	assert(snprintf(block_cmd, sizeof(block_cmd), "iptables -A OUTPUT -p all -s %s -j DROP", node_ip) >= 0);
	run_in_container(block_cmd, node, false);

	assert(snprintf(block_cmd, sizeof(block_cmd), "iptables -A INPUT -p all -s %s -j DROP", node_ip) >= 0);
	run_in_container(block_cmd, node, false);

	assert(snprintf(block_cmd, sizeof(block_cmd), "iptables -A FORWARD -p all -s %s -j DROP", node_ip) >= 0);
	run_in_container(block_cmd, node, false);

	free(node_ip);
}

void accept_port_rule(const char *node, const char *chain, const char *protocol, int port) {
	char block_cmd[100];

	assert(port >= 0 && port < 65536);
	assert(!strcmp(chain, "INPUT") || !strcmp(chain, "FORWARD") || !strcmp(chain, "OUTPUT"));
	assert(!strcmp(protocol, "all") || !strcmp(protocol, "tcp") || !strcmp(protocol, "udp"));
	assert(snprintf(block_cmd, sizeof(block_cmd), "iptables -A %s -p %s --dport %d -j ACCEPT", chain, protocol, port) >= 0);
	run_in_container(block_cmd, node, false);
}

/* Restore the network that was blocked before by the NAT rule with it's own IP address */
void unblock_node_ip(const char *node) {
	char unblock_cmd[100];
	char *node_ip;

	node_ip = get_container_ip(node);
	assert(node_ip);
	assert(snprintf(unblock_cmd, sizeof(unblock_cmd), "iptables -D OUTPUT -p all -s %s -j DROP", node_ip) >= 0);
	run_in_container(unblock_cmd, node, false);

	assert(snprintf(unblock_cmd, sizeof(unblock_cmd), "iptables -D INPUT -p all -s %s -j DROP", node_ip) >= 0);
	run_in_container(unblock_cmd, node, false);

	assert(snprintf(unblock_cmd, sizeof(unblock_cmd), "iptables -D FORWARD -p all -s %s -j DROP", node_ip) >= 0);
	run_in_container(unblock_cmd, node, false);
}

/* Takes bridgeName as input parameter and creates a bridge */
void create_bridge(const char *bridgeName) {
	char command[100] = "brctl addbr ";
	strcat(command, bridgeName);
	int create_bridge_status = system(command);
	assert(create_bridge_status == 0);
	PRINT_TEST_CASE_MSG("%s bridge created\n", bridgeName);
}

/* Add interface for the bridge created */
void add_interface(const char *bridgeName, const char *interfaceName) {
	char command[100] = "brctl addif ";
	char cmd[100] = "dhclient ";

	strcat(command, bridgeName);
	strcat(command, " ");
	strcat(command, interfaceName);
	int addif_status = system(command);
	assert(addif_status == 0);
	strcat(cmd, bridgeName);
	int dhclient_status = system(cmd);
	assert(dhclient_status == 0);
	PRINT_TEST_CASE_MSG("Added interface for %s\n", bridgeName);
}

/* Create a veth pair and bring them up */
void add_veth_pair(const char *vethName1, const char *vethName2) {
	char command[100] = "ip link add ";
	char upCommand1[100] = "ip link set ";
	char upCommand2[100] = "ip link set ";

	strcat(command, vethName1);
	strcat(command, " type veth peer name ");
	strcat(command, vethName2);
	int link_add_status = system(command);
	assert(link_add_status == 0);
	strcat(upCommand1, vethName1);
	strcat(upCommand1, " up");
	int link_set_veth1_status = system(upCommand1);
	assert(link_set_veth1_status == 0);
	strcat(upCommand2, vethName2);
	strcat(upCommand2, " up");
	int link_set_veth2_status = system(upCommand2);
	assert(link_set_veth2_status == 0);
	PRINT_TEST_CASE_MSG("Added veth pairs %s and %s\n", vethName1, vethName2);
}

/* Bring the interface up for the bridge created */
void bring_if_up(const char *bridgeName) {
	char command[300] = "ifconfig ";
	char dhcommand[300] = "dhclient ";
	strcat(command, bridgeName);
	strcat(command, " up");
	int if_up_status = system(command);
	assert(if_up_status == 0);
	sleep(2);
	PRINT_TEST_CASE_MSG("Interface brought up for %s created\n", bridgeName);
}

/**
 * Replace all occurrences of a given a word in string.
 */
void replaceAll(char *str, const char *oldWord, const char *newWord) {
	char *pos, temp[BUFSIZ];
	int index = 0;
	int owlen;
	owlen = strlen(oldWord);

	while((pos = strstr(str, oldWord)) != NULL) {
		strcpy(temp, str);
		index = pos - str;
		str[index] = '\0';
		strcat(str, newWord);
		strcat(str, temp + index + owlen);
	}
}

/* Switches the bridge for a given container */
void switch_bridge(const char *containerName, const char *currentBridge, const char *newBridge) {
	char command[100] = "lxc-stop -n ";
	char command_start[100] = "lxc-start -n ";
	PRINT_TEST_CASE_MSG("Switching %s container to %s\n", containerName, newBridge);
	strcat(command, containerName);
	strcat(command_start, containerName);
	int container_stop_status = system(command);
	assert(container_stop_status == 0);
	sleep(2);
	FILE *fPtr;
	FILE *fTemp;
	char path[300] = "/var/lib/lxc/";
	strcat(path, containerName);
	strcat(path, "/config");

	char buffer[BUFSIZ];
	/*  Open all required files */
	fPtr  = fopen(path, "r");
	fTemp = fopen("replace.tmp", "w");

	if(fPtr == NULL || fTemp == NULL) {
		PRINT_TEST_CASE_MSG("\nUnable to open file.\n");
		PRINT_TEST_CASE_MSG("Please check whether file exists and you have read/write privilege.\n");
		exit(EXIT_SUCCESS);
	}

	while((fgets(buffer, BUFSIZ, fPtr)) != NULL) {
		replaceAll(buffer, currentBridge, newBridge);
		fputs(buffer, fTemp);
	}

	fclose(fPtr);
	fclose(fTemp);
	remove(path);
	rename("replace.tmp", path);
	PRINT_TEST_CASE_MSG("Switching procedure done successfully\n");
	int container_start_status = system(command_start);
	assert(container_start_status == 0);
	sleep(2);
}

/* Bring the interface down for the bridge created */
void bring_if_down(const char *bridgeName) {
	char command[300] = "ip link set dev ";
	strcat(command, bridgeName);
	strcat(command, " down");
	int if_down_status = system(command);
	assert(if_down_status == 0);
	PRINT_TEST_CASE_MSG("Interface brought down for %s created\n", bridgeName);
}

/* Delete interface for the bridge created */
void del_interface(const char *bridgeName, const char *interfaceName) {
	char command[300] = "brctl delif ";
	strcat(command, bridgeName);
	strcat(command, interfaceName);
	int if_delete_status = system(command);
	assert(if_delete_status == 0);
	PRINT_TEST_CASE_MSG("Deleted interface for %s\n", bridgeName);
}

/* Takes bridgeName as input parameter and deletes a bridge */
void delete_bridge(const char *bridgeName) {
	bring_if_down(bridgeName);
	char command[300] = "brctl delbr ";
	strcat(command, bridgeName);
	int bridge_delete = system(command);
	assert(bridge_delete == 0);
	PRINT_TEST_CASE_MSG("%s bridge deleted\n", bridgeName);
	sleep(2);
}

/* Creates container on a specified bridge with added interface */
void create_container_on_bridge(const char *containerName, const char *bridgeName, const char *ifName) {
	char command[100] = "lxc-create -t download -n ";
	char cmd[100] = " -- -d ubuntu -r trusty -a ";
	char start[100] = "lxc-start -n ";
	FILE *fPtr;
	char path[300] = "/var/lib/lxc/";
	strcat(path, containerName);
	strcat(path, "/config");
	strcat(command, containerName);
	strcat(command, cmd);
	strcat(command, choose_arch);
	int container_create_status = system(command);
	assert(container_create_status == 0);
	sleep(3);
	assert(fPtr = fopen(path, "a+"));
	fprintf(fPtr, "lxc.net.0.name = eth0\n");
	fprintf(fPtr, "\n");
	fprintf(fPtr, "lxc.net.1.type = veth\n");
	fprintf(fPtr, "lxc.net.1.flags = up\n");
	fprintf(fPtr, "lxc.net.1.link = %s\n", bridgeName);
	fprintf(fPtr, "lxc.net.1.name = %s\n", ifName);
	fprintf(fPtr, "lxc.net.1.hwaddr = 00:16:3e:ab:xx:xx\n");
	fclose(fPtr);
	strcat(start, containerName);
	int container_start_status = system(start);
	assert(container_start_status == 0);
	sleep(3);
	PRINT_TEST_CASE_MSG("Created %s on %s with interface name %s\n", containerName, bridgeName, ifName);
}

/* Configures dnsmasq and iptables for the specified container with inputs of listen address and dhcp range */
void config_dnsmasq(const char *containerName, const char *ifName, const char *listenAddress, const char *dhcpRange) {
	char command[500] = "echo \"apt-get install dnsmasq iptables -y\" | lxc-attach -n ";
	strcat(command, containerName);
	strcat(command, " --");
	int iptables_install_status = system(command);
	assert(iptables_install_status == 0);
	sleep(5);
	char com1[300] = "echo \"echo \"interface=eth1\" >> /etc/dnsmasq.conf\" | lxc-attach -n ";
	strcat(com1, containerName);
	strcat(com1, " --");
	int dnsmasq_status = system(com1);
	assert(dnsmasq_status == 0);
	sleep(5);
	char com2[300] = "echo \"echo \"bind-interfaces\" >> /etc/dnsmasq.conf\" | lxc-attach -n ";
	strcat(com2, containerName);
	strcat(com2, " --");
	dnsmasq_status = system(com2);
	assert(dnsmasq_status == 0);
	sleep(5);
	char com3[300] = "echo \"echo \"listen-address=";
	strcat(com3, listenAddress);
	strcat(com3, "\" >> /etc/dnsmasq.conf\" | lxc-attach -n ");
	strcat(com3, containerName);
	strcat(com3, " --");
	dnsmasq_status = system(com3);
	assert(dnsmasq_status == 0);
	sleep(5);
	char com4[300] = "echo \"echo \"dhcp-range=";
	strcat(com4, dhcpRange);
	strcat(com4, "\" >> /etc/dnsmasq.conf\" | lxc-attach -n ");
	strcat(com4, containerName);
	strcat(com4, " --");
	dnsmasq_status = system(com4);
	assert(dnsmasq_status == 0);
	sleep(5);
	char cmd[300] = "echo \"ifconfig ";
	strcat(cmd, ifName);
	strcat(cmd, " ");
	strcat(cmd, listenAddress);
	strcat(cmd, " netmask 255.255.255.0 up\" | lxc-attach -n ");
	strcat(cmd, containerName);
	strcat(cmd, " --");
	dnsmasq_status = system(cmd);
	assert(dnsmasq_status == 0);
	sleep(2);
	char com[500] = "echo \"service dnsmasq restart >> /dev/null\" | lxc-attach -n ";
	strcat(com, containerName);
	strcat(com, " --");
	dnsmasq_status = system(com);
	assert(dnsmasq_status == 0);
	sleep(2);
	PRINT_TEST_CASE_MSG("Configured dnsmasq in %s with interface name %s, listen-address = %s, dhcp-range = %s\n", containerName, ifName, listenAddress, dhcpRange);
}

/* Configure the NAT rules inside the container */
void config_nat(const char *containerName, const char *listenAddress) {
	char *last_dot_in_ip;
	int last_ip_byte = 0;
	char new_ip[300] = {0};
	strncpy(new_ip, listenAddress, sizeof(new_ip));
	assert(last_dot_in_ip = strrchr(new_ip, '.'));
	assert(snprintf(last_dot_in_ip + 1, 4, "%d", last_ip_byte) >= 0);
	char comd[300] = "echo \"iptables -t nat -A POSTROUTING -s ";
	strcat(comd, new_ip);
	strcat(comd, "/24 ! -d ");
	strcat(comd, new_ip);
	strcat(comd, "/24 -j MASQUERADE\" | lxc-attach -n ");
	strcat(comd, containerName);
	strcat(comd, " --");
	int conf_nat_status = system(comd);
	assert(conf_nat_status == 0);
	sleep(2);
	PRINT_TEST_CASE_MSG("Configured NAT on %s\n", containerName);
}

/* Creates a NAT layer on a specified bridge with certain dhcp range to allocate ips for nodes */
void create_nat_layer(const char *containerName, const char *bridgeName, const char *ifName, const char *listenAddress, char *dhcpRange) {
	create_bridge(bridgeName);
	bring_if_up(bridgeName);
	create_container_on_bridge(containerName, bridgeName, ifName);
	config_dnsmasq(containerName, ifName, listenAddress, dhcpRange);
	config_nat(containerName, listenAddress);
	PRINT_TEST_CASE_MSG("NAT layer created with %s\n", containerName);
}

/* Destroys the NAT layer created */
void destroy_nat_layer(const char *containerName, const char *bridgeName) {
	bring_if_down(bridgeName);
	delete_bridge(bridgeName);
	char command[100] = "lxc-stop -n ";
	strcat(command, containerName);
	int container_stop_status = system(command);
	assert(container_stop_status == 0);
	char destroy[100] = "lxc-destroy -n ";
	strcat(destroy, containerName);
	strcat(destroy, " -s");
	int container_destroy_status = system(destroy);
	assert(container_destroy_status == 0);
	PRINT_TEST_CASE_MSG("NAT layer destroyed with %s\n", containerName);
}

/* Add incoming firewall rules for ipv4 addresses with packet type and port number */
void incoming_firewall_ipv4(const char *packetType, int portNumber) {
	char buf[5];
	snprintf(buf, sizeof(buf), "%d", portNumber);
	assert(system("iptables -F") == 0);
	assert(system("iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT") == 0);
	assert(system("iptables -A INPUT -s 127.0.0.1 -d 127.0.0.1 -j ACCEPT") == 0);
	char command[100] = "iptables -A INPUT -p ";
	strcat(command, packetType);
	strcat(command, " --dport ");
	strcat(command, buf);
	strcat(command, " -j ACCEPT");
	assert(system(command) == 0);
	sleep(2);
	assert(system("iptables -A INPUT -j DROP") == 0);
	PRINT_TEST_CASE_MSG("Firewall for incoming requests added on IPv4");
	assert(system("iptables -L") == 0);
}

/* Add incoming firewall rules for ipv6 addresses with packet type and port number */
void incoming_firewall_ipv6(const char *packetType, int portNumber) {
	char buf[5];
	snprintf(buf, sizeof(buf), "%d", portNumber);
	assert(system("ip6tables -F") == 0);
	assert(system("ip6tables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT") == 0);
	assert(system("ip6tables -A INPUT -s ::1 -d ::1 -j ACCEPT") == 0);
	char command[100] = "ip6tables -A INPUT -p ";
	strcat(command, packetType);
	strcat(command, " --dport ");
	strcat(command, buf);
	strcat(command, " -j ACCEPT");
	assert(system(command) == 0);
	sleep(2);
	assert(system("ip6tables -A INPUT -j DROP") == 0);
	PRINT_TEST_CASE_MSG("Firewall for incoming requests added on IPv6");
	assert(system("ip6tables -L") == 0);
}

/* Add outgoing firewall rules for ipv4 addresses with packet type and port number */
void outgoing_firewall_ipv4(const char *packetType, int portNumber) {
	char buf[5];
	snprintf(buf, sizeof(buf), "%d", portNumber);
	assert(system("iptables -F") == 0);
	assert(system("iptables -A OUTPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT") == 0);
	assert(system("iptables -A OUTPUT -s 127.0.0.1 -d 127.0.0.1 -j ACCEPT") == 0);
	char command[100] = "iptables -A OUTPUT -p ";
	strcat(command, packetType);
	strcat(command, " --dport ");
	strcat(command, buf);
	strcat(command, " -j ACCEPT");
	assert(system(command) == 0);
	sleep(2);
	assert(system("iptables -A OUTPUT -j DROP") == 0);
	PRINT_TEST_CASE_MSG("Firewall for outgoing requests added on IPv4");
	assert(system("iptables -L") == 0);
}

/* Add outgoing firewall rules for ipv6 addresses with packet type and port number */
void outgoing_firewall_ipv6(const char *packetType, int portNumber) {
	char buf[5];
	snprintf(buf, sizeof(buf), "%d", portNumber);
	assert(system("ip6tables -F") == 0);
	assert(system("ip6tables -A OUTPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT") == 0);
	assert(system("ip6tables -A OUTPUT -s ::1 -d ::1 -j ACCEPT") == 0);
	char command[100] = "ip6tables -A OUTPUT -p ";
	strcat(command, packetType);
	strcat(command, " --dport ");
	strcat(command, buf);
	strcat(command, " -j ACCEPT");
	assert(system(command) == 0);
	sleep(2);
	assert(system("ip6tables -A OUTPUT -j DROP") == 0);
	PRINT_TEST_CASE_MSG("Firewall for outgoing requests added on IPv6");
	assert(system("ip6tables -L") == 0);
}
