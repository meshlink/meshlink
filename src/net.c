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

#include "utils.h"
#include "conf.h"
#include "connection.h"
#include "devtools.h"
#include "graph.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "meta.h"
#include "net.h"
#include "netutl.h"
#include "protocol.h"
#include "sptps.h"
#include "xalloc.h"

#include <assert.h>

#if !defined(min)
static inline int min(int a, int b) {
	return a < b ? a : b;
}
#endif

static const int default_timeout = 5;
static const int default_interval = 60;

/*
  Terminate a connection:
  - Mark it as inactive
  - Remove the edge representing this connection
  - Kill it with fire
  - Check if we need to retry making an outgoing connection
*/
void terminate_connection(meshlink_handle_t *mesh, connection_t *c, bool report) {
	if(c->status.active) {
		logger(mesh, MESHLINK_INFO, "Closing connection with %s", c->name);
	}

	if(c->node && c->node->connection == c) {
		if(c->status.active && mesh->meta_status_cb) {
			mesh->meta_status_cb(mesh, (meshlink_node_t *)c->node, false);
		}

		c->node->connection = NULL;
	}

	c->status.active = false;

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
	assert(data);

	meshlink_handle_t *mesh = loop->data;
	logger(mesh, MESHLINK_DEBUG, "timeout_handler()");

	for list_each(connection_t, c, mesh->connections) {
		int pingtimeout = c->node ? mesh->dev_class_traits[c->node->devclass].pingtimeout : default_timeout;
		int pinginterval = c->node ? mesh->dev_class_traits[c->node->devclass].pinginterval : default_interval;

		if(c->outgoing && !c->status.active && c->outgoing->timeout < 5) {
			pingtimeout = 1;
		}

		// Also make sure that if outstanding key requests for the UDP counterpart of a connection has timed out, we restart it.
		if(c->node) {
			if(c->node->status.waitingforkey && c->node->last_req_key + pingtimeout < mesh->loop.now.tv_sec) {
				send_req_key(mesh, c->node);
			}
		}

		if(c->status.active && c->last_key_renewal + 3600 < mesh->loop.now.tv_sec) {
			devtool_sptps_renewal_probe((meshlink_node_t *)c->node);

			if(!sptps_force_kex(&c->sptps)) {
				logger(mesh, MESHLINK_ERROR, "SPTPS key renewal for connection with %s failed", c->name);
				terminate_connection(mesh, c, true);
				continue;
			} else {
				c->last_key_renewal = mesh->loop.now.tv_sec;
			}
		}

		if(c->last_ping_time + pingtimeout < mesh->loop.now.tv_sec) {
			if(c->status.active) {
				if(c->status.pinged) {
					logger(mesh, MESHLINK_INFO, "%s didn't respond to PING in %ld seconds", c->name, (long)mesh->loop.now.tv_sec - c->last_ping_time);
				} else if(c->last_ping_time + pinginterval <= mesh->loop.now.tv_sec) {
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

	timeout_set(&mesh->loop, data, &(struct timespec) {
		1, prng(mesh, TIMER_FUDGE)
	});
}

// devclass asc, last_successfull_connection desc
static int node_compare_devclass_asc_lsc_desc(const void *a, const void *b) {
	const node_t *na = a, *nb = b;

	if(na->devclass < nb->devclass) {
		return -1;
	}

	if(na->devclass > nb->devclass) {
		return 1;
	}

	if(na->last_successfull_connection == nb->last_successfull_connection) {
		return 0;
	}

	if(na->last_successfull_connection == 0 || na->last_successfull_connection > nb->last_successfull_connection) {
		return -1;
	}

	if(nb->last_successfull_connection == 0 || na->last_successfull_connection < nb->last_successfull_connection) {
		return 1;
	}

	if(na < nb) {
		return -1;
	}

	if(na > nb) {
		return 1;
	}

	return 0;
}

// last_successfull_connection desc
static int node_compare_lsc_desc(const void *a, const void *b) {
	const node_t *na = a, *nb = b;

	if(na->last_successfull_connection == nb->last_successfull_connection) {
		return 0;
	}

	if(na->last_successfull_connection == 0 || na->last_successfull_connection > nb->last_successfull_connection) {
		return -1;
	}

	if(nb->last_successfull_connection == 0 || na->last_successfull_connection < nb->last_successfull_connection) {
		return 1;
	}

	if(na < nb) {
		return -1;
	}

	if(na > nb) {
		return 1;
	}

	return 0;
}

// devclass desc
static int node_compare_devclass_desc(const void *a, const void *b) {
	const node_t *na = a, *nb = b;

	if(na->devclass < nb->devclass) {
		return -1;
	}

	if(na->devclass > nb->devclass) {
		return 1;
	}

	if(na < nb) {
		return -1;
	}

	if(na > nb) {
		return 1;
	}

	return 0;
}


/*

autoconnect()
{
        timeout = 5

        // find the best one for initial connect

        if cur < min
                newcon =
                        first from nodes
                                where dclass <= my.dclass and !connection and (timestamp - last_retry) > retry_timeout
                                order by dclass asc, last_connection desc
                if newcon
                        timeout = 0
                        goto connect


        // find better nodes to connect to: in case we have less than min connections within [BACKBONE, i] and there are nodes which we are not connected to within the range

        if min <= cur < max
                j = 0
                for i = BACKBONE to my.dclass
                        j += count(from connections where node.dclass = i)
                        if j < min
                                newcon =
                                        first from nodes
                                                where dclass = i and !connection and (timestamp - last_retry) > retry_timeout
                                                order by last_connection desc
                                if newcon
                                        goto connect
                        else
                                break


        // heal partitions

        if min <= cur < max
                newcon =
                        first from nodes
                                where dclass <= my.dclass and !reachable and (timestamp - last_retry) > retry_timeout
                                order by dclass asc, last_connection desc
                if newcon
                        goto connect


        // connect

connect:
        if newcon
                connect newcon


        // disconnect outgoing connections in case we have more than min connections within [BACKBONE, i] and there are nodes which we are connected to within the range [i, PORTABLE]

        if min < cur <= max
                j = 0
                for i = BACKBONE to my.dclass
                        j += count(from connections where node.dclass = i)
                        if min < j
                                delcon =
                                        first from nodes
                                                where dclass >= i and outgoing_connection
                                                order by dclass desc
                                if disconnect
                                        goto disconnect
                                else
                                        break


        // disconnect connections in case we have more than enough connections

        if max < cur
                delcon =
                        first from nodes
                                where outgoing_connection
                                order by dclass desc
                goto disconnect

        // disconnect

disconnect
        if delcon
                disconnect delcon


        // next iteration
        next (timeout, autoconnect)

}

*/


static void periodic_handler(event_loop_t *loop, void *data) {
	meshlink_handle_t *mesh = loop->data;

	/* Check if there are too many contradicting ADD_EDGE and DEL_EDGE messages.
	   This usually only happens when another node has the same Name as this node.
	   If so, sleep for a short while to prevent a storm of contradicting messages.
	*/

	if(mesh->contradicting_del_edge > 100 && mesh->contradicting_add_edge > 100) {
		logger(mesh, MESHLINK_WARNING, "Possible node with same Name as us! Sleeping %d seconds.", mesh->sleeptime);
		struct timespec ts = {mesh->sleeptime, 0};
		nanosleep(&ts, NULL);
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

		logger(mesh, MESHLINK_DEBUG, "--- autoconnect begin ---");

		int retry_timeout = min(mesh->nodes->count * default_timeout, 60);

		logger(mesh, MESHLINK_DEBUG, "* devclass = %d", mesh->devclass);
		logger(mesh, MESHLINK_DEBUG, "* nodes = %d", mesh->nodes->count);
		logger(mesh, MESHLINK_DEBUG, "* retry_timeout = %d", retry_timeout);


		// connect disconnect nodes

		node_t *connect_to = NULL;
		node_t *disconnect_from = NULL;


		// get cur_connects

		unsigned int cur_connects = 0;

		for list_each(connection_t, c, mesh->connections) {
			if(c->status.active) {
				cur_connects += 1;
			}
		}

		logger(mesh, MESHLINK_DEBUG, "* cur_connects = %d", cur_connects);
		logger(mesh, MESHLINK_DEBUG, "* outgoings = %d", mesh->outgoings->count);

		// get min_connects and max_connects

		unsigned int min_connects = mesh->dev_class_traits[mesh->devclass].min_connects;
		unsigned int max_connects = mesh->dev_class_traits[mesh->devclass].max_connects;

		logger(mesh, MESHLINK_DEBUG, "* min_connects = %d", min_connects);
		logger(mesh, MESHLINK_DEBUG, "* max_connects = %d", max_connects);

		// find the best one for initial connect

		if(cur_connects < min_connects) {
			splay_tree_t *nodes = splay_alloc_tree(node_compare_devclass_asc_lsc_desc, NULL);

			for splay_each(node_t, n, mesh->nodes) {
				logger(mesh, MESHLINK_DEBUG, "* %s->devclass = %d", n->name, n->devclass);

				if(n != mesh->self && n->devclass <= mesh->devclass && !n->connection && !n->status.blacklisted && (n->last_connect_try == 0 || (mesh->loop.now.tv_sec - n->last_connect_try) > retry_timeout)) {
					splay_insert(nodes, n);
				}
			}

			if(nodes->head) {
				//timeout = 0;
				connect_to = (node_t *)nodes->head->data;

				logger(mesh, MESHLINK_DEBUG, "* found best one for initial connect: %s", connect_to->name);
			} else {
				logger(mesh, MESHLINK_DEBUG, "* could not find node for initial connect");
			}

			splay_delete_tree(nodes);
		}


		// find better nodes to connect to

		if(!connect_to && min_connects <= cur_connects && cur_connects < max_connects) {
			unsigned int connects = 0;

			for(dev_class_t devclass = 0; devclass <= mesh->devclass; ++devclass) {
				for list_each(connection_t, c, mesh->connections) {
					if(c->status.active && c->node && c->node->devclass == devclass) {
						connects += 1;
					}
				}

				if(connects < min_connects) {
					splay_tree_t *nodes = splay_alloc_tree(node_compare_lsc_desc, NULL);

					for splay_each(node_t, n, mesh->nodes) {
						if(n != mesh->self && n->devclass == devclass && !n->connection && !n->status.blacklisted && (n->last_connect_try == 0 || (mesh->loop.now.tv_sec - n->last_connect_try) > retry_timeout)) {
							splay_insert(nodes, n);
						}
					}

					if(nodes->head) {
						logger(mesh, MESHLINK_DEBUG, "* found better node");
						connect_to = (node_t *)nodes->head->data;

						splay_delete_tree(nodes);
						break;
					}

					splay_delete_tree(nodes);
				} else {
					break;
				}
			}

			if(!connect_to) {
				logger(mesh, MESHLINK_DEBUG, "* could not find better nodes");
			}
		}


		// heal partitions

		if(!connect_to && min_connects <= cur_connects && cur_connects < max_connects) {
			splay_tree_t *nodes = splay_alloc_tree(node_compare_devclass_asc_lsc_desc, NULL);

			for splay_each(node_t, n, mesh->nodes) {
				if(n != mesh->self && n->devclass <= mesh->devclass && !n->status.reachable && !n->status.blacklisted && (n->last_connect_try == 0 || (mesh->loop.now.tv_sec - n->last_connect_try) > retry_timeout)) {
					splay_insert(nodes, n);
				}
			}

			if(nodes->head) {
				logger(mesh, MESHLINK_DEBUG, "* try to heal partition");
				connect_to = (node_t *)nodes->head->data;
			} else {
				logger(mesh, MESHLINK_DEBUG, "* could not find nodes for partition healing");
			}

			splay_delete_tree(nodes);
		}


		// perform connect

		if(connect_to && !connect_to->connection) {
			connect_to->last_connect_try = mesh->loop.now.tv_sec;
			logger(mesh, MESHLINK_DEBUG, "Autoconnect trying to connect to %s", connect_to->name);

			/* check if there is already a connection attempt to this node */
			bool skip = false;

			for list_each(outgoing_t, outgoing, mesh->outgoings) {
				if(outgoing->node == connect_to) {
					logger(mesh, MESHLINK_DEBUG, "* skip autoconnect since it is an outgoing connection already");
					skip = true;
					break;
				}
			}

			if(!connect_to->status.reachable && !node_read_public_key(mesh, connect_to)) {
				logger(mesh, MESHLINK_DEBUG, "* skip autoconnect since we don't know this node's public key");
				skip = true;
			}

			if(!skip) {
				logger(mesh, MESHLINK_DEBUG, "Autoconnecting to %s", connect_to->name);
				outgoing_t *outgoing = xzalloc(sizeof(outgoing_t));
				outgoing->node = connect_to;
				list_insert_tail(mesh->outgoings, outgoing);
				setup_outgoing_connection(mesh, outgoing);
			}
		}


		// disconnect suboptimal outgoing connections

		if(min_connects < cur_connects /*&& cur_connects <= max_connects*/) {
			unsigned int connects = 0;

			for(dev_class_t devclass = 0; devclass <= mesh->devclass; ++devclass) {
				for list_each(connection_t, c, mesh->connections) {
					if(c->status.active && c->node && c->node->devclass == devclass) {
						connects += 1;
					}
				}

				if(min_connects < connects) {
					splay_tree_t *nodes = splay_alloc_tree(node_compare_devclass_desc, NULL);

					for list_each(connection_t, c, mesh->connections) {
						if(c->outgoing && c->node && c->node->devclass >= devclass) {
							splay_insert(nodes, c->node);
						}
					}

					if(nodes->head) {
						logger(mesh, MESHLINK_DEBUG, "* disconnect suboptimal outgoing connection");
						disconnect_from = (node_t *)nodes->head->data;
					}

					splay_delete_tree(nodes);
					break;
				}
			}

			if(!disconnect_from) {
				logger(mesh, MESHLINK_DEBUG, "* no suboptimal outgoing connections");
			}
		}


		// disconnect connections (too many connections)

		if(!disconnect_from && max_connects < cur_connects) {
			splay_tree_t *nodes = splay_alloc_tree(node_compare_devclass_desc, NULL);

			for list_each(connection_t, c, mesh->connections) {
				if(c->status.active && c->node) {
					splay_insert(nodes, c->node);
				}
			}

			if(nodes->head) {
				logger(mesh, MESHLINK_DEBUG, "* disconnect connection (too many connections)");

				//timeout = 0;
				disconnect_from = (node_t *)nodes->head->data;
			} else {
				logger(mesh, MESHLINK_DEBUG, "* no node we want to disconnect, even though we have too many connections");
			}

			splay_delete_tree(nodes);
		}


		// perform disconnect

		if(disconnect_from && disconnect_from->connection) {
			logger(mesh, MESHLINK_DEBUG, "Autodisconnecting from %s", disconnect_from->connection->name);
			list_delete(mesh->outgoings, disconnect_from->connection->outgoing);
			disconnect_from->connection->outgoing = NULL;
			terminate_connection(mesh, disconnect_from->connection, disconnect_from->connection->status.active);
		}

		// reduce timeout if we don't have enough connections + outgoings
		if(cur_connects + mesh->outgoings->count < 3) {
			timeout = 1;
		}

		// done!

		logger(mesh, MESHLINK_DEBUG, "--- autoconnect end ---");
	}

	for splay_each(node_t, n, mesh->nodes) {
		if(n->status.dirty) {
			if(!node_write_config(mesh, n, false)) {
				logger(mesh, MESHLINK_DEBUG, "Could not update %s", n->name);
			}
		}

		if(n->status.reachable && n->status.validkey && n->last_req_key + 3600 < mesh->loop.now.tv_sec) {
			logger(mesh, MESHLINK_DEBUG, "SPTPS key renewal for node %s", n->name);
			devtool_sptps_renewal_probe((meshlink_node_t *)n);

			if(!sptps_force_kex(&n->sptps)) {
				logger(mesh, MESHLINK_ERROR, "SPTPS key renewal for node %s failed", n->name);
				n->status.validkey = false;
				sptps_stop(&n->sptps);
				n->status.waitingforkey = false;
				n->last_req_key = -3600;
			} else {
				n->last_req_key = mesh->loop.now.tv_sec;
			}
		}
	}

	timeout_set(&mesh->loop, data, &(struct timespec) {
		timeout, prng(mesh, TIMER_FUDGE)
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

		if(outgoing->ev.cb) {
			timeout_set(&mesh->loop, &outgoing->ev, &(struct timespec) {
				0, 0
			});
		}
	}

	/* For active connections, check if their addresses are still valid.
	 * If yes, reset their ping timers, otherwise terminate them. */
	for list_each(connection_t, c, mesh->connections) {
		if(!c->status.active) {
			continue;
		}

		if(!c->status.pinged) {
			c->last_ping_time = -3600;
		}

		sockaddr_t sa;
		socklen_t salen = sizeof(sa);

		if(getsockname(c->socket, &sa.sa, &salen)) {
			continue;
		}

		switch(sa.sa.sa_family) {
		case AF_INET:
			sa.in.sin_port = 0;
			break;

		case AF_INET6:
			sa.in6.sin6_port = 0;
			break;

		default:
			continue;
		}

		int sock = socket(sa.sa.sa_family, SOCK_STREAM, IPPROTO_TCP);

		if(sock == -1) {
			continue;
		}

		if(bind(sock, &sa.sa, salen) && errno == EADDRNOTAVAIL) {
			logger(mesh, MESHLINK_DEBUG, "Local address for connection to %s no longer valid, terminating", c->name);
			terminate_connection(mesh, c, c->status.active);
		}

		closesocket(sock);
	}

	/* Kick the ping timeout handler */
	if(mesh->pingtimer.cb) {
		timeout_set(&mesh->loop, &mesh->pingtimer, &(struct timespec) {
			0, 0
		});
	}
}

/*
  this is where it all happens...
*/
void main_loop(meshlink_handle_t *mesh) {
	timeout_add(&mesh->loop, &mesh->pingtimer, timeout_handler, &mesh->pingtimer, &(struct timespec) {
		1, prng(mesh, TIMER_FUDGE)
	});
	timeout_add(&mesh->loop, &mesh->periodictimer, periodic_handler, &mesh->periodictimer, &(struct timespec) {
		0, 0
	});

	//Add signal handler
	mesh->datafromapp.signum = 0;
	signal_add(&mesh->loop, &mesh->datafromapp, meshlink_send_from_queue, mesh, mesh->datafromapp.signum);

	if(!event_loop_run(&mesh->loop, mesh)) {
		logger(mesh, MESHLINK_ERROR, "Error while waiting for input: %s", strerror(errno));
		call_error_cb(mesh, MESHLINK_ENETWORK);
	}

	signal_del(&mesh->loop, &mesh->datafromapp);
	timeout_del(&mesh->loop, &mesh->periodictimer);
	timeout_del(&mesh->loop, &mesh->pingtimer);
}
