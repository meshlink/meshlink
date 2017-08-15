#ifndef MESHLINK_DEVTOOLS_H
#define MESHLINK_DEVTOOLS_H

/*
    devtools.h -- header for devtools.h
    Copyright (C) 2014, 2017 Guus Sliepen <guus@meshlink.io>

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

/// \file devtools.h
/** This header files declares functions that are only intended for debugging and quality control.
 *  They are not necessary for the normal operation of MeshLink.
 *  Applications should not depend on any of these functions for their normal operation.
 */

/// An edge in the MeshLink network.
typedef struct devtool_edge devtool_edge_t;

/// An edge in the MeshLink network.
struct devtool_edge {
	struct meshlink_node *from;     ///< Pointer to a node. Node memory is
	//   owned by meshlink and should not be
	//   deallocated. Node contents may be
	//   changed by meshlink.
	struct meshlink_node *to;       ///< Pointer to a node. Node memory is
	//   owned by meshlink and should not be
	//   deallocated. Node contents may be
	//   changed by meshlink.
	struct sockaddr_storage address;///< The address information associated
	//   with this edge.
	uint32_t options;               ///< Edge options. @TODO what are edge options?
	int weight;                     ///< Weight assigned to this edge.
};

/// Get a list of edges.
/** This function returns an array with copies of all known bidirectional edges.
 *  The edges are copied to capture the mesh state at call time, since edges
 *  mutate frequently. The nodes pointed to within the devtool_edge_t type
 *  are not copies; these are the same pointers that one would get from a call
 *  to meshlink_get_all_nodes().
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param edges        A pointer to a previously allocated array of
 *                      devtool_edge_t, or NULL in which case MeshLink will
 *                      allocate a new array.
 *                      The application is allowed to call free() on the array whenever it wishes.
 *                      The pointers in the devtool_edge_t elements are valid until
 *                      meshlink_close() is called.
 *  @param nmemb        A pointer to a variable holding the number of elements that
 *                      are stored in the array. In case the @a edges @a
 *                      argument is not NULL, MeshLink might call realloc()
 *                      on the array to change its size.
 *                      The contents of this variable will be changed to reflect
 *                      the new size of the array.
 *  @return             A pointer to an array containing devtool_edge_t elements,
 *                      or NULL in case of an error.
 *                      If the @a edges @a argument was not NULL, then the
 *                      retun value can be either the same value or a different
 *                      value. If the new values is NULL, then the old array
 *                      will have been freed by Meshlink.
 */
extern devtool_edge_t *devtool_get_all_edges(meshlink_handle_t *mesh, devtool_edge_t *edges, size_t *nmemb);

/// Export a list of edges to a file in JSON format.
/*  @param mesh         A handle which represents an instance of MeshLink.
 *  @param FILE         An open file descriptor to which a JSON representation of the edges will be written.
 *
 *  @return             True in case of success, false otherwise.
 */
extern bool devtool_export_json_all_edges_state(meshlink_handle_t *mesh, FILE *stream);

#endif
