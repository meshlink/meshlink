/*
    test_cases.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>
                        Manav Kumar Mehta <manavkumarm@yahoo.com>

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
#include "test_cases_sign.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

/* Execute sign_data Test Case # 1 - Valid case - sign a data successfully*/
void test_case_sign_01(void **state) {
    execute_test(test_sign_01, state);
    return;
}

bool test_sign_01(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("sign01conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    fprintf(stderr, "[ sign 01 ]Running execute_sign\n");
    bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
    if (!ret) {
    fprintf(stderr, "[ sign 01 ]meshlink_sign FAILED to sign data\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
      return false;
   }
    fprintf(stderr, "[ sign 01 ]meshlink_sign Successfuly signed data\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
      return true;
}

/* Execute sign_data Test Case # 2 - Invalid case - meshlink_sign passing NULL args*/
void test_case_sign_02(void **state) {
    execute_test(test_sign_02, state);
    return;
}

bool test_sign_02(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("sign02conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    fprintf(stderr, "[ sign 02 ]Running execute_sign\n");
    bool ret = meshlink_sign(NULL, data, strlen(data) + 1, sig, &ssize);
    if (!ret) {
    fprintf(stderr, "[ sign 02 ]meshlink_sign Successfuly reported error on passing NULL as mesh_handle arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return true;
   }
    fprintf(stderr, "[ sign 02 ]meshlink_sign FAILED to report error on passing NULL as mesh_handle arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
}

/* Execute sign_data Test Case # 3 - Invalid case - meshlink_sign passing NULL args*/
void test_case_sign_03(void **state) {
    execute_test(test_sign_03, state);
    return;
}

bool test_sign_03(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("sign03conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    fprintf(stderr, "[ sign 03 ]Running execute_sign\n");
    bool ret = meshlink_sign(mesh_handle, NULL, strlen(data) + 1, sig, &ssize);
    if (!ret) {
    fprintf(stderr, "[ sign 03 ]meshlink_sign Successfuly reported error on passing NULL as data arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return true;
   }
    fprintf(stderr, "[ sign 03 ]meshlink_sign FAILED to report error on passing NULL as data arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
}

/* Execute sign_data Test Case # 4 - Invalid case - meshlink_sign passing NULL args*/
void test_case_sign_04(void **state) {
    execute_test(test_sign_04, state);
    return;
}

bool test_sign_04(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("sign04conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    fprintf(stderr, "[ sign 04 ]Running execute_sign\n");
    bool ret = meshlink_sign(mesh_handle, data, 0, sig, &ssize);
    if (!ret) {
    fprintf(stderr, "[ sign 04 ]meshlink_sign Successfuly reported error on passing 0 as size of data arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return true;
   }
    fprintf(stderr, "[ sign 04 ]meshlink_sign FAILED to report error on passing 0 as size of data arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
}

/* Execute sign_data Test Case # 5 - Invalid case - meshlink_sign passing NULL args*/
void test_case_sign_05(void **state) {
    execute_test(test_sign_05, state);
    return;
}

bool test_sign_05(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("sign05conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    fprintf(stderr, "[ sign 05 ]Running execute_sign\n");
    bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, NULL, &ssize);
    if (!ret) {
    fprintf(stderr, "[ sign 05 ]meshlink_sign Successfuly reported error on passing NULL as sign arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return true;
   }
    fprintf(stderr, "[ sign 05 ]meshlink_sign FAILED to report error on passing NULL as sign arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
}

/* Execute sign_data Test Case # 6 - Invalid case - meshlink_sign passing NULL args*/
void test_case_sign_06(void **state) {
    execute_test(test_sign_06, state);
    return;
}

bool test_sign_06(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("sign06conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    fprintf(stderr, "[ sign 06 ]Running execute_sign\n");
    bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, NULL);
    if (!ret) {
    fprintf(stderr, "[ sign 06 ]meshlink_sign Successfuly reported error on passing NULL as signsize arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return true;
   }
    fprintf(stderr, "[ sign 06 ]meshlink_sign FAILED to report error on passing NULL as signsize arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
}

/* Execute sign_data Test Case # 7 - Invalid case - meshlink_sign passing size of signature < MESHLINK_SIGLEN*/
void test_case_sign_07(void **state) {
    execute_test(test_sign_07, state);
    return;
}

bool test_sign_07(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("sign07conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = 5;  //5 < MESHLINK_SIGLEN

    fprintf(stderr, "[ sign 07 ]Running execute_sign\n");
    bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
    if (!ret) {
    fprintf(stderr, "[ sign 07 ]meshlink_sign Successfuly reported error on passing signsize < MESHLINK_SIGLEN arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return true;
   }
    fprintf(stderr, "[ sign 07 ]meshlink_sign FAILED to report error on passing signsize < MESHLINK_SIGLEN arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
}

