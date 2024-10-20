#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../src/meshlink.h"
#include "../src/devtools.h"

#define CHAT_PORT 531

static void log_message(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	(void) mesh;

	static const char *levelstr[] = {
		[MESHLINK_DEBUG] = "\x1b[34mDEBUG",
		[MESHLINK_INFO] = "\x1b[32mINFO",
		[MESHLINK_WARNING] = "\x1b[33mWARNING",
		[MESHLINK_ERROR] = "\x1b[31mERROR",
		[MESHLINK_CRITICAL] = "\x1b[31mCRITICAL",
	};
	fprintf(stderr, "%s:\x1b[0m %s\n", levelstr[level], text);
}

static void channel_receive(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	if(!len) {
		if(meshlink_errno) {
			fprintf(stderr, "Error while reading data from %s: %s\n", channel->node->name, meshlink_strerror(meshlink_errno));
		} else {
			fprintf(stderr, "Chat connection closed by %s\n", channel->node->name);
		}

		channel->node->priv = NULL;
		meshlink_channel_close(mesh, channel);
		return;
	}

	// TODO: we now have TCP semantics, don't expect exactly one message per receive call.

	fprintf(stderr, "%s says: ", channel->node->name);
	fwrite(data, len, 1, stderr);
	fputc('\n', stderr);
}

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)data;
	(void)len;

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

static void channel_poll(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	fprintf(stderr, "Channel to '%s' connected\n", channel->node->name);
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
}

static void node_status(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(reachable) {
		fprintf(stderr, "%s joined.\n", node->name);
	} else {
		fprintf(stderr, "%s left.\n", node->name);
	}
}

