/*
    test_cases_get_node_reachability.c -- Execution of specific meshlink black box test cases
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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include "execute_tests.h"
#include "test_cases_get_node_reachability.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../../utils.h"

#define NUT                         "nut"
#define PEER                        "peer"
#define PEER2                       "peer2"
#define GET_NODE_REACHABILITY       "test_get_node_reachability"
#define create_path(confbase, node_name, test_case_no)   assert(snprintf(confbase, sizeof(confbase), GET_NODE_REACHABILITY "_%ld_%s_%02d", (long) getpid(), node_name, test_case_no) > 0)

static struct sync_flag peer_reachable_status_cond = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static bool peer_reachable_status;
static struct sync_flag nut_reachable_status_cond = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static bool nut_reachable_status;
static struct sync_flag nut_started_status_cond = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static bool peer_node_callback_test_status;

static void test_case_get_node_reachability_01(void **state);
static bool test_get_node_reachability_01(void);
static void test_case_get_node_reachability_02(void **state);
static bool test_get_node_reachability_02(void);
static void test_case_get_node_reachability_03(void **state);
static bool test_get_node_reachability_03(void);
static void test_case_get_node_reachability_04(void **state);
static bool test_get_node_reachability_04(void);
static void test_case_get_node_reachability_05(void **state);
static bool test_get_node_reachability_05(void);
static void test_case_get_node_reachability_06(void **state);
static bool test_get_node_reachability_06(void);
static void test_case_get_node_reachability_07(void **state);
static bool test_get_node_reachability_07(void);

/* Node reachable status callback which signals the respective conditional varibale */
static void meshlink_node_reachable_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable_status) {
	if(meshlink_get_self(mesh) == node) {
		return;
	}

	if(!strcasecmp(mesh->name, NUT)) {
		if(!strcasecmp(node->name, PEER)) {
			peer_reachable_status = reachable_status;
			set_sync_flag(&peer_reachable_status_cond, true);
		}
	} else if(!strcasecmp(mesh->name, PEER)) {
		if(!strcasecmp(node->name, NUT)) {
			nut_reachable_status = reachable_status;
			set_sync_flag(&nut_reachable_status_cond, true);
		}
	}

	// Reset the node reachability status callback, as the two nodes making a simultaneous connection to each other, and then one connection will win and cause the other one to be disconnected.
	meshlink_set_node_status_cb(mesh, NULL);
}

static void meshlink_node_reachable_status_cb_2(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable_status) {
	meshlink_node_t *peer_handle;
	char *peer_name = NULL;
	time_t last_unreachable, last_reachable;
	static int count = 2;

	if(meshlink_get_self(mesh) == node) {
		return;
	}

	/*  Of the 2 node reachable callbacks, the latest callback calls meshlink_get_node_reachability API
	    for the 1st node joined  */
	if(count && reachable_status && !strcasecmp(mesh->name, NUT)) {
		--count;

		if(!count) {
			if(!strcasecmp(node->name, PEER)) {
				peer_name = PEER2;
			} else if(!strcasecmp(node->name, PEER2)) {
				peer_name = PEER;
			}

			peer_handle = meshlink_get_node(mesh, peer_name);
			assert_non_null(peer_handle);

			bool status = meshlink_get_node_reachability(mesh, peer_handle, &last_reachable, &last_unreachable);

			peer_node_callback_test_status = status && last_reachable && !last_unreachable;
			set_sync_flag(&peer_reachable_status_cond, true);
		}
	}
}

/* SIGUSR2 signal handler that signals the NUT started and PEER node can join */
void nut_started_user_signal_handler(int signum) {
	if(signum == SIGUSR2) {
		set_sync_flag(&nut_started_status_cond, true);
	}

}

/*
    Execute meshlink get last node reachability times feature Test Case # 1 -
    Sanity API test
*/
static void test_case_get_node_reachability_01(void **state) {
	execute_test(test_get_node_reachability_01, state);
}

