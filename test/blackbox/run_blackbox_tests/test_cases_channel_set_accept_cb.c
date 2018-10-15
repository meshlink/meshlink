/*
    test_cases_channel_set_accept_cb.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>

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
#include "execute_tests.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "test_cases_channel_set_accept_cb.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <pthread.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG
/* Modify this to change the port number */
#define PORT 8000

static void test_case_set_channel_accept_cb_01(void **state);
static bool test_steps_set_channel_accept_cb_01(void);
static void test_case_set_channel_accept_cb_02(void **state);
static bool test_steps_set_channel_accept_cb_02(void);

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len);

/* channel_acc gives us access to test whether the accept callback has been invoked or not */
static bool channel_acc;
/* mutex for the common variable */
pthread_mutex_t lock;

static black_box_state_t test_case_channel_set_accept_cb_01_state = {
    /* test_case_name = */ "test_case_channel_set_accept_cb_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};
static black_box_state_t test_case_channel_set_accept_cb_02_state = {
    /* test_case_name = */ "test_case_channel_set_accept_cb_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


/* channel accept callback */
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len) {
	(void)dat;
	(void)len;
  char *data = (char *) dat;

  pthread_mutex_lock(&lock);
	channel_acc = true;
	pthread_mutex_unlock(&lock);

	fprintf(stderr, "received data is : %s \n", data);

	if (PORT == port) {
	  fprintf(stderr, "Accepted incoming channel from '%s'\n", channel->node->name);
    return true;
	}
	else {
	  fprintf(stderr, "Rejected incoming channel from '%s'\n", channel->node->name);
  	return false;
	}
}

/* Execute meshlink_channel_set_accept_cb Test Case # 1 - Valid case*/
static void test_case_set_channel_accept_cb_01(void **state) {
    execute_test(test_steps_set_channel_accept_cb_01, state);
    return;
}
/* Test Steps for meshlink_channel_set_accept_cb Test Case # 1 - Valid case

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Set channel_accept callback for NUT's meshlink_set_channel_accept_cb API.
    3. Export and Import to merge NUT and bar nodes in a single mesh.
    4. Obtain node handle of NUT with bar's mesh handle.
    5. Open a channel with NUT from bar to invoke channel accept callback

    Expected Result:
    Opens a channel by invoking accept callback.
*/
static bool test_steps_set_channel_accept_cb_01(void) {
  /* deleting the confbase if already exists */
  meshlink_destroy("acceptconf1");
  meshlink_destroy("acceptconf2");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance for NUT */
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh1 = meshlink_open("acceptconf1", "nut", "chat", DEV_CLASS_STATIONARY);
  if(!mesh1) {
    PRINT_TEST_CASE_MSG("meshlink_open status for NUT: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh1 != NULL);

  /* Create meshlink instance for bar */
  PRINT_TEST_CASE_MSG("Opening bar\n");
  meshlink_handle_t *mesh2 = meshlink_open("acceptconf2", "bar", "chat", DEV_CLASS_STATIONARY);
  if(!mesh2) {
    fprintf(stderr, "meshlink_open status for bar: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh2 != NULL);

  PRINT_TEST_CASE_MSG("setting channel accept cb for NUT\n");
  meshlink_set_channel_accept_cb(mesh1, channel_accept);
  PRINT_TEST_CASE_MSG("setting NULL for channel accept cb for bar\n");
  meshlink_set_channel_accept_cb(mesh2, channel_accept);
  /* Making the channel_acc false before opening channel */
  pthread_mutex_lock(&lock);
  channel_acc = false;
  pthread_mutex_unlock(&lock);

  /* importing and exporting mesh meta data */
  char *exp1 = meshlink_export(mesh1);
  assert(exp1 != NULL);
  char *exp2 = meshlink_export(mesh2);
  assert(exp2 != NULL);
  assert(meshlink_import(mesh1, exp2));
  assert(meshlink_import(mesh2, exp1));

  PRINT_TEST_CASE_MSG("NUT and bar connected successfully\n");

  PRINT_TEST_CASE_MSG("acquiring NUT node handle from bar\n");
  meshlink_node_t *destination = meshlink_get_node(mesh2, "nut");
  assert(destination != NULL);

  bool mesh1_start = meshlink_start(mesh1);
  if(!mesh1_start) {
    fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh1_start);
  sleep(1);
  bool mesh2_start = meshlink_start(mesh2);
  if(!mesh2_start) {
  	fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh2_start);
  sleep(1);

  meshlink_channel_t *channel = meshlink_channel_open(mesh2, destination, PORT, NULL, NULL, 0);
  if(!channel) {
    fprintf(stderr, "Could not create channel to '%s': %s\n", destination->name, meshlink_strerror(meshlink_errno));
    return false;
  }
  sleep(1);

  pthread_mutex_lock(&lock);
  bool ret = channel_acc;
  pthread_mutex_unlock(&lock);
  if(ret) {
    PRINT_TEST_CASE_MSG("Accept callback invoked successfully when a new node imported\n");
  } else {
    PRINT_TEST_CASE_MSG("Accept callback not invoked when a new node imported\n");
  }

  /* closing channel, meshes and destroying confbase */
  meshlink_channel_close(mesh2, channel);
  meshlink_stop(mesh1);
  meshlink_stop(mesh2);
  meshlink_close(mesh1);
  meshlink_close(mesh2);
  meshlink_destroy("acceptconf1");
  meshlink_destroy("acceptconf2");
  // returns according whether callback is invoked or not.
  return ret;
}

/* Execute meshlink_channel_set_accept_cb Test Case # 2 - Invalid case*/
static void test_case_set_channel_accept_cb_02(void **state) {
    execute_test(test_steps_set_channel_accept_cb_02, state);
    return;
}
/* Test Steps for meshlink_channel_set_accept_cb Test Case # 2 - Invalid case

    Test Steps:
    1. Passing NULL as mesh handle argument for channel accept callback.

    Expected Result:
    meshlink_channel_set_accept_cb returning proper meshlink_errno.
*/
static bool test_steps_set_channel_accept_cb_02(void) {
  PRINT_TEST_CASE_MSG("setting channel accept cb for NUT\n");
  meshlink_set_channel_accept_cb(NULL, channel_accept);
  if(meshlink_errno == MESHLINK_EINVAL) {
    PRINT_TEST_CASE_MSG("Accept callback reported error successfully when NULL is passed as mesh argument\n");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("Accept callback didn't report error when NULL is passed as mesh argument\n");
    return false;
  }
}


int test_meshlink_set_channel_accept_cb(void) {
  const struct CMUnitTest blackbox_channel_set_accept_cb_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_set_channel_accept_cb_01, NULL, NULL,
            (void *)&test_case_channel_set_accept_cb_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_set_channel_accept_cb_02, NULL, NULL,
            (void *)&test_case_channel_set_accept_cb_02_state)
  };

  total_tests += sizeof(blackbox_channel_set_accept_cb_tests) / sizeof(blackbox_channel_set_accept_cb_tests[0]);

  assert(pthread_mutex_init(&lock, NULL) == 0);
  int failed = cmocka_run_group_tests(blackbox_channel_set_accept_cb_tests ,NULL , NULL);
  assert(pthread_mutex_destroy(&lock) == 0);

  return failed;
}



