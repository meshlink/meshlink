/*
    test_cases.c -- Execution of specific meshlink black box test cases
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

#define RELAY_ID "0"
#define PEER_ID  "1"
#define NUT_ID   "2"

static void test_case_meta_conn_01(void **state);
static bool test_steps_meta_conn_01(void);
static void test_case_meta_conn_02(void **state);
static bool test_steps_meta_conn_02(void);
static void test_case_meta_conn_03(void **state);
static bool test_steps_meta_conn_03(void);
static void test_case_meta_conn_04(void **state);
static bool test_steps_meta_conn_04(void);
static void test_case_meta_conn_05(void **state);
static bool test_steps_meta_conn_05(void);

/* State structure for Meta-connections Test Case #1 */
static char *test_meta_conn_1_nodes[] = { "relay", "peer", "nut" };
static black_box_state_t test_meta_conn_1_state = {
	.test_case_name =  "test_case_meta_conn_01",
	.node_names =  test_meta_conn_1_nodes,
	.num_nodes =  3,
};

/* State structure for Meta-connections Test Case #2 */
static char *test_meta_conn_2_nodes[] = { "relay", "peer", "nut" };
static black_box_state_t test_meta_conn_2_state = {
	.test_case_name = "test_case_meta_conn_02",
	.node_names = test_meta_conn_2_nodes,
	.num_nodes = 3,
};

/* State structure for Meta-connections Test Case #3 */
static char *test_meta_conn_3_nodes[] = { "relay", "peer", "nut" };
static black_box_state_t test_meta_conn_3_state = {
	.test_case_name = "test_case_meta_conn_03",
	.node_names = test_meta_conn_3_nodes,
	.num_nodes = 3,
};

/* State structure for Meta-connections Test Case #4 */
static char *test_meta_conn_4_nodes[] = { "peer", "nut" };
static black_box_state_t test_meta_conn_4_state = {
	.test_case_name = "test_case_meta_conn_04",
	.node_names = test_meta_conn_4_nodes,
	.num_nodes = 2,
};

/* State structure for Meta-connections Test Case #5 */
static char *test_meta_conn_5_nodes[] = { "peer", "nut" };
static black_box_state_t test_meta_conn_5_state = {
	.test_case_name = "test_case_meta_conn_05",
	.node_names = test_meta_conn_5_nodes,
	.num_nodes = 2,
};

int black_box_group0_setup(void **state) {
	const char *nodes[] = { "peer", "relay", "nut"};
	int num_nodes = sizeof(nodes) / sizeof(nodes[0]);

	PRINT_TEST_CASE_MSG("Creating Containers\n");
	destroy_containers();
	create_containers(nodes, num_nodes);

	return 0;
}

int black_box_group0_teardown(void **state) {
	PRINT_TEST_CASE_MSG("Destroying Containers\n");
	destroy_containers();

	return 0;
}

int black_box_all_nodes_setup(void **state) {
	const char *nodes[] = { "peer" };
	int num_nodes = sizeof(nodes) / sizeof(nodes[0]);

	PRINT_TEST_CASE_MSG("Creating Containers\n");
	destroy_containers();
	create_containers(nodes, num_nodes);
	PRINT_TEST_CASE_MSG("Created Containers\n");
	return 0;
}

static bool meta_conn01_conn;
static bool meta_conn01_closed;
static bool meta_conn01_reconn;

static void meta_conn01_cb(mesh_event_payload_t payload) {
	char event_node_name[][10] = {"RELAY", "PEER", "NUT"};
	fprintf(stderr, "%s : ", event_node_name[payload.client_id]);

	switch(payload.mesh_event) {
	case META_CONN_SUCCESSFUL   :
		meta_conn01_conn = true;
		break;

	case NODE_STARTED           :
		fprintf(stderr, "Node started\n");
		break;

	case META_CONN_CLOSED       :
		meta_conn01_closed = true;
		break;

	case META_RECONN_SUCCESSFUL :
		meta_conn01_reconn = true;
		break;
	}
}

/* Execute Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third node */
static void test_case_meta_conn_01(void **state) {
	execute_test(test_steps_meta_conn_01, state);
}

