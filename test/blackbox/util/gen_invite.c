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
#include <assert.h>
#include <stdlib.h>
#include "../../../src/meshlink.h"
#include "../common/test_step.h"

#define CMD_LINE_ARG_NODENAME   1
#define CMD_LINE_ARG_INVITEE    2

void logger_cb(meshlink_handle_t *mesh, meshlink_log_level_t level,
               const char *text) {
	(void)mesh;
	(void)level;

	fprintf(stderr, "meshlink>> %s\n", text);
}

int main(int argc, char *argv[]) {
	char *invite = NULL;

	/* Start mesh, generate an invite and print out the invite */
	/* Set up logging for Meshlink */
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, logger_cb);

	/* Create meshlink instance */
	meshlink_handle_t *mesh = meshlink_open("testconf", argv[1], "node_sim", DEV_CLASS_STATIONARY);
	assert(mesh);

	/* Set up logging for Meshlink with the newly acquired Mesh Handle */
	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, logger_cb);
	meshlink_enable_discovery(mesh, false);
	assert(meshlink_start(mesh));
	invite = meshlink_invite_ex(mesh, NULL, argv[2], MESHLINK_INVITE_LOCAL | MESHLINK_INVITE_NUMERIC);
	printf("%s\n", invite);
	meshlink_close(mesh);

	return EXIT_SUCCESS;
}
