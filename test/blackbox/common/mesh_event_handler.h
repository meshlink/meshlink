/*
    mesh_event_handler.h
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>

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

#ifndef _MESH_EVENT_HANDLER_H_
#define _MESH_EVENT_HANDLER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>

/// mesh events
// TODO: Add more mesh event if required.
typedef enum {
	NO_PREFERENCE = 0,
	META_CONN_SUCCESSFUL,
	META_CONN,
	META_DISCONN,
	META_CONN_CLOSED,
	NODE_INVITATION,
	CHANGED_IP_ADDRESS,
	NODE_UNREACHABLE,
	NODE_REACHABLE,
	META_RECONN_SUCCESSFUL,
	META_RECONN_FAILURE,
	MESH_DATA_RECEIVED,
	NODE_STARTED,
	NODE_RESTARTED,
	NODE_JOINED,
	NODE_JOINED1,
	NODE_JOINED2,
	NODE_JOINED3,
	PORT_NO,
	ERR_NETWORK,
	MESH_DATA_VERIFED,
	CHANNEL_OPENED,
	CHANNEL_REQ_RECIEVED,
	CHANNEL_CONNECTED,
	CHANNEL_DATA_RECIEVED,
	MESH_NODE_DISCOVERED,
	INCOMING_META_CONN,
	OUTGOING_META_CONN,
	AUTO_DISCONN,

	MAX_EVENT           // Maximum event enum
} mesh_event_t;

/// mesh event UDP packet
typedef struct  mesh_event_payload {
	uint16_t      client_id;
	mesh_event_t  mesh_event;
	uint8_t       payload_length;
	void          *payload;
} mesh_event_payload_t;

/// callback for handling the mesh event
/** mesh event callback called from wait_for_event() if the mesh event UDP server gets a mesh event.
 *
 *  @param mesh_event_packet    packet containing client-id, mesh event & payload (if any).
 */
typedef bool (*mesh_event_callback_t)(mesh_event_payload_t mesh_event_packet);

/// Creates an UDP server for listening mesh events.
/** This function creates an UDP socket, binds it with given interface address and returns a NULL
 *  terminated string containing server's IP address & port number.
 *
 *  @param ifname       Name of the network interface to which the socket has to be created.
 *
 *  @return             This function returns a NULL terminated string which has IP address and
 *                                                                              port number of the server socket. The application should call free() after
 *                                                                                      it has finished using the exported string.
 */
extern char *mesh_event_sock_create(const char *ifname);

/// Waits for the mesh event for about the given timeout.
/** This function waits for the mesh event that's expected to occur for the given timeout. If a mesh event
 *  is received then the given callback will be invoked.
 *
 *  @param callback     callback which handles the mesh event packet.
 *  @param timeout      timeout for which the the function has to wait for the event.
 *
 *  @return             This function returns true if a mesh event occured else false if timeout exceeded.
 */
extern bool wait_for_event(mesh_event_callback_t callback, int timeout);

/// Sends the mesh event to server.
/** This function sends the mesh event to the server. At the server end it's expected to wait_for_event()
 *  otherwise the packet will be dropped.
 *
 *  @param client_id        Client id by which server can identify the client/node.
 *  @param event            An enum describing the mesh event.
 *  @param payload          Payload can also be attached along with the mesh event if any, else NULL can
 *                          can be specified.
 *  @param payload_length   Length of the payload if specified else 0 can be specified.
 *                                                                                                      the maximum payload size can be upto PAYLOAD_MAX_SIZE and if the
 *                          PAYLOAD_MAX_SIZE macro is changed it should not exceed the UDP datagram size.
 *
 *  @return                  This function returns true on success else returns false.
 */
extern bool mesh_event_sock_send(int client_id, mesh_event_t event, void *payload, size_t payload_length);

/// Imports the server address, saves it and opens an UDP client socket.
/** This function creates an UDP socket, binds it with given interface address and returns a NULL
 *  terminated string containing server's IP address & port number.
 *
 *  @param server_address    NULL terminated string that's exported by mesh_event_sock_create() which
 *                           which contains IP address and port number of the mesh event server.
 *
 *  @return                  void
 */
extern void mesh_event_sock_connect(const char *server_address);
#endif // _MESH_EVENT_HANDLER_H_
