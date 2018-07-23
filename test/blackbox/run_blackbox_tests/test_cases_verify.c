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
#include "test_cases_verify.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>



/* Execute verify_data Test Case # 1 - Valid case - verify a data successfully*/
void test_case_verify_01(void **state) {
    execute_test(test_verify_01, state);
    return;
}

bool test_verify_01(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("verify01conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    fprintf(stderr, "[ verify 01 ]Running execute_sign\n");
    bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
    assert(ret);
    if (!ret) {
    fprintf(stderr, "[ verify 01 ]meshlink_verify FAILED to sign data\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
   }
    fprintf(stderr, "[ verify 01 ]meshlink_sign Successfuly signed data\n");

    fprintf(stderr, "[ verify 01 ]get nut node_handle\n");
    meshlink_node_t *source = meshlink_get_node(mesh_handle, "nut");

    fprintf(stderr, "[ verify 01 ]Running execute_verify\n");
   ret = meshlink_verify(mesh_handle, source, data, strlen(data) + 1, sig, ssize);

    if (!ret) {
    fprintf(stderr, "[ verify 01 ]meshlink_verify FAILED to verify data\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
   }
    fprintf(stderr, "[ verify 01 ]meshlink_verify Successfuly verified data\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return true;
}


/* Execute verify_data Test Case # 2 - Invalid case - meshlink_verify passing NULL args*/
void test_case_verify_02(void **state) {
    execute_test(test_verify_02, state);
    return;
}

bool test_verify_02(void) {
    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("verify02conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    meshlink_node_t *source = meshlink_get_self(mesh_handle);
    assert(source);

    fprintf(stderr, "[ sign 02 ]Running execute_sign\n");
    bool ret = meshlink_verify(NULL, source, data, strlen(data) + 1, sig, ssize);
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


/* Execute verify_data Test Case # 3 - Invalid case - meshlink_verify passing NULL args*/
void test_case_verify_03(void **state) {
    execute_test(test_verify_03, state);
    return;
}

bool test_verify_03(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("verify03conf", "nut", "node_sim", 1);
    assert(mesh_handle);
    meshlink_set_log_cb(mesh_handle, MESHLINK_DEBUG, meshlink_callback_logger);


    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    fprintf(stderr, "[ verify 03 ]Running execute_sign\n");
    bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
    assert(ret);
    if (!ret) {
    fprintf(stderr, "[ verify 03 ]meshlink_verify FAILED to sign data\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
   }
    fprintf(stderr, "[ verify 03 ]meshlink_sign Successfuly signed data\n");

    fprintf(stderr, "[ verify 03 ]get nut node_handle\n");
    meshlink_node_t *source = meshlink_get_self(mesh_handle);

    fprintf(stderr, "[ verify 03 ]Running execute_verify\n");
   ret = meshlink_verify(mesh_handle, NULL, data, strlen(data) + 1, sig, ssize);

    if (!ret) {
    fprintf(stderr, "[ verify 03 ]meshlink_verify successfully reported NULL as node_handle arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return true;
   }
    fprintf(stderr, "[ verify 03 ]meshlink_verify FAILED to report NULL as node_handle arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
}

/* Execute verify_data Test Case # 4 - Invalid case - meshlink_verify passing NULL args*/
void test_case_verify_04(void **state) {
    execute_test(test_verify_04, state);
    return;
}

bool test_verify_04(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("verify04conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    fprintf(stderr, "[ verify 04 ]Running execute_sign\n");
    bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
    if (!ret) {
    fprintf(stderr, "[ verify 04 ]meshlink_verify FAILED to sign data\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
   }
    fprintf(stderr, "[ verify 04 ]meshlink_sign Successfuly signed data\n");

    fprintf(stderr, "[ verify 04 ]get nut node_handle\n");
    meshlink_node_t *source = meshlink_get_self(mesh_handle);

    fprintf(stderr, "[ verify 04 ]Running execute_verify\n");
   ret = meshlink_verify(mesh_handle, source, NULL, strlen(data) + 1, sig, ssize);

    if (!ret) {
    fprintf(stderr, "[ verify 04 ]meshlink_verify successfully reported NULL as data arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return true;
   }
    fprintf(stderr, "[ verify 04 ]meshlink_verify FAILED to report NULL as data arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
}

/* Execute verify_data Test Case # 5 - Invalid case - meshlink_verify passing NULL args*/
void test_case_verify_05(void **state) {
    execute_test(test_verify_05, state);
    return;
}

bool test_verify_05(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("verify05conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    fprintf(stderr, "[ verify 05 ]Running execute_sign\n");
    bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
    if (!ret) {
    fprintf(stderr, "[ verify 05 ]meshlink_verify FAILED to sign data\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
   }
    fprintf(stderr, "[ verify 05 ]meshlink_sign Successfuly signed data\n");

    fprintf(stderr, "[ verify 05 ]get nut node_handle\n");
    meshlink_node_t *source = meshlink_get_self(mesh_handle);

    fprintf(stderr, "[ verify 05 ]Running execute_verify\n");
   ret = meshlink_verify(mesh_handle, source, data, strlen(data) + 1, NULL, ssize);

    if (!ret) {
    fprintf(stderr, "[ verify 05 ]meshlink_verify successfully reported 0 as data size arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return true;
   }
    fprintf(stderr, "[ verify 05 ]meshlink_verify FAILED to report 0 as data size arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
}

/* Execute verify_data Test Case # 6 - Invalid case - meshlink_verify passing NULL args*/
void test_case_verify_06(void **state) {
    execute_test(test_verify_06, state);
    return;
}

bool test_verify_06(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("verify06conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    fprintf(stderr, "[ verify 06 ]Running execute_sign\n");
    bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
    assert(ret);
    if (!ret) {
    fprintf(stderr, "[ verify 06 ]meshlink_verify FAILED to sign data\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
   }
    fprintf(stderr, "[ verify 06 ]meshlink_sign Successfuly signed data\n");

    fprintf(stderr, "[ verify 06 ]get nut node_handle\n");
    meshlink_node_t *source = meshlink_get_self(mesh_handle);

    fprintf(stderr, "[ verify 06 ]Running execute_verify\n");
   ret = meshlink_verify(mesh_handle, source, data, strlen(data) + 1, NULL, ssize);

    if (!ret) {
    fprintf(stderr, "[ verify 06 ]meshlink_verify successfully NULL as sign arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return true;
   }
    fprintf(stderr, "[ verify 06 ]meshlink_verify FAILED to report NULL as sign arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
}

/* Execute verify_data Test Case # 7 - Invalid case - meshlink_verify passing NULL args*/
void test_case_verify_07(void **state) {
    execute_test(test_verify_07, state);
    return;
}

bool test_verify_07(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("verify07conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    char *data = "Test";
    char sig[MESHLINK_SIGLEN];
    size_t ssize = MESHLINK_SIGLEN;

    fprintf(stderr, "[ verify 07 ]Running execute_sign\n");
    bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
    if (!ret) {
    fprintf(stderr, "[ verify 07 ]meshlink_verify FAILED to sign data\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
   }
    fprintf(stderr, "[ verify 07 ]meshlink_sign Successfuly signed data\n");

    fprintf(stderr, "[ verify 07 ]get nut node_handle\n");
    meshlink_node_t *source = meshlink_get_self(mesh_handle);

    fprintf(stderr, "[ verify 07 ]Running execute_verify\n");
   ret = meshlink_verify(mesh_handle, source, data, strlen(data) + 1, sig, 0);

    if (!ret) {
    fprintf(stderr, "[ verify 07 ]meshlink_verify successfully reported 0 as sign size arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return true;
   }
    fprintf(stderr, "[ verify 07 ]meshlink_verify FAILED to report 0 as sign size arg\n");
      meshlink_stop(mesh_handle);
      meshlink_close(mesh_handle);
      return false;
}

