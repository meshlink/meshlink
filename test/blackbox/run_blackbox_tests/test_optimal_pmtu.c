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
#include <pthread.h>
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../common/network_namespace_framework.h"
#include "../../utils.h"
#include "../test_case_optimal_pmtu_01/test_case_optimal_pmtu.h"
#include "test_optimal_pmtu.h"

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

extern void *node_sim_relay_01(void *arg);
extern void *node_sim_peer_01(void *arg);
extern void *node_sim_nut_01(void *arg);
extern pmtu_attr_t node_pmtu[2];

typedef bool (*test_step_func_t)(void);
static int setup_test(void **state);
bool test_pmtu_relay_running = true;
bool test_pmtu_peer_running = true;
bool test_pmtu_nut_running = true;
bool ping_channel_enable_07 = false;

struct sync_flag test_pmtu_nut_closed = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static netns_state_t *test_pmtu_state;

static int setup_test(void **state) {
	(void)state;

	netns_create_topology(test_pmtu_state);
	fprintf(stderr, "\nCreated topology\n");

	test_pmtu_relay_running = true;
	test_pmtu_peer_running = true;
	test_pmtu_nut_running = true;
	ping_channel_enable_07 = false;
	memset(node_pmtu, 0, sizeof(node_pmtu));
	set_sync_flag(&test_pmtu_nut_closed, false);
	meshlink_destroy("nut");
	meshlink_destroy("peer");
	meshlink_destroy("relay");

	return EXIT_SUCCESS;
}

static int teardown_test(void **state) {
	(void)state;

	meshlink_destroy("nut");
	meshlink_destroy("peer");
	meshlink_destroy("relay");
	netns_destroy_topology(test_pmtu_state);

	return EXIT_SUCCESS;
}

static void execute_test(test_step_func_t step_func, void **state) {
	(void)state;


	fprintf(stderr, "\n\x1b[32mRunning Test\x1b[0m\n");
	bool test_result = step_func();

	if(!test_result) {
		fail();
	}
}

static void *gen_inv(void *arg) {
	mesh_invite_arg_t *mesh_invite_arg = (mesh_invite_arg_t *)arg;
	meshlink_handle_t *mesh;
	mesh = meshlink_open(mesh_invite_arg->mesh_arg->node_name, mesh_invite_arg->mesh_arg->confbase, mesh_invite_arg->mesh_arg->app_name, mesh_invite_arg->mesh_arg->dev_class);
	assert(mesh);

	char *invitation = meshlink_invite(mesh, NULL, mesh_invite_arg->invitee_name);
	assert(invitation);
	mesh_invite_arg->invite_str = invitation;
	meshlink_close(mesh);

	return NULL;
}

/* Test Steps for optimal PMTU discovery Test Case # 1 -
    Validating NUT MTU parameters without blocking ICMP under designed
    network topology */
static void test_case_optimal_pmtu_01(void **state) {
	execute_test(test_steps_optimal_pmtu_01, state);
	return;
}