static void parse_command(meshlink_handle_t *mesh, char *buf) {
	char *arg = strchr(buf, ' ');
	char *arg1 = NULL;

	if(arg) {
		*arg++ = 0;

		arg1 = strchr(arg, ' ');

		if(arg1) {
			*arg1++ = 0;
		}

	}

	if(!strcasecmp(buf, "invite")) {
		if(!arg) {
			fprintf(stderr, "/invite requires an argument!\n");
			return;
		}

		meshlink_submesh_t *s = NULL;

		if(arg1) {
			size_t nmemb;
			meshlink_submesh_t **submeshes = devtool_get_all_submeshes(mesh, NULL, &nmemb);

			if(!submeshes || !nmemb) {
				fprintf(stderr, "Group does not exist!\n");
				return;
			}

			for(size_t i = 0; i < nmemb; i++) {
				if(!strcmp(arg1, submeshes[i]->name)) {
					s = submeshes[i];
					break;
				}
			}

			free(submeshes);

			if(!s) {
				fprintf(stderr, "Group is not yet created!\n");
				return;
			}
		}

		char *invitation = meshlink_invite(mesh, s, arg);

		if(!invitation) {
			fprintf(stderr, "Could not invite '%s': %s\n", arg, meshlink_strerror(meshlink_errno));
			return;
		}

		fprintf(stderr, "Invitation for %s: %s\n", arg, invitation);
		free(invitation);
	} else if(!strcasecmp(buf, "canonical")) {
		bool set;
		char *host = NULL, *port = NULL;

		if(!arg) {
			fprintf(stderr, "/canonical requires an argument!\n");
			return;
		}

		if((0 == strncasecmp(arg, "-h", 2)) && (strlen(arg) > 2)) {
			host = arg + 2;
		} else if((0 == strncasecmp(arg, "-p", 2)) && (strlen(arg) > 2)) {
			port = arg + 2;
		} else {
			fprintf(stderr, "Unknown argument: %s!\n", arg);
			return;
		}

		if(arg1) {
			if((0 == strncasecmp(arg1, "-h", 2)) && (strlen(arg1) > 2)) {
				host = arg1 + 2;
			} else if((0 == strncasecmp(arg1, "-p", 2)) && (strlen(arg1) > 2)) {
				port = arg1 + 2;
			} else {
				fprintf(stderr, "Unknown argument: %s!\n", arg1);
				return;
			}
		}

		if(!host && !port) {
			fprintf(stderr, "Unable to set Canonical address because no valid arguments are found!\n");
			return;
		}

		set = meshlink_set_canonical_address(mesh, meshlink_get_self(mesh), host, port);

		if(!set) {
			fprintf(stderr, "Could not set canonical address '%s:%s': %s\n", host, port, meshlink_strerror(meshlink_errno));
			return;
		}

		fprintf(stderr, "Canonical address set as '%s:%s'\n", host, port);
	} else if(!strcasecmp(buf, "group")) {
		if(!arg) {
			fprintf(stderr, "/group requires an argument!\n");
			return;
		}

		meshlink_submesh_t *s = meshlink_submesh_open(mesh, arg);

		if(!s) {
			fprintf(stderr, "Could not create group: %s\n", meshlink_strerror(meshlink_errno));
		} else {
			fprintf(stderr, "Group '%s' created!\n", s->name);
		}
	} else if(!strcasecmp(buf, "join")) {
		if(!arg) {
			fprintf(stderr, "/join requires an argument!\n");
			return;
		}

		meshlink_stop(mesh);

		if(!meshlink_join(mesh, arg)) {
			fprintf(stderr, "Could not join using invitation: %s\n", meshlink_strerror(meshlink_errno));
		} else {
			fprintf(stderr, "Invitation accepted!\n");
		}

		if(!meshlink_start(mesh)) {
			fprintf(stderr, "Could not restart MeshLink: %s\n", meshlink_strerror(meshlink_errno));
			exit(1);
		}
	} else if(!strcasecmp(buf, "monitor")) {
		size_t nnodes = 0;
		meshlink_node_t **nodes = meshlink_get_all_nodes(mesh, NULL, &nnodes);

		if(!nnodes) {
			fprintf(stderr, "Could not get list of nodes: %s\n", meshlink_strerror(meshlink_errno));
			return;
		}

		fprintf(stderr, "Found %lu known nodes\n", (unsigned long)nnodes);

		for(size_t i = 0; i < nnodes; i++) {
			devtool_node_status_t status;

			devtool_get_node_status(mesh, nodes[i], &status);
			const char *desc;
			switch(status.udp_status) {
			case DEVTOOL_UDP_FAILED:
				desc = "UDP failed";
				break;

			case DEVTOOL_UDP_IMPOSSIBLE:
				desc = "unreachable";
				break;

			case DEVTOOL_UDP_TRYING:
				desc = "probing";
				break;

			case DEVTOOL_UDP_WORKING:
				desc = "UDP working";
				break;

			case DEVTOOL_UDP_UNKNOWN:
			default:
				desc = "unknown";
				break;
			};

			if(!strcmp(nodes[i]->name, mesh->name)) {
				desc = "myself";
			}

			char mtustate = ' ';

			if(status.minmtu) {
				if(status.minmtu != status.maxmtu) {
					mtustate = '~';
				}
			};

			fprintf(stderr, "Status of node: %-16s  %-12s  %c%5d\n", nodes[i]->name, desc, mtustate, status.maxmtu);
		}

		free(nodes);
	} else if(!strcasecmp(buf, "external")) {
		char *externalAddress = meshlink_get_external_address(mesh);
		if (externalAddress == NULL) {
			fprintf(stderr, "Couldn't get my external address\n");
			return;
		}
		fprintf(stderr, "Found my address as %s\n", externalAddress);
		free(externalAddress);
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

		if(!meshlink_blacklist(mesh, node)) {
			fprintf(stderr, "Error blacklising '%s': %s", arg, meshlink_strerror(meshlink_errno));
			return;
		}

		fprintf(stderr, "Node '%s' blacklisted.\n", arg);
	} else if(!strcasecmp(buf, "whitelist")) {
		if(!arg) {
			fprintf(stderr, "/whitelist requires an argument!\n");
			return;
		}

		meshlink_node_t *node = meshlink_get_node(mesh, arg);

		if(!node) {
			fprintf(stderr, "Error looking up '%s': %s\n", arg, meshlink_strerror(meshlink_errno));
			return;
		}

		if(!meshlink_whitelist(mesh, node)) {
			fprintf(stderr, "Error whitelising '%s': %s", arg, meshlink_strerror(meshlink_errno));
			return;
		}

		fprintf(stderr, "Node '%s' whitelisted.\n", arg);
	} else if(!strcasecmp(buf, "who")) {
		meshlink_submesh_t *node_group = NULL;

		if(!arg) {
			size_t nnodes;
			meshlink_node_t **nodes = meshlink_get_all_nodes(mesh, NULL, &nnodes);

			if(!nnodes) {
				fprintf(stderr, "Could not get list of nodes: %s\n", meshlink_strerror(meshlink_errno));
				return;
			}

			fprintf(stderr, "%lu known nodes:\n", (unsigned long)nnodes);

			for(size_t i = 0; i < nnodes; i++) {
				fprintf(stderr, " %lu. %s", (unsigned long)i, nodes[i]->name);

				if((node_group = meshlink_get_node_submesh(mesh, nodes[i]))) {
					fprintf(stderr, "\t%s", node_group->name);
				}

				fprintf(stderr, "\n");
			}

			fprintf(stderr, "\n");

			free(nodes);
		} else {
			meshlink_node_t *node = meshlink_get_node(mesh, arg);

			if(!node) {
				fprintf(stderr, "Error looking up '%s': %s\n", arg, meshlink_strerror(meshlink_errno));
			} else {
				fprintf(stderr, "Node %s found", arg);

				if((node_group = meshlink_get_node_submesh(mesh, node))) {
					fprintf(stderr, " in group %s", node_group->name);
				}

				fprintf(stderr, "\n");
			}
		}
	} else if(!strcasecmp(buf, "listgroup")) {
		if(!arg) {
			fprintf(stderr, "/listgroup requires an argument!\n");
			return;
		}

		size_t nmemb;
		meshlink_submesh_t **submeshes = devtool_get_all_submeshes(mesh, NULL, &nmemb);

		if(!submeshes || !nmemb) {
			fprintf(stderr, "Group does not exist!\n");
			return;
		}

		meshlink_submesh_t *s = NULL;

		for(size_t i = 0; i < nmemb; i++) {
			if(!strcmp(arg, submeshes[i]->name)) {
				s = submeshes[i];
				break;
			}
		}

		free(submeshes);

		if(!s) {
			fprintf(stderr, "Group %s does not exist!\n", arg);
			return;
		}

		size_t nnodes;
		meshlink_node_t **nodes = meshlink_get_all_nodes_by_submesh(mesh, s, NULL, &nnodes);

		if(!nodes || !nnodes) {
			fprintf(stderr, "Group %s does not contain any nodes!\n", arg);
			return;
		}

		fprintf(stderr, "%zu known nodes in group %s:", nnodes, arg);

		for(size_t i = 0; i < nnodes; i++) {
			fprintf(stderr, " %s", nodes[i]->name);
		}

		fprintf(stderr, "\n");

		free(nodes);
	} else if(!strcasecmp(buf, "quit")) {
		fprintf(stderr, "Bye!\n");
		fclose(stdin);
	} else if(!strcasecmp(buf, "help")) {
		fprintf(stderr,
		        "<name>: <message>     			  Send a message to the given node.\n"
		        "                      			  Subsequent messages don't need the <name>: prefix.\n"
		        "/group <name>				  Create a new group"
		        "/invite <name> [submesh]		  Create an invitation for a new node.\n"
		        "                      			  Node joins either coremesh or submesh depending on submesh parameter.\n"
		        "/join <invitation>				  Join an existing mesh using an invitation.\n"
		        "/kick <name>          			  Blacklist the given node.\n"
		        "/who [<name>]         			  List all nodes or show information about the given node.\n"
		        "/listgroup <name>         		  List all nodes in a given group.\n"
		        "/canonical -h<hostname> -p<port> Set Canonical address to be present in invitation.\n"
		        "                      			  Any one of two options an be specified. At least one option must be present\n"
		        "/quit                 			  Exit this program.\n"
		       );
	} else {
		fprintf(stderr, "Unknown command '/%s'\n", buf);
	}
}

