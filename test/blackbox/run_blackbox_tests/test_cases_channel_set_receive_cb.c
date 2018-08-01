
/*
    test_cases_add_ex_addr.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>
                        Manav Kumar Mehta <manavkumarm@yahoo.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty o
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "execute_tests.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
#include "test_cases_channel_set_receive_cb.h"

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG
#define CHAT_PORT 8000
#define TCP_TEST 9000

static bool rec_stat;
static char rec_msg[TCP_TEST];

static void channel_receive(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
  static int j = 0;
  int i;
  char *data = (char *) dat;
	if(!len) {
		if(meshlink_errno) {
			fprintf(stderr, "Error while reading data from %s: %s\n", channel->node->name, meshlink_strerror(meshlink_errno));
		} else {
			fprintf(stderr, "Chat connection closed by %s\n", channel->node->name);
		}

		channel->node->priv = NULL;
		rec_stat = true;
		meshlink_channel_close(mesh, channel);
		return;
	}

	rec_stat = true;
	if (len != TCP_TEST) {
	  fprintf(stderr, "%s says: ", channel->node->name);
    fwrite(data, len, 1, stderr);
	  fputc('\n', stderr);
	}
	else {
    for (i = 0; i < len; j++, i++) {
      rec_msg[j] = data[i];
    }
	}

}

static void channel_poll(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	fprintf(stderr, "Channel to '%s' connected\n", channel->node->name);
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
}

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)data;
	(void)len;

	fprintf(stderr, "Accepted incoming channel from '%s'\n", channel->node->name);

	// Remember the channel
	channel->node->priv = channel;

	// Accept this channel
	return true;
}

/* Execute meshlink_channel_set_receive_cb Test Case # 1 */
void test_case_set_channel_receive_cb_01(void **state) {
  execute_test(test_steps_set_channel_receive_cb_01, state);
  return;
}

bool test_steps_set_channel_receive_cb_01(void) {
  meshlink_destroy("channelreceiveconf");
  fprintf(stderr, "[ channel receive 01 ] Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  meshlink_handle_t *mesh_handle = meshlink_open("channelreceiveconf", "nut", "node_sim", 1);
  fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle != NULL);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "[ channel receive 01 ] disabling channel_accept callback for NUT\n");
  meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  fprintf(stderr, "[ channel receive 01 ] Starting NUT\n");
  assert(meshlink_start(mesh_handle));

  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  sleep(1);
  fprintf(stderr, "[ channel receive 01 ] Opening channel for NUT(ourself) UDP semantic\n");
  meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_handle, node, 8000, NULL, NULL, 0, MESHLINK_CHANNEL_UDP);
  assert(channel != NULL);

  meshlink_set_channel_poll_cb(mesh_handle, channel, channel_poll);

  rec_stat = false;
  fprintf(stderr, "[ channel receive 01 ] Setting channel for NUT using meshlink_set_channel_receive_cb API\n");
  meshlink_set_channel_receive_cb(mesh_handle, channel, channel_receive);

  char *msg = "Test\n";
  fprintf(stderr, "[ channel receive 01 ] Sending msg to ourself\n");
  ssize_t send_ret = meshlink_channel_send(mesh_handle, channel, msg, strlen(msg));
  fprintf(stderr, "meshlink_ocahnnel_send status: %s\n", meshlink_strerror(meshlink_errno));
  assert( send_ret > 0);

  sleep(2);
  if (rec_stat) {
    fprintf(stderr, "[ channel receive 01 ] receive callback invoked correctly\n");
  }
  else {
    fprintf(stderr, "[ channel receive 01 ] receive callback didnt invoke after setting using meshlink_set_channel_receive_cb\n");
  }

  meshlink_channel_close(mesh_handle, channel);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("channelreceiveconf");

  return rec_stat;
}

/* Execute meshlink_channel_set_receive_cb Test Case # 2 */
void test_case_set_channel_receive_cb_02(void **state) {
  execute_test(test_steps_set_channel_receive_cb_02, state);
  return;
}

bool test_steps_set_channel_receive_cb_02(void) {
  meshlink_destroy("channelreceiveconf");
  fprintf(stderr, "[ channel receive 02 ] Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  meshlink_handle_t *mesh_handle = meshlink_open("channelreceiveconf", "nut", "node_sim", 1);
  fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle != NULL);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "[ channel receive 02 ] disabling channel_accept callback for NUT\n");
  meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  fprintf(stderr, "[ channel receive 02 ] Starting NUT\n");
  assert(meshlink_start(mesh_handle));

  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  sleep(1);
  fprintf(stderr, "[ channel receive 02 ] Opening channel for NUT(ourself) UDP semantic\n");
  meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_handle, node, 8000, NULL, NULL, 0, MESHLINK_CHANNEL_UDP);
  assert(channel != NULL);

  meshlink_set_channel_poll_cb(mesh_handle, channel, channel_poll);

  rec_stat = false;
  fprintf(stderr, "[ channel receive 02 ] Setting channel for NUT using meshlink_set_channel_receive_cb API\n");
  meshlink_set_channel_receive_cb(NULL, channel, channel_receive);

  if (meshlink_errno == MESHLINK_EINVAL) {
    fprintf(stderr, "[channel receive 02] receive callback reported error successfully when NULL is passed as mesh argument\n");
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelreceiveconf");
    return true;
  }
  else {
    fprintf(stderr, "[channel receive 02] receive callback didn't report error when NULL is passed as mesh argument\n");
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelreceiveconf");
    return false;
  }
}

