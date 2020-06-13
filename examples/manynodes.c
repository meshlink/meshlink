#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <linux/limits.h>
#elif defined(__APPLE__)
#include <sys/syslimits.h>
#endif

#include "../src/meshlink.h"
#include "../src/devtools.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <sys/time.h>
#include <signal.h>
#include <assert.h>

static int n = 10;
static meshlink_handle_t **meshes;
static const char *namesprefix = "machine1";
static int nodeindex = 0;

static meshlink_node_t **nodes;
static size_t nnodes;

static void log_message(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	const char *levelstr[] = {
		[MESHLINK_DEBUG] = "\x1b[34mDEBUG",
		[MESHLINK_INFO] = "\x1b[32mINFO",
		[MESHLINK_WARNING] = "\x1b[33mWARNING",
		[MESHLINK_ERROR] = "\x1b[31mERROR",
		[MESHLINK_CRITICAL] = "\x1b[31mCRITICAL",
	};
	fprintf(stderr, "%s\t%s:\x1b[0m %s\n", mesh ? mesh->name : "global", levelstr[level], text);
}

//Test mesh sending data
static void testmesh(void) {
	for(int nindex = 0; nindex < n; nindex++) {

		nodes = meshlink_get_all_nodes(meshes[nindex], nodes, &nnodes);

		if(!nodes) {
			fprintf(stderr, "Could not get list of nodes: %s\n", meshlink_strerror(meshlink_errno));
		} else {
			printf("%zu known nodes:\n", nnodes);

			for(size_t i = 0; i < nnodes; i++) {
				//printf(" %s\n", nodes[i]->name);
				if(!meshlink_send(meshes[nindex], nodes[i], "magic", strlen("magic") + 1)) {
					fprintf(stderr, "Could not send message to '%s': %s\n", nodes[i]->name, meshlink_strerror(meshlink_errno));
				}
			}

		}

	}
}
// Make all nodes know about each other by importing each others public keys and addresses.
static void linkmesh(void) {
	for(int i = 0; i < n; i++) {
		char *datai = meshlink_export(meshes[i]);

		for(int j = i + 1; j < n; j++) {
			char *dataj = meshlink_export(meshes[j]);

			if(!meshlink_import(meshes[i], dataj) || !meshlink_import(meshes[j], datai)) {
				fprintf(stderr, "Could not exchange keys between %s and %s: %s\n", meshes[i]->name, meshes[j]->name, meshlink_strerror(meshlink_errno));
			}

			free(dataj);
		}

		free(datai);
	}
}

static bool exportmeshgraph(const char *path) {
	assert(path);

	struct stat ps;
	int psr = stat(path, &ps);

	if(psr == 0 || errno != ENOENT) {
		if(psr == -1) {
			perror("stat");
		} else {
			fprintf(stderr, "%s exists already\n", path);
		}

		return false;
	}

	FILE *stream = fopen(path, "w");

	if(!stream) {
		perror("stream");
		return false;
	}

	if(!devtool_export_json_all_edges_state(meshes[0], stream)) {
		fclose(stream);
		fprintf(stderr, "could not export graph\n");
		return false;
	}

	fclose(stream);
	return true;
}


static void exportmeshgraph_timer(int signum) {
	(void)signum;

	struct timeval ts;
	gettimeofday(&ts, NULL);

	char name[1024];
	snprintf(name, sizeof(name), "%sgraph_%ld_%03ld.json", namesprefix, ts.tv_sec, ts.tv_usec / 1000L);

	exportmeshgraph(name);
}

#ifdef ITIMER_REAL
static bool exportmeshgraph_started = false;

static bool exportmeshgraph_end(void) {
	if(!exportmeshgraph_started) {
		return false;
	}

	struct itimerval zero_timer;

	setitimer(ITIMER_REAL, &zero_timer, NULL);

	exportmeshgraph_started = false;

	return true;
}

