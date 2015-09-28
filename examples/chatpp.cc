#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "meshlink/meshlink++.h"

class ChatMesh : public meshlink::mesh
{
public:
	void log(meshlink::log_level_t level, const char *text) {
		const char *levelstr[] = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};
		fprintf(stderr, "%s: %s\n", levelstr[level], text);
		}

	void receive(meshlink::node *source, const void *data, size_t len) {
		const char *msg = (const char *)data;
	
		if(!len || msg[len - 1]) {
			fprintf(stderr, "Received invalid data from %s\n", source->name);
			return;
		}
		
		printf("%s says: %s\n", source->name, msg);
	}

	void node_status(meshlink::node *node, bool reachable) {
		if(reachable)
			printf("%s joined.\n", node->name);
		else
			printf("%s left.\n", node->name);
	}
};

static meshlink::node **nodes;
static size_t nnodes;

static void parse_command(meshlink::mesh *mesh, char *buf) {
	char *arg = strchr(buf, ' ');
	if(arg)
		*arg++ = 0;

	if(!strcasecmp(buf, "invite")) {
		std::string invitation;

		if(!arg) {
			fprintf(stderr, "/invite requires an argument!\n");
			return;
		}

		invitation = mesh->invite(arg);
		if(invitation.empty()) {
			fprintf(stderr, "Could not invite '%s': %s\n", arg, meshlink::strerror());
			return;
		}

		printf("Invitation for %s: %s\n", arg, invitation.c_str());
	} else if(!strcasecmp(buf, "join")) {
		if(!arg) {
			fprintf(stderr, "/join requires an argument!\n");
			return;
		}

		if(!mesh->join(arg))
			fprintf(stderr, "Could not join using invitation: %s\n", meshlink::strerror());
		else
			fprintf(stderr, "Invitation accepted!\n");
	} else if(!strcasecmp(buf, "kick")) {
		if(!arg) {
			fprintf(stderr, "/kick requires an argument!\n");
			return;
		}

		meshlink::node *node = mesh->get_node(arg);
		if(!node) {
			fprintf(stderr, "Error looking up '%s': %s\n", arg, meshlink::strerror());
			return;
		}

		mesh->blacklist(node);

		printf("Node '%s' blacklisted.\n", arg);
	} else if(!strcasecmp(buf, "who")) {
		if(!arg) {
			nodes = mesh->get_all_nodes(nodes, &nnodes);
			if(!nodes) {
				fprintf(stderr, "Could not get list of nodes: %s\n", meshlink::strerror());
			} else {
				printf("%zu known nodes:", nnodes);
				for(size_t i = 0; i < nnodes; i++)
					printf(" %s", nodes[i]->name);
				printf("\n");
			}
		} else {
			meshlink::node *node = mesh->get_node(arg);
			if(!node) {
				fprintf(stderr, "Error looking up '%s': %s\n", arg, meshlink::strerror());
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

static void parse_input(meshlink::mesh *mesh, char *buf) {
	static meshlink::node *destination;
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

		destination = mesh->get_node(buf);
		if(!destination) {
			fprintf(stderr, "Error looking up '%s': %s\n", buf, meshlink::strerror());
			return;
		}
	}

	if(!destination) {
		fprintf(stderr, "Who are you talking to? Write 'name: message...'\n");
		return;
	}

	if(!mesh->send(destination, msg, strlen(msg) + 1)) {
		fprintf(stderr, "Could not send message to '%s': %s\n", destination->name, meshlink::strerror());
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

	ChatMesh mesh;
	mesh.open(confbase, nick, "chatpp", DEV_CLASS_STATIONARY);

	if(!mesh.isOpen()) {
		fprintf(stderr, "Could not open MeshLink: %s\n", meshlink::strerror());
		return 1;
	}

	if(!mesh.start()) {
		fprintf(stderr, "Could not start MeshLink: %s\n", meshlink::strerror());
		return 1;
	}

	printf("Chat started.\nType /help for a list of commands.\n");

	while(fgets(buf, sizeof buf, stdin))
		parse_input(&mesh, buf);

	printf("Chat stopping.\n");

	mesh.stop();
	mesh.close();

	return 0;
}