/* Test Steps for optimal PMTU discovery Test Case # 1 - Success case

    Test Steps:
    1. Create NAT setup and run each node instances in discrete namespace.
    2. Open a channel from NUT to peer and hence triggering Peer to peer connection
    3. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_01(void) {
	mesh_arg_t relay_arg = {.node_name = "relay", .confbase = "relay", .app_name = "relay", .dev_class = 0 };
	mesh_arg_t peer_arg = {.node_name = "peer", .confbase = "peer", .app_name = "peer", .dev_class = 1 };
	mesh_arg_t nut_arg = {.node_name = "nut", .confbase = "nut", .app_name = "nut", .dev_class = 1 };


	mesh_invite_arg_t relay_nut_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "nut" };
	netns_thread_t netns_relay_nut_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_nut_invite_arg};
	run_node_in_namespace_thread(&netns_relay_nut_invite);
	sleep(1);
	assert(relay_nut_invite_arg.invite_str);
	nut_arg.join_invitation = relay_nut_invite_arg.invite_str;

	mesh_invite_arg_t relay_peer_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "peer" };
	netns_thread_t netns_relay_peer_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_peer_invite_arg};
	run_node_in_namespace_thread(&netns_relay_peer_invite);
	sleep(1);
	assert(relay_peer_invite_arg.invite_str);
	peer_arg.join_invitation = relay_peer_invite_arg.invite_str;

	netns_thread_t netns_relay_handle = {.namespace_name = "relay", .netns_thread = node_sim_pmtu_relay_01, .arg = &relay_arg};
	run_node_in_namespace_thread(&netns_relay_handle);

	netns_thread_t netns_peer_handle = {.namespace_name = "peer", .netns_thread = node_sim_pmtu_peer_01, .arg = &peer_arg};
	run_node_in_namespace_thread(&netns_peer_handle);

	netns_thread_t netns_nut_handle = {.namespace_name = "nut", .netns_thread = node_sim_pmtu_nut_01, .arg = &nut_arg};
	run_node_in_namespace_thread(&netns_nut_handle);

	assert(wait_sync_flag(&test_pmtu_nut_closed, 300));
	test_pmtu_relay_running = false;
	test_pmtu_peer_running = false;

	sleep(1);
	assert_in_range(node_pmtu[NODE_PMTU_PEER].mtu_size, 1450, 1501);
	assert_in_range(node_pmtu[NODE_PMTU_PEER].mtu_discovery.probes, 120, 160);
	assert_in_range(node_pmtu[NODE_PMTU_RELAY].mtu_size, 1450, 1501);
	assert_in_range(node_pmtu[NODE_PMTU_RELAY].mtu_discovery.probes, 120, 160);

	return true;
}

/* Test Steps for optimal PMTU discovery Test Case # 2 -
    Validating NUT MTU parameters blocking ICMP under designed
    network topology */
static void test_case_optimal_pmtu_02(void **state) {
	execute_test(test_steps_optimal_pmtu_02, state);
	return;
}

/* Test Steps for optimal PMTU discovery Test Case # 2 -
    Test Steps:
    1. Create NAT setup and run each node instances in discrete namespace,
    2. Block ICMP protocol at NUT's NAT
    3. Open a channel from NUT to peer and hence triggering Peer to peer connection
    4. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_02(void) {
	mesh_arg_t relay_arg = {.node_name = "relay", .confbase = "relay", .app_name = "relay", .dev_class = 0 };
	mesh_arg_t peer_arg = {.node_name = "peer", .confbase = "peer", .app_name = "peer", .dev_class = 1 };
	mesh_arg_t nut_arg = {.node_name = "nut", .confbase = "nut", .app_name = "nut", .dev_class = 1 };

	assert(system("ip netns exec peer_nat iptables -A FORWARD -p icmp -j DROP") == 0);
	assert(system("ip netns exec nut_nat iptables -A FORWARD -p icmp -j DROP") == 0);

	mesh_invite_arg_t relay_nut_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "nut" };
	netns_thread_t netns_relay_nut_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_nut_invite_arg};
	run_node_in_namespace_thread(&netns_relay_nut_invite);
	sleep(1);
	assert(relay_nut_invite_arg.invite_str);
	nut_arg.join_invitation = relay_nut_invite_arg.invite_str;

	mesh_invite_arg_t relay_peer_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "peer" };
	netns_thread_t netns_relay_peer_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_peer_invite_arg};
	run_node_in_namespace_thread(&netns_relay_peer_invite);
	sleep(1);
	assert(relay_peer_invite_arg.invite_str);
	peer_arg.join_invitation = relay_peer_invite_arg.invite_str;

	netns_thread_t netns_relay_handle = {.namespace_name = "relay", .netns_thread = node_sim_pmtu_relay_01, .arg = &relay_arg};
	run_node_in_namespace_thread(&netns_relay_handle);

	netns_thread_t netns_peer_handle = {.namespace_name = "peer", .netns_thread = node_sim_pmtu_peer_01, .arg = &peer_arg};
	run_node_in_namespace_thread(&netns_peer_handle);

	netns_thread_t netns_nut_handle = {.namespace_name = "nut", .netns_thread = node_sim_pmtu_nut_01, .arg = &nut_arg};
	run_node_in_namespace_thread(&netns_nut_handle);

	assert(wait_sync_flag(&test_pmtu_nut_closed, 300));
	test_pmtu_relay_running = false;
	test_pmtu_peer_running = false;

	sleep(1);
	assert_in_range(node_pmtu[NODE_PMTU_PEER].mtu_size, 1450, 1501);
	assert_in_range(node_pmtu[NODE_PMTU_PEER].mtu_discovery.probes, 120, 160);
	assert_in_range(node_pmtu[NODE_PMTU_RELAY].mtu_size, 1450, 1501);
	assert_in_range(node_pmtu[NODE_PMTU_RELAY].mtu_discovery.probes, 120, 160);

	return true;
}

/* Test Steps for optimal PMTU discovery Test Case # 3 -
    Validating NUT MTU parameters with MTU size of NAT = 1250 under designed
    network topology */
