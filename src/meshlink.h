#ifndef MESHLINK_H
#define MESHLINK_H

/*
    meshlink.h -- MeshLink API
    Copyright (C) 2014-2021 Guus Sliepen <guus@meshlink.io>

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
#define MESHLINK_SIGLEN (64ul)

// The maximum length of fingerprints
#define MESHLINK_FINGERPRINTLEN (64ul)

/// A handle for an instance of MeshLink.
typedef struct meshlink_handle meshlink_handle_t;

/// A handle for a MeshLink node.
typedef struct meshlink_node meshlink_node_t;

/// A handle for a MeshLink channel.
typedef struct meshlink_channel meshlink_channel_t;

/// A struct containing all parameters used for opening a mesh.
typedef struct meshlink_open_params meshlink_open_params_t;

/// A handle for a MeshLink sub-mesh.
typedef struct meshlink_submesh meshlink_submesh_t;

/// Code of most recent error encountered.
typedef enum {
	MESHLINK_OK,           ///< Everything is fine
	MESHLINK_EINVAL,       ///< Invalid parameter(s) to function call
	MESHLINK_ENOMEM,       ///< Out of memory
	MESHLINK_ENOENT,       ///< Node is not known
	MESHLINK_EEXIST,       ///< Node already exists
	MESHLINK_EINTERNAL,    ///< MeshLink internal error
	MESHLINK_ERESOLV,      ///< MeshLink could not resolve a hostname
	MESHLINK_ESTORAGE,     ///< MeshLink could not load or write data from/to disk
	MESHLINK_ENETWORK,     ///< MeshLink encountered a network error
	MESHLINK_EPEER,        ///< A peer caused an error
	MESHLINK_ENOTSUP,      ///< The operation is not supported in the current configuration of MeshLink
	MESHLINK_EBUSY,        ///< The MeshLink instance is already in use by another process
	MESHLINK_EBLACKLISTED  ///< The operation is not allowed because the node is blacklisted
} meshlink_errno_t;

/// Device class
typedef enum {
	DEV_CLASS_BACKBONE = 0,
	DEV_CLASS_STATIONARY = 1,
	DEV_CLASS_PORTABLE = 2,
	DEV_CLASS_UNKNOWN = 3,
	DEV_CLASS_COUNT
} dev_class_t;

/// Storage policy
typedef enum {
	MESHLINK_STORAGE_ENABLED,    ///< Store all updates.
	MESHLINK_STORAGE_DISABLED,   ///< Don't store any updates.
	MESHLINK_STORAGE_KEYS_ONLY   ///< Only store updates when a node's key has changed.
} meshlink_storage_policy_t;

/// Invitation flags
static const uint32_t MESHLINK_INVITE_LOCAL = 1;    // Only use local addresses in the URL
static const uint32_t MESHLINK_INVITE_PUBLIC = 2;   // Only use public or canonical addresses in the URL
static const uint32_t MESHLINK_INVITE_IPV4 = 4;     // Only use IPv4 addresses in the URL
static const uint32_t MESHLINK_INVITE_IPV6 = 8;     // Only use IPv6 addresses in the URL
static const uint32_t MESHLINK_INVITE_NUMERIC = 16; // Don't look up hostnames

/// Channel flags
static const uint32_t MESHLINK_CHANNEL_RELIABLE = 1;   // Data is retransmitted when packets are lost.
static const uint32_t MESHLINK_CHANNEL_ORDERED = 2;    // Data is delivered in-order to the application.
static const uint32_t MESHLINK_CHANNEL_FRAMED = 4;     // Data is delivered in chunks of the same length as data was originally sent.
static const uint32_t MESHLINK_CHANNEL_DROP_LATE = 8;  // When packets are reordered, late packets are ignored.
static const uint32_t MESHLINK_CHANNEL_NO_PARTIAL = 16; // Calls to meshlink_channel_send() will either send all data or nothing.
static const uint32_t MESHLINK_CHANNEL_TCP = 3;        // Select TCP semantics.
static const uint32_t MESHLINK_CHANNEL_UDP = 0;        // Select UDP semantics.

/// A variable holding the last encountered error from MeshLink.
/** This is a thread local variable that contains the error code of the most recent error
 *  encountered by a MeshLink API function called in the current thread.
 *  The variable is only updated when an error is encountered, and is not reset to MESHLINK_OK
 *  if a function returned successfully.
 */
extern __thread meshlink_errno_t meshlink_errno;

#ifndef MESHLINK_INTERNAL_H

struct meshlink_handle {
	const char *const name; ///< Textual name of ourself. It is stored in a nul-terminated C string, which is allocated by MeshLink.
	void *priv;             ///< Private pointer which may be set freely by the application, and is never used or modified by MeshLink.
};

struct meshlink_node {
	const char *const name; ///< Textual name of this node. It is stored in a nul-terminated C string, which is allocated by MeshLink.
	void *priv;             ///< Private pointer which may be set freely by the application, and is never used or modified by MeshLink.
};

struct meshlink_submesh {
	const char *const name; ///< Textual name of this Sub-Mesh. It is stored in a nul-terminated C string, which is allocated by MeshLink.
	void *priv;             ///< Private pointer which may be set freely by the application, and is never used or modified by MeshLink.
};

struct meshlink_channel {
	struct meshlink_node *const node; ///< Pointer to the peer of this channel.
	void *priv;                       ///< Private pointer which may be set freely by the application, and is never used or modified by MeshLink.
};

#endif // MESHLINK_INTERNAL_H

/// Get the text for the given MeshLink error code.
/** This function returns a pointer to the string containing the description of the given error code.
 *
 *  @param err      An error code returned by MeshLink.
 *
 *  @return         A pointer to a string containing the description of the error code.
 *                  The pointer is to static storage that is valid for the lifetime of the application.
 *                  This function will always return a valid pointer, even if an invalid error code has been passed.
 */
const char *meshlink_strerror(meshlink_errno_t err) __attribute__((__warn_unused_result__));

/// Create a new meshlink_open_params_t struct.
/** This function allocates and initializes a new meshlink_open_params_t struct that can be passed to meshlink_open_ex().
 *  The resulting struct may be reused for multiple calls to meshlink_open_ex().
 *  After the last use, the application must free this struct using meshlink_open_params_free().
 *
 *  @param confbase The directory in which MeshLink will store its configuration files.
 *                  After the function returns, the application is free to overwrite or free @a confbase.
 *  @param name     The name which this instance of the application will use in the mesh.
 *                  After the function returns, the application is free to overwrite or free @a name.
 *                  If NULL is passed as the name, the name used last time the MeshLink instance was initialized is used.
 *  @param appname  The application name which will be used in the mesh.
 *                  After the function returns, the application is free to overwrite or free @a name.
 *  @param devclass The device class which will be used in the mesh.
 *
 *  @return         A pointer to a meshlink_open_params_t which can be passed to meshlink_open_ex(), or NULL in case of an error.
 *                  The pointer is valid until meshlink_open_params_free() is called.
 */
meshlink_open_params_t *meshlink_open_params_init(const char *confbase, const char *name, const char *appname, dev_class_t devclass) __attribute__((__warn_unused_result__));

/// Free a meshlink_open_params_t struct.
/** This function frees a meshlink_open_params_t struct and all resources associated with it.
 *
 *  @param params   A pointer to a meshlink_open_params_t which must have been created earlier with meshlink_open_params_init().
 */
void meshlink_open_params_free(meshlink_open_params_t *params);

/// Set the network namespace MeshLink should use.
/** This function changes the open parameters to use the given netns filedescriptor.
 *
 *  @param params   A pointer to a meshlink_open_params_t which must have been created earlier with meshlink_open_params_init().
 *  @param netns    A filedescriptor that must point to a valid network namespace, or -1 to have MeshLink use the same namespace as the calling thread.
 *
 *  @return         This function will return true if the open parameters have been successfully updated, false otherwise.
 */
bool meshlink_open_params_set_netns(meshlink_open_params_t *params, int netns) __attribute__((__warn_unused_result__));

/// Set the encryption key MeshLink should use for local storage.
/** This function changes the open parameters to use the given key for encrypting MeshLink's own configuration files.
 *
 *  @param params   A pointer to a meshlink_open_params_t which must have been created earlier with meshlink_open_params_init().
 *  @param key      A pointer to a key, or NULL in case no encryption should be used.
 *  @param keylen   The length of the given key, or 0 in case no encryption should be used.
 *
 *  @return         This function will return true if the open parameters have been successfully updated, false otherwise.
 */
bool meshlink_open_params_set_storage_key(meshlink_open_params_t *params, const void *key, size_t keylen) __attribute__((__warn_unused_result__));

/// Set the encryption key MeshLink should use for local storage.
/** This function changes the open parameters to use the given storage policy.
 *
 *  @param params   A pointer to a meshlink_open_params_t which must have been created earlier with meshlink_open_params_init().
 *  @param policy   The storage policy to use.
 *
 *  @return         This function will return true if the open parameters have been successfully updated, false otherwise.
 */
bool meshlink_open_params_set_storage_policy(meshlink_open_params_t *params, meshlink_storage_policy_t policy) __attribute__((__warn_unused_result__));

/// Set the filename of the lockfile.
/** This function changes the path of the lockfile used to ensure only one instance of MeshLink can be open at the same time.
 *  If an application changes this, it must always set it to the same location.
 *
 *  @param params   A pointer to a meshlink_open_params_t which must have been created earlier with meshlink_open_params_init().
 *  @param filename The filename of the lockfile.
 *
 *  @return         This function will return true if the open parameters have been successfully updated, false otherwise.
 */
bool meshlink_open_params_set_lock_filename(meshlink_open_params_t *params, const char *filename) __attribute__((__warn_unused_result__));

/// Open or create a MeshLink instance.
/** This function opens or creates a MeshLink instance.
 *  All parameters needed by MeshLink are passed via a meshlink_open_params_t struct,
 *  which must have been initialized earlier by the application.
 *
 *  This function returns a pointer to a struct meshlink_handle that will be allocated by MeshLink.
 *  When the application does no longer need to use this handle, it must call meshlink_close() to
 *  free its resources.
 *
 *  This function does not start any network I/O yet. The application should
 *  first set callbacks, and then call meshlink_start().
 *
 *  @param params   A pointer to a meshlink_open_params_t which must be filled in by the application.
 *                  After the function returns, the application is free to reuse or free @a params.
 *
 *  @return         A pointer to a struct meshlink_handle which represents this instance of MeshLink, or NULL in case of an error.
 *                  The pointer is valid until meshlink_close() is called.
 */
