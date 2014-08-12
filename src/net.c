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


static const int min(int a, int b) {
	return a < b ? a : b;
}

/*
  Terminate a connection:
  - Mark it as inactive
  - Remove the edge representing this connection
  - Kill it with fire
  - Check if we need to retry making an outgoing connection
*/
void terminate_connection(meshlink_handle_t *mesh, connection_t *c, bool report) {
	logger(mesh, MESHLINK_INFO, "Closing connection with %s (%s)", c->name, c->hostname);

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
	logger(mesh, MESHLINK_DEBUG, "timeout_handler()");

	for list_each(connection_t, c, mesh->connections) {
		// Also make sure that if outstanding key requests for the UDP counterpart of a connection has timed out, we restart it.
		if(c->node) {
			if(c->node->status.waitingforkey && c->node->last_req_key + mesh->pingtimeout <= mesh->loop.now.tv_sec)
				send_req_key(mesh, c->node);
		}
		if(c->last_ping_time + mesh->pingtimeout <= mesh->loop.now.tv_sec) {
			if(c->status.active) {
				if(c->status.pinged) {
					logger(mesh, MESHLINK_INFO, "%s (%s) didn't respond to PING in %ld seconds", c->name, c->hostname, (long)mesh->loop.now.tv_sec - c->last_ping_time);
				} else if(c->last_ping_time + mesh->pinginterval <= mesh->loop.now.tv_sec) {
					send_ping(mesh, c);
					continue;
				} else {
					continue;
				}
			} else {
				if(c->status.connecting)
					logger(mesh, MESHLINK_WARNING, "Timeout while connecting to %s (%s)", c->name, c->hostname);
				else
					logger(mesh, MESHLINK_WARNING, "Timeout from %s (%s) during authentication", c->name, c->hostname);
			}
			terminate_connection(mesh, c, c->status.active);
		}
	}

	timeout_set(&mesh->loop, data, &(struct timeval){mesh->pingtimeout, rand() % 100000});
}

// devclass asc, last_connect_try desc
static int node_compare_devclass_asc_last_connect_try_desc(const void *a, const void *b)
{
	const node_t *na = a, *nb = b;

	if(na->devclass < nb->devclass)
		{ return -1; }

	if(na->devclass > nb->devclass)
		{ return 1; }

	if(na->last_connect_try == nb->last_connect_try)
		return 0;

	if(nb->last_connect_try == 0 || na->last_connect_try < nb->last_connect_try)
		return -1;

	if(na->last_connect_try == 0 || na->last_connect_try > nb->last_connect_try)
		return 1;

	return 0;
}

// last_connect_try desc
static int node_compare_last_connect_try_desc(const void *a, const void *b)
{
	const node_t *na = a, *nb = b;

	if(na->last_connect_try == nb->last_connect_try)
		return 0;

	if(nb->last_connect_try == 0 || na->last_connect_try < nb->last_connect_try)
		return -1;

	if(na->last_connect_try == 0 || na->last_connect_try > nb->last_connect_try)
		return 1;

	return 0;
}