static void test_case_optimal_pmtu_03(void **state) {
	execute_test(test_steps_optimal_pmtu_03, state);
	return;
}

/* Test Steps for optimal PMTU discovery Test Case # 3 -
    Test Steps:
    1. Create NAT setup and run each node instances in discrete namespace,
    2. Change the MTU size of NUT's NAT to 1250
    3. Open a channel from NUT to peer and hence triggering Peer to peer connection
    4. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_03(void) {
	mesh_arg_t relay_arg = {.node_name = "relay", .confbase = "relay", .app_name = "relay", .dev_class = 0 };
	mesh_arg_t peer_arg = {.node_name = "peer", .confbase = "peer", .app_name = "peer", .dev_class = 1 };
	mesh_arg_t nut_arg = {.node_name = "nut", .confbase = "nut", .app_name = "nut", .dev_class = 1 };

	assert(system("ip netns exec nut_nat ifconfig eth_nut mtu 1250") == 0);

	mesh_invite_arg_t relay_nut_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "nut" };
	netns_thread_t netns_relay_nut_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_nut_invite_arg};
	run_node_in_namespace_thread(&netns_relay_nut_invite);
	sleep(1);
	assert(relay_nut_invite_arg.invite_str);
	nut_arg.join_invitation = relay_nut_invite_arg.invite_str;

	mesh_invite_arg_t relay_peer_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "peer" };
	netns_thread_t netns_relay_peer_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_peer_invite_arg};
	run_node_in_namespace_thread(&netns_relay_peer_invite);
	sleep(1);
	assert(relay_peer_invite_arg.invite_str);
	peer_arg.join_invitation = relay_peer_invite_arg.invite_str;

	netns_thread_t netns_relay_handle = {.namespace_name = "relay", .netns_thread = node_sim_pmtu_relay_01, .arg = &relay_arg};
	run_node_in_namespace_thread(&netns_relay_handle);

	netns_thread_t netns_peer_handle = {.namespace_name = "peer", .netns_thread = node_sim_pmtu_peer_01, .arg = &peer_arg};
	run_node_in_namespace_thread(&netns_peer_handle);

	netns_thread_t netns_nut_handle = {.namespace_name = "nut", .netns_thread = node_sim_pmtu_nut_01, .arg = &nut_arg};
	run_node_in_namespace_thread(&netns_nut_handle);

	assert(wait_sync_flag(&test_pmtu_nut_closed, 300));
	test_pmtu_relay_running = false;
	test_pmtu_peer_running = false;

	sleep(1);
	assert_in_range(node_pmtu[NODE_PMTU_PEER].mtu_size, 1200, 1250);
	assert_in_range(node_pmtu[NODE_PMTU_RELAY].mtu_size, 1200, 1250);

	return true;
}

/* Test Steps for optimal PMTU discovery Test Case # 4 -
    Validating NUT MTU parameters with MTU size of NAT = 1000 under designed
    network topology */
static void test_case_optimal_pmtu_04(void **state) {
	execute_test(test_steps_optimal_pmtu_04, state);
	return;
}

