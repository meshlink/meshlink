/*
    test_cases_rec_cb.c -- Execution of specific meshlink black box test cases
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
#include "test_cases.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "test_cases_rec_cb.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <pthread.h>


/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_set_rec_cb_01(void **state);
static bool test_set_rec_cb_01(void);
static void test_case_set_rec_cb_02(void **state);
static bool test_set_rec_cb_02(void);
static void test_case_set_rec_cb_03(void **state);
static bool test_set_rec_cb_03(void);
static void test_case_set_rec_cb_04(void **state);
static bool test_set_rec_cb_04(void);

/* Test Steps for meshlink_set_receive_cb Test Case #1 */
static black_box_state_t test_case_set_rec_cb_01_state = {
	.test_case_name = "test_case_set_rec_cb_01",
};

/* Test Steps for meshlink_set_receive_cb Test Case #2 */
static black_box_state_t test_case_set_rec_cb_02_state = {
	.test_case_name = "test_case_set_rec_cb_02",
};

/* Test Steps for meshlink_set_receive_cb Test Case #3 */
static black_box_state_t test_case_set_rec_cb_03_state = {
	.test_case_name = "test_case_set_rec_cb_03",
};

static bool received;

/* mutex for the common variable */
pthread_mutex_t lock;

/* receive callback function */
static void rec_cb(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len) {
	assert(len);

	pthread_mutex_lock(&lock);

	if(len == 5 && !memcmp(data, "test", 5)) {
		received = true;
	}

	pthread_mutex_unlock(&lock);
	return;
}

/* Execute meshlink_set_receive_cb Test Case # 1 - Valid case */
static void test_case_set_rec_cb_01(void **state) {
	execute_test(test_set_rec_cb_01, state);
	return;
}
/* Test Steps for meshlink_set_receive_cb Test Case # 1

    Test Steps:
    1. Open NUT
    2. Set receive callback for the NUT
    3. Echo NUT with some data.

    Expected Result:
    Receive callback should be invoked when NUT echoes or sends data for itself.
*/
static bool test_set_rec_cb_01(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("set_receive_cb_conf", "nut", "test", 1);
	assert(mesh_handle);
	meshlink_set_receive_cb(mesh_handle, rec_cb);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	assert(meshlink_start(mesh_handle));
	sleep(1);

	pthread_mutex_lock(&lock);
	received = false;
	pthread_mutex_unlock(&lock);
	meshlink_node_t *node_handle = meshlink_get_self(mesh_handle);
	assert(node_handle);
	assert(meshlink_send(mesh_handle, node_handle, "test", 5));
	sleep(1);

	pthread_mutex_lock(&lock);
	bool ret = received;
	pthread_mutex_unlock(&lock);

	meshlink_close(mesh_handle);
	meshlink_destroy("set_receive_cb_conf");

	if(ret) {
		PRINT_TEST_CASE_MSG("Invoked callback\n");
		return true;
	} else {
		PRINT_TEST_CASE_MSG("No callback invoked\n");
		return false;
	}
}


/* Execute meshlink_set_receive_cb Test Case # 2 - Invalid case */
static void test_case_set_rec_cb_02(void **state) {
	execute_test(test_set_rec_cb_02, state);
	return;
}
/* Test Steps for meshlink_set_receive_cb Test Case # 2

    Test Steps:
    1. Call meshlink_set_receive_cb with NULL as mesh handle argument

    Expected Result:
    meshlink_set_receive_cb API reports proper error accordingly.
*/
static bool test_set_rec_cb_02(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Setting receive callback with NULL as mesh handle
	meshlink_set_receive_cb(NULL, rec_cb);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	return true;
}

/* Execute meshlink_set_receive_cb Test Case # 3 - Functionality Test, Trying to set receive call back after
      starting the mesh */
static void test_case_set_rec_cb_03(void **state) {
	execute_test(test_set_rec_cb_03, state);
	return;
}
/* Test Steps for meshlink_set_receive_cb Test Case # 3

    Test Steps:
    1. Open NUT
    2. Starting mesh
    2. Set receive callback for the NUT
    3. Echo NUT with some data.

    Expected Result:
    Receive callback can be invoked when NUT echoes or sends data for itself
*/
static bool test_set_rec_cb_03(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("set_receive_cb_conf", "nut", "test", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	assert(meshlink_start(mesh_handle));
	sleep(1);
	meshlink_set_receive_cb(mesh_handle, rec_cb);

	pthread_mutex_lock(&lock);
	received = false;
	pthread_mutex_unlock(&lock);
	meshlink_node_t *node_handle = meshlink_get_self(mesh_handle);
	assert(node_handle);
	assert(meshlink_send(mesh_handle, node_handle, "test", 5));
	sleep(1);

	pthread_mutex_lock(&lock);
	bool ret = received;
	pthread_mutex_unlock(&lock);

	meshlink_close(mesh_handle);
	meshlink_destroy("set_receive_cb_conf");

	if(ret) {
		PRINT_TEST_CASE_MSG("Invoked callback\n");
		return true;
	} else {
		PRINT_TEST_CASE_MSG("No callback invoked\n");
		return false;
	}
}

int test_meshlink_set_receive_cb(void) {
	const struct CMUnitTest blackbox_receive_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_set_rec_cb_01, NULL, NULL,
		(void *)&test_case_set_rec_cb_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_set_rec_cb_02, NULL, NULL,
		(void *)&test_case_set_rec_cb_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_set_rec_cb_03, NULL, NULL,
		(void *)&test_case_set_rec_cb_03_state)
	};
	total_tests += sizeof(blackbox_receive_tests) / sizeof(blackbox_receive_tests[0]);

	assert(pthread_mutex_init(&lock, NULL) == 0);
	int failed = cmocka_run_group_tests(blackbox_receive_tests , NULL , NULL);
	assert(pthread_mutex_destroy(&lock) == 0);

	return failed;
}
