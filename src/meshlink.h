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

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// The length in bytes of a signature made with meshlink_sign()
#define MESHLINK_SIGLEN  (64)

// The maximum length of fingerprints
#define MESHLINK_FINGERPRINTLEN  (64)

/// A handle for an instance of MeshLink.
typedef struct meshlink_handle meshlink_handle_t;

/// A handle for a MeshLink node.
typedef struct meshlink_node meshlink_node_t;

/// A handle for a MeshLink edge.
typedef struct meshlink_edge meshlink_edge_t;

/// A handle for a MeshLink channel.
typedef struct meshlink_channel meshlink_channel_t;

/// Code of most recent error encountered.
typedef enum {
	MESHLINK_OK,     ///< Everything is fine
	MESHLINK_EINVAL, ///< Invalid parameter(s) to function call
	MESHLINK_ENOMEM, ///< Out of memory
	MESHLINK_ENOENT, ///< Node is not known
	MESHLINK_EEXIST, ///< Node already exists
	MESHLINK_EINTERNAL, ///< MeshLink internal error
	MESHLINK_ERESOLV, ///< MeshLink could not resolve a hostname
	MESHLINK_ESTORAGE, ///< MeshLink coud not load or write data from/to disk
	MESHLINK_ENETWORK, ///< MeshLink encountered a network error
	MESHLINK_EPEER, ///< A peer caused an error
} meshlink_errno_t;

/// Device class
typedef enum {
	DEV_CLASS_BACKBONE = 0,
	DEV_CLASS_STATIONARY = 1,
	DEV_CLASS_PORTABLE = 2,
	DEV_CLASS_UNKNOWN = 3,
	_DEV_CLASS_MAX = 3
} dev_class_t;

/// A variable holding the last encountered error from MeshLink.
/** This is a thread local variable that contains the error code of the most recent error
 *  encountered by a MeshLink API function called in the current thread.
 *  The variable is only updated when an error is encountered, and is not reset to MESHLINK_OK
 *  if a function returned succesfully.
 */
extern __thread meshlink_errno_t meshlink_errno;

#ifndef MESHLINK_INTERNAL_H

struct meshlink_handle {
	char *name;       ///< Textual name of ourself. It is stored in a nul-terminated C string, which is allocated by MeshLink.
	void *priv;       ///< Private pointer which may be set freely by the application, and is never used or modified by MeshLink.
};

struct meshlink_node {
	char *name;       ///< Textual name of this node. It is stored in a nul-terminated C string, which is allocated by MeshLink.
	void *priv;       ///< Private pointer which may be set freely by the application, and is never used or modified by MeshLink.
};

struct meshlink_channel {
	struct meshlink_node *node; ///< Pointer to the peer of this channel.
	void *priv;                 ///< Private pointer which may be set freely by the application, and is never used or modified by MeshLink.
};

#endif // MESHLINK_INTERNAL_H

