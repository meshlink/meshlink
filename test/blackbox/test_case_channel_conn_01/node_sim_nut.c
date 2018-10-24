/*
    node_sim_nut.c -- Implementation of Node Simulation for Meshlink Testing
                    for meta connection test case 01 - re-connection of
                    two nodes when relay node goes down
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <signal.h>
#include "../common/common_handlers.h"
#include "../common/test_step.h"
#include "../common/mesh_event_handler.h"

#define CMD_LINE_ARG_NODENAME   1
#define CMD_LINE_ARG_DEVCLASS   2
#define CMD_LINE_ARG_CLIENTID   3
#define CMD_LINE_ARG_IMPORTSTR  4
#define CMD_LINE_ARG_INVITEURL  5
#define CHANNEL_PORT 1234

static bool sigusr1_received;
static int client_id = -1;
static bool peer_reachable;
static bool channel_opened;
static bool channel_closed;

static pthread_mutex_t reachable_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reachable_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t lock_channel_closed = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t channel_closed_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t lock_receive = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t receive_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t lock_sigusr = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sigusr_cond = PTHREAD_COND_INITIALIZER;

static void send_event(mesh_event_t event);
static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node,
                                        bool reachable);
static void mesh_siguser1_signal_handler(int sig_num);

static void mesh_siguser1_signal_handler(int sig_num) {
  pthread_mutex_lock(&lock_sigusr);
  sigusr1_received = true;
  assert(!pthread_cond_broadcast(&sigusr_cond));
  pthread_mutex_unlock(&lock_sigusr);

  return;
}

static void send_event(mesh_event_t event) {
  bool send_ret = false;
  int attempts;
  for(attempts = 0; attempts < 5; attempts += 1) {
    send_ret = mesh_event_sock_send(client_id, event, NULL, 0);
    if(send_ret) {
      break;
    }
  }
  assert(send_ret);

  return;
}

static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node,
                                        bool reachable) {
    if(!strcasecmp(node->name, "peer")) {
      pthread_mutex_lock(&reachable_lock);
      peer_reachable = reachable;
      assert(!pthread_cond_broadcast(&reachable_cond));
      pthread_mutex_unlock(&reachable_lock);
    }

    return;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;
  meshlink_set_channel_poll_cb(mesh, channel, NULL);
	assert(meshlink_channel_send(mesh, channel, "test", 5) >= 0);
	return;
}

/* channel receive callback */
static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
  if(len == 0) {
    if(!meshlink_errno) {
      pthread_mutex_lock(& lock_channel_closed);
      channel_closed = true;
      assert(!pthread_cond_broadcast(&channel_closed_cond));
      pthread_mutex_unlock(& lock_channel_closed);

      return;
    } else {
      pthread_mutex_lock(& lock_channel_closed);
      channel_closed = true;
      assert(!pthread_cond_broadcast(&channel_closed_cond));
      pthread_mutex_unlock(& lock_channel_closed);

      return;
    }
  }

  if(!strcmp(channel->node->name, "peer") && len == 5 && !memcmp(dat, "reply", 5)) {
    pthread_mutex_lock(& lock_receive);
    channel_opened = true;
    assert(!pthread_cond_broadcast(&receive_cond));
    pthread_mutex_unlock(& lock_receive);
  }

  return;
}

int main(int argc, char *argv[]) {
  struct timespec timeout = {0};
  int i;

  // Import mesh event handler

  if((argv[CMD_LINE_ARG_CLIENTID]) && (argv[CMD_LINE_ARG_IMPORTSTR] )) {
    client_id = atoi(argv[CMD_LINE_ARG_CLIENTID]);
    mesh_event_sock_connect(argv[CMD_LINE_ARG_IMPORTSTR]);
  }

  // Setup required signals

  signal(SIGUSR1, mesh_siguser1_signal_handler);

  // Execute test steps

  meshlink_handle_t *mesh = meshlink_open("testconf", argv[CMD_LINE_ARG_NODENAME],
                              "test_channel_conn", atoi(argv[CMD_LINE_ARG_DEVCLASS]));
  assert(mesh);
  meshlink_set_log_cb(mesh, MESHLINK_DEBUG, meshlink_callback_logger);
  meshlink_set_node_status_cb(mesh, node_status_cb);

  if(argv[CMD_LINE_ARG_INVITEURL]) {
    assert(meshlink_join(mesh, argv[CMD_LINE_ARG_INVITEURL]));
  }
  assert(meshlink_start(mesh));

  // Wait for peer node to join

  timeout.tv_sec = time(NULL) + 5;
  pthread_mutex_lock(&reachable_lock);
  if(peer_reachable == false) {
    assert(!pthread_cond_timedwait(&reachable_cond, &reachable_lock, &timeout));
  }
  pthread_mutex_unlock(&reachable_lock);
  assert(peer_reachable);
  send_event(NODE_JOINED);

  // Open a channel to peer node

  meshlink_node_t *peer_node = meshlink_get_node(mesh, "peer");
  assert(peer_node);
  meshlink_channel_t *channel = meshlink_channel_open(mesh, peer_node, CHANNEL_PORT,
                                      channel_receive_cb, NULL, 0);
  meshlink_set_channel_poll_cb(mesh, channel, poll_cb);

  timeout.tv_sec = time(NULL) + 10;
  pthread_mutex_lock(&lock_receive);
  if(channel_opened == false) {
    assert(!pthread_cond_timedwait(&receive_cond, &lock_receive, &timeout));
  }
  pthread_mutex_unlock(&lock_receive);
  assert(channel_opened);
  send_event(CHANNEL_OPENED);

  timeout.tv_sec = time(NULL) + 120;
  pthread_mutex_lock(&lock_sigusr);
  if(sigusr1_received == false) {
    assert(!pthread_cond_timedwait(&sigusr_cond, &lock_sigusr, &timeout));
  }
  pthread_mutex_unlock(&lock_sigusr);

  timeout.tv_sec = time(NULL) + 10;
  pthread_mutex_lock(&reachable_lock);
  if(peer_reachable == false) {
    assert(!pthread_cond_timedwait(&reachable_cond, &reachable_lock, &timeout));
  }
  pthread_mutex_unlock(&reachable_lock);
  assert(peer_reachable);

  assert(meshlink_channel_send(mesh, channel, "test2", 5) >= 0);

  timeout.tv_sec = time(NULL) + 10;
  pthread_mutex_lock(& lock_channel_closed);
  if(channel_closed == false) {
    assert(!pthread_cond_timedwait(&channel_closed_cond, &lock_channel_closed, &timeout));
  }
  pthread_mutex_unlock(&lock_channel_closed);
  assert(channel_closed);
  send_event(ERR_NETWORK);

  meshlink_close(mesh);
}
