/*
    test_cases_blacklist.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_blacklist.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

static void test_case_mesh_blacklist_01(void **state);
static bool test_steps_mesh_blacklist_01(void);
static void test_case_mesh_blacklist_02(void **state);
static bool test_steps_mesh_blacklist_02(void);
static void test_case_mesh_blacklist_03(void **state);
static bool test_steps_mesh_blacklist_03(void);

/* State structure for meshlink_blacklist Test Case #1 */
static black_box_state_t test_mesh_blacklist_01_state = {
	.test_case_name = "test_case_mesh_blacklist_01",
};

/* State structure for meshlink_blacklist Test Case #2 */
static black_box_state_t test_mesh_blacklist_02_state = {
	.test_case_name = "test_case_mesh_blacklist_02",
};

/* State structure for meshlink_blacklist Test Case #3 */
static black_box_state_t test_mesh_blacklist_03_state = {
	.test_case_name = "test_case_mesh_blacklist_03",
};

/* Execute meshlink_blacklist Test Case # 1*/
void test_case_mesh_blacklist_01(void **state) {
	execute_test(test_steps_mesh_blacklist_01, state);
}

static bool received;

static void receive(meshlink_handle_t *mesh, meshlink_node_t *src, const void *data, size_t len) {
	const char *msg = data;
	assert(len);

	if(!strcmp(src->name, "bar") && len == 5 && !strcmp(msg, "test")) {
		received = true;
	}
}

static bool bar_reachable;

static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	if(!strcmp(node->name, "bar") && reachable) {
		bar_reachable = true;
	}
}


/* Test Steps for meshlink_blacklist Test Case # 1

    Test Steps:
    1. Open both the node instances
    2. Join bar node with foo and Send & Receive data
    3. Blacklist bar and Send & Receive data

    Expected Result:
    When default blacklist is disabled, foo node should receive data from bar
    but when enabled foo node should not receive data
*/
bool test_steps_mesh_blacklist_01(void) {
	meshlink_destroy("blacklist_conf.1");
	meshlink_destroy("blacklist_conf.2");

	// Open two new meshlink instance.
	meshlink_handle_t *mesh1 = meshlink_open("blacklist_conf.1", "foo", "blacklist", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh2 = meshlink_open("blacklist_conf.2", "bar", "blacklist", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_receive_cb(mesh1, receive);

	// Start both instances
	bar_reachable = false;
	meshlink_set_node_status_cb(mesh1, status_cb);
	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));
	sleep(1);

	char *foo_export = meshlink_export(mesh1);
	assert(foo_export != NULL);
	assert(meshlink_import(mesh2, foo_export));
	char *bar_export = meshlink_export(mesh2);
	assert(meshlink_import(mesh1, bar_export));
	sleep(5);
	assert_int_equal(bar_reachable, true);

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	assert(bar);
	meshlink_node_t *foo = meshlink_get_node(mesh2, "foo");
	assert(foo);

	received = false;
	assert(meshlink_send(mesh2, foo, "test", 5));
	sleep(1);
	assert(received);

	meshlink_blacklist(mesh1, bar);

	received = false;
	assert(meshlink_send(mesh2, foo, "test", 5));
	sleep(1);
	assert_int_equal(received, false);

	// Clean up.
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("blacklist_conf.1");
	meshlink_destroy("blacklist_conf.2");
	return true;
}

/* Execute meshlink_blacklist Test Case # 2*/
void test_case_mesh_blacklist_02(void **state) {
	execute_test(test_steps_mesh_blacklist_02, state);
}


/* Test Steps for meshlink_blacklist Test Case # 2

    Test Steps:
    1. Calling meshlink_blacklist with NULL as mesh handle argument.

    Expected Result:
    meshlink_blacklist API handles the invalid parameter when called by giving proper error number.
*/
bool test_steps_mesh_blacklist_02(void) {
	meshlink_destroy("blacklist_conf.3");

	// Open two new meshlink instance.
	meshlink_handle_t *mesh = meshlink_open("blacklist_conf.3", "foo", "blacklist", DEV_CLASS_BACKBONE);
	assert(mesh != NULL);

	meshlink_node_t *node = meshlink_get_self(mesh);
	assert(node);

	// Passing NULL as mesh handle and node handle being some valid node handle
	meshlink_blacklist(NULL, node);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	// Clean up.
	meshlink_close(mesh);
	meshlink_destroy("blacklist_conf.3");
	return true;
}

/* Execute meshlink_blacklist Test Case # 3*/
void test_case_mesh_blacklist_03(void **state) {
	execute_test(test_steps_mesh_blacklist_03, state);
}

/* Test Steps for meshlink_blacklist Test Case # 3

    Test Steps:
    1. Create node instance
    2. Calling meshlink_blacklist with NULL as node handle argument.

    Expected Result:
    meshlink_blacklist API handles the invalid parameter when called by giving proper error number.
*/
bool test_steps_mesh_blacklist_03(void) {
	meshlink_destroy("blacklist_conf.4");

	// Open two new meshlink instance.
	meshlink_handle_t *mesh = meshlink_open("blacklist_conf.4", "foo", "blacklist", DEV_CLASS_BACKBONE);
	assert(mesh != NULL);

	// Passing NULL as node handle and mesh handle being some valid mesh handle value
	meshlink_blacklist(mesh, NULL);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	// Clean up.
	meshlink_close(mesh);
	meshlink_destroy("blacklist_conf.4");
	return true;
}

int test_meshlink_blacklist(void) {
	const struct CMUnitTest blackbox_blacklist_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_blacklist_01, NULL, NULL,
		(void *)&test_mesh_blacklist_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_blacklist_02, NULL, NULL,
		(void *)&test_mesh_blacklist_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_blacklist_03, NULL, NULL,
		(void *)&test_mesh_blacklist_03_state)
	};

	total_tests += sizeof(blackbox_blacklist_tests) / sizeof(blackbox_blacklist_tests[0]);

	return cmocka_run_group_tests(blackbox_blacklist_tests, NULL, NULL);
}

