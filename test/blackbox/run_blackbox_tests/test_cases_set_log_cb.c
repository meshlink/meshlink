/*
    test_cases_set_log_cb.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_set_log_cb.h"
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

static void test_case_set_log_cb_01(void **state);
static bool test_set_log_cb_01(void);
static void test_case_set_log_cb_02(void **state);
static bool test_set_log_cb_02(void);

/* log variable gives access to the log callback to know whether invoked or not */
static bool log;

/* State structure for log callback Test Case #1 */
static black_box_state_t test_case_set_log_cb_01_state = {
	.test_case_name = "test_case_set_log_cb_01",
};

/* State structure for log callback Test Case #2 */
static black_box_state_t test_case_set_log_cb_02_state = {
	.test_case_name = "test_case_set_log_cb_02",
};


/* log callback */
static void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	(void)mesh;
	(void)level;

	fprintf(stderr, "Received log text : %s\n", text);
	log = true;
}

/* Execute meshlink_set_log_cb Test Case # 1 - Valid case */
static void test_case_set_log_cb_01(void **state) {
	execute_test(test_set_log_cb_01, state);
}
/* Test Steps for meshlink_set_receive_cb Test Case # 1

    Test Steps:
    1. Run relay and Open NUT
    2. Set log callback for the NUT and Start NUT

    Expected Result:
    log callback should be invoked when NUT joins with relay.
*/
static bool test_set_log_cb_01(void) {
	assert(meshlink_destroy("logconf"));

	// Create meshlink instance for NUT

	meshlink_handle_t *mesh = meshlink_open("logconf", "nut", "test", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);

	// Set up logging for Meshlink with the newly acquired Mesh Handle

	log = false;
	meshlink_set_log_cb(mesh, TEST_MESHLINK_LOG_LEVEL, log_cb);

	// Starting node to log

	bool mesh_start = meshlink_start(mesh);
	assert(mesh_start);

	bool ret = log;

	assert_int_equal(ret, true);

	// closing meshes and destroying confbase

	meshlink_close(mesh);
	assert(meshlink_destroy("logconf"));

	return true;
}

/* Execute meshlink_set_log_cb Test Case # 2 - Invalid case */
static void test_case_set_log_cb_02(void **state) {
	execute_test(test_set_log_cb_02, state);
}
/* Test Steps for meshlink_set_poll_cb Test Case # 2

    Test Steps:
    1. Calling meshlink_set_poll_cb with some invalid integer other than the valid enums.

    Expected Result:
    set poll callback handles the invalid parameter when called by giving proper error number.
*/
static bool test_set_log_cb_02(void) {

	// Setting an invalid level

	meshlink_set_log_cb(NULL, 1000, NULL);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	return true;
}


int test_meshlink_set_log_cb(void) {
	const struct CMUnitTest blackbox_log_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_set_log_cb_01, NULL, NULL,
		                (void *)&test_case_set_log_cb_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_set_log_cb_02, NULL, NULL,
		                (void *)&test_case_set_log_cb_02_state)
	};
	total_tests += sizeof(blackbox_log_tests) / sizeof(blackbox_log_tests[0]);

	int failed = cmocka_run_group_tests(blackbox_log_tests, NULL, NULL);

	return failed;
}
