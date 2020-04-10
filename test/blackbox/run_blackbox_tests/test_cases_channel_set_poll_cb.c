/*
    test_cases_channel_set_poll_cb.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_channel_set_poll_cb.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../../utils.h"
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <pthread.h>
#include <linux/limits.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG
/* Modify this to change the port number */
#define PORT 8000

#define NUT                         "nut"
#define PEER                        "peer"
#define TEST_POLL_CB                "test_poll_cb"
#define create_path(confbase, node_name, test_case_no)   assert(snprintf(confbase, sizeof(confbase), TEST_POLL_CB "_%ld_%s_%02d", (long) getpid(), node_name, test_case_no) > 0)

typedef struct test_cb_data {
	size_t cb_data_len;
	size_t cb_total_data_len;
	int total_cb_count;
	void (*cb_handler)(void);
	bool cb_flag;
} test_cb_t;

static void test_case_channel_set_poll_cb_01(void **state);
static bool test_steps_channel_set_poll_cb_01(void);
static void test_case_channel_set_poll_cb_02(void **state);
static bool test_steps_channel_set_poll_cb_02(void);
static void test_case_channel_set_poll_cb_03(void **state);
static bool test_steps_channel_set_poll_cb_03(void);
static void test_case_channel_set_poll_cb_04(void **state);
static bool test_steps_channel_set_poll_cb_04(void);
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
static black_box_state_t test_case_channel_set_poll_cb_04_state = {
	.test_case_name = "test_case_channel_set_poll_cb_04",
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
}

static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *source, bool reach) {
	(void)mesh;
	(void)source;

	if(!reach) {
		return;
	}

	pthread_mutex_lock(&reachable_lock);
	reachable = true;
	assert(!pthread_cond_broadcast(&reachable_cond));
	pthread_mutex_unlock(&reachable_lock);
}

/* Execute meshlink_channel_set_poll_cb Test Case # 1 */
static void test_case_channel_set_poll_cb_01(void **state) {
	execute_test(test_steps_channel_set_poll_cb_01, state);
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
	assert(meshlink_destroy("pollconf1"));
	assert(meshlink_destroy("pollconf2"));
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
	assert(meshlink_destroy("pollconf1"));
	assert(meshlink_destroy("pollconf2"));

	return true;
}

/* Execute meshlink_channel_set_poll_cb Test Case # 2 */
static void test_case_channel_set_poll_cb_02(void **state) {
	execute_test(test_steps_channel_set_poll_cb_02, state);
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
	assert_int_not_equal(meshlink_errno, 0);

	meshlink_close(mesh_handle);
	assert(meshlink_destroy("channelpollconf3"));
	return true;
}

/* Execute meshlink_channel_set_poll_cb Test Case # 3 */
static void test_case_channel_set_poll_cb_03(void **state) {
	execute_test(test_steps_channel_set_poll_cb_03, state);
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
	assert(meshlink_destroy("channelpollconf4"));
	return true;
}

static test_cb_t poll_cb_data;
static test_cb_t recv_cb_data;
static meshlink_handle_t *mesh;

/* Peer node channel receive callback's internal handler function - Blocks for 2 seconds whenever it gets invoked */
static void recv_cb_data_handler(void) {
	static int poll_cb_last_count;

	// Sleep for 1 second to allow NUT's callback to invoke already scheduled callbacks
	// i.e, this sleeps prevents a condition where if the flag is set assuming that
	// further callbacks are invalid then pending poll callbacks can misinterpreted as invalid.
	// TODO: Fix this race condition in the test case without sleep()

	sleep(1);

	// Make sure there is change in the cumulative poll callbacks count

	if(!poll_cb_last_count) {
		poll_cb_last_count = poll_cb_data.total_cb_count;
	} else {
		assert(poll_cb_data.total_cb_count > poll_cb_last_count);
	}

	// Set the receive callback block flag and reset it back after 2 seconds sleep

	recv_cb_data.cb_flag = true;
	sleep(2);
	recv_cb_data.cb_flag = false;
}

/* Peer node channel receive callback's internal handler function - Stops the NUT's instance and
    resets it's own internal handler */
static void recv_cb_data_handler2(void) {

	// Stop the NUT's meshlink instance, set the receive callback flag indicating that further
	// poll callbacks are considered to be invalid

	meshlink_stop(mesh);
	recv_cb_data.cb_flag = true;

	// Reset the callback handler (i.e, this is a one-time operation)

	recv_cb_data.cb_handler = NULL;
}

