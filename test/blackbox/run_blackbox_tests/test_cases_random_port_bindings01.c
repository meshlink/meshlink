/*
    test_cases_random_port_bindings01.c -- Execution of specific meshlink black box test cases
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

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

#include "execute_tests.h"
#include "test_cases_random_port_bindings01.h"
#include "../../../src/meshlink.h"
#include "../../../src/devtools.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

static void test_case_mesh_random_port_bindings_01(void **state);
static bool test_steps_mesh_random_port_bindings_01(void);
static void test_case_mesh_random_port_bindings_02(void **state);
static bool test_steps_mesh_random_port_bindings_02(void);
static void test_case_mesh_random_port_bindings_03(void **state);
static bool test_steps_mesh_random_port_bindings_03(void);

/* State structure for meshlink_random_port_bindings Test Case #1 */
static black_box_state_t test_mesh_random_port_bindings_01_state = {
	.test_case_name = "test_case_mesh_random_port_bindings_01",
};

/* State structure for meshlink_random_port_bindings Test Case #2 */
static black_box_state_t test_mesh_random_port_bindings_02_state = {
	.test_case_name = "test_case_mesh_random_port_bindings_02",
};

/* State structure for meshlink_random_port_bindings Test Case #3 */
static black_box_state_t test_mesh_random_port_bindings_03_state = {
	.test_case_name = "test_case_mesh_random_port_bindings_03",
};

static int sockfd = -1, ipv6_fd = -1;

static void log_message(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	(void) mesh;

	static const char *levelstr[] = {
		[MESHLINK_DEBUG] = "\x1b[34mDEBUG",
		[MESHLINK_INFO] = "\x1b[32mINFO",
		[MESHLINK_WARNING] = "\x1b[33mWARNING",
		[MESHLINK_ERROR] = "\x1b[31mERROR",
		[MESHLINK_CRITICAL] = "\x1b[31mCRITICAL",
	};
	fprintf(stderr, "%s:\x1b[0m %s\n", levelstr[level], text);
}

static void occupy_port(int port) {
	int ret_val;
	int mode = 1;
	struct sockaddr_in servaddr;
	struct sockaddr_in6 ipv6addr;

	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_int_not_equal(sockfd, -1);
	memset(&servaddr, 0, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	assert_int_equal(bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)), 0);

	ipv6_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	assert_int_not_equal(ipv6_fd, -1);

	mode = 1;
	setsockopt(ipv6_fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&mode, sizeof(mode));

	memset(&ipv6addr, 0, sizeof(ipv6addr));

	ipv6addr.sin6_family = AF_INET6;
	ipv6addr.sin6_addr   = in6addr_any;
	ipv6addr.sin6_port   = htons(port);

	if((ret_val = bind(ipv6_fd, (const struct sockaddr *)&ipv6addr, sizeof(ipv6addr))) < 0) {
		fprintf(stderr, "Bind to ipv6 failed due to %s\n", strerror(errno));
		assert(false);
	}

	listen(ipv6_fd, 5);

	return;
}

static void occupy_trybind_port(void) {
	occupy_port(10000);
	return;
}

/* Execute meshlink_random_port_bindings Test Case # 1*/
void test_case_mesh_random_port_bindings_01(void **state) {
	execute_test(test_steps_mesh_random_port_bindings_01, state);
}

/* Test Steps for meshlink random port bindings Test Case # 1

    Test Steps:
    1. Open a node instance
    2. Bind a Socket on port 10000
    3. Call meshlink_set_port() with same port 10000

    Expected Result:
    The meshlink_set_port() API should fail and the Listening Port
    of the instance should be unchanged.
*/
bool test_steps_mesh_random_port_bindings_01(void) {
	meshlink_handle_t *relay = NULL;
	meshlink_destroy("relay_conf");

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_message);

	relay = meshlink_open("relay_conf", "relay", "test", DEV_CLASS_BACKBONE);
	fprintf(stderr, "Got mesh handle %p\n", (void *)relay);
	assert_non_null(relay);

	meshlink_set_log_cb(relay, MESHLINK_DEBUG, log_message);
	meshlink_enable_discovery(relay, false);

	assert_true(meshlink_start(relay));

	occupy_port(10000);

	meshlink_stop(relay);
	fprintf(stderr, "Meshlink stop returned\n");

	assert_int_equal(meshlink_set_port(relay, 10000), false);
	fprintf(stderr, "Meshlink set port returned\n");

	close(sockfd);
	close(ipv6_fd);

	sockfd = -1;
	ipv6_fd = -1;

	meshlink_close(relay);
	meshlink_destroy("relay_conf");

	return true;
}