struct meshlink_handle *meshlink_open_ex(const meshlink_open_params_t *params) __attribute__((__warn_unused_result__));

/// Open or create a MeshLink instance.
/** This function opens or creates a MeshLink instance.
 *  The state is stored in the configuration directory passed in the variable @a confbase.
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
 *                  After the function returns, the application is free to overwrite or free @a confbase.
 *  @param name     The name which this instance of the application will use in the mesh.
 *                  After the function returns, the application is free to overwrite or free @a name.
 *                  If NULL is passed as the name, the name used last time the MeshLink instance was initialized is used.
 *  @param appname  The application name which will be used in the mesh.
 *                  After the function returns, the application is free to overwrite or free @a name.
 *  @param devclass The device class which will be used in the mesh.
 *
 *  @return         A pointer to a struct meshlink_handle which represents this instance of MeshLink, or NULL in case of an error.
 *                  The pointer is valid until meshlink_close() is called.
 */
struct meshlink_handle *meshlink_open(const char *confbase, const char *name, const char *appname, dev_class_t devclass) __attribute__((__warn_unused_result__));

/// Open or create a MeshLink instance that uses encrypted storage.
/** This function opens or creates a MeshLink instance.
 *  The state is stored in the configuration directory passed in the variable @a confbase.
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
 *                  After the function returns, the application is free to overwrite or free @a confbase.
 *  @param name     The name which this instance of the application will use in the mesh.
 *                  After the function returns, the application is free to overwrite or free @a name.
 *                  If NULL is passed as the name, the name used last time the MeshLink instance was initialized is used.
 *  @param appname  The application name which will be used in the mesh.
 *                  After the function returns, the application is free to overwrite or free @a name.
 *  @param devclass The device class which will be used in the mesh.
 *  @param key      A pointer to a key used to encrypt storage.
 *  @param keylen   The length of the key in bytes.
 *
 *  @return         A pointer to a struct meshlink_handle which represents this instance of MeshLink, or NULL in case of an error.
 *                  The pointer is valid until meshlink_close() is called.
 */
struct meshlink_handle *meshlink_open_encrypted(const char *confbase, const char *name, const char *appname, dev_class_t devclass, const void *key, size_t keylen) __attribute__((__warn_unused_result__));

/// Create an ephemeral MeshLink instance that does not store any state.
/** This function creates a MeshLink instance.
 *  No state is ever saved, so once this instance is closed, all its state is gone.
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
 *  @param name     The name which this instance of the application will use in the mesh.
 *                  After the function returns, the application is free to overwrite or free @a name.
 *  @param appname  The application name which will be used in the mesh.
 *                  After the function returns, the application is free to overwrite or free @a name.
 *  @param devclass The device class which will be used in the mesh.
 *
 *  @return         A pointer to a struct meshlink_handle which represents this instance of MeshLink, or NULL in case of an error.
 *                  The pointer is valid until meshlink_close() is called.
 */
struct meshlink_handle *meshlink_open_ephemeral(const char *name, const char *appname, dev_class_t devclass) __attribute__((__warn_unused_result__));

/// Create Sub-Mesh.
/** This function causes MeshLink to open a new Sub-Mesh network
 *  create a new thread, which will handle all network I/O.
 *
 *  It is allowed to call this function even if MeshLink is already started, in which case it will return true.
 *
 *  \memberof meshlink_handle
 *  @param mesh     A handle which represents an instance of MeshLink.
 *
 *  @param submesh  Name of the new Sub-Mesh to create.
 *
 *  @return         A pointer to a struct meshlink_submesh which represents this instance of SubMesh, or NULL in case of an error.
 *                  The pointer is valid until meshlink_close() is called.
 */
struct meshlink_submesh *meshlink_submesh_open(struct meshlink_handle *mesh, const char *submesh) __attribute__((__warn_unused_result__));

/// Start MeshLink.
/** This function causes MeshLink to open network sockets, make outgoing connections, and
 *  create a new thread, which will handle all network I/O.
 *
 *  It is allowed to call this function even if MeshLink is already started, in which case it will return true.
 *
 *  \memberof meshlink_handle
 *  @param mesh     A handle which represents an instance of MeshLink.
 *
 *  @return         This function will return true if MeshLink has successfully started, false otherwise.
 */
bool meshlink_start(struct meshlink_handle *mesh) __attribute__((__warn_unused_result__));

/// Stop MeshLink.
/** This function causes MeshLink to disconnect from all other nodes,
 *  close all sockets, and shut down its own thread.
 *
 *  This function always succeeds. It is allowed to call meshlink_stop() even if MeshLink is already stopped or has never been started.
 *  Channels that are still open will remain valid, but any communication via channels will stop as well.
 *
 *  \memberof meshlink_handle
 *  @param mesh     A handle which represents an instance of MeshLink.
 */
void meshlink_stop(struct meshlink_handle *mesh);

/// Close the MeshLink handle.
/** This function calls meshlink_stop() if necessary,
 *  and frees the struct meshlink_handle and all associacted memory allocated by MeshLink, including all channels.
 *  Afterwards, the handle and any pointers to a struct meshlink_node or struct meshlink_channel are invalid.
 *
 *  It is allowed to call this function at any time on a valid handle, except inside callback functions.
 *  If called at a proper time with a valid handle, this function always succeeds.
 *  If called within a callback or with an invalid handle, the result is undefined.
 *
 * \memberof meshlink_handle
 *  @param mesh     A handle which represents an instance of MeshLink.
 */
void meshlink_close(struct meshlink_handle *mesh);

/// Destroy a MeshLink instance.
/** This function remove all configuration files of a MeshLink instance. It should only be called when the application
 *  does not have an open handle to this instance. Afterwards, a call to meshlink_open() will create a completely
 *  new instance.
 *
 *  @param confbase The directory in which MeshLink stores its configuration files.
 *                  After the function returns, the application is free to overwrite or free @a confbase.
 *
 *  @return         This function will return true if the MeshLink instance was successfully destroyed, false otherwise.
 */
bool meshlink_destroy(const char *confbase) __attribute__((__warn_unused_result__));

/// Destroy a MeshLink instance using open parameters.
/** This function remove all configuration files of a MeshLink instance. It should only be called when the application
 *  does not have an open handle to this instance. Afterwards, a call to meshlink_open() will create a completely
 *  new instance.
 *
 *  This version expects a pointer to meshlink_open_params_t,
 *  and will use exactly the same settings used for opening a handle to destroy it.
 *
 *  @param params   A pointer to a meshlink_open_params_t which must be filled in by the application.
 *                  After the function returns, the application is free to reuse or free @a params.
 *
 *  @return         This function will return true if the MeshLink instance was successfully destroyed, false otherwise.
 */
bool meshlink_destroy_ex(const meshlink_open_params_t *params) __attribute__((__warn_unused_result__));

/// A callback for receiving data from the mesh.
/** @param mesh      A handle which represents an instance of MeshLink.
 *  @param source    A pointer to a struct meshlink_node describing the source of the data.
 *  @param data      A pointer to a buffer containing the data sent by the source, or NULL in case there is no data (an empty packet was received).
 *                   The pointer is only valid during the lifetime of the callback.
 *                   The callback should mempcy() the data if it needs to be available outside the callback.
 *  @param len       The length of the received data, or 0 in case there is no data.
 */
typedef void (*meshlink_receive_cb_t)(struct meshlink_handle *mesh, struct meshlink_node *source, const void *data, size_t len);

/// Set the receive callback.
/** This functions sets the callback that is called whenever another node sends data to the local node.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  \memberof meshlink_handle
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param cb        A pointer to the function which will be called when another node sends data to the local node.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
void meshlink_set_receive_cb(struct meshlink_handle *mesh, meshlink_receive_cb_t cb);

/// A callback reporting the meta-connection attempt made by the host node to an another node.
/** @param mesh      A handle which represents an instance of MeshLink.
 *  @param node      A pointer to a struct meshlink_node describing the node to whom meta-connection is being tried.
 *                   This pointer is valid until meshlink_close() is called.
 */
typedef void (*meshlink_connection_try_cb_t)(struct meshlink_handle *mesh, struct meshlink_node *node);

/// Set the meta-connection try callback.
/** This functions sets the callback that is called whenever a connection attempt is happened to another node.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  \memberof meshlink_handle
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param cb        A pointer to the function which will be called when host node attempts to make
 *                   the connection to another node. If a NULL pointer is given, the callback will be disabled.
 */
void meshlink_set_connection_try_cb(struct meshlink_handle *mesh, meshlink_connection_try_cb_t cb);

/// A callback reporting node status changes.
/** @param mesh      A handle which represents an instance of MeshLink.
 *  @param node       A pointer to a struct meshlink_node describing the node whose status changed.
 *                    This pointer is valid until meshlink_close() is called.
 *  @param reachable  True if the node is reachable, false otherwise.
 */
typedef void (*meshlink_node_status_cb_t)(struct meshlink_handle *mesh, struct meshlink_node *node, bool reachable);

/// Set the node status callback.
/** This functions sets the callback that is called whenever another node's status changed.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  \memberof meshlink_handle
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param cb        A pointer to the function which will be called when another node's status changes.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
void meshlink_set_node_status_cb(struct meshlink_handle *mesh, meshlink_node_status_cb_t cb);

/// A callback reporting node path MTU changes.
/** @param mesh      A handle which represents an instance of MeshLink.
 *  @param node       A pointer to a struct meshlink_node describing the node whose status changed.
 *                    This pointer is valid until meshlink_close() is called.
 *  @param pmtu       The current path MTU to the node, or 0 if UDP communication is not (yet) possible.
 */
typedef void (*meshlink_node_pmtu_cb_t)(struct meshlink_handle *mesh, struct meshlink_node *node, uint16_t pmtu);

/// Set the node extended status callback.
/** This functions sets the callback that is called whenever certain connectivity parameters for a node change.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  \memberof meshlink_handle
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param cb        A pointer to the function which will be called when another node's extended status changes.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
void meshlink_set_node_pmtu_cb(struct meshlink_handle *mesh, meshlink_node_pmtu_cb_t cb);

/// A callback reporting duplicate node detection.
/** @param mesh       A handle which represents an instance of MeshLink.
 *  @param node       A pointer to a struct meshlink_node describing the node which is duplicate.
 *                    This pointer is valid until meshlink_close() is called.
 */
