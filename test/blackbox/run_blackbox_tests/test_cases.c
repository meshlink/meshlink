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
#include "pthread.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../common/mesh_event_handler.h"

#define RELAY_ID "0"
#define PEER_ID  "1"
#define NUT_ID   "2"

/* mutex for the common variable */
static pthread_mutex_t lock;

static bool meta_conn01_result;

void meta_conn01_cb(mesh_event_payload_t payload) {
  char event_node_name[][10] = {"RELAY", "PEER", "NUT"};
  printf("%s : ", event_node_name[payload.client_id]);

  pthread_mutex_lock(&lock);
  switch(payload.mesh_event) {
    case META_CONN_SUCCESSFUL   : printf("Meta Connection Successful\n");
                                  break;
    case NODE_STARTED           : printf("Node started\n");
                                  break;
    case META_RECONN_FAILURE    : printf("Failed to reconnect with");
                                  meta_conn01_result = false;
                                  break;
    case META_RECONN_SUCCESSFUL : printf("Reconnected with");
                                  meta_conn01_result = true;
                                  break;
  }
  pthread_mutex_unlock(&lock);
  if(payload.payload_length) {
    printf(" %s\n", (char *)payload.payload);
  }
  return;
}

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
    char *import;

    assert(!pthread_mutex_init(&lock, NULL));
    import = mesh_event_sock_create(eth_if_name);
    invite_peer = invite_in_container("relay", "peer");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
    wait_for_event(meta_conn01_cb, 5);
    node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
    wait_for_event(meta_conn01_cb, 5);
    node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);
    wait_for_event(meta_conn01_cb, 5);

    PRINT_TEST_CASE_MSG("Waiting for peer to be connected with NUT\n");
    if(!wait_for_event(meta_conn01_cb, 60)) {
      return false;
    }

    PRINT_TEST_CASE_MSG("Sending SIGTERM to peer\n");
    node_step_in_container("peer", "SIGTERM");
    PRINT_TEST_CASE_MSG("Waiting for peer to become unreachable\n");
    if(!wait_for_event(meta_conn01_cb, 60)) {
      return false;
    }

    PRINT_TEST_CASE_MSG("Waiting 60 sec before re-starting the peer node\n");
    sleep(60);
    node_sim_in_container_event("peer", "1", NULL, PEER_ID, import);
    wait_for_event(meta_conn01_cb, 5);
    PRINT_TEST_CASE_MSG("Waiting for peer to be re-connected\n");
    wait_for_event(meta_conn01_cb, 60);
    pthread_mutex_lock(&lock);
    result = meta_conn01_result;
    pthread_mutex_unlock(&lock);
    free(invite_peer);
    free(invite_nut);
    pthread_mutex_destroy(&lock);

    return result;
}


static bool meta_conn02_result;

void meta_conn02_cb(mesh_event_payload_t payload) {
  char event_node_name[][10] = {"RELAY", "PEER", "NUT"};
  printf("%s : ", event_node_name[payload.client_id]);

  pthread_mutex_lock(&lock);
  switch(payload.mesh_event) {
    case META_CONN_SUCCESSFUL   : printf("Meta Connection Successful\n");
                                  meta_conn02_result = true;
                                  break;
    case NODE_STARTED           : printf("Node started\n");
                                  break;
  }
  pthread_mutex_unlock(&lock);
  if(payload.payload_length) {
    printf(" %s\n", (char *)payload.payload);
  }
  return;
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
    char *invite_peer, *invite_nut;
    bool result = false;
    int i;
    char *import;

    assert(!pthread_mutex_init(&lock, NULL));
    import = mesh_event_sock_create(eth_if_name);
    invite_peer = invite_in_container("relay", "peer");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
    wait_for_event(meta_conn02_cb, 5);
    node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
    wait_for_event(meta_conn02_cb, 5);
    node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);
    wait_for_event(meta_conn02_cb, 5);

    PRINT_TEST_CASE_MSG("Waiting for peer to be connected with NUT\n");
    if(!wait_for_event(meta_conn02_cb, 60)) {
      return false;
    }

    meta_conn02_result = false;
    node_sim_in_container_event("peer", "1", NULL, PEER_ID, import);
    wait_for_event(meta_conn02_cb, 5);
    node_sim_in_container_event("nut", "1", NULL, NUT_ID, import);
    wait_for_event(meta_conn02_cb, 5);

    PRINT_TEST_CASE_MSG("Waiting for peer to be connected with NUT\n");
    if(!wait_for_event(meta_conn02_cb, 60)) {
      return false;
    }
    pthread_mutex_lock(&lock);
    result = meta_conn02_result;
    pthread_mutex_unlock(&lock);
    free(invite_peer);
    free(invite_nut);
    pthread_mutex_destroy(&lock);

    return result;
}

