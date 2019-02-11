#ifndef TEST_CASES_OPTIMAL_PMTU_H
#define TEST_CASES_OPTIMAL_PMTU_H

/*
    test_optimal_pmtu.h -- Declarations for Individual Test Case implementation functions
    Copyright (C) 2019  Guus Sliepen <guus@meshlink.io>

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

extern int test_optimal_pmtu(void);
extern int total_tests;
extern char *lxc_path;

typedef struct pmtu_attr_para {
	int    probes;
	int    probes_total_len;
	int    count;
	time_t time;
	time_t time_l;
	time_t time_h;
} pmtu_attr_para_t;

typedef struct pmtu_attr {
	pmtu_attr_para_t mtu_sent_probes;
	pmtu_attr_para_t mtu_recv_probes;
	pmtu_attr_para_t mtu_discovery;
	pmtu_attr_para_t mtu_ping;
	pmtu_attr_para_t mtu_increase;
	pmtu_attr_para_t mtu_start;
	int mtu_size;
} pmtu_attr_t;

#define NODE_PMTU_RELAY 1
#define NODE_PMTU_PEER 2

#define find_node_index(i, node_name) if(!strcasecmp(node_name, "peer")) {          \
		i = NODE_PMTU_PEER;                        \
	} else if(!strcasecmp(node_name, "relay")) {  \
		i = NODE_PMTU_RELAY;                        \
	} else {                                      \
		abort();                                    \
	}

#define PING_TRACK_TIMEOUT 100
#define CHANNEL_PORT 1234

#endif // TEST_CASES_OPTIMAL_PMTU_H
