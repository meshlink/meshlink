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
#include "test_cases_get_self.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

static void test_case_mesh_get_self_01(void **state);
static bool test_steps_mesh_get_self_01(void);
static void test_case_mesh_get_self_02(void **state);
static bool test_steps_mesh_get_self_02(void);

/* State structure for meshlink_get_self Test Case #1 */
static black_box_state_t test_mesh_get_self_01_state = {
	.test_case_name = "test_case_mesh_get_self_01",
};

/* State structure for meshlink_get_self Test Case #2 */
static black_box_state_t test_mesh_get_self_02_state = {
	.test_case_name = "test_case_mesh_get_self_02",
};

/* Execute meshlink_get_self Test Case # 1 */
static void test_case_mesh_get_self_01(void **state) {
	execute_test(test_steps_mesh_get_self_01, state);
	return;
}

/* Test Steps for meshlink_get_self Test Case # 1

    Test Steps:
    1. Open node instance
    2. Get node's self handle

    Expected Result:
    node handle of it's own is obtained
*/
static bool test_steps_mesh_get_self_01(void) {
	meshlink_handle_t *mesh = meshlink_open("self_conf", "foo", "test", DEV_CLASS_STATIONARY);
	assert(mesh);

	assert(meshlink_start(mesh));
	meshlink_node_t *dest_node = meshlink_get_self(mesh);
	assert_int_not_equal(dest_node, NULL);

	if(strcmp(dest_node->name, "foo")) {
		return false;
	}

	meshlink_close(mesh);
	meshlink_destroy("self_conf");
	return true;

}

/* Execute meshlink_get_self Test Case # 2 */
static void test_case_mesh_get_self_02(void **state) {
	execute_test(test_steps_mesh_get_self_02, state);
	return;
}

/* Test Steps for meshlink_get_self Test Case # 2

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Export and Import mutually

    Expected Result:
    Both the nodes imports successfully
*/
static bool test_steps_mesh_get_self_02(void) {
	meshlink_node_t *dest_node = meshlink_get_self(NULL);
	assert_int_equal(dest_node, NULL);

	return true;
}

int test_meshlink_get_self(void) {
	const struct CMUnitTest blackbox_get_self_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_self_01, NULL, NULL,
		(void *)&test_mesh_get_self_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_self_02, NULL, NULL,
		(void *)&test_mesh_get_self_02_state)
	};

	total_tests += sizeof(blackbox_get_self_tests) / sizeof(blackbox_get_self_tests[0]);

	return cmocka_run_group_tests(blackbox_get_self_tests, NULL, NULL);
}