static bool meta_conn03_result;

void meta_conn03_cb(mesh_event_payload_t payload) {
  char event_node_name[][10] = {"RELAY", "PEER", "NUT"};
  printf("%s : ", event_node_name[payload.client_id]);

  pthread_mutex_lock(&lock);
  switch(payload.mesh_event) {
    case META_CONN_SUCCESSFUL   : printf("Meta Connection Successful\n");
                                  break;
    case NODE_STARTED           : printf("Node started\n");
                                  break;
    case META_RECONN_FAILURE    : printf("Failed to reconnect with");
                                  meta_conn03_result = false;
                                  break;
    case META_RECONN_SUCCESSFUL : printf("Reconnected with");
                                  meta_conn03_result = true;
                                  break;
  }
  pthread_mutex_unlock(&lock);
  if(payload.payload_length) {
    printf(" %s\n", (char *)payload.payload);
  }
  return;
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
    char *invite_peer, *invite_nut;
    bool result = false;
    int i;
    char *import;

    assert(!pthread_mutex_init(&lock, NULL));
    import = mesh_event_sock_create(eth_if_name);
    invite_peer = invite_in_container("relay", "peer");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    node_sim_in_container_event("relay", "1", NULL, RELAY_ID, import);
    wait_for_event(meta_conn03_cb, 5);
    node_sim_in_container_event("peer", "1", invite_peer, PEER_ID, import);
    wait_for_event(meta_conn03_cb, 5);
    node_sim_in_container_event("nut", "1", invite_nut, NUT_ID, import);
    wait_for_event(meta_conn03_cb, 5);

    PRINT_TEST_CASE_MSG("Waiting for peer to be connected with NUT\n");
    if(!wait_for_event(meta_conn03_cb, 60)) {
      return false;
    }

    PRINT_TEST_CASE_MSG("Changing IP address of PEER container\n");
    change_ip(1);
    node_sim_in_container_event("peer", "1", NULL, PEER_ID, import);
    wait_for_event(meta_conn03_cb, 5);
    PRINT_TEST_CASE_MSG("Waiting for peer to be re-connected\n");
    wait_for_event(meta_conn03_cb, 120);
    pthread_mutex_lock(&lock);
    result = meta_conn03_result;
    pthread_mutex_unlock(&lock);
    free(invite_peer);
    free(invite_nut);
    pthread_mutex_destroy(&lock);

    return result;
}

static char *invite_peer = NULL;
static bool meta_conn04 = false;