/* Test Steps for meshlink_get_node_reachability Test Case # 1

    Test steps and scenarios:
    1.  Open Node-Under-Test (NUT) instance, Call meshlink_get_node_reachability API
        with valid mesh handle, self node handle,  last_reachable pointer and
        last_unreachable pointer.
        Expected Result:
        API returns self node unreachable, last_reachable and last_unreachable values
        as 0 seconds

    2.  Call meshlink_get_node_reachability API with valid mesh handle, self node handle.
        But pass NULL pointers for last_reachable and last_unreachable arguments
        Expected Result:
        API returns self node unreachable

    3.  Call meshlink_get_node_reachability API with NULL as mesh handle,
        valid self node handle, last_reachable pointer and last_unreachable pointer.
        Expected Result:
        API fails and sets MESHLINK_EINVAL as meshlink errno value

    4.  Call meshlink_get_node_reachability API with NULL as mesh handle,
        valid self node handle, NULL pointers for last_reachable and last_unreachable
        arguments
        Expected Result:
        API fails and sets MESHLINK_EINVAL as meshlink errno value

    5.  Call meshlink_get_node_reachability API with valid mesh handle,
        NULL as self node handle, last_reachable pointer and last_unreachable pointer.
        Expected Result:
        API fails and sets MESHLINK_EINVAL as meshlink errno value

    6.  Call meshlink_get_node_reachability API with valid mesh handle,
        NULL as self node handle, NULL pointers for last_reachable and last_unreachable
        arguments
        Expected Result:
        API fails and sets MESHLINK_EINVAL as meshlink errno value

*/
static bool test_get_node_reachability_01(void) {
	bool status;
	time_t last_unreachable, last_reachable;
	char nut_confbase[PATH_MAX];
	create_path(nut_confbase, NUT, 1);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open Node-Under-Test node instance

	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY, DEV_CLASS_STATIONARY);
	assert_int_not_equal(mesh, NULL);

	// Call meshlink_get_node_reachability API with all valid arguments

	status = meshlink_get_node_reachability(mesh, meshlink_get_self(mesh), &last_reachable, &last_unreachable);
	assert_int_equal(status, false);
	assert_int_equal(last_reachable, 0);
	assert_int_equal(last_unreachable, 0);

	// Call meshlink_get_node_reachability API with all valid arguments

	status = meshlink_get_node_reachability(mesh, meshlink_get_self(mesh), NULL, NULL);
	assert_int_equal(status, false);

	// Call meshlink_get_node_reachability API with invalid parameters

	meshlink_errno = MESHLINK_OK;
	meshlink_get_node_reachability(NULL, meshlink_get_self(mesh), NULL, NULL);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);
	meshlink_errno = MESHLINK_OK;
	meshlink_get_node_reachability(NULL, meshlink_get_self(mesh), &last_reachable, &last_unreachable);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);
	meshlink_errno = MESHLINK_OK;
	meshlink_get_node_reachability(mesh, NULL, NULL, NULL);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);
	meshlink_errno = MESHLINK_OK;
	meshlink_get_node_reachability(mesh, NULL, &last_reachable, &last_unreachable);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	// Cleanup

	meshlink_close(mesh);
	assert_true(meshlink_destroy(nut_confbase));
	return true;
}

/*
    Execute meshlink get last node reachability times feature Test Case # 2 -
    API testing with stand-alone node
*/
static void test_case_get_node_reachability_02(void **state) {
	execute_test(test_get_node_reachability_02, state);
}

/* Test Steps for meshlink_get_node_reachability Test Case # 2

    Test steps and scenarios:
    1.  Open and start Node-Under-Test (NUT) instance, Call meshlink_get_node_reachability API.
        Expected Result:
        API returns self node reachable status, last_reachable as some positive non-zero integer
        and last_unreachable value as 0 seconds

    2.  Stop the NUT instance, Call meshlink_get_node_reachability API.
        Expected Result:
        API returns self node unreachable, both last_reachable and last_unreachable values
        as some positive non-zero time in seconds

    3.  Close and reopen NUT instance, Call meshlink_get_node_reachability API.
        Expected Result:
        API returns self node unreachable, both last_reachable and last_unreachable values
        as some positive non-zero time in seconds

*/
static bool test_get_node_reachability_02(void) {
	bool status;
	time_t last_unreachable, last_reachable, last_peer_unreachable, last_peer_reachable;
	char nut_confbase[PATH_MAX];
	create_path(nut_confbase, NUT, 2);
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open and start Node-Under-Test node instance

	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);
	assert_true(meshlink_start(mesh));

	// Call meshlink_get_node_reachability API with all valid arguments

	status = meshlink_get_node_reachability(mesh, meshlink_get_self(mesh), &last_reachable, &last_unreachable);
	assert_true(status);
	assert_int_not_equal(last_reachable, 0);
	assert_int_equal(last_unreachable, 0);
	last_peer_reachable = last_reachable;

	// Stop NUT node instance

	meshlink_stop(mesh);

	// Call meshlink_get_node_reachability API with all valid arguments

	status = meshlink_get_node_reachability(mesh, meshlink_get_self(mesh), &last_reachable, &last_unreachable);
	assert_false(status);
	assert_int_not_equal(last_unreachable, 0);
	assert_int_equal(last_reachable, last_peer_reachable);
	last_peer_unreachable = last_unreachable;

	// Reinitialize NUT node instance

	meshlink_close(mesh);
	mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);

	// Call meshlink_get_node_reachability API with all valid arguments

	status = meshlink_get_node_reachability(mesh, meshlink_get_self(mesh), &last_reachable, &last_unreachable);
	assert_false(status);
	assert_int_equal(last_reachable, last_peer_reachable);
	assert_int_equal(last_unreachable, last_peer_unreachable);

	// Cleanup

	meshlink_close(mesh);
	assert_true(meshlink_destroy(nut_confbase));
	return true;
}