/* Test Steps for optimal PMTU discovery Test Case # 4 -
    Test Steps:
    1. Create NAT setup and run each node instances in discrete namespace,
    2. Change the MTU size of NUT's NAT to 1000
    3. Open a channel from NUT to peer and hence triggering Peer to peer connection
    4. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_04(void) {
	mesh_arg_t relay_arg = {.node_name = "relay", .confbase = "relay", .app_name = "relay", .dev_class = 0 };
	mesh_arg_t peer_arg = {.node_name = "peer", .confbase = "peer", .app_name = "peer", .dev_class = 1 };
	mesh_arg_t nut_arg = {.node_name = "nut", .confbase = "nut", .app_name = "nut", .dev_class = 1 };

	assert(system("ip netns exec nut_nat ifconfig eth_nut mtu 1000") == 0);

	mesh_invite_arg_t relay_nut_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "nut" };
	netns_thread_t netns_relay_nut_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_nut_invite_arg};
	run_node_in_namespace_thread(&netns_relay_nut_invite);
	sleep(1);
	assert(relay_nut_invite_arg.invite_str);
	nut_arg.join_invitation = relay_nut_invite_arg.invite_str;

	mesh_invite_arg_t relay_peer_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "peer" };
	netns_thread_t netns_relay_peer_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_peer_invite_arg};
	run_node_in_namespace_thread(&netns_relay_peer_invite);
	sleep(1);
	assert(relay_peer_invite_arg.invite_str);
	peer_arg.join_invitation = relay_peer_invite_arg.invite_str;

	netns_thread_t netns_relay_handle = {.namespace_name = "relay", .netns_thread = node_sim_pmtu_relay_01, .arg = &relay_arg};
	run_node_in_namespace_thread(&netns_relay_handle);

	netns_thread_t netns_peer_handle = {.namespace_name = "peer", .netns_thread = node_sim_pmtu_peer_01, .arg = &peer_arg};
	run_node_in_namespace_thread(&netns_peer_handle);

	netns_thread_t netns_nut_handle = {.namespace_name = "nut", .netns_thread = node_sim_pmtu_nut_01, .arg = &nut_arg};
	run_node_in_namespace_thread(&netns_nut_handle);

	assert(wait_sync_flag(&test_pmtu_nut_closed, 300));
	test_pmtu_relay_running = false;
	test_pmtu_peer_running = false;

	sleep(1);
	assert_in_range(node_pmtu[NODE_PMTU_PEER].mtu_size, 925, 1000);
	assert_in_range(node_pmtu[NODE_PMTU_RELAY].mtu_size, 925, 1000);

	return true;
}

/* Test Steps for optimal PMTU discovery Test Case # 5 -
    Validating NUT MTU parameters with MTU size of NAT = 800 under designed
    network topology */
static void test_case_optimal_pmtu_05(void **state) {
	execute_test(test_steps_optimal_pmtu_05, state);
	return;
}

