/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_channel_ex.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_channel_ex.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <pthread.h>
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
static void test_case_channel_ex_01(void **state);
static bool test_steps_channel_ex_01(void);
static void test_case_channel_ex_02(void **state);
static bool test_steps_channel_ex_02(void);
static void test_case_channel_ex_03(void **state);
static bool test_steps_channel_ex_03(void);
static void test_case_channel_ex_04(void **state);
static bool test_steps_channel_ex_04(void);
static void test_case_channel_ex_05(void **state);
static bool test_steps_channel_ex_05(void);
static void test_case_channel_ex_06(void **state);
static bool test_steps_channel_ex_06(void);
static void test_case_channel_ex_07(void **state);
static bool test_steps_channel_ex_07(void);

static void cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len);

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len);

/*************************************************************************************
 *                          LOCAL VARIABLES                                          *
 *************************************************************************************/
/* channel_acc gives us access to test whether the accept callback has been invoked or not */
static bool channel_acc;

/* mutex for the common variable */
pthread_mutex_t lock;

static black_box_state_t test_case_channel_ex_01_state = {
    /* test_case_name = */ "test_case_channel_ex_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_ex_02_state = {
    /* test_case_name = */ "test_case_channel_ex_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_ex_03_state = {
    /* test_case_name = */ "test_case_channel_ex_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_ex_04_state = {
    /* test_case_name = */ "test_case_channel_ex_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_ex_05_state = {
    /* test_case_name = */ "test_case_channel_ex_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_ex_06_state = {
    /* test_case_name = */ "test_case_channel_ex_06",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_ex_07_state = {
    /* test_case_name = */ "test_case_channel_ex_07",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* channel receive callback */
static void cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
  char *data = (char *) dat;
  fprintf(stderr, "Invoked channel Receive callback\n");

  if (dat != NULL) {
  fprintf(stderr, "Received message is : %s\n", data);
  }
}

/* channel accept callback */
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len) {
	(void)dat;
	(void)len;
  char *data = (char *) dat;

  pthread_mutex_lock(&lock);
	channel_acc = true;
	pthread_mutex_unlock(&lock);

	fprintf(stderr, "Accepted incoming channel from '%s'\n", channel->node->name);
	fprintf(stderr, "received data is : %s \n", data);

	// Accept this channel by default
	return true;
}



/* Execute meshlink_channel_open_ex Test Case # 1 - testing meshlink_channel_open_ex API's
    valid case by passing all valid arguments */
