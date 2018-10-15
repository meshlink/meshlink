/*
    test_cases_channel_shutdown.c -- Execution of specific meshlink black box test cases
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

#include "execute_tests.h"
#include "test_cases_channel_shutdown.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

static void test_case_mesh_channel_shutdown_01(void **state);
static bool test_steps_mesh_channel_shutdown_01(void);
static void test_case_mesh_channel_shutdown_02(void **state);
static bool test_steps_mesh_channel_shutdown_02(void);
static void test_case_mesh_channel_shutdown_03(void **state);
static bool test_steps_mesh_channel_shutdown_03(void);
static void test_case_mesh_channel_shutdown_04(void **state);
static bool test_steps_mesh_channel_shutdown_04(void);
static void test_case_mesh_channel_shutdown_05(void **state);
static bool test_steps_mesh_channel_shutdown_05(void);

/* State structure for meshlink_channel_shutdown Test Case #1 */
static black_box_state_t test_mesh_channel_shutdown_01_state = {
    .test_case_name = "test_case_mesh_channel_shutdown_01",
};

/* State structure for meshlink_channel_shutdown Test Case #2 */
static black_box_state_t test_mesh_channel_shutdown_02_state = {
    .test_case_name = "test_case_mesh_channel_shutdown_02",
};

/* State structure for meshlink_channel_shutdown Test Case #3 */
static black_box_state_t test_mesh_channel_shutdown_03_state = {
    .test_case_name = "test_case_mesh_channel_shutdown_03",
};

/* State structure for meshlink_channel_shutdown Test Case #4 */
static black_box_state_t test_mesh_channel_shutdown_04_state = {
    .test_case_name = "test_case_mesh_channel_shutdown_04",
};

/* State structure for meshlink_channel_shutdown Test Case #5 */
static black_box_state_t test_mesh_channel_shutdown_05_state = {
    .test_case_name = "test_case_mesh_channel_shutdown_05",
};

/* Execute meshlink_channel_shutdown Test Case # 1*/
static void test_case_mesh_channel_shutdown_01(void **state) {
	 execute_test(test_steps_mesh_channel_shutdown_01, state);
   return;
}

static volatile bool bar_reachable = false;
static volatile bool bar_responded = false;

static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(!strcmp(node->name, "bar")) {
		bar_reachable = reachable;
	}
}

static void foo_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;

	printf("foo_receive_cb %zu: ", len);
	fwrite(data, 1, len, stdout);
	printf("\n");
	if(len == 5 && !memcmp(data, "Hello", 5)) {
		bar_responded = true;
	}
}

static void bar_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	printf("bar_receive_cb %zu: ", len);
	fwrite(data, 1, len, stdout);
	printf("\n");
	// Echo the data back.
	meshlink_channel_send(mesh, channel, data, len);
}

static bool reject_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)port;
	(void)data;
	(void)len;
	printf("reject_cb: (from %s on port %u) ", channel->node->name, (unsigned int)port);
	return false;
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	printf("accept_cb: (from %s on port %u) ", channel->node->name, (unsigned int)port);

	if(data) {
		fwrite(data, 1, len, stdout);
	}

	printf("\n");

	if(port != 7) {
		return false;
	}

	meshlink_set_channel_receive_cb(mesh, channel, bar_receive_cb);

	if(data) {
		bar_receive_cb(mesh, channel, data, len);
	}

	return true;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);

	if(meshlink_channel_send(mesh, channel, "Hello", 5) != 5) {
		fprintf(stderr, "Could not send whole message\n");
	}
sleep(2);
	meshlink_channel_shutdown(mesh, channel, SHUT_WR);
}


