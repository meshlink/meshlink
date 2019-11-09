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

#ifdef NDEBUG
#undef NDEBUG
#endif

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

#include "execute_tests.h"
#include "test_cases_autoconnect.h"
#include <pthread.h>
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../../utils.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

static void test_case_autoconnect(void **state);
static bool test_steps_mesh_autoconnect(void);
static meshlink_handle_t *mesh1, *mesh2;

/* State structure for meshlink_blacklist Test Case #1 */
static black_box_state_t test_mesh_autoconnect_state = {
	.test_case_name = "test_case_mesh_autoconnect",
};
struct sync_flag test_autoconnect_m1n1_reachable = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .flag = false};
struct sync_flag test_autoconnect_blacklisted = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .flag = false};
struct sync_flag test_autoconnect_successful = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .flag = false};

/* Execute meshlink_blacklist Test Case # 1*/
void test_case_autoconnect(void **state) {
	execute_test(test_steps_mesh_autoconnect, state);
}

void callback_logger(meshlink_handle_t *mesh, meshlink_log_level_t level,
                     const char *text) {
	(void)level;

	fprintf(stderr, "%s: {%s}\n", mesh->name, text);

	if((check_sync_flag(&test_autoconnect_blacklisted) == true) && (strcmp("m1n2", mesh->name) == 0) && (strcmp("* could not find node for initial connect", text) == 0)) {
		fprintf(stderr, "Test case successful\n");
		set_sync_flag(&test_autoconnect_successful, true);
	} else if((check_sync_flag(&test_autoconnect_blacklisted) == true) && (strcmp("m1n2", mesh->name) == 0)) {
		assert(strcmp(text, "Autoconnect trying to connect to m1n1") != 0);
	}

}

static void receive(meshlink_handle_t *mesh, meshlink_node_t *src, const void *data, size_t len) {
	(void)mesh;
	(void)src;
	(void)data;
	assert(len);
}

static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;
	fprintf(stderr, "Status of node {%s} is %d\n", node->name, reachable);

	if(!strcmp(node->name, "m1n1") && reachable) {
		set_sync_flag(&test_autoconnect_m1n1_reachable, true);
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
bool test_steps_mesh_autoconnect(void) {
	char *invite = NULL;
	meshlink_node_t *node = NULL;

	assert(meshlink_destroy("m1n1"));
	assert(meshlink_destroy("m1n2"));

	// Open two new meshlink instance.
	mesh1 = meshlink_open("m1n1", "m1n1", "autoconnect", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, callback_logger);

	mesh2 = meshlink_open("m1n2", "m1n2", "autoconnect", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
	meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, callback_logger);
	meshlink_set_receive_cb(mesh1, receive);

	// Start both instances
	meshlink_set_node_status_cb(mesh1, status_cb);
	assert(meshlink_start(mesh1));

	invite = meshlink_invite(mesh1, NULL, "m1n2");
	assert(invite);

	assert(meshlink_join(mesh2, invite));

	meshlink_set_node_status_cb(mesh2, status_cb);
	assert(meshlink_start(mesh2));

	assert(wait_sync_flag(&test_autoconnect_m1n1_reachable, 30));

	node = meshlink_get_node(mesh2, "m1n1");
	assert(meshlink_blacklist(mesh2, node));
	set_sync_flag(&test_autoconnect_blacklisted, true);

	assert(wait_sync_flag(&test_autoconnect_successful, 60));

	// Clean up.
	meshlink_close(mesh1);
	fprintf(stderr, "Meshlink node1 closed\n");
	meshlink_close(mesh2);
	fprintf(stderr, "Meshlink node2 closed\n");

	assert(meshlink_destroy("m1n1"));
	assert(meshlink_destroy("m1n2"));
	fprintf(stderr, "Meshlink nodes destroyed\n");

	return true;
}

int test_meshlink_autoconnect(void) {
	const struct CMUnitTest blackbox_blacklist_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_autoconnect, NULL, NULL,
		                (void *)&test_mesh_autoconnect_state)
	};

	total_tests += sizeof(blackbox_blacklist_tests) / sizeof(blackbox_blacklist_tests[0]);

	return cmocka_run_group_tests(blackbox_blacklist_tests, NULL, NULL);
}