typedef void (*meshlink_node_duplicate_cb_t)(struct meshlink_handle *mesh, struct meshlink_node *node);

/// Set the node duplicate callback.
/** This functions sets the callback that is called whenever a duplicate node is detected.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  \memberof meshlink_handle
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param cb        A pointer to the function which will be called when a duplicate node is detected.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
void meshlink_set_node_duplicate_cb(struct meshlink_handle *mesh, meshlink_node_duplicate_cb_t cb);

/// Severity of log messages generated by MeshLink.
typedef enum {
	MESHLINK_DEBUG,    ///< Internal debugging messages. Only useful during application development.
	MESHLINK_INFO,     ///< Informational messages.
	MESHLINK_WARNING,  ///< Warnings which might indicate problems, but which are not real errors.
	MESHLINK_ERROR,    ///< Errors which hamper correct functioning of MeshLink, without causing it to fail completely.
	MESHLINK_CRITICAL  ///< Critical errors which cause MeshLink to fail completely.
} meshlink_log_level_t;

/// A callback for receiving log messages generated by MeshLink.
/** @param mesh      A handle which represents an instance of MeshLink, or NULL.
 *  @param level     An enum describing the severity level of the message.
 *  @param text      A pointer to a nul-terminated C string containing the textual log message.
 *                   This pointer is only valid for the duration of the callback.
 *                   The application must not free() this pointer.
 *                   The application should strdup() the text if it has to be available outside the callback.
 */
typedef void (*meshlink_log_cb_t)(struct meshlink_handle *mesh, meshlink_log_level_t level, const char *text);

/// Set the log callback.
/** This functions sets the callback that is called whenever MeshLink has some information to log.
 *
 *  The @a mesh parameter can either be a valid MeshLink handle, or NULL.
 *  In case it is NULL, the callback will be called for errors that happen outside the context of a valid mesh instance.
 *  Otherwise, it will be called for errors that happen in the context of the given mesh instance.
 *
 *  If @a mesh is not NULL, then the callback is run in MeshLink's own thread.
 *  It is important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  The @a mesh parameter can either be a valid MeshLink handle, or NULL.
 *  In case it is NULL, the callback will be called for errors that happen outside the context of a valid mesh instance.
 *  Otherwise, it will be called for errors that happen in the context of the given mesh instance.
 *
 *  \memberof meshlink_handle
 *  @param mesh      A handle which represents an instance of MeshLink, or NULL.
 *  @param level     An enum describing the minimum severity level. Debugging information with a lower level will not trigger the callback.
 *  @param cb        A pointer to the function which will be called when another node sends data to the local node.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
void meshlink_set_log_cb(struct meshlink_handle *mesh, meshlink_log_level_t level, meshlink_log_cb_t cb);

/// A callback for receiving error conditions encountered by the MeshLink thread.
/** @param mesh      A handle which represents an instance of MeshLink, or NULL.
 *  @param errno     The error code describing what kind of error occurred.
 */
typedef void (*meshlink_error_cb_t)(struct meshlink_handle *mesh, meshlink_errno_t meshlink_errno);

/// Set the error callback.
/** This functions sets the callback that is called whenever the MeshLink thread encounters a serious error.
 *
 *  While most API functions report an error directly to the caller in case something went wrong,
 *  MeshLink also runs a background thread which can encounter error conditions.
 *  Most of them will be dealt with automatically, however there can be errors that will prevent MeshLink from
 *  working correctly. When the callback is called, it means that MeshLink is no longer functioning
 *  as expected. The application should then present an error message and shut down, or perform any other
 *  action it deems appropriate.
 *
 *  The callback is run in MeshLink's own thread.
 *  It is important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  Even though the callback signals a serious error inside MeshLink, all open handles are still valid,
 *  and the application should close handles in exactly the same it would have to do if the callback
 *  was not called. This must not be done inside the callback itself.
 *
 *  \memberof meshlink_handle
 *  @param mesh      A handle which represents an instance of MeshLink, or NULL.
 *  @param cb        A pointer to the function which will be called when a serious error is encountered.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
void meshlink_set_error_cb(struct meshlink_handle *mesh, meshlink_error_cb_t cb);

/// A callback for receiving blacklisted conditions encountered by the MeshLink thread.
/** @param mesh      A handle which represents an instance of MeshLink, or NULL.
 *  @param node      The node that blacklisted the local node.
 */
typedef void (*meshlink_blacklisted_cb_t)(struct meshlink_handle *mesh, struct meshlink_node *node);

/// Set the blacklisted callback.
/** This functions sets the callback that is called whenever MeshLink detects that it is blacklisted by another node.
 *
 *  The callback is run in MeshLink's own thread.
 *  It is important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  \memberof meshlink_handle
 *  @param mesh      A handle which represents an instance of MeshLink, or NULL.
 *  @param cb        A pointer to the function which will be called when a serious error is encountered.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
void meshlink_set_blacklisted_cb(struct meshlink_handle *mesh, meshlink_blacklisted_cb_t cb);

/// A callback notifying when the MeshLink thread starts and stops.
/*  @param mesh      A handle which represents an instance of MeshLink, or NULL.
 *  @param started   True if the MeshLink thread has started, false if it is about to stop.
 */
typedef void (*meshlink_thread_status_cb_t)(struct meshlink_handle *mesh, bool started);

/// Set the thread status callback.
/** This functions sets the callback that is called whenever the MeshLink thread has started or is about to stop.
 *
 *  The callback is run in MeshLink's own thread.
 *  It is important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  \memberof meshlink_handle
 *  @param mesh      A handle which represents an instance of MeshLink, or NULL.
 *  @param cb        A pointer to the function which will be called when a serious error is encountered.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
void meshlink_set_thread_status_cb(struct meshlink_handle *mesh, meshlink_thread_status_cb_t cb);

/// Send data to another node.
/** This functions sends one packet of data to another node in the mesh.
 *  The packet is sent using UDP semantics, which means that
 *  the packet is sent as one unit and is received as one unit,
 *  and that there is no guarantee that the packet will arrive at the destination.
 *  Packets that are too big to be sent over the network as one unit might be dropped, and this function may return an error if this situation can be detected beforehand.
 *  The application should not send packets that are larger than the path MTU, which can be queried with meshlink_get_pmtu().
 *  The application should take care of getting an acknowledgement and retransmission if necessary.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param destination  A pointer to a struct meshlink_node describing the destination for the data.
 *  @param data         A pointer to a buffer containing the data to be sent to the source.
 *                      After meshlink_send() returns, the application is free to overwrite or free this buffer.
 *                      It is valid to specify a NULL pointer, but only if @a len is also 0.
 *  @param len          The length of the data.
 *  @return             This function will return true if MeshLink has queued the message for transmission, and false otherwise.
 *                      A return value of true does not guarantee that the message will actually arrive at the destination.
 */
bool meshlink_send(struct meshlink_handle *mesh, struct meshlink_node *destination, const void *data, size_t len) __attribute__((__warn_unused_result__));

/// Query the maximum packet size that can be sent to a node.
/** This functions returns the maximum size of packets (path MTU) that can be sent to a specific node with meshlink_send().
 *  The path MTU is a property of the path packets will take to the destination node over the Internet.
 *  It can be different for different destination nodes.
 *  and the path MTU can change at any point in time due to changes in the Internet.
 *  Therefore, although this should only occur rarely, it can still happen that packets that do not exceed this size get dropped.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param destination  A pointer to a struct meshlink_node describing the destination for the data.
 *
 *  @return             The recommended maximum size of packets that are to be sent to the destination node, 0 if the node is unreachable,
 *                      or a negative value in case of an error.
 */
ssize_t meshlink_get_pmtu(struct meshlink_handle *mesh, struct meshlink_node *destination) __attribute__((__warn_unused_result__));

/// Get a handle for our own node.
/** This function returns a handle for the local node.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *
 *  @return             A pointer to a struct meshlink_node which represents the local node.
 *                      The pointer is guaranteed to be valid until meshlink_close() is called.
 */
struct meshlink_node *meshlink_get_self(struct meshlink_handle *mesh) __attribute__((__warn_unused_result__));

/// Get a handle for a specific node.
/** This function returns a handle for the node with the given name.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param name         The name of the node for which a handle is requested.
 *                      After this function returns, the application is free to overwrite or free @a name.
 *
 *  @return             A pointer to a struct meshlink_node which represents the requested node,
 *                      or NULL if the requested node does not exist.
 *                      The pointer is guaranteed to be valid until meshlink_close() is called.
 */
struct meshlink_node *meshlink_get_node(struct meshlink_handle *mesh, const char *name) __attribute__((__warn_unused_result__));

/// Get a handle for a specific submesh.
/** This function returns a handle for the submesh with the given name.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param name         The name of the submesh for which a handle is requested.
 *                      After this function returns, the application is free to overwrite or free @a name.
 *
 *  @return             A pointer to a struct meshlink_submesh which represents the requested submesh,
 *                      or NULL if the requested submesh does not exist.
 *                      The pointer is guaranteed to be valid until meshlink_close() is called.
 */
struct meshlink_submesh *meshlink_get_submesh(struct meshlink_handle *mesh, const char *name) __attribute__((__warn_unused_result__));

/// Get the fingerprint of a node's public key.
/** This function returns a fingerprint of the node's public key.
 *  It should be treated as an opaque blob.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         A pointer to a struct meshlink_node describing the node.
 *
 *  @return             A nul-terminated C string containing the fingerprint of the node's public key in a printable ASCII format.
 *                      The application should call free() after it is done using this string.
 */
char *meshlink_get_fingerprint(struct meshlink_handle *mesh, struct meshlink_node *node) __attribute__((__warn_unused_result__));

/// Get a list of all nodes.
/** This function returns a list with handles for all known nodes.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param nodes        A pointer to a previously allocated array of pointers to struct meshlink_node, or NULL in which case MeshLink will allocate a new array.
 *                      The application can supply an array it allocated itself with malloc, or the return value from the previous call to this function (which is the preferred way).
 *                      The application is allowed to call free() on the array whenever it wishes.
 *                      The pointers in the array are valid until meshlink_close() is called.
 *  @param nmemb        A pointer to a variable holding the number of nodes that are stored in the array.
 *                      In case the @a nodes argument is not NULL, MeshLink might call realloc() on the array to change its size.
 *                      The contents of this variable will be changed to reflect the new size of the array.
 *
 *  @return             A pointer to an array containing pointers to all known nodes, or NULL in case of an error.
 *                      If the @a nodes argument was not NULL, then the return value can either be the same value or a different value.
 *                      If it is a new value, the old value of @a nodes should not be used anymore.
 *                      If the new value is NULL, then the old array will have been freed by MeshLink.
 */