/// An edge in the meshlink network.
struct meshlink_edge {
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

/// Get the text for the given MeshLink error code.
/** This function returns a pointer to the string containing the description of the given error code.
 *
 *  @param err      An error code returned by MeshLink.
 *
 *  @return         A pointer to a string containing the description of the error code.
 *                  The pointer is to static storage that is valid for the lifetime of the application.
 *                  This function will always return a valid pointer, even if an invalid error code has been passed.
 */
extern const char *meshlink_strerror(meshlink_errno_t err);

/// Open or create a MeshLink instance.
/** This function opens or creates a MeshLink instance.
 *  The state is stored in the configuration directory passed in the variable @a confbase @a.
 *  If the configuration directory does not exist yet, for example when it is the first time
 *  this instance is opened, the configuration directory will be automatically created and initialized.
 *  However, the parent directory should already exist, otherwise an error will be returned.
 *
 *  The name given should be a unique identifier for this instance.
 *
 *  This function returns a pointer to a struct meshlink_handle that will be allocated by MeshLink.
 *  When the application does no longer need to use this handle, it must call meshlink_close() to
 *  free its resources.
 *
 *  This function does not start any network I/O yet. The application should
 *  first set callbacks, and then call meshlink_start().
 *
 *  @param confbase The directory in which MeshLink will store its configuration files.
 *                  After the function returns, the application is free to overwrite or free @a confbase @a.
 *  @param name     The name which this instance of the application will use in the mesh.
 *                  After the function returns, the application is free to overwrite or free @a name @a.
 *  @param appname  The application name which will be used in the mesh.
 *                  After the function returns, the application is free to overwrite or free @a name @a.
 *  @param devclass The device class which will be used in the mesh.
 *
 *  @return         A pointer to a meshlink_handle_t which represents this instance of MeshLink, or NULL in case of an error.
 *                  The pointer is valid until meshlink_close() is called.
 */
extern meshlink_handle_t *meshlink_open(const char *confbase, const char *name, const char* appname, dev_class_t devclass);

/// Start MeshLink.
/** This function causes MeshLink to open network sockets, make outgoing connections, and
 *  create a new thread, which will handle all network I/O.
 *
 *  It is allowed to call this function even if MeshLink is already started, in which case it will return true.
 *
 *  @param mesh     A handle which represents an instance of MeshLink.
 *
 *  @return         This function will return true if MeshLink has succesfully started, false otherwise.
 */
extern bool meshlink_start(meshlink_handle_t *mesh);

/// Stop MeshLink.
/** This function causes MeshLink to disconnect from all other nodes,
 *  close all sockets, and shut down its own thread.
 *
 *  This function always succeeds. It is allowed to call meshlink_stop() even if MeshLink is already stopped or has never been started.
 *  Channels that are still open will remain valid, but any communication via channels will stop as well.
 *
 *  @param mesh     A handle which represents an instance of MeshLink.
 */
extern void meshlink_stop(meshlink_handle_t *mesh);

/// Close the MeshLink handle.
/** This function calls meshlink_stop() if necessary,
 *  and frees the struct meshlink_handle and all associacted memory allocated by MeshLink, including all channels.
 *  Afterwards, the handle and any pointers to a struct meshlink_node or struct meshlink_channel are invalid.
 *
 *  It is allowed to call this function at any time on a valid handle, except inside callback functions.
 *  If called at a proper time with a valid handle, this function always succeeds.
 *  If called within a callback or with an invalid handle, the result is undefined.
 *
 *  @param mesh     A handle which represents an instance of MeshLink.
 */
extern void meshlink_close(meshlink_handle_t *mesh);

/// A callback for receiving data from the mesh.
/** @param mesh      A handle which represents an instance of MeshLink.
 *  @param source    A pointer to a meshlink_node_t describing the source of the data.
 *  @param data      A pointer to a buffer containing the data sent by the source, or NULL in case there is no data (an empty packet was received).
 *                   The pointer is only valid during the lifetime of the callback.
 *                   The callback should mempcy() the data if it needs to be available outside the callback.
 *  @param len       The length of the received data, or 0 in case there is no data.
 */
typedef void (*meshlink_receive_cb_t)(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len);

/// Set the receive callback.
/** This functions sets the callback that is called whenever another node sends data to the local node.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param cb        A pointer to the function which will be called when another node sends data to the local node.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
extern void meshlink_set_receive_cb(meshlink_handle_t *mesh, meshlink_receive_cb_t cb);

/// A callback reporting node status changes.
/** @param mesh       A handle which represents an instance of MeshLink.
 *  @param node       A pointer to a meshlink_node_t describing the node whose status changed.
 *                    This pointer is valid until meshlink_close() is called.
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
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param cb        A pointer to the function which will be called when another node's status changes.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
extern void meshlink_set_node_status_cb(meshlink_handle_t *mesh, meshlink_node_status_cb_t cb);

/// Severity of log messages generated by MeshLink.
typedef enum {
	MESHLINK_DEBUG,    ///< Internal debugging messages. Only useful during application development.
	MESHLINK_INFO,     ///< Informational messages.
	MESHLINK_WARNING,  ///< Warnings which might indicate problems, but which are not real errors.
	MESHLINK_ERROR,    ///< Errors which hamper correct functioning of MeshLink, without causing it to fail completely.
	MESHLINK_CRITICAL, ///< Critical errors which cause MeshLink to fail completely.
} meshlink_log_level_t;

/// A callback for receiving log messages generated by MeshLink.
/** @param mesh      A handle which represents an instance of MeshLink, or NULL.
 *  @param level     An enum describing the severity level of the message.
 *  @param text      A pointer to a nul-terminated C string containing the textual log message.
 *                   This pointer is only valid for the duration of the callback.
 *                   The application must not free() this pointer.
 *                   The application should strdup() the text if it has to be available outside the callback.
 */
typedef void (*meshlink_log_cb_t)(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text);

/// Set the log callback.
/** This functions sets the callback that is called whenever MeshLink has some information to log.
 *
 *  The @a mesh @a parameter can either be a valid MeshLink handle, or NULL.
 *  In case it is NULL, the callback will be called for errors that happen outside the context of a valid mesh instance.
 *  Otherwise, it will be called for errors that happen in the context of the given mesh instance.
 *
 *  If @a mesh @a is not NULL, then the callback is run in MeshLink's own thread.
 *  It is important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  The @a mesh @a parameter can either be a valid MeshLink handle, or NULL.
 *  In case it is NULL, the callback will be called for errors that happen outside the context of a valid mesh instance.
 *  Otherwise, it will be called for errors that happen in the context of the given mesh instance.
 *
 *  @param mesh      A handle which represents an instance of MeshLink, or NULL.
 *  @param level     An enum describing the minimum severity level. Debugging information with a lower level will not trigger the callback.
 *  @param cb        A pointer to the function which will be called when another node sends data to the local node.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
extern void meshlink_set_log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, meshlink_log_cb_t cb);

/// Send data to another node.
/** This functions sends one packet of data to another node in the mesh.
 *  The packet is sent using UDP semantics, which means that
 *  the packet is sent as one unit and is received as one unit,
 *  and that there is no guarantee that the packet will arrive at the destination.
 *  Packets that are too big to be sent over the network as one unit might be dropped, and this function may return an error if this situation can be detected beforehand.
 *  The application should not send packets that are larger than the path MTU, which can be queried with meshlink_get_pmtu().
 *  The application should take care of getting an acknowledgement and retransmission if necessary.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param destination  A pointer to a meshlink_node_t describing the destination for the data.
 *  @param data         A pointer to a buffer containing the data to be sent to the source.
 *                      After meshlink_send() returns, the application is free to overwrite or free this buffer.
 *                      It is valid to specify a NULL pointer, but only if @a len @a is also 0.
 *  @param len          The length of the data.
 *  @return             This function will return true if MeshLink has queued the message for transmission, and false otherwise.
 *                      A return value of true does not guarantee that the message will actually arrive at the destination.
 */
extern bool meshlink_send(meshlink_handle_t *mesh, meshlink_node_t *destination, const void *data, size_t len);

/// Query the maximum packet size that can be sent to a node.
/** This functions returns the maximum size of packets (path MTU) that can be sent to a specific node with meshlink_send().
 *  The path MTU is a property of the path packets will take to the destination node over the Internet.
 *  It can be different for different destination nodes.
 *  and the path MTU can change at any point in time due to changes in the Internet.
 *  Therefore, although this should only occur rarely, it can still happen that packets that do not exceed this size get dropped.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param destination  A pointer to a meshlink_node_t describing the destination for the data.
 *
 *  @return             The recommended maximum size of packets that are to be sent to the destination node, 0 if the node is unreachable,
 *                      or a negative value in case of an error.
 */
extern ssize_t meshlink_get_pmtu(meshlink_handle_t *mesh, meshlink_node_t *destination);

/// Get a handle for a specific node.
/** This function returns a handle for the node with the given name.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param name         The name of the node for which a handle is requested.
 *                      After this function returns, the application is free to overwrite or free @a name @a.
 *
 *  @return             A pointer to a meshlink_node_t which represents the requested node,
 *                      or NULL if the requested node does not exist.
 *                      The pointer is guaranteed to be valid until meshlink_close() is called.
 */
extern meshlink_node_t *meshlink_get_node(meshlink_handle_t *mesh, const char *name);

/// Get the fingerprint of a node's public key.
/** This function returns a fingerprint of the node's public key.
 *  It should be treated as an opaque blob.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         A pointer to a meshlink_node_t describing the node.
 *
 *  @return             A nul-terminated C string containing the fingerprint of the node's public key in a printable ASCII format.
 *                      The application should call free() after it is done using this string.
 */
extern char *meshlink_get_fingerprint(meshlink_handle_t *mesh, meshlink_node_t *node);

/// Get a list of all nodes.
/** This function returns a list with handles for all known nodes.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param nodes        A pointer to a previously allocated array of pointers to meshlink_node_t, or NULL in which case MeshLink will allocate a new array.
 *                      The application can supply an array it allocated itself with malloc, or the return value from the previous call to this function (which is the preferred way).
 *                      The application is allowed to call free() on the array whenever it wishes.
 *                      The pointers in the array are valid until meshlink_close() is called.
 *  @param nmemb        A pointer to a variable holding the number of nodes that are stored in the array.
 *                      In case the @a nodes @a argument is not NULL, MeshLink might call realloc() on the array to change its size.
 *                      The contents of this variable will be changed to reflect the new size of the array.
 *
 *  @return             A pointer to an array containing pointers to all known nodes, or NULL in case of an error.
 *                      If the @a nodes @a argument was not NULL, then the return value can either be the same value or a different value.
 *                      If it is a new value, the old value of @a nodes @a should not be used anymore.
 *                      If the new value is NULL, then the old array will have been freed by MeshLink.
 */
extern meshlink_node_t **meshlink_get_all_nodes(meshlink_handle_t *mesh, meshlink_node_t **nodes, size_t *nmemb);

/// Sign data using the local node's MeshLink key.
/** This function signs data using the local node's MeshLink key.
 *  The generated signature can be securely verified by other nodes.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param data         A pointer to a buffer containing the data to be signed.
 *  @param len          The length of the data to be signed.
 *  @param signature    A pointer to a buffer where the signature will be stored.
 *                      The buffer must be allocated by the application, and should be at least MESHLINK_SIGLEN bytes big.
 *                      The signature is a binary blob, and is not nul-terminated.
 *  @param siglen       The size of the signature buffer. Will be changed after the call to match the size of the signature itself.
 *
 *  @return             This function returns true if the signature was correctly generated, false otherwise.
 */
extern bool meshlink_sign(meshlink_handle_t *mesh, const void *data, size_t len, void *signature, size_t *siglen);

/// Verify the signature generated by another node of a piece of data.
/** This function verifies the signature that another node generated for a piece of data.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param source       A pointer to a meshlink_node_t describing the source of the signature.
 *  @param data         A pointer to a buffer containing the data to be verified.
 *  @param len          The length of the data to be verified.
 *  @param signature    A pointer to a buffer where the signature is stored.
 *  @param siglen       A pointer to a variable holding the size of the signature buffer.
 *                      The contents of the variable will be changed by meshlink_sign() to reflect the actual size of the signature.
 *
 *  @return             This function returns true if the signature is valid, false otherwise.
 */
extern bool meshlink_verify(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len, const void *signature, size_t siglen);

/// Add an Address for the local node.
/** This function adds an Address for the local node, which will be used for invitation URLs.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param address      A nul-terminated C string containing the address, which can be either in numeric format or a hostname.
 *
 *  @return             This function returns true if the address was added, false otherwise.
 */
extern bool meshlink_add_address(meshlink_handle_t *mesh, const char *address);

/// Invite another node into the mesh.
/** This function generates an invitation that can be used by another node to join the same mesh as the local node.
 *  The generated invitation is a string containing a URL.
 *  This URL should be passed by the application to the invitee in a way that no eavesdroppers can see the URL.
 *  The URL can only be used once, after the user has joined the mesh the URL is no longer valid.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param name         A nul-terminated C string containing the name that the invitee will be allowed to use in the mesh.
 *                      After this function returns, the application is free to overwrite or free @a name @a.
 *
 *  @return             This function returns a nul-terminated C string that contains the invitation URL, or NULL in case of an error.
 *                      The application should call free() after it has finished using the URL.
 */
extern char *meshlink_invite(meshlink_handle_t *mesh, const char *name);

/// Use an invitation to join a mesh.
/** This function allows the local node to join an existing mesh using an invitation URL generated by another node.
 *  An invitation can only be used if the local node has never connected to other nodes before.
 *  After a succesfully accepted invitation, the name of the local node may have changed.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param invitation   A nul-terminated C string containing the invitation URL.
 *                      After this function returns, the application is free to overwrite or free @a invitation @a.
 *
 *  @return             This function returns true if the local node joined the mesh it was invited to, false otherwise.
 */
extern bool meshlink_join(meshlink_handle_t *mesh, const char *invitation);

/// Export the local node's key and addresses.
/** This function generates a string that contains the local node's public key and one or more IP addresses.
 *  The application can pass it in some way to another node, which can then import it,
 *  granting the local node access to the other node's mesh.
 *  The exported data does not contain any secret keys, it is therefore safe to transmit this data unencrypted over public networks.
 *
 *  Note that to create a working connection between two nodes, both must call meshink_export() and both must meshlink_import() each other's data.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *
 *  @return             This function returns a nul-terminated C string that contains the exported key and addresses, or NULL in case of an error.
 *                      The application should call free() after it has finished using this string.
 */
extern char *meshlink_export(meshlink_handle_t *mesh);

/// Import another node's key and addresses.
/** This function accepts a string containing the exported public key and addresses of another node.
 *  By importing this data, the local node grants the other node access to its mesh.
 *  The application should make sure that the data it imports is really coming from the node it wants to import,
 *
 *  Note that to create a working connection between two nodes, both must call meshink_export() and both must meshlink_import() each other's data.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param data         A nul-terminated C string containing the other node's exported key and addresses.
 *                      After this function returns, the application is free to overwrite or free @a data @a.
 *
 *  @return             This function returns true if the data was valid and the other node has been granted access to the mesh, false otherwise.
 */
extern bool meshlink_import(meshlink_handle_t *mesh, const char *data);

/// Blacklist a node from the mesh.
/** This function causes the local node to blacklist another node.
 *  The local node will drop any existing connections to that node,
 *  and will not send data to it nor accept any data received from it any more.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         A pointer to a meshlink_node_t describing the node to be blacklisted.
 */
extern void meshlink_blacklist(meshlink_handle_t *mesh, meshlink_node_t *node);

/// Whitelist a node on the mesh.
/** This function causes the local node to whitelist a previously blacklisted node.
 *  The local node will allow connections to and from that node,
 *  and will send data to it and accept any data received from it.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         A pointer to a meshlink_node_t describing the node to be blacklisted.
 */
extern void meshlink_blacklist(meshlink_handle_t *mesh, meshlink_node_t *node);

/// A callback for accepting incoming channels.
/** This function is called whenever a remote node wants to open a channel to the local node.
 *  The application then has to decide whether to accept or reject this channel.
 *
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback return quickly and uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand any data over to the application's thread.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the incoming channel.
 *                      If the application accepts the incoming channel by returning true,
 *                      then this handle is valid until meshlink_channel_close() is called on it.
 *                      If the application rejects the incoming channel by returning false,
 *                      then this handle is invalid after the callback returns
 *                      (the callback does not need to call meshlink_channel_close() itself in this case).
 *  @param port         The port number the peer wishes to connect to.
 *  @param data         A pointer to a buffer containing data already received, or NULL in case no data has been received yet. (Not yet used.)
 *                      The pointer is only valid during the lifetime of the callback.
 *                      The callback should mempcy() the data if it needs to be available outside the callback.
 *  @param len          The length of the data, or 0 in case no data has been received yet. (Not yet used.)
 *
 *  @return             This function should return true if the application accepts the incoming channel, false otherwise.
 *                      If returning false, the channel is invalid and may not be used anymore.
 */
typedef bool (*meshlink_channel_accept_cb_t)(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len);

/// A callback for receiving data from a channel.
/** This function is called whenever data is received from a remote node on a channel.
 *
 *  This function is also called in case the channel has been closed by the remote node, or when the channel is terminated abnormally.
 *  In both cases, @a data @a will be NULL and @a len @a will be 0, and meshlink_errno will be set.
 *  In any case, the @a channel @a handle will still be valid until the application calls meshlink_close().
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *  @param data         A pointer to a buffer containing data sent by the source, or NULL in case of an error.
 *                      The pointer is only valid during the lifetime of the callback.
 *                      The callback should mempcy() the data if it needs to be available outside the callback.
 *  @param len          The length of the data, or 0 in case of an error.
 */
typedef void (*meshlink_channel_receive_cb_t)(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len);

/// A callback informing the application when data can be sent on a channel.
/** This function is called whenever there is enough free buffer space so a call to meshlink_channel_send() will succeed.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *  @param len          The maximum amount of data that is guaranteed to be accepted by meshlink_channel_send().
 */
typedef void (*meshlink_channel_poll_cb_t)(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len);

/// Set the accept callback.
/** This functions sets the callback that is called whenever another node sends data to the local node.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  If no accept callback is set, incoming channels are rejected.
 *
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param cb        A pointer to the function which will be called when another node sends data to the local node.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
extern void meshlink_set_channel_accept_cb(meshlink_handle_t *mesh, meshlink_channel_accept_cb_t cb);

/// Set the receive callback.
/** This functions sets the callback that is called whenever another node sends data to the local node.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param channel   A handle for the channel.
 *  @param cb        A pointer to the function which will be called when another node sends data to the local node.
 *                   If a NULL pointer is given, the callback will be disabled and incoming data is ignored.
 */
extern void meshlink_set_channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, meshlink_channel_receive_cb_t cb);

