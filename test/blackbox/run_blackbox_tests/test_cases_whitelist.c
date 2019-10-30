/*
    test_cases_whitelist.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_whitelist.h"
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
#include <errno.h>

#include "../../utils.h"

static void test_case_mesh_whitelist_01(void **state);
static bool test_steps_mesh_whitelist_01(void);
static void test_case_mesh_whitelist_02(void **state);
static bool test_steps_mesh_whitelist_02(void);
static void test_case_mesh_whitelist_03(void **state);
static bool test_steps_mesh_whitelist_03(void);

/* State structure for meshlink_whitelist Test Case #1 */
static black_box_state_t test_mesh_whitelist_01_state = {
	.test_case_name = "test_case_mesh_whitelist_01",
};

/* State structure for meshlink_whitelist Test Case #2 */
static black_box_state_t test_mesh_whitelist_02_state = {
	.test_case_name = "test_case_mesh_whitelist_02",
};

/* State structure for meshlink_whitelist Test Case #3 */
static black_box_state_t test_mesh_whitelist_03_state = {
	.test_case_name = "test_case_mesh_whitelist_03",
};

static bool rec_stat;
static bool reachable;
static pthread_mutex_t lock_receive = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t receive_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t reachable_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reachable_cond = PTHREAD_COND_INITIALIZER;


/* Execute meshlink_whitelist Test Case # 1*/
static void test_case_mesh_whitelist_01(void **state) {
	execute_test(test_steps_mesh_whitelist_01, state);
}


static void receive(meshlink_handle_t *mesh, meshlink_node_t *src, const void *data, size_t len) {
	(void)mesh;
	(void)src;
	(void)data;

	assert(len);

	pthread_mutex_lock(& lock_receive);
	rec_stat = true;
	assert(!pthread_cond_broadcast(&receive_cond));
	pthread_mutex_unlock(& lock_receive);

}

static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reach) {
	(void)mesh;

	if(!strcmp(node->name, "bar")) {
		pthread_mutex_lock(&reachable_lock);
		reachable = reach;
		assert(!pthread_cond_broadcast(&reachable_cond));
		pthread_mutex_unlock(&reachable_lock);
	}
}


/* Test Steps for meshlink_whitelist Test Case # 1

    Test Steps:
    1. Run 2 node instances
    2. Blacklist one node and again whitelist the blacklisted node

    Expected Result:
    meshlink_whitelist API whitelists the blacklisted node
*/
static bool test_steps_mesh_whitelist_01(void) {
	struct timespec timeout = {0};

	// Open two new meshlink instance.

	meshlink_destroy("whitelist_conf.1");
	meshlink_destroy("whitelist_conf.2");
	meshlink_handle_t *mesh1 = meshlink_open("whitelist_conf.1", "foo", "test", DEV_CLASS_BACKBONE);
	assert(mesh1);
	meshlink_set_log_cb(mesh1, MESHLINK_DEBUG, meshlink_callback_logger);
	meshlink_handle_t *mesh2 = meshlink_open("whitelist_conf.2", "bar", "test", DEV_CLASS_BACKBONE);
	assert(mesh2);
	meshlink_set_log_cb(mesh2, MESHLINK_DEBUG, meshlink_callback_logger);
	meshlink_set_receive_cb(mesh2, receive);
	meshlink_set_receive_cb(mesh1, receive);

	// Export & Import to join the mesh

	reachable = false;
	char *data = meshlink_export(mesh1);
	assert(data);
	assert(meshlink_import(mesh2, data));
	free(data);
	data = meshlink_export(mesh2);
	assert(data);
	assert(meshlink_import(mesh1, data));
	free(data);

	// Start both instances

	meshlink_set_node_status_cb(mesh1, status_cb);
	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));

	// Nodes should know each other
	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&reachable_lock);

	while(reachable == false) {
		assert(!pthread_cond_timedwait(&reachable_cond, &reachable_lock, &timeout));
	}

	pthread_mutex_unlock(&reachable_lock);
	sleep(1);

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	assert(bar);
	meshlink_node_t *foo = meshlink_get_node(mesh2, "foo");
	assert(foo);

	rec_stat = false;
	assert(meshlink_send(mesh1, bar, "test", 5));
	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(& lock_receive);

	if(rec_stat == false) {
		assert(pthread_cond_timedwait(&receive_cond, &lock_receive, &timeout) == 0);
	}

	pthread_mutex_unlock(& lock_receive);


	meshlink_blacklist(mesh1, foo);

	rec_stat = false;
	assert(meshlink_send(mesh1, bar, "test", 5));
	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(& lock_receive);

	if(rec_stat == false) {
		int err = pthread_cond_timedwait(&receive_cond, &lock_receive, &timeout);
		assert(err == ETIMEDOUT);
	}

	pthread_mutex_unlock(& lock_receive);
	meshlink_whitelist(mesh1, foo);

	rec_stat = false;
	bool result = meshlink_send(mesh2, foo, "test", 5);
	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(& lock_receive);

	if(rec_stat == false) {
		assert(pthread_cond_timedwait(&receive_cond, &lock_receive, &timeout) == 0);
	}

	pthread_mutex_unlock(& lock_receive);

	// Clean up.

	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("whitelist_conf.1");
	meshlink_destroy("whitelist_conf.2");

	return result;
}

