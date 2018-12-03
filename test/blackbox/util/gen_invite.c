/*
    gen_invite.c -- Black Box Test Utility to generate a meshlink invite
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
#include <assert.h>
#include "../../../src/meshlink.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"

#define CMD_LINE_ARG_NODENAME   1
#define CMD_LINE_ARG_INVITEE    2

int main(int argc, char *argv[]) {

	/* Set up logging for Meshlink */
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh = meshlink_open(argv[1], argv[1], "node_sim", DEV_CLASS_BACKBONE);
	assert(mesh);
	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, meshlink_callback_logger);

	assert(meshlink_start(mesh));
	char *invitation = meshlink_invite(mesh, argv[2]);
	assert(invitation);
	printf("%s\n", invitation);

	meshlink_close(mesh);

	return EXIT_SUCCESS;
}