/* Execute meshlink_channel_set_receive_cb Test Case # 3 */
void test_case_set_channel_receive_cb_03(void **state) {
  execute_test(test_steps_set_channel_receive_cb_03, state);
  return;
}

bool test_steps_set_channel_receive_cb_03(void) {
  meshlink_destroy("channelreceiveconf");
  fprintf(stderr, "[ channel receive 03 ] Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  meshlink_handle_t *mesh_handle = meshlink_open("channelreceiveconf", "nut", "node_sim", 1);
  fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle != NULL);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "[ channel receive 03 ] disabling channel_accept callback for NUT\n");
  meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  fprintf(stderr, "[ channel receive 03 ] Starting NUT\n");
  assert(meshlink_start(mesh_handle));

  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  sleep(1);
  fprintf(stderr, "[ channel receive 03 ] Setting channel for NUT using meshlink_set_channel_receive_cb API\n");
  meshlink_set_channel_receive_cb(mesh_handle, NULL, channel_receive);

  if (meshlink_errno == MESHLINK_EINVAL) {
    fprintf(stderr, "[channel receive 03] receive callback reported error successfully when NULL is passed as channel argument\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelreceiveconf");
    return true;
  }
  else {
    fprintf(stderr, "[channel receive 03] receive callback didn't report error when NULL is passed as channel argument\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelreceiveconf");
    return false;
  }
}

/* Execute meshlink_channel_set_receive_cb Test Case # 4 */
void test_case_set_channel_receive_cb_04(void **state) {
  execute_test(test_steps_set_channel_receive_cb_04, state);
  return;
}

bool test_steps_set_channel_receive_cb_04(void) {
  meshlink_destroy("channelreceiveconf");
  fprintf(stderr, "[ channel receive 04 ] Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  meshlink_handle_t *mesh_handle = meshlink_open("channelreceiveconf", "nut", "node_sim", 1);
  fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle != NULL);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "[ channel receive 04 ] disabling channel_accept callback for NUT\n");
  meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  fprintf(stderr, "[ channel receive 04 ] Starting NUT\n");
  assert(meshlink_start(mesh_handle));

  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  sleep(1);
  fprintf(stderr, "[ channel receive 04 ] Opening channel for NUT(ourself) UDP semantic\n");
  meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_handle, node, 8000, NULL, NULL, 0, MESHLINK_CHANNEL_UDP);
  assert(channel != NULL);

  meshlink_set_channel_poll_cb(mesh_handle, channel, channel_poll);

  rec_stat = false;
  fprintf(stderr, "[ channel receive 04 ] Setting channel for NUT using meshlink_set_channel_receive_cb API\n");
  meshlink_set_channel_receive_cb(mesh_handle, channel, channel_receive);

  char send_msg[TCP_TEST];
  int i;
  for (i = 0; i < TCP_TEST; i++) {
    if (i < 3000) {
      send_msg[i] = 'A';
    }
    else {
      send_msg[i] = 'B';
    }
  }
  fprintf(stderr, "[ channel receive 04 ] Sending msg to ourself\n");
  assert(meshlink_channel_send(mesh_handle, channel, send_msg, TCP_TEST) > 0);

  sleep(1);

  if (rec_stat) {
    fprintf(stderr, "[ channel receive 04 ] receive callback invoked correctly\n");
  }
  else {
    fprintf(stderr, "[ channel receive 04 ] receive callback didnt invoke after setting using meshlink_set_channel_receive_cb\n");
  }

  bool ret = false;

  for (i = 0; i < TCP_TEST; i++) {
    if (i < 3000) {
      if (rec_msg[i] != 'A') {
        break;
      }
    }
    else {
      if (rec_msg[i] != 'B') {
        break;
      }
    }
  }
  if (i == TCP_TEST) {
    fprintf(stderr, "[ channel receive 04 ] receive callback received packets orderly\n");
    ret = true;
  }
  else {
    fprintf(stderr, "[ channel receive 04 ] receive callback didn't receive packets orderly\n");
    ret = false;
  }

  meshlink_channel_close(mesh_handle, channel);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("channelreceiveconf");

  return ret;
}
