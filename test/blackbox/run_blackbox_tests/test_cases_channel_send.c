/*
    test_cases_channel_send.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_channel_send.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

static void test_case_mesh_channel_send_01(void **state);
static bool test_steps_mesh_channel_send_01(void);
static void test_case_mesh_channel_send_02(void **state);
static bool test_steps_mesh_channel_send_02(void);
static void test_case_mesh_channel_send_03(void **state);
static bool test_steps_mesh_channel_send_03(void);
static void test_case_mesh_channel_send_04(void **state);
static bool test_steps_mesh_channel_send_04(void);

static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len);
static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable);
static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len);

/* State structure for meshlink_channel_send Test Case #1 */
static black_box_state_t test_mesh_channel_send_01_state = {
	.test_case_name = "test_case_mesh_channel_send_01",
};

/* State structure for meshlink_channel_send Test Case #2 */
static black_box_state_t test_mesh_channel_send_02_state = {
	.test_case_name = "test_case_mesh_channel_send_02",
};

/* State structure for meshlink_channel_send Test Case #3 */
static black_box_state_t test_mesh_channel_send_03_state = {
	.test_case_name = "test_case_mesh_channel_send_03",
};

/* State structure for meshlink_channel_send Test Case #4 */
static black_box_state_t test_mesh_channel_send_04_state = {
	.test_case_name = "test_case_mesh_channel_send_04",
};

/* Execute meshlink_channel_send Test Case # 1*/
static void test_case_mesh_channel_send_01(void **state) {
	execute_test(test_steps_mesh_channel_send_01, state);
}

static pthread_mutex_t poll_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t bar_reach_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t bar_responded_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t poll_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t status_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t send_cond = PTHREAD_COND_INITIALIZER;

static bool polled;
static bool bar_reachable;
static bool bar_responded;

static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(!strcmp(node->name, "bar")) {
		pthread_mutex_lock(& bar_reach_lock);
		bar_reachable = reachable;
		assert(!pthread_cond_broadcast(&status_cond));
		pthread_mutex_unlock(& bar_reach_lock);
	}
}

static bool reject_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)port;
	(void)data;
	(void)len;
	return false;
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	assert(port == 7);
	assert(!len);

	meshlink_set_channel_receive_cb(mesh, channel, receive_cb);

	return true;
}

/* channel receive callback */
static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
	if(len == 5 && !memcmp(dat, "Hello", 5)) {
		pthread_mutex_lock(& bar_responded_lock);
		bar_responded = true;
		assert(!pthread_cond_broadcast(&send_cond));
		pthread_mutex_unlock(& bar_responded_lock);
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


/* Test Steps for meshlink_channel_send Test Case # 1*/
static bool test_steps_mesh_channel_send_01(void) {
	struct timespec timeout = {0};
	meshlink_destroy("chan_send_conf.1");
	meshlink_destroy("chan_send_conf.2");

	// Open two new meshlink instance.
	meshlink_handle_t *mesh1 = meshlink_open("chan_send_conf.1", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	meshlink_handle_t *mesh2 = meshlink_open("chan_send_conf.2", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	meshlink_set_log_cb(mesh1, MESHLINK_DEBUG, meshlink_callback_logger);
	meshlink_set_log_cb(mesh2, MESHLINK_DEBUG, meshlink_callback_logger);

	char *data = meshlink_export(mesh1);
	assert(data);
	assert(meshlink_import(mesh2, data));
	free(data);
	data = meshlink_export(mesh2);
	assert(data);
	assert(meshlink_import(mesh1, data));
	free(data);

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh1, reject_cb);
	meshlink_set_channel_accept_cb(mesh2, accept_cb);

	meshlink_set_node_status_cb(mesh1, status_cb);

	// Start both instances
	bar_reachable = false;
	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));

	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&bar_reach_lock);

	while(bar_reachable == false) {
		assert(!pthread_cond_timedwait(&status_cond, &bar_reach_lock, &timeout));
	}

	pthread_mutex_unlock(&bar_reach_lock);

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	assert(bar);

	bar_responded = false;
	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, NULL, NULL, 0);
	meshlink_set_channel_poll_cb(mesh1, channel, poll_cb);

	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&poll_lock);

	while(polled == false) {
		assert(!pthread_cond_timedwait(&poll_cond, &poll_lock, &timeout));
	}

	pthread_mutex_unlock(&poll_lock);

	assert(meshlink_channel_send(mesh1, channel, "Hello", 5) >= 0);

	timeout.tv_sec = time(NULL) + 20;
	pthread_mutex_lock(&bar_responded_lock);

	if(bar_responded == false) {
		assert(!pthread_cond_timedwait(&send_cond, &bar_responded_lock, &timeout));
	}

	pthread_mutex_unlock(&bar_responded_lock);

	// Clean up.

	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("chan_send_conf.1");
	meshlink_destroy("chan_send_conf.2");

	return true;
}

