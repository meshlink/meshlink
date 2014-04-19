#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/meshlink.h"

static void log(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	const char *levelstr[] = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};
	fprintf(stderr, "%s: %s\n", levelstr[level], text);
}

static void receive(meshlink_handle_t *mesh, meshlink_node_t *source, const char *data, size_t len) {
	if(!len || data[len - 1]) {
		fprintf(stderr, "Received invalid data from %s\n", source->name);
		return;
	}

	printf("%s says: %s\n", source->name, data);
}

static void node_status(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	if(reachable)
		printf("%s joined.\n", node->name);
	else
		printf("%s left.\n", node->name);
}

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
			fprintf(stderr, "Could not invite '%s': %s\n", arg, mesh->errstr);
			return;
		}

		printf("Invitation for %s: %s\n", arg, invitation);
		free(invitation);
	} else if(!strcasecmp(buf, "join")) {
		if(!arg) {
			fprintf(stderr, "/join requires an argument!\n");
			return;
		}

		if(!meshlink_join(mesh, arg))
			fprintf(stderr, "Could not join using invitation: %s\n", mesh->errstr);
		else
			fprintf(stderr, "Invitation accepted!\n");
	} else if(!strcasecmp(buf, "kick")) {
		if(!arg) {
			fprintf(stderr, "/kick requires an argument!\n");
			return;
		}

		meshlink_node_t *node = meshlink_get_node(mesh, arg);
		if(!node) {
			fprintf(stderr, "Unknown node '%s'\n", arg);
			return;
		}

		meshlink_blacklist(mesh, node);

		printf("Node '%s' blacklisted.\n", arg);
	} else if(!strcasecmp(buf, "quit")) {
		printf("Bye!\n");
		fclose(stdin);
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
			fprintf(stderr, "Unknown node '%s'\n", buf);
			return;
		}
	}

	if(!destination) {
		fprintf(stderr, "Who are you talking to? Write 'name: message...'\n");
		return;
	}

	if(!meshlink_send(mesh, destination, msg, strlen(msg) + 1)) {
		fprintf(stderr, "Could not send message to '%s': %s\n", destination->name, mesh->errstr);
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

	meshlink_handle_t *mesh = meshlink_open(confbase, nick);
	if(!mesh) {
		fprintf(stderr, "Could not open MeshLink: %s\n", mesh->errstr);
		return 1;
	}

	meshlink_set_receive_cb(mesh, receive);
	meshlink_set_node_status_cb(mesh, node_changed);
	meshlink_set_log_cb(mesh, MESHLINK_INFO, log);

	if(!meshlink_start(mesh)) {
		fprintf(stderr, "Could not start MeshLink: %s\n", mesh->errstr);
		return 1;
	}

	printf("Chat started.\n");

	while(fgets(buf, sizeof buf, stdin))
		parse_input(mesh, buf);

	printf("Chat stopping.\n");

	meshlink_stop(mesh);
	meshlink_close(mesh);

	return 0;
}
