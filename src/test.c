#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>


#include "utcp.h"

#define DIR_READ 1
#define DIR_WRITE 2

struct utcp_connection *c;
int dir = DIR_READ | DIR_WRITE;
bool running = true;
long inpktno;
long outpktno;
long dropfrom;
long dropto;
double reorder;
long reorder_dist = 10;
double dropin;
double dropout;
long total_out;
long total_in;
FILE *reference;

char *reorder_data;
size_t reorder_len;
int reorder_countdown;

#if UTCP_DEBUG
void debug(const char *format, ...) {
	struct timeval now;
	gettimeofday(&now, NULL);
	fprintf(stderr, "%lu.%lu ", now.tv_sec, now.tv_usec / 1000);
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}
#else
#define debug(...) do {} while(0)
#endif

ssize_t do_recv(struct utcp_connection *c, const void *data, size_t len) {
	(void)c;

	if(!data || !len) {
		if(errno) {
			debug("Error: %s\n", strerror(errno));
			dir = 0;
		} else {
			dir &= ~DIR_WRITE;
			debug("Connection closed by peer\n");
		}

		return -1;
	}

	if(reference) {
		char buf[len];

		if(fread(buf, len, 1, reference) != 1) {
			debug("Error reading reference\n");
			abort();
		}

		if(memcmp(buf, data, len)) {
			debug("Received data differs from reference\n");
			abort();
		}
	}

	return write(1, data, len);
}

void do_accept(struct utcp_connection *nc, uint16_t port) {
	(void)port;
	utcp_accept(nc, do_recv, NULL);
	c = nc;
	utcp_set_accept_cb(c->utcp, NULL, NULL);
}

ssize_t do_send(struct utcp *utcp, const void *data, size_t len) {
	int s = *(int *)utcp->priv;
	outpktno++;

	if(outpktno >= dropfrom && outpktno < dropto) {
		if(drand48() < dropout) {
			debug("Dropped outgoing packet\n");
			return len;
		}

		if(!reorder_data && drand48() < reorder) {
			reorder_data = malloc(len);

			if(!reorder_data) {
				debug("Out of memory\n");
				return len;
			}

			reorder_len = len;
			memcpy(reorder_data, data, len);
			reorder_countdown = 1 + drand48() * reorder_dist;
			return len;
		}
	}

	if(reorder_data) {
		if(--reorder_countdown < 0) {
			total_out += reorder_len;
			send(s, reorder_data, reorder_len, MSG_DONTWAIT);
			free(reorder_data);
			reorder_data = NULL;
		}
	}

	total_out += len;
	ssize_t result = send(s, data, len, MSG_DONTWAIT);

	if(result <= 0) {
		debug("Error sending UDP packet: %s\n", strerror(errno));
	}

	return result;
}

