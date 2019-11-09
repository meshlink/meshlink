/*
    node_sim.c -- Implementation of Node Simulation for Meshlink Testing
                    for meta connection test case 01 - re-connection of
                    two nodes when relay node goes down
    Copyright (C) 2018  Guus Sliepen <guus@meshlink.io>

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
#include <string.h>
#include <pthread.h>
#include "../common/common_handlers.h"
#include "../common/test_step.h"
#include "../common/mesh_event_handler.h"

#define CMD_LINE_ARG_NODENAME   1
#define CMD_LINE_ARG_DEVCLASS   2
#define CMD_LINE_ARG_CLIENTID   3
#define CMD_LINE_ARG_IMPORTSTR  4
#define CMD_LINE_ARG_INVITEURL  5

static bool conn_status = false;

void callback_logger(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	(void)mesh;
	(void)level;

	char connection_match_msg[100];

	fprintf(stderr, "meshlink>> %s\n", text);

	if(strstr(text, "Connection") || strstr(text, "connection")) {
		assert(snprintf(connection_match_msg, sizeof(connection_match_msg),
		                "Connection with peer") >= 0);

		if(strstr(text, connection_match_msg) && strstr(text, "activated")) {
			conn_status = true;
			return;
		}

		assert(snprintf(connection_match_msg, sizeof(connection_match_msg),
		                "Already connected to peer") >= 0);

		if(strstr(text, connection_match_msg)) {
			conn_status = true;
			return;
		}

		assert(snprintf(connection_match_msg, sizeof(connection_match_msg),
		                "Connection closed by peer") >= 0);

		if(strstr(text, connection_match_msg)) {
			conn_status = false;
			return;
		}

		assert(snprintf(connection_match_msg, sizeof(connection_match_msg),
		                "Closing connection with peer") >= 0);

		if(strstr(text, connection_match_msg)) {
			conn_status = false;
			return;
		}
	}

	return;
}

int main(int argc, char *argv[]) {
	(void)argc;

	int clientId = -1;
	char *invite_peer;

	if((argv[3]) && (argv[4])) {
		clientId = atoi(argv[3]);
		mesh_event_sock_connect(argv[4]);
	}

	execute_open(argv[1], argv[2]);
	meshlink_set_log_cb(mesh_handle, MESHLINK_INFO, callback_logger);

	execute_start();

	if(!mesh_event_sock_send(clientId, NODE_STARTED, NULL, 0)) {
		fprintf(stderr, "Trying to resend mesh event\n");
		sleep(1);
	}

	if(!argv[CMD_LINE_ARG_INVITEURL]) {
		fprintf(stderr, "Generating Invitation to PEER\n");
		invite_peer = execute_invite("peer", NULL);
		assert(invite_peer != NULL);

		if(!mesh_event_sock_send(clientId, NODE_INVITATION, invite_peer, strlen(invite_peer) + 1)) {
			fprintf(stderr, "Trying to resend mesh event\n");
			sleep(1);
		}
	}

	fprintf(stderr, "Waiting for PEER to be connected\n");

	/* Connectivity of peer is checked */
	while(!conn_status) {
		sleep(1);
	}

	fprintf(stderr, "Connected with Peer\n");

	if(!mesh_event_sock_send(clientId, META_CONN_SUCCESSFUL, NULL, 0)) {
		fprintf(stderr, "Trying to resend mesh event\n");
		sleep(1);
	}

	execute_close();
	assert(meshlink_destroy(argv[1]));
}
