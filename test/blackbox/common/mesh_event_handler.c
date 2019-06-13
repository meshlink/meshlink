/*
    mesh_event_handler.c -- handling of mesh events API
    Copyright (C) 2018  Guus Sliepen <guus@meshlink.io>

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
#include <pthread.h>
#include "../../../src/meshlink_queue.h"
#include "../../utils.h"
#include "mesh_event_handler.h"

#define SERVER_LISTEN_PORT "9000" /* Port number that is binded with mesh event server socket */
#define UDP_BUFF_MAX 2000

const char *event_status[] = {
	[NODE_STARTED]                          = "Node Started",
	[NODE_JOINED]                           = "Node Joined",
	[ERR_NETWORK]                           = "Network Error",
	[CHANNEL_OPENED]                        = "Channel Opened",
	[CHANNEL_DATA_RECIEVED]                         = "Channel Data Received",
	[SIG_ABORT]                             = "SIG_ABORT Received",
	[MESH_EVENT_COMPLETED]                          = "MESH_EVENT_COMPLETED Received"
};

// TODO: Implement mesh event handling with reentrancy .
static struct sockaddr_in server_addr;
static int client_fd = -1;
static int server_fd = -1;
static pthread_t event_receive_thread, event_handle_thread;
static meshlink_queue_t event_queue;
static bool event_receive_thread_running, event_handle_thread_running;
static struct cond_flag sync_event = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};

static void set_cond_flag(struct cond_flag *s, bool flag) {
	pthread_mutex_lock(&s->mutex);
	s->flag = flag;
	pthread_cond_broadcast(&s->cond);
	pthread_mutex_unlock(&s->mutex);
}

static bool wait_cond_flag(struct cond_flag *s, int seconds) {
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += seconds;

	pthread_mutex_lock(&s->mutex);

	while(!s->flag)
		if(!pthread_cond_timedwait(&s->cond, &s->mutex, &timeout) || errno != EINTR) {
			break;
		}

	pthread_mutex_unlock(&s->mutex);

	return s->flag;
}

// event_receive_handler running in a separate thread queues all the events received from the UDP port
static void *event_receive_handler(void *arg) {
	(void)arg;
	size_t recv_ret;
	char udp_buff[UDP_BUFF_MAX];
	struct sockaddr client;
	socklen_t soc_len;

	while(event_receive_thread_running) {
		recv_ret = recvfrom(server_fd, udp_buff, sizeof(udp_buff), 0, &client, &soc_len);
		assert(recv_ret >= sizeof(mesh_event_payload_t));

		// Push received mesh event data into the event_queue
		mesh_event_payload_t *data = malloc(sizeof(mesh_event_payload_t));
		assert(data);
		memcpy(data, udp_buff, sizeof(mesh_event_payload_t));

		// Also receive if there is any payload
		if(data->payload_length) {
			void *payload_data = malloc(data->payload_length);
			assert(payload_data);
			memcpy(payload_data, udp_buff + (int)sizeof(mesh_event_payload_t), data->payload_length);
			data->payload = payload_data;
		} else {
			data->payload = NULL;
		}

		// Push the event into the event queue
		assert(meshlink_queue_push(&event_queue, data));
	}

	return NULL;
}

// `event_handler' runs in a separate thread which invokes the event handle callback with
// event packet as argument returns from the thread when the callback returns `true' or timeout
static void *event_handler(void *argv) {
	bool callback_return = false;
	void *data;
	mesh_event_payload_t mesh_event_rec_packet;
	mesh_event_callback_t callback = *(mesh_event_callback_t *)argv;

	while(event_handle_thread_running) {

		// Pops the event if found in the event queue
		while((data = meshlink_queue_pop(&event_queue)) != NULL) {
			memcpy(&mesh_event_rec_packet, data, sizeof(mesh_event_payload_t));
			free(data);

			// Invokes the callback with the popped event packet
			callback_return = callback(mesh_event_rec_packet);

			if(mesh_event_rec_packet.payload_length) {
				free(mesh_event_rec_packet.payload);
			}

			// Return or Close the event handle thread if callback returns true
			if(callback_return) {
				set_cond_flag(&sync_event, true);
				event_handle_thread_running = false;
				break;
			}
		}
	}

	return NULL;
}