/* Test Steps for meshlink_whitelist Test Case # 2

    Test Steps:
    1. Calling meshlink_whitelist with NULL as mesh handle argument.

    Expected Result:
    meshlink_whitelist API handles the invalid parameter when called by giving proper error number.
*/
static void test_case_mesh_whitelist_02(void **state) {
	execute_test(test_steps_mesh_whitelist_02, state);
}

/* Test Steps for meshlink_whitelist Test Case # 2*/
static bool test_steps_mesh_whitelist_02(void) {
	struct timespec timeout = {0};

	// Open two new meshlink instance.

	meshlink_destroy("whitelist_conf.3");
	meshlink_destroy("whitelist_conf.4");
	meshlink_handle_t *mesh1 = meshlink_open("whitelist_conf.3", "foo", "test", DEV_CLASS_BACKBONE);
	assert(mesh1);
	meshlink_handle_t *mesh2 = meshlink_open("whitelist_conf.4", "bar", "test", DEV_CLASS_BACKBONE);
	assert(mesh2);
	meshlink_set_receive_cb(mesh2, receive);
	meshlink_set_receive_cb(mesh1, receive);

	char *data = meshlink_export(mesh1);
	assert(data);
	assert(meshlink_import(mesh2, data));
	free(data);
	data = meshlink_export(mesh2);
	assert(data);
	assert(meshlink_import(mesh1, data));
	free(data);

	// Start both instances

	reachable = false;
	meshlink_set_node_status_cb(mesh1, status_cb);
	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));

	// Nodes should know each other
	timeout.tv_sec = time(NULL) + 10;
	pthread_mutex_lock(&reachable_lock);

	while(reachable == false) {
		assert(!pthread_cond_timedwait(&reachable_cond, &reachable_lock, &timeout));
	}

	pthread_mutex_unlock(&reachable_lock);

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	assert(bar);
	meshlink_node_t *foo = meshlink_get_node(mesh2, "foo");
	assert(foo);

	assert(meshlink_send(mesh1, bar, "test", 5));

	meshlink_blacklist(mesh1, foo);

	// Passing NULL as mesh handle but with valid node handle 'foo'

	meshlink_whitelist(NULL, foo);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	// Clean up.

	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("whitelist_conf.3");
	meshlink_destroy("whitelist_conf.4");

	return true;
}

/* Execute meshlink_whitelist Test Case # 3*/
static void test_case_mesh_whitelist_03(void **state) {
	execute_test(test_steps_mesh_whitelist_03, state);
}

/* Test Steps for meshlink_whitelist Test Case # 3

    Test Steps:
    1. Calling meshlink_whitelist with NULL as node handle argument.

    Expected Result:
    meshlink_whitelist API handles the invalid parameter when called by giving proper error number.
*/
static bool test_steps_mesh_whitelist_03(void) {
	// Open meshlink instance.

	meshlink_destroy("whitelist_conf");
	meshlink_handle_t *mesh = meshlink_open("whitelist_conf", "foo", "test", DEV_CLASS_BACKBONE);
	assert(mesh);

	// Start instance
	assert(meshlink_start(mesh));

	meshlink_whitelist(mesh, NULL);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	// Clean up.

	meshlink_close(mesh);
	meshlink_destroy("whitelist_conf");
	return true;
}

int test_meshlink_whitelist(void) {
	const struct CMUnitTest blackbox_whitelist_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_whitelist_01, NULL, NULL,
		                (void *)&test_mesh_whitelist_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_whitelist_02, NULL, NULL,
		                (void *)&test_mesh_whitelist_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_whitelist_03, NULL, NULL,
		                (void *)&test_mesh_whitelist_03_state)
	};

	total_tests += sizeof(blackbox_whitelist_tests) / sizeof(blackbox_whitelist_tests[0]);

	return cmocka_run_group_tests(blackbox_whitelist_tests, NULL, NULL);
}
