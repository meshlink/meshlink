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
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../common/network_namespace_framework.h"
#include "../../utils.h"
#include "test_cases_channel_blacklist.h"
#include "../test_case_channel_blacklist_01/node_sim_nut_01.h"

typedef bool (*test_step_func_t)(void);
extern int total_tests;

static bool test_steps_channel_blacklist_01(void);
static void test_case_channel_blacklist_01(void **state);

static int setup_test(void **state);
static int teardown_test(void **state);
static void *gen_inv(void *arg);

netns_state_t *test_channel_disconnection_state;

static mesh_arg_t relay_arg = {.node_name = "relay", .confbase = "relay", .app_name = "relay", .dev_class = 0 };
static mesh_arg_t peer_arg = {.node_name = "peer", .confbase = "peer", .app_name = "peer", .dev_class = 1 };
static mesh_arg_t nut_arg = {.node_name = "nut", .confbase = "nut", .app_name = "nut", .dev_class = 1 };
static mesh_invite_arg_t relay_nut_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "nut" };
static mesh_invite_arg_t relay_peer_invite_arg = {.mesh_arg = &relay_arg, .invitee_name = "peer" };
static netns_thread_t netns_relay_nut_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_nut_invite_arg};
static netns_thread_t netns_relay_peer_invite = {.namespace_name = "relay", .netns_thread = gen_inv, .arg = &relay_peer_invite_arg};
static netns_thread_t netns_relay_handle = {.namespace_name = "relay", .netns_thread = test_channel_blacklist_disonnection_relay_01, .arg = &relay_arg};
static netns_thread_t netns_peer_handle = {.namespace_name = "peer", .netns_thread = test_channel_blacklist_disonnection_peer_01, .arg = &peer_arg};
static netns_thread_t netns_nut_handle = {.namespace_name = "nut", .netns_thread = test_channel_blacklist_disonnection_nut_01, .arg = &nut_arg};

struct sync_flag test_channel_discon_nut_close = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};

static int setup_test(void **state) {
	(void)state;

	netns_create_topology(test_channel_disconnection_state);
	fprintf(stderr, "\nCreated topology\n");

	meshlink_destroy("nut");
	meshlink_destroy("peer");
	meshlink_destroy("relay");
	channel_discon_case_ping = false;
	channel_discon_network_failure_01 = false;
	channel_discon_network_failure_02 = false;
	test_channel_restart_01 = false;
	set_sync_flag(&test_channel_discon_nut_close, false);

	return EXIT_SUCCESS;
}

static int teardown_test(void **state) {
	(void)state;

	meshlink_destroy("nut");
	meshlink_destroy("peer");
	meshlink_destroy("relay");
	netns_destroy_topology(test_channel_disconnection_state);

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

static void launch_3_nodes(void) {
	run_node_in_namespace_thread(&netns_relay_nut_invite);
	sleep(1);
	assert(relay_nut_invite_arg.invite_str);
	nut_arg.join_invitation = relay_nut_invite_arg.invite_str;

	run_node_in_namespace_thread(&netns_relay_peer_invite);
	sleep(1);
	assert(relay_peer_invite_arg.invite_str);
	peer_arg.join_invitation = relay_peer_invite_arg.invite_str;

	relay_arg.join_invitation = NULL;

	run_node_in_namespace_thread(&netns_relay_handle);
	sleep(1);

	run_node_in_namespace_thread(&netns_peer_handle);
	sleep(1);

	run_node_in_namespace_thread(&netns_nut_handle);
}

static void test_case_channel_blacklist_01(void **state) {
	execute_test(test_steps_channel_blacklist_01, state);
	return;
}

static bool test_steps_channel_blacklist_01(void) {

	launch_3_nodes();

	wait_sync_flag(&test_channel_discon_nut_close, 240);

	test_channel_blacklist_disonnection_peer_01_running = false;
	test_channel_blacklist_disonnection_relay_01_running = false;
	assert_int_equal(total_reachable_callbacks_01, 1);
	assert_int_equal(total_unreachable_callbacks_01, 1);
	assert_int_equal(total_channel_closure_callbacks_01, 2);

	return true;
}

int test_meshlink_channel_blacklist(void) {

	interface_t relay_ifs[] = { { .if_peer = "wan_bridge" } };
	namespace_t relay = {
		.name = "relay",
		.type = HOST,
		.interfaces = relay_ifs,
		.interfaces_no = 1,
	};

	interface_t peer_ifs[] = { { .if_peer = "wan_bridge" } };
	namespace_t peer = {
		.name = "peer",
		.type = HOST,
		.interfaces = peer_ifs,
		.interfaces_no = 1,
	};

	interface_t nut_ifs[] = { { .if_peer = "wan_bridge" } };
	namespace_t nut = {
		.name = "nut",
		.type = HOST,
		.interfaces = nut_ifs,
		.interfaces_no = 1,
	};

	interface_t wan_ifs[] = { { .if_peer = "peer" }, { .if_peer = "nut" }, { .if_peer = "relay" } };
	namespace_t wan_bridge = {
		.name = "wan_bridge",
		.type = BRIDGE,
		.interfaces = wan_ifs,
		.interfaces_no = 3,
	};

	namespace_t test_channel_nodes[] = {  relay, wan_bridge, nut, peer };

	netns_state_t test_channels_nodes = {
		.test_case_name =  "test_case_channel",
		.namespaces =  test_channel_nodes,
		.num_namespaces = 4,
	};
	test_channel_disconnection_state = &test_channels_nodes;

	const struct CMUnitTest blackbox_group0_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_blacklist_01, setup_test, teardown_test,
		                (void *)NULL),
	};
	total_tests += sizeof(blackbox_group0_tests) / sizeof(blackbox_group0_tests[0]);

	return cmocka_run_group_tests(blackbox_group0_tests, NULL, NULL);
}
