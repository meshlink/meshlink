/*
    test_cases_get_port.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_get_port.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

static void test_case_mesh_get_port_01(void **state);
static bool test_steps_mesh_get_port_01(void);
static void test_case_mesh_get_port_02(void **state);
static bool test_steps_mesh_get_port_02(void);

/* State structure for meshlink_get_port Test Case #1 */
static black_box_state_t test_mesh_get_port_01_state = {
	.test_case_name = "test_case_mesh_get_port_01",
};

/* State structure for meshlink_get_port Test Case #2 */
static black_box_state_t test_mesh_get_port_02_state = {
	.test_case_name = "test_case_mesh_get_port_02",
};

/* Execute meshlink_get_port Test Case # 1 */
static void test_case_mesh_get_port_01(void **state) {
	execute_test(test_steps_mesh_get_port_01, state);
}

/* Test Steps for meshlink_get_port Test Case # 1

    Test Steps:
    1. Open node instance
    2. Run the node instance
    3. Obtain port of that mesh using meshlink_get_port API

    Expected Result:
    API returns valid port number.
*/
static bool test_steps_mesh_get_port_01(void) {
	meshlink_handle_t *mesh = meshlink_open("port_conf", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh);
	assert(meshlink_start(mesh));

	int port = meshlink_get_port(mesh);
	assert_int_not_equal(port, -1);

	meshlink_close(mesh);
	meshlink_destroy("port_conf");
	return true;
}

/* Execute meshlink_get_port Test Case # 2 */
static void test_case_mesh_get_port_02(void **state) {
	execute_test(test_steps_mesh_get_port_02, state);
}

/* Test Steps for meshlink_get_port Test Case # 2 - Invalid case

    Test Steps:
    1. Pass NULL as mesh handle argument to meshlink_get_port API

    Expected Result:
    Reports error successfully by returning -1
*/
static bool test_steps_mesh_get_port_02(void) {
	int port = meshlink_get_port(NULL);
	assert_int_equal(port, -1);

	return true;
}

int test_meshlink_get_port(void) {
	const struct CMUnitTest blackbox_get_port_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_port_01, NULL, NULL,
		(void *)&test_mesh_get_port_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_port_02, NULL, NULL,
		(void *)&test_mesh_get_port_02_state)
	};

	total_tests += sizeof(blackbox_get_port_tests) / sizeof(blackbox_get_port_tests[0]);

	return cmocka_run_group_tests(blackbox_get_port_tests, NULL, NULL);
}
