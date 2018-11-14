/*
    test_cases_get_node.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_get_node.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_mesh_get_node_01(void **state);
static bool test_steps_mesh_get_node_01(void);
static void test_case_mesh_get_node_02(void **state);
static bool test_steps_mesh_get_node_02(void);
static void test_case_mesh_get_node_03(void **state);
static bool test_steps_mesh_get_node_03(void);
static void test_case_mesh_get_node_04(void **state);
static bool test_steps_mesh_get_node_04(void);

/* State structure for meshlink_get_node Test Case #1 */
static black_box_state_t test_mesh_get_node_01_state = {
	.test_case_name = "test_case_mesh_get_node_01",
};

/* State structure for meshlink_get_node Test Case #2 */
static black_box_state_t test_mesh_get_node_02_state = {
	.test_case_name = "test_case_mesh_get_node_02",
};

/* State structure for meshlink_get_node Test Case #3 */
static black_box_state_t test_mesh_get_node_03_state = {
	.test_case_name = "test_case_mesh_get_node_03",
};

/* State structure for meshlink_get_node Test Case #4 */
static black_box_state_t test_mesh_get_node_04_state = {
	.test_case_name = "test_case_mesh_get_node_04",
};

/* Execute meshlink_get_node Test Case # 1 */
static void test_case_mesh_get_node_01(void **state) {
	execute_test(test_steps_mesh_get_node_01, state);
}

/* Test Steps for meshlink_get_node Test Case # 1

    Test Steps:
    1. Open nodes instance
    2. Get node's handle

    Expected Result:
    node handle of it's own is obtained
*/
static bool test_steps_mesh_get_node_01(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_destroy("getnode1");
	meshlink_destroy("getnode2");

	// Opening NUT and bar nodes
	meshlink_handle_t *mesh1 = meshlink_open("getnode1", "nut", "test", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh2 = meshlink_open("getnode2", "bar", "test", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
	meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Exporting and Importing mutually
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);
	bool imp1 = meshlink_import(mesh1, exp2);
	assert(imp1);
	bool imp2 = meshlink_import(mesh2, exp1);
	assert(imp2);

	// Get node handles
	meshlink_node_t *get_node = meshlink_get_node(mesh1, "bar");
	assert_int_not_equal(get_node, NULL);
	get_node = meshlink_get_node(mesh1, "nut");
	assert_int_not_equal(get_node, NULL);

	// Cleanup
	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("getnode1");
	meshlink_destroy("getnode2");
	return true;
}

/* Execute meshlink_get_node Test Case # 2 */
static void test_case_mesh_get_node_02(void **state) {
	execute_test(test_steps_mesh_get_node_02, state);
}

/* Test Steps for meshlink_get_node Test Case # 2

    Test Steps:
    1. Get node handles by passing NULL as mesh handle argument

    Expected Result:
    Reports error successfully by returning NULL
*/
static bool test_steps_mesh_get_node_02(void) {
	meshlink_node_t *get_node = meshlink_get_node(NULL, "foo");
	assert_int_equal(get_node, NULL);

	return true;
}

/* Execute meshlink_get_node Test Case # 3 */
static void test_case_mesh_get_node_03(void **state) {
	execute_test(test_steps_mesh_get_node_03, state);
}

/* Test Steps for meshlink_get_node Test Case # 3

    Test Steps:
    1. Get node handles by passing NULL as node name argument

    Expected Result:
    Reports error successfully by returning NULL
*/
static bool test_steps_mesh_get_node_03(void) {
	meshlink_handle_t *mesh = meshlink_open("node_conf.3", "foo", "test", DEV_CLASS_STATIONARY);
	assert(mesh);
	assert(meshlink_start(mesh));

	meshlink_node_t *get_node = meshlink_get_node(mesh, NULL);
	assert_int_equal(get_node, NULL);

	meshlink_close(mesh);
	meshlink_destroy("node_conf.3");
	return true;
}

/* Execute meshlink_get_node Test Case # 4 */
static void test_case_mesh_get_node_04(void **state) {
	execute_test(test_steps_mesh_get_node_04, state);
}

/* Test Steps for meshlink_get_node Test Case # 4

    Test Steps:
    1. Open node instance
    2. Get node handle with the name of the node
        that's not in the mesh

    Expected Result:
    Reports error successfully by returning NULL
*/
static bool test_steps_mesh_get_node_04(void) {
	meshlink_handle_t *mesh = meshlink_open("node_conf", "foo", "test", DEV_CLASS_STATIONARY);
	assert(mesh);
	assert(meshlink_start(mesh));

	const char *nonexisting_node = "bar";
	meshlink_node_t *get_node = meshlink_get_node(mesh, nonexisting_node);
	assert_int_equal(get_node, NULL);

	meshlink_close(mesh);
	meshlink_destroy("node_conf");
	return true;
}

int test_meshlink_get_node(void) {
	const struct CMUnitTest blackbox_get_node_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_node_01, NULL, NULL,
		(void *)&test_mesh_get_node_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_node_02, NULL, NULL,
		(void *)&test_mesh_get_node_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_node_03, NULL, NULL,
		(void *)&test_mesh_get_node_03_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_node_04, NULL, NULL,
		(void *)&test_mesh_get_node_04_state)
	};

	total_tests += sizeof(blackbox_get_node_tests) / sizeof(blackbox_get_node_tests[0]);

	return cmocka_run_group_tests(blackbox_get_node_tests, NULL, NULL);
}
