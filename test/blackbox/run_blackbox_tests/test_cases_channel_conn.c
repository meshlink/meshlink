/*
    test_cases_channel_conn.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_channel_conn.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../common/mesh_event_handler.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <pthread.h>
#include <cmocka.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG
/* Modify this to change the port number */
#define PORT 8000

#define PEER_ID "0"
#define NUT_ID  "1"

static void test_case_channel_conn_01(void **state);
static bool test_steps_channel_conn_01(void);

static char *test_channel_conn_1_nodes[] = { "peer", "nut" };
static black_box_state_t test_case_channel_conn_01_state = {
    .test_case_name = "test_case_channel_conn_01",
    .node_names = test_channel_conn_1_nodes,
    .num_nodes = 2,
};

static bool joined;
static bool channel_opened;
static bool peer_restarted;
static bool received_error;

static int black_box_group_setup(void **state) {
  const char *nodes[] = { "relay", "nut" };
  int num_nodes = sizeof(nodes) / sizeof(nodes[0]);

  printf("Creating Containers\n");
  destroy_containers();
  create_containers(nodes, num_nodes);

  return 0;
}

static int black_box_group_teardown(void **state) {
  printf("Destroying Containers\n");
  destroy_containers();

  return 0;
}

/* Execute channel connections Test Case # 1 - receiving an error when node-under-test
    tries to send data on channel to peer node after peer node stops and starts the
    node instance */
static void test_case_channel_conn_01(void **state) {
  execute_test(test_steps_channel_conn_01, state);
  return;
}

static void channel_conn01_cb(mesh_event_payload_t payload) {
  char event_node_name[][10] = {"PEER", "NUT"};

  switch(payload.mesh_event) {
    case NODE_JOINED      : joined = true;
                            break;
    case CHANNEL_OPENED   : channel_opened = true;
                            break;
    case NODE_RESTARTED   : peer_restarted = true;
                            break;
    case ERR_NETWORK      : received_error = true;
                            break;
    default               : PRINT_TEST_CASE_MSG("Undefined event occurred\n");
  }

  return;
}

/* Test Steps for Meta-connections Test Case # 1 - receiving an error when node-under-test
    tries to send data on channel to peer node after peer node stops and starts the
    node instance

    Test Steps:

    Expected Result:
*/
static bool test_steps_channel_conn_01(void) {
  char *invite_nut;
  char *import;

  import = mesh_event_sock_create(eth_if_name);
  invite_nut = invite_in_container("peer", "nut");

  joined = false;
  channel_opened = false;
  node_sim_in_container_event("peer", "1", NULL, PEER_ID, import);
  node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);

  wait_for_event(channel_conn01_cb, 10);
  assert_int_equal(joined, true);

  wait_for_event(channel_conn01_cb, 10);
  assert_int_equal(channel_opened, true);

  wait_for_event(channel_conn01_cb, 105);
  assert_int_equal(peer_restarted, true);

  // Sending SIGUSR1 signal to node-under-test which sends data on the channel to peer
  received_error = false;
  node_step_in_container("nut", "SIGUSR1");
  wait_for_event(channel_conn01_cb, 15);
  assert_int_equal(received_error, true);

  return true;
}



int test_case_channel_conn(void) {
  const struct CMUnitTest blackbox_group0_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_channel_conn_01, setup_test, teardown_test,
            (void *)&test_case_channel_conn_01_state)
  };
  total_tests += sizeof(blackbox_group0_tests) / sizeof(blackbox_group0_tests[0]);

  return cmocka_run_group_tests(blackbox_group0_tests, black_box_group_setup, black_box_group_teardown);
}
