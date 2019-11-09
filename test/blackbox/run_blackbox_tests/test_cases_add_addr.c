/*
    test_cases_add_addr.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_add_addr.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>

static void test_case_mesh_add_address_01(void **state);
static bool test_steps_mesh_add_address_01(void);
static void test_case_mesh_add_address_02(void **state);
static bool test_steps_mesh_add_address_02(void);
static void test_case_mesh_add_address_03(void **state);
static bool test_steps_mesh_add_address_03(void);

/* State structure for meshlink_add_address Test Case #1 */
static black_box_state_t test_mesh_add_address_01_state = {
	.test_case_name = "test_case_mesh_add_address_01",
};

/* State structure for meshlink_add_address Test Case #2 */
static black_box_state_t test_mesh_add_address_02_state = {
	.test_case_name = "test_case_mesh_add_address_02",
};

/* State structure for meshlink_add_address Test Case #3 */
static black_box_state_t test_mesh_add_address_03_state = {
	.test_case_name = "test_case_mesh_add_address_03",
};

/* Execute meshlink_add_address Test Case # 1 */
static void test_case_mesh_add_address_01(void **state) {
	execute_test(test_steps_mesh_add_address_01, state);
}

/* Test Steps for meshlink_add_address Test Case # 1

    Test Steps:
    1. Create node instance
    2. Add an address to the host node
    2. Open host file from confbase & verify address being added

    Expected Result:
    meshlink_add_address API adds the new address given to it's confbase
*/
static bool test_steps_mesh_add_address_01(void) {
	char *node = "foo";
	assert(meshlink_destroy("add_conf.1"));

	// Create node instance
	meshlink_handle_t *mesh = meshlink_open("add_conf.1", node, "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);

	char *hostname = "localhost";
	bool ret = meshlink_add_address(mesh, hostname);
	assert_int_equal(ret, true);

	// Open the foo host file from confbase to verify address being added
	bool found = false;
	FILE *fp = fopen("./add_conf.1/hosts/foo", "r");
	assert(fp);
	char line[100];

	while(fgets(line, 100, fp) != NULL) {
		if(strcasestr(line, "Address") && strcasestr(line, hostname)) {
			found = true;
		}
	}

	assert(!fclose(fp));

	assert_int_equal(found, true);

	// Clean up
	meshlink_close(mesh);
	assert(meshlink_destroy("add_conf.1"));
	return true;
}

/* Execute meshlink_add_address Test Case # 2 */
static void test_case_mesh_add_address_02(void **state) {
	execute_test(test_steps_mesh_add_address_02, state);
}

/* Test Steps for meshlink_add_address Test Case # 2

    Test Steps:
    1. Create node instance
    2. Call meshlink_add_address API using NULL as mesh handle argument

    Expected Result:
    meshlink_add_address API returns false by reporting error successfully.
*/
static bool test_steps_mesh_add_address_02(void) {
	// Passing NULL as mesh handle argument to meshlink_add_address API
	bool result = meshlink_add_address(NULL, "localhost");
	assert_int_equal(result, false);

	return true;
}

/* Execute meshlink_add_address Test Case # 3 */
static void test_case_mesh_add_address_03(void **state) {
	execute_test(test_steps_mesh_add_address_03, state);
}

/* Test Steps for meshlink_add_address Test Case # 3

    Test Steps:
    1. Create node instance
    2. Call meshlink_add_address API using NULL as address argument

    Expected Result:
    meshlink_add_address API returns false by reporting error successfully.
*/
static bool test_steps_mesh_add_address_03(void) {
	assert(meshlink_destroy("add_conf.3"));

	// Create node instance
	meshlink_handle_t *mesh = meshlink_open("add_conf.3", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);

	bool result = meshlink_add_address(mesh, NULL);
	assert_int_equal(result, false);

	meshlink_close(mesh);
	assert(meshlink_destroy("add_conf.3"));
	return true;
}

int test_meshlink_add_address(void) {
	const struct CMUnitTest blackbox_add_addr_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_add_address_01, NULL, NULL,
		                (void *)&test_mesh_add_address_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_add_address_02, NULL, NULL,
		                (void *)&test_mesh_add_address_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_add_address_03, NULL, NULL,
		                (void *)&test_mesh_add_address_03_state)
	};

	total_tests += sizeof(blackbox_add_addr_tests) / sizeof(blackbox_add_addr_tests[0]);

	return cmocka_run_group_tests(blackbox_add_addr_tests, NULL, NULL);
}

