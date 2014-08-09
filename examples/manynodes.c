#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include "../src/meshlink.h"

static int n = 10;
static meshlink_handle_t **mesh;

static meshlink_node_t **nodes;
static size_t nnodes;

//Test mesh sending data
static void testmesh () {

	for(int nindex = 0; nindex < n; nindex++) {

			nodes = meshlink_get_all_nodes(mesh[nindex], nodes, &nnodes);
			if(!nodes) {
				fprintf(stderr, "Could not get list of nodes: %s\n", meshlink_strerror(meshlink_errno));
			} else {
				printf("%zu known nodes:", nnodes);
				for(int i = 0; i < nnodes; i++) {
					printf(" %s", nodes[i]->name);
					if(!meshlink_send(mesh[nindex], nodes[i], "magic", strlen("magic") + 1)) {
		fprintf(stderr, "Could not send message to '%s': %s\n", nodes[i]->name, meshlink_strerror(meshlink_errno));
	}
	}

			}

	}
}
// Make all nodes know about each other by importing each others public keys and addresses.
static void linkmesh() {
	for(int i = 0; i < n; i++) {
		char *datai = meshlink_export(mesh[i]);

		for(int j = i + 1; j < n; j++) {
			char *dataj = meshlink_export(mesh[j]);
			meshlink_import(mesh[i], dataj);
			meshlink_import(mesh[j], datai);
			free(dataj);
		}

		free(datai);
	}
}

static void parse_command(char *buf) {
	char *arg = strchr(buf, ' ');
	if(arg)
		*arg++ = 0;

	if(!strcasecmp(buf, "invite")) {
		char *invitation;

		if(!arg) {
			fprintf(stderr, "/invite requires an argument!\n");
			return;
		}

		invitation = meshlink_invite(mesh[0], arg);
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
		meshlink_stop(mesh[0]);
		if(!meshlink_join(mesh[0], arg))
			fprintf(stderr, "Could not join using invitation: %s\n", meshlink_strerror(meshlink_errno));
		else {
			fprintf(stderr, "Invitation accepted!\n");
			meshlink_start(mesh[0]);
		}
	} else if(!strcasecmp(buf, "kick")) {
		if(!arg) {
			fprintf(stderr, "/kick requires an argument!\n");
			return;
		}

		meshlink_node_t *node = meshlink_get_node(mesh[0], arg);
		if(!node) {
			fprintf(stderr, "Unknown node '%s'\n", arg);
			return;
		}

		meshlink_blacklist(mesh[0], node);

		printf("Node '%s' blacklisted.\n", arg);
	} else if(!strcasecmp(buf, "who")) {
		if(!arg) {
			nodes = meshlink_get_all_nodes(mesh[0], nodes, &nnodes);
			if(!nodes) {
				fprintf(stderr, "Could not get list of nodes: %s\n", meshlink_strerror(meshlink_errno));
			} else {
				printf("%zu known nodes:", nnodes);
				for(int i = 0; i < nnodes; i++)
					printf(" %s", nodes[i]->name);
				printf("\n");
			}
		} else {
			meshlink_node_t *node = meshlink_get_node(mesh[0], arg);
			if(!node) {
				fprintf(stderr, "Unknown node '%s'\n", arg);
			} else {
				printf("Node %s found, pmtu %zd\n", arg, meshlink_get_pmtu(mesh[0], node));
			}
		}
	} else if(!strcasecmp(buf, "link")) {
		linkmesh();
	} else if(!strcasecmp(buf, "test")) {
		testmesh();
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
			"/link                 Link all nodes together.\n"
			"/test                 Test functionality sending some data to all nodes\n"
			"/quit                 Exit this program.\n"
			);
	} else {
		fprintf(stderr, "Unknown command '/%s'\n", buf);
	}
}

static void parse_input(char *buf) {
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
		return parse_command(buf + 1);

	// Lines in the form "name: message..." set the destination node.

	char *msg = buf;
	char *colon = strchr(buf, ':');

	if(colon) {
		*colon = 0;
		msg = colon + 1;
		if(*msg == ' ')
			msg++;

		destination = meshlink_get_node(mesh[0], buf);
		if(!destination) {
			fprintf(stderr, "Unknown node '%s'\n", buf);
			return;
		}
	}

	if(!destination) {
		fprintf(stderr, "Who are you talking to? Write 'name: message...'\n");
		return;
	}

	if(!meshlink_send(mesh[0], destination, msg, strlen(msg) + 1)) {
		fprintf(stderr, "Could not send message to '%s': %s\n", destination->name, meshlink_strerror(meshlink_errno));
		return;
	}

	printf("Message sent to '%s'.\n", destination->name);
}

int main(int argc, char *argv[]) {
	const char *basebase = ".manynodes";
	const char *namesprefix = "machine1";
	char buf[1024];

	if(argc > 1)
		n = atoi(argv[1]);

	if(n < 1) {
		fprintf(stderr, "Usage: %s [number of local nodes] [confbase] [prefixnodenames]\n", argv[0]);
		return 1;
	}

	if(argc > 2)
		basebase = argv[2];

	if(argc > 3)
		namesprefix = argv[3];

	mesh = calloc(n, sizeof *mesh);

	mkdir(basebase, 0750);

	char filename[PATH_MAX];
	char nodename[100];
	for(int i = 0; i < n; i++) {
		snprintf(nodename, sizeof nodename, "%snode%d", namesprefix,i);
		snprintf(filename, sizeof filename, "%s/%s", basebase, nodename);
		bool itsnew = access(filename, R_OK);
		mesh[i] = meshlink_open(filename, nodename, "manynodes", STATIONARY);
		if(itsnew)
			meshlink_add_address(mesh[i], "localhost");
		if(!mesh[i]) {
			fprintf(stderr, "errno is: %d\n", meshlink_errno);
			fprintf(stderr, "Could not open %s: %s\n", filename, meshlink_strerror(meshlink_errno));
			return 1;
		}
	}

	int started = 0;

	for(int i = 0; i < n; i++) {
		if(!meshlink_start(mesh[i]))
			fprintf(stderr, "Could not start node %d: %s\n", i, meshlink_strerror(meshlink_errno));
		else
			started++;
	}

	if(!started) {
		fprintf(stderr, "Could not start any node!\n");
		return 1;
	}

	printf("%d nodes started.\nType /help for a list of commands.\n", started);

	while(fgets(buf, sizeof buf, stdin))
		parse_input(buf);

	printf("Nodes stopping.\n");

	for(int i = 0; i < n; i++)
		meshlink_close(mesh[i]);

	return 0;
}