/* Test Steps for optimal PMTU discovery Test Case # 5 -
    Test Steps:
    1. Create NAT setup and run each node instances in discrete namespace,
    2. Change the MTU size of NUT's NAT to 800
    3. Open a channel from NUT to peer and hence triggering Peer to peer connection
    4. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_05(void) {
	mesh_arg_t relay_arg = {.node_name = "relay", .confbase = "relay", .app_name = "relay", .dev_class = 0 };
	mesh_arg_t peer_arg = {.node_name = "peer", .confbase = "peer", .app_name = "peer", .dev_class = 1 };
	mesh_arg_t nut_arg = {.node_name = "nut", .confbase = "nut", .app_name = "nut", .dev_class = 1 };

	assert(system("ip netns exec nut_nat ifconfig eth_nut mtu 750") == 0);

	mesh_invite_arg_t relay_nut_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "nut" };
	netns_thread_t netns_relay_nut_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_nut_invite_arg};
	run_node_in_namespace_thread(&netns_relay_nut_invite);
	sleep(1);
	assert(relay_nut_invite_arg.invite_str);
	nut_arg.join_invitation = relay_nut_invite_arg.invite_str;

	mesh_invite_arg_t relay_peer_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "peer" };
	netns_thread_t netns_relay_peer_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_peer_invite_arg};
	run_node_in_namespace_thread(&netns_relay_peer_invite);
	sleep(1);
	assert(relay_peer_invite_arg.invite_str);
	peer_arg.join_invitation = relay_peer_invite_arg.invite_str;

	netns_thread_t netns_relay_handle = {.namespace_name = "relay", .netns_thread = node_sim_pmtu_relay_01, .arg = &relay_arg};
	run_node_in_namespace_thread(&netns_relay_handle);

	netns_thread_t netns_peer_handle = {.namespace_name = "peer", .netns_thread = node_sim_pmtu_peer_01, .arg = &peer_arg};
	run_node_in_namespace_thread(&netns_peer_handle);

	netns_thread_t netns_nut_handle = {.namespace_name = "nut", .netns_thread = node_sim_pmtu_nut_01, .arg = &nut_arg};
	run_node_in_namespace_thread(&netns_nut_handle);

	assert(wait_sync_flag(&test_pmtu_nut_closed, 300));
	test_pmtu_relay_running = false;
	test_pmtu_peer_running = false;

	sleep(1);
	assert_in_range(node_pmtu[NODE_PMTU_PEER].mtu_size, 700, 750);
	assert_in_range(node_pmtu[NODE_PMTU_RELAY].mtu_size, 700, 750);

	return true;
}

/* Test Steps for optimal PMTU discovery Test Case # 6 -
    Flushing the tracked connections via NUT NAT for every 60 seconds */
static void test_case_optimal_pmtu_06(void **state) {
	execute_test(test_steps_optimal_pmtu_06, state);
	return;
}

static bool run_conntrack;
static pthread_t pmtu_test_case_conntrack_thread;
static void *conntrack_flush(void *arg) {
	(void)arg;

	// flushes mappings for every 60 seconds

	while(run_conntrack) {
		sleep(100);
		assert(system("ip netns exec nut_nat conntrack -F") == 0);
		assert(system("ip netns exec peer_nat conntrack -F") == 0);
	}

	pthread_exit(NULL);
}

/* Test Steps for optimal PMTU discovery Test Case # 6 -
    Test Steps:
    1. Create NAT setup and Launch conntrack thread which flushes the tracked connections for every 90 seconds
    2. Run each node instances in discrete namespace,
    3. Open a channel from NUT to peer and hence triggering Peer to peer connection
    4. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_06(void) {
	mesh_arg_t relay_arg = {.node_name = "relay", .confbase = "relay", .app_name = "relay", .dev_class = 0 };
	mesh_arg_t peer_arg = {.node_name = "peer", .confbase = "peer", .app_name = "peer", .dev_class = 1 };
	mesh_arg_t nut_arg = {.node_name = "nut", .confbase = "nut", .app_name = "nut", .dev_class = 1 };

	run_conntrack = true;
	assert(!pthread_create(&pmtu_test_case_conntrack_thread, NULL, conntrack_flush, NULL));

	mesh_invite_arg_t relay_nut_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "nut" };
	netns_thread_t netns_relay_nut_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_nut_invite_arg};
	run_node_in_namespace_thread(&netns_relay_nut_invite);
	sleep(1);
	assert(relay_nut_invite_arg.invite_str);
	nut_arg.join_invitation = relay_nut_invite_arg.invite_str;

	mesh_invite_arg_t relay_peer_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "peer" };
	netns_thread_t netns_relay_peer_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_peer_invite_arg};
	run_node_in_namespace_thread(&netns_relay_peer_invite);
	sleep(1);
	assert(relay_peer_invite_arg.invite_str);
	peer_arg.join_invitation = relay_peer_invite_arg.invite_str;

	netns_thread_t netns_relay_handle = {.namespace_name = "relay", .netns_thread = node_sim_pmtu_relay_01, .arg = &relay_arg};
	run_node_in_namespace_thread(&netns_relay_handle);

	netns_thread_t netns_peer_handle = {.namespace_name = "peer", .netns_thread = node_sim_pmtu_peer_01, .arg = &peer_arg};
	run_node_in_namespace_thread(&netns_peer_handle);

	netns_thread_t netns_nut_handle = {.namespace_name = "nut", .netns_thread = node_sim_pmtu_nut_01, .arg = &nut_arg};
	run_node_in_namespace_thread(&netns_nut_handle);

	assert(wait_sync_flag(&test_pmtu_nut_closed, 300));
	test_pmtu_relay_running = false;
	test_pmtu_peer_running = false;
	run_conntrack = false;
	pthread_join(pmtu_test_case_conntrack_thread, NULL);

	sleep(1);

	assert_in_range(node_pmtu[NODE_PMTU_PEER].mtu_size, 1440, 1500);
	assert_in_range(node_pmtu[NODE_PMTU_RELAY].mtu_size, 1440, 1500);
	assert_in_range(node_pmtu[NODE_PMTU_PEER].mtu_ping.probes, 38, 42);
	assert_in_range(node_pmtu[NODE_PMTU_RELAY].mtu_ping.probes, 38, 42);

	return true;
}

/* Test Steps for optimal PMTU discovery Test Case # 7 -
    NUT sending data to peer node via channel for every 30 seconds
    */
