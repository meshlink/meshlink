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

#ifdef NDEBUG
#undef NDEBUG
#endif

#include "execute_tests.h"
#include "test_cases_channel_ex.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../../utils.h"
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <pthread.h>
#include <cmocka.h>
#include <limits.h>
#include <linux/limits.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG
/* Modify this to change the port number */
#define PORT 8000

#define NUT                         "nut"
#define PEER                        "peer"
#define TEST_CHANNEL_OPEN           "test_channel_open"
#define create_path(confbase, node_name, test_case_no)   assert(snprintf(confbase, sizeof(confbase), TEST_CHANNEL_OPEN "_%ld_%s_%02d", (long) getpid(), node_name, test_case_no) > 0)

typedef struct test_cb_data {
	size_t cb_data_len;
	size_t cb_total_data_len;
	int total_cb_count;
	void (*cb_handler)(void);
	bool cb_flag;
} test_cb_t;

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
static void test_case_channel_ex_07(void **state);
static bool test_steps_channel_ex_07(void);

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
static black_box_state_t test_case_channel_ex_07_state = {
	.test_case_name = "test_case_channel_ex_07",
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
	assert(meshlink_destroy("channelexconf"));

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
	assert(meshlink_destroy("channelexconf"));
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
	assert(meshlink_destroy("channelexconf"));
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
	assert(meshlink_destroy("channelexconf"));
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
	assert(meshlink_destroy("channelexconf"));
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
	assert(meshlink_destroy("channelexconf"));
	return true;
}

static test_cb_t recv_cb_data;
static test_cb_t nut_recv_cb_data;

/* Peer node's receive callback handler */
static void peer_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)data;

	(recv_cb_data.total_cb_count)++;
	recv_cb_data.cb_total_data_len += len;
	recv_cb_data.cb_data_len = len;

	assert_int_equal(meshlink_channel_send(mesh, channel, data, len), len);
}

/* NUT's receive callback handler */
static void nut_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)data;

	(nut_recv_cb_data.total_cb_count)++;
	nut_recv_cb_data.cb_total_data_len += len;
	nut_recv_cb_data.cb_data_len = len;

}

/* NUT's poll callback handler */
static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)mesh;
	(void)channel;

	fail();
}

static bool peer_accept_flag;

/* Peer node's accept callback handler */
static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)port;
	(void)data;
	(void)len;

	channel->node->priv = channel;
	meshlink_set_channel_receive_cb(mesh, channel, peer_receive_cb);
	return peer_accept_flag;
}

/* Execute meshlink_channel_open_ex Test Case # 7 - UDP channel corner cases */
static void test_case_channel_ex_07(void **state) {
	execute_test(test_steps_channel_ex_07, state);
}

