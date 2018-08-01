/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_invite.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_invite.h"
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
static void test_case_invite_01(void **state);
static bool test_invite_01(void);
static void test_case_invite_02(void **state);
static bool test_invite_02(void);
static void test_case_invite_03(void **state);
static bool test_invite_03(void);
static void test_case_invite_04(void **state);
static bool test_invite_04(void);
static void test_case_invite_05(void **state);
static bool test_invite_05(void);
static void test_case_invite_06(void **state);
static bool test_invite_06(void);

/*************************************************************************************
 *                          LOCAL VARIABLES                                          *
 *************************************************************************************/
/* State structure for invite API Test Case #1 */
static black_box_state_t test_case_invite_01_state = {
    /* test_case_name = */ "test_case_invite_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for invite API Test Case #2 */
static black_box_state_t test_case_invite_02_state = {
    /* test_case_name = */ "test_case_invite_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for invite API Test Case #3 */
static black_box_state_t test_case_invite_03_state = {
    /* test_case_name = */ "test_case_invite_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* Execute invite Test Case # 1 - valid case*/
static void test_case_invite_01(void **state) {
    execute_test(test_invite_01, state);
    return;
}

/*Test Steps for meshlink_invite Test Case # 1 - Valid case
    Test Steps:
    1. Run NUT
    2. Invite 'new' node

    Expected Result:
    Generates an invitation
*/
static bool test_invite_01(void) {
  meshlink_destroy("inviteconf");
  fprintf(stderr, "[ invite 01 ] Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  meshlink_handle_t *mesh_handle = meshlink_open("inviteconf", "nut", "node_sim", 1);
  fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle);
  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "\n[ invite 01 ]Generating INVITATION\n");
  char *invitation = meshlink_invite(mesh_handle, "new");
  if (NULL == invitation) {
    fprintf(stderr, "\n[ invite 01 ]Failed to generate INVITATION\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("inviteconf");
    return false;
  }
  fprintf(stderr, "\n[ invite 01 ]Generated INVITATION successfully\t %s \n", invitation);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("inviteconf");
  return true;
}


/* Execute invite Test Case # 2 - Invalid case*/
static void test_case_invite_02(void **state) {
    execute_test(test_invite_02, state);
    return;
}

/*Test Steps for meshlink_invite Test Case # 2 - Invalid case
    Test Steps:
    1. Calling meshlink_invite API with NULL as mesh handle argument

    Expected Result:
    Reports appropriate error by returning NULL
*/
static bool test_invite_02(void) {
    fprintf(stderr, "\n[ invite 02 ]Trying to generate INVITATION by passing NULL as mesh link handle\n");
    char *invitation = meshlink_invite(NULL, "nut");
    if (NULL == invitation && MESHLINK_EINVAL == meshlink_errno) {
      fprintf(stderr, "[ invite 02 ]invite API reported error SUCCESSFULLY\n");
      return true;
    }
      fprintf(stderr, "[ invite 02 ]Failed to report error\n");

    return false;
}

/* Execute invite Test Case # 3 - Invalid case*/
static void test_case_invite_03(void **state) {
    execute_test(test_invite_03, state);
    return;
}

/*Test Steps for meshlink_invite Test Case # 3 - Invalid case
    Test Steps:
    1. Run NUT
    2. Call meshlink_invite with NULL node name argument

    Expected Result:
    Reports appropriate error by returning NULL
*/
static bool test_invite_03(void) {
  meshlink_destroy("inviteconf");
  fprintf(stderr, "[ invite 03 ] Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  meshlink_handle_t *mesh_handle = meshlink_open("inviteconf", "nut", "node_sim", 1);
  fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle);
  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "[ invite 03 ]Trying to generate INVITATION by passing NULL as mesh link handle\n");
  char *invitation = meshlink_invite(mesh_handle, NULL);
  if (NULL == invitation) {
    fprintf(stderr, "[ invite 03 ]invite API reported error SUCCESSFULLY\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("inviteconf");
    return true;
  }

  fprintf(stderr, "[ invite 03 ]Failed to report error\n");
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("inviteconf");
  return false;
}

/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/
 int test_meshlink_invite(void) {
   const struct CMUnitTest blackbox_invite_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_invite_01, NULL, NULL,
            (void *)&test_case_invite_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_invite_02, NULL, NULL,
            (void *)&test_case_invite_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_invite_03, NULL, NULL,
            (void *)&test_case_invite_03_state)
   };

   total_tests += sizeof(blackbox_invite_tests) / sizeof(blackbox_invite_tests[0]);

   return cmocka_run_group_tests(blackbox_invite_tests ,NULL , NULL);
 }
