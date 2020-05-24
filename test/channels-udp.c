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


struct client {
	meshlink_handle_t *mesh;
	size_t received;
	bool got_large_packet;
	struct sync_flag accept_flag;
	struct sync_flag close_flag;
};

static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)channel;
	(void)data;

	// We expect always the same amount of data from the server.
	assert(mesh->priv);
	struct client *client = mesh->priv;

	if(!data && len == 0) {
		set_sync_flag(&client->close_flag, true);
		meshlink_channel_close(mesh, channel);
		return;
	}

	assert(len == 512 || len == 65535);
	client->received += len;

	if(len == 65535) {
		client->got_large_packet = true;
	}
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	assert(port == 1);
	assert(!data);
	assert(!len);
	assert(meshlink_channel_get_flags(mesh, channel) == MESHLINK_CHANNEL_UDP);

	assert(mesh->priv);
	struct client *client = mesh->priv;

	meshlink_set_channel_receive_cb(mesh, channel, receive_cb);
	set_sync_flag(&client->accept_flag, true);

	return true;
}

int main(void) {
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
	assert(meshlink_start(server));

	for(int i = 0; i < 3; i++) {
		char dir[100];
		snprintf(dir, sizeof(dir), "channels_udp_conf.%d", i + 1);
		assert(meshlink_destroy(dir));
		clients[i].mesh = meshlink_open(dir, names[i], "channels-udp", DEV_CLASS_STATIONARY);
		assert(clients[i].mesh);
		clients[i].mesh->priv = &clients[i];
		meshlink_enable_discovery(clients[i].mesh, false);
		meshlink_set_channel_accept_cb(clients[i].mesh, accept_cb);
		link_meshlink_pair(server, clients[i].mesh);
		assert(meshlink_start(clients[i].mesh));
	}

	// Open channels

	for(int i = 0; i < 3; i++) {
		meshlink_node_t *peer = meshlink_get_node(server, names[i]);
		assert(peer);
		channels[i] = meshlink_channel_open_ex(server, peer, 1, NULL, NULL, 0, MESHLINK_CHANNEL_UDP);
		assert(channels[i]);
	}

	// Wait for all three channels to connect

	for(int i = 0; i < 3; i++) {
		assert(wait_sync_flag(&clients[i].accept_flag, 10));
	}

	// Check that we can send up to 65535 bytes without errors

	char large_data[65535] = "";

	for(int i = 0; i < 3; i++) {
		assert(meshlink_channel_send(server, channels[i], large_data, sizeof(large_data)) == sizeof(large_data));
	}

	// Check that we can send zero bytes without errors

	for(int i = 0; i < 3; i++) {
		assert(meshlink_channel_send(server, channels[i], large_data, 0) == 0);
	}

	// Assert that any larger packets are not allowed

	assert(meshlink_channel_send(server, channels[0], large_data, 65536) == -1);

	// Stream packets from server to clients for 5 seconds at 40 Mbps (1 kB * 500 Hz)

	char data[512];
	memset(data, 'U', sizeof(data));

	for(int j = 0; j < 2500; j++) {
		for(int i = 0; i < 3; i++) {
			size_t result = meshlink_channel_send(server, channels[i], data, sizeof(data));
			assert(result == sizeof(data));
		}

		const struct timespec req = {0, 2000000};
		clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL);
	}

	// Close the channels

	for(int i = 0; i < 3; i++) {
		meshlink_channel_close(server, channels[i]);
	}

	for(int i = 0; i < 3; i++) {
		assert(wait_sync_flag(&clients[i].close_flag, 10));
	}

	// Check that the clients have received (most of) the data

	for(int i = 0; i < 3; i++) {
		fprintf(stderr, "%s received %zu\n", clients[i].mesh->name, clients[i].received);
	}

	bool got_large_packet = false;

	for(int i = 0; i < 3; i++) {
		assert(clients[i].received >= 1000000);
		assert(clients[i].received <= 1345536);
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