/* Test Steps for Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third (relay) node

    Test Steps:
    1. Run NUT, relay and peer nodes with relay inviting the other two nodes
    2. After connection to peer, terminate the peer node's running instance
    3. After peer becomes unreachable, wait 60 seconds then re-start the peer node's instance

    Expected Result:
    NUT is re-connected to peer
*/
static bool test_steps_meta_conn_01(void) {
	char *invite_peer, *invite_nut;
	bool result = false;
	int i;
	char *import;

	import = mesh_event_sock_create(eth_if_name);
	invite_peer = invite_in_container("relay", "peer");
	invite_nut = invite_in_container("relay", NUT_NODE_NAME);
	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	wait_for_event(meta_conn01_cb, 5);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	wait_for_event(meta_conn01_cb, 5);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);
	wait_for_event(meta_conn01_cb, 5);

	PRINT_TEST_CASE_MSG("Waiting for peer to be connected with NUT\n");
	assert(wait_for_event(meta_conn01_cb, 60));
	assert(meta_conn01_conn);

	PRINT_TEST_CASE_MSG("Sending SIGTERM to peer\n");
	node_step_in_container("peer", "SIGTERM");
	PRINT_TEST_CASE_MSG("Waiting for peer to become unreachable\n");
	assert(wait_for_event(meta_conn01_cb, 60));
	assert(meta_conn01_closed);

	node_sim_in_container_event("peer", "1", NULL, PEER_ID, import);
	wait_for_event(meta_conn01_cb, 5);
	PRINT_TEST_CASE_MSG("Waiting for peer to be re-connected\n");
	wait_for_event(meta_conn01_cb, 60);

	assert_int_equal(meta_conn01_reconn, true);

	free(invite_peer);
	free(invite_nut);

	return true;
}


static bool meta_conn02_conn;

static void meta_conn02_cb(mesh_event_payload_t payload) {
	char event_node_name[][10] = {"RELAY", "PEER", "NUT"};
	fprintf(stderr, "%s : ", event_node_name[payload.client_id]);

	switch(payload.mesh_event) {
	case META_CONN_SUCCESSFUL   :
		fprintf(stderr, "Meta Connection Successful\n");
		meta_conn02_conn = true;
		break;

	case NODE_STARTED           :
		fprintf(stderr, "Node started\n");
		break;
	}

	if(payload.payload_length) {
		fprintf(stderr, " %s\n", (char *)payload.payload);
	}
}
/* Execute Meta-connections Test Case # 2 - re-connection to peer via third node
    after changing IP of NUT and peer */
static void test_case_meta_conn_02(void **state) {
	execute_test(test_steps_meta_conn_02, state);
}
/* Test Steps for Meta-connections Test Case # 2 - re-connection to peer via third node
    after changing IP of NUT and peer

    Test Steps:
    1. Run NUT, relay and peer nodes with relay inviting the other two nodes
    2. After connection to peer, change the NUT's IP Address and the peer node's IP Address

    Expected Result:
    NUT is first disconnected from peer then automatically re-connected to peer
*/
static bool test_steps_meta_conn_02(void) {
	char *invite_peer, *invite_nut;
	bool result = false;
	int i;
	char *import;

	import = mesh_event_sock_create(eth_if_name);
	invite_peer = invite_in_container("relay", "peer");
	invite_nut = invite_in_container("relay", NUT_NODE_NAME);
	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	wait_for_event(meta_conn02_cb, 5);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	wait_for_event(meta_conn02_cb, 5);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);
	wait_for_event(meta_conn02_cb, 5);

	PRINT_TEST_CASE_MSG("Waiting for peer to be connected with NUT\n");
	assert(wait_for_event(meta_conn02_cb, 60));
	assert(meta_conn02_conn);

	meta_conn02_conn = false;
	node_sim_in_container_event("peer", "1", NULL, PEER_ID, import);
	wait_for_event(meta_conn02_cb, 5);
	node_sim_in_container_event("nut", "1", NULL, NUT_ID, import);
	wait_for_event(meta_conn02_cb, 5);

	PRINT_TEST_CASE_MSG("Waiting for peer to be connected with NUT\n");

	if(!wait_for_event(meta_conn02_cb, 60)) {
		return false;
	}

	result = meta_conn02_conn;

	free(invite_peer);
	free(invite_nut);

	return result;
}

