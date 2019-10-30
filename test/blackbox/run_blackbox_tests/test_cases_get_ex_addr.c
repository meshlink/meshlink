/*
    test_cases_get_ex_addr.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_get_ex_addr.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

static void test_case_mesh_get_address_01(void **state);
static bool test_steps_mesh_get_address_01(void);
static void test_case_mesh_get_address_02(void **state);
static bool test_steps_mesh_get_address_02(void);
static void test_case_mesh_get_address_03(void **state);
static bool test_steps_mesh_get_address_03(void);

/* State structure for meshlink_get_external_address Test Case #1 */
static black_box_state_t test_mesh_get_address_01_state = {
	.test_case_name = "test_case_mesh_get_address_01",
};

/* State structure for meshlink_get_external_address Test Case #2 */
static black_box_state_t test_mesh_get_address_02_state = {
	.test_case_name = "test_case_mesh_get_address_02",
};

/* State structure for meshlink_get_external_address Test Case #3 */
static black_box_state_t test_mesh_get_address_03_state = {
	.test_case_name = "test_case_mesh_get_address_03",
};

/* Execute meshlink_get_external_address Test Case # 1 */
static void test_case_mesh_get_address_01(void **state) {
	execute_test(test_steps_mesh_get_address_01, state);
}

/* Test Steps for meshlink_get_external_address Test Case # 1

    Test Steps:
    1. Create an instance of the node & start it
    2. Get node's external address using meshlink_get_external_address

    Expected Result:
    API returns the external address successfully.
*/
static bool test_steps_mesh_get_address_01(void) {
	meshlink_handle_t *mesh = meshlink_open("getex_conf", "foo", "test", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	assert(meshlink_start(mesh));

	char *addr = meshlink_get_external_address(mesh);
	assert_int_not_equal(addr, NULL);

	free(addr);
	meshlink_close(mesh);
	meshlink_destroy("getex_conf");
	return true;
}

/* Execute meshlink_get_external_address Test Case # 2 */
static void test_case_mesh_get_address_02(void **state) {
	execute_test(test_steps_mesh_get_address_02, state);
}

/* Test Steps for meshlink_get_external_address Test Case # 2

    Test Steps:
    1. Obtain external address by passing NULL as mesh handle
        to meshlink_get_external_address API

    Expected Result:
    Return NULL by reporting error successfully.
*/
static bool test_steps_mesh_get_address_02(void) {
	char *ext = meshlink_get_external_address(NULL);
	assert_int_equal(ext, NULL);

	return true;
}

/* Execute meshlink_get_external_address Test Case # 3 */
static void test_case_mesh_get_address_03(void **state) {
	execute_test(test_steps_mesh_get_address_03, state);
}

/* Test Steps for meshlink_get_external_address Test Case # 3 - Functionality test

    Test Steps:
    1. Create an instance of the node
    2. Get node's external address using meshlink_get_external_address

    Expected Result:
    API returns the external address successfully even if the mesh is started.
*/
static bool test_steps_mesh_get_address_03(void) {
	meshlink_handle_t *mesh = meshlink_open("getex_conf", "foo", "test", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	assert(meshlink_start(mesh));

	char *addr = meshlink_get_external_address(mesh);
	assert_int_not_equal(addr, NULL);

	free(addr);
	meshlink_close(mesh);
	meshlink_destroy("getex_conf");
	return true;
}

int test_meshlink_get_external_address(void) {
	const struct CMUnitTest blackbox_get_ex_addr_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_address_01, NULL, NULL,
		                (void *)&test_mesh_get_address_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_address_02, NULL, NULL,
		                (void *)&test_mesh_get_address_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_address_03, NULL, NULL,
		                (void *)&test_mesh_get_address_03_state)
	};
	total_tests += sizeof(blackbox_get_ex_addr_tests) / sizeof(blackbox_get_ex_addr_tests[0]);

	return cmocka_run_group_tests(blackbox_get_ex_addr_tests, NULL, NULL);
}
