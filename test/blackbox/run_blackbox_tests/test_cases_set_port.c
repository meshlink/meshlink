/*
    test_cases_set_port.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_destroy.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../../utils.h"
#include "test_cases_set_port.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <signal.h>
#include <wait.h>
#include <linux/limits.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

#define NUT                         "nut"
#define PEER                        "peer"
#define TEST_MESHLINK_SET_PORT      "test_set_port"
#define create_path(confbase, node_name, test_case_no)   assert(snprintf(confbase, sizeof(confbase), TEST_MESHLINK_SET_PORT "_%ld_%s_%02d", (long) getpid(), node_name, test_case_no) > 0)

static void test_case_set_port_01(void **state);
static bool test_set_port_01(void);
static void test_case_set_port_02(void **state);
static bool test_set_port_02(void);
static void test_case_set_port_03(void **state);
static bool test_set_port_03(void);
static void test_case_set_port_04(void **state);
static bool test_set_port_04(void);

/* State structure for set port API Test Case #1 */
static black_box_state_t test_case_set_port_01_state = {
	.test_case_name = "test_case_set_port_01",
};
/* State structure for set port API Test Case #2 */
static black_box_state_t test_case_set_port_02_state = {
	.test_case_name = "test_case_set_port_02",
};
/* State structure for set port API Test Case #3 */
static black_box_state_t test_case_set_port_03_state = {
	.test_case_name = "test_case_set_port_03",
};
/* State structure for set port API Test Case #4 */
static black_box_state_t test_case_set_port_04_state = {
	.test_case_name = "test_case_set_port_04",
};

static bool try_bind(int portno) {
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	assert_int_not_equal(socket_fd, -1);

	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	bzero(&sin, len);

	assert_int_not_equal(getsockname(socket_fd, (struct sockaddr *)&sin, &len), -1);
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(portno);

	errno = 0;
	int bind_status = bind(socket_fd, (struct sockaddr *)&sin, len);

	// Exempt EADDRINUSE error only

	if(bind_status) {
		assert_int_equal(errno, EADDRINUSE);
	}

	assert_int_not_equal(close(socket_fd), -1);

	return !bind_status;
}

static void wait_for_socket_free(int portno) {

	// Wait upto 20 seconds and poll every second whether the port is freed or not

	for(int i = 0; i < 20; i++) {
		if(try_bind(portno)) {
			return;
		} else {
			sleep(1);
		}
	}

	fail();
}

static int get_free_port(void) {

	// Get a free port

	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	assert_int_not_equal(socket_fd, -1);

	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	bzero(&sin, len);

	assert_int_not_equal(getsockname(socket_fd, (struct sockaddr *)&sin, &len), -1);
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = 0;

	assert_int_not_equal(bind(socket_fd, (struct sockaddr *)&sin, len), -1);

	assert_int_not_equal(getsockname(socket_fd, (struct sockaddr *)&sin, &len), -1);

	assert_int_not_equal(close(socket_fd), -1);

	return (int) sin.sin_port;
}


/* Execute meshlink_set_port Test Case # 1 - valid case*/
static void test_case_set_port_01(void **state) {
	execute_test(test_set_port_01, state);
}
/* Test Steps for meshlink_set_port Test Case # 1 - Valid case

    Test Steps:
    1. Open NUT(Node Under Test)
    2. Set Port for NUT

    Expected Result:
    Set the new port to the NUT.
*/
static bool test_set_port_01(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance

	mesh_handle = meshlink_open("setportconf", "nut", "test", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Get old port and set a new port number

	int port;
	port = meshlink_get_port(mesh_handle);
	assert(port > 0);
	bool ret = meshlink_set_port(mesh_handle, 8000);
	port = meshlink_get_port(mesh_handle);

	assert_int_equal(port, 8000);
	assert_int_equal(ret, true);

	// Clean up

	meshlink_close(mesh_handle);
	assert(meshlink_destroy("setportconf"));
	return true;
}


/* Execute meshlink_set_port Test Case # 2 - Invalid arguments */
static void test_case_set_port_02(void **state) {
	execute_test(test_set_port_02, state);
}

/* Test Steps for meshlink_set_port Test Case # 2 - functionality test

    Test Steps:
    1. Open and start NUT and then pass invalid arguments to the set port API

    Expected Result:
    Meshlink set port API should fail and error out when invalid arguments are passed
*/
static bool test_set_port_02(void) {
	char nut_confbase[PATH_MAX];
	create_path(nut_confbase, NUT, 2);

	// Create meshlink instance

	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, log_cb);
	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_SET_PORT, DEV_CLASS_STATIONARY);
	meshlink_set_log_cb(mesh, TEST_MESHLINK_LOG_LEVEL, log_cb);

	// meshlink_set_port called using NULL as mesh handle

	meshlink_errno = MESHLINK_OK;
	assert_false(meshlink_set_port(NULL, 8000));
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	// Setting port after starting NUT
	meshlink_errno = MESHLINK_OK;
	assert_false(meshlink_set_port(mesh, -1));
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	meshlink_errno = MESHLINK_OK;
	assert_false(meshlink_set_port(mesh, 70000));
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	assert_true(meshlink_start(mesh));
	meshlink_errno = MESHLINK_OK;
	assert_false(meshlink_set_port(mesh, 8000));
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	// Clean up

	meshlink_close(mesh);
	assert_true(meshlink_destroy(nut_confbase));
	return true;
}

/* Execute meshlink_set_port Test Case # 3 - Synchronization testing */
static void test_case_set_port_03(void **state) {
	execute_test(test_set_port_03, state);
}

static bool test_set_port_03(void) {
	pid_t pid;
	int pid_status;
	char nut_confbase[PATH_MAX];
	create_path(nut_confbase, NUT, 3);

	int new_port = get_free_port();

	// Fork a new process in which NUT opens it's instance, set's the new port and raises SIGINT to terminate.

	pid = fork();
	assert_int_not_equal(pid, -1);

	if(!pid) {
		meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
		meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_SET_PORT, DEV_CLASS_STATIONARY);
		assert(mesh);

		assert(meshlink_set_port(mesh, new_port));
		raise(SIGINT);
	}

	// Wait for child exit and verify which signal terminated it

	assert_int_not_equal(waitpid(pid, &pid_status, 0), -1);
	assert_int_equal(WIFSIGNALED(pid_status), true);
	assert_int_equal(WTERMSIG(pid_status), SIGINT);

	// Wait for the NUT's listening socket to be freed. (i.e, preventing meshlink from binding to a new port
	// when NUT instance is reopened and the actual port is not freed due EADDRINUSE)

	wait_for_socket_free(new_port);

	// Reopen the NUT instance in the same test suite

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_SET_PORT, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);

	assert_false(try_bind(new_port));

	// Validate the new port that's being set in the previous instance persists.

	int get_port = meshlink_get_port(mesh);
	assert_int_equal(get_port, new_port);

	// Close the mesh instance and verify that the listening port is closed or not

	meshlink_close(mesh);

	wait_for_socket_free(new_port);

	assert_true(meshlink_destroy(nut_confbase));
	return true;
}


int test_meshlink_set_port(void) {
	const struct CMUnitTest blackbox_set_port_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_set_port_01, NULL, NULL,
		                (void *)&test_case_set_port_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_set_port_02, NULL, NULL,
		                (void *)&test_case_set_port_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_set_port_03, NULL, NULL,
		                (void *)&test_case_set_port_03_state)
	};
	total_tests += sizeof(blackbox_set_port_tests) / sizeof(blackbox_set_port_tests[0]);
	return cmocka_run_group_tests(blackbox_set_port_tests, NULL, NULL);
}