/*
    Execute meshlink get last node reachability times feature Test Case # 3 -
    API testing with host node which already joined with a peer node which later
    goes offline, test host node with an offline peer node case.
*/
static void test_case_get_node_reachability_03(void **state) {
	execute_test(test_get_node_reachability_03, state);
}

/* Test Steps for meshlink_get_node_reachability Test Case # 3

    Test steps and scenarios:
    1.  Open Node-Under-Test (NUT) and peer node instance, start peer node instance
        and invite NUT. NUT joins peer and destroy peer node instance.
        Call meshlink_get_node_reachability API.
        Expected Result:
        API returns peer node unreachable status, last_reachable and last_unreachable
        value as 0 seconds.

    2.  Start the NUT instance, Call meshlink_get_node_reachability API.
        Expected Result:
        API returns peer node unreachable status, last_reachable and last_unreachable
        value as 0 seconds.

    3.  Stop the NUT instance, Call meshlink_get_node_reachability API.
        Expected Result:
        API returns peer node unreachable status, last_reachable and last_unreachable
        value as 0 seconds.

    4.  Close and reopen NUT instance, Call meshlink_get_node_reachability API.
        Expected Result:
        API returns peer node unreachable status, last_reachable and last_unreachable
        value as 0 seconds.

*/
static bool test_get_node_reachability_03(void) {
	bool status;
	time_t last_unreachable, last_reachable;
	char nut_confbase[PATH_MAX];
	char peer_confbase[PATH_MAX];
	create_path(nut_confbase, NUT, 3);
	create_path(peer_confbase, PEER, 3);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open and start peer node instance, invite NUT.

	meshlink_handle_t *mesh_peer = meshlink_open(peer_confbase, PEER, GET_NODE_REACHABILITY,
	                               DEV_CLASS_STATIONARY);
	assert_non_null(mesh_peer);
	assert_true(meshlink_start(mesh_peer));
	char *invitation = meshlink_invite(mesh_peer, NULL, NUT);
	assert_non_null(invitation);

	// Open NUT node instance and join with the peer node

	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);
	assert_true(meshlink_join(mesh, invitation));
	free(invitation);
	meshlink_node_t *peer_handle = meshlink_get_node(mesh, PEER);
	assert_non_null(peer_handle);

	// Cleanup peer node instance

	meshlink_close(mesh_peer);
	assert_true(meshlink_destroy(peer_confbase));

	// Call meshlink_get_node_reachability API with valid arguments

	status = meshlink_get_node_reachability(mesh, peer_handle, &last_reachable, &last_unreachable);
	assert_false(status);
	assert_int_equal(last_reachable, 0);
	assert_int_equal(last_unreachable, 0);

	// Start NUT node instance

	assert_true(meshlink_start(mesh));

	// Call meshlink_get_node_reachability API with valid arguments

	status = meshlink_get_node_reachability(mesh, peer_handle, &last_reachable, &last_unreachable);
	assert_false(status);
	assert_int_equal(last_reachable, 0);
	assert_int_equal(last_unreachable, 0);

	// Stop NUT node instance

	meshlink_stop(mesh);

	// Call meshlink_get_node_reachability API with valid arguments

	status = meshlink_get_node_reachability(mesh, peer_handle, &last_reachable, &last_unreachable);
	assert_false(status);
	assert_int_equal(last_reachable, 0);
	assert_int_equal(last_unreachable, 0);

	// Reinitialize NUT node instance

	meshlink_close(mesh);
	mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);
	peer_handle = meshlink_get_node(mesh, PEER);
	assert_non_null(peer_handle);

	// Call meshlink_get_node_reachability API with valid arguments

	status = meshlink_get_node_reachability(mesh, peer_handle, &last_reachable, &last_unreachable);
	assert_false(status);
	assert_int_equal(last_reachable, 0);
	assert_int_equal(last_unreachable, 0);

	// Cleanup NUT

	meshlink_close(mesh);
	assert_true(meshlink_destroy(nut_confbase));
	return true;
}