/* Test Steps for meshlink_channel_shutdown Test Case # 1*/
static bool test_steps_mesh_channel_shutdown_01(void) {
	bool result = false;
	meshlink_destroy("chan_shutdown_conf.1");
	meshlink_destroy("chan_shutdown_conf.2");
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("chan_shutdown_conf.1", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return false;
	}

	meshlink_handle_t *mesh2 = meshlink_open("chan_shutdown_conf.2", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return false;
	}

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");

	char *data = meshlink_export(mesh1);

	if(!data) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh2, data)) {
		fprintf(stderr, "Bar could not import foo's configuration\n");
		return false;
	}

	free(data);

	data = meshlink_export(mesh2);

	if(!data) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh1, data)) {
		fprintf(stderr, "Foo could not import bar's configuration\n");
		return false;
	}

	free(data);

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh1, reject_cb);
	meshlink_set_channel_accept_cb(mesh2, accept_cb);

	meshlink_set_node_status_cb(mesh1, status_cb);

	// Start both instances

	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return false;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return false;
	}

	// Wait for the two to connect.

	for(int i = 0; i < 20; i++) {
		sleep(1);

		if(bar_reachable) {
			break;
		}
	}

	if(!bar_reachable) {
		fprintf(stderr, "Bar not reachable for foo after 20 seconds\n");
		return false;
	}

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");

	if(!bar) {
		fprintf(stderr, "Foo could not find bar\n");
		return false;
	}

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, foo_receive_cb, NULL, 0);

	meshlink_set_channel_poll_cb(mesh1, channel, poll_cb);

	for(int i = 0; i < 20; i++) {
		sleep(1);

		if(bar_responded) {
			break;
		}
	}

	if(!bar_responded) {
		fprintf(stderr, "Bar did not respond to foo's channel message\n");
		return false;
	}

	meshlink_channel_close(mesh1, channel);

	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("chan_shutdown_conf.1");
	meshlink_destroy("chan_shutdown_conf.2");

	return true;
}

/* Execute meshlink_channel_shutdown Test Case # 2*/
static void test_case_mesh_channel_shutdown_02(void **state) {
	 execute_test(test_steps_mesh_channel_shutdown_02, state);
   return;
}

static bool accept_cb1(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	printf("accept_cb: (from %s on port %u) ", channel->node->name, (unsigned int)port);

	if(data) {
		fwrite(data, 1, len, stdout);
	}

	printf("\n");

	if(port != 7) {
		return false;
	}
sleep(1);
	meshlink_channel_shutdown(mesh, channel, SHUT_RD);
sleep(1);
	meshlink_set_channel_receive_cb(mesh, channel, bar_receive_cb);

	if(data) {
		bar_receive_cb(mesh, channel, data, len);
	}

	return true;
}

static void poll_cb1(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);

	if(meshlink_channel_send(mesh, channel, "Hello", 5) != 5) {
		fprintf(stderr, "Could not send whole message\n");
	}
//sleep(2);
	//meshlink_channel_shutdown(mesh, channel, SHUT_WR);
}


/* Test Steps for meshlink_channel_shutdown Test Case # 2*/
static bool test_steps_mesh_channel_shutdown_02(void) {
	bool result = false;
	meshlink_destroy("chan_shutdown_conf.3");
	meshlink_destroy("chan_shutdown_conf.4");
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("chan_shutdown_conf.3", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return false;
	}

	meshlink_handle_t *mesh2 = meshlink_open("chan_shutdown_conf.4", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return false;
	}

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");

	char *data = meshlink_export(mesh1);

	if(!data) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh2, data)) {
		fprintf(stderr, "Bar could not import foo's configuration\n");
		return false;
	}

	free(data);

	data = meshlink_export(mesh2);

	if(!data) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh1, data)) {
		fprintf(stderr, "Foo could not import bar's configuration\n");
		return false;
	}

	free(data);

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh1, reject_cb);
	meshlink_set_channel_accept_cb(mesh2, accept_cb1);

	meshlink_set_node_status_cb(mesh1, status_cb);

	// Start both instances

	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return false;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return false;
	}

	// Wait for the two to connect.

	for(int i = 0; i < 20; i++) {
		sleep(1);

		if(bar_reachable) {
			break;
		}
	}

	if(!bar_reachable) {
		fprintf(stderr, "Bar not reachable for foo after 20 seconds\n");
		return false;
	}

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");

	if(!bar) {
		fprintf(stderr, "Foo could not find bar\n");
		return false;
	}

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, foo_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh1, channel, poll_cb1);

	for(int i = 0; i < 20; i++) {
		sleep(1);

		if(bar_responded) {
			break;
		}
	}

	if(!bar_responded) {
		fprintf(stderr, "Bar did not respond to foo's channel message\n");
		return false;
	}

	meshlink_channel_close(mesh1, channel);

	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("chan_shutdown_conf.3");
	meshlink_destroy("chan_shutdown_conf.4");

	return true;
}

/* Execute meshlink_channel_shutdown Test Case # 3*/
static void test_case_mesh_channel_shutdown_03(void **state) {
	 execute_test(test_steps_mesh_channel_shutdown_03, state);
   return;
}

