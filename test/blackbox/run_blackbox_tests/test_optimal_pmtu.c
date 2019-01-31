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
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include "execute_tests.h"
#include "test_cases.h"
#include "pthread.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../common/mesh_event_handler.h"
#include "test_optimal_pmtu.h"

#define RELAY_ID "0"
#define PEER_ID  "1"
#define NUT_ID   "2"
#define PEER_NAT "peer_nat"
#define NUT_NAT "nut_nat"

#pragma pack(1)

static void test_case_optimal_pmtu_01(void **state);
static bool test_steps_optimal_pmtu_01(void);
static void test_case_optimal_pmtu_02(void **state);
static bool test_steps_optimal_pmtu_02(void);
static void test_case_optimal_pmtu_03(void **state);
static bool test_steps_optimal_pmtu_03(void);
static void test_case_optimal_pmtu_04(void **state);
static bool test_steps_optimal_pmtu_04(void);
static void test_case_optimal_pmtu_05(void **state);
static bool test_steps_optimal_pmtu_05(void);
static void test_case_optimal_pmtu_06(void **state);
static bool test_steps_optimal_pmtu_06(void);
static void test_case_optimal_pmtu_07(void **state);
static bool test_steps_optimal_pmtu_07(void);
static void tcpdump_in_container(const char *cmd, const char *container_name);

static char *test_optimal_pmtu_1_nodes[] = { "relay", "peer", "nut" };
static black_box_state_t test_pmtu_state_01 = {
	.test_case_name =  "test_case_optimal_pmtu_01",
	.node_names =  test_optimal_pmtu_1_nodes,
	.num_nodes =  3,
};

static black_box_state_t test_pmtu_state_02 = {
	.test_case_name =  "test_case_optimal_pmtu_02",
	.node_names =  test_optimal_pmtu_1_nodes,
	.num_nodes =  3,
};

static black_box_state_t test_pmtu_state_03 = {
	.test_case_name =  "test_case_optimal_pmtu_03",
	.node_names =  test_optimal_pmtu_1_nodes,
	.num_nodes =  3,
};

static black_box_state_t test_pmtu_state_04 = {
	.test_case_name =  "test_case_optimal_pmtu_04",
	.node_names =  test_optimal_pmtu_1_nodes,
	.num_nodes =  3,
};

static black_box_state_t test_pmtu_state_05 = {
	.test_case_name =  "test_case_optimal_pmtu_05",
	.node_names =  test_optimal_pmtu_1_nodes,
	.num_nodes =  3,
};

static black_box_state_t test_pmtu_state_06 = {
	.test_case_name =  "test_case_optimal_pmtu_06",
	.node_names =  test_optimal_pmtu_1_nodes,
	.num_nodes =  3,
};

static black_box_state_t test_pmtu_state_07 = {
	.test_case_name =  "test_case_optimal_pmtu_07",
	.node_names =  test_optimal_pmtu_1_nodes,
	.num_nodes =  3,
};

static pmtu_attr_t node_pmtu_peer;
static pmtu_attr_t node_pmtu_relay;
static char *import = NULL;


static void tcpdump_in_container(const char *cmd, const char *container_name) {
	execute_in_container(cmd, container_name, true);
}

static int setup_pmtu_test_case(void **state) {
	char *ret_str;
	char container_name[200];

	PRINT_TEST_CASE_MSG("\n\n======================================================================\n\n\n");

	fprintf(stderr, "Setting up Containers\n");
	state_ptr = (black_box_state_t *)(*state);

	// Setup containers for test cases
	setup_containers(state);

	// Switch bridges of test containers from lxcbr0 to it's respective NAT bridges
	assert(snprintf(container_name, sizeof(container_name), "%s_%s", state_ptr->test_case_name, "nut") >= 0);
	container_switch_bridge(container_name, lxc_path, lxc_bridge, "nut_nat_bridge");
	assert(snprintf(container_name, sizeof(container_name), "%s_%s", state_ptr->test_case_name, "peer") >= 0);
	container_switch_bridge(container_name, lxc_path, lxc_bridge, "peer_nat_bridge");

	// Flush iptable filter rules if there are any
	flush_nat_rules(NUT_NAT, "-t filter");
	flush_nat_rules(PEER_NAT, "-t filter");

	// Reset the MTU size of NAT's public interface to 1500 bytes
	assert(change_container_mtu(NUT_NAT, PUB_INTERFACE, 1500) == NULL);
	assert(change_container_mtu(PEER_NAT, PUB_INTERFACE, 1500) == NULL);

	// Flush all the pushed data in the meshlink event handler receive queue
	mesh_events_flush();

	return EXIT_SUCCESS;
}