static bool meta_conn03_result;
static bool meta_conn03_conn;

static void meta_conn03_cb(mesh_event_payload_t payload) {
	char event_node_name[][10] = {"RELAY", "PEER", "NUT"};
	fprintf(stderr, "%s : ", event_node_name[payload.client_id]);

	switch(payload.mesh_event) {
	case META_CONN_SUCCESSFUL   :
		fprintf(stderr, "Meta Connection Successful\n");
		meta_conn03_conn = true;
		break;

	case NODE_STARTED           :
		fprintf(stderr, "Node started\n");
		break;

	case META_RECONN_FAILURE    :
		fprintf(stderr, "Failed to reconnect with");
		meta_conn03_result = false;
		break;

	case META_RECONN_SUCCESSFUL :
		fprintf(stderr, "Reconnected\n");
		meta_conn03_result = true;
		break;
	}
}
/* Execute Meta-connections Test Case # 3 - re-connection to peer via third node
    after changing IP of peer */
static void test_case_meta_conn_03(void **state) {
	execute_test(test_steps_meta_conn_03, state);
}
/* Test Steps for Meta-connections Test Case # 3 - re-connection to peer via third node
    after changing IP of peer

    Test Steps:
    1. Run NUT, relay and peer nodes with relay inviting the other two nodes
    2. After connection to peer, change the peer node's IP Address

    Expected Result:
    NUT is first disconnected from peer then automatically re-connected to peer
*/
static bool test_steps_meta_conn_03(void) {
	char *invite_peer, *invite_nut;
	bool result = false;
	int i;
	char *import;

	import = mesh_event_sock_create(eth_if_name);
	invite_peer = invite_in_container("relay", "peer");
	invite_nut = invite_in_container("relay", NUT_NODE_NAME);
	node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
	wait_for_event(meta_conn03_cb, 5);
	node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
	wait_for_event(meta_conn03_cb, 5);
	node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);
	wait_for_event(meta_conn03_cb, 5);

	PRINT_TEST_CASE_MSG("Waiting for peer to be connected with NUT\n");
	assert(wait_for_event(meta_conn03_cb, 60));
	assert(meta_conn03_conn);

	PRINT_TEST_CASE_MSG("Changing IP address of PEER container\n");
	change_ip(1);
	node_sim_in_container_event("peer", "1", NULL, PEER_ID, import);
	wait_for_event(meta_conn03_cb, 5);
	PRINT_TEST_CASE_MSG("Waiting for peer to be re-connected\n");
	wait_for_event(meta_conn03_cb, 5);
	result = meta_conn03_result;
	free(invite_peer);
	free(invite_nut);

	return result;
}

static char *invite_peer = NULL;
static bool meta_conn04 = false;

static void meta_conn04_cb(mesh_event_payload_t payload) {
	char event_node_name[][10] = {"PEER", "NUT"};
	fprintf(stderr, "%s : ", event_node_name[payload.client_id]);

	switch(payload.mesh_event) {
	case META_CONN_SUCCESSFUL   :
		fprintf(stderr, "Meta Connection Successful\n");
		meta_conn04 = true;
		break;

	case NODE_INVITATION        :
		fprintf(stderr, "Invitation generated\n");
		invite_peer = malloc(payload.payload_length);
		strcpy(invite_peer, (char *)payload.payload);
		break;

	case NODE_STARTED           :
		fprintf(stderr, "Node started\n");
		break;
	}
}

/* Execute Meta-connections Test Case # 4 - re-connection to peer after changing IP of
    NUT and peer */
static void test_case_meta_conn_04(void **state) {
	execute_test(test_steps_meta_conn_04, state);
}

