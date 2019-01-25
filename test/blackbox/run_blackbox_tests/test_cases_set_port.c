/*
    test_cases_set_port.c -- Execution of specific meshlink black box test cases
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

#include "execute_tests.h"
#include "test_cases_destroy.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "test_cases_set_port.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>


/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_set_port_01(void **state);
static bool test_set_port_01(void);
static void test_case_set_port_02(void **state);
static bool test_set_port_02(void);
static void test_case_set_port_03(void **state);
static bool test_set_port_03(void);

/* State structure for set port API Test Case #1 */
static black_box_state_t test_case_set_port_01_state = {
	.test_case_name = "test_case_set_port_01",
};
/* State structure for set port API Test Case #2 */
static black_box_state_t test_case_set_port_02_state = {
	.test_case_name = "test_case_set_port_02",
};
/* State structure for set port API Test Case #3 */
static black_box_state_t test_case_set_port_03_state = {
	.test_case_name = "test_case_set_port_03",
};


/* Execute meshlink_set_port Test Case # 1 - valid case*/
static void test_case_set_port_01(void **state) {
	execute_test(test_set_port_01, state);
}
/* Test Steps for meshlink_set_port Test Case # 1 - Valid case

    Test Steps:
    1. Open NUT(Node Under Test)
    2. Set Port for NUT

    Expected Result:
    Set the new port to the NUT.
*/
static bool test_set_port_01(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance

	mesh_handle = meshlink_open("setportconf", "nut", "test", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Get old port and set a new port number

	int port;
	port = meshlink_get_port(mesh_handle);
	assert(port > 0);
	bool ret = meshlink_set_port(mesh_handle, 8000);
	port = meshlink_get_port(mesh_handle);

	assert_int_equal(port, 8000);
	assert_int_equal(ret, true);

	// Clean up

	meshlink_close(mesh_handle);
	meshlink_destroy("setportconf");
	return true;
}

/* Execute meshlink_set_port Test Case # 2 - Invalid case*/
static void test_case_set_port_02(void **state) {
	execute_test(test_set_port_02, state);
}

/* Test Steps for meshlink_set_port Test Case # 2 - Invalid case

    Test Steps:
    1. Pass NULL as mesh handle argument for meshlink_set_port

    Expected Result:
    Report false indicating error.
*/
static bool test_set_port_02(void) {
	// meshlink_set_port called using NULL as mesh handle

	bool ret = meshlink_set_port(NULL, 8000);
	assert_int_equal(meshlink_errno, 0);
	assert_int_equal(ret, false);

	return false;
}


/* Execute meshlink_set_port Test Case # 3 - Setting port after starting mesh*/
static void test_case_set_port_03(void **state) {
	execute_test(test_set_port_03, state);
}

/* Test Steps for meshlink_set_port Test Case # 2 - functionality test

    Test Steps:
    1. Open and start NUT and then try to set new port number

    Expected Result:
    New port number cannot be set while mesh is running.
*/
static bool test_set_port_03(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance

	mesh_handle = meshlink_open("getportconf", "nut", "test", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Setting port after starting NUT
	bool ret = meshlink_set_port(mesh_handle, 50000);
	assert_int_equal(meshlink_errno, 0);
	assert_int_equal(ret, false);

	// Clean up

	meshlink_close(mesh_handle);
	meshlink_destroy("getportconf");
}


int test_meshlink_set_port(void) {
	const struct CMUnitTest blackbox_set_port_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_set_port_01, NULL, NULL,
		(void *)&test_case_set_port_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_set_port_02, NULL, NULL,
		(void *)&test_case_set_port_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_set_port_03, NULL, NULL,
		(void *)&test_case_set_port_03_state)
	};
	total_tests += sizeof(blackbox_set_port_tests) / sizeof(blackbox_set_port_tests[0]);
	return cmocka_run_group_tests(blackbox_set_port_tests, NULL, NULL);
}