static int teardown_pmtu_test_case(void **state) {

	// Reset the MTU size of NAT's public interface to 1500 bytes
	change_container_mtu(NUT_NAT, PUB_INTERFACE, 1500);
	change_container_mtu(PEER_NAT, PUB_INTERFACE, 1500);

	// Flush iptable filter rules if there are any
	flush_nat_rules(NUT_NAT, "-t filter");
	flush_nat_rules(PEER_NAT, "-t filter");

	black_box_state_t *test_state = (black_box_state_t *)(*state);
	int i;

	// Send SIG_TERM to all the running nodes
	for(i = 0; i < test_state->num_nodes; i++) {
		/* Shut down node */
		node_step_in_container(test_state->node_names[i], "SIGTERM");
	}

	return teardown_test(state);
}

// Print the calculated values of MTU parameters obtained from the node
static void print_mtu_calc(pmtu_attr_t node_pmtu) {
	fprintf(stderr, "MTU size : %d\n", node_pmtu.mtu_size);
	fprintf(stderr, "Probes took for calculating PMTU discovery : %d\n", node_pmtu.mtu_discovery.probes);
	fprintf(stderr, "Probes total length took for calculating PMTU discovery : %d\n", node_pmtu.mtu_discovery.probes_total_len);
	fprintf(stderr, "Time took for calculating PMTU discovery : %lu\n", node_pmtu.mtu_discovery.time);
	fprintf(stderr, "Total MTU ping probes : %d\n", node_pmtu.mtu_ping.probes);
	fprintf(stderr, "Total MTU ping probes length : %d\n", node_pmtu.mtu_ping.probes_total_len);
	float avg = 0;

	if(node_pmtu.mtu_ping.probes) {
		avg = (float)node_pmtu.mtu_ping.time / (float)node_pmtu.mtu_ping.probes;
	}

	fprintf(stderr, "Average MTU ping probes ping time : %f\n", avg);
	fprintf(stderr, "Total probes received %d\n", node_pmtu.mtu_recv_probes.probes);
	fprintf(stderr, "Total probes sent %d\n", node_pmtu.mtu_sent_probes.probes);
}

static int black_box_pmtu_group_setup(void **state) {
	const char *nodes[] = { "relay", "peer", "nut" };
	int num_nodes = sizeof(nodes) / sizeof(nodes[0]);

	/*
	                        ------------
	    ____________________|  lxcbr0  |_____________________
	   |eth0                ------------                    |eth0
	 -------------               |                  --------------
	// NUT NAT  //               |                 //  Peer NAT //
	 ------------           -------------           --------------
	       |eth1            |           |                   |eth1
	       |                |   relay   |                   |
	-------------------     |           |        --------------------
	|  nut_nat_bridge |     -------------        |  peer_nat_bridge |
	-------------------                          --------------------
	       |                                                |
	---------------                                ------------------
	|             |                                |                |
	|     NUT     |                                |      peer      |
	|             |                                |                |
	---------------                                ------------------
	*/

	PRINT_TEST_CASE_MSG("Creating Containers\n");

	// Create Node-Under-Test(NUT) and Peer node's containers

	destroy_containers();
	create_containers(nodes, num_nodes);

	// Create NAT containers for NUT and peer nodes

	nat_create(NUT_NAT, lxc_path, FULLCONE_NAT);
	nat_create(PEER_NAT, lxc_path, FULLCONE_NAT);

	tcpdump_in_container("tcpdump udp -i any > tcpdump.log", NUT_NAT);

	/*char cmd[200];
	assert(snprintf(cmd, sizeof(cmd),
	                "%s/" LXC_UTIL_REL_PATH "/log_drops.sh %s",
	                meshlink_root_path, NUT_NAT) >= 0);
	assert(system(cmd) == 0);*/

	// Switch the Node-under-test node and peer node containers to the respective bridge
	// i.e making the node to sit behind the NAT's private network interface

	container_switch_bridge("run_nut", lxc_path, lxc_bridge, "nut_nat_bridge");
	container_switch_bridge("run_peer", lxc_path, lxc_bridge, "peer_nat_bridge");

	// Open mesh event handling UDP socket
	import = mesh_event_sock_create(eth_if_name);
	assert(import);

	return 0;
}

