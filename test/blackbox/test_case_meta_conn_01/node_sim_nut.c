/*
    node_sim_nut.c -- Implementation of Node Simulation for Meshlink Testing
                    for meta connection test case 01 - re-connection of
                    two nodes when relay node goes down
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
#include <stdio.h>
#include <stdlib.h>
#include "../common/common_handlers.h"
#include "../common/test_step.h"
#include "../common/mesh_event_handler.h"

#define CMD_LINE_ARG_NODENAME   1
#define CMD_LINE_ARG_DEVCLASS   2
#define CMD_LINE_ARG_CLIENTID   3
#define CMD_LINE_ARG_IMPORTSTR  4
#define CMD_LINE_ARG_INVITEURL  5

static bool peer_reachable = false;

static void node_status_callback(meshlink_handle_t *mesh, meshlink_node_t *node,
                                        bool reachable) {
    int i;

    fprintf(stderr, "Node %s became %s\n", node->name, (reachable) ? "reachable" : "unreachable");
    if(!strcasecmp(node->name, "peer")) {
      if(!reachable) {
        peer_reachable = false;
      }
      else {
        peer_reachable = true;
      }
    }

    return;
}

int main(int argc, char *argv[]) {
    int clientId;
    bool result = false;
    int i;

    if((argv[CMD_LINE_ARG_CLIENTID]) && (argv[CMD_LINE_ARG_IMPORTSTR] )) {
      clientId = atoi(argv[CMD_LINE_ARG_CLIENTID]);
      mesh_event_sock_connect(argv[CMD_LINE_ARG_IMPORTSTR]);
    }

    execute_open(argv[CMD_LINE_ARG_NODENAME], argv[CMD_LINE_ARG_DEVCLASS]);
    meshlink_set_node_status_cb(mesh_handle, node_status_callback);
    if(argv[CMD_LINE_ARG_INVITEURL]) {
        execute_join(argv[CMD_LINE_ARG_INVITEURL]);
    }
    execute_start();
    mesh_event_sock_send(clientId, NODE_STARTED, NULL, 0);

    /* Connectivity of peer is checked using meshlink_get_node API */
    meshlink_node_t *node = NULL;
    while((node = meshlink_get_node(mesh_handle, "peer")) == NULL) {
      sleep(1);
    }
    fprintf(stderr, "Connected with Peer\n");
    mesh_event_sock_send(clientId, META_CONN_SUCCESSFUL, "Connected with Peer", 30);

    /* peer_reachable reads true/false that has been set by node_status_callback */
    while(peer_reachable) {
      sleep(1);
    }
    fprintf(stderr, "Peer node become unreachable\n");
    mesh_event_sock_send(clientId, NODE_UNREACHABLE, "Peer node become unreachable", 30);

    /* Waiting 60 sec before re-starting the peer node */
    fprintf(stderr, "Waiting 60 sec before re-starting the peer node\n");
    sleep(60);

    fprintf(stderr, "Waiting for peer to be re-connected\n");
    for(i = 0; i < 60; i++) {
      if((node = meshlink_get_node(mesh_handle, "peer")) != NULL) {
        result = true;
        break;
      }
      sleep(1);
    }
    if(result) {
      fprintf(stderr, "Re-connected with Peer\n");
      mesh_event_sock_send(clientId, META_RECONN_SUCCESSFUL, "Peer", 30);
    } else {
      fprintf(stderr, "Failed to reconnect with Peer\n");
      mesh_event_sock_send(clientId, META_RECONN_FAILURE, "Peer", 30);
    }

    execute_close();

    return 0;
}
