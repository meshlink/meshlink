/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_destroy.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_destroy.h"
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

/*************************************************************************************
 *                          LOCAL PROTOTYPES                                         *
 *************************************************************************************/
static void test_case_meshlink_destroy_01(void **state);
static bool test_meshlink_destroy_01(void);
static void test_case_meshlink_destroy_02(void **state);
static bool test_meshlink_destroy_02(void);
static void test_case_meshlink_destroy_03(void **state);
static bool test_meshlink_destroy_03(void);

/*************************************************************************************
 *                          LOCAL VARIABLES                                          *
 *************************************************************************************/
static black_box_state_t test_case_meshlink_destroy_01_state = {
    /* test_case_name = */ "test_case_meshlink_destroy_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_meshlink_destroy_02_state = {
    /* test_case_name = */ "test_case_meshlink_destroy_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_meshlink_destroy_03_state = {
    /* test_case_name = */ "test_case_meshlink_destroy_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/

/* Execute destroy Test Case # 1 - valid case*/
static void test_case_meshlink_destroy_01(void **state) {
    execute_test(test_meshlink_destroy_01, state);
    return;
}

/*
    Test Steps:
    1. Run NUT, sleep for a second
    2. Stop and Close NUT, and destroy the confbase

    Expected Result:
    confbase should be deleted
*/
/* TODO: Can meshlink_destroy be implemented using mesh_handle as argument rather
    confbase directly as an argument which can probably be safer */
static bool test_meshlink_destroy_01(void) {
  bool result = false;
  fprintf(stderr, "[ destroy 01 ] Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  mesh_handle = meshlink_open("destroyconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  fprintf(stderr, "[ destroy 01] Destroying NUT's confbase\n");
  result = meshlink_destroy("destroyconf");

  if (result) {
    fprintf(stderr, "[ destroy 01 ] destroyed confbase successfully\n");
    return true;
  }
  else {
    fprintf(stderr, "[ destroy 01 ] failed to destroy confbase\n");
    return false;
  }
}



/* Execute destroy Test Case # 2 - passing NULL argument to the API */
static void test_case_meshlink_destroy_02(void **state) {
    execute_test(test_meshlink_destroy_02, state);
    return;
}

/*
    Test Steps:
    1. Just passing NULL as argument to the API

    Expected Result:
    Return false reporting failure
*/
static bool test_meshlink_destroy_02(void) {
  fprintf(stderr, "[ destroy 02 ] Passing NULL as an argument to meshlink_destroy\n");

  bool result = meshlink_destroy(NULL);

  if (!result) {
    fprintf(stderr, "[ destroy 02 ] Error reported by returning false when NULL is passed as confbase argument\n");
    return true;
  }
  else {
    fprintf(stderr, "[ destroy 02 ] Failed to report error when NULL is passed as confbase argument\n");
    return true;
  }
}




/* Execute status Test Case # 3 - destroying non existing file */
static void test_case_meshlink_destroy_03(void **state) {
    execute_test(test_meshlink_destroy_03, state);
    return;
}

/*

    Test Steps:
    1. unlink if there's any such test file
    2. Call API with that file name

    Expected Result:
    Return false reporting failure
*/
static bool test_meshlink_destroy_03(void) {
  bool result = false;

  unlink("non_existing_file");

  fprintf(stderr, "[ destroy 03 ] Passing non-existing file as an argument to meshlink_destroy\n");

  result = meshlink_destroy("non_existing_file");

  return !result;
}


/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/

int test_meshlink_destroy(void) {
  const struct CMUnitTest blackbox_destroy_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_destroy_01, NULL, NULL,
          (void *)&test_case_meshlink_destroy_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_destroy_02, NULL, NULL,
          (void *)&test_case_meshlink_destroy_02_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_destroy_03, NULL, NULL,
          (void *)&test_case_meshlink_destroy_03_state)
  };

  total_tests += sizeof(blackbox_destroy_tests) / sizeof(blackbox_destroy_tests[0]);

  return cmocka_run_group_tests(blackbox_destroy_tests ,NULL , NULL);
}