// devclass desc
static int node_compare_devclass_desc(const void *a, const void *b)
{
	const node_t *na = a, *nb = b;

	if(na->devclass < nb->devclass)
		{ return -1; }

	if(na->devclass > nb->devclass)
		{ return 1; }

	return 0;
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
		if(mesh->sleeptime < 0)
			mesh->sleeptime = 3600;
	} else {
		mesh->sleeptime /= 2;
		if(mesh->sleeptime < 10)
			mesh->sleeptime = 10;
	}

	mesh->contradicting_add_edge = 0;
	mesh->contradicting_del_edge = 0;

	int timeout = 5;

	/* Check if we need to make or break connections. */

	if(mesh->nodes->count > 1) {

		logger(mesh, MESHLINK_INFO, "--- autoconnect begin ---");


		int retry_timeout = min(mesh->nodes->count * 5, 60);

		// connect disconnect nodes

		node_t* connect_to = NULL;
		node_t* disconnect_from = NULL;


		// get cur_connects

		int cur_connects = 0;

		for list_each(connection_t, c, mesh->connections)
		{
			if(!c->status.remove_unused)
			{
				cur_connects += 1;
			}
		}

		logger(mesh, MESHLINK_INFO, "* cur_connects = %d", cur_connects);


		// get min_connects and max_connects

		int min_connects = dev_class_traits[mesh->devclass].min_connects;
		int max_connects = dev_class_traits[mesh->devclass].max_connects;

		logger(mesh, MESHLINK_INFO, "* min_connects = %d", min_connects);
		logger(mesh, MESHLINK_INFO, "* max_connects = %d", max_connects);


		// find the best one for initial connect

		if(cur_connects < min_connects)
		{
			splay_tree_t *nodes = splay_alloc_tree(node_compare_devclass_asc_last_connect_try_desc, NULL);

			for splay_each(node_t, n, mesh->nodes)
			{
				if(n->devclass <= mesh->devclass && !n->connection && (n->last_connect_try == 0 || (time(NULL) - n->last_connect_try) > retry_timeout))
					{ splay_insert(nodes, n); }
			}

			if(nodes->head)
			{
				logger(mesh, MESHLINK_INFO, "* found best one for initial connect");

				timeout = 0;
				connect_to = (node_t*)nodes->head->data;
			}

			splay_free_tree(nodes);
		}


		// find better nodes to connect to

		if(!connect_to && min_connects <= cur_connects < max_connects)
		{
			unsigned int connects = 0;

			for(int devclass = 0; devclass <= mesh->devclass; ++devclass)
			{
				for list_each(connection_t, c, mesh->connections)
				{
					if(!c->status.remove_unused && c->node && c->node->devclass == devclass)
						{ connects += 1; }
				}

				if( connects < min_connects )
				{
					splay_tree_t *nodes = splay_alloc_tree(node_compare_last_connect_try_desc, NULL);

					for splay_each(node_t, n, mesh->nodes)
					{
						if(n->devclass == devclass && !n->connection && (n->last_connect_try == 0 || (time(NULL) - n->last_connect_try) > retry_timeout))
							{ splay_insert(nodes, n); }
					}

					if(nodes->head)
					{
						logger(mesh, MESHLINK_INFO, "* found better node");
						connect_to = (node_t*)nodes->head->data;

						splay_free_tree(nodes);
						break;
					}

					splay_free_tree(nodes);
				}
				else
					{ break; }
			}
		}


		// heal partitions

		if(!connect_to && min_connects <= cur_connects < max_connects)
		{
			splay_tree_t *nodes = splay_alloc_tree(node_compare_devclass_asc_last_connect_try_desc, NULL);

			for splay_each(node_t, n, mesh->nodes)
			{
				if(n->devclass <= mesh->devclass && !n->status.reachable && (n->last_connect_try == 0 || (time(NULL) - n->last_connect_try) > retry_timeout))
					{ splay_insert(nodes, n); }
			}

			if(nodes->head)
			{
				logger(mesh, MESHLINK_INFO, "* try to heal partition");
				connect_to = (node_t*)nodes->head->data;
			}

			splay_free_tree(nodes);
		}


		// perform connect

		if(connect_to && !connect_to->connection)
		{
			logger(mesh, MESHLINK_INFO, "Autoconnecting to %s", connect_to->name);
			outgoing_t *outgoing = xzalloc(sizeof(outgoing_t));
			outgoing->mesh = mesh;
			outgoing->name = xstrdup(connect_to->name);
			list_insert_tail(mesh->outgoings, outgoing);
			setup_outgoing_connection(mesh, outgoing);
		}


		// disconnect suboptimal outgoing connections

		if(min_connects < cur_connects <= max_connects)
		{
			unsigned int connects = 0;

			for(int devclass = 0; devclass <= mesh->devclass; ++devclass)
			{
				for list_each(connection_t, c, mesh->connections)
				{
					if(!c->status.remove_unused && c->node && c->node->devclass == devclass)
						{ connects += 1; }
				}

				if( min_connects < connects )
				{
					splay_tree_t *nodes = splay_alloc_tree(node_compare_devclass_desc, NULL);

					for list_each(connection_t, c, mesh->connections)
					{
						if(!c->status.remove_unused && c->outgoing && c->node && c->node->devclass >= devclass)
							{ splay_insert(nodes, c->node); }
					}

					if(nodes->head)
					{
						logger(mesh, MESHLINK_INFO, "* disconnect suboptimal outgoing connection");
						disconnect_from = (node_t*)nodes->head->data;
					}

					splay_free_tree(nodes);
					break;
				}
			}
		}


		// disconnect connections (too many connections)

		if(!disconnect_from && max_connects < cur_connects)
		{
			splay_tree_t *nodes = splay_alloc_tree(node_compare_devclass_desc, NULL);

			for list_each(connection_t, c, mesh->connections)
			{
				if(!c->status.remove_unused && c->node)
					{ splay_insert(nodes, c->node); }
			}

			if(nodes->head)
			{
				logger(mesh, MESHLINK_INFO, "* disconnect connection (too many connections");

				timeout = 0;
				disconnect_from = (node_t*)nodes->head->data;
			}

			splay_free_tree(nodes);
		}


		// perform disconnect

		if(disconnect_from && disconnect_from->connection)
		{
			logger(mesh, MESHLINK_INFO, "Autodisconnecting from %s", disconnect_from->connection->name);
			list_delete(mesh->outgoings, disconnect_from->connection->outgoing);
			disconnect_from->connection->outgoing = NULL;
			terminate_connection(mesh, disconnect_from->connection, disconnect_from->connection->status.active);
		}


		// done!

		logger(mesh, MESHLINK_INFO, "--- autoconnect end ---");
	}

	timeout_set(&mesh->loop, data, &(struct timeval){timeout, rand() % 100000});
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
	timeout_add(&mesh->loop, &mesh->periodictimer, periodic_handler, &mesh->periodictimer, &(struct timeval){0, 0});

	//Add signal handler
	mesh->datafromapp.signum = 0;
	signal_add(&(mesh->loop),&(mesh->datafromapp), (signal_cb_t)meshlink_send_from_queue,mesh, mesh->datafromapp.signum);

	if(!event_loop_run(&mesh->loop)) {
		logger(mesh, MESHLINK_ERROR, "Error while waiting for input: %s", strerror(errno));
		return 1;
	}

	timeout_del(&mesh->loop, &mesh->periodictimer);
	timeout_del(&mesh->loop, &mesh->pingtimer);

	return 0;
}