/// Set the poll callback.
/** This functions sets the callback that is called whenever data can be sent to another node.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to pass data to or from the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param channel   A handle for the channel.
 *  @param cb        A pointer to the function which will be called when data can be sent to another node.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
extern void meshlink_set_channel_poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, meshlink_channel_poll_cb_t cb);

/// Open a reliable stream channel to another node.
/** This function is called whenever a remote node wants to open a channel to the local node.
 *  The application then has to decide whether to accept or reject this channel.
 *
 *  This function returns a pointer to a struct meshlink_channel that will be allocated by MeshLink.
 *  When the application does no longer need to use this channel, it must call meshlink_close()
 *  to free its resources.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         The node to which this channel is being initiated.
 *  @param port         The port number the peer wishes to connect to.
 *  @param cb           A pointer to the function which will be called when the remote node sends data to the local node.
 *                      The pointer may be NULL, in which case incoming data is ignored.
 *  @param data         A pointer to a buffer containing data to already queue for sending, or NULL if there is no data to send.
 *                      After meshlink_send() returns, the application is free to overwrite or free this buffer.
 *  @param len          The length of the data, or 0 if there is no data to send.
 *
 *  @return             A handle for the channel, or NULL in case of an error.
 *                      The handle is valid until meshlink_channel_close() is called.
 */