void meta_conn04_cb(mesh_event_payload_t payload) {
  char event_node_name[][10] = {"PEER", "NUT"};
  printf("%s : ", event_node_name[payload.client_id]);

  pthread_mutex_lock(&lock);
  switch(payload.mesh_event) {
    case META_CONN_SUCCESSFUL   : printf("Meta Connection Successful\n");
                                  meta_conn04 = true;
                                  break;
    case NODE_INVITATION        : printf("Invitation generated\n");
                                  invite_peer = malloc(payload.payload_length);
                                  strcpy(invite_peer, (char *)payload.payload);
                                  break;
    case NODE_STARTED           : printf("Node started\n");
                                  break;
  }
  pthread_mutex_unlock(&lock);
  return;
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
    char *import;

    import = mesh_event_sock_create(eth_if_name);
    node_sim_in_container_event("nut", "1", NULL, "1", import);
    wait_for_event(meta_conn04_cb, 5);

    PRINT_TEST_CASE_MSG("Waiting for NUT to generate invitation to PEER\n");
    wait_for_event(meta_conn04_cb, 5);
    if(!invite_peer) {
      return false;
    }
    PRINT_TEST_CASE_MSG("Running PEER node in the container\n");
    node_sim_in_container_event("peer", "1", invite_peer, "0", import);
    wait_for_event(meta_conn04_cb, 5);
    PRINT_TEST_CASE_MSG("Waiting for peer to be connected with NUT\n");
    if(!wait_for_event(meta_conn04_cb, 60)) {
      return false;
    }
    PRINT_TEST_CASE_MSG("Changing IP address of NUT container\n");
    change_ip(1);
    /* TODO: rather using invitation argument NULL and non-NULL just for
            variating execute_invite code at node_sim_nut implement other method */
    node_sim_in_container_event("nut", "1", "restart", "1", import);
    wait_for_event(meta_conn04_cb, 5);
    PRINT_TEST_CASE_MSG("Changing IP address of PEER container\n");
    change_ip(0);
    node_sim_in_container_event("peer", "1", NULL, "0", import);
    wait_for_event(meta_conn04_cb, 5);

    PRINT_TEST_CASE_MSG("Waiting 120 sec for peer to be re-connected\n");
    wait_for_event(meta_conn04_cb, 120);
    pthread_mutex_lock(&lock);
    result = meta_conn04;
    pthread_mutex_unlock(&lock);

    free(invite_peer);
    free(import);
    return result;
}

static char *invitation = NULL;

static bool meta_conn05 = false;

void meta_conn05_cb(mesh_event_payload_t payload) {
  char event_node_name[][10] = {"PEER", "NUT"};
  printf("%s : ", event_node_name[payload.client_id]);

  pthread_mutex_lock(&lock);
  switch(payload.mesh_event) {
    case META_CONN_SUCCESSFUL   : printf("Meta Connection Successful\n");
                                  meta_conn05 = true;
                                  break;
    case NODE_INVITATION        : printf("Invitation generated\n");
                                  invitation = malloc(payload.payload_length);
                                  strcpy(invitation, (char *)payload.payload);
                                  break;
    case NODE_STARTED           : printf("Node started\n");
                                  break;
  }
  pthread_mutex_unlock(&lock);
  return;
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
    char *import;

    import = mesh_event_sock_create(eth_if_name);
    node_sim_in_container_event("nut", "1", NULL, "1", import);
    wait_for_event(meta_conn05_cb, 5);

    wait_for_event(meta_conn05_cb, 5);
    if(!invitation) {
      return false;
    }
    node_sim_in_container_event("peer", "1", invitation, "0", import);
    wait_for_event(meta_conn05_cb, 5);
    PRINT_TEST_CASE_MSG("Waiting for peer to be connected with NUT\n");
    if(!wait_for_event(meta_conn05_cb, 60)) {
      return false;
    }

    PRINT_TEST_CASE_MSG("Changing IP address of PEER container\n");
    change_ip(0);
    meta_conn05 = false;
    node_sim_in_container_event("peer", "1", NULL, "0", import);
    wait_for_event(meta_conn05_cb, 5);
    PRINT_TEST_CASE_MSG("Waiting 120 sec for peer to be re-connected\n");
    wait_for_event(meta_conn05_cb, 120);
    pthread_mutex_lock(&lock);
    result = meta_conn05;
    pthread_mutex_unlock(&lock);

    free(invitation);
    free(import);
    return result;
}
