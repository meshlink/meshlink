/*
    test_cases_channel_set_poll_cb.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_channel_set_poll_cb.h"
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
/* Modify this to change the port number */
#define PORT 8000

static void test_case_channel_set_poll_cb_01(void **state);
static bool test_steps_channel_set_poll_cb_01(void);
static void test_case_channel_set_poll_cb_02(void **state);
static bool test_steps_channel_set_poll_cb_02(void);
static void test_case_channel_set_poll_cb_03(void **state);
static bool test_steps_channel_set_poll_cb_03(void);
static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len);

static black_box_state_t test_case_channel_set_poll_cb_01_state = {
	.test_case_name = "test_case_channel_set_poll_cb_01",
};
static black_box_state_t test_case_channel_set_poll_cb_02_state = {
	.test_case_name = "test_case_channel_set_poll_cb_02",
};
static black_box_state_t test_case_channel_set_poll_cb_03_state = {
	.test_case_name = "test_case_channel_set_poll_cb_03",
};

static bool polled;
static bool reachable;

static pthread_mutex_t poll_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t poll_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t reachable_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reachable_cond = PTHREAD_COND_INITIALIZER;

/* channel accept callback */
static bool channel_accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)port;
	(void)data;
	(void)len;
	return false;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	pthread_mutex_lock(&poll_lock);
	polled = true;
	assert(!pthread_cond_broadcast(&poll_cond));
	pthread_mutex_unlock(&poll_lock);
	return;
}

static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *source, bool reach) {
	pthread_mutex_lock(&reachable_lock);
	reachable = true;
	assert(!pthread_cond_broadcast(&reachable_cond));
	pthread_mutex_unlock(&reachable_lock);

	return;
}

/* Execute meshlink_channel_set_poll_cb Test Case # 1 */
static void test_case_channel_set_poll_cb_01(void **state) {
	execute_test(test_steps_channel_set_poll_cb_01, state);
	return;
}
/* Test Steps for meshlink_channel_set_poll_cb Test Case # 1

    Test Steps:
    1. Run NUT
    2. Open channel of the NUT itself

    Expected Result:
    Opens a channel and also invokes poll callback.
*/
static bool test_steps_channel_set_poll_cb_01(void) {
	/* deleting the confbase if already exists */
	struct timespec timeout = {0};
	meshlink_destroy("pollconf1");
	meshlink_destroy("pollconf2");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instances */
	meshlink_handle_t *mesh1 = meshlink_open("pollconf1", "nut", "chat", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	meshlink_handle_t *mesh2 = meshlink_open("pollconf2", "bar", "chat", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
	meshlink_set_log_cb(mesh1, MESHLINK_INFO, meshlink_callback_logger);
	meshlink_set_log_cb(mesh2, MESHLINK_INFO, meshlink_callback_logger);
	meshlink_set_node_status_cb(mesh1, node_status_cb);
	meshlink_set_channel_accept_cb(mesh1, channel_accept_cb);

	/* Export and Import on both sides */
	reachable = false;
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);
	assert(meshlink_import(mesh1, exp2));
	assert(meshlink_import(mesh2, exp1));

	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));

	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&reachable_lock);

	while(reachable == false) {
		assert(!pthread_cond_timedwait(&reachable_cond, &reachable_lock, &timeout));
	}

	pthread_mutex_unlock(&reachable_lock);

	meshlink_node_t *destination = meshlink_get_node(mesh2, "nut");
	assert(destination != NULL);

	/* Open channel for nut node from bar node which should be accepted */
	polled = false;
	meshlink_channel_t *channel = meshlink_channel_open(mesh2, destination, PORT, NULL, NULL, 0);
	assert(channel);
	meshlink_set_channel_poll_cb(mesh2, channel, poll_cb);

	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&poll_lock);

	while(polled == false) {
		assert(!pthread_cond_timedwait(&poll_cond, &poll_lock, &timeout));
	}

	pthread_mutex_unlock(&poll_lock);

	/* closing channel, meshes and destroying confbase */
	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("pollconf1");
	meshlink_destroy("pollconf2");

	return true;
}

/* Execute meshlink_channel_set_poll_cb Test Case # 2 */
static void test_case_channel_set_poll_cb_02(void **state) {
	execute_test(test_steps_channel_set_poll_cb_02, state);
	return;
}
/* Test Steps for meshlink_channel_set_poll_cb Test Case # 2

    Test Steps:
    1. Run NUT
    2. Open channel of the NUT itself
    3. Pass NULL as mesh handle argument for meshlink_set_channel_poll_cb API

    Expected Result:
    Reports error accordingly by returning NULL
*/
static bool test_steps_channel_set_poll_cb_02(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelpollconf3", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	assert(meshlink_start(mesh_handle));

	meshlink_node_t *node = meshlink_get_self(mesh_handle);
	assert(node != NULL);

	/* Opening channel */
	meshlink_channel_t *channel = meshlink_channel_open(mesh_handle, node, PORT, NULL, NULL, 0);
	assert(channel != NULL);

	/* Setting poll cb with NULL as mesh handler */
	meshlink_set_channel_poll_cb(NULL, channel, poll_cb);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	meshlink_close(mesh_handle);
	meshlink_destroy("channelpollconf3");
	return true;
}

/* Execute meshlink_channel_set_poll_cb Test Case # 3 */
static void test_case_channel_set_poll_cb_03(void **state) {
	execute_test(test_steps_channel_set_poll_cb_03, state);
	return;
}
/* Test Steps for meshlink_channel_set_poll_cb Test Case # 3

    Test Steps:
    1. Run NUT
    2. Open channel of the NUT itself
    3. Pass NULL as channel handle argument for meshlink_set_channel_poll_cb API

    Expected Result:
    Reports error accordingly by returning NULL
*/
static bool test_steps_channel_set_poll_cb_03(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelpollconf4", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	assert(meshlink_start(mesh_handle));

	/* Setting poll cb with NULL as channel handler */
	meshlink_set_channel_poll_cb(mesh_handle, NULL, poll_cb);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	meshlink_close(mesh_handle);
	meshlink_destroy("channelpollconf4");
	return true;
}


int test_meshlink_set_channel_poll_cb(void) {
	const struct CMUnitTest blackbox_channel_set_poll_cb_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_set_poll_cb_01, NULL, NULL,
		(void *)&test_case_channel_set_poll_cb_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_set_poll_cb_02, NULL, NULL,
		(void *)&test_case_channel_set_poll_cb_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_set_poll_cb_03, NULL, NULL,
		(void *)&test_case_channel_set_poll_cb_03_state)
	};
	total_tests += sizeof(blackbox_channel_set_poll_cb_tests) / sizeof(blackbox_channel_set_poll_cb_tests[0]);

	return cmocka_run_group_tests(blackbox_channel_set_poll_cb_tests , NULL , NULL);
}