struct meshlink_node **meshlink_get_all_nodes(struct meshlink_handle *mesh, struct meshlink_node **nodes, size_t *nmemb) __attribute__((__warn_unused_result__));

/// Sign data using the local node's MeshLink key.
/** This function signs data using the local node's MeshLink key.
 *  The generated signature can be securely verified by other nodes.
 *
 *  \memberof meshlink_handle
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
bool meshlink_sign(struct meshlink_handle *mesh, const void *data, size_t len, void *signature, size_t *siglen) __attribute__((__warn_unused_result__));

/// Get the list of all nodes by device class.
/** This function returns a list with handles for all the nodes that matches with the given @a devclass.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param devclass     Device class of the nodes for which the list has to be obtained.
 *  @param nodes        A pointer to a previously allocated array of pointers to struct meshlink_node, or NULL in which case MeshLink will allocate a new array.
 *                      The application can supply an array it allocated itself with malloc, or the return value from the previous call to this function (which is the preferred way).
 *                      The application is allowed to call free() on the array whenever it wishes.
 *                      The pointers in the array are valid until meshlink_close() is called.
 *  @param nmemb        A pointer to a variable holding the number of nodes with the same @a device class that are stored in the array.
 *                      In case the @a nodes argument is not NULL, MeshLink might call realloc() on the array to change its size.
 *                      The contents of this variable will be changed to reflect the new size of the array.
 *
 *  @return             A pointer to an array containing pointers to all known nodes of the given device class, or NULL in case of an error.
 *                      If the @a nodes argument was not NULL, then the return value can either be the same value or a different value.
 *                      If it is a new value, the old value of @a nodes should not be used anymore.
 *                      If the new value is NULL, then the old array will have been freed by MeshLink.
 */
struct meshlink_node **meshlink_get_all_nodes_by_dev_class(struct meshlink_handle *mesh, dev_class_t devclass, struct meshlink_node **nodes, size_t *nmemb) __attribute__((__warn_unused_result__));

/// Get the list of all nodes by Submesh.
/** This function returns a list with handles for all the nodes that matches with the given @a Submesh.
 *
 *  \memberof meshlink_submesh
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param submesh      Submesh handle of the nodes for which the list has to be obtained.
 *  @param nodes        A pointer to a previously allocated array of pointers to struct meshlink_node, or NULL in which case MeshLink will allocate a new array.
 *                      The application can supply an array it allocated itself with malloc, or the return value from the previous call to this function (which is the preferred way).
 *                      The application is allowed to call free() on the array whenever it wishes.
 *                      The pointers in the array are valid until meshlink_close() is called.
 *  @param nmemb        A pointer to a variable holding the number of nodes with the same @a device class that are stored in the array.
 *                      In case the @a nodes argument is not NULL, MeshLink might call realloc() on the array to change its size.
 *                      The contents of this variable will be changed to reflect the new size of the array.
 *
 *  @return             A pointer to an array containing pointers to all known nodes of the given Submesh, or NULL in case of an error.
 *                      If the @a nodes argument was not NULL, then the return value can either be the same value or a different value.
 *                      If it is a new value, the old value of @a nodes should not be used anymore.
 *                      If the new value is NULL, then the old array will have been freed by MeshLink.
 */
struct meshlink_node **meshlink_get_all_nodes_by_submesh(struct meshlink_handle *mesh, struct meshlink_submesh *submesh, struct meshlink_node **nodes, size_t *nmemb) __attribute__((__warn_unused_result__));

/// Get the list of all nodes by time they were last reachable.
/** This function returns a list with handles for all the nodes whose last known reachability time overlaps with the given time range.
 *  If the range includes 0, it will count nodes that were never online.
 *  If start is bigger than end, the result will be inverted.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param start        Start time.
 *  @param end          End time.
 *  @param nodes        A pointer to a previously allocated array of pointers to struct meshlink_node, or NULL in which case MeshLink will allocate a new array.
 *                      The application can supply an array it allocated itself with malloc, or the return value from the previous call to this function (which is the preferred way).
 *                      The application is allowed to call free() on the array whenever it wishes.
 *                      The pointers in the array are valid until meshlink_close() is called.
 *  @param nmemb        A pointer to a variable holding the number of nodes that were reachable within the period given by @a start and @a end.
 *                      In case the @a nodes argument is not NULL, MeshLink might call realloc() on the array to change its size.
 *                      The contents of this variable will be changed to reflect the new size of the array.
 *
 *  @return             A pointer to an array containing pointers to all known nodes that were reachable within the period given by @a start and @a end.
 *                      If the @a nodes argument was not NULL, then the return value can either be the same value or a different value.
 *                      If it is a new value, the old value of @a nodes should not be used anymore.
 *                      If the new value is NULL, then the old array will have been freed by MeshLink.
 */
struct meshlink_node **meshlink_get_all_nodes_by_last_reachable(struct meshlink_handle *mesh, time_t start, time_t end, struct meshlink_node **nodes, size_t *nmemb) __attribute__((__warn_unused_result__));

/// Get the list of all nodes by blacklist status.
/** This function returns a list with handles for all the nodes who were either blacklisted or whitelisted.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param blacklisted  If true, a list of blacklisted nodes will be returned, otherwise whitelisted nodes.
 *  @param nodes        A pointer to a previously allocated array of pointers to struct meshlink_node, or NULL in which case MeshLink will allocate a new array.
 *                      The application can supply an array it allocated itself with malloc, or the return value from the previous call to this function (which is the preferred way).
 *                      The application is allowed to call free() on the array whenever it wishes.
 *                      The pointers in the array are valid until meshlink_close() is called.
 *  @param nmemb        A pointer to a variable holding the number of nodes that were reachable within the period given by @a start and @a end.
 *                      In case the @a nodes argument is not NULL, MeshLink might call realloc() on the array to change its size.
 *                      The contents of this variable will be changed to reflect the new size of the array.
 *
 *  @return             A pointer to an array containing pointers to all known nodes with the given blacklist status.
 *                      If the @a nodes argument was not NULL, then the return value can either be the same value or a different value.
 *                      If it is a new value, the old value of @a nodes should not be used anymore.
 *                      If the new value is NULL, then the old array will have been freed by MeshLink.
 */
struct meshlink_node **meshlink_get_all_nodes_by_blacklisted(struct meshlink_handle *mesh, bool blacklisted, struct meshlink_node **nodes, size_t *nmemb) __attribute__((__warn_unused_result__));

/// Get the node's device class.
/** This function returns the device class of the given node.
 *
 *  \memberof meshlink_node
 *  @param mesh          A handle which represents an instance of MeshLink.
 *  @param node          A pointer to a struct meshlink_node describing the node.
 *
 *  @return              This function returns the device class of the @a node, or -1 in case of an error.
 */
dev_class_t meshlink_get_node_dev_class(struct meshlink_handle *mesh, struct meshlink_node *node) __attribute__((__warn_unused_result__));

/// Get the node's tiny status.
/** This function returns true if the given node is a tiny node.
 *  Note that the tiny status of a node will only be known if the node has been reachable at least once.
 *
 *  \memberof meshlink_node
 *  @param mesh          A handle which represents an instance of MeshLink.
 *  @param node          A pointer to a struct meshlink_node describing the node.
 *
 *  @return              This function returns true if the node is a tiny node.
 */
bool meshlink_get_node_tiny(struct meshlink_handle *mesh, struct meshlink_node *node) __attribute__((__warn_unused_result__));

/// Get the node's blacklist status.
/** This function returns the given node is blacklisted.
 *
 *  \memberof meshlink_node
 *  @param mesh          A handle which represents an instance of MeshLink.
 *  @param node          A pointer to a struct meshlink_node describing the node.
 *
 *  @return              This function returns true if the node is blacklisted, false otherwise.
 */
bool meshlink_get_node_blacklisted(struct meshlink_handle *mesh, struct meshlink_node *node) __attribute__((__warn_unused_result__));

/// Get the node's submesh handle.
/** This function returns the submesh handle of the given node.
 *
 *  \memberof meshlink_node
 *  @param mesh          A handle which represents an instance of MeshLink.
 *  @param node          A pointer to a struct meshlink_node describing the node.
 *
 *  @return              This function returns the submesh handle of the @a node, or NULL in case of an error.
 */
struct meshlink_submesh *meshlink_get_node_submesh(struct meshlink_handle *mesh, struct meshlink_node *node) __attribute__((__warn_unused_result__));

/// Get a node's reachability status.
/** This function returns the current reachability of a given node, and the times of the last state changes.
 *  If a given state change never happened, the time returned will be 0.
 *
 *  \memberof meshlink_node
 *  @param mesh              A handle which represents an instance of MeshLink.
 *  @param node              A pointer to a struct meshlink_node describing the node.
 *  @param last_reachable    A pointer to a time_t variable that will be filled in with the last time the node became reachable.
 *                           Pass NULL to not have anything written.
 *  @param last_unreachable  A pointer to a time_t variable that will be filled in with the last time the node became unreachable.
 *                           Pass NULL to not have anything written.
 *
 *  @return                  This function returns true if the node is currently reachable, false otherwise.
 */
bool meshlink_get_node_reachability(struct meshlink_handle *mesh, struct meshlink_node *node, time_t *last_reachable, time_t *last_unreachable);

/// Verify the signature generated by another node of a piece of data.
/** This function verifies the signature that another node generated for a piece of data.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param source       A pointer to a struct meshlink_node describing the source of the signature.
 *  @param data         A pointer to a buffer containing the data to be verified.
 *  @param len          The length of the data to be verified.
 *  @param signature    A pointer to a buffer where the signature is stored.
 *  @param siglen       A pointer to a variable holding the size of the signature buffer.
 *                      The contents of the variable will be changed by meshlink_sign() to reflect the actual size of the signature.
 *
 *  @return             This function returns true if the signature is valid, false otherwise.
 */
bool meshlink_verify(struct meshlink_handle *mesh, struct meshlink_node *source, const void *data, size_t len, const void *signature, size_t siglen) __attribute__((__warn_unused_result__));