extern meshlink_channel_t *meshlink_channel_open(meshlink_handle_t *mesh, meshlink_node_t *node, uint16_t port, meshlink_channel_receive_cb_t cb, const void *data, size_t len);

/// Partially close a reliable stream channel.
/** This shuts down the read or write side of a channel, or both, without closing the handle.
 *  It can be used to inform the remote node that the local node has finished sending all data on the channel,
 *  but still allows waiting for incoming data from the remote node.
 *
 *  Shutting down the receive direction is also possible, and is equivalent to setting the receive callback to NULL.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *  @param direction    Must be one of SHUT_RD, SHUT_WR or SHUT_RDWR, otherwise this call will not have any affect.
 */
extern void meshlink_channel_shutdown(meshlink_handle_t *mesh, meshlink_channel_t *channel, int direction);

/// Close a reliable stream channel.
/** This informs the remote node that the local node has finished sending all data on the channel.
 *  It also causes the local node to stop accepting incoming data from the remote node.
 *  It will free the struct meshlink_channel and all associated resources.
 *  Afterwards, the channel handle is invalid and must not be used any more.
 *
 *  It is allowed to call this function at any time on a valid handle, even inside callback functions.
 *  If called with a valid handle, this function always succeeds, otherwise the result is undefined.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 */
extern void meshlink_channel_close(meshlink_handle_t *mesh, meshlink_channel_t *channel);

