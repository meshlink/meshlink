/*
    test_step.h -- Handlers for executing test steps during node simulation
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

#ifndef TEST_STEP_H
#define TEST_STEP_H

#include "../../../src/meshlink.h"

meshlink_handle_t *execute_open(char *node_name, char *dev_class);
char *execute_invite(char *invitee);
void execute_join(char *invite_url);
void execute_start(void);
void execute_stop(void);
void execute_close(void);
void execute_change_ip(void);

#endif // TEST_STEP_H