static bool accept_cb2(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	printf("accept_cb: (from %s on port %u) ", channel->node->name, (unsigned int)port);

	if(data) {
		fwrite(data, 1, len, stdout);
	}

	printf("\n");

	if(port != 7) {
		return false;
	}
	meshlink_set_channel_receive_cb(mesh, channel, bar_receive_cb);

	if(data) {
		bar_receive_cb(mesh, channel, data, len);
	}

	return true;
}

static void poll_cb2(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);

	if(meshlink_channel_send(mesh, channel, "Hello", 5) != 5) {
		fprintf(stderr, "Could not send whole message\n");
	}
sleep(2);
	meshlink_channel_shutdown(mesh, channel, SHUT_RDWR);
}


/* Test Steps for meshlink_channel_shutdown Test Case # 3*/
static bool test_steps_mesh_channel_shutdown_03(void) {
	bool result = false;
	meshlink_destroy("chan_shutdown_conf.5");
	meshlink_destroy("chan_shutdown_conf.6");
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("chan_shutdown_conf.5", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return false;
	}

	meshlink_handle_t *mesh2 = meshlink_open("chan_shutdown_conf.6", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return false;
	}

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");

	char *data = meshlink_export(mesh1);

	if(!data) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh2, data)) {
		fprintf(stderr, "Bar could not import foo's configuration\n");
		return false;
	}

	free(data);

	data = meshlink_export(mesh2);

	if(!data) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh1, data)) {
		fprintf(stderr, "Foo could not import bar's configuration\n");
		return false;
	}

	free(data);

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh1, reject_cb);
	meshlink_set_channel_accept_cb(mesh2, accept_cb2);

	meshlink_set_node_status_cb(mesh1, status_cb);

	// Start both instances

	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return false;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return false;
	}

	// Wait for the two to connect.

	for(int i = 0; i < 20; i++) {
		sleep(1);

		if(bar_reachable) {
			break;
		}
	}

	if(!bar_reachable) {
		fprintf(stderr, "Bar not reachable for foo after 20 seconds\n");
		return false;
	}

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");

	if(!bar) {
		fprintf(stderr, "Foo could not find bar\n");
		return false;
	}

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, foo_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh1, channel, poll_cb2);

	for(int i = 0; i < 20; i++) {
		sleep(1);

		if(bar_responded) {
			break;
		}
	}

	if(!bar_responded) {
		fprintf(stderr, "Bar did not respond to foo's channel message\n");
		return false;
	}

	meshlink_channel_close(mesh1, channel);

	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("chan_shutdown_conf.5");
	meshlink_destroy("chan_shutdown_conf.6");

	return true;
}

/* Execute meshlink_channel_shutdown Test Case # 4*/
static void test_case_mesh_channel_shutdown_04(void **state) {
	 execute_test(test_steps_mesh_channel_shutdown_04, state);
   return;
}

static bool accept_cb3(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	printf("accept_cb: (from %s on port %u) ", channel->node->name, (unsigned int)port);

	if(data) {
		fwrite(data, 1, len, stdout);
	}

	printf("\n");

	if(port != 7) {
		return false;
	}
	meshlink_set_channel_receive_cb(mesh, channel, bar_receive_cb);

	if(data) {
		bar_receive_cb(mesh, channel, data, len);
	}

	return true;
}

static void poll_cb3(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);

	if(meshlink_channel_send(mesh, channel, "Hello", 5) != 5) {
		fprintf(stderr, "Could not send whole message\n");
	}
sleep(2);
	meshlink_channel_shutdown(NULL, channel, SHUT_RDWR);
}


/* Test Steps for meshlink_channel_shutdown Test Case # 4*/
static bool test_steps_mesh_channel_shutdown_04(void) {
	bool result = false;
	meshlink_destroy("chan_shutdown_conf.7");
	meshlink_destroy("chan_shutdown_conf.8");
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("chan_shutdown_conf.7", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return false;
	}

	meshlink_handle_t *mesh2 = meshlink_open("chan_shutdown_conf.8", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return false;
	}

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");

	char *data = meshlink_export(mesh1);

	if(!data) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh2, data)) {
		fprintf(stderr, "Bar could not import foo's configuration\n");
		return false;
	}

	free(data);

	data = meshlink_export(mesh2);

	if(!data) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh1, data)) {
		fprintf(stderr, "Foo could not import bar's configuration\n");
		return false;
	}

	free(data);

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh1, reject_cb);
	meshlink_set_channel_accept_cb(mesh2, accept_cb3);

	meshlink_set_node_status_cb(mesh1, status_cb);

	// Start both instances

	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return false;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return false;
	}

	// Wait for the two to connect.

	for(int i = 0; i < 20; i++) {
		sleep(1);

		if(bar_reachable) {
			break;
		}
	}

	if(!bar_reachable) {
		fprintf(stderr, "Bar not reachable for foo after 20 seconds\n");
		return false;
	}

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");

	if(!bar) {
		fprintf(stderr, "Foo could not find bar\n");
		return false;
	}

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, foo_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh1, channel, poll_cb3);

	for(int i = 0; i < 20; i++) {
		sleep(1);

		if(bar_responded) {
			break;
		}
	}

	if(!bar_responded) {
		fprintf(stderr, "Bar did not respond to foo's channel message\n");
		return false;
	}

	meshlink_channel_close(mesh1, channel);

	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("chan_shutdown_conf.7");
	meshlink_destroy("chan_shutdown_conf.8");

	return true;
}