/* Execute Meta-connections Test Case # 4 - re-connection to peer after changing IP of
    NUT and peer

    Test Steps:
    1. Run NUT and peer nodes with NUT inviting the peer node
    2. After connection to peer, change the NUT's IP Address and the peer node's IP Address

    Expected Result:
    NUT is first disconnected from peer then automatically re-connected to peer
*/
static bool test_steps_meta_conn_04(void) {
	bool result = false;
	char *import;

	import = mesh_event_sock_create(eth_if_name);
	node_sim_in_container_event("nut", "1", NULL, "1", import);
	wait_for_event(meta_conn04_cb, 5);

	PRINT_TEST_CASE_MSG("Waiting for NUT to generate invitation to PEER\n");
	wait_for_event(meta_conn04_cb, 5);

	if(!invite_peer) {
		return false;
	}

	PRINT_TEST_CASE_MSG("Running PEER node in the container\n");
	node_sim_in_container_event("peer", "1", invite_peer, "0", import);
	wait_for_event(meta_conn04_cb, 5);
	PRINT_TEST_CASE_MSG("Waiting for peer to be connected with NUT\n");

	if(!wait_for_event(meta_conn04_cb, 60)) {
		return false;
	}

	PRINT_TEST_CASE_MSG("Changing IP address of NUT container\n");
	change_ip(1);

	node_sim_in_container_event("nut", "1", "restart", "1", import);
	wait_for_event(meta_conn04_cb, 5);
	PRINT_TEST_CASE_MSG("Changing IP address of PEER container\n");
	change_ip(0);
	node_sim_in_container_event("peer", "1", NULL, "0", import);
	wait_for_event(meta_conn04_cb, 5);

	PRINT_TEST_CASE_MSG("Waiting for peer to be re-connected\n");
	wait_for_event(meta_conn04_cb, 5);
	result = meta_conn04;

	free(invite_peer);
	free(import);
	return result;
}

static char *invitation = NULL;

static bool meta_conn05 = false;

static void meta_conn05_cb(mesh_event_payload_t payload) {
	char event_node_name[][10] = {"PEER", "NUT"};
	fprintf(stderr, "%s : ", event_node_name[payload.client_id]);

	switch(payload.mesh_event) {
	case META_CONN_SUCCESSFUL   :
		meta_conn05 = true;
		break;

	case NODE_INVITATION        :
		invitation = malloc(payload.payload_length);
		strcpy(invitation, (char *)payload.payload);
		break;

	case NODE_STARTED           :
		fprintf(stderr, "Node started\n");
		break;
	}
}

/* Execute Meta-connections Test Case # 5 - re-connection to peer after changing IP of peer */
static void test_case_meta_conn_05(void **state) {
	execute_test(test_steps_meta_conn_05, state);
}

/* Execute Meta-connections Test Case # 5 - re-connection to peer after changing IP of peer

    Test Steps:
    1. Run NUT and peer nodes with NUT inviting the peer node
    2. After connection to peer, change the peer node's IP Address

    Expected Result:
    NUT is first disconnected from peer then automatically re-connected to peer
*/
static bool test_steps_meta_conn_05(void) {
	bool result = false;
	char *import;

	import = mesh_event_sock_create(eth_if_name);
	node_sim_in_container_event("nut", "1", NULL, "1", import);
	wait_for_event(meta_conn05_cb, 5);

	wait_for_event(meta_conn05_cb, 5);

	if(!invitation) {
		return false;
	}

	node_sim_in_container_event("peer", "1", invitation, "0", import);
	wait_for_event(meta_conn05_cb, 5);

	if(!wait_for_event(meta_conn05_cb, 5)) {
		return false;
	}

	change_ip(0);
	meta_conn05 = false;
	node_sim_in_container_event("peer", "1", NULL, "0", import);
	wait_for_event(meta_conn05_cb, 5);
	PRINT_TEST_CASE_MSG("Waiting for peer to be re-connected\n");
	wait_for_event(meta_conn05_cb, 5);
	result = meta_conn05;

	free(invitation);
	free(import);
	return result;
}

int test_meta_conn(void) {
	const struct CMUnitTest blackbox_group0_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_meta_conn_01, setup_test, teardown_test,
		                (void *)&test_meta_conn_1_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_meta_conn_02, setup_test, teardown_test,
		                (void *)&test_meta_conn_2_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_meta_conn_03, setup_test, teardown_test,
		                (void *)&test_meta_conn_3_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_meta_conn_04, setup_test, teardown_test,
		                (void *)&test_meta_conn_4_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_meta_conn_05, setup_test, teardown_test,
		                (void *)&test_meta_conn_5_state)
	};
	total_tests += sizeof(blackbox_group0_tests) / sizeof(blackbox_group0_tests[0]);

	return cmocka_run_group_tests(blackbox_group0_tests, black_box_group0_setup, black_box_group0_teardown);
}
