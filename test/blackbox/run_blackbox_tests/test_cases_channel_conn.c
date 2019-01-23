/*
    test_cases_channel_conn.c -- Execution of specific meshlink black box test cases
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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <pthread.h>
#include <cmocka.h>
#include "execute_tests.h"
#include "test_cases_channel_conn.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../common/mesh_event_handler.h"

#define PEER_ID "0"
#define NUT_ID  "1"
#define RELAY_ID  "2"

static void test_case_channel_conn_01(void **state);
static bool test_steps_channel_conn_01(void);
static void test_case_channel_conn_02(void **state);
static bool test_steps_channel_conn_02(void);
static void test_case_channel_conn_03(void **state);
static bool test_steps_channel_conn_03(void);
static void test_case_channel_conn_04(void **state);
static bool test_steps_channel_conn_04(void);
static void test_case_channel_conn_05(void **state);
static bool test_steps_channel_conn_05(void);
static void test_case_channel_conn_06(void **state);
static bool test_steps_channel_conn_06(void);
static void test_case_channel_conn_07(void **state);
static bool test_steps_channel_conn_07(void);
static void test_case_channel_conn_08(void **state);
static bool test_steps_channel_conn_08(void);

static char *test_channel_conn_2_nodes[] = { "peer", "nut" };
static char *test_channel_conn_3_nodes[] = { "peer", "nut", "relay" };

static black_box_state_t test_case_channel_conn_01_state = {
	.test_case_name = "test_case_channel_conn_01",
	.node_names = test_channel_conn_2_nodes,
	.num_nodes = 2,
};
static black_box_state_t test_case_channel_conn_02_state = {
	.test_case_name = "test_case_channel_conn_02",
	.node_names = test_channel_conn_2_nodes,
	.num_nodes = 2,
};
static black_box_state_t test_case_channel_conn_03_state = {
	.test_case_name = "test_case_channel_conn_03",
	.node_names = test_channel_conn_2_nodes,
	.num_nodes = 2,
};
static black_box_state_t test_case_channel_conn_04_state = {
	.test_case_name = "test_case_channel_conn_04",
	.node_names = test_channel_conn_2_nodes,
	.num_nodes = 2,
};
static black_box_state_t test_case_channel_conn_05_state = {
	.test_case_name = "test_case_channel_conn_05",
	.node_names = test_channel_conn_3_nodes,
	.num_nodes = 3,
};
static black_box_state_t test_case_channel_conn_06_state = {
	.test_case_name = "test_case_channel_conn_06",
	.node_names = test_channel_conn_3_nodes,
	.num_nodes = 3,
};
static black_box_state_t test_case_channel_conn_07_state = {
	.test_case_name = "test_case_channel_conn_07",
	.node_names = test_channel_conn_3_nodes,
	.num_nodes = 3,
};
static black_box_state_t test_case_channel_conn_08_state = {
	.test_case_name = "test_case_channel_conn_08",
	.node_names = test_channel_conn_3_nodes,
	.num_nodes = 3,
};

static bool joined;
static bool channel_opened;
static bool node_restarted;
static bool received_error;
static bool channel_received;
static bool node_reachable;
static bool node_unreachable;

/* Callback function for handling channel connection test cases mesh events */
static bool channel_conn_cb(mesh_event_payload_t payload) {
	switch(payload.mesh_event) {
	case NODE_JOINED            :
		joined = true;
		break;

	case CHANNEL_OPENED         :
		channel_opened = true;
		break;

	case NODE_RESTARTED         :
		node_restarted = true;
		break;

	case ERR_NETWORK            :
		received_error = true;
		break;

	case CHANNEL_DATA_RECIEVED  :
		channel_received = true;
		break;

	case NODE_UNREACHABLE       :
		node_unreachable = true;
		break;

	case NODE_REACHABLE         :
		node_reachable = true;
		break;

	default                     :
		PRINT_TEST_CASE_MSG("Undefined event occurred\n");
	}

	return true;
}

/* Execute channel connections Test Case # 1 - simulate a temporary network
    failure of about 30 seconds, messages sent while the network was down
    should be received by the other side after the network comes up again. */
static void test_case_channel_conn_01(void **state) {
	execute_test(test_steps_channel_conn_01, state);
	return;
}

