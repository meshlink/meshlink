/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_channel_get_flags.c -- Execution of specific meshlink black box test cases
 * @see
 * @author    Sai Roop, sairoop@elear.solutions
 * @copyright 2017  Guus Sliepen <guus@meshlink.io>
 *                  Manav Kumar Mehta <manavkumarm@yahoo.com>
 * @license   To any person (the "Recipient") obtaining a copy of this software and
 *            associated documentation files (the "Software"):\n
 *            All information contained in or disclosed by this software is
 *            confidential and proprietary information of Elear Solutions Tech
 *            Private Limited and all rights therein are expressly reserved.
 *            By accepting this material the recipient agrees that this material and
 *            the information contained therein is held in confidence and in trust
 *            and will NOT be used, copied, modified, merged, published, distributed,
 *            sublicensed, reproduced in whole or in part, nor its contents revealed
 *            in any manner to others without the express written permission of
 *            Elear Solutions Tech Private Limited.
 */
/*************************************************************************************/
/*===================================================================================*/
#include "execute_tests.h"
#include "test_cases_channel_get_flags.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
/*************************************************************************************
 *                          LOCAL MACROS                                             *
 *************************************************************************************/
/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG
/* Modify this to change the port number */
#define PORT 8000

/*************************************************************************************
 *                          LOCAL PROTOTYPES                                         *
 *************************************************************************************/
static void test_case_channel_get_flags_01(void **state);
static bool test_steps_channel_get_flags_01(void);
static void test_case_channel_get_flags_02(void **state);
static bool test_steps_channel_get_flags_02(void);
static void test_case_channel_get_flags_03(void **state);
static bool test_steps_channel_get_flags_03(void);

/*************************************************************************************
 *                          LOCAL VARIABLES                                          *
 *************************************************************************************/