static bool test_steps_channel_ex_07(void) {
	char nut_confbase[PATH_MAX];
	char peer_confbase[PATH_MAX];
	char *buffer;
	size_t mss_size;
	size_t send_size;
	meshlink_channel_t *channel_peer;
	create_path(nut_confbase, NUT, 5);
	create_path(peer_confbase, PEER, 5);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_CHANNEL_OPEN, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);
	meshlink_handle_t *mesh_peer = meshlink_open(peer_confbase, PEER, TEST_CHANNEL_OPEN, DEV_CLASS_STATIONARY);
	assert_non_null(mesh_peer);

	link_meshlink_pair(mesh, mesh_peer);
	meshlink_set_channel_accept_cb(mesh_peer, accept_cb);
	bzero(&recv_cb_data, sizeof(recv_cb_data));

	meshlink_node_t *node = meshlink_get_node(mesh, PEER);
	meshlink_node_t *node_peer = meshlink_get_node(mesh_peer, NUT);
	assert_true(meshlink_start(mesh));
	assert_true(meshlink_start(mesh_peer));

	/* 1. Peer rejects the channel that's being opened by NUT, when data is sent on that rejected channel
	        it should not lead to any undefined behavior and the peer should ignore the data sent */

	peer_accept_flag = false;
	meshlink_channel_t *channel = meshlink_channel_open_ex(mesh, node, PORT, nut_receive_cb, NULL, 0, MESHLINK_CHANNEL_UDP);
	assert_non_null(channel);

	assert_after(node_peer->priv, 5);
	assert_after((nut_recv_cb_data.total_cb_count == 1), 5);
	assert_int_equal(nut_recv_cb_data.cb_data_len, 0);

	mss_size = meshlink_channel_get_mss(mesh, channel);

	if((mss_size != -1) && !mss_size) {
		buffer = malloc(mss_size);
		assert_non_null(buffer);
		assert_int_equal(meshlink_channel_send(mesh, channel, buffer, mss_size), mss_size);
		sleep(5);
		assert_int_equal(nut_recv_cb_data.total_cb_count, 0);
		free(buffer);
	}

	meshlink_channel_close(mesh, channel);

	/* 2. Open channel to an offline node and sleep for 30 seconds once the offline node comes back online
	    both the nodes should be able to create the channel */

	peer_accept_flag = true;
	meshlink_stop(mesh_peer);
	node_peer->priv = NULL;
	channel = meshlink_channel_open_ex(mesh, node, PORT, nut_receive_cb, NULL, 0, MESHLINK_CHANNEL_UDP);
	assert_non_null(channel);

	sleep(30);
	assert_true(meshlink_start(mesh_peer));

	// Peer set's this while accepting channel

	assert_after(node_peer->priv, 5);

	/* 2. Active UDP channel should be able to do bi-directional data transfer */

	bzero(&recv_cb_data, sizeof(recv_cb_data));
	bzero(&nut_recv_cb_data, sizeof(nut_recv_cb_data));
	buffer = malloc(mss_size);
	assert_non_null(buffer);
	mss_size = meshlink_channel_get_mss(mesh, channel);
	assert_int_not_equal(mss_size, -1);
	send_size = mss_size;
	assert_int_equal(meshlink_channel_send(mesh, channel, buffer, send_size), send_size);
	assert_after((recv_cb_data.cb_total_data_len == send_size), 5);
	assert_int_equal(recv_cb_data.total_cb_count, 1);
	assert_after((nut_recv_cb_data.cb_total_data_len == send_size), 5);
	assert_int_equal(nut_recv_cb_data.total_cb_count, 1);

	/* 3. Set poll callback for an UDP channel - Even though poll callback's return value is void
	        according to the design poll callback is meant only for TCP channel. */

	// Set the poll callback and sleep for 5 seconds, fail the test case if poll callback gets invoked

	meshlink_set_channel_poll_cb(mesh, channel, poll_cb);
	sleep(5);

	/* 4. Sent data on the active channel with data length more than the obtained MSS value.
	        It's expected that peer node doesn't receive it if received then the MSS calculations might be wrong */

	bzero(&recv_cb_data, sizeof(recv_cb_data));
	send_size = mss_size + 100;
	assert_int_equal(meshlink_channel_send(mesh, channel, buffer, send_size), send_size);
	sleep(5);
	assert_int_equal(recv_cb_data.total_cb_count, 0);

	/* 5. Sent the minimum data (here 1 byte) possible to the peer node via the active UDP channel */

	bzero(&recv_cb_data, sizeof(recv_cb_data));
	send_size = 1;
	assert_int_equal(meshlink_channel_send(mesh, channel, buffer, send_size), send_size);
	assert_after((recv_cb_data.cb_total_data_len == send_size), 5);
	assert_int_equal(recv_cb_data.total_cb_count, 1);

	/* 6. Sent more than maximum allowed data i.e, > UDP max length */

	bzero(&recv_cb_data, sizeof(recv_cb_data));
	send_size = USHRT_MAX + 2; // 65537 bytes should fail
	assert_int_equal(meshlink_channel_send(mesh, channel, buffer, send_size), -1);
	sleep(5);
	assert_int_equal(recv_cb_data.total_cb_count, 0);

	/* 7. Pass get MSS API with NULL as mesh handle */

	assert_int_equal(meshlink_channel_get_mss(NULL, channel), -1);

	/* 8. Pass get MSS API with NULL as channel handle */

	assert_int_equal(meshlink_channel_get_mss(mesh, NULL), -1);

	/* 9. Obtained MSS value should be less than PMTU value */

	ssize_t pmtu_size = meshlink_get_pmtu(mesh, node);
	assert_int_not_equal(pmtu_size, -1);
	assert_true(mss_size <= pmtu_size);

	/* 10. Close/free the channel at the NUT's end, but when peer node still tries to send data on that channel
	        meshlink should gracefully handle it */

	bzero(&recv_cb_data, sizeof(recv_cb_data));
	bzero(&nut_recv_cb_data, sizeof(nut_recv_cb_data));
	recv_cb_data.cb_data_len = 1;
	meshlink_channel_close(mesh, channel);
	assert_after((recv_cb_data.total_cb_count == 1), 5);
	assert_int_equal(recv_cb_data.cb_data_len, 0);

	channel_peer = node_peer->priv;
	send_size = mss_size / 2;
	assert_int_equal(meshlink_channel_send(mesh_peer, channel_peer, buffer, send_size), send_size);
	sleep(5);
	assert_int_equal(nut_recv_cb_data.total_cb_count, 0);

	/* 11. Getting MSS value on a node which is closed by other node but not freed/closed by the host node */

	assert_int_equal(meshlink_channel_get_mss(mesh_peer, channel_peer), -1);

	// Cleanup

	free(buffer);
	meshlink_close(mesh);
	meshlink_close(mesh_peer);
	assert_true(meshlink_destroy(nut_confbase));
	assert_true(meshlink_destroy(peer_confbase));
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
		                (void *)&test_case_channel_ex_06_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_07, NULL, NULL,
		                (void *)&test_case_channel_ex_07_state)
	};

	total_tests += sizeof(blackbox_channel_ex_tests) / sizeof(blackbox_channel_ex_tests[0]);

	assert(pthread_mutex_init(&lock, NULL) == 0);
	int failed = cmocka_run_group_tests(blackbox_channel_ex_tests, NULL, NULL);
	assert(pthread_mutex_destroy(&lock) == 0);

	return failed;
}
