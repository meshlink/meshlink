#ifndef TEST_CASES_CHANNEL_CONN_H
#define TEST_CASES_CHANNEL_CONN_H

/*
    test_cases_channel_blacklist.h -- Declarations for Individual Test Case implementation functions
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


#include <stdbool.h>

extern int test_meshlink_channel_blacklist(void);

extern void *test_channel_blacklist_disonnection_nut_01(void *arg);
extern void *test_channel_blacklist_disonnection_peer_01(void *arg);
extern void *test_channel_blacklist_disonnection_relay_01(void *arg);

extern int total_reachable_callbacks_01;
extern int total_unreachable_callbacks_01;
extern int total_channel_closure_callbacks_01;
extern bool channel_discon_case_ping;
extern bool channel_discon_network_failure_01;
extern bool channel_discon_network_failure_02;
extern bool test_channel_blacklist_disonnection_peer_01_running;
extern bool test_channel_blacklist_disonnection_relay_01_running;
extern bool test_blacklist_whitelist_01;
extern bool test_channel_restart_01;

#endif // TEST_CASES_CHANNEL_CONN_H
