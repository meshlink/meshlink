/*
    test_cases_default_blacklist.c -- Execution of specific meshlink black box test cases
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

#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>
#include "execute_tests.h"
#include "test_cases_default_blacklist.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../../utils.h"

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_mesh_default_blacklist_01(void **state);
static bool test_steps_mesh_default_blacklist_01(void);
static void test_case_mesh_default_blacklist_02(void **state);
static bool test_steps_mesh_default_blacklist_02(void);

/* State structure for meshlink_default_blacklist Test Case #1 */
static black_box_state_t test_mesh_default_blacklist_01_state = {
	.test_case_name = "test_case_mesh_default_blacklist_01",
};

/* State structure for meshlink_default_blacklist Test Case #2 */
static black_box_state_t test_mesh_default_blacklist_02_state = {
	.test_case_name = "test_case_mesh_default_blacklist_02",
};

/* Execute meshlink_default_blacklist Test Case # 1*/
static void test_case_mesh_default_blacklist_01(void **state) {
	execute_test(test_steps_mesh_default_blacklist_01, state);
	return;
}

static bool received = false;

static void receive(meshlink_handle_t *mesh, meshlink_node_t *src, const void *data, size_t len) {
	(void)mesh;
	(void)data;

	assert(len);

	if(!strcmp(src->name, "bar") || !strcmp(src->name, "foz")) {
		received = true;
	}
}

static bool bar_reachable = false;
static bool foz_reachable = false;

void status_cb1(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(!strcmp(node->name, "bar")) {
		bar_reachable = reachable;
	} else if(!strcmp(node->name, "foz")) {
		foz_reachable = reachable;
	}
}

/* Test Steps for meshlink_default_blacklist Test Case # 1

    Test Steps:
    1. Open all the node instances & Disable default blacklist
    2. Join bar node with foo and Send & Receive data
    3. Enable default blacklist and join foz node with foo node
        and follow the steps done for bar node

    Expected Result:
    When default blacklist is disabled, foo node should receive data from bar
    but when enabled foo node should not receive data from foz
*/
static bool test_steps_mesh_default_blacklist_01(void) {
	assert(meshlink_destroy("def_blacklist_conf.1"));
	assert(meshlink_destroy("def_blacklist_conf.2"));
	assert(meshlink_destroy("def_blacklist_conf.3"));

	// Open two new meshlink instance.
	meshlink_handle_t *mesh1 = meshlink_open("def_blacklist_conf.1", "foo", "blacklist", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh2 = meshlink_open("def_blacklist_conf.2", "bar", "blacklist", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh3 = meshlink_open("def_blacklist_conf.3", "foz", "blacklist", DEV_CLASS_BACKBONE);
	assert(mesh3);
	meshlink_set_log_cb(mesh3, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_set_receive_cb(mesh1, receive);

	meshlink_set_default_blacklist(mesh1, false);

	// Start both instances
	bar_reachable = false;
	foz_reachable = false;
	meshlink_set_node_status_cb(mesh1, status_cb1);
	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));
	assert(meshlink_start(mesh3));
	sleep(1);

	char *foo_export = meshlink_export(mesh1);
	assert(foo_export != NULL);
	assert(meshlink_import(mesh2, foo_export));
	char *bar_export = meshlink_export(mesh2);
	assert(meshlink_import(mesh1, bar_export));
	sleep(5);
	assert(bar_reachable);

	// Nodes should learn about each other
	meshlink_node_t *foo = NULL;
	foo = meshlink_get_node(mesh2, "foo");
	assert(foo);

	received = false;
	assert(meshlink_send(mesh2, foo, "test", 5));
	assert_after(received, 2);

	// Enable default blacklist and join another node
	meshlink_set_default_blacklist(mesh1, true);

	char *foz_export = meshlink_export(mesh3);
	assert(foz_export);
	assert(meshlink_import(mesh1, foz_export));
	assert(meshlink_import(mesh3, foo_export));
	sleep(5);
	assert(foz_reachable);

	foo = meshlink_get_node(mesh3, "foo");
	assert(foo);
	assert(meshlink_send(mesh3, foo, "test", 5));
	received = false;
	assert(meshlink_send(mesh3, foo, "test", 5));
	assert_after(!received, 2);

	// Clean up.
	free(foo_export);
	free(foz_export);
	free(bar_export);
	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh3);
	assert(meshlink_destroy("def_blacklist_conf.1"));
	assert(meshlink_destroy("def_blacklist_conf.2"));
	assert(meshlink_destroy("def_blacklist_conf.3"));

	return true;
}

/* Execute meshlink_default_blacklist Test Case # 2*/
static void test_case_mesh_default_blacklist_02(void **state) {
	execute_test(test_steps_mesh_default_blacklist_02, state);
}

/* Test Steps for meshlink_default_blacklist Test Case # 2

    Test Steps:
    1. Calling meshlink_default_blacklist with NULL as mesh handle argument.

    Expected Result:
    meshlink_default_blacklist API handles the invalid parameter when called by giving proper error number.
*/
static bool test_steps_mesh_default_blacklist_02(void) {
	// Passing NULL as mesh handle argument to the API
	meshlink_set_default_blacklist(NULL, true);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	return true;
}

int test_meshlink_default_blacklist(void) {
	const struct CMUnitTest blackbox_default_blacklist_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_default_blacklist_01, NULL, NULL,
		                (void *)&test_mesh_default_blacklist_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_default_blacklist_02, NULL, NULL,
		                (void *)&test_mesh_default_blacklist_02_state)
	};

	total_tests += sizeof(blackbox_default_blacklist_tests) / sizeof(blackbox_default_blacklist_tests[0]);

	return cmocka_run_group_tests(blackbox_default_blacklist_tests, NULL, NULL);
}
