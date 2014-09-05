#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../src/meshlink.h"

#define CHAT_PORT 531

static void log_message(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	const char *levelstr[] = {
		[MESHLINK_DEBUG] = "\x1b[34mDEBUG",
		[MESHLINK_INFO] = "\x1b[32mINFO",
		[MESHLINK_WARNING] = "\x1b[33mWARNING",
		[MESHLINK_ERROR] = "\x1b[31mERROR",
		[MESHLINK_CRITICAL] = "\x1b[31mCRITICAL",
	};
	fprintf(stderr, "%s:\x1b[0m %s\n", levelstr[level], text);
}

static void channel_receive(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	const char *msg = data;

	if(!len) {
		if(meshlink_errno)
			fprintf(stderr, "Error while reading data from %s: %s\n", channel->node->name, meshlink_strerror(meshlink_errno));
		else
			fprintf(stderr, "Chat connection closed by %s\n", channel->node->name);

		channel->node->priv = NULL;
		meshlink_channel_close(mesh, channel);
		return;
	}

	// TODO: we now have TCP semantics, don't expect exactly one message per receive call.
	if(msg[len - 1]) {
		fprintf(stderr, "Received invalid data from %s\n", channel->node->name);
		return;
	}

	printf("%s says: %s\n", channel->node->name, msg);
}

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	// Only accept connections to the chat port
	if(port != CHAT_PORT) {
		fprintf(stderr, "Rejected incoming channel from '%s' to port %u\n", channel->node->name, port);
		return false;
	}

	fprintf(stderr, "Accepted incoming channel from '%s'\n", channel->node->name);

	// Remember the channel
	channel->node->priv = channel;

	// Set the receive callback
	meshlink_set_channel_receive_cb(mesh, channel, channel_receive);

	// Accept this channel
	return true;
}

static void node_status(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	if(reachable)
		printf("%s joined.\n", node->name);
	else
		printf("%s left.\n", node->name);
}

static meshlink_node_t **nodes;
static size_t nnodes;

static void parse_command(meshlink_handle_t *mesh, char *buf) {
	char *arg = strchr(buf, ' ');
	if(arg)
		*arg++ = 0;

	if(!strcasecmp(buf, "invite")) {
		char *invitation;

		if(!arg) {
			fprintf(stderr, "/invite requires an argument!\n");
			return;
		}

		invitation = meshlink_invite(mesh, arg);
		if(!invitation) {
			fprintf(stderr, "Could not invite '%s': %s\n", arg, meshlink_strerror(meshlink_errno));
			return;
		}

		printf("Invitation for %s: %s\n", arg, invitation);
		free(invitation);
	} else if(!strcasecmp(buf, "join")) {
		if(!arg) {
			fprintf(stderr, "/join requires an argument!\n");
			return;
		}
		meshlink_stop(mesh);
		if(!meshlink_join(mesh, arg))
			fprintf(stderr, "Could not join using invitation: %s\n", meshlink_strerror(meshlink_errno));
		else {
			fprintf(stderr, "Invitation accepted!\n");
			if(!meshlink_start(mesh)) {
				fprintf(stderr, "Could not start MeshLink: %s\n", meshlink_strerror(meshlink_errno));
			return;
			}
		}
	} else if(!strcasecmp(buf, "kick")) {
		if(!arg) {
			fprintf(stderr, "/kick requires an argument!\n");
			return;
		}

		meshlink_node_t *node = meshlink_get_node(mesh, arg);
		if(!node) {
			fprintf(stderr, "Error looking up '%s': %s\n", arg, meshlink_strerror(meshlink_errno));
			return;
		}

		meshlink_blacklist(mesh, node);

		printf("Node '%s' blacklisted.\n", arg);
	} else if(!strcasecmp(buf, "who")) {
		if(!arg) {
			nodes = meshlink_get_all_nodes(mesh, nodes, &nnodes);
			if(!nnodes) {
				fprintf(stderr, "Could not get list of nodes: %s\n", meshlink_strerror(meshlink_errno));
			} else {
				printf("%zu known nodes:", nnodes);
				for(int i = 0; i < nnodes; i++)
					printf(" %s", nodes[i]->name);
				printf("\n");
			}
		} else {
			meshlink_node_t *node = meshlink_get_node(mesh, arg);
			if(!node) {
				fprintf(stderr, "Error looking up '%s': %s\n", arg, meshlink_strerror(meshlink_errno));
			} else {
				printf("Node %s found\n", arg);
			}
		}
	} else if(!strcasecmp(buf, "quit")) {
		printf("Bye!\n");
		fclose(stdin);
	} else if(!strcasecmp(buf, "help")) {
		printf(
			"<name>: <message>     Send a message to the given node.\n"
			"                      Subsequent messages don't need the <name>: prefix.\n"
			"/invite <name>        Create an invitation for a new node.\n"
			"/join <invitation>    Join an existing mesh using an invitation.\n"
			"/kick <name>          Blacklist the given node.\n"
			"/who [<name>]         List all nodes or show information about the given node.\n"
			"/quit                 Exit this program.\n"
			);
	} else {
		fprintf(stderr, "Unknown command '/%s'\n", buf);
	}
}

