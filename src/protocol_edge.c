/*
    protocol_edge.c -- handle the meta-protocol, edges
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

#include "conf.h"
#include "connection.h"
#include "edge.h"
#include "graph.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "meta.h"
#include "net.h"
#include "netutl.h"
#include "node.h"
#include "protocol.h"
#include "utils.h"
#include "xalloc.h"

extern bool node_write_devclass(meshlink_handle_t *mesh, node_t *n);

bool send_add_edge(meshlink_handle_t *mesh, connection_t *c, const edge_t *e) {
	char *address, *port;

	sockaddr2str(&e->address, &address, &port);

	int err = send_request(mesh, c, "%d %x %s %d %s %s %s %d %x %d", ADD_EDGE, rand(),
					 e->from->name, e->from->devclass, e->to->name, address, port, e->to->devclass,
					 e->options, e->weight);
    if(err) {
        logger(mesh, MESHLINK_ERROR, "send_add_edge() for connection %p failed with err=%d.\n", c, err);
    }
	free(address);
	free(port);

	return !err;
}

bool add_edge_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	edge_t *e;
	node_t *from, *to;
	char from_name[MAX_STRING_SIZE];
	int from_devclass;
	char to_name[MAX_STRING_SIZE];
	char to_address[MAX_STRING_SIZE];
	char to_port[MAX_STRING_SIZE];
	int to_devclass;
	sockaddr_t address;
	uint32_t options;
	int weight;

	if(sscanf(request, "%*d %*x "MAX_STRING" %d "MAX_STRING" "MAX_STRING" "MAX_STRING" %d %x %d",
			  from_name, &from_devclass, to_name, to_address, to_port, &to_devclass, &options, &weight) != 8) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s (%s)", "ADD_EDGE", c->name,
			   c->hostname);
		return false;
	}

	/* Check if names are valid */

	if(!check_id(from_name) || !check_id(to_name)) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s (%s): %s", "ADD_EDGE", c->name,
			   c->hostname, "invalid name");
		return false;
	}

	// Check if devclasses are valid

	if(from_devclass < 0 || from_devclass > _DEV_CLASS_MAX) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s (%s): %s", "ADD_EDGE", c->name,
			   c->hostname, "from devclass invalid");
		return false;
	}

	if(to_devclass < 0 || to_devclass > _DEV_CLASS_MAX) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s (%s): %s", "ADD_EDGE", c->name,
			   c->hostname, "to devclass invalid");
		return false;
	}

	if(seen_request(mesh, request))
		return true;

	/* Lookup nodes */

	from = lookup_node(mesh, from_name);
	to = lookup_node(mesh, to_name);

	if(!from) {
		from = new_node();
		from->name = xstrdup(from_name);
		node_add(mesh, from);
	}

	from->devclass = from_devclass;
	node_write_devclass(mesh, from);

	if(!to) {
		to = new_node();
		to->name = xstrdup(to_name);
		node_add(mesh, to);
	}

	to->devclass = to_devclass;
	node_write_devclass(mesh, to);

	/* Convert addresses */

	address = str2sockaddr(to_address, to_port);

	/* Check if edge already exists */

	e = lookup_edge(from, to);

	if(e) {
		if(e->weight != weight || e->options != options || sockaddrcmp(&e->address, &address)) {
			if(from == mesh->self) {
				logger(mesh, MESHLINK_WARNING, "Got %s from %s (%s) for ourself which does not match existing entry",
						   "ADD_EDGE", c->name, c->hostname);
				send_add_edge(mesh, c, e);
				return true;
			} else {
				logger(mesh, MESHLINK_WARNING, "Got %s from %s (%s) which does not match existing entry",
						   "ADD_EDGE", c->name, c->hostname);
				edge_del(mesh, e);
				graph(mesh);
			}
		} else
			return true;
	} else if(from == mesh->self) {
		logger(mesh, MESHLINK_WARNING, "Got %s from %s (%s) for ourself which does not exist",
				   "ADD_EDGE", c->name, c->hostname);
		mesh->contradicting_add_edge++;
		e = new_edge();
		e->from = from;
		e->to = to;
		send_del_edge(mesh, c, e);
		free_edge(e);
		return true;
	}

	e = new_edge();
	e->from = from;
	e->to = to;
	e->address = address;
	e->options = options;
	e->weight = weight;
	edge_add(mesh, e);

	/* Tell the rest about the new edge */

	forward_request(mesh, c, request);

	/* Run MST before or after we tell the rest? */

	graph(mesh);

	return true;
}

bool send_del_edge(meshlink_handle_t *mesh, connection_t *c, const edge_t *e) {
	int err = send_request(mesh, c, "%d %x %s %s", DEL_EDGE, rand(), e->from->name, e->to->name);
    if(err) {
        logger(mesh, MESHLINK_ERROR, "send_del_edge() for connection %p failed with err=%d.\n", c, err);
    }
	return !err;
}

bool del_edge_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	edge_t *e;
	char from_name[MAX_STRING_SIZE];
	char to_name[MAX_STRING_SIZE];
	node_t *from, *to;

	if(sscanf(request, "%*d %*x "MAX_STRING" "MAX_STRING, from_name, to_name) != 2) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s (%s)", "DEL_EDGE", c->name,
			   c->hostname);
		return false;
	}

	/* Check if names are valid */

	if(!check_id(from_name) || !check_id(to_name)) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s (%s): %s", "DEL_EDGE", c->name,
			   c->hostname, "invalid name");
		return false;
	}

	if(seen_request(mesh, request))
		return true;

	/* Lookup nodes */

	from = lookup_node(mesh, from_name);
	to = lookup_node(mesh, to_name);

	if(!from) {
		logger(mesh, MESHLINK_ERROR, "Got %s from %s (%s) which does not appear in the edge tree",
				   "DEL_EDGE", c->name, c->hostname);
		return true;
	}

	if(!to) {
		logger(mesh, MESHLINK_ERROR, "Got %s from %s (%s) which does not appear in the edge tree",
				   "DEL_EDGE", c->name, c->hostname);
		return true;
	}

	/* Check if edge exists */

	e = lookup_edge(from, to);

	if(!e) {
		logger(mesh, MESHLINK_WARNING, "Got %s from %s (%s) which does not appear in the edge tree",
				   "DEL_EDGE", c->name, c->hostname);
		return true;
	}

	if(e->from == mesh->self) {
		logger(mesh, MESHLINK_WARNING, "Got %s from %s (%s) for ourself",
				   "DEL_EDGE", c->name, c->hostname);
		mesh->contradicting_del_edge++;
		send_add_edge(mesh, c, e);    /* Send back a correction */
		return true;
	}

	/* Tell the rest about the deleted edge */

	forward_request(mesh, c, request);

	/* Delete the edge */

	edge_del(mesh, e);

	/* Run MST before or after we tell the rest? */

	graph(mesh);

	/* If the node is not reachable anymore but we remember it had an edge to us, clean it up */

	if(!to->status.reachable) {
		e = lookup_edge(to, mesh->self);
		if(e) {
			send_del_edge(mesh, mesh->everyone, e);
			edge_del(mesh, e);
		}
	}

	return true;
}
