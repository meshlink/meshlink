/*
    test_cases_hint_address.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_hint_address.h"
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>


/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

/* Port number used in the structure */
#define PORT 8000

/* hint address used in the socket structure */
#define ADDR "10.1.1.1"

static void test_case_hint_address_01(void **state);
static bool test_steps_hint_address_01(void);

static black_box_state_t test_case_hint_address_01_state = {
    .test_case_name = "test_case_hint_address_01",
};


/* Execute meshlink_hint_address Test Case # 1 - Valid Case*/
void test_case_hint_address_01(void **state) {
    execute_test(test_steps_hint_address_01, state);
    return;
}
/* Test Steps for meshlink_hint_address Test Case # 1 - Valid case */
bool test_steps_hint_address_01(void) {
  meshlink_destroy("hintconf1");
  meshlink_destroy("hintconf2");
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  // Create meshlink instance for the nodes
  meshlink_handle_t *mesh1 = meshlink_open("hintconf1", "nut", "test", DEV_CLASS_STATIONARY);
  assert(mesh1);
  meshlink_handle_t *mesh2 = meshlink_open("hintconf2", "bar", "test", DEV_CLASS_STATIONARY);
  assert(mesh2);
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  // importing and exporting mesh meta data
  char *exp1 = meshlink_export(mesh1);
  assert(exp1 != NULL);
  char *exp2 = meshlink_export(mesh2);
  assert(exp2 != NULL);
  assert(meshlink_import(mesh1, exp2));
  assert(meshlink_import(mesh2, exp1));
  free(exp1);
  free(exp2);

  // Nodes should learn about each other
  sleep(1);

  // Start the nodes
  assert(meshlink_start(mesh1));
  assert(meshlink_start(mesh2));

  // socket structure to be hinted
  struct sockaddr_in hint;
  hint.sin_family        = AF_INET;
  hint.sin_port          = htons(PORT);
  assert(inet_aton(ADDR, &hint.sin_addr));

  // Getting node handle for the NUT itself
  meshlink_node_t *node = meshlink_get_node(mesh1, "bar");
  assert(node != NULL);

  meshlink_hint_address(mesh_handle, node, (struct sockaddr * )&hint);

  int fp;
  fp = open("./hintconf1/hosts/bar", O_RDONLY);
  assert(fp >= 0);
  off_t fsize = lseek(fp, 0, SEEK_END);
  assert(fsize >= 0);
  char *buff = (char *) calloc(1, fsize + 1);
  assert(buff != NULL);
  assert(lseek(fp, 0, SEEK_SET) == 0);
  assert(read(fp, buff, fsize) >=0 );
  buff[fsize] = '\0';
  assert(close(fp) != -1);

  assert_int_not_equal(strstr(buff, ADDR), NULL);

  meshlink_close(mesh1);
  meshlink_close(mesh2);
  meshlink_destroy("hintconf1");
  meshlink_destroy("hintconf2");

  return true;
}


int test_meshlink_hint_address(void) {
  const struct CMUnitTest blackbox_hint_address_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_hint_address_01, NULL, NULL,
            (void *)&test_case_hint_address_01_state)
  };

  total_tests += sizeof(blackbox_hint_address_tests) / sizeof(blackbox_hint_address_tests[0]);

  return cmocka_run_group_tests(blackbox_hint_address_tests ,NULL , NULL);
}