/* Execute meshlink_channel_shutdown Test Case # 5*/
static void test_case_mesh_channel_shutdown_05(void **state) {
	 execute_test(test_steps_mesh_channel_shutdown_05, state);
   return;
}

static bool accept_cb4(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	printf("accept_cb: (from %s on port %u) ", channel->node->name, (unsigned int)port);

	if(data) {
		fwrite(data, 1, len, stdout);
	}

	printf("\n");

	if(port != 7) {
		return false;
	}
	meshlink_set_channel_receive_cb(mesh, channel, bar_receive_cb);

	if(data) {
		bar_receive_cb(mesh, channel, data, len);
	}

	return true;
}

static void poll_cb4(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);

	if(meshlink_channel_send(mesh, channel, "Hello", 5) != 5) {
		fprintf(stderr, "Could not send whole message\n");
	}
sleep(2);
	meshlink_channel_shutdown(mesh, NULL, SHUT_RDWR);
}


/* Test Steps for meshlink_channel_shutdown Test Case # 5*/
static bool test_steps_mesh_channel_shutdown_05(void) {
	bool result = false;
	meshlink_destroy("chan_shutdown_conf.9");
	meshlink_destroy("chan_shutdown_conf.10");
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("chan_shutdown_conf.9", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return false;
	}

	meshlink_handle_t *mesh2 = meshlink_open("chan_shutdown_conf.10", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return false;
	}

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");

	char *data = meshlink_export(mesh1);

	if(!data) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh2, data)) {
		fprintf(stderr, "Bar could not import foo's configuration\n");
		return false;
	}

	free(data);

	data = meshlink_export(mesh2);

	if(!data) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh1, data)) {
		fprintf(stderr, "Foo could not import bar's configuration\n");
		return false;
	}

	free(data);

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh1, reject_cb);
	meshlink_set_channel_accept_cb(mesh2, accept_cb4);

	meshlink_set_node_status_cb(mesh1, status_cb);

	// Start both instances

	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return false;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return false;
	}

	// Wait for the two to connect.

	for(int i = 0; i < 20; i++) {
		sleep(1);

		if(bar_reachable) {
			break;
		}
	}

	if(!bar_reachable) {
		fprintf(stderr, "Bar not reachable for foo after 20 seconds\n");
		return false;
	}

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");

	if(!bar) {
		fprintf(stderr, "Foo could not find bar\n");
		return false;
	}

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, foo_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh1, channel, poll_cb4);

	for(int i = 0; i < 20; i++) {
		sleep(1);

		if(bar_responded) {
			break;
		}
	}

	if(!bar_responded) {
		fprintf(stderr, "Bar did not respond to foo's channel message\n");
		return false;
	}

	meshlink_channel_close(mesh1, channel);

	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("chan_shutdown_conf.9");
	meshlink_destroy("chan_shutdown_conf.10");

	return true;
}

int test_meshlink_channel_shutdown(void) {
		const struct CMUnitTest blackbox_channel_shutdown_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_shutdown_01, NULL, NULL,
            (void *)&test_mesh_channel_shutdown_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_shutdown_02, NULL, NULL,
            (void *)&test_mesh_channel_shutdown_02_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_shutdown_03, NULL, NULL,
            (void *)&test_mesh_channel_shutdown_03_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_shutdown_04, NULL, NULL,
            (void *)&test_mesh_channel_shutdown_04_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_shutdown_05, NULL, NULL,
            (void *)&test_mesh_channel_shutdown_05_state)
		};
  total_tests += sizeof(blackbox_channel_shutdown_tests) / sizeof(blackbox_channel_shutdown_tests[0]);

  return cmocka_run_group_tests(blackbox_channel_shutdown_tests, NULL, NULL);
}