/* Test Steps for channel connections Test Case # 1

    Test Steps:
    1. Run NUT & peer node instances and open a channel between them
    2. Simulate a network failure in NUT's container for about 30 secs,
        meanwhile send data via channel from NUT to peer.
    3. After restoring network, peer node receive's data via channel.

    Expected Result:
    Peer node receives data via channel without any error after restoring network.
*/
static bool test_steps_channel_conn_01(void) {
	char *invite_nut;
	char *import;

	joined = false;
	channel_opened = false;
	channel_received = false;

	// Setup Containers

	install_in_container("nut", "iptables");
	accept_port_rule("nut", "OUTPUT", "udp", 9000);
	import = mesh_event_sock_create(eth_if_name);
	invite_nut = invite_in_container("peer", "nut");
	assert(invite_nut);

	// Run node instances in containers & open a channel

	node_sim_in_container_event("peer", "1", NULL, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	wait_for_event(channel_conn_cb, 30);
	assert_int_equal(joined, true);

	wait_for_event(channel_conn_cb, 30);
	assert_int_equal(channel_opened, true);

	// Simulate network failure in NUT's LXC container with it's IP address as NAT rule

	block_node_ip("nut");
	sleep(2);

	// Sending SIGUSR1 signal to node-under-test indicating the network failure

	node_step_in_container("nut", "SIGUSR1");
	sleep(30);

	// Restore NUT's network

	unblock_node_ip("nut");

	// Wait for peer node to receive data via channel from NUT

	wait_for_event(channel_conn_cb, 60);

	mesh_event_destroy();
	free(invite_nut);
	free(import);

	assert_int_equal(channel_received, true);

	return true;
}

/* Execute channel connections Test Case # 2 - a simulated network failure
    of more than 1 minute, and sending messages over the channel during the
    failure. Then after about 1 minute, the channel should receive an error */
static void test_case_channel_conn_02(void **state) {
	execute_test(test_steps_channel_conn_02, state);
	return;
}

/* Test Steps for channel connections Test Case # 2

    Test Steps:
    1. Run NUT and peer node instances in containers and open a channel between them.
    2. Create a network failure for about 90 secs in NUT container
        and signal NUT node about the network failure.
    3. Meanwhile NUT sends data to peer via channel and restore the network after
        90 secs.

    Expected Result:
      Peer node should receive error closing the channel after channel timeout(60 secs).
*/
static bool test_steps_channel_conn_02(void) {
	char *invite_nut;
	char *import;

	joined = false;
	channel_opened = false;
	received_error = false;

	// Setup containers

	install_in_container("nut", "iptables");
	accept_port_rule("nut", "OUTPUT", "udp", 9000);
	import = mesh_event_sock_create(eth_if_name);
	invite_nut = invite_in_container("peer", "nut");
	assert(invite_nut);

	// Run NUT and peer node instances in containers & open a channel

	node_sim_in_container_event("peer", "1", NULL, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	wait_for_event(channel_conn_cb, 30);
	assert_int_equal(joined, true);

	wait_for_event(channel_conn_cb, 10);
	assert_int_equal(channel_opened, true);

	// Simulate network failure in NUT's LXC container with it's IP address as NAT rule

	block_node_ip("nut");

	// Sending SIGUSR1 signal to node-under-test indicating the network failure

	node_step_in_container("nut", "SIGUSR1");
	sleep(90);

	// Restore NUT containers network after 90 secs

	unblock_node_ip("nut");

	// Wait for peer node to send the event about the channel error occurred with length = 0

	wait_for_event(channel_conn_cb, 90);

	mesh_event_destroy();
	free(invite_nut);
	free(import);

	assert_int_equal(received_error, true);

	return true;
}

/* Execute channel connections Test Case # 3 - a simulated network failure
    once node instance is made offline restore the network and send data via
    channel  */
static void test_case_channel_conn_03(void **state) {
	execute_test(test_steps_channel_conn_03, state);
	return;
}

/* Test Steps for channel connections Test Case # 3

    Test Steps:
    1. Run NUT and peer node instances and open a channel between them.
    2. Create a network failure in NUT container, bring NUT node offline
        and receive the status at test driver and restore the network
    3. After peer node instance is reachable to NUT node send data via channel

    Expected Result:
    Peer node should receive data from NUT without any error.
*/
static bool test_steps_channel_conn_03(void) {
	char *invite_nut;
	char *import;

	joined = false;
	channel_opened = false;
	node_unreachable = false;
	node_reachable = false;
	channel_received = false;

	// Setup containers

	install_in_container("nut", "iptables");
	accept_port_rule("nut", "OUTPUT", "udp", 9000);
	import = mesh_event_sock_create(eth_if_name);
	invite_nut = invite_in_container("peer", "nut");
	assert(invite_nut);

	// Run NUT and peer node instances in containers & open a channel

	node_sim_in_container_event("peer", "1", NULL, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	wait_for_event(channel_conn_cb, 30);
	assert_int_equal(joined, true);

	wait_for_event(channel_conn_cb, 10);
	assert_int_equal(channel_opened, true);

	// Simulate network failure in NUT's LXC container with it's IP address as NAT rule

	node_reachable = false;
	block_node_ip("nut");

	// Sending SIGUSR1 signal to node-under-test indicating the network failure

	node_step_in_container("nut", "SIGUSR1");

	// Wait for the node status to become unreachable

	wait_for_event(channel_conn_cb, 100);
	assert_int_equal(node_unreachable, true);

	// Restore NUT container's network

	unblock_node_ip("nut");

	// Wait for the node status to become reachable

	wait_for_event(channel_conn_cb, 100);
	assert_int_equal(node_reachable, true);

	// Wait for data to be received at peer via channel from NUT after restoring n/w

	wait_for_event(channel_conn_cb, 90);

	mesh_event_destroy();
	free(invite_nut);
	free(import);

	assert_int_equal(channel_received, true);

	return true;
}

/* Execute channel connections Test Case # 4 - receiving an error when node-under-test
    tries to send data on channel to peer node after peer node stops and starts the
    node instance */
static void test_case_channel_conn_04(void **state) {
	execute_test(test_steps_channel_conn_04, state);
	return;
}

/* Test Steps for Meta-connections Test Case # 4

    Test Steps:
    1. Run peer and NUT node instances in containers and open a channel between them.
    2. Stop and start the NUT node instance and wait for about > 60 secs.
    3. Send data via channel from Peer node and wait for event in test driver.

    Expected Result:
    Peer node should receive error(as length = 0) in receive callback of peer node's instance.
*/
static bool test_steps_channel_conn_04(void) {
	char *invite_nut;
	char *import;

	joined = false;
	channel_opened = false;
	node_restarted = false;
	received_error = false;
	import = mesh_event_sock_create(eth_if_name);
	invite_nut = invite_in_container("peer", "nut");
	assert(invite_nut);

	// Run NUT and peer node instances in containers and open a channel

	node_sim_in_container_event("peer", "1", NULL, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	wait_for_event(channel_conn_cb, 10);
	assert_int_equal(joined, true);

	wait_for_event(channel_conn_cb, 10);
	assert_int_equal(channel_opened, true);

	// Wait for NUT node instance to stop and start

	wait_for_event(channel_conn_cb, 60);
	assert_int_equal(node_restarted, true);

	sleep(60);

	// After 1 min the channel between NUT and peer should result in error

	wait_for_event(channel_conn_cb, 10);


	mesh_event_destroy();
	free(invite_nut);
	free(import);

	assert_int_equal(received_error, true);

	return true;
}

/* Execute channel connections Test Case # 5 - simulate a temporary network
    failure of about 30 seconds, messages sent while the network was down
    should be received by the other side after the network comes up again. */
static void test_case_channel_conn_05(void **state) {
	execute_test(test_steps_channel_conn_05, state);
	return;
}

/* Test Steps for channel connections Test Case # 5

    Test Steps:
    1. Run NUT, relay & peer node instances with relay inviting NUT and peer
        and open a channel between them
    2. Simulate a network failure in NUT's container for about 30 secs,
        meanwhile send data via channel from NUT to peer.
    3. After restoring network, peer node receive's data via channel.

    Expected Result:
    Peer node receives data via channel without any error after restoring network.
*/
static bool test_steps_channel_conn_05(void) {
	char *invite_nut, *invite_peer;
	char *import;

	joined = false;
	channel_opened = false;
	channel_received = false;

	// Setup containers

	install_in_container("nut", "iptables");
	accept_port_rule("nut", "OUTPUT", "udp", 9000);
	import = mesh_event_sock_create(eth_if_name);
	invite_peer = invite_in_container("relay", "peer");
	invite_nut = invite_in_container("relay", "nut");
	assert(invite_nut);
	assert(invite_peer);

	// Run node instances and open a channel between NUT and peer nodes

	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	wait_for_event(channel_conn_cb, 30);
	assert_int_equal(joined, true);

	wait_for_event(channel_conn_cb, 30);
	assert_int_equal(channel_opened, true);

	// Create a network failure in NUT node's container with it's IP address

	block_node_ip("nut");

	// Sending SIGUSR1 signal to node-under-test indicating the network failure

	node_step_in_container("nut", "SIGUSR1");
	sleep(30);

	// Restore the network

	unblock_node_ip("nut");

	// Wait for peer to get data from NUT node via channel after restoring network in < 60 secs

	wait_for_event(channel_conn_cb, 60);

	mesh_event_destroy();
	free(invite_peer);
	free(invite_nut);
	free(import);

	assert_int_equal(channel_received, true);

	return true;
}

/* Execute channel connections Test Case # 6 - a simulated network failure
    of more than 1 minute, and sending messages over the channel during the
    failure. Then after about 1 minute, the channel should receive an error */
static void test_case_channel_conn_06(void **state) {
	execute_test(test_steps_channel_conn_06, state);
	return;
}

/* Test Steps for channel connections Test Case # 6

    Test Steps:
    1. Run NUT, relay & peer node instances with relay inviting NUT and peer
        and open a channel between them
    2. Create a network failure for about 90 secs in NUT container
        and signal NUT node about the network failure.
    3. Meanwhile NUT sends data to peer via channel and restore the network after
        90 secs.

    Expected Result:
      Peer node should receive error closing the channel after channel timeout(60 secs).
*/
static bool test_steps_channel_conn_06(void) {
	char *invite_nut, *invite_peer;
	char *import;

	joined = false;
	channel_opened = false;
	received_error = false;

	// Setup containers

	install_in_container("nut", "iptables");
	accept_port_rule("nut", "OUTPUT", "udp", 9000);
	import = mesh_event_sock_create(eth_if_name);
	invite_peer = invite_in_container("relay", "peer");
	assert(invite_peer);
	invite_nut = invite_in_container("relay", "nut");
	assert(invite_nut);

	// Run nodes in containers and open a channel between NUt and peer

	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	wait_for_event(channel_conn_cb, 30);
	assert_int_equal(joined, true);

	wait_for_event(channel_conn_cb, 10);
	assert_int_equal(channel_opened, true);

	// Simulate a network failure in NUT's container for > 60 secs

	block_node_ip("nut");

	// Sending SIGUSR1 signal to node-under-test indicating the network failure

	node_step_in_container("nut", "SIGUSR1");
	sleep(90);

	// Restore the network after 90 secs

	unblock_node_ip("nut");

	// Wait for channel to receive error and receive the event

	wait_for_event(channel_conn_cb, 90);

	mesh_event_destroy();
	free(invite_peer);
	free(invite_nut);
	free(import);

	assert_int_equal(received_error, true);

	return true;
}

/* Execute channel connections Test Case # 7 - a simulated network failure
    once node instance is made offline restore the network and send data via
    channel  */
static void test_case_channel_conn_07(void **state) {
	execute_test(test_steps_channel_conn_07, state);
	return;
}

/* Test Steps for channel connections Test Case # 7

    Test Steps:
    1. Run NUT, relay & peer node instances with relay inviting NUT and peer
        and open a channel between them
    2. Create a network failure in NUT container, bring NUT node offline
        and receive the status at test driver and restore the network
    3. After peer node instance is reachable to NUT node send data via channel

    Expected Result:
    Peer node should receive data from NUT without any error.
*/
static bool test_steps_channel_conn_07(void) {
	char *invite_nut, *invite_peer;
	char *import;

	joined = false;
	channel_opened = false;
	node_unreachable = false;
	node_reachable = false;
	channel_received = false;

	// Setup containers

	install_in_container("nut", "iptables");
	accept_port_rule("nut", "OUTPUT", "udp", 9000);
	import = mesh_event_sock_create(eth_if_name);
	invite_peer = invite_in_container("relay", "peer");
	invite_nut = invite_in_container("relay", "nut");
	assert(invite_nut);
	assert(invite_peer);

	// Run nodes and open a channel

	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	wait_for_event(channel_conn_cb, 30);
	assert_int_equal(joined, true);

	wait_for_event(channel_conn_cb, 15);
	assert_int_equal(channel_opened, true);

	// Simulate a network failure

	node_reachable = false;
	block_node_ip("nut");

	// Sending SIGUSR1 signal to node-under-test indicating the network failure

	node_step_in_container("nut", "SIGUSR1");

	// Wait for node to become unreachable

	wait_for_event(channel_conn_cb, 100);
	assert_int_equal(node_unreachable, true);

	// Restore the network

	unblock_node_ip("nut");

	// Wait for node to become reachable after restoring n/w

	wait_for_event(channel_conn_cb, 100);
	assert_int_equal(node_reachable, true);

	// Wait for peer node to receive data via channel without any error

	wait_for_event(channel_conn_cb, 90);

	mesh_event_destroy();
	free(invite_peer);
	free(invite_nut);
	free(import);

	assert_int_equal(channel_received, true);

	return true;
}

/* Execute channel connections Test Case # 8 - receiving an error when node-under-test
    tries to send data on channel to peer node after peer node stops and starts the
    node instance */
static void test_case_channel_conn_08(void **state) {
	execute_test(test_steps_channel_conn_08, state);
	return;
}

/* Test Steps for Meta-connections Test Case # 8

    Test Steps:
    1. Run NUT, relay & peer node instances with relay inviting NUT and peer
        and open a channel between them
    2. Stop and start the NUT node instance and wait for about > 60 secs.
    3. Send data via channel from Peer node and wait for event in test driver.

    Expected Result:
    Peer node should receive error(as length = 0) in receive callback of peer node's instance.
*/
static bool test_steps_channel_conn_08(void) {
	char *invite_nut, *invite_peer;
	char *import;

	joined = false;
	channel_opened = false;
	node_restarted = false;
	received_error = false;

	// Setup containers

	import = mesh_event_sock_create(eth_if_name);
	invite_peer = invite_in_container("relay", "peer");
	invite_nut = invite_in_container("relay", "nut");
	assert(invite_nut);
	assert(invite_peer);

	// Run nodes and open a channel between NUT and peer

	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	wait_for_event(channel_conn_cb, 10);
	assert_int_equal(joined, true);

	wait_for_event(channel_conn_cb, 10);
	assert_int_equal(channel_opened, true);

	// Wait for NUT node to restart it's instance

	wait_for_event(channel_conn_cb, 60);
	assert_int_equal(node_restarted, true);

	sleep(60);

	// Signal peer to send data to NUT node via channel

	node_step_in_container("peer", "SIGUSR1");

	// Wait for peer to receive channel error

	wait_for_event(channel_conn_cb, 10);

	mesh_event_destroy();
	free(invite_peer);
	free(invite_nut);
	free(import);

	assert_int_equal(received_error, true);

	return true;
}

static int black_box_group_setup(void **state) {
	const char *nodes[] = { "peer", "nut", "relay" };
	int num_nodes = sizeof(nodes) / sizeof(nodes[0]);

	printf("Creating Containers\n");
	destroy_containers();
	create_containers(nodes, num_nodes);

	return 0;
}

static int black_box_group_teardown(void **state) {
	printf("Destroying Containers\n");
	destroy_containers();

	return 0;
}

int test_meshlink_channel_conn(void) {
	const struct CMUnitTest blackbox_group0_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_conn_01, setup_test, teardown_test,
		(void *)&test_case_channel_conn_01_state),/*
                cmocka_unit_test_prestate_setup_teardown(test_case_channel_conn_02, setup_test, teardown_test,
                                (void *)&test_case_channel_conn_02_state),
                cmocka_unit_test_prestate_setup_teardown(test_case_channel_conn_03, setup_test, teardown_test,
                                (void *)&test_case_channel_conn_03_state),
                cmocka_unit_test_prestate_setup_teardown(test_case_channel_conn_04, setup_test, teardown_test,
                                (void *)&test_case_channel_conn_04_state),
                cmocka_unit_test_prestate_setup_teardown(test_case_channel_conn_05, setup_test, teardown_test,
                                (void *)&test_case_channel_conn_05_state),
                cmocka_unit_test_prestate_setup_teardown(test_case_channel_conn_06, setup_test, teardown_test,
                                (void *)&test_case_channel_conn_06_state),
                cmocka_unit_test_prestate_setup_teardown(test_case_channel_conn_07, setup_test, teardown_test,
                                (void *)&test_case_channel_conn_07_state),
                cmocka_unit_test_prestate_setup_teardown(test_case_channel_conn_08, setup_test, teardown_test,
                                (void *)&test_case_channel_conn_08_state)*/
	};
	total_tests += sizeof(blackbox_group0_tests) / sizeof(blackbox_group0_tests[0]);

	return cmocka_run_group_tests(blackbox_group0_tests, black_box_group_setup, black_box_group_teardown);
}