/*
    Execute meshlink get last node reachability times feature Test Case # 4 -
    API testing around invited and invitee node.
*/
static void test_case_get_node_reachability_04(void **state) {
	execute_test(test_get_node_reachability_04, state);
}

/* Test Steps for meshlink_get_node_reachability Test Case # 4

    Test steps and scenarios:
    1.  Open Node-Under-Test (NUT) and peer node instance, join both the node and
        bring them online. Call meshlink_get_node_reachability API from both the nodes.
        Expected Result:
        API for both the nodes returns reachable status, last_reachable should be
         some non-zero positive seconds and last_unreachable should be 0 seconds.

    2.  Stop both the node instances, Call meshlink_get_node_reachability API from both the nodes.
        Expected Result:
        API for both the nodes returns unreachable status. last_reachable should match with
        the old value and last_unreachable should be non-zero positive value.

    3.  Restart both the node instances, Call meshlink_get_node_reachability APIs.
        Expected Result:
        API for both the nodes should return reachable status. last_reachable should not match with
        the old value, but last_unreachable should remain same

    4.  Close and reopen both the node instances, Call meshlink_get_node_reachability APIs.
        Expected Result:
        API returns self node unreachable status, last_reachable should remain same
        but last_unreachable should vary.

    4.  Start both the node instances, Call meshlink_get_node_reachability APIs.
        Expected Result:
        API returns self node reachable status, last_reachable should vary and
        last_unreachable remains same.

*/
static bool test_get_node_reachability_04(void) {
	bool status;
	time_t last_nut_unreachable, last_nut_reachable;
	time_t last_peer_unreachable, last_peer_reachable;
	time_t last_reachable, last_unreachable;
	char nut_confbase[PATH_MAX];
	char peer_confbase[PATH_MAX];
	create_path(nut_confbase, NUT, 4);
	create_path(peer_confbase, PEER, 4);
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open both NUT and peer node instance, invite and join NUT with peer node.

	meshlink_handle_t *mesh_peer = meshlink_open(peer_confbase, PEER, GET_NODE_REACHABILITY,
	                               DEV_CLASS_STATIONARY);
	assert_non_null(mesh_peer);
	meshlink_set_node_status_cb(mesh_peer, meshlink_node_reachable_status_cb);
	char *invitation = meshlink_invite(mesh_peer, NULL, NUT);
	assert_non_null(invitation);
	assert_true(meshlink_start(mesh_peer));

	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY,
	                                        DEV_CLASS_STATIONARY);
	assert_non_null(mesh);
	meshlink_set_node_status_cb(mesh, meshlink_node_reachable_status_cb);
	assert_true(meshlink_join(mesh, invitation));
	free(invitation);

	meshlink_node_t *peer_handle = meshlink_get_node(mesh, PEER);
	assert_non_null(peer_handle);
	meshlink_node_t *nut_handle = meshlink_get_node(mesh_peer, NUT);
	assert_non_null(nut_handle);

	// Bring nodes online.

	set_sync_flag(&peer_reachable_status_cond, false);
	set_sync_flag(&nut_reachable_status_cond, false);
	assert_true(meshlink_start(mesh));
	assert_true(wait_sync_flag(&peer_reachable_status_cond, 60));
	assert_true(peer_reachable_status);
	assert_true(wait_sync_flag(&nut_reachable_status_cond, 60));
	assert_true(nut_reachable_status);

	// Call meshlink_get_node_reachability API from joined node and also from joining node.

	status = meshlink_get_node_reachability(mesh, peer_handle, &last_reachable, &last_unreachable);
	assert_true(status);
	assert_int_not_equal(last_reachable, 0);
	assert_int_equal(last_unreachable, 0);
	last_peer_reachable = last_reachable;

	status = meshlink_get_node_reachability(mesh_peer, nut_handle, &last_reachable, &last_unreachable);
	assert_true(status);
	assert_int_not_equal(last_reachable, 0);
	assert_int_equal(last_unreachable, 0);
	last_nut_reachable = last_reachable;

	// Stop the node instances of both peer and NUT.

	meshlink_stop(mesh);
	meshlink_stop(mesh_peer);

	// Call meshlink_get_node_reachability API from joined node and also from joining node.

	status = meshlink_get_node_reachability(mesh, peer_handle, &last_reachable, &last_unreachable);
	assert_false(status);
	assert_int_not_equal(last_unreachable, 0);
	assert_int_equal(last_reachable, last_peer_reachable);
	last_peer_unreachable = last_unreachable;

	status = meshlink_get_node_reachability(mesh_peer, nut_handle, &last_reachable, &last_unreachable);
	assert_false(status);
	assert_int_not_equal(last_unreachable, 0);
	assert_int_equal(last_reachable, last_nut_reachable);
	last_nut_unreachable = last_unreachable;

	// Restart the node instances of both peer and NUT and wait for nodes to come online

	sleep(2);
	set_sync_flag(&peer_reachable_status_cond, false);
	set_sync_flag(&nut_reachable_status_cond, false);
	meshlink_set_node_status_cb(mesh, meshlink_node_reachable_status_cb);
	meshlink_set_node_status_cb(mesh_peer, meshlink_node_reachable_status_cb);
	assert_true(meshlink_start(mesh));
	assert_true(meshlink_start(mesh_peer));

	assert_true(wait_sync_flag(&peer_reachable_status_cond, 60));
	assert_true(peer_reachable_status);
	assert_true(wait_sync_flag(&nut_reachable_status_cond, 60));
	assert_true(nut_reachable_status);

	// Call meshlink_get_node_reachability API from joined node and also from joining node.

	status = meshlink_get_node_reachability(mesh, peer_handle, &last_reachable, &last_unreachable);
	assert_true(status);
	assert_int_not_equal(last_reachable, last_peer_reachable);
	assert_true(last_unreachable >= last_peer_unreachable);
	last_peer_reachable = last_reachable;

	status = meshlink_get_node_reachability(mesh_peer, nut_handle, &last_reachable, &last_unreachable);
	assert_true(status);
	assert_int_not_equal(last_reachable, last_nut_reachable);
	assert_true(last_unreachable >= last_nut_unreachable);
	last_nut_reachable = last_reachable;

	// Reinitialize the node instances of both peer and NUT

	meshlink_close(mesh);
	meshlink_close(mesh_peer);

	sleep(2);

	mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);
	meshlink_set_node_status_cb(mesh, meshlink_node_reachable_status_cb);
	mesh_peer = meshlink_open(peer_confbase, PEER, GET_NODE_REACHABILITY,
	                          DEV_CLASS_STATIONARY);
	assert_non_null(mesh_peer);
	meshlink_set_node_status_cb(mesh_peer, meshlink_node_reachable_status_cb);

	peer_handle = meshlink_get_node(mesh, PEER);
	assert_non_null(peer_handle);
	nut_handle = meshlink_get_node(mesh_peer, NUT);
	assert_non_null(nut_handle);

	// Call meshlink_get_node_reachability API from joined node and also from joining node.

	status = meshlink_get_node_reachability(mesh, peer_handle, &last_reachable, &last_unreachable);
	assert_false(status);
	assert_int_equal(last_reachable, last_peer_reachable);
	assert_int_not_equal(last_unreachable, last_peer_unreachable);
	last_peer_unreachable = last_unreachable;

	status = meshlink_get_node_reachability(mesh_peer, nut_handle, &last_reachable, &last_unreachable);
	assert_false(status);
	assert_int_equal(last_reachable, last_nut_reachable);
	assert_int_not_equal(last_unreachable, last_nut_unreachable);
	last_nut_unreachable = last_unreachable;

	// Restart the node instances of both peer and NUT

	set_sync_flag(&peer_reachable_status_cond, false);
	set_sync_flag(&nut_reachable_status_cond, false);

	assert_true(meshlink_start(mesh));
	assert_true(meshlink_start(mesh_peer));

	assert_true(wait_sync_flag(&peer_reachable_status_cond, 60));
	assert_true(peer_reachable_status);
	assert_true(wait_sync_flag(&nut_reachable_status_cond, 60));
	assert_true(nut_reachable_status);

	// Call meshlink_get_node_reachability API from joined node and also from joining node.

	status = meshlink_get_node_reachability(mesh, peer_handle, &last_reachable, &last_unreachable);
	assert_true(status);
	assert_int_not_equal(last_reachable, last_peer_reachable);
	assert_true(last_unreachable >= last_peer_unreachable);

	status = meshlink_get_node_reachability(mesh_peer, nut_handle, &last_reachable, &last_unreachable);
	assert_true(status);
	assert_int_not_equal(last_reachable, last_nut_reachable);
	assert_true(last_unreachable >= last_nut_unreachable);

	// Cleanup

	meshlink_close(mesh);
	meshlink_close(mesh_peer);
	assert_true(meshlink_destroy(nut_confbase));
	assert_true(meshlink_destroy(peer_confbase));
	return true;
}

