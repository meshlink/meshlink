/*
    protocol_edge.c -- handle the meta-protocol, edges
    Copyright (C) 2014-2019 Guus Sliepen <guus@meshlink.io>

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
#include "submesh.h"

extern bool node_write_devclass(meshlink_handle_t *mesh, node_t *n);
extern bool node_write_submesh(meshlink_handle_t *mesh, node_t *n);

bool send_add_edge(meshlink_handle_t *mesh, connection_t *c, const edge_t *e, int contradictions) {
	bool x;
	char *address, *port;
	char *from_submesh, *to_submesh;
	submesh_t *s = NULL;

	if(c->node && c->node->submesh) {
		if(!submesh_allows_node(e->from->submesh, c->node)) {
			return true;
		}

		if(!submesh_allows_node(e->to->submesh, c->node)) {
			return true;
		}
	}

	if(e->from->submesh && e->to->submesh && (e->from->submesh != e->to->submesh)) {
		return true;
	}

	sockaddr2str(&e->address, &address, &port);

	if(e->from->submesh) {
		from_submesh = e->from->submesh->name;
	} else {
		from_submesh = CORE_MESH;
	}

	if(e->to->submesh) {
		to_submesh = e->to->submesh->name;
	} else {
		to_submesh = CORE_MESH;
	}

	if(e->from->submesh) {
		s = e->from->submesh;
	} else {
		s = e->to->submesh;
	}

	x = send_request(mesh, c, s, "%d %x %s %d %s %s %s %s %d %s %x %d %d", ADD_EDGE, rand(),
	                 e->from->name, e->from->devclass, from_submesh, e->to->name, address, port,
	                 e->to->devclass, to_submesh, e->options, e->weight, contradictions);
	free(address);
	free(port);

	return x;
}

bool add_edge_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	edge_t *e;
	node_t *from, *to;
	char from_name[MAX_STRING_SIZE];
	int from_devclass;
	char from_submesh_name[MAX_STRING_SIZE] = "";
	char to_name[MAX_STRING_SIZE];
	char to_address[MAX_STRING_SIZE];
	char to_port[MAX_STRING_SIZE];
	int to_devclass;
	char to_submesh_name[MAX_STRING_SIZE] = "";
	sockaddr_t address;
	uint32_t options;
	int weight;
	int contradictions = 0;
	submesh_t *s = NULL;

	if(sscanf(request, "%*d %*x "MAX_STRING" %d "MAX_STRING" "MAX_STRING" "MAX_STRING" "MAX_STRING" %d "MAX_STRING" %x %d %d",
	                from_name, &from_devclass, from_submesh_name, to_name, to_address, to_port, &to_devclass, to_submesh_name,
	                &options, &weight, &contradictions) < 10) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s", "ADD_EDGE", c->name);
		return false;
	}

	// Check if devclasses are valid

	if(from_devclass < 0 || from_devclass > _DEV_CLASS_MAX) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "ADD_EDGE", c->name, "from devclass invalid");
		return false;
	}

	if(to_devclass < 0 || to_devclass > _DEV_CLASS_MAX) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "ADD_EDGE", c->name, "to devclass invalid");
		return false;
	}

	if(0 == strcmp(from_submesh_name, "")) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "ADD_EDGE", c->name, "invalid submesh id");
		return false;
	}

	if(0 == strcmp(to_submesh_name, "")) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "ADD_EDGE", c->name, "invalid submesh id");
		return false;
	}

	if(seen_request(mesh, request)) {
		return true;
	}

	/* Lookup nodes */

	from = lookup_node(mesh, from_name);
	to = lookup_node(mesh, to_name);

	if(!from) {
		from = new_node();
		from->status.blacklisted = mesh->default_blacklist;
		from->name = xstrdup(from_name);

		from->submesh = NULL;

		if(0 != strcmp(from_submesh_name, CORE_MESH)) {
			if(!(from->submesh = lookup_or_create_submesh(mesh, from_submesh_name))) {
				return false;
			}
		}

		node_add(mesh, from);
	}

	if(contradictions > 50) {
		handle_duplicate_node(mesh, from);
	}

	from->devclass = from_devclass;
	node_write_devclass(mesh, from);

	if(from->submesh) {
		node_write_submesh(mesh, from);
	}

	if(!to) {
		to = new_node();
		to->status.blacklisted = mesh->default_blacklist;
		to->name = xstrdup(to_name);

		to->submesh = NULL;

		if(0 != strcmp(to_submesh_name, CORE_MESH)) {
			if(!(to->submesh = lookup_or_create_submesh(mesh, to_submesh_name))) {
				return false;

			}
		}

		node_add(mesh, to);
	}

	to->devclass = to_devclass;
	node_write_devclass(mesh, to);

	if(to->submesh) {
		node_write_submesh(mesh, to);
	}

	/* Convert addresses */

	address = str2sockaddr(to_address, to_port);

	/* Check if edge already exists */

	e = lookup_edge(from, to);

	if(e) {
		if(e->weight != weight || e->options != options || sockaddrcmp(&e->address, &address)) {
			if(from == mesh->self) {
				logger(mesh, MESHLINK_WARNING, "Got %s from %s for ourself which does not match existing entry",
				       "ADD_EDGE", c->name);
				send_add_edge(mesh, c, e, 0);
				return true;
			} else {
				logger(mesh, MESHLINK_WARNING, "Got %s from %s which does not match existing entry",
				       "ADD_EDGE", c->name);
				edge_unlink(mesh, e);
				graph_del_edge(mesh, e);
				free_edge(e);
			}
		} else {
			return true;
		}
	} else if(from == mesh->self) {
		logger(mesh, MESHLINK_WARNING, "Got %s from %s for ourself which does not exist",
		       "ADD_EDGE", c->name);
		mesh->contradicting_add_edge++;
		e = new_edge();
		e->from = from;
		e->to = to;
		send_del_edge(mesh, c, e, mesh->contradicting_add_edge);
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

	/* Run MST before or after we tell the rest? */

	graph_add_edge(mesh, e);

	if(e->from->submesh && e->to->submesh && (e->from->submesh != e->to->submesh)) {
		logger(mesh, MESHLINK_ERROR, "Dropping add edge ( %s to %s )", e->from->submesh->name, e->to->submesh->name);
		return false;
	}

	if(e->from->submesh) {
		s = e->from->submesh;
	} else {
		s = e->to->submesh;
	}

	/* Tell the rest about the new edge */

	forward_request(mesh, c, s, request);

	return true;
}

