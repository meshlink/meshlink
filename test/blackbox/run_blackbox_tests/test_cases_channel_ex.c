/*
    test_cases_channel_ex.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_channel_ex.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <pthread.h>
#include <cmocka.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG
/* Modify this to change the port number */
#define PORT 8000

static void test_case_channel_ex_01(void **state);
static bool test_steps_channel_ex_01(void);
static void test_case_channel_ex_02(void **state);
static bool test_steps_channel_ex_02(void);
static void test_case_channel_ex_03(void **state);
static bool test_steps_channel_ex_03(void);
static void test_case_channel_ex_04(void **state);
static bool test_steps_channel_ex_04(void);
static void test_case_channel_ex_05(void **state);
static bool test_steps_channel_ex_05(void);
static void test_case_channel_ex_06(void **state);
static bool test_steps_channel_ex_06(void);

static void cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len);
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len);

/* channel_acc gives us access to test whether the accept callback has been invoked or not */
static bool channel_acc;
/* mutex for the common variable */
pthread_mutex_t lock;

static black_box_state_t test_case_channel_ex_01_state = {
	.test_case_name = "test_case_channel_ex_01",
};
static black_box_state_t test_case_channel_ex_02_state = {
	.test_case_name = "test_case_channel_ex_02",
};
static black_box_state_t test_case_channel_ex_03_state = {
	.test_case_name = "test_case_channel_ex_03",
};
static black_box_state_t test_case_channel_ex_04_state = {
	.test_case_name = "test_case_channel_ex_04",
};
static black_box_state_t test_case_channel_ex_05_state = {
	.test_case_name = "test_case_channel_ex_05",
};
static black_box_state_t test_case_channel_ex_06_state = {
	.test_case_name = "test_case_channel_ex_06",
};
/* mutex for the common variable */
static pthread_mutex_t accept_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t accept_cond = PTHREAD_COND_INITIALIZER;

static bool channel_acc;

/* channel receive callback */
static void cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
	(void)mesh;
	(void)channel;
	(void)dat;
	(void)len;

	return;
}

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len) {
	(void)mesh;
	(void)channel;
	(void)dat;
	(void)len;

	assert_int_equal(port, PORT);

	pthread_mutex_lock(&accept_lock);
	channel_acc = true;
	assert(!pthread_cond_broadcast(&accept_cond));
	pthread_mutex_unlock(&accept_lock);

	return true;
}

/* Execute meshlink_channel_open_ex Test Case # 1 - testing meshlink_channel_open_ex API's
    valid case by passing all valid arguments */
static void test_case_channel_ex_01(void **state) {
	execute_test(test_steps_channel_ex_01, state);
}

/* Test Steps for meshlink_channel_open_ex Test Case # 1 - Valid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself

    Expected Result:
    Opens a channel and echoes the send queue data.
*/
/* TODO: When send queue & send queue length are passed with some value other
          than NULL it throws segmentation fault*/
static bool test_steps_channel_ex_01(void) {
	/* Set up logging for Meshlink */
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

	assert(meshlink_start(mesh_handle));

	/* Getting node handle for itself */
	meshlink_node_t *node = meshlink_get_self(mesh_handle);
	assert(node != NULL);

	char string[100] = "Test the 1st case";
	pthread_mutex_lock(&lock);
	channel_acc = false;
	pthread_mutex_unlock(&lock);

	/* Passing all valid arguments for meshlink_channel_open_ex */
	meshlink_channel_t *channel = NULL;
	channel = meshlink_channel_open_ex(mesh_handle, node, PORT, cb, string, strlen(string) + 1, MESHLINK_CHANNEL_UDP);
	assert_int_not_equal(channel, NULL);

	// Delay for establishing a channel
	sleep(1);

	pthread_mutex_lock(&lock);
	bool ret = channel_acc;
	pthread_mutex_unlock(&lock);

	assert_int_equal(ret, true);

	meshlink_close(mesh_handle);
	meshlink_destroy("channelexconf");

	return true;
}

/* Execute meshlink_channel_open_ex Test Case # 2 - testing API's valid case by passing NULL and
    0 for send queue & it's length respectively and others with valid arguments */
static void test_case_channel_ex_02(void **state) {
	execute_test(test_steps_channel_ex_02, state);
}
/* Test Steps for meshlink_channel_open_ex Test Case # 2 - Valid case (TCP channel)

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself

    Expected Result:
    Opens a TCP channel successfully by setting channel_acc true*/
static bool test_steps_channel_ex_02(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

	assert(meshlink_start(mesh_handle));

	meshlink_node_t *node = meshlink_get_self(mesh_handle);
	assert(node != NULL);

	pthread_mutex_lock(&lock);
	channel_acc = false;
	pthread_mutex_unlock(&lock);
	sleep(1);

	PRINT_TEST_CASE_MSG("Opening TCP alike channel ex\n");
	/* Passing all valid arguments for meshlink_channel_open_ex */
	meshlink_channel_t *channel;
	channel = meshlink_channel_open_ex(mesh_handle, node, PORT, cb, NULL, 0, MESHLINK_CHANNEL_TCP);
	assert_int_not_equal(channel, NULL);

	// Delay for establishing a channel
	sleep(1);
	pthread_mutex_lock(&lock);
	bool ret = channel_acc;
	pthread_mutex_unlock(&lock);

	assert_int_equal(ret, true);

	meshlink_close(mesh_handle);
	meshlink_destroy("channelexconf");
	return true;
}

