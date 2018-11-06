/*
    node_sim.c -- Implementation of Node Simulation for Meshlink Testing
                    for meta connection test case 01 - re-connection of
                    two nodes when relay node goes down
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

int main(int argc, char *argv[]) {
	struct timeval main_loop_wait = { 5, 0 };

	int clientid = -1;

	if((argv[3]) && (argv[4])) {
		clientid = atoi(argv[3]);
		mesh_event_sock_connect(argv[4]);
	}

	/* Setup required signals */
	setup_signals();

	/* Execute test steps */
	execute_open(argv[1], argv[2]);
	execute_start();

	if(clientid != -1) {
		mesh_event_sock_send(clientid, NODE_STARTED, NULL, 0);
	}

	/* All test steps executed - wait for signals to stop/start or close the mesh */
	while(test_running) {
		select(1, NULL, NULL, NULL, &main_loop_wait);
	}

	execute_close();
}
