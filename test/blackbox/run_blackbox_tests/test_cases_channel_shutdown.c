/*
    test_cases_channel_shutdown.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_channel_shutdown.h"
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

static void test_case_mesh_channel_shutdown_01(void **state);
static bool test_steps_mesh_channel_shutdown_01(void);
static void test_case_mesh_channel_shutdown_02(void **state);
static bool test_steps_mesh_channel_shutdown_02(void);
static void test_case_mesh_channel_shutdown_03(void **state);
static bool test_steps_mesh_channel_shutdown_03(void);
static void test_case_mesh_channel_shutdown_04(void **state);
static bool test_steps_mesh_channel_shutdown_04(void);
static void test_case_mesh_channel_shutdown_05(void **state);
static bool test_steps_mesh_channel_shutdown_05(void);

static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len);
static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable);
static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len);

/* State structure for meshlink_channel_shutdown Test Case #1 */
static black_box_state_t test_mesh_channel_shutdown_01_state = {
	.test_case_name = "test_case_mesh_channel_shutdown_01",
};

/* State structure for meshlink_channel_shutdown Test Case #2 */
static black_box_state_t test_mesh_channel_shutdown_02_state = {
	.test_case_name = "test_case_mesh_channel_shutdown_02",
};

/* State structure for meshlink_channel_shutdown Test Case #3 */
static black_box_state_t test_mesh_channel_shutdown_03_state = {
	.test_case_name = "test_case_mesh_channel_shutdown_03",
};

/* State structure for meshlink_channel_shutdown Test Case #4 */
static black_box_state_t test_mesh_channel_shutdown_04_state = {
	.test_case_name = "test_case_mesh_channel_shutdown_04",
};

/* State structure for meshlink_channel_shutdown Test Case #5 */
static black_box_state_t test_mesh_channel_shutdown_05_state = {
	.test_case_name = "test_case_mesh_channel_shutdown_05",
};

static bool channel_acc;
static bool polled;
static bool foo_responded;
static bool bar_responded;

/* mutex for the common variable */
static pthread_mutex_t accept_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t poll_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t bar_responded_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t foo_responded_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t accept_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t poll_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t foo_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t bar_cond = PTHREAD_COND_INITIALIZER;

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	assert(port == 7);
	assert(!len);

	meshlink_set_channel_receive_cb(mesh, channel, receive_cb);
	channel->node->priv = channel;
	pthread_mutex_lock(&accept_lock);
	channel_acc = true;
	assert(!pthread_cond_broadcast(&accept_cond));
	pthread_mutex_unlock(&accept_lock);

	return true;
}

/* channel receive callback */
static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
	if(!strcmp(mesh->name, "foo")) {
		pthread_mutex_lock(& foo_responded_lock);
		foo_responded = true;
		assert(!pthread_cond_broadcast(&foo_cond));
		pthread_mutex_unlock(& foo_responded_lock);

	} else if(!strcmp(mesh->name, "bar")) {
		pthread_mutex_lock(& bar_responded_lock);
		bar_responded = true;
		assert(!pthread_cond_broadcast(&bar_cond));
		pthread_mutex_unlock(& bar_responded_lock);

		assert(meshlink_channel_send(mesh, channel, "echo", 4) >= 0);

	}
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	pthread_mutex_lock(&poll_lock);
	polled = true;
	assert(!pthread_cond_broadcast(&poll_cond));
	pthread_mutex_unlock(&poll_lock);
}

/* Execute meshlink_channel_shutdown Test Case # 1*/
static void test_case_mesh_channel_shutdown_01(void **state) {
	execute_test(test_steps_mesh_channel_shutdown_01, state);
}

