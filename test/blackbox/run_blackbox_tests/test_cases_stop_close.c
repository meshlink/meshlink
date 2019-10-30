/*
    test_cases_stop_close.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_stop_close.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>
#include <sys/wait.h>

#define CLOSE_FILE_PATH "/home/sairoop/meshlink/test/blackbox/test_case_close/mesh_close"
#define VALGRIND_LOG "valgrind.log"

static void test_case_mesh_close_01(void **state);
static bool test_steps_mesh_close_01(void);
static void test_case_mesh_stop_01(void **state);
static bool test_steps_mesh_stop_01(void);

/* State structure for meshlink_close Test Case #1 */
static black_box_state_t test_mesh_close_01_state = {
	.test_case_name = "test_case_mesh_close_01",
};

/* State structure for meshlink_close Test Case #1 */
static black_box_state_t test_mesh_stop_01_state = {
	.test_case_name = "test_case_mesh_stop_01",
};

/* Execute meshlink_close Test Case # 1*/
static void test_case_mesh_close_01(void **state) {
	execute_test(test_steps_mesh_close_01, state);
}

/* Test Steps for meshlink_close Test Case # 1*/

static bool test_steps_mesh_close_01(void) {
	meshlink_close(NULL);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	return true;
}

/* Execute meshlink_stop Test Case # 1*/
static void test_case_mesh_stop_01(void **state) {
	execute_test(test_steps_mesh_stop_01, state);
}

/* Test Steps for meshlink_stop Test Case # 1*/
static bool test_steps_mesh_stop_01(void) {
	meshlink_stop(NULL);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	return true;
}

int test_meshlink_stop_close(void) {
	const struct CMUnitTest blackbox_stop_close_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_stop_01, NULL, NULL,
		                (void *)&test_mesh_stop_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_close_01, NULL, NULL,
		                (void *)&test_mesh_close_01_state)
	};

	total_tests += sizeof(blackbox_stop_close_tests) / sizeof(blackbox_stop_close_tests[0]);

	return cmocka_run_group_tests(blackbox_stop_close_tests, NULL, NULL);
}

