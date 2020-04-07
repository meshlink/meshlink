/*
    test_cases_open.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_open.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../../utils.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <linux/limits.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

#define NUT                         "nut"
#define PEER                        "peer"
#define TEST_MESHLINK_OPEN          "test_open"
#define create_path(confbase, node_name, test_case_no)   assert(snprintf(confbase, sizeof(confbase), TEST_MESHLINK_OPEN "_%ld_%s_%02d", (long) getpid(), node_name, test_case_no) > 0)

static void test_case_mesh_open_01(void **state);
static bool test_steps_mesh_open_01(void);
static void test_case_mesh_open_02(void **state);
static bool test_steps_mesh_open_02(void);
static void test_case_mesh_open_03(void **state);
static bool test_steps_mesh_open_03(void);
static void test_case_mesh_open_04(void **state);
static bool test_steps_mesh_open_04(void);
static void test_case_mesh_open_05(void **state);
static bool test_steps_mesh_open_05(void);
static void test_case_mesh_open_06(void **state);
static bool test_steps_mesh_open_06(void);
static void test_case_mesh_open_07(void **state);
static bool test_steps_mesh_open_07(void);

/* State structure for meshlink_open Test Case #1 */
static black_box_state_t test_mesh_open_01_state = {
	.test_case_name = "test_case_mesh_open_01",
};

/* State structure for meshlink_open Test Case #2 */
static black_box_state_t test_mesh_open_02_state = {
	.test_case_name = "test_case_mesh_open_02",
};

/* State structure for meshlink_open Test Case #3 */
static black_box_state_t test_mesh_open_03_state = {
	.test_case_name = "test_case_mesh_open_03",
};

/* State structure for meshlink_open Test Case #4 */
static black_box_state_t test_mesh_open_04_state = {
	.test_case_name = "test_case_mesh_open_04",
};

/* State structure for meshlink_open Test Case #5 */
static black_box_state_t test_mesh_open_05_state = {
	.test_case_name = "test_case_mesh_open_05",
};

/* State structure for meshlink_open Test Case #6 */
static black_box_state_t test_mesh_open_06_state = {
	.test_case_name = "test_case_mesh_open_06",
};

/* State structure for meshlink_open Test Case #7 */
static black_box_state_t test_mesh_open_07_state = {
	.test_case_name = "test_case_mesh_open_07",
};

/* Execute meshlink_open Test Case # 1*/
static void test_case_mesh_open_01(void **state) {
	execute_test(test_steps_mesh_open_01, state);
}

/* Test Steps for meshlink_open Test Case # 1

    Test Steps:
    1. Open the node instance using meshlink_open

    Expected Result:
    meshlink_open API should successfully return a mesh handle.
*/
static bool test_steps_mesh_open_01(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh = meshlink_open("open_conf", "foo", "test", DEV_CLASS_STATIONARY);
	assert_int_not_equal(mesh, NULL);

	meshlink_close(mesh);
	assert(meshlink_destroy("open_conf"));
	return true;
}

/* Execute meshlink_open Test Case # 2*/
static void test_case_mesh_open_02(void **state) {
	execute_test(test_steps_mesh_open_02, state);
}

/* Test Steps for meshlink_open Test Case # 2

    Test Steps:
    1. Open the node instance using meshlink_open with NULL as confbase argument

    Expected Result:
    meshlink_open API should successfully report error by returning NULL pointer
*/
static bool test_steps_mesh_open_02(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh = meshlink_open(NULL, "foo", "test", DEV_CLASS_STATIONARY);
	assert_int_equal(mesh, NULL);

	return true;
}

/* Execute meshlink_open Test Case # 3 */
static void test_case_mesh_open_03(void **state) {
	execute_test(test_steps_mesh_open_03, state);
}

/* Test Steps for meshlink_open Test Case # 3

    Test Steps:
    1. Open the node instance using meshlink_open with NULL as node name argument

    Expected Result:
    meshlink_open API should successfully report error by returning NULL pointer
*/
static bool test_steps_mesh_open_03(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh = meshlink_open("openconf", NULL, "test", DEV_CLASS_STATIONARY);
	assert_int_equal(mesh, NULL);

	assert(meshlink_destroy("open_conf"));
	return true;
}

/* Execute meshlink_open Test Case # 4*/
static void test_case_mesh_open_04(void **state) {
	execute_test(test_steps_mesh_open_04, state);
}

/* Test Steps for meshlink_open Test Case # 4

    Test Steps:
    1. Open the node instance using meshlink_open with NULL as app name argument

    Expected Result:
    meshlink_open API should successfully report error by returning NULL pointer
*/
static bool test_steps_mesh_open_04(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh = meshlink_open("openconf", "foo", NULL, DEV_CLASS_STATIONARY);
	assert_int_equal(mesh, NULL);

	assert(meshlink_destroy("open_conf"));
	return true;
}

/* Execute meshlink_open Test Case # 5*/
static void test_case_mesh_open_05(void **state) {
	execute_test(test_steps_mesh_open_05, state);
}