/// Set the canonical Address for a node.
/** This function sets the canonical Address for a node.
 *  This address is stored permanently until it is changed by another call to this function,
 *  unlike other addresses associated with a node,
 *  such as those added with meshlink_hint_address() or addresses discovered at runtime.
 *
 *  If a canonical Address is set for the local node,
 *  it will be used for the hostname part of generated invitation URLs.
 *  If a canonical Address is set for a remote node,
 *  it is used exclusively for creating outgoing connections to that node.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         A pointer to a struct meshlink_node describing the node.
 *  @param address      A nul-terminated C string containing the address, which can be either in numeric format or a hostname.
 *  @param port         A nul-terminated C string containing the port, which can be either in numeric or symbolic format.
 *                      If it is NULL, the listening port's number will be used.
 *
 *  @return             This function returns true if the address was added, false otherwise.
 */
bool meshlink_set_canonical_address(struct meshlink_handle *mesh, struct meshlink_node *node, const char *address, const char *port) __attribute__((__warn_unused_result__));

/// Clear the canonical Address for a node.
/** This function clears the canonical Address for a node.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         A pointer to a struct meshlink_node describing the node.
 *
 *  @return             This function returns true if the address was removed, false otherwise.
 */
bool meshlink_clear_canonical_address(struct meshlink_handle *mesh, struct meshlink_node *node) __attribute__((__warn_unused_result__));

/// Add an invitation address for the local node.
/** This function adds an address for the local node, which will be used only for invitation URLs.
 *  This address is not stored permanently.
 *  Multiple addresses can be added using multiple calls to this function.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param address      A nul-terminated C string containing the address, which can be either in numeric format or a hostname.
 *  @param port         A nul-terminated C string containing the port, which can be either in numeric or symbolic format.
 *                      If it is NULL, the current listening port's number will be used.
 *
 *  @return             This function returns true if the address was added, false otherwise.
 */
bool meshlink_add_invitation_address(struct meshlink_handle *mesh, const char *address, const char *port) __attribute__((__warn_unused_result__));

/// Clears all invitation address for the local node.
/** This function removes all addresses added with meshlink_add_invitation_address().
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 */
void meshlink_clear_invitation_addresses(struct meshlink_handle *mesh);

/// Add an Address for the local node.
/** This function adds an Address for the local node, which will be used for invitation URLs.
 *  @deprecated This function is deprecated, use meshlink_set_canonical_address() and/or meshlink_add_invitation_address().
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param address      A nul-terminated C string containing the address, which can be either in numeric format or a hostname.
 *
 *  @return             This function returns true if the address was added, false otherwise.
 */
bool meshlink_add_address(struct meshlink_handle *mesh, const char *address) __attribute__((__warn_unused_result__, __deprecated__("use meshlink_set_canonical_address() and/or meshlink_add_invitation_address() instead")));

/// Try to discover the external address for the local node.
/** This function performs tries to discover the local node's external address
 *  by contacting the meshlink.io server. If a reverse lookup of the address works,
 *  the FQDN associated with the address will be returned.
 *
 *  Please note that this is function only returns a single address,
 *  even if the local node might have more than one external address.
 *  In that case, there is no control over which address will be selected.
 *  Also note that if you have a dynamic IP address, or are behind carrier-grade NAT,
 *  there is no guarantee that the external address will be valid for an extended period of time.
 *
 *  This function is blocking. It can take several seconds before it returns.
 *  There is no guarantee it will be able to resolve the external address.
 *  Failures might be because by temporary network outages.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *
 *  @return             This function returns a pointer to a C string containing the discovered external address,
 *                      or NULL if there was an error looking up the address.
 *                      After meshlink_get_external_address() returns, the application is free to overwrite or free this string.
 */
char *meshlink_get_external_address(struct meshlink_handle *mesh) __attribute__((__warn_unused_result__));

/// Try to discover the external address for the local node.
/** This function performs tries to discover the local node's external address
 *  by contacting the meshlink.io server. If a reverse lookup of the address works,
 *  the FQDN associated with the address will be returned.
 *
 *  Please note that this is function only returns a single address,
 *  even if the local node might have more than one external address.
 *  In that case, there is no control over which address will be selected.
 *  Also note that if you have a dynamic IP address, or are behind carrier-grade NAT,
 *  there is no guarantee that the external address will be valid for an extended period of time.
 *
 *  This function is blocking. It can take several seconds before it returns.
 *  There is no guarantee it will be able to resolve the external address.
 *  Failures might be because by temporary network outages.
 *
 *  \memberof meshlink_handle
 *  @param mesh            A handle which represents an instance of MeshLink.
 *  @param address_family  The address family to check, for example AF_INET or AF_INET6. If AF_UNSPEC is given,
 *                         this might return the external address for any working address family.
 *
 *  @return                This function returns a pointer to a C string containing the discovered external address,
 *                         or NULL if there was an error looking up the address.
 *                         After meshlink_get_external_address_for_family() returns, the application is free to overwrite or free this string.
 */
char *meshlink_get_external_address_for_family(struct meshlink_handle *mesh, int address_family) __attribute__((__warn_unused_result__));

/// Try to discover the local address for the local node.
/** This function performs tries to discover the address of the local interface used for outgoing connection.
 *
 *  Please note that this is function only returns a single address,
 *  even if the interface might have more than one address.
 *  In that case, there is no control over which address will be selected.
 *  Also note that if you have a dynamic IP address,
 *  there is no guarantee that the local address will be valid for an extended period of time.
 *
 *  This function will fail if it couldn't find a local address for the given address family.
 *  If hostname resolving is requested, this function may block for a few seconds.
 *
 *  \memberof meshlink_handle
 *  @param mesh            A handle which represents an instance of MeshLink.
 *  @param address_family  The address family to check, for example AF_INET or AF_INET6. If AF_UNSPEC is given,
 *                         this might return the local address for any working address family.
 *
 *  @return                This function returns a pointer to a C string containing the discovered local address,
 *                         or NULL if there was an error looking up the address.
 *                         After meshlink_get_local_address_for_family() returns, the application is free to overwrite or free this string.
 */
char *meshlink_get_local_address_for_family(struct meshlink_handle *mesh, int address_family) __attribute__((__warn_unused_result__));

/// Try to discover the external address for the local node, and add it to its list of addresses.
/** This function is equivalent to:
 *
 *    meshlink_set_canonical_address(mesh, meshlink_get_self(mesh), meshlink_get_external_address(mesh), NULL);
 *
 *  Read the description of meshlink_get_external_address() for the limitations of this function.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *
 *  @return             This function returns true if the address was added, false otherwise.
 */
bool meshlink_add_external_address(struct meshlink_handle *mesh) __attribute__((__warn_unused_result__));

/// Get the network port used by the local node.
/** This function returns the network port that the local node is listening on.
 *
 *  \memberof meshlink_handle
 *  @param mesh          A handle which represents an instance of MeshLink.
 *
 *  @return              This function returns the port number, or -1 in case of an error.
 */
int meshlink_get_port(struct meshlink_handle *mesh) __attribute__((__warn_unused_result__));

/// Set the network port used by the local node.
/** This function sets the network port that the local node is listening on.
 *  It may only be called when the mesh is not running.
 *  If unsure, call meshlink_stop() before calling this function.
 *  Also note that if your node is already part of a mesh with other nodes,
 *  that the other nodes may no longer be able to initiate connections to the local node,
 *  since they will try to connect to the previously configured port.
 *
 *  Note that if a canonical address has been set for the local node,
 *  you might need to call meshlink_set_canonical_address() again to ensure it includes the new port number.
 *
 *  \memberof meshlink_handle
 *  @param mesh          A handle which represents an instance of MeshLink.
 *  @param port          The port number to listen on. This must be between 0 and 65535.
 *                       If the port is set to 0, then MeshLink will listen on a port
 *                       that is randomly assigned by the operating system every time meshlink_open() is called.
 *
 *  @return              This function returns true if the port was successfully changed
 *                       to the desired port, false otherwise. If it returns false, there
 *                       is no guarantee that MeshLink is listening on the old port.
 */

bool meshlink_set_port(struct meshlink_handle *mesh, int port) __attribute__((__warn_unused_result__));

/// Set the timeout for invitations.
/** This function sets the timeout for invitations.
 *  Note that timeouts are only checked at the time a node tries to join using an invitation.
 *  The default timeout for invitations is 1 week.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param timeout      The timeout for invitations in seconds.
 */
void meshlink_set_invitation_timeout(struct meshlink_handle *mesh, int timeout);

/// Invite another node into the mesh.
/** This function generates an invitation that can be used by another node to join the same mesh as the local node.
 *  The generated invitation is a string containing a URL.
 *  This URL should be passed by the application to the invitee in a way that no eavesdroppers can see the URL.
 *  The URL can only be used once, after the user has joined the mesh the URL is no longer valid.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param submesh      A handle which represents an instance of SubMesh.
 *  @param name         A nul-terminated C string containing the name that the invitee will be allowed to use in the mesh.
 *                      After this function returns, the application is free to overwrite or free @a name.
 *  @param flags        A bitwise-or'd combination of flags that controls how the URL is generated.
 *
 *  @return             This function returns a nul-terminated C string that contains the invitation URL, or NULL in case of an error.
 *                      The application should call free() after it has finished using the URL.
 */
char *meshlink_invite_ex(struct meshlink_handle *mesh, struct meshlink_submesh *submesh, const char *name, uint32_t flags) __attribute__((__warn_unused_result__));

/// Invite another node into the mesh.
/** This function generates an invitation that can be used by another node to join the same mesh as the local node.
 *  The generated invitation is a string containing a URL.
 *  This URL should be passed by the application to the invitee in a way that no eavesdroppers can see the URL.
 *  The URL can only be used once, after the user has joined the mesh the URL is no longer valid.
 *
 *  Calling this function is equal to callen meshlink_invite_ex() with flags set to 0.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param submesh      A handle which represents an instance of SubMesh.
 *  @param name         A nul-terminated C string containing the name that the invitee will be allowed to use in the mesh.
 *                      After this function returns, the application is free to overwrite or free @a name.
 *
 *  @return             This function returns a nul-terminated C string that contains the invitation URL, or NULL in case of an error.
 *                      The application should call free() after it has finished using the URL.
 */
char *meshlink_invite(struct meshlink_handle *mesh, struct meshlink_submesh *submesh, const char *name) __attribute__((__warn_unused_result__));