/*
    Execute meshlink get last node reachability times feature Test Case # 5 -
    API testing by calling it in the meshlink callback(s) and also isolation property.
*/
static void test_case_get_node_reachability_05(void **state) {
	execute_test(test_get_node_reachability_05, state);
}

/* Test Steps for meshlink_get_node_reachability Test Case # 5

    Test steps and scenarios:
    1.  Open Node-Under-Test (NUT), peer and peer2 node instances. Join both the peer nodes
        with NUT and bring them online.
        Expected Result:
        API called from the node reachable callback of the latest peer node from NUT
        about other peer node which joined 1st should return reachable status,
        last_reachable status as some positive non-zero value and last unreachable value as 0.

*/
static bool test_get_node_reachability_05(void) {
	char *invitation;
	bool status;
	time_t last_reachable, last_unreachable;
	char nut_confbase[PATH_MAX];
	char peer_confbase[PATH_MAX];
	char peer2_confbase[PATH_MAX];
	create_path(nut_confbase, NUT, 5);
	create_path(peer_confbase, PEER, 5);
	create_path(peer2_confbase, PEER2, 5);
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open NUT, peer and peer2 and join peer nodes with NUT.

	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY,
	                                        DEV_CLASS_STATIONARY);
	assert_non_null(mesh);
	meshlink_set_node_status_cb(mesh, meshlink_node_reachable_status_cb_2);
	meshlink_handle_t *mesh_peer = meshlink_open(peer_confbase, PEER, GET_NODE_REACHABILITY,
	                               DEV_CLASS_STATIONARY);
	assert_non_null(mesh_peer);
	meshlink_handle_t *mesh_peer2 = meshlink_open(peer2_confbase, PEER2, GET_NODE_REACHABILITY,
	                                DEV_CLASS_STATIONARY);
	assert_non_null(mesh_peer2);

	assert_true(meshlink_start(mesh));

	invitation = meshlink_invite(mesh, NULL, PEER);
	assert_non_null(invitation);
	assert_true(meshlink_join(mesh_peer, invitation));
	invitation = meshlink_invite(mesh, NULL, PEER2);
	assert_non_null(invitation);
	assert_true(meshlink_join(mesh_peer2, invitation));

	// Call meshlink_get_node_reachability API from NUT and check they remained 0 and unreachable

	status = meshlink_get_node_reachability(mesh, meshlink_get_node(mesh, PEER), &last_reachable, &last_unreachable);
	assert_int_equal(status, false);
	assert_int_equal(last_reachable, 0);
	assert_int_equal(last_unreachable, 0);
	status = meshlink_get_node_reachability(mesh, meshlink_get_node(mesh, PEER2), &last_reachable, &last_unreachable);
	assert_int_equal(status, false);
	assert_int_equal(last_reachable, 0);
	assert_int_equal(last_unreachable, 0);

	// Start and wait for the signal from the node reachable callback which is raised when
	// NUT is able to call meshlink_get_node_reachability API from callback of other peer node.

	set_sync_flag(&peer_reachable_status_cond, false);
	assert_true(meshlink_start(mesh_peer));
	assert_true(meshlink_start(mesh_peer2));
	assert_true(wait_sync_flag(&peer_reachable_status_cond, 60));
	assert_true(peer_node_callback_test_status);

	// Cleanup

	meshlink_close(mesh);
	meshlink_close(mesh_peer);
	meshlink_close(mesh_peer2);
	assert_true(meshlink_destroy(nut_confbase));
	assert_true(meshlink_destroy(peer_confbase));
	assert_true(meshlink_destroy(peer2_confbase));
	return true;
}