/* Test Steps for meshlink_open Test Case # 5

    Test Steps:
    1. Open the node instance using meshlink_open with invalid device class argument

    Expected Result:
    meshlink_open API should successfully report error by returning NULL pointer
*/
static bool test_steps_mesh_open_05(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh = meshlink_open("openconf", "foo", "test", -1);
	assert_int_equal(mesh, NULL);

	assert(meshlink_destroy("open_conf"));
	return true;
}

/* Execute meshlink_open Test Case # 7 - Atomicity testing
    Validate the meshlink_open behavior opened a new confbase and terminated immediately the open call.
*/
static void test_case_mesh_open_06(void **state) {
	execute_test(test_steps_mesh_open_06, state);
}

static bool test_steps_mesh_open_06(void) {
	bool status;
	pid_t pid;
	int pid_status;
	char nut_confbase[PATH_MAX];
	create_path(nut_confbase, NUT, 6);

	// Fork a new process in which NUT opens it's instance and raises SIGINT to terminate.

	pid = fork();
	assert_int_not_equal(pid, -1);

	if(!pid) {
		meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
		meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_OPEN, DEV_CLASS_STATIONARY);
		assert(mesh);
		raise(SIGINT);
	}

	// Wait for child exit and verify which signal terminated it

	assert_int_not_equal(waitpid(pid, &pid_status, 0), -1);
	assert_int_equal(WIFSIGNALED(pid_status), true);
	assert_int_equal(WTERMSIG(pid_status), SIGINT);

	// Reopen the NUT instance in the same test suite

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_OPEN, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);

	// Validate parameters that were used to open meshlink instance.

	assert_int_equal(strcmp(mesh->name, NUT), 0);
	meshlink_node_t *self = meshlink_get_self(mesh);
	assert_int_equal(strcmp(self->name, NUT), 0);
	assert_int_equal(meshlink_get_node_dev_class(mesh, self), DEV_CLASS_STATIONARY);

	// Cleanup

	meshlink_close(mesh);
	assert_true(meshlink_destroy(nut_confbase));
	return true;
}

/* Execute meshlink_open Test Case # 7 - Atomicity testing
    Validate the meshlink_open behavior opened an existing confbase and terminated immediately the open call.
*/
static void test_case_mesh_open_07(void **state) {
	execute_test(test_steps_mesh_open_07, state);
}

static bool test_steps_mesh_open_07(void) {
	bool status;
	pid_t pid;
	int pid_status;
	char nut_confbase[PATH_MAX];
	char peer_confbase[PATH_MAX];
	create_path(nut_confbase, NUT, 7);
	create_path(peer_confbase, PEER, 7);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_OPEN, DEV_CLASS_BACKBONE);
	assert_non_null(mesh);
	meshlink_handle_t *mesh_peer = meshlink_open(peer_confbase, PEER, TEST_MESHLINK_OPEN, DEV_CLASS_STATIONARY);
	assert_non_null(mesh_peer);

	// Exporting and Importing mutually
	char *export_data = meshlink_export(mesh);
	assert_non_null(export_data);
	assert_true(meshlink_import(mesh_peer, export_data));
	free(export_data);
	export_data = meshlink_export(mesh_peer);
	assert_non_null(export_data);
	assert_true(meshlink_import(mesh, export_data));
	free(export_data);

	meshlink_close(mesh);
	meshlink_close(mesh_peer);


	// Fork a new process in which NUT reopens it's instance and raises SIGINT to terminate.

	pid = fork();
	assert_int_not_equal(pid, -1);

	if(!pid) {
		meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
		meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_OPEN, DEV_CLASS_BACKBONE);
		assert(mesh);
		raise(SIGINT);
	}

	// Wait for child exit and verify which signal terminated it

	assert_int_not_equal(waitpid(pid, &pid_status, 0), -1);
	assert_int_equal(WIFSIGNALED(pid_status), true);
	assert_int_equal(WTERMSIG(pid_status), SIGINT);

	// Reopen the NUT instance in the same test suite

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_OPEN, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);

	// Validate parameters that were used to open meshlink instance.

	assert_int_equal(strcmp(mesh->name, NUT), 0);
	meshlink_node_t *self = meshlink_get_self(mesh);
	assert_int_equal(strcmp(self->name, NUT), 0);
	assert_int_equal(meshlink_get_node_dev_class(mesh, self), DEV_CLASS_STATIONARY);

	// Cleanup

	meshlink_close(mesh);
	assert_true(meshlink_destroy(nut_confbase));
	assert_true(meshlink_destroy(peer_confbase));
	return true;
}

int test_meshlink_open(void) {
	const struct CMUnitTest blackbox_open_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_01, NULL, NULL,
		                (void *)&test_mesh_open_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_02, NULL, NULL,
		                (void *)&test_mesh_open_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_03, NULL, NULL,
		                (void *)&test_mesh_open_03_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_04, NULL, NULL,
		                (void *)&test_mesh_open_04_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_05, NULL, NULL,
		                (void *)&test_mesh_open_05_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_06, NULL, NULL,
		                (void *)&test_mesh_open_06_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_07, NULL, NULL,
		                (void *)&test_mesh_open_07_state)

	};
	total_tests += sizeof(blackbox_open_tests) / sizeof(blackbox_open_tests[0]);

	return cmocka_run_group_tests(blackbox_open_tests, NULL, NULL);
}
