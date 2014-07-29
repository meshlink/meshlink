/*
    net.c -- most of the network code
    Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>

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

#include "utils.h"
#include "conf.h"
#include "connection.h"
#include "graph.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "meta.h"
#include "net.h"
#include "netutl.h"
#include "protocol.h"
#include "xalloc.h"

/*
  Terminate a connection:
  - Mark it as inactive
  - Remove the edge representing this connection
  - Kill it with fire
  - Check if we need to retry making an outgoing connection
*/
void terminate_connection(meshlink_handle_t *mesh, connection_t *c, bool report) {
	logger(DEBUG_CONNECTIONS, LOG_NOTICE, "Closing connection with %s (%s)", c->name, c->hostname);

	c->status.active = false;

	if(c->node && c->node->connection == c)
		c->node->connection = NULL;

	if(c->edge) {
		if(report)
			send_del_edge(mesh, mesh->everyone, c->edge);

		edge_del(mesh, c->edge);
		c->edge = NULL;

		/* Run MST and SSSP algorithms */

		graph(mesh);

		/* If the node is not reachable anymore but we remember it had an edge to us, clean it up */

		if(report && !c->node->status.reachable) {
			edge_t *e;
			e = lookup_edge(c->node, mesh->self);
			if(e) {
				send_del_edge(mesh, mesh->everyone, e);
				edge_del(mesh, e);
			}
		}
	}

	outgoing_t *outgoing = c->outgoing;
	connection_del(mesh, c);

	/* Check if this was our outgoing connection */

	if(outgoing)
		do_outgoing_connection(mesh, outgoing);

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

	for list_each(connection_t, c, mesh->connections) {
		if(c->last_ping_time + mesh->pingtimeout <= mesh->loop.now.tv_sec) {
			if(c->status.active) {
				if(c->status.pinged) {
					logger(DEBUG_CONNECTIONS, LOG_INFO, "%s (%s) didn't respond to PING in %ld seconds", c->name, c->hostname, (long)mesh->loop.now.tv_sec - c->last_ping_time);
				} else if(c->last_ping_time + mesh->pinginterval <= mesh->loop.now.tv_sec) {
					send_ping(mesh, c);
					continue;
				} else {
					continue;
				}
			} else {
				if(c->status.connecting)
					logger(DEBUG_CONNECTIONS, LOG_WARNING, "Timeout while connecting to %s (%s)", c->name, c->hostname);
				else
					logger(DEBUG_CONNECTIONS, LOG_WARNING, "Timeout from %s (%s) during authentication", c->name, c->hostname);
			}
			terminate_connection(mesh, c, c->status.active);
		}
	}

	timeout_set(&mesh->loop, data, &(struct timeval){mesh->pingtimeout, rand() % 100000});
}