static int black_box_pmtu_group_teardown(void **state) {
	PRINT_TEST_CASE_MSG("Destroying Containers\n");
	destroy_containers();
	nat_destroy(NUT_NAT);
	nat_destroy(PEER_NAT);

	mesh_event_destroy();

	return 0;
}

static bool mtu_calc_peer = false;
static bool mtu_calc_relay = false;

// Common event handler callback for all the test cases
static bool event_mtu_handle_cb(mesh_event_payload_t payload) {
	char event_node_name[][10] = {"RELAY", "PEER", "NUT"};
	fprintf(stderr, " %s : ", event_node_name[payload.client_id]);
	char *name;
	uint32_t payload_length = payload.payload_length;
	uint32_t node_pmtu_calc_size = sizeof(pmtu_attr_t);

	switch(payload.mesh_event) {
	case META_CONN_CLOSED   :
		name = (char *)payload.payload;
		fprintf(stderr, "NUT closed connection with %s\n", name);
		break;

	case META_CONN_SUCCESSFUL   :
		name = (char *)payload.payload;
		fprintf(stderr, "NUT made connection with %s\n", name);
		break;

	case NODE_JOINED  :
		name = (char *)payload.payload;
		fprintf(stderr, "Node %s joined with NUT\n", name);
		break;

	case NODE_LEFT  :
		name = (char *)payload.payload;
		fprintf(stderr, "Node %s the left mesh\n", name);
		break;

	case ERR_NETWORK            :
		name = (char *)payload.payload;
		fprintf(stderr, "NUT closed channel with %s\n", name);
		break;

	case NODE_STARTED           :
		fprintf(stderr, "%s node started\n", event_node_name[payload.client_id]);
		break;

	case CHANNEL_OPENED         :
		fprintf(stderr, "Channel opened\n");
		break;

	case OPTIMAL_PMTU_PEER      :
		assert(payload.payload);
		fprintf(stderr, "Obtained peer MTU values from NUT\n");
		memcpy(&node_pmtu_peer, payload.payload, payload_length);
		fprintf(stderr, "NUT and peer PMTU handling in 120 seconds with ICMP unblocked\n");
		print_mtu_calc(node_pmtu_peer);
		mtu_calc_peer = true;

		if(mtu_calc_peer && mtu_calc_relay) {
			return true;
		}

		break;

	case OPTIMAL_PMTU_RELAY      :
		assert(payload.payload);
		//assert(payload_length != node_pmtu_calc_size);
		fprintf(stderr, "Obtained relay MTU values from NUT\n");
		memcpy(&node_pmtu_relay, payload.payload, payload_length);
		fprintf(stderr, "NUT and peer PMTU handling in 120 seconds with ICMP unblocked\n");
		print_mtu_calc(node_pmtu_relay);
		mtu_calc_relay = true;

		if(mtu_calc_peer && mtu_calc_relay) {
			return true;
		}

		break;

	default :
		fprintf(stderr, "UNDEFINED EVENT RECEIVED (%d)\n", payload.mesh_event);
	}

	return false;
}

/* Test Steps for optimal PMTU discovery Test Case # 1 -
    Validating NUT MTU parameters without blocking ICMP under designed
    network topology */
static void test_case_optimal_pmtu_01(void **state) {
	execute_test(test_steps_optimal_pmtu_01, state);
}

/* Test Steps for optimal PMTU discovery Test Case # 1 - Success case

    Test Steps:
    1. Create NAT setup and run each node instances in discrete containers.
    2. Open a channel from NUT to peer and hence triggering Peer to peer connection
    3. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_01(void) {
	char *invite_peer, *invite_nut;

	mtu_calc_peer = false;
	mtu_calc_relay = false;
	memset(&node_pmtu_peer, 0, sizeof(node_pmtu_peer));
	memset(&node_pmtu_relay, 0, sizeof(node_pmtu_relay));

	// Invite peer and nut nodes by relay node
	invite_peer = invite_in_container("relay", "peer");
	assert(invite_peer);
	invite_nut = invite_in_container("relay", NUT_NODE_NAME);
	assert(invite_nut);

	// Launch NUT, relay and peer nodes in discrete containers
	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	// Wait for test case events
	bool ret = wait_for_event(event_mtu_handle_cb, PING_TRACK_TIMEOUT + 100);
	assert_int_equal(ret && mtu_calc_peer && mtu_calc_relay, true);

	// Verify the obtained values
	assert_in_range(node_pmtu_peer.mtu_size, 1450, 1501);
	assert_in_range(node_pmtu_peer.mtu_discovery.probes, 38, 42);
	assert_in_range(node_pmtu_relay.mtu_size, 1450, 1501);
	assert_in_range(node_pmtu_relay.mtu_discovery.probes, 38, 42);

	return true;
}

/* Test Steps for optimal PMTU discovery Test Case # 2 -
    Validating NUT MTU parameters blocking ICMP under designed
    network topology */