/* Execute meshlink_blacklist Test Case # 2*/
void test_case_mesh_random_port_bindings_02(void **state) {
	execute_test(test_steps_mesh_random_port_bindings_02, state);
}

/* Test Steps for meshlink random port bindings Test Case # 2

    Test Steps:
    1. Open a node and start the instance.
    2. Call meshlink_set_port() with port 10000
    3. When try bind succeds block the port using devtool_trybind_probe() callback.

    Expected Result:
    The meshlink_set_port() API should fail.
*/
bool test_steps_mesh_random_port_bindings_02(void) {
	meshlink_handle_t *relay = NULL;
	meshlink_destroy("relay_conf");

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_message);

	relay = meshlink_open("relay_conf", "relay", "test", DEV_CLASS_BACKBONE);
	fprintf(stderr, "Got mesh handle %p\n", (void *)relay);
	assert_non_null(relay);

	meshlink_set_log_cb(relay, MESHLINK_DEBUG, log_message);
	meshlink_enable_discovery(relay, false);

	assert_true(meshlink_start(relay));

	sleep(1);

	devtool_trybind_probe = occupy_trybind_port;
	meshlink_stop(relay);

	assert_int_equal(meshlink_set_port(relay, 10000), false);

	close(sockfd);
	close(ipv6_fd);

	sockfd = -1;
	ipv6_fd = -1;

	meshlink_close(relay);
	meshlink_destroy("relay_conf");
	return true;
}

/* Execute meshlink_blacklist Test Case # 3*/
void test_case_mesh_random_port_bindings_03(void **state) {
	execute_test(test_steps_mesh_random_port_bindings_03, state);
}

/* Test Steps for meshlink random port bindings Test Case # 3

    Test Steps:
    1. Open a node and start the instance.
    2. Retrieve the port number of current instance using meshlink_get_port().
    3. Close the instance and try to occupy the meshlink instance port.
    4. Start the instance again with same confdir.

    Expected Result:
    The meshlink instance should start with a new random port different to
    previous port number.
*/
bool test_steps_mesh_random_port_bindings_03(void) {
	int port, new_port;
	meshlink_handle_t *relay = NULL;
	meshlink_destroy("relay_conf");

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_message);

	relay = meshlink_open("relay_conf", "relay", "test", DEV_CLASS_BACKBONE);
	fprintf(stderr, "Got mesh handle %p\n", (void *)relay);
	assert_non_null(relay);

	meshlink_set_log_cb(relay, MESHLINK_DEBUG, log_message);
	meshlink_enable_discovery(relay, false);

	assert_true(meshlink_start(relay));
	port = meshlink_get_port(relay);

	meshlink_close(relay);

	occupy_port(port);

	relay = meshlink_open("relay_conf", "relay", "test", DEV_CLASS_BACKBONE);
	fprintf(stderr, "Got mesh handle %p\n", (void *)relay);
	assert_non_null(relay);

	meshlink_set_log_cb(relay, MESHLINK_DEBUG, log_message);
	meshlink_enable_discovery(relay, false);

	assert_true(meshlink_start(relay));

	new_port = meshlink_get_port(relay);

	assert_int_not_equal(port, new_port);

	close(sockfd);
	close(ipv6_fd);

	sockfd = -1;
	ipv6_fd = -1;

	meshlink_close(relay);
	meshlink_destroy("relay_conf");
	return true;
}

int test_meshlink_random_port_bindings01(void) {
	const struct CMUnitTest blackbox_random_port_bindings_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_random_port_bindings_01, NULL, NULL,
		                (void *)&test_mesh_random_port_bindings_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_random_port_bindings_02, NULL, NULL,
		                (void *)&test_mesh_random_port_bindings_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_random_port_bindings_03, NULL, NULL,
		                (void *)&test_mesh_random_port_bindings_03_state)
	};

	total_tests += sizeof(blackbox_random_port_bindings_tests) / sizeof(blackbox_random_port_bindings_tests[0]);

	return cmocka_run_group_tests(blackbox_random_port_bindings_tests, NULL, NULL);
}
