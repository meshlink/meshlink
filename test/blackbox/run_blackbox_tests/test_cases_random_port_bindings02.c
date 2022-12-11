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

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <pthread.h>
#include "meshlink.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../common/network_namespace_framework.h"
#include "../../utils.h"
#include "test_cases_random_port_bindings02.h"

static void test_case_mesh_random_port_bindings_04(void **state);
static bool test_steps_mesh_random_port_bindings_04(void);
static void test_case_mesh_random_port_bindings_05(void **state);
static bool test_steps_mesh_random_port_bindings_05(void);

typedef bool (*test_step_func_t)(void);
static int setup_test(void **state);

static meshlink_handle_t *peer, *nut_instance, *relay;
static char *peer_invite, *nut_invite;
struct sync_flag test_random_port_binding_node_connected = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
struct sync_flag test_random_port_binding_node_started = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
struct sync_flag test_random_port_binding_peer_reachable = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
struct sync_flag test_random_port_binding_make_switch = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
struct sync_flag test_random_port_binding_relay_closed = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
struct sync_flag test_random_port_binding_peer_closed = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
struct sync_flag test_random_port_binding_nut_closed = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static netns_state_t *test_random_port_bindings_state;
static bool localnode = false;

static int setup_test(void **state) {
	(void)state;

	netns_create_topology(test_random_port_bindings_state);
	fprintf(stderr, "\nCreated topology\n");

	set_sync_flag(&test_random_port_binding_node_connected, false);
	set_sync_flag(&test_random_port_binding_node_started, false);
	set_sync_flag(&test_random_port_binding_peer_reachable, false);
	set_sync_flag(&test_random_port_binding_make_switch, false);
	set_sync_flag(&test_random_port_binding_relay_closed, false);
	set_sync_flag(&test_random_port_binding_peer_closed, false);
	set_sync_flag(&test_random_port_binding_nut_closed, false);

	assert(meshlink_destroy("nut"));
	assert(meshlink_destroy("peer"));
	assert(meshlink_destroy("relay"));

	return EXIT_SUCCESS;
}