/*
    Execute meshlink get last node reachability times feature Test Case # 6 -
    Persistence testing on the joining node.
*/
static void test_case_get_node_reachability_06(void **state) {
	execute_test(test_get_node_reachability_06, state);
}

/* Test Steps for meshlink_get_node_reachability Test Case # 6

    Test steps and scenarios:
    1.  Open Node-Under-Test (NUT) and invite peer node and close it's instance.
        Spawn a process which waits for the peer node to join and raises SIGINT if the
        appropriate callback is received (on the other hand the test suite opens and joins
        the peer node with NUT in the forked process).
        Reopen NUT instance in the test suite process and call meshlink_get_node_reachability.
        Expected Result:
        API returns peer node unreachable, last_reachable and last_unreachable values
        as 0 seconds. It is expected that this feature synchronize it at least for the first time
        when the NUT receives that a new peer node joined.

*/
static bool test_get_node_reachability_06(void) {
	bool status;
	time_t last_reachable, last_unreachable;
	pid_t pid;
	int pid_status;
	char nut_confbase[PATH_MAX];
	char peer_confbase[PATH_MAX];
	create_path(nut_confbase, NUT, 6);
	create_path(peer_confbase, PEER, 6);
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open NUT node instance and invite peer node. Close NUT node instance.

	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);
	char *invitation = meshlink_invite(mesh, NULL, PEER);
	meshlink_close(mesh);

	// Set the SIGUSR2 signal handler with handler that signal the condition to the test suite

	sighandler_t usr2sighandler = signal(SIGUSR2, nut_started_user_signal_handler);
	assert_int_not_equal(usr2sighandler, SIG_ERR);

	// Fork a new process and run NUT in it which just waits for the peer node reachable status callback
	// and terminates the process immediately.

	pid = fork();
	assert_int_not_equal(pid, -1);

	if(!pid) {
		assert(signal(SIGUSR2, SIG_DFL) != SIG_ERR);

		mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY, DEV_CLASS_STATIONARY);
		assert(mesh);
		meshlink_set_log_cb(mesh, MESHLINK_DEBUG, log_cb);
		meshlink_set_node_status_cb(mesh, meshlink_node_reachable_status_cb);

		set_sync_flag(&peer_reachable_status_cond, false);
		assert(meshlink_start(mesh));

		assert(kill(getppid(), SIGUSR2) != -1);

		assert(wait_sync_flag(&peer_reachable_status_cond, 60));
		assert(peer_reachable_status);

		raise(SIGINT);
	}

	// Open peer node instance and join with the invitation obtained.

	meshlink_handle_t *mesh_peer = meshlink_open(peer_confbase, PEER, GET_NODE_REACHABILITY,
	                               DEV_CLASS_STATIONARY);
	assert_non_null(mesh_peer);

	// Wait for the started signal from NUT and reset the previous SIGUSR2 signal handler

	assert_true(wait_sync_flag(&nut_started_status_cond, 60));
	assert_int_not_equal(signal(SIGUSR2, usr2sighandler), SIG_ERR);

	assert_true(meshlink_join(mesh_peer, invitation));
	assert_true(meshlink_start(mesh_peer));

	// Wait for child exit and verify which signal terminated it

	assert_int_not_equal(waitpid(pid, &pid_status, 0), -1);
	assert_int_equal(WIFSIGNALED(pid_status), true);
	assert_int_equal(WTERMSIG(pid_status), SIGINT);

	// Reopen the NUT instance in the same test suite

	mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);

	// Call meshlink_get_node_reachability API and verify that the time stamps has persisted.

	status = meshlink_get_node_reachability(mesh, meshlink_get_node(mesh, PEER), &last_reachable, &last_unreachable);
	assert_int_equal(status, false);
	assert_int_not_equal(last_reachable, 0);
	assert_int_equal(last_unreachable, 0);

	// Cleanup

	meshlink_close(mesh);
	meshlink_close(mesh_peer);
	assert_true(meshlink_destroy(nut_confbase));
	assert_true(meshlink_destroy(peer_confbase));
	return true;
}