static void test_case_optimal_pmtu_07(void **state) {
	execute_test(test_steps_optimal_pmtu_07, state);
	return;
}

/* Test Steps for optimal PMTU discovery Test Case # 7 -
    Test Steps:
    1. Create NAT setup and run each node instances in discrete namespace.
    2. Open a channel from NUT to peer and hence triggering Peer to peer connection
    3. Send data periodically via channel from NUT to peer node.
    4. Send the analyzed MTU parameters mesh event information to test driver
    Expected Result:
      NUT and Peer should be able to hole puch the NATs and MTU parameters should be in
      the expected range
*/
static bool test_steps_optimal_pmtu_07(void) {
	mesh_arg_t relay_arg = {.node_name = "relay", .confbase = "relay", .app_name = "relay", .dev_class = 0 };
	mesh_arg_t peer_arg = {.node_name = "peer", .confbase = "peer", .app_name = "peer", .dev_class = 1 };
	mesh_arg_t nut_arg = {.node_name = "nut", .confbase = "nut", .app_name = "nut", .dev_class = 1 };

	ping_channel_enable_07 = true;

	mesh_invite_arg_t relay_nut_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "nut" };
	netns_thread_t netns_relay_nut_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_nut_invite_arg};
	run_node_in_namespace_thread(&netns_relay_nut_invite);
	sleep(1);
	assert(relay_nut_invite_arg.invite_str);
	nut_arg.join_invitation = relay_nut_invite_arg.invite_str;

	mesh_invite_arg_t relay_peer_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "peer" };
	netns_thread_t netns_relay_peer_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_peer_invite_arg};
	run_node_in_namespace_thread(&netns_relay_peer_invite);
	sleep(1);
	assert(relay_peer_invite_arg.invite_str);
	peer_arg.join_invitation = relay_peer_invite_arg.invite_str;

	netns_thread_t netns_relay_handle = {.namespace_name = "relay", .netns_thread = node_sim_pmtu_relay_01, .arg = &relay_arg};
	run_node_in_namespace_thread(&netns_relay_handle);

	netns_thread_t netns_peer_handle = {.namespace_name = "peer", .netns_thread = node_sim_pmtu_peer_01, .arg = &peer_arg};
	run_node_in_namespace_thread(&netns_peer_handle);

	netns_thread_t netns_nut_handle = {.namespace_name = "nut", .netns_thread = node_sim_pmtu_nut_01, .arg = &nut_arg};
	run_node_in_namespace_thread(&netns_nut_handle);

	assert(wait_sync_flag(&test_pmtu_nut_closed, 300));
	test_pmtu_relay_running = false;
	test_pmtu_peer_running = false;

	sleep(1);
	assert_in_range(node_pmtu[NODE_PMTU_PEER].mtu_size, 1450, 1501);
	assert_in_range(node_pmtu[NODE_PMTU_PEER].mtu_discovery.probes, 120, 160);
	assert_in_range(node_pmtu[NODE_PMTU_RELAY].mtu_size, 1450, 1501);
	assert_in_range(node_pmtu[NODE_PMTU_RELAY].mtu_discovery.probes, 120, 160);

	return true;
}

