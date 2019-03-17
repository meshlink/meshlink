/*
    test_cases_submesh02.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_submesh02.h"
#include "pthread.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../common/mesh_event_handler.h"

#define CORENODE1_ID  "0"
#define APP1NODE1_ID  "1"
#define APP2NODE1_ID  "2"
#define CORENODE2_ID  "3"
#define APP1NODE2_ID  "4"
#define APP2NODE2_ID  "5"

#define INIT_ST         0

static bool test_case_status = false;

static void test_case_submesh_02(void **state);
static bool test_steps_submesh_02(void);

static char event_node_name[][10] = {"CORENODE1", "APP1NODE1", "APP2NODE1", "CORENODE2",
                                     "APP1NODE2", "APP2NODE2"
                                    };
static const char *node_ids[] = { "corenode1", "app1node1", "app2node1", "corenode2",
                                  "app1node2", "app2node2"
                                };

static mesh_event_t core_node1[] = { NODE_STARTED, CHANNEL_OPENED, CHANNEL_DATA_RECIEVED};

static mesh_event_t core_node2[] = { NODE_STARTED, NODE_JOINED, CHANNEL_OPENED, CHANNEL_DATA_RECIEVED};

static mesh_event_t app1_node1[] = { NODE_STARTED, NODE_JOINED, CHANNEL_OPENED, CHANNEL_DATA_RECIEVED};

static mesh_event_t app2_node1[] = { NODE_STARTED, NODE_JOINED, CHANNEL_OPENED, CHANNEL_DATA_RECIEVED};

static mesh_event_t app1_node2[] = { NODE_STARTED, NODE_JOINED, CHANNEL_OPENED, CHANNEL_DATA_RECIEVED, CHANNEL_OPENED, CHANNEL_DATA_RECIEVED, MESH_EVENT_COMPLETED};

static mesh_event_t app2_node2[] = { NODE_STARTED, NODE_JOINED, CHANNEL_OPENED, CHANNEL_DATA_RECIEVED, CHANNEL_OPENED, CHANNEL_DATA_RECIEVED, MESH_EVENT_COMPLETED};

/* State structure for SubMesh Test Case #1 */
static char *test_case_submesh_2_nodes[] = { "corenode1", "app1node1", "app2node1", "corenode2", "app1node2", "app2node2" };
static black_box_state_t test_case_submesh_2_state = {
	.test_case_name =  "test_cases_submesh02",
	.node_names =  test_case_submesh_2_nodes,
	.num_nodes =  6
};

static int black_box_group0_setup(void **state) {
	const char *nodes[] = { "corenode1", "app1node1", "app2node1", "corenode2", "app1node2", "app2node2" };
	int num_nodes = sizeof(nodes) / sizeof(nodes[0]);

	PRINT_TEST_CASE_MSG("Creating Containers\n");
	destroy_containers();
	create_containers(nodes, num_nodes);

	return 0;
}

static int black_box_group0_teardown(void **state) {
	PRINT_TEST_CASE_MSG("Destroying Containers\n");
	destroy_containers();

	return 0;
}

static int black_box_all_nodes_setup(void **state) {
	const char *nodes[] = { "corenode1" };
	int num_nodes = sizeof(nodes) / sizeof(nodes[0]);

	PRINT_TEST_CASE_MSG("Creating Containers\n");
	destroy_containers();
	create_containers(nodes, num_nodes);
	PRINT_TEST_CASE_MSG("Created Containers\n");
	return 0;
}

static bool event_cb(mesh_event_payload_t payload) {
	static node_status_t node_status[6] = {
		{core_node1, 0, 3, false},
		{app1_node1, 0, 4, false},
		{app2_node1, 0, 4, false},
		{core_node2, 0, 4, false},
		{app1_node2, 0, 7, false},
		{app2_node2, 0, 7, false}
	};

	fprintf(stderr, "%s(%lu) : %s\n", event_node_name[payload.client_id], time(NULL), event_status[payload.mesh_event]);
	assert(change_state(&node_status[payload.client_id], payload.mesh_event));

	if(payload.mesh_event == NODE_JOINED) {
		signal_node_start(node_status, 1, 5, node_ids);
	}

	if(check_nodes_finished(node_status, 6)) {
		test_case_status = true;
		return true;
	}

	return false;
}

/* Execute SubMesh Test Case # 2 */
static void test_case_submesh_02(void **state) {
	execute_test(test_steps_submesh_02, state);
}

/* Test Steps for SubMesh Test Case # 2

    Test Steps:
    1. Run corenode1, app1node1, app2node1, corenode2, app1node2 and app2node2
    2. Generate invites to app1node1, app2node1, corenode2, app1node2 and app2node2
        from corenode1 to join corenode1.
    3. After Join is successful start channels from all nodes and exchange data on channels
    4. Try to fetch the list of all nodes and check if the nodes in other submesh doesnot
       appear in the list.
    5. Try fetch all the nodes with a submesh handle and check only if both the nodes joining
       the submesh are present.

    Expected Result:
    Channels should be formed between nodes of sub-mesh & coremesh, nodes with in sub-mesh
    and should be able to exchange data. Lis of all nodes should only contain four nodes
    and the list of submesh should only contain two nodes of that submesh.
*/
static bool test_steps_submesh_02(void) {
	char *invite_corenode2, *invite_app1node1, *invite_app2node1, *invite_app1node2, *invite_app2node2;
	bool result = false;
	int i;
	char *import;
	pthread_t thread1, thread2;

	import = mesh_event_sock_create(eth_if_name);
	invite_corenode2 = invite_in_container("corenode1", "corenode2");
	invite_app1node1 = submesh_invite_in_container("corenode1", "app1node1", "app1");
	invite_app2node1 = submesh_invite_in_container("corenode1", "app2node1", "app2");
	invite_app1node2 = submesh_invite_in_container("corenode1", "app1node2", "app1");
	invite_app2node2 = submesh_invite_in_container("corenode1", "app2node2", "app2");

	node_sim_in_container_event("corenode1", "1", NULL, CORENODE1_ID, import);
	node_sim_in_container_event("corenode2", "1", invite_corenode2, CORENODE2_ID, import);
	node_sim_in_container_event("app1node1", "1", invite_app1node1, APP1NODE1_ID, import);
	node_sim_in_container_event("app2node1", "1", invite_app2node1, APP2NODE1_ID, import);
	node_sim_in_container_event("app1node2", "1", invite_app1node2, APP1NODE2_ID, import);
	node_sim_in_container_event("app2node2", "1", invite_app2node2, APP2NODE2_ID, import);

	PRINT_TEST_CASE_MSG("Waiting for nodes to get connected with corenode1\n");

	assert(wait_for_event(event_cb, 240));
	assert(test_case_status);

	free(invite_corenode2);
	free(invite_app1node1);
	free(invite_app2node1);
	free(invite_app1node2);
	free(invite_app2node2);

	mesh_event_destroy();

	return true;
}

int test_cases_submesh02(void) {
	const struct CMUnitTest blackbox_group0_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_submesh_02, setup_test, teardown_test,
		                (void *)&test_case_submesh_2_state)
	};
	total_tests += sizeof(blackbox_group0_tests) / sizeof(blackbox_group0_tests[0]);

	return cmocka_run_group_tests(blackbox_group0_tests, black_box_group0_setup, black_box_group0_teardown);
}