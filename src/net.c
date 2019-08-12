/*
    net.c -- most of the network code
    Copyright (C) 2014-2017 Guus Sliepen <guus@meshlink.io>

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

#include "system.h"

#include "autoconnect.h"
#include "conf.h"
#include "connection.h"
#include "graph.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "meta.h"
#include "net.h"
#include "netutl.h"
#include "protocol.h"
#include "utils.h"
#include "xalloc.h"

#include <assert.h>

#if !defined(min)
static inline int min(int a, int b) {
	return a < b ? a : b;
}
#endif

static const int default_timeout = 5;

/*
  Terminate a connection:
  - Mark it as inactive
  - Remove the edge representing this connection
  - Kill it with fire
  - Check if we need to retry making an outgoing connection
*/
void terminate_connection(meshlink_handle_t *mesh, connection_t *c, bool report) {
	logger(mesh, MESHLINK_INFO, "Closing connection with %s", c->name);

	c->status.active = false;

	if(c->node && c->node->connection == c) {
		c->node->connection = NULL;
	}

	if(c->edge) {
		if(report) {
			send_del_edge(mesh, mesh->everyone, c->edge, 0);
		}

		edge_del(mesh, c->edge);
		c->edge = NULL;

		/* Run MST and SSSP algorithms */

		graph(mesh);

		/* If the node is not reachable anymore but we remember it had an edge to us, clean it up */

		if(report && c->node && !c->node->status.reachable) {
			edge_t *e;
			e = lookup_edge(c->node, mesh->self);

			if(e) {
				send_del_edge(mesh, mesh->everyone, e, 0);
				edge_del(mesh, e);
			}
		}
	}

	outgoing_t *outgoing = c->outgoing;
	connection_del(mesh, c);

	/* Check if this was our outgoing connection */

	if(outgoing) {
		do_outgoing_connection(mesh, outgoing);
	}

#ifndef HAVE_MINGW
	/* Clean up dead proxy processes */

	while(waitpid(-1, NULL, WNOHANG) > 0);

#endif
}

/*
  Check if the other end is active.
  If we have sent packets, but didn't receive any,
  then possibly the other end is dead. We send a
  PING request over the meta connection. If the other
  end does not reply in time, we consider them dead
  and close the connection.
*/
static void timeout_handler(event_loop_t *loop, void *data) {
	meshlink_handle_t *mesh = loop->data;
	logger(mesh, MESHLINK_DEBUG, "timeout_handler()");

	for list_each(connection_t, c, mesh->connections) {
		int pingtimeout = c->node ? mesh->dev_class_traits[c->node->devclass].pingtimeout : default_timeout;

		// Also make sure that if outstanding key requests for the UDP counterpart of a connection has timed out, we restart it.
		if(c->node) {
			if(c->node->status.waitingforkey && c->node->last_req_key + pingtimeout <= mesh->loop.now.tv_sec) {
				send_req_key(mesh, c->node);
			}
		}

		if(c->last_ping_time + pingtimeout <= mesh->loop.now.tv_sec) {
			if(c->status.active) {
				if(c->status.pinged) {
					logger(mesh, MESHLINK_INFO, "%s didn't respond to PING in %ld seconds", c->name, (long)mesh->loop.now.tv_sec - c->last_ping_time);
				} else if(c->last_ping_time + mesh->dev_class_traits[c->node->devclass].pinginterval <= mesh->loop.now.tv_sec) {
					send_ping(mesh, c);
					continue;
				} else {
					continue;
				}
			} else {
				if(c->status.connecting) {
					logger(mesh, MESHLINK_WARNING, "Timeout while connecting to %s", c->name);
				} else {
					logger(mesh, MESHLINK_WARNING, "Timeout from %s during authentication", c->name);
				}
			}

			terminate_connection(mesh, c, c->status.active);
		}
	}

	timeout_set(&mesh->loop, data, &(struct timeval) {
		default_timeout, rand() % 100000
	});
}

