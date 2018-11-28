/*
    test_cases_channel_set_receive_cb.c -- Execution of specific meshlink black box test cases
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
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "test_cases_channel_set_receive_cb.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <pthread.h>
#include <errno.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

/* Modify this to change the port number */
#define PORT 8000

/* Modify this to change the channel receive callback access buffer */
#define TCP_TEST 8000

static void test_case_set_channel_receive_cb_01(void **state);
static bool test_steps_set_channel_receive_cb_01(void);
static void test_case_set_channel_receive_cb_02(void **state);
static bool test_steps_set_channel_receive_cb_02(void);
static void test_case_set_channel_receive_cb_03(void **state);
static bool test_steps_set_channel_receive_cb_03(void);
static void test_case_set_channel_receive_cb_04(void **state);
static bool test_steps_set_channel_receive_cb_04(void);

static void channel_poll(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len);
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len);

static bool rec_stat = false;
static bool accept_stat = false;

/* mutex for the receive callback common resources */
static pthread_mutex_t lock_accept = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock_receive = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t accept_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t receive_cond = PTHREAD_COND_INITIALIZER;

static black_box_state_t test_case_channel_set_receive_cb_01_state = {
	.test_case_name = "test_case_channel_set_receive_cb_01",
};
static black_box_state_t test_case_channel_set_receive_cb_02_state = {
	.test_case_name = "test_case_channel_set_receive_cb_02",
};
static black_box_state_t test_case_channel_set_receive_cb_03_state = {
	.test_case_name = "test_case_channel_set_receive_cb_03",
};
static black_box_state_t test_case_channel_set_receive_cb_04_state = {
	.test_case_name = "test_case_channel_set_receive_cb_04",
};

/* channel receive callback */
static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
	pthread_mutex_lock(& lock_receive);
	rec_stat = true;
	assert(!pthread_cond_broadcast(&receive_cond));
	pthread_mutex_unlock(& lock_receive);
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	meshlink_set_channel_receive_cb(mesh, channel, channel_receive_cb);

	pthread_mutex_lock(& lock_accept);
	accept_stat = true;
	assert(!pthread_cond_broadcast(&accept_cond));
	pthread_mutex_unlock(& lock_accept);

	return true;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	assert(meshlink_channel_send(mesh, channel, "Hello", 5) >= 0);
}

/* Execute meshlink_channel_set_receive_cb Test Case # 1 */
static void test_case_set_channel_receive_cb_01(void **state) {
	execute_test(test_steps_set_channel_receive_cb_01, state);
}
/* Test Steps for meshlink_channel_set_receive_cb Test Case # 1 - Valid case

    Test Steps:
    1. Run NUT and Open channel for itself.
    2. Set channel receive callback and send data.

    Expected Result:
    Opens a channel by invoking channel receive callback when data sent to it.
*/
static bool test_steps_set_channel_receive_cb_01(void) {
	struct timespec timeout = {0};
	meshlink_destroy("channelreceiveconf");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelreceiveconf", "nut", "node_sim", 1);
	assert(mesh_handle != NULL);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_channel_accept_cb(mesh_handle, accept_cb);

	assert(meshlink_start(mesh_handle));

	meshlink_node_t *node = meshlink_get_self(mesh_handle);
	assert(node != NULL);

	rec_stat = false;
	accept_stat = false;
	meshlink_channel_t *channel = meshlink_channel_open(mesh_handle, node, 8000, NULL, NULL, 0);
	meshlink_set_channel_poll_cb(mesh_handle, channel, poll_cb);

	timeout.tv_sec = time(NULL) + 20;
	pthread_mutex_lock(& lock_accept);

	while(accept_stat == false) {
		assert(!pthread_cond_timedwait(&accept_cond, &lock_accept, &timeout));
	}

	pthread_mutex_unlock(& lock_accept);

	timeout.tv_sec = time(NULL) + 20;
	pthread_mutex_lock(& lock_receive);

	while(rec_stat == false) {
		assert(pthread_cond_timedwait(&receive_cond, &lock_receive, &timeout) == 0);
	}

	pthread_mutex_unlock(& lock_receive);

	meshlink_close(mesh_handle);
	meshlink_destroy("channelreceiveconf");

	return true;
}

/* Execute meshlink_channel_set_receive_cb Test Case # 2 */
static void test_case_set_channel_receive_cb_02(void **state) {
	execute_test(test_steps_set_channel_receive_cb_02, state);
}
/* Test Steps for meshlink_channel_set_receive_cb Test Case # 2 - Invalid case

    Test Steps:
    1. Run NUT and Open channel for itself.
    2. Set channel receive callback with NULL as mesh handle.

    Expected Result:
    meshlink_channel_set_receive_cb returning proper meshlink_errno.
*/
static bool test_steps_set_channel_receive_cb_02(void) {
	meshlink_destroy("channelreceiveconf");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelreceiveconf", "nut", "node_sim", 1);
	assert(mesh_handle != NULL);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_channel_accept_cb(mesh_handle, accept_cb);

	/* Starting NUT */
	assert(meshlink_start(mesh_handle));
	meshlink_node_t *node = meshlink_get_self(mesh_handle);
	assert(node != NULL);

	meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_handle, node, 8000, NULL, NULL, 0, MESHLINK_CHANNEL_UDP);
	assert(channel != NULL);
	meshlink_set_channel_poll_cb(mesh_handle, channel, poll_cb);

	/* Setting channel for NUT using meshlink_set_channel_receive_cb API with NULL as mesh handle */
	meshlink_set_channel_receive_cb(NULL, channel, channel_receive_cb);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	meshlink_close(mesh_handle);
	meshlink_destroy("channelreceiveconf");

	return true;
}

/* Execute meshlink_channel_set_receive_cb Test Case # 3 */
static void test_case_set_channel_receive_cb_03(void **state) {
	execute_test(test_steps_set_channel_receive_cb_03, state);
}
/* Test Steps for meshlink_channel_set_receive_cb Test Case # 3 - Invalid case

    Test Steps:
    1. Run NUT and Open channel for itself.
    2. Set channel receive callback with NULL as channel handle.

    Expected Result:
    meshlink_channel_set_receive_cb returning proper meshlink_errno.
*/
static bool test_steps_set_channel_receive_cb_03(void) {
	meshlink_destroy("channelreceiveconf");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelreceiveconf", "nut", "node_sim", 1);
	fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
	assert(mesh_handle != NULL);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_channel_accept_cb(mesh_handle, accept_cb);

	/* Starting NUT */
	assert(meshlink_start(mesh_handle));

	/* Setting channel for NUT using meshlink_set_channel_receive_cb API channel handle as NULL */
	meshlink_set_channel_receive_cb(mesh_handle, NULL, channel_receive_cb);

	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);
	meshlink_close(mesh_handle);
	meshlink_destroy("channelreceiveconf");
	return true;
}


int test_meshlink_set_channel_receive_cb(void) {
	const struct CMUnitTest blackbox_channel_set_receive_cb_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_set_channel_receive_cb_01, NULL, NULL,
		(void *)&test_case_channel_set_receive_cb_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_set_channel_receive_cb_02, NULL, NULL,
		(void *)&test_case_channel_set_receive_cb_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_set_channel_receive_cb_03, NULL, NULL,
		(void *)&test_case_channel_set_receive_cb_03_state)
	};
	total_tests += sizeof(blackbox_channel_set_receive_cb_tests) / sizeof(blackbox_channel_set_receive_cb_tests[0]);

	int failed = cmocka_run_group_tests(blackbox_channel_set_receive_cb_tests, NULL, NULL);

	return failed;
}