/* Peer node channel receive callback's internal handler function - Blocks for straight 5 seconds and
    resets it's own internal handler */
static void recv_cb_data_handler3(void) {
	sleep(5);
	recv_cb_data.cb_handler = NULL;
	recv_cb_data.cb_flag = false;
}

/* NUT channel poll callback's internal handler function - Assert on peer node receive callback's flag */
static void poll_cb_data_handler(void) {
	assert_false(recv_cb_data.cb_flag);
}

/* Peer node's receive callback handler */
static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)data;

	// printf("Received %zu bytes\n", len);
	(recv_cb_data.total_cb_count)++;
	recv_cb_data.cb_total_data_len += len;
	recv_cb_data.cb_data_len = len;

	if(recv_cb_data.cb_handler) {
		recv_cb_data.cb_handler();
	}
}

/* NUT's poll callback handler */
static void poll_cb2(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)mesh;
	(void)channel;

	// printf("Poll len: %zu\n", len);
	assert(len);
	(poll_cb_data.total_cb_count)++;
	poll_cb_data.cb_total_data_len += len;
	poll_cb_data.cb_data_len = len;

	if(poll_cb_data.cb_handler) {
		(poll_cb_data.cb_handler)();
	}
}

/* Peer node's accept callback handler */
static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)port;
	(void)data;
	(void)len;

	channel->node->priv = channel;
	meshlink_set_channel_receive_cb(mesh, channel, receive_cb);
	return true;
}