int main(int argc, char *argv[]) {
	srand(time(NULL));
	srand48(time(NULL));

	if(argc < 2 || argc > 3) {
		return 1;
	}

	bool server = argc == 2;
	bool connected = false;
	uint32_t flags = UTCP_TCP;
	size_t read_size = 102400;

	if(getenv("DROPIN")) {
		dropin = atof(getenv("DROPIN"));
	}

	if(getenv("DROPOUT")) {
		dropout = atof(getenv("DROPOUT"));
	}

	if(getenv("DROPFROM")) {
		dropfrom = atoi(getenv("DROPFROM"));
	}

	if(getenv("DROPTO")) {
		dropto = atoi(getenv("DROPTO"));
	}

	if(getenv("REORDER")) {
		reorder = atof(getenv("REORDER"));
	}

	if(getenv("REORDER_DIST")) {
		reorder_dist = atoi(getenv("REORDER_DIST"));
	}

	if(getenv("FLAGS")) {
		flags = atoi(getenv("FLAGS"));
	}

	if(getenv("READ_SIZE")) {
		read_size = atoi(getenv("READ_SIZE"));
	}

	char *reference_filename = getenv("REFERENCE");

	if(reference_filename) {
		reference = fopen(reference_filename, "r");
	}

	if(dropto < dropfrom) {
		dropto = 1 << 30;
	}

	struct addrinfo *ai;

	struct addrinfo hint = {
		.ai_flags = server ? AI_PASSIVE : 0,
		.ai_socktype = SOCK_DGRAM,
	};

	getaddrinfo(server ? NULL : argv[1], server ? argv[1] : argv[2], &hint, &ai);

	if(!ai) {
		debug("Could not lookup address: %s\n", strerror(errno));
		return 1;
	}

	int s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

	if(s == -1) {
		debug("Could not create socket: %s\n", strerror(errno));
		return 1;
	}

	static const int one = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

	if(server) {
		if(bind(s, ai->ai_addr, ai->ai_addrlen)) {
			debug("Could not bind: %s\n", strerror(errno));
			return 1;
		}
	} else {
		if(connect(s, ai->ai_addr, ai->ai_addrlen)) {
			debug("Could not connect: %s\n", strerror(errno));
			return 1;
		}

		connected = true;
	}

	freeaddrinfo(ai);

	struct utcp *u = utcp_init(server ? do_accept : NULL, NULL, do_send, &s);

	if(!u) {
		debug("Could not initialize UTCP\n");
		return 1;
	}

	utcp_set_mtu(u, 1300);
	utcp_set_user_timeout(u, 10);

	if(!server) {
		c = utcp_connect_ex(u, 1, do_recv, NULL, flags);
	}

	struct pollfd fds[2] = {
		{.fd = 0, .events = POLLIN | POLLERR | POLLHUP},
		{.fd = s, .events = POLLIN | POLLERR | POLLHUP},
	};

	char buf[102400];

	struct timeval timeout = utcp_timeout(u);

	while(!connected || utcp_is_active(u)) {
		size_t max = c ? utcp_get_sndbuf_free(c) : 0;

		if(max > sizeof(buf)) {
			max = sizeof(buf);
		}

		if(max > read_size) {
			max = read_size;
		}

		int timeout_ms = timeout.tv_sec * 1000 + timeout.tv_usec / 1000 + 1;

		debug("polling, dir = %d, timeout = %d\n", dir, timeout_ms);

		if((dir & DIR_READ) && max) {
			poll(fds, 2, timeout_ms);
		} else {
			poll(fds + 1, 1, timeout_ms);
		}

		if(fds[0].revents) {
			fds[0].revents = 0;
			debug("stdin\n");
			ssize_t len = read(0, buf, max);

			if(len <= 0) {
				fds[0].fd = -1;
				dir &= ~DIR_READ;

				if(c) {
					utcp_shutdown(c, SHUT_WR);
				}

				if(len == -1) {
					break;
				} else {
					continue;
				}
			}

			if(c) {
				ssize_t sent = utcp_send(c, buf, len);

				if(sent != len) {
					debug("Short send: %zd != %zd\n", sent, len);
				}
			}
		}

		if(fds[1].revents) {
			fds[1].revents = 0;
			debug("netin\n");
			struct sockaddr_storage ss;
			socklen_t sl = sizeof(ss);
			int len = recvfrom(s, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *)&ss, &sl);

			if(len <= 0) {
				debug("Error receiving UDP packet: %s\n", strerror(errno));
				break;
			}

			if(!connected)
				if(!connect(s, (struct sockaddr *)&ss, sl)) {
					connected = true;
				}

			inpktno++;

			if(inpktno >= dropto || inpktno < dropfrom || drand48() >= dropin) {
				total_in += len;

				if(utcp_recv(u, buf, len) == -1) {
					debug("Error receiving UTCP packet: %s\n", strerror(errno));
				}
			} else {
				debug("Dropped incoming packet\n");
			}
		}

		timeout = utcp_timeout(u);
	};

	utcp_close(c);

	utcp_exit(u);

	free(reorder_data);

	debug("Total bytes in: %ld, out: %ld\n", total_in, total_out);

	return 0;
}
