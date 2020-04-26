#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "../src/meshlink.h"
#include "utils.h"

#define SMALL_SIZE 512
#define SMALL_COUNT 2500
#define LARGE_SIZE 131072

static struct sync_flag accept_flag;

struct client {
	meshlink_handle_t *mesh;
	meshlink_channel_t *channel;
	size_t received;
	bool got_large_packet;
	struct sync_flag close_flag;
};

static void client_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	assert(mesh->priv);
	struct client *client = mesh->priv;

	if(!data && !len) {
		set_sync_flag(&client->close_flag, true);
		meshlink_channel_close(mesh, channel);
		return;
	}

	// We expect always the same amount of data from the server.
	assert(len == 512 || len == LARGE_SIZE);
	client->received += len;

	if(len == LARGE_SIZE) {
		client->got_large_packet = true;
	}
}

static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	assert(mesh->priv);
	struct client *client = mesh->priv;

	if(reachable && !strcmp(node->name, "server")) {
		assert(!client->channel);
		client->channel = meshlink_channel_open_ex(mesh, node, 1, client_receive_cb, NULL, 0, MESHLINK_CHANNEL_UDP);
		assert(client->channel);
	}
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)data;
	(void)len;

	assert(port == 1);
	assert(meshlink_channel_get_flags(mesh, channel) == MESHLINK_CHANNEL_UDP);

	assert(mesh->priv);
	meshlink_channel_t **c = mesh->priv;

	for(int i = 0; i < 3; i++) {
		if(c[i] == NULL) {
			c[i] = channel;

			if(i == 2) {
				set_sync_flag(&accept_flag, true);
			}

			return true;
		}
	}

	return false;
}

int main(void) {
	init_sync_flag(&accept_flag);

	meshlink_set_log_cb(NULL, MESHLINK_WARNING, log_cb);

	// Open four new meshlink instance, the server and three peers.

	const char *names[3] = {"foo", "bar", "baz"};
	struct client clients[3];
	meshlink_channel_t *channels[3] = {NULL, NULL, NULL};
	memset(clients, 0, sizeof(clients));

	assert(meshlink_destroy("channels_udp_conf.0"));
	meshlink_handle_t *server = meshlink_open("channels_udp_conf.0", "server", "channels-udp", DEV_CLASS_BACKBONE);
	assert(server);
	meshlink_enable_discovery(server, false);
	server->priv = channels;
	meshlink_set_channel_accept_cb(server, accept_cb);
	assert(meshlink_start(server));

	for(int i = 0; i < 3; i++) {
		char dir[100];
		snprintf(dir, sizeof(dir), "channels_udp_conf.%d", i + 1);
		assert(meshlink_destroy(dir));
		init_sync_flag(&clients[i].close_flag);
		clients[i].mesh = meshlink_open(dir, names[i], "channels-udp", DEV_CLASS_STATIONARY);
		assert(clients[i].mesh);
		clients[i].mesh->priv = &clients[i];
		meshlink_enable_discovery(clients[i].mesh, false);
		link_meshlink_pair(server, clients[i].mesh);
		meshlink_set_node_status_cb(clients[i].mesh, status_cb);
		assert(meshlink_start(clients[i].mesh));
	}

	// Wait for all three channels to connect

	assert(wait_sync_flag(&accept_flag, 10));

	for(int i = 0; i < 3; i++) {
		assert(channels[i]);
		assert(clients[i].channel);
	}

	// Check that we can send up to 65535 bytes without errors

	char large_data[LARGE_SIZE] = "";

	for(int i = 0; i < 3; i++) {
		assert(meshlink_channel_send(server, channels[i], large_data, sizeof(large_data)) == sizeof(large_data));
	}

	// Assert that packets larger than 16 MiB are not allowed

	assert(meshlink_channel_send(server, channels[0], large_data, 16777216) == -1);

	// Stream packets from server to clients for 5 seconds at 40 Mbps (1 kB * 500 Hz)

	char data[SMALL_SIZE];
	memset(data, 'U', sizeof(data));

	for(int j = 0; j < SMALL_COUNT; j++) {
		const struct timespec req = {0, 2000000};
		nanosleep(&req, NULL);

		for(int i = 0; i < 3; i++) {
			assert(meshlink_channel_send(server, channels[i], data, sizeof(data)) == sizeof(data));
		}
	}

	// Shutdown the write side of the server's channels

	for(int i = 0; i < 3; i++) {
		meshlink_channel_shutdown(server, channels[i], SHUT_WR);
	}

	// Wait for the clients to finish reading all the data

	for(int i = 0; i < 3; i++) {
		assert(wait_sync_flag(&clients[i].close_flag, 10));
	}

	// Check that the clients have received (most of) the data

	for(int i = 0; i < 3; i++) {
		fprintf(stderr, "%s received %zu\n", clients[i].mesh->name, clients[i].received);
	}

	bool got_large_packet = false;

	for(int i = 0; i < 3; i++) {
		size_t max_received = SMALL_SIZE * SMALL_COUNT + LARGE_SIZE;
		assert(clients[i].received >= max_received / 2);
		assert(clients[i].received <= max_received);
		got_large_packet |= clients[i].got_large_packet;
	}

	assert(got_large_packet);

	// Clean up.

	for(int i = 0; i < 3; i++) {
		meshlink_close(clients[i].mesh);
	}

	meshlink_close(server);

	return 0;
}
