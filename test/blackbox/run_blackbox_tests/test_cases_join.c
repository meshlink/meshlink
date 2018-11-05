/*
    test_cases_join.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>

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

#include "execute_tests.h"
#include "test_cases_join.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <pthread.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_meshlink_join_01(void **state);
static bool test_meshlink_join_01(void);
static void test_case_meshlink_join_02(void **state);
static bool test_meshlink_join_02(void);
static void test_case_meshlink_join_03(void **state);
static bool test_meshlink_join_03(void);
static void test_case_meshlink_join_04(void **state);
static bool test_meshlink_join_04(void);
static void status_callback(meshlink_handle_t *mesh, meshlink_node_t *source, bool reach);

/* State structure for join Test Case #1 */
static char *test_join_1_nodes[] = { "relay" };
static black_box_state_t test_case_join_01_state = {
	.test_case_name = "test_case_join_01",
	.node_names = test_join_1_nodes,
	.num_nodes = 1,
};

/* State structure for join Test Case #1 */
static char *test_join_2_nodes[] = { "relay" };
static black_box_state_t test_case_join_02_state = {
	.test_case_name = "test_case_join_02",
	.node_names = test_join_2_nodes,
	.num_nodes = 1,
};

/* State structure for join Test Case #1 */
static black_box_state_t test_case_join_03_state = {
	.test_case_name = "test_case_join_03",
};

static bool join_status;

static int black_box_group_join_setup(void **state) {
	const char *nodes[] = { "relay" };
	int num_nodes = sizeof(nodes) / sizeof(nodes[0]);

	printf("Creating Containers\n");
	destroy_containers();
	create_containers(nodes, num_nodes);

	return 0;
}

static int black_box_group_join_teardown(void **state) {
	printf("Destroying Containers\n");
	destroy_containers();

	return 0;
}

/* status callback */
static void status_callback(meshlink_handle_t *mesh, meshlink_node_t *source, bool reach) {
	if(reach && !strcmp(source->name, "relay")) {
		join_status = true;
	}

	return;
}

/* Execute join Test Case # 1 - valid case*/
static void test_case_meshlink_join_01(void **state) {
	execute_test(test_meshlink_join_01, state);
	return;
}

/* Test Steps for meshlink_join Test Case # 1 - Valid case

    Test Steps:
    1. Generate invite in relay container and run 'relay' node
    2. Run NUT
    3. Join NUT with relay using invitation generated.

    Expected Result:
    NUT joins relay using the invitation generated.
*/
static bool test_meshlink_join_01(void) {
	meshlink_destroy("join_conf");

	// Create node instances

	char *invite_nut = invite_in_container("relay", "nut");
	node_sim_in_container("relay", "1", NULL);
	sleep(1);

	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	mesh_handle = meshlink_open("join_conf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_node_status_cb(mesh_handle, status_callback);

	// Joining Node-Under-Test with relay
	bool ret = meshlink_join(mesh_handle, invite_nut);
	assert(meshlink_start(mesh_handle));
	sleep(1);

	bool stat_ret = join_status;

	if(stat_ret) {
		PRINT_TEST_CASE_MSG("NUT joined with relay\n");
	} else {
		PRINT_TEST_CASE_MSG("NUT didn't join with relay\n");
	}

	free(invite_nut);
	meshlink_close(mesh_handle);
	meshlink_destroy("joinconf");

	return stat_ret && ret;
}

/* Execute join Test Case # 2 - Invalid case*/
static void test_case_meshlink_join_02(void **state) {
	execute_test(test_meshlink_join_02, state);
	return;
}

/* Test Steps for meshlink_join Test Case # 2 - Invalid case

    Test Steps:
    1. Call meshlink_join with NULL as mesh handler argument.

    Expected Result:
    report error accordingly when NULL is passed as mesh handle argument
*/
static bool test_meshlink_join_02(void) {
	char *invite_nut = invite_in_container("relay", NUT_NODE_NAME);

	/* meshlink_join called with NULL as mesh handle and with valid invitation */
	bool ret = meshlink_join(NULL, invite_nut);
	free(invite_nut);

	if(ret) {
		PRINT_TEST_CASE_MSG("meshlink_join reported error accordingly\n");
	} else {
		PRINT_TEST_CASE_MSG("meshlink_join failed to report error accordingly\n");
	}

	return !ret;
}

/* Execute join Test Case # 3- Invalid case*/
static void test_case_meshlink_join_03(void **state) {
	execute_test(test_meshlink_join_03, state);
	return;
}

/* Test Steps for meshlink_join Test Case # 3 - Invalid case

    Test Steps:
    1. Run NUT
    1. Call meshlink_join with NULL as invitation argument.

    Expected Result:
    Report error accordingly when NULL is passed as invite argument
*/
static bool test_meshlink_join_03(void) {
	meshlink_destroy("joinconf");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	mesh_handle = meshlink_open("joinconf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_node_status_cb(mesh_handle, status_callback);

	/*Joining the NUT with relay*/
	bool ret  = meshlink_join(mesh_handle, NULL);

	if(ret) {
		PRINT_TEST_CASE_MSG("meshlink_join reported error accordingly when NULL is passed as invite argument\n");
	} else {
		PRINT_TEST_CASE_MSG("meshlink_join failed to report error accordingly when NULL is passed as invite argument\n");
	}

	meshlink_close(mesh_handle);
	meshlink_destroy("joinconf");
	return !ret;
}

int test_meshlink_join(void) {
	const struct CMUnitTest blackbox_join_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_join_01, setup_test, teardown_test,
		(void *)&test_case_join_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_join_02, setup_test, teardown_test,
		(void *)&test_case_join_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_join_03, NULL, NULL,
		(void *)&test_case_join_03_state)
	};
	total_tests += sizeof(blackbox_join_tests) / sizeof(blackbox_join_tests[0]);

	int failed = cmocka_run_group_tests(blackbox_join_tests , black_box_group_join_setup , black_box_group_join_teardown);

	return failed;
}

