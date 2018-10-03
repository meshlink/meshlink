/*
    mesh_event_handler.c -- handling of mesh events API
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>
                        Manav Kumar Mehta <manavkumarm@yahoo.com>

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
#include "mesh_event_handler.h"

#define SERVER_LISTEN_PORT "9000" /* Port number that is binded with mesh event server socket */

// TODO: Implement mesh event handling with reentrant functions(if required).
static struct sockaddr_in server_addr;
static int client_fd;
static int server_fd;

/* check for endianness, if little endian return true else false*/
static bool check_little_endian_order(void) {
  union {
    uint16_t u16;
    uint8_t  u8;
  } check;
  check.u16 = 1;

  if(check.u8 == 1) {
    return true;
  } else {
    return false;
  }
}

/* convert unsigned 16-bit accorging to it's endianness */
static uint16_t ec_byte_stream_to_uint16(const uint8_t *byte_stream) {
  uint16_t ret_val;

  if(check_little_endian_order()) {
    ret_val = byte_stream[0] | (uint16_t) byte_stream[1] << 8;
  } else {
    ret_val = byte_stream[1] | (uint16_t) byte_stream[0] << 8;
  }
  return ret_val;
}

/* convert unsigned 32-bit accorging to it's endianness */
static uint32_t ec_byte_stream_to_uint32(const uint8_t *byte_stream) {
  uint32_t ret_val;

  if (check_little_endian_order()) {
      ret_val = byte_stream[0] | (uint32_t) byte_stream[1] << 8 | (uint32_t) byte_stream[2] << 16 | (uint32_t) byte_stream[3] << 24;
  } else {
    ret_val = byte_stream[3] | (uint32_t) byte_stream[2] << 8 | (uint32_t) byte_stream[1] << 16 | (uint32_t) byte_stream[0] << 24;
  }
  return ret_val;
}

/* updating the packet byte order according to the machine byte order */
static void mesh_event_packet_endianness(mesh_event_payload_t *mesh_event_packet) {
  mesh_event_packet -> client_id = ec_byte_stream_to_uint32( (uint8_t *) &mesh_event_packet->client_id );
  mesh_event_packet->mesh_event = ec_byte_stream_to_uint32( (uint8_t *) &mesh_event_packet->mesh_event );
  mesh_event_packet->payload_length = ec_byte_stream_to_uint16( (uint8_t *) &mesh_event_packet->payload_length );
}

char *mesh_event_sock_create(const char *if_name ) {
  struct sockaddr_in server;
  char *ip;
  struct ifreq req_if;
  struct sockaddr_in *resp_if_addr;

  if(if_name == NULL) {
    return NULL;
  }

  server_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if(server_fd < 0) {
    perror("socket");
  }
  assert(server_fd >= 0);

  int reuse = 1;
  assert(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != -1);

  memset(&req_if, 0, sizeof(req_if));
  req_if.ifr_addr.sa_family = AF_INET;
  strncpy(req_if.ifr_name, if_name, IFNAMSIZ - 1);
  assert(ioctl(server_fd, SIOCGIFADDR, &req_if) != -1);
  resp_if_addr = (struct sockaddr_in *) &(req_if.ifr_addr);

  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr   = resp_if_addr->sin_addr;
  server.sin_port   = htons(atoi(SERVER_LISTEN_PORT));
  assert(bind(server_fd, (struct sockaddr*) &server, sizeof(struct sockaddr)) != -1);

  assert(ip = malloc(30));
  strncpy(ip, inet_ntoa(resp_if_addr->sin_addr), 20);
  strcat(ip, ":");
  strcat(ip, SERVER_LISTEN_PORT);
  fprintf(stderr, "Socket created and exported the IP address & port successfully\n");

  return ip;
}