/* Test Steps for meshlink_channel_shutdown Test Case # 1 - Valid case

    Test Steps:
    1. Open foo and bar instances and open a channel between them
    2. Send data through the channel.
    3. Shut down channel's read and send data
    4. Shutdown channel's write and send data

    Expected Result:
    Data is able to receive through channel before shutting down,
    On shutting down read its should not able to receive data and when write
    is shut down its should be able to send data through channel.
*/
static bool test_steps_mesh_channel_shutdown_01(void) {
	struct timespec timeout = {0};
	meshlink_destroy("chan_shutdown_conf.1");
	meshlink_destroy("chan_shutdown_conf.2");
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("chan_shutdown_conf.1", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);

	meshlink_handle_t *mesh2 = meshlink_open("chan_shutdown_conf.2", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);

	char *data = meshlink_export(mesh1);
	assert(data);
	assert(meshlink_import(mesh2, data));
	free(data);
	data = meshlink_export(mesh2);
	assert(data);
	assert(meshlink_import(mesh1, data));
	free(data);

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh2, accept_cb);

	// Start both instances

	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));
	sleep(1);

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	assert(bar);

	meshlink_channel_t *channel1 = meshlink_channel_open(mesh1, bar, 7, receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh1, channel1, poll_cb);

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

	meshlink_channel_t *channel2 = bar->priv;

	// Sending to bar and testing the echo

	assert(meshlink_channel_send(mesh1, channel1, "echo", 4) >= 0);

	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&foo_responded_lock);

	while(foo_responded == false) {
		assert(!pthread_cond_timedwait(&foo_cond, &foo_responded_lock, &timeout));
	}

	pthread_mutex_unlock(&foo_responded_lock);
	assert(foo_responded);

	// Shutting down channel read

	meshlink_channel_shutdown(mesh1, channel1, SHUT_RD);
	bar_responded = false;
	foo_responded = false;
	assert(meshlink_channel_send(mesh1, channel1, "echo", 4) >= 0);

	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&bar_responded_lock);

	while(bar_responded == false) {
		assert(!pthread_cond_timedwait(&bar_cond, &bar_responded_lock, &timeout));
	}

	pthread_mutex_unlock(&bar_responded_lock);
	assert_int_equal(bar_responded, true);
	sleep(1);
	assert_int_equal(foo_responded, false);

	// Shutting down channel write

	meshlink_channel_shutdown(mesh1, channel1, SHUT_WR);

	ssize_t send_ret = meshlink_channel_send(mesh1, channel1, "echo", 4);
	assert_int_equal(send_ret, -1);

	// Clean up.

	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("chan_shutdown_conf.1");
	meshlink_destroy("chan_shutdown_conf.2");

	return true;
}

/* Execute meshlink_channel_shutdown Test Case # 2*/
static void test_case_mesh_channel_shutdown_02(void **state) {
	execute_test(test_steps_mesh_channel_shutdown_02, state);
}

/* Test Steps for meshlink_channel_shutdown Test Case # 2 - Invalid case

    Test Steps:
    1. Open node instance and create a channel
    2. Call meshlink_channel_shutdown API by passing NULL as mesh handle

    Expected Result:
    meshlink_channel_shutdown API should report proper error handling
*/
static bool test_steps_mesh_channel_shutdown_02(void) {
	meshlink_destroy("channelshutdownconf.3");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelshutdownconf.3", "nut", "node_sim", 1);
	assert(mesh_handle != NULL);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_channel_accept_cb(mesh_handle, accept_cb);

	assert(meshlink_start(mesh_handle));

	meshlink_node_t *node = meshlink_get_self(mesh_handle);
	assert(node != NULL);

	meshlink_channel_t *channel = meshlink_channel_open(mesh_handle, node, 8000, NULL, NULL, 0);
	assert(channel);
	meshlink_set_channel_poll_cb(mesh_handle, channel, poll_cb);

	// Passing NULL as mesh handle and other arguments being valid

	meshlink_channel_shutdown(NULL, channel, SHUT_WR);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	meshlink_close(mesh_handle);
	meshlink_destroy("channelshutdownconf.3");

	return true;
}

/* Execute meshlink_channel_shutdown Test Case # 3*/
static void test_case_mesh_channel_shutdown_03(void **state) {
	execute_test(test_steps_mesh_channel_shutdown_03, state);
}

/* Test Steps for meshlink_channel_shutdown Test Case # 3

    Test Steps:
    1. Open node instance
    2. Call meshlink_channel_shutdown API by passing NULL as channel handle

    Expected Result:
    meshlink_channel_shutdown API should report proper error handling
*/
static bool test_steps_mesh_channel_shutdown_03(void) {
	meshlink_destroy("channelshutdownconf.4");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("channelshutdownconf.4", "nut", "node_sim", 1);
	assert(mesh_handle != NULL);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_channel_accept_cb(mesh_handle, accept_cb);

	assert(meshlink_start(mesh_handle));

	// Passing NULL as mesh handle and other arguments being valid

	meshlink_channel_shutdown(mesh_handle, NULL, SHUT_WR);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	meshlink_close(mesh_handle);
	meshlink_destroy("channelshutdownconf.4");

	return true;
}


int test_meshlink_channel_shutdown(void) {
	const struct CMUnitTest blackbox_channel_shutdown_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_shutdown_01, NULL, NULL,
		(void *)&test_mesh_channel_shutdown_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_shutdown_02, NULL, NULL,
		(void *)&test_mesh_channel_shutdown_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_shutdown_03, NULL, NULL,
		(void *)&test_mesh_channel_shutdown_03_state)
	};
	total_tests += sizeof(blackbox_channel_shutdown_tests) / sizeof(blackbox_channel_shutdown_tests[0]);

	return cmocka_run_group_tests(blackbox_channel_shutdown_tests, NULL, NULL);
}