static int teardown_test(void **state) {
	(void)state;

	assert(meshlink_destroy("nut"));
	assert(meshlink_destroy("peer"));
	assert(meshlink_destroy("relay"));
	netns_destroy_topology(test_random_port_bindings_state);

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

static void message_log(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	(void)level;

	char *levelstr = "\x1b[32mRELAY";

	if(strcmp(mesh->name, "peer") == 0) {
		if(strcmp("Connection with nut activated", text) == 0) {
			set_sync_flag(&test_random_port_binding_node_connected, true);
		}

		levelstr = "\x1b[34mPEER";
	} else if(strcmp(mesh->name, "nut") == 0) {
		if(strcmp("Connection with peer activated", text) == 0) {
			set_sync_flag(&test_random_port_binding_node_connected, true);
		}

		levelstr = "\x1b[33mNUT";
	}

	fprintf(stderr, "%s:\x1b[0m %s\n", levelstr, text);
}

static void node_status(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(reachable) {
		if((strcmp(mesh->name, "nut") == 0) && (strcmp(node->name, "peer") == 0)) {
			set_sync_flag(&test_random_port_binding_peer_reachable, true);
		}

		fprintf(stderr, "%s: %s joined.\n", mesh->name, node->name);
	}
}

static void *relay_node(void *arg) {
	mesh_arg_t *mesh_arg = (mesh_arg_t *)arg;

	//system("ifconfig");

	assert(meshlink_destroy("relay"));

	relay = meshlink_open(mesh_arg->node_name, mesh_arg->confbase, mesh_arg->app_name, mesh_arg->dev_class);
	assert(relay);

	assert_true(meshlink_start(relay));
	fprintf(stderr, "\n\x1b[32mRelay Started\x1b[0m\n");

	assert((peer_invite = meshlink_invite(relay, NULL, "peer")));
	assert((nut_invite = meshlink_invite(relay, NULL, "nut")));

	set_sync_flag(&test_random_port_binding_node_started, true);

	meshlink_set_log_cb(relay, MESHLINK_DEBUG, message_log);

	if(localnode == true) {
		assert(wait_sync_flag(&test_random_port_binding_make_switch, 300));
		meshlink_close(relay);
		assert(meshlink_destroy("relay"));


		set_sync_flag(&test_random_port_binding_relay_closed, true);

		return NULL;
	}

	assert(wait_sync_flag(&test_random_port_binding_node_connected, 300));

	meshlink_close(relay);
	assert(meshlink_destroy("relay"));


	set_sync_flag(&test_random_port_binding_relay_closed, true);

	return NULL;
}

static void *peer_node(void *arg) {
	mesh_arg_t *mesh_arg = (mesh_arg_t *)arg;

	fprintf(stderr, "\n\x1b[32mPeer Thread Started\x1b[0m\n");

	assert(meshlink_destroy("peer"));

	peer = meshlink_open(mesh_arg->node_name, mesh_arg->confbase, mesh_arg->app_name, mesh_arg->dev_class);
	assert(peer);
	meshlink_set_log_cb(peer, MESHLINK_DEBUG, message_log);

	fprintf(stderr, "\n\x1b[32mPeer joining relay\x1b[0m\n");

	assert_true(meshlink_join(peer, (const char *)mesh_arg->join_invitation));

	assert_true(meshlink_start(peer));

	fprintf(stderr, "\n\x1b[32mPeer Started\x1b[0m\n");

	set_sync_flag(&test_random_port_binding_node_started, true);

	assert(wait_sync_flag(&test_random_port_binding_make_switch, 300));

	meshlink_stop(peer);

	//meshlink_set_log_cb(peer, MESHLINK_DEBUG, message_log);

	assert(meshlink_set_port(peer, 20000));

	assert_true(meshlink_start(peer));

	assert(wait_sync_flag(&test_random_port_binding_node_connected, 300));

	meshlink_close(peer);
	assert(meshlink_destroy("peer"));

	set_sync_flag(&test_random_port_binding_peer_closed, true);

	return NULL;
}

static void *nut_node(void *arg) {
	mesh_arg_t *mesh_arg = (mesh_arg_t *)arg;

	fprintf(stderr, "\n\x1b[32mNut Thread Started\x1b[0m\n");

	assert(meshlink_destroy("nut"));

	nut_instance = meshlink_open(mesh_arg->node_name, mesh_arg->confbase, mesh_arg->app_name, mesh_arg->dev_class);
	assert(nut_instance);

	meshlink_set_log_cb(nut_instance, MESHLINK_DEBUG, message_log);

	fprintf(stderr, "\n\x1b[32mNut joining relay\x1b[0m\n");

	assert_true(meshlink_join(nut_instance, (const char *)mesh_arg->join_invitation));

	meshlink_set_node_status_cb(nut_instance, node_status);

	assert_true(meshlink_start(nut_instance));

	fprintf(stderr, "\n\x1b[32mNut Started\x1b[0m\n");
	sleep(5);

	set_sync_flag(&test_random_port_binding_node_started, true);

	assert(wait_sync_flag(&test_random_port_binding_make_switch, 300));

	meshlink_stop(nut_instance);

	//meshlink_set_log_cb(nut_instance, MESHLINK_DEBUG, message_log);

	assert(meshlink_set_port(nut_instance, 30000));

	assert_true(meshlink_start(nut_instance));

	assert(wait_sync_flag(&test_random_port_binding_node_connected, 300));

	meshlink_close(nut_instance);
	assert(meshlink_destroy("nut"));

	set_sync_flag(&test_random_port_binding_nut_closed, true);

	return NULL;
}

/* Test Steps for Random port bindings Test Case # 4 */
static void test_case_mesh_random_port_bindings_04(void **state) {
	execute_test(test_steps_mesh_random_port_bindings_04, state);
	return;
}

/* Test Steps for Random port bindings Test Case # 4

    Test Steps:
    1. Create three node nut, peer and relay in three different name spaces.
    2. Join nut and peer to relay with invitation.
    3. Stop the three nodes and change the ports of nut and peer.
    4. Start all the nodes again.
    Expected Result:
      NUT and Peer should be able to discover each others port with the help
      of RELAY and form the direct meta connection.
*/
static bool test_steps_mesh_random_port_bindings_04(void) {
	mesh_arg_t relay_arg = {.node_name = "relay", .confbase = "relay", .app_name = "chat", .dev_class = 0 };
	mesh_arg_t peer_arg = {.node_name = "peer", .confbase = "peer", .app_name = "chat", .dev_class = 1 };
	mesh_arg_t nut_arg = {.node_name = "nut", .confbase = "nut", .app_name = "chat", .dev_class = 1 };

	netns_thread_t netns_relay_handle = {.namespace_name = "relay", .netns_thread = relay_node, .arg = &relay_arg};
	run_node_in_namespace_thread(&netns_relay_handle);

	assert(wait_sync_flag(&test_random_port_binding_node_started, 5));
	fprintf(stderr, "\n\x1b[32mTest-04 : Relay Started\x1b[0m\n");

	set_sync_flag(&test_random_port_binding_node_started, false);
	peer_arg.join_invitation = peer_invite;
	fprintf(stderr, "\n\x1b[32mTest-04: Got Invite {%s} for peer\x1b[0m\n", peer_arg.join_invitation);
	netns_thread_t netns_peer_handle = {.namespace_name = "peer", .netns_thread = peer_node, .arg = &peer_arg};
	run_node_in_namespace_thread(&netns_peer_handle);

	assert(wait_sync_flag(&test_random_port_binding_node_started, 20));
	fprintf(stderr, "\n\x1b[32mTest-04 : Peer Started\x1b[0m\n");

	set_sync_flag(&test_random_port_binding_node_started, false);
	nut_arg.join_invitation = nut_invite;
	fprintf(stderr, "\n\x1b[32mTest-04: Got Invite {%s} for nut\x1b[0m\n", nut_arg.join_invitation);
	netns_thread_t netns_nut_handle = {.namespace_name = "nut", .netns_thread = nut_node, .arg = &nut_arg};
	run_node_in_namespace_thread(&netns_nut_handle);

	assert(wait_sync_flag(&test_random_port_binding_node_started, 20));
	fprintf(stderr, "\n\x1b[32mTest-04 : Nut Started\x1b[0m\n");

	set_sync_flag(&test_random_port_binding_make_switch, true);
	fprintf(stderr, "\n\x1b[32mTest-04 : Making Switch\x1b[0m\n");

	assert(wait_sync_flag(&test_random_port_binding_node_connected, 300));

	fprintf(stderr, "\n\x1b[32mDone Test-04\x1b[0m\n");

	assert(wait_sync_flag(&test_random_port_binding_relay_closed, 10));
	assert(wait_sync_flag(&test_random_port_binding_peer_closed, 10));
	assert(wait_sync_flag(&test_random_port_binding_nut_closed, 10));

	return true;
}

/* Test Steps for Random port bindings Test Case # 5 */
static void test_case_mesh_random_port_bindings_05(void **state) {
	execute_test(test_steps_mesh_random_port_bindings_05, state);
	return;
}

/* Test Steps for Random port bindings Test Case # 5

    Test Steps:
    1. Create three node nut, peer and relay in same name spaces.
    2. Join nut and peer to relay with invitation.
    3. Stop the three nodes and change the ports of nut and peer.
    4. Close the relay node and start nut and peer nodes again.
    Expected Result:
      NUT and Peer should be able to discover each others port with the help
      of CATTA and form the direct meta connection.
*/
static bool test_steps_mesh_random_port_bindings_05(void) {
	localnode = true;

	mesh_arg_t relay_arg = {.node_name = "relay", .confbase = "relay", .app_name = "chat", .dev_class = 1 };
	mesh_arg_t peer_arg = {.node_name = "peer", .confbase = "peer", .app_name = "chat", .dev_class = 1 };
	mesh_arg_t nut_arg = {.node_name = "nut", .confbase = "nut", .app_name = "chat", .dev_class = 1 };

	netns_thread_t netns_relay_handle = {.namespace_name = "relay", .netns_thread = relay_node, .arg = &relay_arg};
	run_node_in_namespace_thread(&netns_relay_handle);

	assert(wait_sync_flag(&test_random_port_binding_node_started, 20));

	set_sync_flag(&test_random_port_binding_node_started, false);
	peer_arg.join_invitation = peer_invite;
	netns_thread_t netns_peer_handle = {.namespace_name = "peer", .netns_thread = peer_node, .arg = &peer_arg};
	run_node_in_namespace_thread(&netns_peer_handle);

	assert(wait_sync_flag(&test_random_port_binding_node_started, 20));

	set_sync_flag(&test_random_port_binding_node_started, false);
	nut_arg.join_invitation = nut_invite;
	netns_thread_t netns_nut_handle = {.namespace_name = "nut", .netns_thread = nut_node, .arg = &nut_arg};
	run_node_in_namespace_thread(&netns_nut_handle);

	assert(wait_sync_flag(&test_random_port_binding_node_started, 20));

	assert(wait_sync_flag(&test_random_port_binding_peer_reachable, 300));

	set_sync_flag(&test_random_port_binding_make_switch, true);

	assert(wait_sync_flag(&test_random_port_binding_node_connected, 300));

	fprintf(stderr, "\n\x1b[32mDone Test-05\x1b[0m\n");

	assert(wait_sync_flag(&test_random_port_binding_relay_closed, 10));
	assert(wait_sync_flag(&test_random_port_binding_peer_closed, 10));
	assert(wait_sync_flag(&test_random_port_binding_nut_closed, 10));

	return true;
}

// Optimal PMTU test case driver

int test_meshlink_random_port_bindings02(void) {
	interface_t nut_ifs[] = {{.if_peer = "wan_bridge"}};
	namespace_t nut = {
		.name = "nut",
		.type = HOST,
		.interfaces = nut_ifs,
		.interfaces_no = 1,
	};

	interface_t peer_ifs[] = {{.if_peer = "wan_bridge"}};
	namespace_t peer = {
		.name = "peer",
		.type = HOST,
		.interfaces = peer_ifs,
		.interfaces_no = 1,
	};

	interface_t relay_ifs[] = {{.if_peer = "wan_bridge"}};
	namespace_t relay = {
		.name = "relay",
		.type = HOST,
		.interfaces = relay_ifs,
		.interfaces_no = 1,
	};

	interface_t wan_ifs[] = { { .if_peer = "nut" }, { .if_peer = "peer" }, { .if_peer = "relay" } };
	namespace_t wan_bridge = {
		.name = "wan_bridge",
		.type = BRIDGE,
		.interfaces = wan_ifs,
		.interfaces_no = 3,
	};

	namespace_t test_random_port_bindings_02_nodes[] = {wan_bridge, nut, peer, relay };

	netns_state_t test_port_bindings_nodes = {
		.test_case_name =  "test_case_random_port_bindings_02",
		.namespaces =  test_random_port_bindings_02_nodes,
		.num_namespaces = 4,
	};
	test_random_port_bindings_state = &test_port_bindings_nodes;

	const struct CMUnitTest blackbox_group0_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_random_port_bindings_04, setup_test, teardown_test,
		                (void *)&test_random_port_bindings_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_random_port_bindings_05, setup_test, teardown_test,
		                (void *)&test_random_port_bindings_state)
	};
	total_tests += sizeof(blackbox_group0_tests) / sizeof(blackbox_group0_tests[0]);

	return cmocka_run_group_tests(blackbox_group0_tests, NULL, NULL);
}