static void test_case_optimal_pmtu_02(void **state) {
	execute_test(test_steps_optimal_pmtu_02, state);
}

/* Test Steps for optimal PMTU discovery Test Case # 2 -

    Test Steps:
    1. Create NAT setup and run each node instances in discrete containers,
    2. Block ICMP protocol at NUT's NAT
    3. Open a channel from NUT to peer and hence triggering Peer to peer connection
    4. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_02(void) {
	char *invite_peer, *invite_nut;

	mtu_calc_peer = false;
	mtu_calc_relay = false;
	memset(&node_pmtu_peer, 0, sizeof(node_pmtu_peer));
	memset(&node_pmtu_relay, 0, sizeof(node_pmtu_relay));

	block_icmp(NUT_NAT);

	invite_peer = invite_in_container("relay", "peer");
	assert(invite_peer);
	invite_nut = invite_in_container("relay", NUT_NODE_NAME);
	assert(invite_nut);
	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	bool ret = wait_for_event(event_mtu_handle_cb, PING_TRACK_TIMEOUT + 100);
	assert_int_equal(ret && mtu_calc_peer && mtu_calc_relay, true);

	assert_in_range(node_pmtu_peer.mtu_size, 1450, 1501);
	assert_in_range(node_pmtu_peer.mtu_discovery.probes, 38, 42);
	assert_in_range(node_pmtu_relay.mtu_size, 1450, 1501);
	assert_in_range(node_pmtu_relay.mtu_discovery.probes, 38, 42);
	return true;
}

/* Test Steps for optimal PMTU discovery Test Case # 3 -
    Validating NUT MTU parameters with MTU size of NAT = 1250 under designed
    network topology */
static void test_case_optimal_pmtu_03(void **state) {
	execute_test(test_steps_optimal_pmtu_03, state);
}

/* Test Steps for optimal PMTU discovery Test Case # 3 -

    Test Steps:
    1. Create NAT setup and run each node instances in discrete containers,
    2. Change the MTU size of NUT's NAT to 1250
    3. Open a channel from NUT to peer and hence triggering Peer to peer connection
    4. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_03(void) {
	char *invite_peer, *invite_nut;

	mtu_calc_peer = false;
	mtu_calc_relay = false;
	memset(&node_pmtu_peer, 0, sizeof(node_pmtu_peer));
	memset(&node_pmtu_relay, 0, sizeof(node_pmtu_relay));

	change_container_mtu(NUT_NAT, "eth0", 1250);

	invite_peer = invite_in_container("relay", "peer");
	assert(invite_peer);
	invite_nut = invite_in_container("relay", NUT_NODE_NAME);
	assert(invite_nut);
	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	bool ret = wait_for_event(event_mtu_handle_cb, PING_TRACK_TIMEOUT + 100);
	assert_int_equal(ret && mtu_calc_peer && mtu_calc_relay, true);

	assert_in_range(node_pmtu_relay.mtu_size, 1200, 1250);
	assert_in_range(node_pmtu_peer.mtu_size, 1200, 1250);
	assert_in_range(node_pmtu_peer.mtu_discovery.probes, 38, 42);
	assert_in_range(node_pmtu_relay.mtu_size, 1450, 1501);
	assert_in_range(node_pmtu_relay.mtu_discovery.probes, 38, 42);
	return true;
}
/* Test Steps for optimal PMTU discovery Test Case # 4 -
    Validating NUT MTU parameters with MTU size of NAT = 1000 under designed
    network topology */
static void test_case_optimal_pmtu_04(void **state) {
	execute_test(test_steps_optimal_pmtu_04, state);
}

