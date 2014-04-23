/*
    meshlink.h -- MeshLink API
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

#ifndef MESHLINK_H
#define MESHLINK_H

#include <stdbool.h>
#include <stddef.h>

/// A handle for an instance of MeshLink.
typedef struct meshlink_handle meshlink_handle_t;

/// A handle for a MeshLink node.
typedef struct meshlink_node meshlink_node_t;

/// Code of most recent error encountered.
typedef enum {
	MESHLINK_OK,     // Everything is fine
	MESHLINK_ENOMEM, // Out of memory
	MESHLINK_ENOENT, // Node is not known
} meshlink_errno_t;

#ifndef MESHLINK_INTERNAL_H

struct meshlink_handle {
	meshlink_errno_t meshlink_errno; /// Code of the last encountered error.
	const char *errstr;     /// Textual representation of most recent error encountered.
};

struct meshlink_node {
	const char *name; // Textual name of this node.
	void *priv;       // Private pointer which the application can set at will.
};

#endif // MESHLINK_INTERNAL_H

/// Get the text for the given MeshLink error code.
/** This function returns a pointer to the string containing the description of the given error code.
 *
 *  @param errno    An error code returned by MeshLink.
 *
 *  @return         A pointer to a string containing the description of the error code.
 */
extern const char *meshlink_strerror(meshlink_errno_t errno);

/// Initialize MeshLink's configuration directory.
/** This function causes MeshLink to initialize its configuration directory,
 *  if it hasn't already been initialized.
 *  It only has to be run the first time the application starts,
 *  but it is not a problem if it is run more than once, as long as
 *  the arguments given are the same.
 *
 *  This function does not start any network I/O yet. The application should
 *  first set callbacks, and then call meshlink_start().
 *
 *  @param confbase The directory in which MeshLink will store its configuration files.
 *  @param name     The name which this instance of the application will use in the mesh.
 *
 *  @return         This function will return true if MeshLink has succesfully set up its configuration files, false otherwise.
 */
extern meshlink_handle_t *meshlink_open(const char *confbase, const char *name);

/// Start MeshLink.
/** This function causes MeshLink to open network sockets, make outgoing connections, and
 *  create a new thread, which will handle all network I/O.
 *
 *  @param confbase The directory in which MeshLink will store its configuration files.
 *
 *  @return         This function will return true if MeshLink has succesfully started its thread, false otherwise.
 */
extern bool meshlink_start(meshlink_handle_t *mesh);

/// Stop MeshLink.
/** This function causes MeshLink to disconnect from all other nodes,
 *  close all sockets, and shut down its own thread.
 *
 * @param handle    A handle which represents an instance of MeshLink.
 */
extern void meshlink_stop(meshlink_handle_t *mesh);

/// Close the MeshLink handle.
/** This function calls meshlink_stop() if necessary,
 *  and frees all memory allocated by MeshLink.
 *  Afterwards, the handle and any pointers to a struct meshlink_node are invalid.
 *
 * @param handle    A handle which represents an instance of MeshLink.
 */
extern void meshlink_close(meshlink_handle_t *mesh);

/// A callback for receiving data from the mesh.
/** @param handle    A handle which represents an instance of MeshLink.
 *  @param source    A pointer to a meshlink_node_t describing the source of the data.
 *  @param data      A pointer to a buffer containing the data sent by the source.
 *  @param len       The length of the received data.
 */
typedef void (*meshlink_receive_cb_t)(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len);

/// Set the receive callback.
/** This functions sets the callback that is called whenever another node sends data to the local node.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  @param handle    A handle which represents an instance of MeshLink.
 *  @param cb        A pointer to the function which will be called when another node sends data to the local node.
 */
extern void meshlink_set_receive_cb(meshlink_handle_t *mesh, meshlink_receive_cb_t cb);

/// A callback reporting node status changes.
/** @param handle     A handle which represents an instance of MeshLink.
 *  @param node       A pointer to a meshlink_node_t describing the node whose status changed.
 *  @param reachable  True if the node is reachable, false otherwise.
 */
typedef void (*meshlink_node_status_cb_t)(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable);

/// Set the node status callback.
/** This functions sets the callback that is called whenever another node's status changed.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  @param handle    A handle which represents an instance of MeshLink.
 *  @param cb        A pointer to the function which will be called when another node's status changes.
 */
extern void meshlink_set_node_status_cb(meshlink_handle_t *mesh, meshlink_node_status_cb_t cb);

/// Severity of log messages generated by MeshLink.
typedef enum {
	MESHLINK_DEBUG,    // Internal debugging messages. Only useful during application development.
	MESHLINK_INFO,     // Informational messages.
	MESHLINK_WARNING,  // Warnings which might indicate problems, but which are not real errors.
	MESHLINK_ERROR,    // Errors which hamper correct functioning of MeshLink, without causing it to fail completely.
	MESHLINK_CRITICAL, // Critical errors which cause MeshLink to fail completely.
} meshlink_log_level_t;

/// A callback for receiving log messages generated by MeshLink.
/** @param handle    A handle which represents an instance of MeshLink.
 *  @param level     An enum describing the severity level of the message.
 *  @param text      A pointer to a string containing the textual log message.
 */
typedef void (*meshlink_log_cb_t)(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text);

/// Set the log callback.
/** This functions sets the callback that is called whenever MeshLink has some information to log.
 *  The callback is run in MeshLink's own thread.
 *  It is important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  @param handle    A handle which represents an instance of MeshLink.
 *  @param level     An enum describing the minimum severity level. Debugging information with a lower level will not trigger the callback.
 *  @param cb        A pointer to the function which will be called when another node sends data to the local node.
 */
extern void meshlink_set_log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, meshlink_log_cb_t cb);