/*
    Execute meshlink get last node reachability times feature Test Case # 7 -
    Persistence testing on the invited node.
*/
static void test_case_get_node_reachability_07(void **state) {
	execute_test(test_get_node_reachability_07, state);
}

/* Test Steps for meshlink_get_node_reachability Test Case # 7

    Test steps and scenarios:
    1.  Open peer node instance, invite NUT and start peer node. Spawn a new process in
        which it opens and joins the NUT with peer node.
        Reopen NUT instance in the test suite process and call meshlink_get_node_reachability API.
        Expected Result:
        API returns peer node unreachable, last_reachable and last_unreachable values
        as 0 seconds. It is expected that this feature synchronize it at least for the first time
        when the Node-Under-Test joined with the peer node.

*/
static bool test_get_node_reachability_07(void) {
	bool status;
	time_t last_reachable, last_unreachable;
	pid_t pid;
	int pid_status;
	char nut_confbase[PATH_MAX];
	char peer_confbase[PATH_MAX];
	create_path(nut_confbase, NUT, 7);
	create_path(peer_confbase, PEER, 7);
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open peer node instance and invite NUT.

	meshlink_handle_t *mesh_peer = meshlink_open(peer_confbase, PEER, GET_NODE_REACHABILITY,
	                               DEV_CLASS_STATIONARY);
	assert_int_not_equal(mesh_peer, NULL);
	char *invitation = meshlink_invite(mesh_peer, NULL, NUT);
	assert_non_null(invitation);

	assert_true(meshlink_start(mesh_peer));

	// Fork a new process in which NUT is joins with the peer node and raises SIGINT to terminate.

	pid = fork();
	assert_int_not_equal(pid, -1);

	if(!pid) {
		meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY, DEV_CLASS_STATIONARY);
		assert(mesh);
		meshlink_set_log_cb(mesh, MESHLINK_DEBUG, log_cb);

		assert(meshlink_join(mesh, invitation));

		raise(SIGINT);
	}

	// Wait for child exit and verify which signal terminated it

	assert_int_not_equal(waitpid(pid, &pid_status, 0), -1);
	assert_int_equal(WIFSIGNALED(pid_status), true);
	assert_int_equal(WTERMSIG(pid_status), SIGINT);

	// Reopen the NUT instance in the same test suite

	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, GET_NODE_REACHABILITY, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);

	// Call meshlink_get_node_reachability API and verify that the time stamps has persisted.

	status = meshlink_get_node_reachability(mesh, meshlink_get_node(mesh, PEER), &last_reachable, &last_unreachable);
	assert_int_equal(status, false);
	assert_int_equal(last_reachable, 0);
	assert_int_equal(last_unreachable, 0);

	// Cleanup

	meshlink_close(mesh);
	meshlink_close(mesh_peer);
	assert_true(meshlink_destroy(nut_confbase));
	assert_true(meshlink_destroy(peer_confbase));
	return true;
}

