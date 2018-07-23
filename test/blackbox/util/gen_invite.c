/*
    gen_invite.c -- Black Box Test Utility to generate a meshlink invite
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
#include "../../../src/meshlink.h"
#include "../common/test_step.h"

#define CMD_LINE_ARG_NODENAME   1
#define CMD_LINE_ARG_INVITEE    2

int main(int argc, char *argv[]) {
    char *invite = NULL;

    /* Start mesh, generate an invite and print out the invite */
    execute_open(argv[CMD_LINE_ARG_NODENAME], "1");
    execute_start();
    invite = execute_invite(argv[CMD_LINE_ARG_INVITEE]);
    printf("%s\n", invite);
    //execute_close();

    return EXIT_SUCCESS;
}