static void periodic_handler(event_loop_t *loop, void *data) {
	meshlink_handle_t *mesh = loop->data;

	/* Check if there are too many contradicting ADD_EDGE and DEL_EDGE messages.
	   This usually only happens when another node has the same Name as this node.
	   If so, sleep for a short while to prevent a storm of contradicting messages.
	*/

	if(mesh->contradicting_del_edge > 100 && mesh->contradicting_add_edge > 100) {
		logger(mesh, MESHLINK_WARNING, "Possible node with same Name as us! Sleeping %d seconds.", mesh->sleeptime);
		usleep(mesh->sleeptime * 1000000LL);
		mesh->sleeptime *= 2;

		if(mesh->sleeptime < 0) {
			mesh->sleeptime = 3600;
		}
	} else {
		mesh->sleeptime /= 2;

		if(mesh->sleeptime < 10) {
			mesh->sleeptime = 10;
		}
	}

	mesh->contradicting_add_edge = 0;
	mesh->contradicting_del_edge = 0;

	int timeout = default_timeout;

	/* Check if we need to make or break connections. */

	if(mesh->nodes->count > 1) {
		unsigned int cur_connects = do_autoconnect(mesh);

		// reduce timeout if we don't have enough connections + outgoings
		if (cur_connects + mesh->outgoings->count < 3) {
			timeout = 1;
		}
	}

	/* Write dirty config files out to disk */

	for splay_each(node_t, n, mesh->nodes) {
		if(n->status.dirty) {
			node_write_config(mesh, n);
			n->status.dirty = false;
		}
	}

	timeout_set(&mesh->loop, data, &(struct timeval) {
		timeout, rand() % 100000
	});
}

void handle_meta_connection_data(meshlink_handle_t *mesh, connection_t *c) {
	if(!receive_meta(mesh, c)) {
		terminate_connection(mesh, c, c->status.active);
		return;
	}
}

void retry(meshlink_handle_t *mesh) {
	/* Reset the reconnection timers for all outgoing connections */
	for list_each(outgoing_t, outgoing, mesh->outgoings) {
		outgoing->timeout = 0;

		if(outgoing->ev.cb)
			timeout_set(&mesh->loop, &outgoing->ev, &(struct timeval) {
			0, 0
		});
	}

#ifdef HAVE_IFADDRS_H
	struct ifaddrs *ifa = NULL;
	getifaddrs(&ifa);
#endif

	/* For active connections, check if their addresses are still valid.
	 * If yes, reset their ping timers, otherwise terminate them. */
	for list_each(connection_t, c, mesh->connections) {
		if(!c->status.active) {
			continue;
		}

		if(!c->status.pinged) {
			c->last_ping_time = 0;
		}

#ifdef HAVE_IFADDRS_H

		if(!ifa) {
			continue;
		}

		sockaddr_t sa;
		socklen_t salen = sizeof(sa);

		if(getsockname(c->socket, &sa.sa, &salen)) {
			continue;
		}

		bool found = false;

		for(struct ifaddrs *ifap = ifa; ifap; ifap = ifap->ifa_next) {
			if(ifap->ifa_addr && !sockaddrcmp_noport(&sa, (sockaddr_t *)ifap->ifa_addr)) {
				found = true;
				break;
			}

		}

		if(!found) {
			logger(mesh, MESHLINK_DEBUG, "Local address for connection to %s no longer valid, terminating", c->name);
			terminate_connection(mesh, c, c->status.active);
		}

#endif
	}

#ifdef HAVE_IFADDRS_H

	if(ifa) {
		freeifaddrs(ifa);
	}

#endif

	/* Kick the ping timeout handler */
	timeout_set(&mesh->loop, &mesh->pingtimer, &(struct timeval) {
		0, 0
	});
}

/*
  this is where it all happens...
*/
int main_loop(meshlink_handle_t *mesh) {
	timeout_add(&mesh->loop, &mesh->pingtimer, timeout_handler, &mesh->pingtimer, &(struct timeval) {
		default_timeout, rand() % 100000
	});
	timeout_add(&mesh->loop, &mesh->periodictimer, periodic_handler, &mesh->periodictimer, &(struct timeval) {
		0, 0
	});

	//Add signal handler
	mesh->datafromapp.signum = 0;
	signal_add(&(mesh->loop), &(mesh->datafromapp), (signal_cb_t)meshlink_send_from_queue, mesh, mesh->datafromapp.signum);

	if(!event_loop_run(&(mesh->loop), &(mesh->mesh_mutex))) {
		logger(mesh, MESHLINK_ERROR, "Error while waiting for input: %s", strerror(errno));
		abort();
		timeout_del(&mesh->loop, &mesh->periodictimer);
		timeout_del(&mesh->loop, &mesh->pingtimer);

		return 1;
	}

	timeout_del(&mesh->loop, &mesh->periodictimer);
	timeout_del(&mesh->loop, &mesh->pingtimer);

	return 0;
}