/// Send data to another node.
/** This functions sends one packet of data to another node in the mesh.
 *  The packet is sent using UDP semantics, which means that
 *  the packet is sent as one unit and is received as one unit,
 *  and that there is no guarantee that the packet will arrive at the destination.
 *  The application should take care of getting an acknowledgement and retransmission if necessary.
 *
 *  @param handle       A handle which represents an instance of MeshLink.
 *  @param destination  A pointer to a meshlink_node_t describing the destination for the data.
 *  @param data         A pointer to a buffer containing the data to be sent to the source.
 *  @param len          The length of the data.
 *  @return             This function will return true if MeshLink has queued the message for transmission, and false otherwise.
 *                      A return value of true does not guarantee that the message will actually arrive at the destination.
 */
extern bool meshlink_send(meshlink_handle_t *mesh, meshlink_node_t *destination, const void *data, unsigned int len);

/// Get a handle for a specific node.
/** This function returns a handle for the node with the given name.
 *
 *  @param handle       A handle which represents an instance of MeshLink.
 *  @param name         The name of the node for which a handle is requested.
 *
 *  @return             A pointer to a meshlink_node_t which represents the requested node,
 *                      or NULL if the requested node does not exist.
 */
extern meshlink_node_t *meshlink_get_node(meshlink_handle_t *mesh, const char *name);

/// Get a list of all nodes.
/** This function returns a list with handles for all known nodes.
 *
 *  @param handle       A handle which represents an instance of MeshLink.
 *  @param nodes        A pointer to an array of pointers to meshlink_node_t, which should be allocated by the application.
 *  @param nmemb        The maximum number of pointers that can be stored in the nodes array.
 *
 *  @param return       The number of known nodes. This can be larger than nmemb, in which case not all nodes were stored in the nodes array.
 */
extern size_t meshlink_get_all_nodes(meshlink_handle_t *mesh, meshlink_node_t **nodes, size_t nmemb);

/// Sign data using the local node's MeshLink key.
/** This function signs data using the local node's MeshLink key.
 *  The generated signature can be securely verified by other nodes.
 *
 *  @param handle       A handle which represents an instance of MeshLink.
 *  @param data         A pointer to a buffer containing the data to be signed.
 *  @param len          The length of the data to be signed.
 *
 *  @return             This function returns a pointer to a string containing the signature, or NULL in case of an error. 
 *                      The application should call free() after it has finished using the signature.
 */
extern char *meshlink_sign(meshlink_handle_t *mesh, const char *data, size_t len);

/// Verify the signature generated by another node of a piece of data.
/** This function verifies the signature that another node generated for a piece of data.
 *
 *  @param handle       A handle which represents an instance of MeshLink.
 *  @param source       A pointer to a meshlink_node_t describing the source of the signature.
 *  @param data         A pointer to a buffer containing the data to be verified.
 *  @param len          The length of the data to be verified.
 *  @param signature    A pointer to a string containing the signature.
 *
 *  @return             This function returns true if the signature is valid, false otherwise.
 */
extern bool meshlink_verify(meshlink_handle_t *mesh, meshlink_node_t *source, const char *data, size_t len, const char *signature);

/// Invite another node into the mesh.
/** This function generates an invitation that can be used by another node to join the same mesh as the local node.
 *  The generated invitation is a string containing a URL.
 *  This URL should be passed by the application to the invitee in a way that no eavesdroppers can see the URL.
 *  The URL can only be used once, after the user has joined the mesh the URL is no longer valid.
 *
 *  @param handle       A handle which represents an instance of MeshLink.
 *  @param name         The name that the invitee will use in the mesh.
 *
 *  @return             This function returns a string that contains the invitation URL.
 *                      The application should call free() after it has finished using the URL.
 */
extern char *meshlink_invite(meshlink_handle_t *mesh, const char *name);

/// Use an invitation to join a mesh.
/** This function allows the local node to join an existing mesh using an invitation URL generated by another node.
 *  An invitation can only be used if the local node has never connected to other nodes before.
 *  After a succesfully accepted invitation, the name of the local node may have changed.
 *
 *  @param handle       A handle which represents an instance of MeshLink.
 *  @param invitation   A string containing the invitation URL.
 *
 *  @return             This function returns true if the local node joined the mesh it was invited to, false otherwise.
 */
extern bool meshlink_join(meshlink_handle_t *mesh, const char *invitation);

/// Export the local node's key and addresses.
/** This function generates a string that contains the local node's public key and one or more IP addresses.
 *  The application can pass it in some way to another node, which can then import it,
 *  granting the local node access to the other node's mesh.
 *
 *  @param handle       A handle which represents an instance of MeshLink.
 *
 *  @return             This function returns a string that contains the exported key and addresses.
 *                      The application should call free() after it has finished using this string.
 */
extern char *meshlink_export(meshlink_handle_t *mesh);

/// Import another node's key and addresses.
/** This function accepts a string containing the exported public key and addresses of another node.
 *  By importing this data, the local node grants the other node access to its mesh.
 *
 *  @param handle       A handle which represents an instance of MeshLink.
 *  @param data         A string containing the other node's exported key and addresses.
 *
 *  @return             This function returns true if the data was valid and the other node has been granted access to the mesh, false otherwise.
 */
extern bool meshlink_import(meshlink_handle_t *mesh, const char *data);

/// Blacklist a node from the mesh.
/** This function causes the local node to blacklist another node.
 *  The local node will drop any existing connections to that node,
 *  and will not send data to it nor accept any data received from it any more.
 *
 *  @param handle       A handle which represents an instance of MeshLink.
 *  @param node         A pointer to a meshlink_node_t describing the node to be blacklisted.
 */
extern void meshlink_blacklist(meshlink_handle_t *mesh, meshlink_node_t *node);

#endif // MESHLINK_H
