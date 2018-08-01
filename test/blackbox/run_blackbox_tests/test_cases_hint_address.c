/*
    test_cases_add_ex_addr.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>
                        Manav Kumar Mehta <manavkumarm@yahoo.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty o
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "execute_tests.h"
#include "test_cases_channel_get_flags.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

/* Execute meshlink_hint_address Test Case # 1 */
void test_case_hint_address_01(void **state) {
    execute_test(test_steps_hint_address_01, state);
    return;
}

/* Test Steps for meshlink_hint_address Test Case # 1*/
bool test_steps_hint_address_01(void) {
    meshlink_destroy("hintconf");
    fprintf(stderr, "[ hint 01 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("hintconf", "nut", "node_sim", 1);
    fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    struct sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port   =

}