/// Use an invitation to join a mesh.
/** This function allows the local node to join an existing mesh using an invitation URL generated by another node.
 *  An invitation can only be used if the local node has never connected to other nodes before.
 *  After a successfully accepted invitation, the name of the local node may have changed.
 *
 *  This function may only be called on a mesh that has not been started yet and which is not already part of an existing mesh.
 *  It is not valid to call this function when the storage policy set to MESHLINK_STORAGE_DISABLED.
 *
 *  This function is blocking. It can take several seconds before it returns.
 *  There is no guarantee it will perform a successful join.
 *  Failures might be caused by temporary network outages, or by the invitation having expired.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param invitation   A nul-terminated C string containing the invitation URL.
 *                      After this function returns, the application is free to overwrite or free @a invitation.
 *
 *  @return             This function returns true if the local node joined the mesh it was invited to, false otherwise.
 */
bool meshlink_join(struct meshlink_handle *mesh, const char *invitation) __attribute__((__warn_unused_result__));

/// Export the local node's key and addresses.
/** This function generates a string that contains the local node's public key and one or more IP addresses.
 *  The application can pass it in some way to another node, which can then import it,
 *  granting the local node access to the other node's mesh.
 *  The exported data does not contain any secret keys, it is therefore safe to transmit this data unencrypted over public networks.
 *
 *  Note that to create a working connection between two nodes, both must call meshink_export() and both must meshlink_import() each other's data.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *
 *  @return             This function returns a nul-terminated C string that contains the exported key and addresses, or NULL in case of an error.
 *                      The application should call free() after it has finished using this string.
 */
char *meshlink_export(struct meshlink_handle *mesh) __attribute__((__warn_unused_result__));

/// Import another node's key and addresses.
/** This function accepts a string containing the exported public key and addresses of another node.
 *  By importing this data, the local node grants the other node access to its mesh.
 *  The application should make sure that the data it imports is really coming from the node it wants to import,
 *
 *  Note that to create a working connection between two nodes, both must call meshink_export() and both must meshlink_import() each other's data.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param data         A nul-terminated C string containing the other node's exported key and addresses.
 *                      After this function returns, the application is free to overwrite or free @a data.
 *
 *  @return             This function returns true if the data was valid and the other node has been granted access to the mesh, false otherwise.
 */
bool meshlink_import(struct meshlink_handle *mesh, const char *data) __attribute__((__warn_unused_result__));

/// Forget any information about a node.
/** This function allows the local node to forget any information it has about a node,
 *  and if possible will remove any data it has stored on disk about the node.
 *
 *  Any open channels to this node must be closed before calling this function.
 *
 *  After this call returns, the node handle is invalid and may no longer be used, regardless
 *  of the return value of this call.
 *
 *  Note that this function does not prevent MeshLink from actually forgetting about a node,
 *  or re-learning information about a node at a later point in time. It is merely a hint that
 *  the application does not care about this node anymore and that any resources kept could be
 *  cleaned up.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         A pointer to a struct meshlink_node describing the node to be forgotten.
 *
 *  @return             This function returns true if all currently known data about the node has been forgotten, false otherwise.
 */
bool meshlink_forget_node(struct meshlink_handle *mesh, struct meshlink_node *node);

/// Blacklist a node from the mesh.
/** This function causes the local node to blacklist another node.
 *  The local node will drop any existing connections to that node,
 *  and will not send data to it nor accept any data received from it any more.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         A pointer to a struct meshlink_node describing the node to be blacklisted.
 *
 *  @return             This function returns true if the node has been blacklisted, false otherwise.
 */
bool meshlink_blacklist(struct meshlink_handle *mesh, struct meshlink_node *node) __attribute__((__warn_unused_result__));

/// Blacklist a node from the mesh by name.
/** This function causes the local node to blacklist another node by name.
 *  The local node will drop any existing connections to that node,
 *  and will not send data to it nor accept any data received from it any more.
 *
 *  If no node by the given name is known, it is created.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param name         The name of the node to blacklist.
 *
 *  @return             This function returns true if the node has been blacklisted, false otherwise.
 */
bool meshlink_blacklist_by_name(struct meshlink_handle *mesh, const char *name) __attribute__((__warn_unused_result__));

/// Whitelist a node on the mesh.
/** This function causes the local node to whitelist a previously blacklisted node.
 *  The local node will allow connections to and from that node,
 *  and will send data to it and accept any data received from it.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         A pointer to a struct meshlink_node describing the node to be whitelisted.
 *
 *  @return             This function returns true if the node has been whitelisted, false otherwise.
 */
bool meshlink_whitelist(struct meshlink_handle *mesh, struct meshlink_node *node) __attribute__((__warn_unused_result__));

/// Whitelist a node on the mesh by name.
/** This function causes the local node to whitelist a node by name.
 *  The local node will allow connections to and from that node,
 *  and will send data to it and accept any data received from it.
 *
 *  If no node by the given name is known, it is created.
 *  This is useful if new nodes are blacklisted by default.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param name         The name of the node to whitelist.
 *
 *  @return             This function returns true if the node has been whitelisted, false otherwise.
 */
bool meshlink_whitelist_by_name(struct meshlink_handle *mesh, const char *name) __attribute__((__warn_unused_result__));

/// Set whether new nodes are blacklisted by default.
/** This function sets the blacklist behaviour for newly discovered nodes.
 *  If set to true, new nodes will be automatically blacklisted.
 *  If set to false, which is the default, new nodes are automatically whitelisted.
 *  The whitelist/blacklist status of a node may be changed afterwards with the
 *  meshlink_whitelist() and meshlink_blacklist() functions.
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param blacklist    True if new nodes are to be blacklisted, false if whitelisted.
 */
void meshlink_set_default_blacklist(struct meshlink_handle *mesh, bool blacklist);

/// A callback for listening for incoming channels.
/** This function is called whenever a remote node wants to open a channel to the local node.
 *  This callback should only make a decision whether to accept or reject this channel.
 *  The accept callback should be set to get a handle to the actual channel.
 *
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback return quickly and uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand any data over to the application's thread.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         A handle for the node that wants to open a channel.
 *  @param port         The port number the peer wishes to connect to.
 *
 *  @return             This function should return true if the application accepts the incoming channel, false otherwise.
 */
typedef bool (*meshlink_channel_listen_cb_t)(struct meshlink_handle *mesh, struct meshlink_node *node, uint16_t port);

/// A callback for accepting incoming channels.
/** This function is called whenever a remote node has opened a channel to the local node.
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
typedef bool (*meshlink_channel_accept_cb_t)(struct meshlink_handle *mesh, struct meshlink_channel *channel, uint16_t port, const void *data, size_t len);

/// A callback for receiving data from a channel.
/** This function is called whenever data is received from a remote node on a channel.
 *
 *  This function is also called in case the channel has been closed by the remote node, or when the channel is terminated abnormally.
 *  In both cases, @a data will be NULL and @a len will be 0, and meshlink_errno will be set.
 *  In any case, the @a channel handle will still be valid until the application calls meshlink_close().
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *  @param data         A pointer to a buffer containing data sent by the source, or NULL in case of an error.
 *                      The pointer is only valid during the lifetime of the callback.
 *                      The callback should mempcy() the data if it needs to be available outside the callback.
 *  @param len          The length of the data, or 0 in case of an error.
 */
typedef void (*meshlink_channel_receive_cb_t)(struct meshlink_handle *mesh, struct meshlink_channel *channel, const void *data, size_t len);

/// A callback informing the application when data can be sent on a channel.
/** This function is called whenever there is enough free buffer space so a call to meshlink_channel_send() will succeed.
 *
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *  @param len          The maximum amount of data that is guaranteed to be accepted by meshlink_channel_send(),
 *                      or 0 in case of an error.
 */
typedef void (*meshlink_channel_poll_cb_t)(struct meshlink_handle *mesh, struct meshlink_channel *channel, size_t len);

/// Set the listen callback.
/** This functions sets the callback that is called whenever another node wants to open a channel to the local node.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  If no listen or accept callbacks are set, incoming channels are rejected.
 *
 *  \memberof meshlink_handle
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param cb        A pointer to the function which will be called when another node want to open a channel.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
void meshlink_set_channel_listen_cb(struct meshlink_handle *mesh, meshlink_channel_listen_cb_t cb);

/// Set the accept callback.
/** This functions sets the callback that is called whenever a remote node has opened a channel to the local node.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  If no listen or accept callbacks are set, incoming channels are rejected.
 *
 *  \memberof meshlink_handle
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param cb        A pointer to the function which will be called when a new channel has been opened by a remote node.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
void meshlink_set_channel_accept_cb(struct meshlink_handle *mesh, meshlink_channel_accept_cb_t cb);

/// Set the receive callback.
/** This functions sets the callback that is called whenever another node sends data to the local node.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to hand the data over to the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  \memberof meshlink_channel
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param channel   A handle for the channel.
 *  @param cb        A pointer to the function which will be called when another node sends data to the local node.
 *                   If a NULL pointer is given, the callback will be disabled and incoming data is ignored.
 */
void meshlink_set_channel_receive_cb(struct meshlink_handle *mesh, struct meshlink_channel *channel, meshlink_channel_receive_cb_t cb);

/// Set the poll callback.
/** This functions sets the callback that is called whenever data can be sent to another node.
 *  The callback is run in MeshLink's own thread.
 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
 *  to pass data to or from the application's thread.
 *  The callback should also not block itself and return as quickly as possible.
 *
 *  \memberof meshlink_channel
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param channel   A handle for the channel.
 *  @param cb        A pointer to the function which will be called when data can be sent to another node.
 *                   If a NULL pointer is given, the callback will be disabled.
 */
void meshlink_set_channel_poll_cb(struct meshlink_handle *mesh, struct meshlink_channel *channel, meshlink_channel_poll_cb_t cb);

/// Set the send buffer size of a channel.
/** This function sets the desired size of the send buffer.
 *  The default size is 128 kB.
 *
 *  \memberof meshlink_channel
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param channel   A handle for the channel.
 *  @param size      The desired size for the send buffer.
 */
void meshlink_set_channel_sndbuf(struct meshlink_handle *mesh, struct meshlink_channel *channel, size_t size);

/// Set the receive buffer size of a channel.
/** This function sets the desired size of the receive buffer.
 *  The default size is 128 kB.
 *
 *  \memberof meshlink_channel
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param channel   A handle for the channel.
 *  @param size      The desired size for the send buffer.
 */
void meshlink_set_channel_rcvbuf(struct meshlink_handle *mesh, struct meshlink_channel *channel, size_t size);

