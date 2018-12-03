#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "utcp.h"

struct utcp *a;
struct utcp *b;
struct utcp_connection *c;

ssize_t do_recv(struct utcp_connection *x, const void *data, size_t len) {
	if(!len) {
		if(errno) {
			fprintf(stderr, "%p Error: %s\n", (void *)x->utcp, strerror(errno));
		} else {
			fprintf(stderr, "%p Connection closed by peer\n", (void *)x->utcp);
		}

		if(x != c) {
			fprintf(stderr, "closing my side too...\n");
			utcp_close(x);
		}

		return -1;
	}

	if(x == c) {
		return write(0, data, len);
	} else {
		return utcp_send(x, data, len);
	}
}

bool do_pre_accept(struct utcp *utcp, uint16_t port) {
	(void)utcp;
	fprintf(stderr, "pre-accept\n");

	if(port != 7) {
		return false;
	}

	return true;
}

void do_accept(struct utcp_connection *c, uint16_t port) {
	(void)port;
	fprintf(stderr, "accept\n");
	utcp_accept(c, do_recv, NULL);
}

ssize_t do_send(struct utcp *utcp, const void *data, size_t len) {
	static int count = 0;

	if(++count > 1000) {
		fprintf(stderr, "Too many packets!\n");
		abort();
	}

	if(utcp == a) {
		return utcp_recv(b, data, len);
	} else {
		return utcp_recv(a, data, len);
	}
}

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	srand(time(NULL));

	a = utcp_init(do_accept, do_pre_accept, do_send, NULL);
	b = utcp_init(NULL, NULL, do_send, NULL);

	fprintf(stderr, "Testing connection to closed port\n\n");
	c = utcp_connect(b, 6, do_recv, NULL);

	fprintf(stderr, "\nTesting conection to non-listening side\n\n");
	c = utcp_connect(a, 7, do_recv, NULL);

	fprintf(stderr, "\nTesting connection to open port, close\n\n");
	c = utcp_connect(b, 7, do_recv, NULL);
	fprintf(stderr, "closing...\n");
	utcp_close(c);

	fprintf(stderr, "\nTesting connection to open port, abort\n\n");
	c = utcp_connect(b, 7, do_recv, NULL);
	fprintf(stderr, "aborting...\n");
	utcp_abort(c);

	fprintf(stderr, "\nTesting connection with data transfer\n\n");

	c = utcp_connect(b, 7, do_recv, NULL);
	ssize_t len = utcp_send(c, "Hello world!\n", 13);

	if(len != 13) {
		if(len == -1) {
			fprintf(stderr, "Error: %s\n", strerror(errno));
		} else {
			fprintf(stderr, "Short write %zd!\n", len);
		}
	}

	len = utcp_send(c, "This is a test.\n", 16);

	if(len != 16) {
		if(len == -1) {
			fprintf(stderr, "Error: %s\n", strerror(errno));
		} else {
			fprintf(stderr, "Short write %zd!\n", len);
		}
	}

	fprintf(stderr, "closing...\n");
	utcp_close(c);

	fprintf(stderr, "\nTesting connection with huge data transfer\n\n");

	c = utcp_connect(b, 7, do_recv, NULL);
	utcp_set_sndbuf(c, 10240);
	char buf[20480] = "buf";

	len = utcp_send(c, buf, sizeof(buf));

	if(len != 10240) {
		fprintf(stderr, "Error: utcp_send() returned %zd, expected 10240\n", len);
	}

	fprintf(stderr, "closing...\n");
	utcp_close(c);

	utcp_exit(a);
	utcp_exit(b);

	return 0;
}
