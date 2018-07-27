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
#include "test_cases_set_log_cb.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static bool log;
static void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level,
                                      const char *text) {
    int i;
    char connection_match_msg[100];

    log = true;

    fprintf(stderr, "meshlink>> %s\n", text);

    if(state_ptr && (strstr(text, "Connection") || strstr(text, "connection")))
        for(i = 0; i < state_ptr->num_nodes; i++) {
            assert(snprintf(connection_match_msg, sizeof(connection_match_msg),
                "Connection with %s", state_ptr->node_names[i]) >= 0);
            if(strstr(text, connection_match_msg) && strstr(text, "activated")) {
                meta_conn_status[i] = true;
                continue;
            }

            assert(snprintf(connection_match_msg, sizeof(connection_match_msg),
                "Already connected to %s", state_ptr->node_names[i]) >= 0);
            if(strstr(text, connection_match_msg)) {
                meta_conn_status[i] = true;
                continue;
            }

            assert(snprintf(connection_match_msg, sizeof(connection_match_msg),
                "Connection closed by %s", state_ptr->node_names[i]) >= 0);
            if(strstr(text, connection_match_msg)) {
                meta_conn_status[i] = false;
                continue;
            }

            assert(snprintf(connection_match_msg, sizeof(connection_match_msg),
                "Closing connection with %s", state_ptr->node_names[i]) >= 0);
            if(strstr(text, connection_match_msg)) {
                meta_conn_status[i] = false;
                continue;
            }
        }

    return;
}


/* Execute status callback Test Case # 1 - valid case */
void test_case_set_log_cb_01(void **state) {
    execute_test(test_set_log_cb_01, state);
    return;
}

bool test_set_log_cb_01(void) {
    char *invite_peer, *invite_nut;
    log = false;
    int i;

    meshlink_destroy("testconf");
    invite_peer = invite_in_container("relay", "peer");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    node_sim_in_container("relay", "1", NULL);
    node_sim_in_container("peer", "1", invite_peer);
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, log_cb);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("testconf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, log_cb);
    execute_join(invite_nut);
    execute_start();
    PRINT_TEST_CASE_MSG("Waiting 60 sec for peer to be re-connected\n");
    for(i = 0; i < 60; i++) {
        if(meta_conn_status[1]) {
            break;
        }
        sleep(1);
    }

    free(invite_peer);
    free(invite_nut);

    if (log) {
    PRINT_TEST_CASE_MSG("Log call back invoked at least more than once\n");
    }
    else {
    PRINT_TEST_CASE_MSG("Log call back not invoked at least once\n");
    }
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    return log;
}

/* Execute status callback Test Case # 2 - Invalid case */
void test_case_set_log_cb_02(void **state) {
    execute_test(test_set_log_cb_02, state);
    return;
}

bool test_set_log_cb_02(void) {
    char *invite_peer, *invite_nut;
    log = false;
    int i;
    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, NULL);
    meshlink_destroy("testconf");
    invite_peer = invite_in_container("relay", "peer");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    node_sim_in_container("relay", "1", NULL);
    node_sim_in_container("peer", "1", invite_peer);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("testconf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, NULL);
    execute_join(invite_nut);
    execute_start();
    PRINT_TEST_CASE_MSG("Waiting 60 sec for peer to be re-connected\n");
    for(i = 0; i < 60; i++) {
        if(meta_conn_status[1]) {
            break;
        }
        sleep(1);
    }


    free(invite_peer);
    free(invite_nut);

    if (log) {
    PRINT_TEST_CASE_MSG("Log call back invoked at least more than once\n");
    }
    else {
    PRINT_TEST_CASE_MSG("Log call back not invoked at least once\n");
    }

    return !log;
}

/* Execute log callback Test Case # 3 - setting callback after starting mesh */
void test_case_set_log_cb_03(void **state) {
    execute_test(test_set_log_cb_03, state);
    return;
}

bool test_set_log_cb_03(void) {
    /*Setting an invalid level*/
    meshlink_set_log_cb(NULL, 1000, NULL);
    if (meshlink_errno != MESHLINK_EINVAL) {
    PRINT_TEST_CASE_MSG("No proper reporting for invalid level\n");
    return false;
    }
    else {
    PRINT_TEST_CASE_MSG("No proper reporting for invalid level\n");
    return true;
    }
}