/// Set the send buffer storage of a channel.
/** This function provides MeshLink with a send buffer allocated by the application.
 *  The buffer must be valid until the channel is closed or until this function is called again with a NULL pointer for @a buf.
 *
 *  \memberof meshlink_channel
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param channel   A handle for the channel.
 *  @param buf       A pointer to the start of the buffer.
 *                   If a NULL pointer is given, MeshLink will use its own internal buffer again.
 *  @param size      The size of the buffer.
 */
void meshlink_set_channel_sndbuf_storage(struct meshlink_handle *mesh, struct meshlink_channel *channel, void *buf, size_t size);

/// Set the receive buffer storage of a channel.
/** This function provides MeshLink with a receive buffer allocated by the application.
 *  The buffer must be valid until the channel is closed or until this function is called again with a NULL pointer for @a buf.
 *
 *  \memberof meshlink_channel
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param channel   A handle for the channel.
 *  @param buf       A pointer to the start of the buffer.
 *                   If a NULL pointer is given, MeshLink will use its own internal buffer again.
 *  @param size      The size of the buffer.
 */
void meshlink_set_channel_rcvbuf_storage(struct meshlink_handle *mesh, struct meshlink_channel *channel, void *buf, size_t size);

/// Set the flags of a channel.
/** This function allows changing some of the channel flags.
 *  Currently only MESHLINK_CHANNEL_NO_PARTIAL and MESHLINK_CHANNEL_DROP_LATE are supported, other flags are ignored.
 *  These flags only affect the local side of the channel with the peer.
 *  The changes take effect immediately.
 *
 *  \memberof meshlink_channel
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param channel   A handle for the channel.
 *  @param flags     A bitwise-or'd combination of flags that set the semantics for this channel.
 */
void meshlink_set_channel_flags(struct meshlink_handle *mesh, struct meshlink_channel *channel, uint32_t flags);

/// Open a reliable stream channel to another node.
/** This function is called whenever a remote node wants to open a channel to the local node.
 *  The application then has to decide whether to accept or reject this channel.
 *
 *  This function returns a pointer to a struct meshlink_channel that will be allocated by MeshLink.
 *  When the application does no longer need to use this channel, it must call meshlink_close()
 *  to free its resources.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         The node to which this channel is being initiated.
 *  @param port         The port number the peer wishes to connect to.
 *  @param cb           A pointer to the function which will be called when the remote node sends data to the local node.
 *                      The pointer may be NULL, in which case incoming data is ignored.
 *  @param data         A pointer to a buffer containing data to already queue for sending, or NULL if there is no data to send.
 *                      After meshlink_send() returns, the application is free to overwrite or free this buffer.
 *                      If len is 0, the data pointer is copied into the channel's priv member.
 *  @param len          The length of the data, or 0 if there is no data to send.
 *  @param flags        A bitwise-or'd combination of flags that set the semantics for this channel.
 *
 *  @return             A handle for the channel, or NULL in case of an error.
 *                      The handle is valid until meshlink_channel_close() is called.
 */
struct meshlink_channel *meshlink_channel_open_ex(struct meshlink_handle *mesh, struct meshlink_node *node, uint16_t port, meshlink_channel_receive_cb_t cb, const void *data, size_t len, uint32_t flags) __attribute__((__warn_unused_result__));

/// Open a reliable stream channel to another node.
/** This function is called whenever a remote node wants to open a channel to the local node.
 *  The application then has to decide whether to accept or reject this channel.
 *
 *  This function returns a pointer to a struct meshlink_channel that will be allocated by MeshLink.
 *  When the application does no longer need to use this channel, it must call meshlink_close()
 *  to free its resources.
 *
 *  Calling this function is equivalent to calling meshlink_channel_open_ex()
 *  with the flags set to MESHLINK_CHANNEL_TCP.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         The node to which this channel is being initiated.
 *  @param port         The port number the peer wishes to connect to.
 *  @param cb           A pointer to the function which will be called when the remote node sends data to the local node.
 *                      The pointer may be NULL, in which case incoming data is ignored.
 *  @param data         A pointer to a buffer containing data to already queue for sending, or NULL if there is no data to send.
 *                      After meshlink_send() returns, the application is free to overwrite or free this buffer.
 *  @param len          The length of the data, or 0 if there is no data to send.
 *                      If len is 0, the data pointer is copied into the channel's priv member.
 *
 *  @return             A handle for the channel, or NULL in case of an error.
 *                      The handle is valid until meshlink_channel_close() is called.
 */
struct meshlink_channel *meshlink_channel_open(struct meshlink_handle *mesh, struct meshlink_node *node, uint16_t port, meshlink_channel_receive_cb_t cb, const void *data, size_t len) __attribute__((__warn_unused_result__));

/// Partially close a reliable stream channel.
/** This shuts down the read or write side of a channel, or both, without closing the handle.
 *  It can be used to inform the remote node that the local node has finished sending all data on the channel,
 *  but still allows waiting for incoming data from the remote node.
 *
 *  Shutting down the receive direction is also possible, and is equivalent to setting the receive callback to NULL.
 *
 *  \memberof meshlink_channel
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *  @param direction    Must be one of SHUT_RD, SHUT_WR or SHUT_RDWR, otherwise this call will not have any affect.
 */
void meshlink_channel_shutdown(struct meshlink_handle *mesh, struct meshlink_channel *channel, int direction);

/// Close a reliable stream channel.
/** This informs the remote node that the local node has finished sending all data on the channel.
 *  It also causes the local node to stop accepting incoming data from the remote node.
 *  Afterwards, the channel handle is invalid and must not be used any more.
 *
 *  It is allowed to call this function at any time on a valid handle, even inside callback functions.
 *  If called with a valid handle, this function always succeeds, otherwise the result is undefined.
 *
 *  \memberof meshlink_channel
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 */
void meshlink_channel_close(struct meshlink_handle *mesh, struct meshlink_channel *channel);

/// Abort a reliable stream channel.
/** This aborts a channel.
 *  Data that was in the send and receive buffers is dropped, so potentially there is some data that
 *  was sent on this channel that will not be received by the peer.
 *  Afterwards, the channel handle is invalid and must not be used any more.
 *
 *  It is allowed to call this function at any time on a valid handle, even inside callback functions.
 *  If called with a valid handle, this function always succeeds, otherwise the result is undefined.
 *
 *  \memberof meshlink_channel
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 */
void meshlink_channel_abort(struct meshlink_handle *mesh, struct meshlink_channel *channel);

/// Transmit data on a channel
/** This queues data to send to the remote node.
 *
 *  \memberof meshlink_channel
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *  @param data         A pointer to a buffer containing data sent by the source, or NULL if there is no data to send.
 *                      After meshlink_send() returns, the application is free to overwrite or free this buffer.
 *  @param len          The length of the data, or 0 if there is no data to send.
 *
 *  @return             The amount of data that was queued, which can be less than len, or a negative value in case of an error.
 *                      If MESHLINK_CHANNEL_NO_PARTIAL is set, then the result will either be len,
 *                      0 if the buffer is currently too full, or -1 if len is too big even for an empty buffer.
 */
ssize_t meshlink_channel_send(struct meshlink_handle *mesh, struct meshlink_channel *channel, const void *data, size_t len) __attribute__((__warn_unused_result__));

/// A callback for cleaning up buffers submitted for asynchronous I/O.
/** This callbacks signals that MeshLink has finished using this buffer.
 *  The ownership of the buffer is now back into the application's hands.
 *
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param channel   A handle for the channel which used this buffer.
 *  @param data      A pointer to a buffer containing the enqueued data.
 *  @param len       The length of the buffer.
 *  @param priv      A private pointer which was set by the application when submitting the buffer.
 */
typedef void (*meshlink_aio_cb_t)(struct meshlink_handle *mesh, struct meshlink_channel *channel, const void *data, size_t len, void *priv);

/// A callback for asynchronous I/O to and from filedescriptors.
/** This callbacks signals that MeshLink has finished using this filedescriptor.
 *
 *  @param mesh      A handle which represents an instance of MeshLink.
 *  @param channel   A handle for the channel which used this filedescriptor.
 *  @param fd        The filedescriptor that was used.
 *  @param len       The length of the data that was successfully sent or received.
 *  @param priv      A private pointer which was set by the application when submitting the buffer.
 */
typedef void (*meshlink_aio_fd_cb_t)(struct meshlink_handle *mesh, struct meshlink_channel *channel, int fd, size_t len, void *priv);

/// Transmit data on a channel asynchronously
/** This registers a buffer that will be used to send data to the remote node.
 *  Multiple buffers can be registered, in which case data will be sent in the order the buffers were registered.
 *  While there are still buffers with unsent data, the poll callback will not be called.
 *
 *  \memberof meshlink_channel
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *  @param data         A pointer to a buffer containing data sent by the source, or NULL if there is no data to send.
 *                      After meshlink_channel_aio_send() returns, the buffer may not be modified or freed by the application
 *                      until the callback routine is called.
 *  @param len          The length of the data, or 0 if there is no data to send.
 *  @param cb           A pointer to the function which will be called when MeshLink has finished using the buffer.
 *  @param priv         A private pointer which is passed unchanged to the callback.
 *
 *  @return             True if the buffer was enqueued, false otherwise.
 */
bool meshlink_channel_aio_send(struct meshlink_handle *mesh, struct meshlink_channel *channel, const void *data, size_t len, meshlink_aio_cb_t cb, void *priv) __attribute__((__warn_unused_result__));

/// Transmit data on a channel asynchronously from a filedescriptor
/** This will read up to the specified length number of bytes from the given filedescriptor, and send it over the channel.
 *  The callback may be returned early if there is an error reading from the filedescriptor.
 *  While there is still with unsent data, the poll callback will not be called.
 *
 *  \memberof meshlink_channel
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *  @param fd           A file descriptor from which data will be read.
 *  @param len          The length of the data, or 0 if there is no data to send.
 *  @param cb           A pointer to the function which will be called when MeshLink has finished using the filedescriptor.
 *  @param priv         A private pointer which is passed unchanged to the callback.
 *
 *  @return             True if the buffer was enqueued, false otherwise.
 */
bool meshlink_channel_aio_fd_send(struct meshlink_handle *mesh, struct meshlink_channel *channel, int fd, size_t len, meshlink_aio_fd_cb_t cb, void *priv) __attribute__((__warn_unused_result__));