int test_get_node_reachability(void) {
	/* State structures for get node reachability Test Cases */
	black_box_state_t test_case_get_node_reachability_01_state = {
		.test_case_name = "test_case_get_node_reachability_01",
	};
	black_box_state_t test_case_get_node_reachability_02_state = {
		.test_case_name = "test_case_get_node_reachability_02",
	};
	black_box_state_t test_case_get_node_reachability_03_state = {
		.test_case_name = "test_case_get_node_reachability_03",
	};
	black_box_state_t test_case_get_node_reachability_04_state = {
		.test_case_name = "test_case_get_node_reachability_04",
	};
	black_box_state_t test_case_get_node_reachability_05_state = {
		.test_case_name = "test_case_get_node_reachability_05",
	};
	black_box_state_t test_case_get_node_reachability_06_state = {
		.test_case_name = "test_case_get_node_reachability_06",
	};
	black_box_state_t test_case_get_node_reachability_07_state = {
		.test_case_name = "test_case_get_node_reachability_07",
	};

	const struct CMUnitTest blackbox_status_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_get_node_reachability_01, NULL, NULL,
		                (void *)&test_case_get_node_reachability_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_get_node_reachability_02, NULL, NULL,
		                (void *)&test_case_get_node_reachability_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_get_node_reachability_03, NULL, NULL,
		                (void *)&test_case_get_node_reachability_03_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_get_node_reachability_04, NULL, NULL,
		                (void *)&test_case_get_node_reachability_04_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_get_node_reachability_05, NULL, NULL,
		                (void *)&test_case_get_node_reachability_05_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_get_node_reachability_06, NULL, NULL,
		                (void *)&test_case_get_node_reachability_06_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_get_node_reachability_07, NULL, NULL,
		                (void *)&test_case_get_node_reachability_07_state),
	};
	total_tests += sizeof(blackbox_status_tests) / sizeof(blackbox_status_tests[0]);

	return cmocka_run_group_tests(blackbox_status_tests, NULL, NULL);
}
