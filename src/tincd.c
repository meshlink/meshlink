/*
    tincd.c -- the main file for tincd
    Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>

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

#include "system.h"

/* Darwin (MacOS/X) needs the following definition... */
#ifndef _P1003_1B_VISIBLE
#define _P1003_1B_VISIBLE
#endif

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifndef HAVE_MINGW
#include <pwd.h>
#include <grp.h>
#include <time.h>
#endif

#include <getopt.h>

#include "conf.h"
#include "crypto.h"
#include "logger.h"
#include "net.h"
#include "netutl.h"
#include "protocol.h"
#include "utils.h"
#include "xalloc.h"

/* If nonzero, display usage information and exit. */
static bool show_help = false;

/* If nonzero, print the version on standard output and exit.  */
static bool show_version = false;

char **g_argv;                  /* a copy of the cmdline arguments */

static int status = 1;

static struct option const long_options[] = {
	{"config", required_argument, NULL, 'c'},
	{"net", required_argument, NULL, 'n'},
	{"help", no_argument, NULL, 1},
	{"version", no_argument, NULL, 2},
	{"no-detach", no_argument, NULL, 'D'},
	{"debug", optional_argument, NULL, 'd'},
	{"bypass-security", no_argument, NULL, 3},
	{"option", required_argument, NULL, 'o'},
	{NULL, 0, NULL, 0}
};

#ifdef HAVE_MINGW
static struct WSAData wsa_state;
CRITICAL_SECTION mutex;
int main2(int argc, char **argv);
#endif

static void usage(bool status) {
	if(status)
		fprintf(stderr, "Try `tincd --help\' for more information.\n");
	else {
		printf("Usage: tincd [option]...\n\n");
		printf( "  -c, --config=DIR              Read configuration options from DIR.\n"
				"  -D, --no-detach               Don't fork and detach.\n"
				"  -d, --debug[=LEVEL]           Increase debug level or set it to LEVEL.\n"
				"  -n, --net=NETNAME             Connect to net NETNAME.\n"
				"      --bypass-security         Disables meta protocol security, for debugging.\n"
				"  -o, --option[HOST.]KEY=VALUE  Set global/host configuration value.\n"
				"      --help                    Display this help and exit.\n"
				"      --version                 Output version information and exit.\n\n");
		printf("Report bugs to bugs@meshlink.io.\n");
	}
}

static bool parse_options(int argc, char **argv) {
	config_t *cfg;
	int r;
	int option_index = 0;
	int lineno = 0;

	while((r = getopt_long(argc, argv, "c:DLd::n:o:RU:", long_options, &option_index)) != EOF) {
		switch (r) {
			case 0:   /* long option */
				break;

			case 'c': /* config file */
				confbase = xstrdup(optarg);
				break;

			case 'd': /* increase debug level */
				if(!optarg && optind < argc && *argv[optind] != '-')
					optarg = argv[optind++];
				if(optarg)
					mesh->debug_level = atoi(optarg);
				else
					mesh->debug_level++;
				break;

			case 1:   /* show help */
				show_help = true;
				break;

			case 2:   /* show version */
				show_version = true;
				break;

			case 3:   /* bypass security */
				bypass_security = true;
				break;

			case '?': /* wrong options */
				usage(true);
				return false;

			default:
				break;
		}
	}

	if(optind < argc) {
		fprintf(stderr, "%s: unrecognized argument '%s'\n", argv[0], argv[optind]);
		usage(true);
		return false;
	}

	return true;
}

int old_main(int argc, char **argv) {
	if(!parse_options(argc, argv))
		return 1;

	if(show_version) {
		printf("%s version %s (built %s %s, protocol %d.%d)\n", PACKAGE,
			   VERSION, __DATE__, __TIME__, PROT_MAJOR, PROT_MINOR);
		printf("Copyright (C) 1998-2014 Ivo Timmermans, Guus Sliepen and others.\n"
				"See the AUTHORS file for a complete list.\n\n"
				"tinc comes with ABSOLUTELY NO WARRANTY.  This is free software,\n"
				"and you are welcome to redistribute it under certain conditions;\n"
				"see the file COPYING for details.\n");

		return 0;
	}

	if(show_help) {
		usage(false);
		return 0;
	}

#ifdef HAVE_MINGW
	if(WSAStartup(MAKEWORD(2, 2), &wsa_state)) {
		logger(DEBUG_ALWAYS, LOG_ERR, "System call `%s' failed: %s", "WSAStartup", winerror(GetLastError()));
		return 1;
	}
#endif

	openlogger("tinc", LOGMODE_STDERR);

	g_argv = argv;

	init_configuration(&config_tree);

	/* Slllluuuuuuurrrrp! */

	gettimeofday(&now, NULL);
	srand(now.tv_sec + now.tv_usec);
	crypto_init();

	if(!read_server_config())
		return 1;

	char *priority = NULL;

	/* Setup sockets. */

	if(!setup_network())
		goto end;

	/* Start main loop. It only exits when tinc is killed. */

	logger(DEBUG_ALWAYS, LOG_NOTICE, "Ready");

	try_outgoing_connections();

	status = main_loop();

	/* Shutdown properly. */

end:
	close_network_connections();

	logger(DEBUG_ALWAYS, LOG_NOTICE, "Terminating");

	free(priority);

	crypto_exit();

	exit_configuration(&config_tree);

	return status;
}