/* Execute meshlink_channel_set_poll_cb Test Case # 4 - Corner cases */
static void test_case_channel_set_poll_cb_04(void **state) {
	execute_test(test_steps_channel_set_poll_cb_04, state);
}
/* Test Steps for meshlink_channel_set_poll_cb Test Case # 4

    Test Scenarios:
        1. Validate that Node-Under-Test never invokes the poll callback from a silent channel, here 65 seconds
        2. Send some data on to the data channel and block the reader end of the channel for a while where
            at that instance nUT should not invokes any periodic callbacks. Once the peer node unblocks it's
            instance it should continue to invokes callbacks.
            This should repeat until the NUT channel sends it's complete data or the poll callback invokes with
            max default size as length.
        3. Send a big packet of maximum send buffer size where length becomes 0 bytes, still NUT channel
            should not invoke 0 length callback. Make sure by blocking the receiver and assert on the poll callback.
        4. NUT channel should not invoke the poll callback after self node going offline or due to it's reachability status.
        5. Modify the channel send buffer queue size which should be the new poll callback size further.
*/
static bool test_steps_channel_set_poll_cb_04(void) {
	char nut_confbase[PATH_MAX];
	char peer_confbase[PATH_MAX];
	create_path(nut_confbase, NUT, 4);
	create_path(peer_confbase, PEER, 4);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	mesh = meshlink_open(nut_confbase, NUT, TEST_POLL_CB, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);
	meshlink_handle_t *mesh_peer = meshlink_open(peer_confbase, PEER, TEST_POLL_CB, DEV_CLASS_STATIONARY);
	assert_non_null(mesh_peer);

	link_meshlink_pair(mesh, mesh_peer);
	meshlink_set_channel_accept_cb(mesh_peer, accept_cb);

	assert_true(meshlink_start(mesh));
	assert_true(meshlink_start(mesh_peer));
	meshlink_node_t *node = meshlink_get_node(mesh, PEER);

	/* 1. Accept and stay idle for 65 seconds */

	bzero(&poll_cb_data, sizeof(poll_cb_data));
	bzero(&recv_cb_data, sizeof(recv_cb_data));

	meshlink_channel_t *channel = meshlink_channel_open(mesh, node, PORT, NULL, NULL, 0);
	assert_non_null(channel);
	meshlink_set_channel_poll_cb(mesh, channel, poll_cb2);
	sleep(65);
	assert_int_equal(poll_cb_data.total_cb_count, 1);
	assert_int_not_equal(poll_cb_data.cb_data_len, 0);
	size_t default_max_size = poll_cb_data.cb_data_len;

	// Send 7 MSS size packet

	char *buffer = malloc(default_max_size);
	assert_non_null(buffer);
	size_t send_size = default_max_size;
	bzero(&poll_cb_data, sizeof(poll_cb_data));
	bzero(&recv_cb_data, sizeof(recv_cb_data));

	size_t mss_size = meshlink_channel_get_mss(mesh, channel);
	assert_int_not_equal(mss_size, -1);

	if(mss_size * 7 <= default_max_size) {
		send_size = mss_size * 7;
	}

	/* 2. Validate whether poll callback is invoked in case of channel is holding data in send buffer for a while */

	bzero(&poll_cb_data, sizeof(poll_cb_data));
	bzero(&recv_cb_data, sizeof(recv_cb_data));
	poll_cb_data.cb_handler = poll_cb_data_handler;
	recv_cb_data.cb_handler = recv_cb_data_handler;
	assert_int_equal(meshlink_channel_send(mesh, channel, buffer, send_size), send_size);
	assert_after(poll_cb_data.cb_data_len == default_max_size, 60);
	assert_int_equal(recv_cb_data.cb_total_data_len, send_size);

	/* 3. On sending max send buffer sized packed on a channel should not invoke callback with length 0 */

	bzero(&poll_cb_data, sizeof(poll_cb_data));
	bzero(&recv_cb_data, sizeof(recv_cb_data));
	poll_cb_data.cb_handler = poll_cb_data_handler;
	recv_cb_data.cb_handler = recv_cb_data_handler3;
	recv_cb_data.cb_flag = true;
	assert_int_equal(meshlink_channel_send(mesh, channel, buffer, default_max_size), default_max_size);
	assert_after(poll_cb_data.cb_data_len == default_max_size, 60);


	/* 4. Poll callback should not be invoked when the self node is offline and it has data in it's buffer */

	bzero(&recv_cb_data, sizeof(recv_cb_data));
	recv_cb_data.cb_handler = recv_cb_data_handler2;
	poll_cb_data.cb_handler = poll_cb_data_handler;
	assert_int_equal(meshlink_channel_send(mesh, channel, buffer, send_size), send_size);
	assert_after(recv_cb_data.cb_flag, 20);
	sleep(2);
	assert_int_equal(meshlink_channel_send(mesh, channel, buffer, 50), 50);
	sleep(2);
	recv_cb_data.cb_flag = false;
	assert_true(meshlink_start(mesh));
	assert_after(poll_cb_data.cb_data_len == default_max_size, 10);

	/* 5. Changing the sendq size should reflect on the poll callback length */

	bzero(&poll_cb_data, sizeof(poll_cb_data));
	bzero(&recv_cb_data, sizeof(recv_cb_data));

	size_t new_size = meshlink_channel_get_mss(mesh, channel) * 3;
	assert_int_not_equal(new_size, -1);
	meshlink_set_channel_sndbuf(mesh, channel, new_size);
	assert_after(new_size == poll_cb_data.cb_data_len, 5);
	send_size = new_size / 2;
	assert_int_equal(meshlink_channel_send(mesh, channel, buffer, send_size), send_size);
	assert_after(new_size == poll_cb_data.cb_data_len, 5);

	/* If peer node's channel is closed/freed but the host node is not freed then poll callback with length 0 is expected */

	/*assert_int_not_equal(poll_cb_data.cb_total_data_len, 0);

	meshlink_node_t *channel_peer = node_peer->priv;
	meshlink_channel_close(mesh_peer, channel_peer);
	assert_after(!poll_cb_data.cb_data_len, 60);*/

	// Cleanup

	free(buffer);
	meshlink_close(mesh);
	meshlink_close(mesh_peer);
	assert_true(meshlink_destroy(nut_confbase));
	assert_true(meshlink_destroy(peer_confbase));
	return true;
}

int test_meshlink_set_channel_poll_cb(void) {
	const struct CMUnitTest blackbox_channel_set_poll_cb_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_set_poll_cb_01, NULL, NULL,
		                (void *)&test_case_channel_set_poll_cb_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_set_poll_cb_02, NULL, NULL,
		                (void *)&test_case_channel_set_poll_cb_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_set_poll_cb_03, NULL, NULL,
		                (void *)&test_case_channel_set_poll_cb_03_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_set_poll_cb_04, NULL, NULL,
		                (void *)&test_case_channel_set_poll_cb_04_state)
	};
	total_tests += sizeof(blackbox_channel_set_poll_cb_tests) / sizeof(blackbox_channel_set_poll_cb_tests[0]);

	return cmocka_run_group_tests(blackbox_channel_set_poll_cb_tests, NULL, NULL);
}