static bool exportmeshgraph_begin(const char *timeout_str) {
	if(!timeout_str) {
		return false;
	}

	if(exportmeshgraph_started) {
		if(!exportmeshgraph_end()) {
			return false;
		}
	}

	// get timeout
	int timeout = atoi(timeout_str);

	if(timeout < 100) {
		timeout = 100;
	}

	int timeout_sec = timeout / 1000;
	int timeout_msec = timeout % 1000;

	/* Install timer_handler as the signal handler for SIGALRM. */
	signal(SIGALRM, exportmeshgraph_timer);

	/* Configure the timer to expire immediately... */
	struct itimerval timer;
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 1000;

	/* ... and every X msec after that. */
	timer.it_interval.tv_sec = timeout_sec;
	timer.it_interval.tv_usec = timeout_msec * 1000;

	/* Start a real timer. */
	setitimer(ITIMER_REAL, &timer, NULL);

	exportmeshgraph_started = true;

	return true;
}
#else
static bool exportmeshgraph_end(void) {
	return false;
}

static bool exportmeshgraph_begin(const char *timeout_str) {
	return false;
}
#endif

static void parse_command(char *buf) {
	char *arg = strchr(buf, ' ');

	if(arg) {
		*arg++ = 0;
	}

	if(!strcasecmp(buf, "invite")) {
		char *invitation;

		if(!arg) {
			fprintf(stderr, "/invite requires an argument!\n");
			return;
		}

		invitation = meshlink_invite(meshes[nodeindex], NULL, arg);

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

		meshlink_stop(meshes[nodeindex]);

		if(!meshlink_join(meshes[nodeindex], arg)) {
			fprintf(stderr, "Could not join using invitation: %s\n", meshlink_strerror(meshlink_errno));
		} else {
			fprintf(stderr, "Invitation accepted!\n");
		}

		if(!meshlink_start(meshes[nodeindex])) {
			fprintf(stderr, "Could not restart MeshLink: %s\n", meshlink_strerror(meshlink_errno));
			exit(1);
		}
	} else if(!strcasecmp(buf, "kick")) {
		if(!arg) {
			fprintf(stderr, "/kick requires an argument!\n");
			return;
		}

		meshlink_node_t *node = meshlink_get_node(meshes[nodeindex], arg);

		if(!node) {
			fprintf(stderr, "Unknown node '%s'\n", arg);
			return;
		}

		if(!meshlink_blacklist(meshes[nodeindex], node)) {
			fprintf(stderr, "Error blacklising '%s': %s", arg, meshlink_strerror(meshlink_errno));
			return;
		}

		printf("Node '%s' blacklisted.\n", arg);
	} else if(!strcasecmp(buf, "whitelist")) {
		if(!arg) {
			fprintf(stderr, "/whitelist requires an argument!\n");
			return;
		}

		meshlink_node_t *node = meshlink_get_node(meshes[nodeindex], arg);

		if(!node) {
			fprintf(stderr, "Error looking up '%s': %s\n", arg, meshlink_strerror(meshlink_errno));
			return;
		}

		if(!meshlink_whitelist(meshes[nodeindex], node)) {
			fprintf(stderr, "Error whitelising '%s': %s", arg, meshlink_strerror(meshlink_errno));
			return;
		}

		printf("Node '%s' whitelisted.\n", arg);
	} else if(!strcasecmp(buf, "who")) {
		if(!arg) {
			nodes = meshlink_get_all_nodes(meshes[nodeindex], nodes, &nnodes);

			if(!nodes) {
				fprintf(stderr, "Could not get list of nodes: %s\n", meshlink_strerror(meshlink_errno));
			} else {
				printf("%zu known nodes:", nnodes);

				for(size_t i = 0; i < nnodes; i++) {
					printf(" %s", nodes[i]->name);
				}

				printf("\n");
			}
		} else {
			meshlink_node_t *node = meshlink_get_node(meshes[nodeindex], arg);

			if(!node) {
				fprintf(stderr, "Unknown node '%s'\n", arg);
			} else {
				printf("Node %s found, pmtu %ld\n", arg, (long int)meshlink_get_pmtu(meshes[nodeindex], node));
			}
		}
	} else if(!strcasecmp(buf, "link")) {
		linkmesh();
	} else if(!strcasecmp(buf, "eg")) {
		exportmeshgraph(arg);
	} else if(!strcasecmp(buf, "egb")) {
		exportmeshgraph_begin(arg);
	} else if(!strcasecmp(buf, "ege")) {
		exportmeshgraph_end();
	} else if(!strcasecmp(buf, "test")) {
		testmesh();
	} else if(!strcasecmp(buf, "select")) {
		if(!arg) {
			fprintf(stderr, "/select requires an argument!\n");
			return;
		}

		nodeindex = atoi(arg);
		printf("Index is now %d\n", nodeindex);
	} else if(!strcasecmp(buf, "stop")) {
		meshlink_stop(meshes[nodeindex]);
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
		        "/eg <path>            Export graph as json file.\n"
		        "/test                 Test functionality sending some data to all nodes\n"
		        "/select <number>      Select the active node running the user commands\n"
		        "/stop		       Call meshlink_stop, use /select first to select which node to stop\n"
		        "/quit                 Exit this program.\n"
		);
	} else {
		fprintf(stderr, "Unknown command '/%s'\n", buf);
	}
}

