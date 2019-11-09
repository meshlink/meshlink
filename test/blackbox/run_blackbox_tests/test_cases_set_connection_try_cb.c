/*
    test_cases_set_connection_try_cb.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2019  Guus Sliepen <guus@meshlink.io>

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
#include "test_cases_set_connection_try_cb.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../../utils.h"
#include <assert.h>
#include <setjmp.h>
#include <cmocka.h>
#include <pthread.h>

static void test_case_set_connection_try_cb_01(void **state);
static bool test_set_connection_try_cb_01(void);

static bool bar_reachable;
static int connection_attempts;
static struct sync_flag status_changed_cond = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static struct sync_flag connection_attempt_cond = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};

static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *source, bool reachable) {
	if(!strcmp(mesh->name, "foo") && !strcmp(source->name, "bar")) {
		bar_reachable = reachable;
		set_sync_flag(&status_changed_cond, true);
	}
}

/* Meta-connection try callback handler */
static void connection_try_cb(meshlink_handle_t *mesh, meshlink_node_t *source) {
	(void)source;

	if(!strcmp(mesh->name, "foo")) {
		++connection_attempts;

		if(connection_attempts > 3) {
			set_sync_flag(&connection_attempt_cond, true);
		}
	}
}

/* Execute set meta connection try callback Test Case # 1 */
static void test_case_set_connection_try_cb_01(void **state) {
	execute_test(test_set_connection_try_cb_01, state);
}

/* Test Steps for meshlink_set_connection_try_cb Test Case # 1

    Test Steps:
    1. Run foo and bar nodes after exporting and importing node's keys and addresses mutually.
    2. Close bar node. Wait for connection attempts and cleanup.

    Expected Result:
    Connection try callback should be invoked initially when foo and bar forms a meta-connection.
    After closing bar node it should invoke 3 connection try callbacks in span of about 30 seconds.
*/
static bool test_set_connection_try_cb_01(void) {
	assert(meshlink_destroy("meshlink_conf.1"));
	assert(meshlink_destroy("meshlink_conf.2"));

	// Opening foo and bar nodes
	meshlink_handle_t *mesh1 = meshlink_open("meshlink_conf.1", "foo", "test", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	meshlink_set_log_cb(mesh1, MESHLINK_DEBUG, meshlink_callback_logger);
	meshlink_enable_discovery(mesh1, false);
	meshlink_handle_t *mesh2 = meshlink_open("meshlink_conf.2", "bar", "test", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);

	// Set up callback for node status
	meshlink_set_node_status_cb(mesh1, node_status_cb);
	meshlink_set_connection_try_cb(mesh1, connection_try_cb);

	// Exporting and Importing mutually
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);
	assert(meshlink_import(mesh1, exp2));
	assert(meshlink_import(mesh2, exp1));
	free(exp1);
	free(exp2);

	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));

	// Wait for foo and bar nodes to join
	assert(wait_sync_flag(&status_changed_cond, 5));
	assert(bar_reachable);

	// Joining should in this case raise one connection try callback
	assert_int_equal(connection_attempts, 1);

	// Close the bar node
	set_sync_flag(&status_changed_cond, false);
	meshlink_close(mesh2);
	assert(wait_sync_flag(&status_changed_cond, 5));
	assert(!bar_reachable);

	// Wait for additional 3 connection try callbacks
	time_t attempt_time_start = time(NULL);
	assert(attempt_time_start != -1);
	assert_int_equal(wait_sync_flag(&connection_attempt_cond, 60), true);

	// Close bar node and assert on number of callbacks invoked and the time taken.
	meshlink_close(mesh1);
	time_t attempt_time_stop = time(NULL);
	assert(attempt_time_stop != -1);
	assert_int_equal(connection_attempts, 4);
	assert_in_range(attempt_time_stop - attempt_time_start, 25, 45);

	// Cleanup
	assert(meshlink_destroy("meshlink_conf.1"));
	assert(meshlink_destroy("meshlink_conf.2"));

	return true;
}

int test_cases_connection_try(void) {
	black_box_state_t test_case_set_connection_try_cb_01_state = {
		.test_case_name = "test_case_set_connection_try_cb_01",
	};

	const struct CMUnitTest blackbox_connection_try_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_set_connection_try_cb_01, NULL, NULL,
		                (void *)&test_case_set_connection_try_cb_01_state),
	};
	total_tests += sizeof(blackbox_connection_try_tests) / sizeof(blackbox_connection_try_tests[0]);

	int failed = cmocka_run_group_tests(blackbox_connection_try_tests, NULL, NULL);

	return failed;
}