static void parse_input(meshlink_handle_t *mesh, char *buf) {
	static meshlink_node_t *destination;
	size_t len;

	if(!buf)
		return;

	// Remove newline.

	len = strlen(buf);

	if(len && buf[len - 1] == '\n')
		buf[--len] = 0;

	if(len && buf[len - 1] == '\r')
		buf[--len] = 0;

	// Ignore empty lines.

	if(!len)
		return;

	// Commands start with '/'

	if(*buf == '/')
		return parse_command(mesh, buf + 1);

	// Lines in the form "name: message..." set the destination node.

	char *msg = buf;
	char *colon = strchr(buf, ':');

	if(colon) {
		*colon = 0;
		msg = colon + 1;
		if(*msg == ' ')
			msg++;

		destination = meshlink_get_node(mesh, buf);
		if(!destination) {
			fprintf(stderr, "Error looking up '%s': %s\n", buf, meshlink_strerror(meshlink_errno));
			return;
		}
	}

	if(!destination) {
		fprintf(stderr, "Who are you talking to? Write 'name: message...'\n");
		return;
	}

	// We want to have one channel per node.
	// We keep the pointer to the meshlink_channel_t in the priv field of that node.
	meshlink_channel_t *channel = destination->priv;

	if(!channel) {
		fprintf(stderr, "Opening chat channel to '%s'\n", destination->name);
		channel = meshlink_channel_open(mesh, destination, CHAT_PORT, channel_receive, NULL, 0);
		if(!channel) {
			fprintf(stderr, "Could not create channel to '%s': %s\n", destination->name, meshlink_strerror(meshlink_errno));
			return;
		}
		destination->priv = channel;
	}

	if(!meshlink_channel_send(mesh, channel, msg, len + 1)) {
		fprintf(stderr, "Could not send message to '%s': %s\n", destination->name, meshlink_strerror(meshlink_errno));
		return;
	}

	printf("Message sent to '%s'.\n", destination->name);
}

int main(int argc, char *argv[]) {
	const char *confbase = ".chat";
	const char *nick = NULL;
	char buf[1024];

	if(argc > 1)
		confbase = argv[1];

	if(argc > 2)
		nick = argv[2];

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_message);

	meshlink_handle_t *mesh = meshlink_open(confbase, nick, "chat", DEV_CLASS_STATIONARY);
	if(!mesh) {
		fprintf(stderr, "Could not open MeshLink: %s\n", meshlink_strerror(meshlink_errno));
		return 1;
	}

	meshlink_set_node_status_cb(mesh, node_status);
	meshlink_set_log_cb(mesh, MESHLINK_INFO, log_message);

	// Set the channel accept callback. This implicitly turns on channels for all nodes.
	// This replaces the call to meshlink_set_receive_cb().
	meshlink_set_channel_accept_cb(mesh, channel_accept);

	if(!meshlink_start(mesh)) {
		fprintf(stderr, "Could not start MeshLink: %s\n", meshlink_strerror(meshlink_errno));
		return 1;
	}

	printf("Chat started.\nType /help for a list of commands.\n");

	while(fgets(buf, sizeof buf, stdin))
		parse_input(mesh, buf);

	printf("Chat stopping.\n");

	meshlink_stop(mesh);
	meshlink_close(mesh);

	return 0;
}