/// Receive data on a channel asynchronously
/** This registers a buffer that will be filled with incoming channel data.
 *  Multiple buffers can be registered, in which case data will be received in the order the buffers were registered.
 *  While there are still buffers that have not been filled, the receive callback will not be called.
 *
 *  \memberof meshlink_channel
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *  @param data         A pointer to a buffer that will be filled with incoming data.
 *                      After meshlink_channel_aio_receive() returns, the buffer may not be modified or freed by the application
 *                      until the callback routine is called.
 *  @param len          The length of the data.
 *  @param cb           A pointer to the function which will be called when MeshLink has finished using the buffer.
 *  @param priv         A private pointer which is passed unchanged to the callback.
 *
 *  @return             True if the buffer was enqueued, false otherwise.
 */
bool meshlink_channel_aio_receive(struct meshlink_handle *mesh, struct meshlink_channel *channel, const void *data, size_t len, meshlink_aio_cb_t cb, void *priv) __attribute__((__warn_unused_result__));

/// Receive data on a channel asynchronously and send it to a filedescriptor
/** This will read up to the specified length number of bytes from the channel, and send it to the filedescriptor.
 *  The callback may be returned early if there is an error writing to the filedescriptor.
 *  While there is still unread data, the receive callback will not be called.
 *
 *  \memberof meshlink_channel
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *  @param fd           A file descriptor to which data will be written.
 *  @param len          The length of the data.
 *  @param cb           A pointer to the function which will be called when MeshLink has finished using the filedescriptor.
 *  @param priv         A private pointer which was set by the application when submitting the buffer.
 *
 *  @return             True if the buffer was enqueued, false otherwise.
 */
bool meshlink_channel_aio_fd_receive(struct meshlink_handle *mesh, struct meshlink_channel *channel, int fd, size_t len, meshlink_aio_fd_cb_t cb, void *priv) __attribute__((__warn_unused_result__));

/// Get channel flags.
/** This returns the flags used when opening this channel.
 *
 *  \memberof meshlink_channel
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *
 *  @return             The flags set for this channel.
 */
uint32_t meshlink_channel_get_flags(struct meshlink_handle *mesh, struct meshlink_channel *channel) __attribute__((__warn_unused_result__));

/// Get the amount of bytes in the send buffer.
/** This returns the amount of bytes in the send buffer.
 *  These bytes have not been received by the peer yet.
 *
 *  \memberof meshlink_channel
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *
 *  @return             The amount of un-ACKed bytes in the send buffer.
 */
size_t meshlink_channel_get_sendq(struct meshlink_handle *mesh, struct meshlink_channel *channel) __attribute__((__warn_unused_result__));

/// Get the amount of bytes in the receive buffer.
/** This returns the amount of bytes in the receive buffer.
 *  These bytes have not been processed by the application yet.
 *
 *  \memberof meshlink_channel
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *
 *  @return             The amount of bytes in the receive buffer.
 */
size_t meshlink_channel_get_recvq(struct meshlink_handle *mesh, struct meshlink_channel *channel) __attribute__((__warn_unused_result__));

/// Get the maximum segment size of a channel.
/** This returns the amount of bytes that can be sent at once for channels with UDP semantics.
 *
 *  \memberof meshlink_channel
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param channel      A handle for the channel.
 *
 *  @return             The amount of bytes in the receive buffer.
 */
size_t meshlink_channel_get_mss(struct meshlink_handle *mesh, struct meshlink_channel *channel) __attribute__((__warn_unused_result__));

/// Set the connection timeout used for channels to the given node.
/** This sets the timeout after which unresponsive channels will be reported as closed.
 *  The timeout is set for all current and future channels to the given node.
 *
 *  \memberof meshlink_node
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param node         A pointer to a struct meshlink_node describing the node to set the channel connection timeout for.
 *  @param timeout      The timeout in seconds after which unresponsive channels will be reported as closed.
 *                      The default is 60 seconds.
 */
void meshlink_set_node_channel_timeout(struct meshlink_handle *mesh, struct meshlink_node *node, int timeout);

/// Hint that a hostname may be found at an address
/** This function indicates to meshlink that the given hostname is likely found
 *  at the given IP address and port.
 *
 *  \memberof meshlink_node
 *  @param mesh     A handle which represents an instance of MeshLink.
 *  @param node     A pointer to a struct meshlink_node describing the node to add the address hint for.
 *  @param addr     The IP address and port which should be tried for the
 *                  given hostname. The caller is free to overwrite or free
 *                  this memory once meshlink returns.
 */
void meshlink_hint_address(struct meshlink_handle *mesh, struct meshlink_node *node, const struct sockaddr *addr);

/// Enable or disable zeroconf discovery of local peers
/** This controls whether zeroconf discovery using the Catta library will be
 *  enabled to search for peers on the local network. By default, it is enabled.
 *
 *  \memberof meshlink_handle
 *  @param mesh    A handle which represents an instance of MeshLink.
 *  @param enable  Set to true to enable discovery, false to disable.
 */
void meshlink_enable_discovery(struct meshlink_handle *mesh, bool enable);

/// Inform MeshLink that the local network configuration might have changed
/** This is intended to be used when there is no way for MeshLink to get notifications of local network changes.
 *  It forces MeshLink to scan all network interfaces for changes in up/down status and new/removed addresses,
 *  and will immediately check if all connections to other nodes are still alive.
 *
 *  \memberof meshlink_handle
 *  @param mesh    A handle which represents an instance of MeshLink.
 */
void meshlink_hint_network_change(struct meshlink_handle *mesh);

/// Performs key rotation for an encrypted storage
/** This rotates the (master) key for an encrypted storage and discards the old key
 *  if the call succeeded. This is an atomic call.
 *
 *  \memberof meshlink_handle
 *  @param mesh     A handle which represents an instance of MeshLink.
 *  @param key      A pointer to the new key used to encrypt storage.
 *  @param keylen   The length of the new key in bytes.
 *
 *  @return         This function returns true if the key rotation for the encrypted storage succeeds, false otherwise.
 */
bool meshlink_encrypted_key_rotate(struct meshlink_handle *mesh, const void *key, size_t keylen) __attribute__((__warn_unused_result__));

/// Set device class timeouts
/** This sets the ping interval and timeout for a given device class.
 *
 *  \memberof meshlink_handle
 *  @param mesh          A handle which represents an instance of MeshLink.
 *  @param devclass      The device class to update
 *  @param pinginterval  The interval between keepalive packets, in seconds. The default is 60.
 *  @param pingtimeout   The required time within which a peer should respond, in seconds. The default is 5.
 *                       The timeout must be smaller than the interval.
 */
void meshlink_set_dev_class_timeouts(struct meshlink_handle *mesh, dev_class_t devclass, int pinginterval, int pingtimeout);

/// Set device class fast retry period
/** This sets the fast retry period for a given device class.
 *  During this period after the last time the mesh becomes unreachable, connections are tried once a second.
 *
 *  \memberof meshlink_handle
 *  @param mesh               A handle which represents an instance of MeshLink.
 *  @param devclass           The device class to update
 *  @param fast_retry_period  The period during which fast connection retries are done. The default is 0.
 */
void meshlink_set_dev_class_fast_retry_period(struct meshlink_handle *mesh, dev_class_t devclass, int fast_retry_period);

/// Set device class maximum timeout
/** This sets the maximum timeout for outgoing connection retries for a given device class.
 *
 *  \memberof meshlink_handle
 *  @param mesh          A handle which represents an instance of MeshLink.
 *  @param devclass      The device class to update
 *  @param maxtimeout    The maximum timeout between reconnection attempts, in seconds. The default is 900.
 */
void meshlink_set_dev_class_maxtimeout(struct meshlink_handle *mesh, dev_class_t devclass, int maxtimeout);

/// Reset all connection timers
/** This resets all timers related to connections, causing pending outgoing connections to be retried immediately.
 * It also sends keepalive packets on all active connections immediately.
 *
 *  \memberof meshlink_handle
 *  @param mesh          A handle which represents an instance of MeshLink.
 */
void meshlink_reset_timers(struct meshlink_handle *mesh);

/// Set which order invitations are committed
/** This determines in which order configuration files are written to disk during an invitation.
 *  By default, the invitee saves the configuration to disk first, then the inviter.
 *  By calling this function with @a inviter_commits_first set to true, the order is reversed.
 *
 *  \memberof meshlink_handle
 *  @param mesh               A handle which represents an instance of MeshLink.
 *  @param inviter_commits_first  If true, then the node that invited a peer will commit data to disk first.
 */
void meshlink_set_inviter_commits_first(struct meshlink_handle *mesh, bool inviter_commits_first);

/// Set the URL used to discover the host's external address
/** For generating invitation URLs, MeshLink can look up the externally visible address of the local node.
 *  It does so by querying an external service. By default, this is http://meshlink.io/host.cgi.
 *  Only URLs starting with http:// are supported.
 *
 *  \memberof meshlink_handle
 *  @param mesh  A handle which represents an instance of MeshLink.
 *  @param url   The URL to use for external address queries, or NULL to revert back to the default URL.
 */
void meshlink_set_external_address_discovery_url(struct meshlink_handle *mesh, const char *url);

/// Set the scheduling granularity of the application
/** This should be set to the effective scheduling granularity for the application.
 *  This depends on the scheduling granularity of the operating system, the application's
 *  process priority and whether it is running as realtime or not.
 *  The default value is 10000 (10 milliseconds).
 *
 *  \memberof meshlink_handle
 *  @param mesh         A handle which represents an instance of MeshLink.
 *  @param granularity  The scheduling granularity of the application in microseconds.
 */
void meshlink_set_scheduling_granularity(struct meshlink_handle *mesh, long granularity);

/// Sets the storage policy used by MeshLink
/** This sets the policy MeshLink uses when it has new information about nodes.
 *  By default, all udpates will be stored to disk (unless an ephemeral instance has been opened).
 *  Setting the policy to MESHLINK_STORAGE_KEYS_ONLY, only updates that contain new keys for nodes
 *  are stored, as well as blacklist/whitelist settings.
 *  By setting the policy to MESHLINK_STORAGE_DISABLED, no updates will be stored.
 *
 *  \memberof meshlink_handle
 *  @param mesh    A handle which represents an instance of MeshLink.
 *  @param policy  The storage policy to use.
 */
void meshlink_set_storage_policy(struct meshlink_handle *mesh, meshlink_storage_policy_t policy);

#ifdef __cplusplus
}
#endif

#endif
