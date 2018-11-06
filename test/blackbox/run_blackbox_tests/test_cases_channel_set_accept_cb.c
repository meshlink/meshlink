/*
    test_cases_channel_set_accept_cb.c -- Execution of specific meshlink black box test cases
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
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "test_cases_channel_set_accept_cb.h"
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

static void test_case_set_channel_accept_cb_01(void **state);
static bool test_steps_set_channel_accept_cb_01(void);
static void test_case_set_channel_accept_cb_02(void **state);
static bool test_steps_set_channel_accept_cb_02(void);

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len);
static bool channel_reject(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len);

static bool channel_acc;
static bool channel_rej;
static bool polled;
static bool rejected;

/* mutex for the common variable */
static pthread_mutex_t accept_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t reject_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t poll_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock_receive = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t accept_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t reject_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t poll_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t receive_cond = PTHREAD_COND_INITIALIZER;

static black_box_state_t test_case_channel_set_accept_cb_01_state = {
	.test_case_name = "test_case_channel_set_accept_cb_01",
};
static black_box_state_t test_case_channel_set_accept_cb_02_state = {
	.test_case_name = "test_case_channel_set_accept_cb_02",
};

/* channel receive callback */
static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
	if(!len) {
		if(!meshlink_errno) {
			pthread_mutex_lock(& lock_receive);
			rejected = true;
			assert(!pthread_cond_broadcast(&receive_cond));
			pthread_mutex_unlock(& lock_receive);
		}
	}
}

/* channel reject callback */
static bool channel_reject(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)port;
	(void)data;
	(void)len;
	return false;
}

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len) {
	(void)dat;
	(void)len;
	char *data = (char *) dat;
	assert_int_equal(port, PORT);

	pthread_mutex_lock(&accept_lock);
	channel_acc = true;
	assert(!pthread_cond_broadcast(&accept_cond));
	pthread_mutex_unlock(&accept_lock);

	return true;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	pthread_mutex_lock(&poll_lock);
	polled = true;
	assert(!pthread_cond_broadcast(&poll_cond));
	pthread_mutex_unlock(&poll_lock);
}

/* Execute meshlink_channel_set_accept_cb Test Case # 1 - Valid case*/
static void test_case_set_channel_accept_cb_01(void **state) {
	execute_test(test_steps_set_channel_accept_cb_01, state);
}
/* Test Steps for meshlink_channel_set_accept_cb Test Case # 1 - Valid case

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Set channel_accept callback for NUT's meshlink_set_channel_accept_cb API.
    3. Export and Import nodes.
    4. Open a channel with NUT from bar to invoke channel accept callback
    5. Open a channel with bar from NUT to invoke channel accept callback

    Expected Result:
    Opens a channel by invoking accept callback, when accept callback rejects the channel
    it should invoke the other node's receive callback with length = 0 and no error.
*/
static bool test_steps_set_channel_accept_cb_01(void) {
	/* deleting the confbase if already exists */
	struct timespec timeout = {0};
	meshlink_destroy("acceptconf1");
	meshlink_destroy("acceptconf2");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instances */
	meshlink_handle_t *mesh1 = meshlink_open("acceptconf1", "nut", "chat", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	meshlink_handle_t *mesh2 = meshlink_open("acceptconf2", "bar", "chat", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
	meshlink_set_log_cb(mesh1, MESHLINK_INFO, meshlink_callback_logger);
	meshlink_set_log_cb(mesh2, MESHLINK_INFO, meshlink_callback_logger);

	meshlink_set_channel_accept_cb(mesh2, channel_reject);
	meshlink_set_channel_accept_cb(mesh1, channel_accept);

	/* Export and Import on both sides */
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);
	assert(meshlink_import(mesh1, exp2));
	assert(meshlink_import(mesh2, exp1));

	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));
	sleep(1);

	meshlink_node_t *destination = meshlink_get_node(mesh2, "nut");
	assert(destination != NULL);

	/* Open channel for nut node from bar node which should be accepted */
	polled = false;
	channel_acc = false;
	meshlink_channel_t *channel2 = meshlink_channel_open(mesh2, destination, PORT, NULL, NULL, 0);
	assert(channel2);
	meshlink_set_channel_poll_cb(mesh2, channel2, poll_cb);

	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&poll_lock);

	while(polled == false) {
		assert(!pthread_cond_timedwait(&poll_cond, &poll_lock, &timeout));
	}

	pthread_mutex_unlock(&poll_lock);

	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&accept_lock);

	while(channel_acc == false) {
		assert(!pthread_cond_timedwait(&accept_cond, &accept_lock, &timeout));
	}

	pthread_mutex_unlock(&accept_lock);

	/* Open channel for bar node from nut node which should be rejected */
	polled = false;
	rejected = false;
	channel_acc = false;
	destination = meshlink_get_node(mesh1, "bar");
	assert(destination != NULL);

	meshlink_channel_t *channel1 = meshlink_channel_open(mesh1, destination, PORT, channel_receive_cb, NULL, 0);
	assert(channel1);
	meshlink_set_channel_poll_cb(mesh1, channel1, poll_cb);

	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&poll_lock);

	while(polled == false) {
		assert(!pthread_cond_timedwait(&poll_cond, &poll_lock, &timeout));
	}

	pthread_mutex_unlock(&poll_lock);

	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&lock_receive);

	while(rejected == false) {
		assert(!pthread_cond_timedwait(&receive_cond, &lock_receive, &timeout));
	}

	pthread_mutex_unlock(&lock_receive);

	/* closing channel, meshes and destroying confbase */
	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("acceptconf1");
	meshlink_destroy("acceptconf2");

	return true;
}

/* Execute meshlink_channel_set_accept_cb Test Case # 2 - Invalid case*/
static void test_case_set_channel_accept_cb_02(void **state) {
	execute_test(test_steps_set_channel_accept_cb_02, state);
}
/* Test Steps for meshlink_channel_set_accept_cb Test Case # 2 - Invalid case

    Test Steps:
    1. Passing NULL as mesh handle argument for channel accept callback.

    Expected Result:
    meshlink_channel_set_accept_cb returning proper meshlink_errno.
*/
static bool test_steps_set_channel_accept_cb_02(void) {
	/* setting channel accept cb with NULL as mesh handle and valid callback */
	meshlink_set_channel_accept_cb(NULL, channel_accept);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	return true;
}


int test_meshlink_set_channel_accept_cb(void) {
	const struct CMUnitTest blackbox_channel_set_accept_cb_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_set_channel_accept_cb_01, NULL, NULL,
		(void *)&test_case_channel_set_accept_cb_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_set_channel_accept_cb_02, NULL, NULL,
		(void *)&test_case_channel_set_accept_cb_02_state)
	};

	total_tests += sizeof(blackbox_channel_set_accept_cb_tests) / sizeof(blackbox_channel_set_accept_cb_tests[0]);

	int failed = cmocka_run_group_tests(blackbox_channel_set_accept_cb_tests , NULL , NULL);

	return failed;
}



