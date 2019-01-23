/*
    test_cases_start.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_start.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

static void test_case_mesh_start_01(void **state);
static bool test_steps_mesh_start_01(void);
static void test_case_mesh_start_02(void **state);
static bool test_steps_mesh_start_02(void);

/* State structure for meshlink_start Test Case #1 */
static black_box_state_t test_mesh_start_01_state = {
	.test_case_name = "test_case_mesh_start_01",
};

/* State structure for meshlink_start Test Case #2 */
static black_box_state_t test_mesh_start_02_state = {
	.test_case_name = "test_case_mesh_start_02",
};

/* Execute meshlink_start Test Case # 1*/
static void test_case_mesh_start_01(void **state) {
	execute_test(test_steps_mesh_start_01, state);
}

/* Test Steps for meshlink_start Test Case # 1

    Test Steps:
    1. Open Instance & start node

    Expected Result:
    Successfully node instance should be running
*/
static bool test_steps_mesh_start_01(void) {

	// Open instance

	bool result = false;
	meshlink_handle_t *mesh = meshlink_open("start_conf", "foo", "test", DEV_CLASS_STATIONARY);
	assert(mesh);

	// Run node instance

	result = meshlink_start(mesh);

	// Clean up
	meshlink_close(mesh);
	meshlink_destroy("start_conf");

	if(!result) {
		fprintf(stderr, "meshlink_start status1: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	} else {
		return true;
	}
}

/* Execute meshlink_start Test Case # 2*/
static void test_case_mesh_start_02(void **state) {
	execute_test(test_steps_mesh_start_02, state);
}

/* Test Steps for meshlink_start Test Case # 2

    Test Steps:
    1. Calling meshlink_start with NULL as mesh handle argument.

    Expected Result:
    meshlink_start API handles the invalid parameter by returning false.
*/
static bool test_steps_mesh_start_02(void) {
	bool result = false;
	meshlink_destroy("start_conf");
	meshlink_handle_t *mesh = meshlink_open("start_conf", "foo", "test", DEV_CLASS_STATIONARY);
	assert(mesh);

	// Run instance with NULL argument

	result = meshlink_start(NULL);
	assert_int_equal(result, true);

	// Clean up

	meshlink_close(mesh);
	meshlink_destroy("start_conf");
	return true;
}

int test_meshlink_start(void) {
	const struct CMUnitTest blackbox_start_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_start_01, NULL, NULL,
		(void *)&test_mesh_start_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_start_02, NULL, NULL,
		(void *)&test_mesh_start_02_state)

	};

	total_tests += sizeof(blackbox_start_tests) / sizeof(blackbox_start_tests[0]);

	return cmocka_run_group_tests(blackbox_start_tests, NULL, NULL);
}