char *mesh_event_sock_create(const char *if_name) {
	struct sockaddr_in server = {0};
	char *ip;
	struct ifreq req_if = {0};
	struct sockaddr_in *resp_if_addr;

	assert(if_name);
	assert(!event_receive_thread_running);

	server_fd = socket(AF_INET, SOCK_DGRAM, 0);
	assert(server_fd >= 0);

	int reuse = 1;
	assert(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != -1);

	req_if.ifr_addr.sa_family = AF_INET;
	strncpy(req_if.ifr_name, if_name, IFNAMSIZ - 1);
	assert(ioctl(server_fd, SIOCGIFADDR, &req_if) != -1);
	resp_if_addr = (struct sockaddr_in *) & (req_if.ifr_addr);

	server.sin_family = AF_INET;
	server.sin_addr   = resp_if_addr->sin_addr;
	server.sin_port   = htons(atoi(SERVER_LISTEN_PORT));
	assert(bind(server_fd, (struct sockaddr *) &server, sizeof(struct sockaddr)) != -1);

	assert((ip = malloc(30)));
	strncpy(ip, inet_ntoa(resp_if_addr->sin_addr), 20);
	strcat(ip, ":");
	strcat(ip, SERVER_LISTEN_PORT);

	meshlink_queue_init(&event_queue);
	event_receive_thread_running = true;
	assert(!pthread_create(&event_receive_thread, NULL, event_receive_handler, NULL));

	return ip;
}

void mesh_event_sock_connect(const char *import) {
	assert(import);

	char *ip = strdup(import);
	assert(ip);
	char *port = strchr(ip, ':');
	assert(port);
	*port = '\0';
	port++;

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family      = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port        = htons(atoi(port));
	client_fd = socket(AF_INET, SOCK_DGRAM, 0);
	free(ip);
	assert(client_fd >= 0);
}

bool mesh_event_sock_send(int client_id, mesh_event_t event, const void *payload, size_t payload_length) {
	if(client_fd < 0) {
		fprintf(stderr, "mesh_event_sock_send called without calling mesh_event_sock_connect\n");
		return false;
	}

	if(client_id < 0 || event < 0 || event >= MAX_EVENT || (payload == NULL && payload_length)) {
		fprintf(stderr, "Invalid parameters\n");
		return false;
	}

	ssize_t send_size = sizeof(mesh_event_payload_t) + payload_length;
	char *send_packet = malloc(send_size);
	assert(send_packet);
	mesh_event_payload_t mesh_event_send_packet;

	mesh_event_send_packet.client_id   = client_id;
	mesh_event_send_packet.mesh_event  = event;
	mesh_event_send_packet.payload_length = payload_length;
	mesh_event_send_packet.payload = NULL;
	memcpy(send_packet, &mesh_event_send_packet, sizeof(mesh_event_send_packet));

	if(payload_length) {
		memcpy(send_packet + sizeof(mesh_event_send_packet), payload, payload_length);
	}

	ssize_t send_ret = sendto(client_fd, send_packet, send_size, 0, (const struct sockaddr *) &server_addr, sizeof(server_addr));
	free(send_packet);

	if(send_ret < 0) {
		perror("sendto status");
		return false;
	} else {
		return true;
	}
}

bool wait_for_event(mesh_event_callback_t callback, int seconds) {
	if(callback == NULL || seconds == 0) {
		fprintf(stderr, "Invalid parameters\n");
		return false;
	}

	if(event_handle_thread_running) {
		fprintf(stderr, "Event handle thread is already running\n");
		return false;
	} else {
		event_handle_thread_running = true;
	}

	set_cond_flag(&sync_event, false);
	assert(!pthread_create(&event_handle_thread, NULL, event_handler, (void *)&callback));
	bool wait_ret = wait_cond_flag(&sync_event, seconds);
	event_handle_thread_running = false;
	pthread_cancel(event_handle_thread);

	return wait_ret;
}

void mesh_events_flush(void) {
	mesh_event_payload_t *data;

	while((data = meshlink_queue_pop(&event_queue)) != NULL) {
		if(data->payload_length) {
			free(data->payload);
		}

		free(data);
	}
}

void mesh_event_destroy(void) {
	mesh_events_flush();
	event_receive_thread_running = false;
	pthread_cancel(event_receive_thread);
}