static black_box_state_t test_case_channel_get_flags_01_state = {
    /* test_case_name = */ "test_case_channel_get_flags_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_get_flags_02_state = {
    /* test_case_name = */ "test_case_channel_get_flags_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_get_flags_03_state = {
    /* test_case_name = */ "test_case_channel_get_flags_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_get_flags_04_state = {
    /* test_case_name = */ "test_case_channel_get_flags_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* Execute meshlink_channel_get_flags Test Case # 1 - Valid case*/
static void test_case_channel_get_flags_01(void **state) {
    execute_test(test_steps_channel_get_flags_01, state);
    return;
}

/* Test Steps for meshlink_channel_get_flags Test Case # 1

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself (with TCP semantic here)
    3. Get flag(s) of that channel

    Expected Result:
    API returning exact flag that has been assigned while opening (here TCP)
*/
static bool test_steps_channel_get_flags_01(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  fprintf(stderr, "[ channel get flags 01 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Set up callback for node status (reachable / unreachable) */
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

  /* Set up callback for channel accept */
	meshlink_set_channel_accept_cb(mesh_handle, NULL);

  fprintf(stderr, "[ channel get flags 01 ] starting mesh\n");
  assert(meshlink_start(mesh_handle));

  /* Getting node handle for itself */
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  /* TODO: Make possible to open a channel immediately after starting mesh */
  sleep(1);

  fprintf(stderr, "[ channel get flags 01 ] Opening TCP channel ex\n");
  /* Passing all valid arguments for meshlink_channel_open_ex */
  meshlink_channel_t *channel;
  channel = meshlink_channel_open_ex(mesh_handle, node, PORT, NULL, NULL, 0, MESHLINK_CHANNEL_TCP);
  if (channel == NULL) {
    fprintf(stderr, "meshlink_channel_open_ex status : %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(channel!= NULL);


  fprintf(stderr, "[ channel get flags 01 ] Obtaining channel flags using meshlink_channel_get_flags\n");
  uint32_t flags = meshlink_channel_get_flags(mesh_handle, channel);
  if (flags == MESHLINK_CHANNEL_TCP) {
    fprintf(stderr, "[ channel get flags 01 ] meshlink_channel_get_flags obtained flags correctly\n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
  }
  else {
    fprintf(stderr, "[ channel get flags 01 ] meshlink_channel_get_flags failed obtain flags \n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
  }
}



/* Execute meshlink_channel_get_flags Test Case # 2 - Invalid case*/
static void test_case_channel_get_flags_02(void **state) {
    execute_test(test_steps_channel_get_flags_02, state);
    return;
}

/* Test Steps for meshlink_channel_get_flags Test Case # 2

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself (with TCP semantic here)
    3. Call meshlink_channel_get_flags by passing NULL as mesh handle argument

    Expected Result:
    API reporting error accordingly.
*/
static bool test_steps_channel_get_flags_02(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  fprintf(stderr, "[ channel get flags 02 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Set up callback for node status (reachable / unreachable) */
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

  /* Set up callback for channel accept */
	meshlink_set_channel_accept_cb(mesh_handle, NULL);

  fprintf(stderr, "[ channel get flags 02 ] starting mesh\n");
  assert(meshlink_start(mesh_handle));

  /* Getting node handle for itself */
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  /* TODO: Make possible to open a channel immediately after starting mesh */
  sleep(1);

  fprintf(stderr, "[ channel get flags 02 ] Opening TCP channel ex\n");
  /* Passing all valid arguments for meshlink_channel_open_ex */
  meshlink_channel_t *channel;
  channel = meshlink_channel_open_ex(mesh_handle, node, PORT, NULL, NULL, 0, MESHLINK_CHANNEL_TCP);
  if (channel == NULL) {
    fprintf(stderr, "meshlink_channel_open_ex status : %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(channel!= NULL);


  fprintf(stderr, "[ channel get flags 02 ] passing NULL as mesh handle argument for meshlink_channel_get_flags\n");
  uint32_t flags = meshlink_channel_get_flags(NULL, channel);

  if (((int32_t)flags == -1) && (meshlink_errno == MESHLINK_EINVAL)) {
    fprintf(stderr, "[ channel get flags 02 ] Reported error correctly when NULL is passed as argument\n");
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
  }
  else {
    fprintf(stderr, "[ channel get flags 02 ] failed to report error when NULL is passed as argument\n");
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
  }
}



/* Execute meshlink_channel_get flags Test Case # 3 - Invalid case*/
static void test_case_channel_get_flags_03(void **state) {
    execute_test(test_steps_channel_get_flags_03, state);
    return;
}

/* Test Steps for meshlink_channel_get_flags Test Case # 3

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself (with TCP semantic here)
    3. Call meshlink_channel_get_flags by passing NULL as channel handle argument

    Expected Result:
    API reporting error accordingly.
*/
static bool test_steps_channel_get_flags_03(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  fprintf(stderr, "[ channel get flags 03 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Set up callback for node status (reachable / unreachable) */
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

  /* Set up callback for channel accept */
	meshlink_set_channel_accept_cb(mesh_handle, NULL);

  fprintf(stderr, "[ channel get flags 03 ] starting mesh\n");
  assert(meshlink_start(mesh_handle));

  /* Getting node handle for itself */
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  /* TODO: Make possible to open a channel immediately after starting mesh */
  sleep(1);

  fprintf(stderr, "[ channel get flags 03 ] Opening TCP channel ex\n");
  /* Passing all valid arguments for meshlink_channel_open_ex */
  meshlink_channel_t *channel;
  channel = meshlink_channel_open_ex(mesh_handle, node, PORT, NULL, NULL, 0, MESHLINK_CHANNEL_TCP);
  if (channel == NULL) {
    fprintf(stderr, "meshlink_channel_open_ex status : %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(channel!= NULL);

  fprintf(stderr, "[ channel get flags 03 ] passing NULL as channel handle argument for meshlink_channel_get_flags\n");
  uint32_t flags = meshlink_channel_get_flags(mesh_handle, NULL);

  if (((int32_t)flags == -1) && (meshlink_errno == MESHLINK_EINVAL)) {
    fprintf(stderr, "[ channel get flags 03 ] Reported error correctly when NULL is passed as argument\n");
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
  }
  else {
    fprintf(stderr, "[ channel get flags 03 ] failed to report error when NULL is passed as argument\n");
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
  }
}

/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/

int test_meshlink_channel_get_flags(void) {
  const struct CMUnitTest blackbox_channel_get_flags_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_get_flags_01, NULL, NULL,
            (void *)&test_case_channel_get_flags_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_get_flags_02, NULL, NULL,
            (void *)&test_case_channel_get_flags_02_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_get_flags_03, NULL, NULL,
            (void *)&test_case_channel_get_flags_03_state)
  };

  total_tests += sizeof(blackbox_channel_get_flags_tests) / sizeof(blackbox_channel_get_flags_tests[0]);

  return cmocka_run_group_tests(blackbox_channel_get_flags_tests ,NULL , NULL);
}