// Optimal PMTU test case driver

int test_optimal_pmtu(void) {
	interface_t nut_ifs[] = { { .if_peer = "nut_nat", .fetch_ip_netns_name = "nut_nat" } };
	namespace_t nut = {
		.name = "nut",
		.type = HOST,
		.interfaces = nut_ifs,
		.interfaces_no = 1,
	};

	interface_t peer_ifs[] = { { .if_peer = "peer_nat", .fetch_ip_netns_name = "peer_nat" } };
	namespace_t peer = {
		.name = "peer",
		.type = HOST,
		.interfaces = peer_ifs,
		.interfaces_no = 1,
	};

	interface_t relay_ifs[] = { { .if_peer = "wan_bridge" } };
	namespace_t relay = {
		.name = "relay",
		.type = HOST,
		.interfaces = relay_ifs,
		.interfaces_no = 1,
	};

	netns_fullcone_handle_t nut_nat_fullcone = { .snat_to_source = "wan_bridge", .dnat_to_destination = "nut" };
	netns_fullcone_handle_t *nut_nat_args[] = { &nut_nat_fullcone, NULL };
	interface_t nut_nat_ifs[] = { { .if_peer = "nut", .fetch_ip_netns_name = "nut_nat" }, { .if_peer = "wan_bridge" } };
	namespace_t nut_nat = {
		.name = "nut_nat",
		.type = FULL_CONE,
		.nat_arg = nut_nat_args,
		.static_config_net_addr = "192.168.1.0/24",
		.interfaces = nut_nat_ifs,
		.interfaces_no = 2,
	};

	netns_fullcone_handle_t peer_nat_fullcone = { .snat_to_source = "wan_bridge", .dnat_to_destination = "peer" };
	netns_fullcone_handle_t *peer_nat_args[] = { &peer_nat_fullcone, NULL };
	interface_t peer_nat_ifs[] = { { .if_peer = "peer", .fetch_ip_netns_name = "peer_nat" }, { .if_peer = "wan_bridge" } };
	namespace_t peer_nat = {
		.name = "peer_nat",
		.type = FULL_CONE,
		.nat_arg = peer_nat_args,
		.static_config_net_addr = "192.168.1.0/24",
		.interfaces = peer_nat_ifs,
		.interfaces_no = 2,
	};

	interface_t wan_ifs[] = { { .if_peer = "peer_nat" }, { .if_peer = "nut_nat" }, { .if_peer = "relay" } };
	namespace_t wan_bridge = {
		.name = "wan_bridge",
		.type = BRIDGE,
		.interfaces = wan_ifs,
		.interfaces_no = 3,
	};

	namespace_t test_optimal_pmtu_1_nodes[] = { nut_nat, peer_nat, wan_bridge, nut, peer, relay };

	netns_state_t test_pmtu_nodes = {
		.test_case_name =  "test_case_optimal_pmtu",
		.namespaces =  test_optimal_pmtu_1_nodes,
		.num_namespaces = 6,
	};
	test_pmtu_state = &test_pmtu_nodes;

	const struct CMUnitTest blackbox_group0_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_01, setup_test, teardown_test,
		                (void *)&test_pmtu_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_02, setup_test, teardown_test,
		                (void *)&test_pmtu_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_03, setup_test, teardown_test,
		                (void *)&test_pmtu_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_04, setup_test, teardown_test,
		                (void *)&test_pmtu_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_05, setup_test, teardown_test,
		                (void *)&test_pmtu_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_06, setup_test, teardown_test,
		                (void *)&test_pmtu_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_optimal_pmtu_07, setup_test, teardown_test,
		                (void *)&test_pmtu_state),
	};
	total_tests += sizeof(blackbox_group0_tests) / sizeof(blackbox_group0_tests[0]);

	return cmocka_run_group_tests(blackbox_group0_tests, NULL, NULL);
}