static void parse_input(char *buf) {
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
		parse_command(buf + 1);
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

		destination = meshlink_get_node(meshes[nodeindex], buf);

		if(!destination) {
			fprintf(stderr, "Unknown node '%s'\n", buf);
			return;
		}
	}

	if(!destination) {
		fprintf(stderr, "Who are you talking to? Write 'name: message...'\n");
		return;
	}

	if(!meshlink_send(meshes[nodeindex], destination, msg, strlen(msg) + 1)) {
		fprintf(stderr, "Could not send message to '%s': %s\n", destination->name, meshlink_strerror(meshlink_errno));
		return;
	}

	printf("Message sent to '%s'.\n", destination->name);
}

int main(int argc, char *argv[]) {
	const char *basebase = ".manynodes";
	const char *graphexporttimeout = NULL;
	char buf[1024];

	if(argc > 1) {
		n = atoi(argv[1]);
	}

	if(n < 1) {
		fprintf(stderr, "Usage: %s [number of local nodes] [confbase] [prefixnodenames] [graphexport timeout]\n", argv[0]);
		return 1;
	}

	if(argc > 2) {
		basebase = argv[2];
	}

	if(argc > 3) {
		namesprefix = argv[3];
	}

	if(argc > 4) {
		graphexporttimeout = argv[4];
	}

	meshes = calloc(n, sizeof(*meshes));

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_message);
#ifndef _WIN32
	mkdir(basebase, 0750);
#else
	mkdir(basebase);
#endif

	char filename[PATH_MAX];
	char nodename[100];

	for(int i = 0; i < n; i++) {
		snprintf(nodename, sizeof(nodename), "%snode%d", namesprefix, i);
		snprintf(filename, sizeof(filename), "%s/%s", basebase, nodename);

		if(n / (i + 1) > n / 4) {
			meshes[i] = meshlink_open(filename, nodename, "manynodes", DEV_CLASS_BACKBONE);
		} else {
			meshes[i] = meshlink_open(filename, nodename, "manynodes", DEV_CLASS_PORTABLE);
		}

		meshlink_set_log_cb(meshes[i], MESHLINK_DEBUG, log_message);

		if(!meshes[i]) {
			fprintf(stderr, "errno is: %d\n", meshlink_errno);
			fprintf(stderr, "Could not open %s: %s\n", filename, meshlink_strerror(meshlink_errno));
			return 1;
		}
	}

	int started = 0;

	for(int i = 0; i < n; i++) {
		if(!meshlink_start(meshes[i])) {
			fprintf(stderr, "Could not start node %d: %s\n", i, meshlink_strerror(meshlink_errno));
		} else {
			started++;
		}
	}

	if(!started) {
		fprintf(stderr, "Could not start any node!\n");
		return 1;
	}

	if(graphexporttimeout) {
		exportmeshgraph_begin(graphexporttimeout);
	}

	printf("%d nodes started.\nType /help for a list of commands.\n", started);

	// handle input
	while(fgets(buf, sizeof(buf), stdin)) {
		parse_input(buf);
	}

	exportmeshgraph_end();

	printf("Nodes stopping.\n");

	for(int i = 0; i < n; i++) {
		meshlink_close(meshes[i]);
	}

	return 0;
}
