#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <curses.h>
#include <math.h>
#include <assert.h>
#include <sys/socket.h>
#include <netdb.h>

#include "../src/meshlink.h"
#include "../src/devtools.h"

static WINDOW *topwin;
static WINDOW *nodewin;
static WINDOW *splitwin;
static WINDOW *logwin;
static WINDOW *statuswin;
static float splitpoint = 0.5;

static meshlink_handle_t *mesh;
static meshlink_node_t **nodes;
static size_t nnodes;

static void log_message(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	(void)mesh;

	wattron(logwin, COLOR_PAIR(level));
	wprintw(logwin, "%s\n", text);
	wattroff(logwin, COLOR_PAIR(level));
	wrefresh(logwin);
}

static void do_resize() {
	const int nodelines = lrintf((LINES - 3) * splitpoint);
	const int loglines = (LINES - 3) - nodelines;
	assert(nodelines > 1);
	assert(loglines > 1);
	assert(COLS > 1);

	mvwin(topwin, 0, 0);
	wresize(topwin, 1, COLS);

	mvwin(nodewin, 1, 0);
	wresize(nodewin, nodelines, COLS);

	mvwin(splitwin, 1 + nodelines, 0);
	wresize(splitwin, 1, COLS);

	mvwin(logwin, 2 + nodelines, 0);
	wresize(logwin, loglines, COLS);

	mvwin(statuswin, LINES - 1, 0);
	wresize(statuswin, 1, COLS);
}


static void do_redraw_nodes() {
	werase(nodewin);
	nodes = meshlink_get_all_nodes(mesh, nodes, &nnodes);

	for(size_t i = 0; i < nnodes; i++) {
		devtool_node_status_t status;
		devtool_get_node_status(mesh, nodes[i], &status);
		char host[NI_MAXHOST] = "";
		char serv[NI_MAXSERV] = "";
		getnameinfo((struct sockaddr *)&status.address, sizeof status.address, host, sizeof host, serv, sizeof serv, NI_NUMERICHOST | NI_NUMERICSERV);
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

		mvwprintw(nodewin, i, 0, "%-16s  %-12s  %-32s %5s  %c%5d", nodes[i]->name, desc, host, serv, mtustate, status.maxmtu);
	}

	wnoutrefresh(nodewin);
}

static void do_redraw() {
	// Draw top line
	werase(topwin);
	mvwprintw(topwin, 0, 0, "%-16s  %-12s  %-32s %5s  %6s", "Node:", "Status:", "UDP address:", "Port:", "MTU:");
	wnoutrefresh(topwin);

	// Draw middle line
	werase(splitwin);
	mvwprintw(splitwin, 0, 0, "Log output:");
	wnoutrefresh(splitwin);

	// Draw bottom line
	werase(statuswin);
	mvwprintw(statuswin, 0, 0, "Status bar");
	wnoutrefresh(statuswin);

	wnoutrefresh(logwin);

	do_redraw_nodes();
}

static void node_status(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;
	(void)node;
	(void)reachable;
	do_redraw_nodes();
	doupdate();
}

int main(int argc, char *argv[]) {
	const char *confbase = ".monitor";
	const char *id = NULL;

	if(argc > 1) {
		confbase = argv[1];
	}

	if(argc > 2) {
		id = argv[2];
	}

	initscr();
	start_color();
	curs_set(false);
	noecho();

	topwin = newwin(1, COLS, 0, 0);
	nodewin = newwin(1, COLS, 1, 0);
	splitwin = newwin(1, COLS, 2, 0);
	logwin = newwin(1, COLS, 3, 0);
	statuswin = newwin(1, COLS, 4, 0);

	leaveok(topwin, true);
	leaveok(nodewin, true);
	leaveok(splitwin, true);
	leaveok(logwin, true);
	leaveok(statuswin, true);

	wattrset(topwin, A_REVERSE);
	wattrset(splitwin, A_REVERSE);
	wattrset(statuswin, A_REVERSE);

	wbkgdset(topwin, ' ' | A_REVERSE);
	wbkgdset(splitwin, ' ' | A_REVERSE);
	wbkgdset(statuswin, ' ' | A_REVERSE);

	init_pair(1, COLOR_GREEN, -1);
	init_pair(2, COLOR_YELLOW, -1);
	init_pair(3, COLOR_RED, -1);
	init_pair(4, COLOR_RED, -1);

	scrollok(logwin, true);

	do_resize();
	do_redraw();
	doupdate();

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_message);

	mesh = meshlink_open(confbase, id, "monitor", DEV_CLASS_STATIONARY);

	if(!mesh) {
		endwin();
		fprintf(stderr, "Could not open MeshLink: %s\n", meshlink_strerror(meshlink_errno));
		return 1;
	}

	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, log_message);
	meshlink_set_node_status_cb(mesh, node_status);

	if(!meshlink_start(mesh)) {
		endwin();
		fprintf(stderr, "Could not start MeshLink: %s\n", meshlink_strerror(meshlink_errno));
		return 1;
	}

	bool running = true;
	timeout(500);
	wtimeout(topwin, 500);

	do_redraw();
	doupdate();

	while(running) {
		int key = wgetch(topwin);

		switch(key) {
		case 'q':
		case 27:
		case KEY_BREAK:
			running = false;
			break;

		case KEY_RESIZE:
			do_resize();
			break;

		case 'r':
		case KEY_REFRESH:
			clearok(topwin, true);
			clearok(nodewin, true);
			clearok(splitwin, true);
			clearok(logwin, true);
			clearok(statuswin, true);
			break;
		}

		do_redraw();
		doupdate();
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);

	endwin();

	return 0;
}