static void test_case_channel_ex_01(void **state) {
  execute_test(test_steps_channel_ex_01, state);
  return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 1 - Valid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself

    Expected Result:
    Opens a channel and echoes the send queue data.
*/
/* TODO: When send queue & send queue length are passed with some value other
          than NULL it throws segmentation fault*/
static bool test_steps_channel_ex_01(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  fprintf(stderr, "[ channel open ex 01 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Set up callback for node status (reachable / unreachable) */
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

  /* Set up callback for channel accept */
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  fprintf(stderr, "[ channel open ex 01 ] starting mesh\n");
  assert(meshlink_start(mesh_handle));

  /* Getting node handle for itself */
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  /* TODO: Make possible to open a channel immediately after starting mesh */
  sleep(1);

  char string[100] = "Test the 1st case";
  /* Making the channel_acc false before opening channel */
  pthread_mutex_lock(&lock);
  channel_acc = false;
  pthread_mutex_unlock(&lock);

  fprintf(stderr, "[ channel open ex 01 ] Opening UDP channel ex\n");
  /* Passing all valid arguments for meshlink_channel_open_ex */
  meshlink_channel_t *channel = NULL;
  channel = meshlink_channel_open_ex(mesh_handle, node, PORT, cb, string, strlen(string) + 1, MESHLINK_CHANNEL_UDP);
  if (channel == NULL) {
    fprintf(stderr, "meshlink_channel_open_ex status : %s\n", meshlink_strerror(meshlink_errno));
  }

  // Delay for establishing a channel
  sleep(1);

  pthread_mutex_lock(&lock);
  bool ret = channel_acc;
  pthread_mutex_unlock(&lock);

  if (ret) {
    fprintf(stderr, "[ channel open ex 01 ] meshlink_channel_open_ex opened a channel and invoked accept callback\n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
  }
  else {
    fprintf(stderr, "[ channel open ex 01 ] meshlink_channel_open_ex failed to invoke accept callback\n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
  }
}



/* Execute meshlink_channel_open_ex Test Case # 2 - testing API's valid case by passing NULL and
    0 for send queue & it's length respectively and others with valid arguments */
static void test_case_channel_ex_02(void **state) {
    execute_test(test_steps_channel_ex_02, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 2 - Valid case (TCP channel)

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself

    Expected Result:
    Opens a TCP channel successfully by setting channel_acc true*/
static bool test_steps_channel_ex_02(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  fprintf(stderr, "[ channel open ex 02 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Set up callback for node status (reachable / unreachable) */
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

  /* Set up callback for channel accept */
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  fprintf(stderr, "[ channel open ex 02 ] starting mesh\n");
  assert(meshlink_start(mesh_handle));

  /* Getting node handle for itself */
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  /* Making the channel_acc false before opening channel */
  pthread_mutex_lock(&lock);
  channel_acc = false;
  pthread_mutex_unlock(&lock);

  /* TODO: Make possible to open a channel immediately after starting mesh */
  sleep(1);

  fprintf(stderr, "[ channel open ex 02 ] Opening TCP channel ex\n");
  /* Passing all valid arguments for meshlink_channel_open_ex */
  meshlink_channel_t *channel;
  channel = meshlink_channel_open_ex(mesh_handle, node, PORT, cb, NULL, 0, MESHLINK_CHANNEL_TCP);
  if (channel == NULL) {
    fprintf(stderr, "meshlink_channel_open_ex status : %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(channel!= NULL);

  // Delay for establishing a channel
  sleep(1);

  pthread_mutex_lock(&lock);
  bool ret = channel_acc;
  pthread_mutex_unlock(&lock);

  if (ret) {
    fprintf(stderr, "[ channel open ex 02 ] meshlink_channel_open_ex opened a channel and invoked accept callback\n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
  }
  else {
    fprintf(stderr, "[ channel open ex 02 ] meshlink_channel_open_ex failed to invoke accept callback\n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
  }
}



/* Execute meshlink_channel_open_ex Test Case # 3 - Open a UDP channel */
static void test_case_channel_ex_03(void **state) {
    execute_test(test_steps_channel_ex_03, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 3 - Valid case (UDP channel)

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself

    Expected Result:
    Opens a UDP channel successfully by setting channel_acc true */
static bool test_steps_channel_ex_03(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  fprintf(stderr, "[ channel open ex 03 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Set up callback for node status (reachable / unreachable) */
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

  /* Set up callback for channel accept */
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  fprintf(stderr, "[ channel open ex 03 ] starting mesh\n");
  assert(meshlink_start(mesh_handle));

  /* Getting node handle for itself */
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  /* Making the channel_acc false before opening channel */
  pthread_mutex_lock(&lock);
  channel_acc = false;
  pthread_mutex_unlock(&lock);

  /* TODO: Make possible to open a channel immediately after starting mesh */
  sleep(1);

  fprintf(stderr, "[ channel open ex 03 ] Opening TCP channel ex\n");
  /* Passing all valid arguments for meshlink_channel_open_ex */
  meshlink_channel_t *channel;
  channel = meshlink_channel_open_ex(mesh_handle, node, PORT, cb, NULL, 0, MESHLINK_CHANNEL_UDP);
  if (channel == NULL) {
    fprintf(stderr, "meshlink_channel_open_ex status : %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(channel!= NULL);

  // Delay for establishing a channel
  sleep(1);

  pthread_mutex_lock(&lock);
  bool ret = channel_acc;
  pthread_mutex_unlock(&lock);

  if (ret) {
    fprintf(stderr, "[ channel open ex 03 ] meshlink_channel_open_ex opened a channel and invoked accept callback\n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
  }
  else {
    fprintf(stderr, "[ channel open ex 03 ] meshlink_channel_open_ex failed to invoke accept callback\n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
  }
}

/* Execute meshlink_channel_open_ex Test Case # 4 - Open a TCP channel with no receive callback
    and send queue */
static void test_case_channel_ex_04(void **state) {
    execute_test(test_steps_channel_ex_04, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 4 - Valid Case (Disabling receive callback)

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself

    Expected Result:
    Opens a channel
*/

static bool test_steps_channel_ex_04(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  fprintf(stderr, "[ channel open ex 04 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Set up callback for node status (reachable / unreachable) */
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

  /* Set up callback for channel accept */
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  fprintf(stderr, "[ channel open ex 04 ] starting mesh\n");
  assert(meshlink_start(mesh_handle));

  /* Getting node handle for itself */
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  /* Making the channel_acc false before opening channel */
  pthread_mutex_lock(&lock);
  channel_acc = false;
  pthread_mutex_unlock(&lock);

  /* TODO: Make possible to open a channel immediately after starting mesh */
  sleep(1);

  fprintf(stderr, "[ channel open ex 04 ] Opening TCP channel ex with no receive callback \n");
  /* Passing all valid arguments for meshlink_channel_open_ex i.e disabling receive callback and send queue */
  meshlink_channel_t *channel;
  channel = meshlink_channel_open_ex(mesh_handle, node, PORT, NULL, NULL, 0, MESHLINK_CHANNEL_UDP);
  if (channel == NULL) {
    fprintf(stderr, "meshlink_channel_open_ex status : %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(channel!= NULL);

  // Delay for establishing a channel
  sleep(1);

  pthread_mutex_lock(&lock);
  bool ret = channel_acc;
  pthread_mutex_unlock(&lock);

  if (ret) {
    fprintf(stderr, "[ channel open ex 04 ] meshlink_channel_open_ex opened a channel and invoked accept callback\n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
  }
  else {
    fprintf(stderr, "[ channel open ex 04] meshlink_channel_open_ex failed to invoke accept callback\n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
  }
}

/* Execute meshlink_channel_open_ex Test Case # 5 - Opening channel using NULL as mesh handle argument
    for the API */
static void test_case_channel_ex_05(void **state) {
    execute_test(test_steps_channel_ex_05, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 5 - Invalid case (NULL as mesh argument)

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel by passing NULL as argument for mesh handle

    Expected Result:
    meshlink_channel_open_ex returns NULL as channel handle reporting error accordingly
*/
static bool test_steps_channel_ex_05(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  fprintf(stderr, "[ channel open ex 05 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Set up callback for node status (reachable / unreachable) */
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

  /* Set up callback for channel accept */
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  fprintf(stderr, "[ channel open ex 05 ] starting mesh\n");
  assert(meshlink_start(mesh_handle));

  /* Getting node handle for itself */
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  /* TODO: Make possible to open a channel immediately after starting mesh */
  sleep(1);

  fprintf(stderr, "[ channel open ex 05 ] Trying to open channel using mesh handle as NULL argument \n");
  meshlink_channel_t *channel = meshlink_channel_open_ex(NULL, node, PORT, cb, NULL, 0, MESHLINK_CHANNEL_TCP);
  assert(channel == NULL);

  if (channel == NULL) {
    fprintf(stderr, "[ channel open ex 06 ] Error reported correctly \n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
  }
  else {
    fprintf(stderr, "[ channel open ex 06 ] Failed to report error \n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
  }
}

/* Execute meshlink_channel_open_ex Test Case # 6 - Opening channel using NULL as node handle argument
    for the API*/
static void test_case_channel_ex_06(void **state) {
    execute_test(test_steps_channel_ex_06, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 6 - Invalid case (NULL as node argument)

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel by passing NULL as argument for node handle

    Expected Result:
    meshlink_channel_open_ex returns NULL as channel handle reporting error accordingly
*/
static bool test_steps_channel_ex_06(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  fprintf(stderr, "[ channel open ex 06 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Set up callback for node status (reachable / unreachable) */
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

  /* Set up callback for channel accept */
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  fprintf(stderr, "[ channel open ex 06 ] starting mesh\n");
  assert(meshlink_start(mesh_handle));

  /* Getting node handle for itself */
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  /* TODO: Make possible to open a channel immediately after starting mesh */
  sleep(1);

  fprintf(stderr, "[ channel open ex 06 ] Trying to open channel using node handle as NULL argument \n");
  meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_handle, NULL, PORT, cb, NULL, 0, MESHLINK_CHANNEL_TCP);
  assert(channel == NULL);

  if (channel == NULL) {
    fprintf(stderr, "[ channel open ex 06 ] Error reported correctly \n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
  }
  else {
    fprintf(stderr, "[ channel open ex 06 ] Failed to report error \n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
  }
}

/* Execute meshlink_channel_open_ex Test Case # 7 Opening channel using invalid argument as
    flag argument for the API*/
static void test_case_channel_ex_07(void **state) {
    execute_test(test_steps_channel_ex_07, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 7 Invalid case (invalid value for channel flag argument)

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel by passing invalid value other channel flag as argument to the API

    Expected Result:
    meshlink_channel_open_ex returns NULL as channel handle reporting error accordingly
*/
/* TODO: Can an error reporting be done for an invalid flag or should be left as it is */
static bool test_steps_channel_ex_07(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  fprintf(stderr, "[ channel open ex 07 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Set up callback for node status (reachable / unreachable) */
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

  /* Set up callback for channel accept */
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  fprintf(stderr, "[ channel open ex 07 ] starting mesh\n");
  assert(meshlink_start(mesh_handle));

  /* Getting node handle for itself */
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  /* TODO: Make possible to open a channel immediately after starting mesh */
  sleep(1);

  fprintf(stderr, "[ channel open ex 07 ] Trying to open channel using invalid flag argument \n");
  meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_handle, node, PORT, cb, NULL, 0, 1000);

  if (channel == NULL) {
    fprintf(stderr, "[ channel open ex 07 ] Error reported correctly \n");
    /* Closing mesh and destroying it's confbase */
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
  }
  else {
    fprintf(stderr, "[ channel open ex 07 ] Failed to report error \n");
    /* Closing mesh and destroying it's confbase */
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

int test_meshlink_channel_open_ex(void) {
  const struct CMUnitTest blackbox_channel_ex_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_01, NULL, NULL,
            (void *)&test_case_channel_ex_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_02, NULL, NULL,
            (void *)&test_case_channel_ex_02_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_03, NULL, NULL,
            (void *)&test_case_channel_ex_03_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_04, NULL, NULL,
            (void *)&test_case_channel_ex_04_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_05, NULL, NULL,
            (void *)&test_case_channel_ex_05_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_06, NULL, NULL,
            (void *)&test_case_channel_ex_06_state),
 //   cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_07, NULL, NULL,
 //           (void *)&test_case_channel_ex_07_state)
  };

  total_tests += sizeof(blackbox_channel_ex_tests) / sizeof(blackbox_channel_ex_tests[0]);

  assert(pthread_mutex_init(&lock, NULL) == 0);
  int failed = cmocka_run_group_tests(blackbox_channel_ex_tests ,NULL , NULL);
  assert(pthread_mutex_destroy(&lock) == 0);

  return failed;
}