/// Transmit data on a channel
/** This queues data to send to the remote node.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *  @param data         A pointer to a buffer containing data sent by the source, or NULL if there is no data to send.
 *                      After meshlink_send() returns, the application is free to overwrite or free this buffer.
 *  @param len          The length of the data, or 0 if there is no data to send.
 *
 *  @return             The amount of data that was queued, which can be less than len, or a negative value in case of an error.
 */
extern ssize_t meshlink_channel_send(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len);

/// Hint that a hostname may be found at an address
/** This function indicates to meshlink that the given hostname is likely found
 *  at the given IP address and port.
 *
 *  @param mesh     A handle which represents an instance of MeshLink.
 *  @param hostname The hostname which can be found at the given address.
 *                  The caller is free to overwrite or free this string
 *                  once meshlink returns.
 *  @param addr     The IP address and port which should be tried for the
 *                  given hostname. The caller is free to overwrite or free
 *                  this memory once meshlink returns.
 */
extern void meshlink_hint_address(meshlink_handle_t *mesh, meshlink_node_t *node, const struct sockaddr *addr);

/// Get a list of edges.
/** This function returns an array with copies of all known bidirectional edges.
 *  The edges are copied to capture the mesh state at call time, since edges
 *  mutate frequently. The nodes pointed to within the meshlink_edge_t type
 *  are not copies; these are the same pointers that one would get from a call
 *  to meshlink_get_all_nodes().
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param edges        A pointer to a previously allocated array of pointers to
 *                      meshlink_edge_t, or NULL in which case MeshLink will
 *                      allocate a new array. The application CANNOT supply an
 *                      array it allocated itself with malloc, but CAN use
 *                      the return value from the previous call to this function
 *                      (which is the preferred way).
 *                      The pointers in the array are valid until meshlink_close() is called.
 *                      ATTENTION: The pointers and values should never be modified
 *                      by the application!!!
 *  @param nmemb        A pointer to a variable holding the number of nodes that
 *                      are stored in the array. In case the @a nodes @a
 *                      argument is not NULL, MeshLink might call realloc()
 *                      on the array to change its size.
 *                      The contents of this variable will be changed to reflect
 *                      the new size of the array.
 *  @return             A pointer to an array containing pointers to all known 
 *                      edges, or NULL in case of an error.
 *                      If the @a edges @a argument was not NULL, then the
 *                      retun value can be either the same value or a different
 *                      value. If the new values is NULL, then the old array
 *                      will have been freed by Meshlink.
 *                      The caller must call free() on each element of this
 *                      array (but not the contents of said elements),
 *                      as well as the array itself when it is finished.
 *                      ATTENTION: The pointers and values should never be modified
 *                      by the application!!!
 */
extern meshlink_edge_t **meshlink_get_all_edges_state(meshlink_handle_t *mesh, meshlink_edge_t **edges, size_t *nmemb);

#ifdef __cplusplus
}
#endif

#endif // MESHLINK_H
