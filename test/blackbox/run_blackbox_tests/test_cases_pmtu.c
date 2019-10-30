/*
    test_cases_pmtu.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_pmtu.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

static void test_case_mesh_pmtu_01(void **state);
static bool test_steps_mesh_pmtu_01(void);
static void test_case_mesh_pmtu_02(void **state);
static bool test_steps_mesh_pmtu_02(void);
static void test_case_mesh_pmtu_03(void **state);
static bool test_steps_mesh_pmtu_03(void);

/* State structure for meshlink_get_pmtu Test Case #1 */
static black_box_state_t test_mesh_pmtu_01_state = {
	.test_case_name = "test_case_mesh_pmtu_01",
};

/* State structure for meshlink_get_pmtu Test Case #2 */
static black_box_state_t test_mesh_pmtu_02_state = {
	.test_case_name = "test_case_mesh_pmtu_02",
};

/* State structure for meshlink_get_pmtu Test Case #3 */
static black_box_state_t test_mesh_pmtu_03_state = {
	.test_case_name = "test_case_mesh_pmtu_03",
};

/* Execute meshlink_get_pmtu Test Case # 1 */
static void test_case_mesh_pmtu_01(void **state) {
	execute_test(test_steps_mesh_pmtu_01, state);
}

/* Test Steps for meshlink_get_pmtu Test Case # 1

    Test Steps:
    1. Create node instance & get self handle
    2. Obtain MTU size

    Expected Result:
    meshlink_get_pmtu should return valid MTU size of a node
*/
static bool test_steps_mesh_pmtu_01(void) {
	meshlink_handle_t *mesh = meshlink_open("pmtu_conf", "foo", "test", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);

	assert(meshlink_start(mesh));
	meshlink_node_t *dest_node = meshlink_get_self(mesh);
	assert(dest_node != NULL);

	ssize_t pmtu = meshlink_get_pmtu(mesh, dest_node);
	assert_int_not_equal(pmtu, -1);

	meshlink_close(mesh);
	meshlink_destroy("pmtu_conf");
	return true;
}

/* Execute meshlink_get_pmtu Test Case # 2

    Test Steps:
    1. Create node instance & get self handle
    2. Try to obtain MTU size by passing NULL as mesh handle to API

    Expected Result:
    meshlink_get_pmtu should return -1 reporting the error
*/
static void test_case_mesh_pmtu_02(void **state) {
	execute_test(test_steps_mesh_pmtu_02, state);
}

/* Test Steps for meshlink_get_pmtu Test Case # 2*/
static bool test_steps_mesh_pmtu_02(void) {
	meshlink_handle_t *mesh = meshlink_open("pmtu_conf", "foo", "test", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);

	assert(meshlink_start(mesh));
	meshlink_node_t *dest_node = meshlink_get_self(mesh);
	assert(dest_node != NULL);

	ssize_t pmtu = meshlink_get_pmtu(NULL, dest_node);
	assert_int_equal(pmtu, -1);

	meshlink_close(mesh);
	meshlink_destroy("pmtu_conf");
	return true;
}

/* Execute meshlink_get_pmtu Test Case # 3 */
static void test_case_mesh_pmtu_03(void **state) {
	execute_test(test_steps_mesh_pmtu_03, state);
}

/* Test Steps for meshlink_get_pmtu Test Case # 3

    Test Steps:
    1. Create node instance & get self handle
    2. Try to obtain MTU size by passing NULL as node handle to API

    Expected Result:
    meshlink_get_pmtu should return -1 reporting the error
*/
static bool test_steps_mesh_pmtu_03(void) {
	meshlink_handle_t *mesh = meshlink_open("pmtu_conf", "foo", "test", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);

	assert(meshlink_start(mesh));

	ssize_t pmtu = meshlink_get_pmtu(mesh, NULL);
	assert_int_equal(pmtu, -1);

	meshlink_close(mesh);
	meshlink_destroy("pmtu_conf");
	return true;
}

int test_meshlink_pmtu(void) {
	const struct CMUnitTest blackbox_pmtu_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_pmtu_01, NULL, NULL,
		                (void *)&test_mesh_pmtu_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_pmtu_02, NULL, NULL,
		                (void *)&test_mesh_pmtu_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_pmtu_03, NULL, NULL,
		                (void *)&test_mesh_pmtu_03_state)
	};

	total_tests += sizeof(blackbox_pmtu_tests) / sizeof(blackbox_pmtu_tests[0]);

	return cmocka_run_group_tests(blackbox_pmtu_tests, NULL, NULL);
}
