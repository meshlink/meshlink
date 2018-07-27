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
#include "test_cases_discovery.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"

/* Execute Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third node */
void test_case_discovery_01(void **state) {
    execute_test(test_steps_discovery_01, state);
    return;
}

/* Test Steps for Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third (relay) node

    Test Steps:
    1. Run NUT, relay and peer nodes with relay inviting the other two nodes
    2. After connection to peer, terminate the peer node's running instance
    3. After peer becomes unreachable, wait 60 seconds then re-start the peer node's instance

    Expected Result:
    NUT is re-connected to peer
*/
bool test_steps_discovery_01(void) {
    char *invite_peer, *invite_nut;
    bool result = false;
    int i;

    invite_peer = invite_in_container("relay", "peer");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    printf("Please turn your interface internet down in 10 seconds");
    sleep(10);

     meshlink_enable_discovery(mesh_handle, true);
    node_sim_in_container("relay", "1", NULL);
    node_sim_in_container("peer", "1", invite_peer);
    PRINT_TEST_CASE_MSG("Waiting for peer to be connected when discovery being enabled\n");

    for(i = 0; i < 60; i++) {
        if(meta_conn_status[1]) {
            result = true;
            break;
        }
        sleep(1);
    }

    free(invite_peer);
    free(invite_nut);

    return result;
}

/* Execute Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third node */
void test_case_discovery_02(void **state) {
    execute_test(test_steps_discovery_02, state);
    return;
}

/* Test Steps for Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third (relay) node

    Test Steps:
    1. Run NUT, relay and peer nodes with relay inviting the other two nodes
    2. After connection to peer, terminate the peer node's running instance
    3. After peer becomes unreachable, wait 60 seconds then re-start the peer node's instance

    Expected Result:
    NUT is re-connected to peer
*/
bool test_steps_discovery_02(void) {
    char *invite_peer, *invite_nut;
    bool result = false;
    int i;

    invite_peer = invite_in_container("relay", "peer");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    printf("Please turn your interface internet down in 10 seconds");
    sleep(10);

     meshlink_enable_discovery(mesh_handle, false);
    node_sim_in_container("relay", "1", NULL);
    node_sim_in_container("peer", "1", invite_peer);
    PRINT_TEST_CASE_MSG("Waiting for peer to be connected when discovery being disabled\n");

    for(i = 0; i < 60; i++) {
        if(meta_conn_status[1]) {
            result = true;
            break;
        }
        sleep(1);
    }

    free(invite_peer);
    free(invite_nut);

    return !result;
}

/* Execute Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third node */
void test_case_discovery_03(void **state) {
    execute_test(test_steps_discovery_03, state);
    return;
}

/* Test Steps for Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third (relay) node

    Test Steps:
    1. Run NUT, relay and peer nodes with relay inviting the other two nodes
    2. After connection to peer, terminate the peer node's running instance
    3. After peer becomes unreachable, wait 60 seconds then re-start the peer node's instance

    Expected Result:
    NUT is re-connected to peer
*/
bool test_steps_discovery_03(void) {
    fprintf(stderr, "[test_steps_discovery_03] Passing NULL: as meshlink_enable_discovery mesh argument\n");
     meshlink_enable_discovery(NULL, true);
     if (meshlink_errno == MESHLINK_EINVAL)
        return true;
    else
        return false;
}