static void parse_input(meshlink_handle_t *mesh, char *buf) {
	static meshlink_node_t *destination;
	size_t len;

	if(!buf) {
		return;
	}

	// Remove newline.

	len = strlen(buf);

	if(len && buf[len - 1] == '\n') {
		buf[--len] = 0;
	}

	if(len && buf[len - 1] == '\r') {
		buf[--len] = 0;
	}

	// Ignore empty lines.

	if(!len) {
		return;
	}

	// Commands start with '/'

	if(*buf == '/') {
		parse_command(mesh, buf + 1);
		return;
	}

	// Lines in the form "name: message..." set the destination node.

	char *msg = buf;
	char *colon = strchr(buf, ':');

	if(colon) {
		*colon = 0;
		msg = colon + 1;

		if(*msg == ' ') {
			msg++;
		}

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
		meshlink_set_channel_poll_cb(mesh, channel, channel_poll);
	}

	if(!meshlink_channel_send(mesh, channel, msg, strlen(msg))) {
		fprintf(stderr, "Could not send message to '%s': %s\n", destination->name, meshlink_strerror(meshlink_errno));
		return;
	}

	fprintf(stderr, "Message sent to '%s'.\n", destination->name);
}

int main(int argc, char *argv[]) {
	const char *confbase = ".chat";
	const char *nick = NULL;
	char buf[1024];

	if(argc > 1) {
		confbase = argv[1];
	}

	if(argc > 2) {
		nick = argv[2];
	}

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_message);

	meshlink_handle_t *mesh = meshlink_open(confbase, nick, "chat", DEV_CLASS_STATIONARY);

	if(!mesh) {
		fprintf(stderr, "Could not open MeshLink: %s\n", meshlink_strerror(meshlink_errno));
		return 1;
	}

	meshlink_set_node_status_cb(mesh, node_status);
	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, log_message);

	// Set the channel accept callback. This implicitly turns on channels for all nodes.
	// This replaces the call to meshlink_set_receive_cb().
	meshlink_set_channel_accept_cb(mesh, channel_accept);

	if(!meshlink_start(mesh)) {
		fprintf(stderr, "Could not start MeshLink: %s\n", meshlink_strerror(meshlink_errno));
		return 1;
	}

	fprintf(stderr, "Chat started.\nType /help for a list of commands.\n");

	while(fgets(buf, sizeof(buf), stdin)) {
		parse_input(mesh, buf);
	}

	fprintf(stderr, "Chat stopping.\n");

	meshlink_stop(mesh);
	meshlink_close(mesh);

	return 0;
}