bool send_del_edge(meshlink_handle_t *mesh, connection_t *c, const edge_t *e, int contradictions) {
	submesh_t *s = NULL;

	if(c->node && c->node->submesh) {
		if(!submesh_allows_node(e->from->submesh, c->node)) {
			return true;
		}

		if(!submesh_allows_node(e->to->submesh, c->node)) {
			return true;
		}
	}

	if(e->from->submesh && e->to->submesh && (e->from->submesh != e->to->submesh)) {
		return true;
	}


	if(e->from->submesh) {
		s = e->from->submesh;
	} else {
		s = e->to->submesh;
	}

	return send_request(mesh, c, s, "%d %x %s %s %d", DEL_EDGE, rand(),
	                    e->from->name, e->to->name, contradictions);
}

bool del_edge_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	edge_t *e;
	char from_name[MAX_STRING_SIZE];
	char to_name[MAX_STRING_SIZE];
	node_t *from, *to;
	int contradictions = 0;
	submesh_t *s = NULL;

	if(sscanf(request, "%*d %*x "MAX_STRING" "MAX_STRING" %d", from_name, to_name, &contradictions) < 2) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s", "DEL_EDGE", c->name);
		return false;
	}

	if(seen_request(mesh, request)) {
		return true;
	}

	/* Lookup nodes */

	from = lookup_node(mesh, from_name);
	to = lookup_node(mesh, to_name);

	if(!from) {
		logger(mesh, MESHLINK_ERROR, "Got %s from %s which does not appear in the edge tree",
		       "DEL_EDGE", c->name);
		return true;
	}

	if(!to) {
		logger(mesh, MESHLINK_ERROR, "Got %s from %s which does not appear in the edge tree",
		       "DEL_EDGE", c->name);
		return true;
	}

	if(contradictions > 50) {
		handle_duplicate_node(mesh, from);
	}

	/* Check if edge exists */

	e = lookup_edge(from, to);

	if(!e) {
		logger(mesh, MESHLINK_WARNING, "Got %s from %s which does not appear in the edge tree",
		       "DEL_EDGE", c->name);
		return true;
	}

	if(e->from == mesh->self) {
		logger(mesh, MESHLINK_WARNING, "Got %s from %s for ourself",
		       "DEL_EDGE", c->name);
		mesh->contradicting_del_edge++;
		send_add_edge(mesh, c, e, mesh->contradicting_del_edge);    /* Send back a correction */
		return true;
	}

	/* Tell the rest about the deleted edge */


	if(!e->from->submesh || !e->to->submesh || (e->from->submesh == e->to->submesh)) {
		if(e->from->submesh) {
			s = e->from->submesh;
		} else {
			s = e->to->submesh;
		}

		/* Tell the rest about the deleted edge */
		forward_request(mesh, c, s, request);

	} else {
		logger(mesh, MESHLINK_ERROR, "Dropping del edge ( %s to %s )", e->from->submesh->name, e->to->submesh->name);
		return false;
	}

	/* Delete the edge */

	edge_unlink(mesh, e);
	graph_del_edge(mesh, e);
	free_edge(e);

	/* If the node is not reachable anymore but we remember it had an edge to us, clean it up */

	if(!to->status.reachable) {
		e = lookup_edge(to, mesh->self);

		if(e) {
			send_del_edge(mesh, mesh->everyone, e, 0);
			edge_unlink(mesh, e);
			graph_del_edge(mesh, e);
			free_edge(e);
		}
	}

	return true;
}