/* Test Steps for optimal PMTU discovery Test Case # 4 -

    Test Steps:
    1. Create NAT setup and run each node instances in discrete containers,
    2. Change the MTU size of NUT's NAT to 1000
    3. Open a channel from NUT to peer and hence triggering Peer to peer connection
    4. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_04(void) {
	char *invite_peer, *invite_nut;

	mtu_calc_peer = false;
	mtu_calc_relay = false;
	memset(&node_pmtu_peer, 0, sizeof(node_pmtu_peer));
	memset(&node_pmtu_relay, 0, sizeof(node_pmtu_relay));

	change_container_mtu(NUT_NAT, "eth0", 1000);

	invite_peer = invite_in_container("relay", "peer");
	assert(invite_peer);
	invite_nut = invite_in_container("relay", NUT_NODE_NAME);
	assert(invite_nut);
	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	bool ret = wait_for_event(event_mtu_handle_cb, PING_TRACK_TIMEOUT + 100);
	assert_int_equal(ret && mtu_calc_peer && mtu_calc_relay, true);

	assert_in_range(node_pmtu_relay.mtu_size, 925, 1000);
	assert_in_range(node_pmtu_peer.mtu_size, 925, 1000);
	assert_in_range(node_pmtu_peer.mtu_discovery.probes, 38, 42);
	assert_in_range(node_pmtu_relay.mtu_size, 1450, 1501);
	assert_in_range(node_pmtu_relay.mtu_discovery.probes, 38, 42);
	return true;
}
/* Test Steps for optimal PMTU discovery Test Case # 5 -
    Validating NUT MTU parameters with MTU size of NAT = 800 under designed
    network topology */
static void test_case_optimal_pmtu_05(void **state) {
	execute_test(test_steps_optimal_pmtu_05, state);
}

/* Test Steps for optimal PMTU discovery Test Case # 5 -

    Test Steps:
    1. Create NAT setup and run each node instances in discrete containers,
    2. Change the MTU size of NUT's NAT to 800
    3. Open a channel from NUT to peer and hence triggering Peer to peer connection
    4. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_05(void) {
	char *invite_peer, *invite_nut;

	mtu_calc_peer = false;
	mtu_calc_relay = false;
	memset(&node_pmtu_peer, 0, sizeof(node_pmtu_peer));
	memset(&node_pmtu_relay, 0, sizeof(node_pmtu_relay));

	change_container_mtu(NUT_NAT, "eth0", 800);

	invite_peer = invite_in_container("relay", "peer");
	assert(invite_peer);
	invite_nut = invite_in_container("relay", NUT_NODE_NAME);
	assert(invite_nut);
	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	bool ret = wait_for_event(event_mtu_handle_cb, PING_TRACK_TIMEOUT + 100);
	assert_int_equal(ret && mtu_calc_peer && mtu_calc_relay, true);

	assert_in_range(node_pmtu_relay.mtu_size, 750, 800);
	assert_in_range(node_pmtu_peer.mtu_size, 750, 800);
	//assert_in_range(node_pmtu_peer.mtu_discovery.probes, 38, 42);
	//assert_in_range(node_pmtu_relay.mtu_size, 1450, 1501);
	//assert_in_range(node_pmtu_relay.mtu_discovery.probes, 38, 42);
	return true;
}

/* Test Steps for optimal PMTU discovery Test Case # 6 -
    Flushing the tracked connections via NUT NAT for every 60 seconds */
static void test_case_optimal_pmtu_06(void **state) {
	execute_test(test_steps_optimal_pmtu_06, state);
}

static pthread_t pmtu_test_case_conntrack_thread;
static void *conntrcak_flush(void *timeout) {
	int thread_timeout = *((int *)timeout);
	time_t thread_end_time = time(NULL) + thread_timeout;

	while(thread_end_time > time(NULL)) {
		sleep(90);
		flush_conntrack(NUT_NAT);
		flush_conntrack(PEER_NAT);
	}

}

static int teardown_conntrack_test_case(void **state) {

	// Close the conntrack thread
	pthread_cancel(pmtu_test_case_conntrack_thread);
	// Close all the test cases
	return teardown_pmtu_test_case(state);
}

