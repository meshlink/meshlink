/*
    test_cases_status_cb.c -- Execution of specific meshlink black box test cases
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

#ifdef NDEBUG
#undef NDEBUG
#endif

#include "execute_tests.h"
#include "test_cases_status_cb.h"
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

static void test_case_set_status_cb_01(void **state);
static bool test_set_status_cb_01(void);
static void test_case_set_status_cb_02(void **state);
static bool test_set_status_cb_02(void);

/* status variable gives access to the status callback to know whether invoked or not */
static bool status;

/* State structure for status callback Test Case #1 */
static black_box_state_t test_case_set_status_cb_01_state = {
	.test_case_name = "test_case_set_status_cb_01",
};

/* State structure for status callback Test Case #2 */
static black_box_state_t test_case_set_status_cb_02_state = {
	.test_case_name = "test_case_set_status_cb_02",
};


static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *source, bool reach) {
	(void)mesh;

	fprintf(stderr, "In status callback\n");

	if(reach) {
		fprintf(stderr, "[ %s ] node reachable\n", source->name);
	} else {
		fprintf(stderr, "[ %s ] node not reachable\n", source->name) ;
	}

	status = reach;
}

/* Execute status callback Test Case # 1 - valid case */
static void test_case_set_status_cb_01(void **state) {
	execute_test(test_set_status_cb_01, state);
}

/* Test Steps for meshlink_set_status_cb Test Case # 1

    Test Steps:
    1. Run bar and nut node instances
    2. Set status callback for the NUT and Start NUT

    Expected Result:
    status callback should be invoked when NUT connects/disconnects with 'relay' node.
*/
static bool test_set_status_cb_01(void) {
	assert(meshlink_destroy("set_status_cb_conf.1"));
	assert(meshlink_destroy("set_status_cb_conf.2"));

	// Opening NUT and bar nodes
	meshlink_handle_t *mesh1 = meshlink_open("set_status_cb_conf.1", "nut", "test", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh2 = meshlink_open("set_status_cb_conf.2", "bar", "test", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
	meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Set up callback for node status
	meshlink_set_node_status_cb(mesh1, status_cb);

	// Exporting and Importing mutually
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);
	assert(meshlink_import(mesh1, exp2));
	assert(meshlink_import(mesh2, exp1));

	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));
	sleep(1);

	// Test for status from status callback
	assert_int_equal(status, true);

	meshlink_close(mesh2);
	sleep(1);

	// Test for status from status callback
	assert_int_equal(status, false);

	free(exp1);
	free(exp2);
	meshlink_close(mesh1);
	assert(meshlink_destroy("set_status_cb_conf.1"));
	assert(meshlink_destroy("set_status_cb_conf.2"));

	return true;
}

/* Execute status callback Test Case # 2 - Invalid case */
static void test_case_set_status_cb_02(void **state) {
	execute_test(test_set_status_cb_02, state);
}

/* Test Steps for meshlink_set_status_cb Test Case # 2

    Test Steps:
    1. Calling meshlink_set_status_cb with NULL as mesh handle argument.

    Expected Result:
    set poll callback handles the invalid parameter when called by giving proper error number.
*/
static bool test_set_status_cb_02(void) {

	// Create meshlink instance

	assert(meshlink_destroy("set_status_cb_conf.3"));
	meshlink_handle_t *mesh_handle = meshlink_open("set_status_cb_conf.3", "nut", "node_sim", 1);
	assert(mesh_handle);

	// Pass NULL as meshlink_set_node_status_cb's argument

	meshlink_set_node_status_cb(NULL, status_cb);
	meshlink_errno_t meshlink_errno_buff = meshlink_errno;
	assert_int_equal(meshlink_errno_buff, MESHLINK_EINVAL);

	// Clean up

	meshlink_close(mesh_handle);
	assert(meshlink_destroy("set_status_cb_conf.3"));
	return true;
}


int test_meshlink_set_status_cb(void) {
	const struct CMUnitTest blackbox_status_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_set_status_cb_01, NULL, NULL,
		                (void *)&test_case_set_status_cb_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_set_status_cb_02, NULL, NULL,
		                (void *)&test_case_set_status_cb_02_state)
	};
	total_tests += sizeof(blackbox_status_tests) / sizeof(blackbox_status_tests[0]);

	int failed = cmocka_run_group_tests(blackbox_status_tests, NULL, NULL);

	return failed;
}
