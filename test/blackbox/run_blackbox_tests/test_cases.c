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
#include "test_cases.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"

/* Execute Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third node */
void test_case_meta_conn_01(void **state) {
    execute_test(test_steps_meta_conn_01, state);
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
bool test_steps_meta_conn_01(void) {
    char *invite_peer, *invite_nut;
    bool result = false;
    int i;

    invite_peer = invite_in_container("relay", "peer");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    node_sim_in_container("relay", "1", NULL);
    node_sim_in_container("peer", "1", invite_peer);
    execute_open(NUT_NODE_NAME, "1");
    execute_join(invite_nut);
    execute_start();
    PRINT_TEST_CASE_MSG("Waiting for peer to be connected\n");
    /* TO DO: Implement a function to perform this watch operation on a variable with a timeout
        and return true or false to indicate whether the variable reached its target value
        before the timeout */
    while(!meta_conn_status[1])
        sleep(1);
    node_step_in_container("peer", "SIGTERM");
    PRINT_TEST_CASE_MSG("Waiting for peer to become unreachable\n");
    /* TO DO: Implement this with a timeout */
    while(node_reachable_status[1])
        sleep(1);
    PRINT_TEST_CASE_MSG("Waiting 60 sec before re-starting the peer node\n");
    sleep(60);
    node_sim_in_container("peer", "1", NULL);
    PRINT_TEST_CASE_MSG("Waiting 60 sec for peer to be re-connected\n");
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



/* Execute Meta-connections Test Case # 2 - re-connection to peer via third node
    after changing IP of NUT and peer */
void test_case_meta_conn_02(void **state) {
    execute_test(test_steps_meta_conn_02, state);
    return;
}

/* Test Steps for Meta-connections Test Case # 2 - re-connection to peer via third node
    after changing IP of NUT and peer

    Test Steps:
    1. Run NUT, relay and peer nodes with relay inviting the other two nodes
    2. After connection to peer, change the NUT's IP Address and the peer node's IP Address

    Expected Result:
    NUT is first disconnected from peer then automatically re-connected to peer
*/
bool test_steps_meta_conn_02(void) {
    bool result = false;
    char *invite_peer, *invite_nut;
    int i;

    invite_peer = invite_in_container("relay", "peer");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    node_sim_in_container("relay", "1", NULL);
    node_sim_in_container("peer", "1", invite_peer);
    execute_open(NUT_NODE_NAME, "1");
    execute_join(invite_nut);
    execute_start();
    PRINT_TEST_CASE_MSG("Waiting for peer to be connected\n");
    /* TO DO: Implement this with a timeout */
    while(!meta_conn_status[1])
        sleep(1);
    execute_change_ip();
    //restart_all_containers();
    //node_sim_in_container("relay", "1", NULL);
    change_ip(1);
    node_sim_in_container("peer", "1", NULL);
    PRINT_TEST_CASE_MSG("Waiting 120 sec for peer to be re-connected\n");
    meta_conn_status[1] = false;
    for(i = 0; i < 120; i++) {
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




/* Execute Meta-connections Test Case # 3 - re-connection to peer via third node
    after changing IP of peer */
void test_case_meta_conn_03(void **state) {
    execute_test(test_steps_meta_conn_03, state);
    return;
}

/* Test Steps for Meta-connections Test Case # 3 - re-connection to peer via third node
    after changing IP of peer

    Test Steps:
    1. Run NUT, relay and peer nodes with relay inviting the other two nodes
    2. After connection to peer, change the peer node's IP Address

    Expected Result:
    NUT is first disconnected from peer then automatically re-connected to peer
*/
bool test_steps_meta_conn_03(void) {
    bool result = false;
    char *invite_peer, *invite_nut;
    int i;

    invite_peer = invite_in_container("relay", "peer");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    node_sim_in_container("relay", "1", NULL);
    node_sim_in_container("peer", "1", invite_peer);
    execute_open(NUT_NODE_NAME, "1");
    execute_join(invite_nut);
    execute_start();
    PRINT_TEST_CASE_MSG("Waiting for peer to be connected\n");
    /* TO DO: Implement this with a timeout */
    while(!meta_conn_status[1])
        sleep(1);
    change_ip(1);
    node_sim_in_container("peer", "1", NULL);
    PRINT_TEST_CASE_MSG("Waiting 120 sec for peer to be re-connected\n");
    for(i = 0; i < 120; i++) {
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




/* Execute Meta-connections Test Case # 4 - re-connection to peer after changing IP of
    NUT and peer */
void test_case_meta_conn_04(void **state) {
    execute_test(test_steps_meta_conn_04, state);
    return;
}

/* Execute Meta-connections Test Case # 4 - re-connection to peer after changing IP of
    NUT and peer

    Test Steps:
    1. Run NUT and peer nodes with NUT inviting the peer node
    2. After connection to peer, change the NUT's IP Address and the peer node's IP Address

    Expected Result:
    NUT is first disconnected from peer then automatically re-connected to peer
*/
bool test_steps_meta_conn_04(void) {
    bool result = false;
    char *invite_peer;
    int i;

    execute_open(NUT_NODE_NAME, "1");
    execute_start();
    invite_peer = execute_invite("peer");
    node_sim_in_container("peer", "1", invite_peer);
    PRINT_TEST_CASE_MSG("Waiting for peer to be connected\n");
    /* TO DO: Implement this with a timeout */
    while(!meta_conn_status[0])
        sleep(1);
    execute_change_ip();
    restart_all_containers();
    change_ip(0);
    node_sim_in_container("peer", "1", NULL);
    PRINT_TEST_CASE_MSG("Waiting 120 sec for peer to be re-connected\n");
    meta_conn_status[0] = false;
    for(i = 0; i < 120; i++) {
        if(meta_conn_status[0]) {
            result = true;
            break;
        }
        sleep(1);
    }

    free(invite_peer);

    return result;
}




/* Execute Meta-connections Test Case # 5 - re-connection to peer after changing IP of peer */
void test_case_meta_conn_05(void **state) {
    execute_test(test_steps_meta_conn_05, state);
    return;
}

/* Execute Meta-connections Test Case # 5 - re-connection to peer after changing IP of peer

    Test Steps:
    1. Run NUT and peer nodes with NUT inviting the peer node
    2. After connection to peer, change the peer node's IP Address

    Expected Result:
    NUT is first disconnected from peer then automatically re-connected to peer
*/
bool test_steps_meta_conn_05(void) {
    bool result = false;
    char *invite_peer;
    int i;

    execute_open(NUT_NODE_NAME, "1");
    execute_start();
    invite_peer = execute_invite("peer");
    node_sim_in_container("peer", "1", invite_peer);
    PRINT_TEST_CASE_MSG("Waiting for peer to be connected\n");
    /* TO DO: Implement this with a timeout */
    while(!meta_conn_status[0])
        sleep(1);
    change_ip(0);
    node_sim_in_container("peer", "1", NULL);
    PRINT_TEST_CASE_MSG("Waiting 120 sec for peer to be re-connected\n");
    for(i = 0; i < 120; i++) {
        if(meta_conn_status[0]) {
            result = true;
            break;
        }
        sleep(1);
    }

    free(invite_peer);

    return result;
}