/* Execute meshlink_channel_send Test Case # 2*/
static void test_case_mesh_channel_send_02(void **state) {
	execute_test(test_steps_mesh_channel_send_02, state);
}

/* Test Steps for meshlink_channel_send Test Case # 2*/
static bool test_steps_mesh_channel_send_02(void) {
	struct timespec timeout = {0};
	meshlink_destroy("chan_send_conf.5");

	// Open new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("chan_send_conf.5", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);

	meshlink_set_channel_accept_cb(mesh1, accept_cb);

	// Start node instance

	assert(meshlink_start(mesh1));

	meshlink_node_t *node = meshlink_get_self(mesh1);
	assert(node);

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, node, 7, receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh1, channel, poll_cb);

	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&poll_lock);

	while(polled == false) {
		assert(!pthread_cond_timedwait(&poll_cond, &poll_lock, &timeout));
	}

	pthread_mutex_unlock(&poll_lock);

	ssize_t send_return = meshlink_channel_send(NULL, channel, "Hello", 5);

	assert_int_equal(send_return, -1);

	// Clean up.

	meshlink_close(mesh1);
	meshlink_destroy("chan_send_conf.5");

	return true;
}

/* Execute meshlink_channel_send Test Case # 3*/
static void test_case_mesh_channel_send_03(void **state) {
	execute_test(test_steps_mesh_channel_send_03, state);
}

/* Test Steps for meshlink_channel_send Test Case # 3*/
static bool test_steps_mesh_channel_send_03(void) {
	struct timespec timeout = {0};
	meshlink_destroy("chan_send_conf.7");
	// Open new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("chan_send_conf.7", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	meshlink_set_channel_accept_cb(mesh1, accept_cb);

	// Start node instance

	assert(meshlink_start(mesh1));

	ssize_t send_return = meshlink_channel_send(mesh1, NULL, "Hello", 5);

	assert_int_equal(send_return, -1);

	// Clean up.

	meshlink_close(mesh1);
	meshlink_destroy("chan_send_conf.7");

	return true;
}

/* Execute meshlink_channel_send Test Case # 4*/
static void test_case_mesh_channel_send_04(void **state) {
	execute_test(test_steps_mesh_channel_send_04, state);
}

/* Test Steps for meshlink_channel_send Test Case # 4*/
static bool test_steps_mesh_channel_send_04(void) {
	struct timespec timeout = {0};
	meshlink_destroy("chan_send_conf.9");
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("chan_send_conf.9", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);

	meshlink_set_channel_accept_cb(mesh1, accept_cb);

	// Start node instance

	assert(meshlink_start(mesh1));

	meshlink_node_t *node = meshlink_get_self(mesh1);
	assert(node);

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, node, 7, receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh1, channel, poll_cb);

	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&poll_lock);

	while(polled == false) {
		assert(!pthread_cond_timedwait(&poll_cond, &poll_lock, &timeout));
	}

	pthread_mutex_unlock(&poll_lock);

	ssize_t send_return = meshlink_channel_send(mesh1, channel, NULL, 5);

	assert_int_equal(send_return, -1);

	// Clean up.

	meshlink_close(mesh1);
	meshlink_destroy("chan_send_conf.9");

	return true;
}

int test_meshlink_channel_send(void) {
	const struct CMUnitTest blackbox_channel_send_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_send_01, NULL, NULL,
		                (void *)&test_mesh_channel_send_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_send_02, NULL, NULL,
		                (void *)&test_mesh_channel_send_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_send_03, NULL, NULL,
		                (void *)&test_mesh_channel_send_03_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_send_04, NULL, NULL,
		                (void *)&test_mesh_channel_send_04_state)
	};

	total_tests += sizeof(blackbox_channel_send_tests) / sizeof(blackbox_channel_send_tests[0]);

	return cmocka_run_group_tests(blackbox_channel_send_tests, NULL, NULL);
}