static void periodic_handler(event_loop_t *loop, void *data) {
	meshlink_handle_t *mesh = loop->data;

	/* Check if there are too many contradicting ADD_EDGE and DEL_EDGE messages.
	   This usually only happens when another node has the same Name as this node.
	   If so, sleep for a short while to prevent a storm of contradicting messages.
	*/

	if(mesh->contradicting_del_edge > 100 && mesh->contradicting_add_edge > 100) {
		logger(DEBUG_ALWAYS, LOG_WARNING, "Possible node with same Name as us! Sleeping %d seconds.", mesh->sleeptime);
		usleep(mesh->sleeptime * 1000000LL);
		mesh->sleeptime *= 2;
		if(mesh->sleeptime < 0)
			mesh->sleeptime = 3600;
	} else {
		mesh->sleeptime /= 2;
		if(mesh->sleeptime < 10)
			mesh->sleeptime = 10;
	}

	mesh->contradicting_add_edge = 0;
	mesh->contradicting_del_edge = 0;

	/* If AutoConnect is set, check if we need to make or break connections. */

	if(autoconnect && mesh->nodes->count > 1) {
		/* Count number of active connections */
		int nc = 0;
		for list_each(connection_t, c, mesh->connections) {
			if(c->status.active)
				nc++;
		}

		if(nc < autoconnect) {
			/* Not enough active connections, try to add one.
			   Choose a random node, if we don't have a connection to it,
			   and we are not already trying to make one, create an
			   outgoing connection to this node.
			*/
			int r = rand() % mesh->nodes->count;
			int i = 0;

			for splay_each(node_t, n, mesh->nodes) {
				bool trying_unreachable = false;

				if(i++ != r) {
					if(n->status->reachable) {
						continue;
					} else {
						/* If we see an unreachable node
						   before node i, try it anyway.
						*/
						trying_unreachable = true;
					}
				}

				if(n->connection)
					break;

				bool found = false;

				for list_each(outgoing_t, outgoing, mesh->outgoings) {
					if(!strcmp(outgoing->name, n->name)) {
						found = true;
						break;
					}
				}

				if(!found) {
					//TODO: if the node is blacklisted the connection will not happen, but
					//the user will read this debug message "Autoconnecting to %s" that is misleading
					logger(DEBUG_CONNECTIONS, LOG_INFO, "Autoconnecting to %s", n->name);
					outgoing_t *outgoing = xzalloc(sizeof *outgoing);
					outgoing->name = xstrdup(n->name);
					list_insert_tail(mesh->outgoings, outgoing);
					setup_outgoing_connection(mesh, outgoing);
				} else if(trying_unreachable) {
					/* We're trying an unreachable node instead
					   of node i. We already have an outgoing
					   to it. Try the next node rather than
					   breaking here, to avoid churning on a
					   connection attempt to the first
					   unreachable node.
					*/
					continue;
				}
				break;
			}
		} else if(nc > autoconnect) {
			/* Too many active connections, try to remove one.
			   Choose a random outgoing connection to a node
			   that has at least one other connection.
			*/
			int r = rand() % nc;
			int i = 0;

			for list_each(connection_t, c, mesh->connections) {
				if(!c->status.active)
					continue;

				if(i++ != r)
					continue;

				if(!c->outgoing || !c->node || c->node->edge_tree->count < 2)
					break;

				logger(DEBUG_CONNECTIONS, LOG_INFO, "Autodisconnecting from %s", c->name);
				list_delete(mesh->outgoings, c->outgoing);
				c->outgoing = NULL;
				terminate_connection(mesh, c, c->status.active);
				break;
			}
		}

		if(nc >= autoconnect) {
			/* If we have enough active connections,
			   remove any pending outgoing connections.
			*/
			for list_each(outgoing_t, o, mesh->outgoings) {
				bool found = false;
				for list_each(connection_t, c, mesh->connections) {
					if(c->outgoing == o) {
						found = true;
						break;
					}
				}
				if(!found) {
					logger(DEBUG_CONNECTIONS, LOG_INFO, "Cancelled outgoing connection to %s", o->name);
					list_delete_node(mesh->outgoings, node);
				}
			}
		}
	}

	timeout_set(&mesh->loop, data, &(struct timeval){5, rand() % 100000});
}

void handle_meta_connection_data(meshlink_handle_t *mesh, connection_t *c) {
	if (!receive_meta(mesh, c)) {
		terminate_connection(mesh, c, c->status.active);
		return;
	}
}

void retry(meshlink_handle_t *mesh) {
	/* Reset the reconnection timers for all outgoing connections */
	for list_each(outgoing_t, outgoing, mesh->outgoings) {
		outgoing->timeout = 0;
		if(outgoing->ev.cb)
			timeout_set(&mesh->loop, &outgoing->ev, &(struct timeval){0, 0});
	}

	/* Check for outgoing connections that are in progress, and reset their ping timers */
	for list_each(connection_t, c, mesh->connections) {
		if(c->outgoing && !c->node)
			c->last_ping_time = 0;
	}

	/* Kick the ping timeout handler */
	timeout_set(&mesh->loop, &mesh->pingtimer, &(struct timeval){0, 0});
}

/*
  this is where it all happens...
*/
int main_loop(meshlink_handle_t *mesh) {
	timeout_add(&mesh->loop, &mesh->pingtimer, timeout_handler, &mesh->pingtimer, &(struct timeval){mesh->pingtimeout, rand() % 100000});
	timeout_add(&mesh->loop, &mesh->periodictimer, periodic_handler, &mesh->periodictimer, &(struct timeval){mesh->pingtimeout, rand() % 100000});

	//Add signal handler
	mesh->datafromapp.signum = 0;
	signal_add(&(mesh->loop),&(mesh->datafromapp), (signal_cb_t)meshlink_send_from_queue,mesh, mesh->datafromapp.signum);

	if(!event_loop_run(&mesh->loop)) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Error while waiting for input: %s", strerror(errno));
		return 1;
	}

	timeout_del(&mesh->loop, &mesh->periodictimer);
	timeout_del(&mesh->loop, &mesh->pingtimer);

	return 0;
}