/* Test Steps for optimal PMTU discovery Test Case # 6 -

    Test Steps:
    1. Create NAT setup and Launch conntrack thread which flushes the tracked connections for every 90 seconds
    2. Run each node instances in discrete containers,
    3. Open a channel from NUT to peer and hence triggering Peer to peer connection
    4. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_06(void) {
	char *invite_peer, *invite_nut;
	float ping_avg;

	mtu_calc_peer = false;
	mtu_calc_relay = false;
	memset(&node_pmtu_peer, 0, sizeof(node_pmtu_peer));
	memset(&node_pmtu_relay, 0, sizeof(node_pmtu_relay));

	int thread_timeout = PING_TRACK_TIMEOUT + 30;
	assert(!pthread_create(&pmtu_test_case_conntrack_thread, NULL, conntrcak_flush, &thread_timeout));
	invite_peer = invite_in_container("relay", "peer");
	assert(invite_peer);
	invite_nut = invite_in_container("relay", NUT_NODE_NAME);
	assert(invite_nut);
	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	bool ret = wait_for_event(event_mtu_handle_cb, PING_TRACK_TIMEOUT + 100);
	assert_int_equal(ret && mtu_calc_peer && mtu_calc_relay, true);

	assert_in_range(node_pmtu_relay.mtu_size, 1440, 1500);
	assert_in_range(node_pmtu_peer.mtu_size, 1440, 1500);
	assert_in_range(node_pmtu_peer.mtu_ping.probes, 38, 42);
	assert_in_range(node_pmtu_relay.mtu_ping.probes, 38, 42);

	ping_avg = 0;

	if(node_pmtu_peer.mtu_ping.probes) {
		ping_avg = ping_avg = node_pmtu_peer.mtu_ping.time / node_pmtu_peer.mtu_ping.probes;
	}

	assert_in_range(ping_avg, 15.0, 20.0);

	ping_avg = 0;

	if(node_pmtu_relay.mtu_ping.probes) {
		ping_avg = ping_avg = node_pmtu_relay.mtu_ping.time / node_pmtu_relay.mtu_ping.probes;
	}

	assert_in_range(ping_avg, 15.0, 20.0);
	return true;
}

/* Test Steps for optimal PMTU discovery Test Case # 7 -
    NUT sending data to peer node via channel for every 30 seconds
    */
static void test_case_optimal_pmtu_07(void **state) {
	execute_test(test_steps_optimal_pmtu_07, state);
}

/* Test Steps for optimal PMTU discovery Test Case # 7 -

    Test Steps:
    1. Create NAT setup and run each node instances in discrete containers.
    2. Open a channel from NUT to peer and hence triggering Peer to peer connection
    3. Send data periodically via channel from NUT to peer node.
    4. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_07(void) {
	char *invite_peer, *invite_nut;

	mtu_calc_peer = false;
	mtu_calc_relay = false;
	memset(&node_pmtu_peer, 0, sizeof(node_pmtu_peer));
	memset(&node_pmtu_relay, 0, sizeof(node_pmtu_relay));
	invite_peer = invite_in_container("relay", "peer");
	assert(invite_peer);
	invite_nut = invite_in_container("relay", NUT_NODE_NAME);
	assert(invite_nut);
	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

	bool ret = wait_for_event(event_mtu_handle_cb, PING_TRACK_TIMEOUT + 100);
	assert_int_equal(ret && mtu_calc_peer && mtu_calc_relay, true);

	assert_in_range(node_pmtu_peer.mtu_size, 1450, 1501);
	assert_in_range(node_pmtu_peer.mtu_discovery.probes, 38, 42);
	assert_in_range(node_pmtu_relay.mtu_size, 1450, 1501);
	assert_in_range(node_pmtu_relay.mtu_discovery.probes, 38, 42);

	return true;
}

// Optimal PMTU test case driver
int test_optimal_pmtu(void) {
	const struct CMUnitTest blackbox_group0_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_01, setup_pmtu_test_case, teardown_pmtu_test_case,
		                (void *)&test_pmtu_state_01),
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_02, setup_pmtu_test_case, teardown_pmtu_test_case,
		                (void *)&test_pmtu_state_02),
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_03, setup_pmtu_test_case, teardown_pmtu_test_case,
		                (void *)&test_pmtu_state_03),
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_04, setup_pmtu_test_case, teardown_pmtu_test_case,
		                (void *)&test_pmtu_state_04),
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_05, setup_pmtu_test_case, teardown_pmtu_test_case,
		                (void *)&test_pmtu_state_05),
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_06, setup_pmtu_test_case, teardown_pmtu_test_case,
		                (void *)&test_pmtu_state_06),
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_07, setup_pmtu_test_case, teardown_pmtu_test_case,
		                (void *)&test_pmtu_state_07),
	};
	total_tests += sizeof(blackbox_group0_tests) / sizeof(blackbox_group0_tests[0]);

	return cmocka_run_group_tests(blackbox_group0_tests, black_box_pmtu_group_setup, NULL);
}