bool mesh_event_sock_connect(const char *import ) {
  char *port = NULL;

  if(import == NULL) {
    return false;
  }

  fprintf(stderr, "Importing IP address and port of the UDP server\n");
  char *ip = (char *) malloc(strlen(import) + 1);
  strcpy(ip, import);
  assert((port = strchr(ip, ':')) != NULL);
  *port = '\0';
  port++;

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family      = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port        = htons(atoi(port));
  fprintf(stderr, "Connecting to IP address :%s & Port :%s\n", ip, port);
  client_fd = socket(AF_INET, SOCK_DGRAM, 0);
  free(ip);
  if(client_fd < 0) {
    perror("client socket status");
    return false;
  } else {
    return true;
  }
}

bool mesh_event_sock_send( int client_id, mesh_event_t event, void *payload, size_t payload_length ) {
  mesh_event_payload_t mesh_event_send_packet;
  ssize_t send_ret;

	// Packing the mesh event
  mesh_event_send_packet.client_id   = client_id;
  mesh_event_send_packet.mesh_event  = event;
  if((payload == NULL) || (payload_length == 0)) {
    mesh_event_send_packet.payload_length = 0;
    fprintf(stderr, "Sending mesh event to test-driver(without payload)\n");
  } else {
    mesh_event_send_packet.payload_length = payload_length;
    memmove(mesh_event_send_packet.payload, payload, payload_length);
    fprintf(stderr, "Sending mesh event to test-driver(with payload)\n");
  }
  mesh_event_packet_endianness(&mesh_event_send_packet);

  send_ret = sendto(client_fd, &mesh_event_send_packet, sizeof(mesh_event_send_packet), 0, (const struct sockaddr *) &server_addr, sizeof(server_addr));
  if(send_ret < 0) {
    perror("sendto status");
    return false;
  } else {
    return true;
  }
}

bool wait_for_event(mesh_event_callback_t callback, int t) {
  struct timeval timeout;
  struct sockaddr client;
  socklen_t soc_len;
  fd_set read_fds;
  int activity;
  mesh_event_payload_t mesh_event_rec_packet;

  timeout.tv_sec  = t;
  timeout.tv_usec = 0;
  FD_ZERO(&read_fds);
  FD_SET(server_fd, &read_fds);

  while(1) {
    activity = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
    assert(activity != -1);

    if(activity == 0) {
      // If no activity happened for the timeout given
      return false;
    } else if (FD_ISSET(server_fd, &read_fds)) {
      // Unpacking the mesh event
      fprintf(stderr, "Found read activity at Server File descriptor\n");
      ssize_t recv_ret = recvfrom(server_fd, &mesh_event_rec_packet, sizeof(mesh_event_rec_packet), 0, &client, &soc_len);

      mesh_event_packet_endianness(&mesh_event_rec_packet);
      callback(mesh_event_rec_packet);
			return true;
    }
  }// while
}
/*
bool wait_for_event_only(mesh_event_callback_t callback, int t, mesh_event_t event) {
  struct timeval timeout;
  struct sockaddr client;
  socklen_t soc_len;
  fd_set read_fds;
  int activity;
  mesh_event_payload_t mesh_event_rec_packet;

  timeout.tv_sec  = t;
  timeout.tv_usec = 0;
  FD_ZERO(&read_fds);
  FD_SET(server_fd, &read_fds);

  while(1) {
    activity = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
    assert(activity != -1);

    if(activity == 0) {
      // If no activity happened for the timeout given
      fprintf(stderr, "wait_for_event timeout\n");
      return false;
    } else if (FD_ISSET(server_fd, &read_fds)) {
      // Unpacking the mesh event
      fprintf(stderr, "Found read activity at Server File descriptor\n");
      do {
      ssize_t recv_ret = recvfrom(server_fd, &mesh_event_rec_packet, sizeof(mesh_event_rec_packet), 0, &client, &soc_len);
      mesh_event_packet_endianness(&mesh_event_rec_packet);

      if(mesh_event_rec_packet.mesh_event == event) {
        callback(mesh_event_rec_packet);
        break;
      } else {
        fprintf(stderr, "Got event %d, and it's dropped\n", mesh_event_rec_packet.mesh_event);
      }
      }while(recv_ret > 0);


			return true;
    }
  }// while
}
*/