/* Execute meshlink_channel_open_ex Test Case # 3 - Open a UDP channel */
static void test_case_channel_ex_03(void **state) {
	execute_test(test_steps_channel_ex_03, state);
}
/* Test Steps for meshlink_channel_open_ex Test Case # 3 - Valid case (UDP channel)

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself

    Expected Result:
    Opens a UDP channel successfully by setting channel_acc true */
static bool test_steps_channel_ex_03(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

	assert(meshlink_start(mesh_handle));

	/* Getting node handle for itself */
	meshlink_node_t *node = meshlink_get_self(mesh_handle);
	assert(node != NULL);

	pthread_mutex_lock(&lock);
	channel_acc = false;
	pthread_mutex_unlock(&lock);
	sleep(1);

	/* Passing all valid arguments for meshlink_channel_open_ex */
	meshlink_channel_t *channel;
	channel = meshlink_channel_open_ex(mesh_handle, node, PORT, cb, NULL, 0, MESHLINK_CHANNEL_UDP);
	assert_int_not_equal(channel, NULL);

	// Delay for establishing a channel
	sleep(1);

	pthread_mutex_lock(&lock);
	bool ret = channel_acc;
	pthread_mutex_unlock(&lock);

	assert_int_equal(ret, true);

	meshlink_close(mesh_handle);
	meshlink_destroy("channelexconf");
	return true;
}

/* Execute meshlink_channel_open_ex Test Case # 4 - Open a TCP channel with no receive callback
    and send queue */
static void test_case_channel_ex_04(void **state) {
	execute_test(test_steps_channel_ex_04, state);
}
/* Test Steps for meshlink_channel_open_ex Test Case # 4 - Valid Case (Disabling receive callback)

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself

    Expected Result:
    Opens a channel
*/

static bool test_steps_channel_ex_04(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

	assert(meshlink_start(mesh_handle));

	/* Getting node handle for itself */
	meshlink_node_t *node = meshlink_get_self(mesh_handle);
	assert(node != NULL);

	pthread_mutex_lock(&lock);
	channel_acc = false;
	pthread_mutex_unlock(&lock);

	/* Passing all valid arguments for meshlink_channel_open_ex i.e disabling receive callback and send queue */
	meshlink_channel_t *channel;
	channel = meshlink_channel_open_ex(mesh_handle, node, PORT, NULL, NULL, 0, MESHLINK_CHANNEL_UDP);
	assert(channel != NULL);
	// Delay for establishing a channel

	pthread_mutex_lock(&lock);
	bool ret = channel_acc;
	pthread_mutex_unlock(&lock);

	assert_int_equal(ret, true);

	meshlink_close(mesh_handle);
	meshlink_destroy("channelexconf");
	return true;
}

/* Execute meshlink_channel_open_ex Test Case # 5 - Opening channel using NULL as mesh handle argument
    for the API */
static void test_case_channel_ex_05(void **state) {
	execute_test(test_steps_channel_ex_05, state);
}
/* Test Steps for meshlink_channel_open_ex Test Case # 5 - Invalid case (NULL as mesh argument)

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel by passing NULL as argument for mesh handle

    Expected Result:
    meshlink_channel_open_ex returns NULL as channel handle reporting error accordingly
*/
static bool test_steps_channel_ex_05(void) {
	/* Set up logging for Meshlink */
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
	assert(mesh_handle);

	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

	assert(meshlink_start(mesh_handle));
	/* Getting node handle for itself */
	meshlink_node_t *node = meshlink_get_self(mesh_handle);
	assert(node != NULL);

	/* Trying to open channel using mesh handle as NULL argument */
	meshlink_channel_t *channel = meshlink_channel_open_ex(NULL, node, PORT, cb, NULL, 0, MESHLINK_CHANNEL_TCP);
	assert(channel == NULL);

	meshlink_close(mesh_handle);
	meshlink_destroy("channelexconf");
	return true;
}

/* Execute meshlink_channel_open_ex Test Case # 6 - Opening channel using NULL as node handle argument
    for the API*/
static void test_case_channel_ex_06(void **state) {
	execute_test(test_steps_channel_ex_06, state);
}

/* Test Steps for meshlink_channel_open_ex Test Case # 6 - Invalid case (NULL as node argument)

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel by passing NULL as argument for node handle

    Expected Result:
    meshlink_channel_open_ex returns NULL as channel handle reporting error accordingly
*/
static bool test_steps_channel_ex_06(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
	assert(mesh_handle);

	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

	assert(meshlink_start(mesh_handle));

	/* Trying to open channel using node handle as NULL argument */
	meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_handle, NULL, PORT, cb, NULL, 0, MESHLINK_CHANNEL_TCP);

	assert_int_equal(channel, NULL);

	meshlink_close(mesh_handle);
	meshlink_destroy("channelexconf");
	return true;
}

int test_meshlink_channel_open_ex(void) {
	const struct CMUnitTest blackbox_channel_ex_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_01, NULL, NULL,
		                (void *)&test_case_channel_ex_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_02, NULL, NULL,
		                (void *)&test_case_channel_ex_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_03, NULL, NULL,
		                (void *)&test_case_channel_ex_03_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_04, NULL, NULL,
		                (void *)&test_case_channel_ex_04_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_05, NULL, NULL,
		                (void *)&test_case_channel_ex_05_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_06, NULL, NULL,
		                (void *)&test_case_channel_ex_06_state)
	};

	total_tests += sizeof(blackbox_channel_ex_tests) / sizeof(blackbox_channel_ex_tests[0]);

	assert(pthread_mutex_init(&lock, NULL) == 0);
	int failed = cmocka_run_group_tests(blackbox_channel_ex_tests, NULL, NULL);
	assert(pthread_mutex_destroy(&lock) == 0);

	return failed;
}
