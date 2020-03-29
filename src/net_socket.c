/*
    net_socket.c -- Handle various kinds of sockets.
    Copyright (C) 2014-2017 Guus Sliepen <guus@meshlink.io>

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

#include "conf.h"
#include "connection.h"
#include "list.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "meta.h"
#include "net.h"
#include "netutl.h"
#include "protocol.h"
#include "utils.h"
#include "xalloc.h"

/* Needed on Mac OS/X */
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static const int max_connection_burst = 100;

/* Setup sockets */

static void configure_tcp(connection_t *c) {
#ifdef O_NONBLOCK
	int flags = fcntl(c->socket, F_GETFL);

	if(fcntl(c->socket, F_SETFL, flags | O_NONBLOCK) < 0) {
		logger(c->mesh, MESHLINK_ERROR, "System call `%s' failed: %s", "fcntl", strerror(errno));
	}

#elif defined(WIN32)
	unsigned long arg = 1;

	if(ioctlsocket(c->socket, FIONBIO, &arg) != 0) {
		logger(c->mesh, MESHLINK_ERROR, "System call `%s' failed: %s", "ioctlsocket", sockstrerror(sockerrno));
	}

#endif

#if defined(SOL_TCP) && defined(TCP_NODELAY)
	int nodelay = 1;
	setsockopt(c->socket, SOL_TCP, TCP_NODELAY, (void *)&nodelay, sizeof(nodelay));
#endif

#if defined(IP_TOS) && defined(IPTOS_LOWDELAY)
	int lowdelay = IPTOS_LOWDELAY;
	setsockopt(c->socket, IPPROTO_IP, IP_TOS, (void *)&lowdelay, sizeof(lowdelay));
#endif
}

static void retry_outgoing_handler(event_loop_t *loop, void *data) {
	assert(data);

	meshlink_handle_t *mesh = loop->data;
	outgoing_t *outgoing = data;
	setup_outgoing_connection(mesh, outgoing);
}

void retry_outgoing(meshlink_handle_t *mesh, outgoing_t *outgoing) {
	if(!mesh->reachable && mesh->loop.now.tv_sec < mesh->last_unreachable + mesh->dev_class_traits[mesh->devclass].fast_retry_period) {
		outgoing->timeout = 1;
	} else {
		outgoing->timeout += 5;
	}

	if(outgoing->timeout > mesh->maxtimeout) {
		outgoing->timeout = mesh->maxtimeout;
	}

	timeout_add(&mesh->loop, &outgoing->ev, retry_outgoing_handler, outgoing, &(struct timespec) {
		outgoing->timeout, prng(mesh, TIMER_FUDGE)
	});

	logger(mesh, MESHLINK_INFO, "Trying to re-establish outgoing connection in %d seconds", outgoing->timeout);
}

void finish_connecting(meshlink_handle_t *mesh, connection_t *c) {
	logger(mesh, MESHLINK_INFO, "Connected to %s", c->name);

	c->last_ping_time = mesh->loop.now.tv_sec;
	c->status.connecting = false;

	send_id(mesh, c);
}

static void handle_meta_write(meshlink_handle_t *mesh, connection_t *c) {
	if(c->outbuf.len <= c->outbuf.offset) {
		return;
	}

	ssize_t outlen = send(c->socket, c->outbuf.data + c->outbuf.offset, c->outbuf.len - c->outbuf.offset, MSG_NOSIGNAL);

	if(outlen <= 0) {
		if(!errno || errno == EPIPE) {
			logger(mesh, MESHLINK_INFO, "Connection closed by %s", c->name);
		} else if(sockwouldblock(sockerrno)) {
			logger(mesh, MESHLINK_DEBUG, "Sending %lu bytes to %s would block", (unsigned long)(c->outbuf.len - c->outbuf.offset), c->name);
			return;
		} else {
			logger(mesh, MESHLINK_ERROR, "Could not send %lu bytes of data to %s: %s", (unsigned long)(c->outbuf.len - c->outbuf.offset), c->name, strerror(errno));
		}

		terminate_connection(mesh, c, c->status.active);
		return;
	}

	buffer_read(&c->outbuf, outlen);

	if(!c->outbuf.len) {
		io_set(&mesh->loop, &c->io, IO_READ);
	}
}

static void handle_meta_io(event_loop_t *loop, void *data, int flags) {
	meshlink_handle_t *mesh = loop->data;
	connection_t *c = data;

	if(c->status.connecting) {
		c->status.connecting = false;

		int result;
		socklen_t len = sizeof(result);
		getsockopt(c->socket, SOL_SOCKET, SO_ERROR, (void *)&result, &len);

		if(!result) {
			finish_connecting(mesh, c);
		} else {
			logger(mesh, MESHLINK_DEBUG, "Error while connecting to %s: %s", c->name, sockstrerror(result));
			terminate_connection(mesh, c, false);
			return;
		}
	}

	if(flags & IO_WRITE) {
		handle_meta_write(mesh, c);
	} else {
		handle_meta_connection_data(mesh, c);
	}
}

// Find edges pointing to this node, and use them to build a list of unique, known addresses.
static struct addrinfo *get_known_addresses(node_t *n) {
	struct addrinfo *ai = NULL;

	for splay_each(edge_t, e, n->edge_tree) {
		if(!e->reverse) {
			continue;
		}

		bool found = false;

		for(struct addrinfo *aip = ai; aip; aip = aip->ai_next) {
			if(!sockaddrcmp(&e->reverse->address, (sockaddr_t *)aip->ai_addr)) {
				found = true;
				break;
			}
		}

		if(found) {
			continue;
		}

		// Create a new struct addrinfo, and put it at the head of the list.
		struct addrinfo *nai = xzalloc(sizeof(*nai) + SALEN(e->reverse->address.sa));
		nai->ai_next = ai;
		ai = nai;

		ai->ai_family = e->reverse->address.sa.sa_family;
		ai->ai_socktype = SOCK_STREAM;
		ai->ai_protocol = IPPROTO_TCP;
		ai->ai_addrlen = SALEN(e->reverse->address.sa);
		ai->ai_addr = (struct sockaddr *)(nai + 1);
		memcpy(ai->ai_addr, &e->reverse->address, ai->ai_addrlen);
	}

	return ai;
}

// Build a list of recently seen addresses.
static struct addrinfo *get_recent_addresses(node_t *n) {
	struct addrinfo *ai = NULL;
	struct addrinfo *aip;

	for(int i = 0; i < 5; i++) {
		if(!n->recent[i].sa.sa_family) {
			break;
		}

		// Create a new struct addrinfo, and put it at the end of the list.
		struct addrinfo *nai = xzalloc(sizeof(*nai) + SALEN(n->recent[i].sa));

		if(!ai) {
			ai = nai;
		} else {
			aip->ai_next = nai;
		}

		aip = nai;

		nai->ai_family = n->recent[i].sa.sa_family;
		nai->ai_socktype = SOCK_STREAM;
		nai->ai_protocol = IPPROTO_TCP;
		nai->ai_addrlen = SALEN(n->recent[i].sa);
		nai->ai_addr = (struct sockaddr *)(nai + 1);
		memcpy(nai->ai_addr, &n->recent[i], nai->ai_addrlen);
	}

	return ai;
}

// Free struct addrinfo list from get_known_addresses().
static void free_known_addresses(struct addrinfo *ai) {
	for(struct addrinfo *aip = ai, *next; aip; aip = next) {
		next = aip->ai_next;
		free(aip);
	}
}

static struct addrinfo *get_canonical_address(node_t *n) {
	if(!n->canonical_address) {
		return false;
	}

	char *address = xstrdup(n->canonical_address);
	char *port = strchr(address, ' ');

	if(!port) {
		free(address);
		return false;
	}

	*port++ = 0;

	struct addrinfo *ai = str2addrinfo(address, port, SOCK_STREAM);
	free(address);

	return ai;
}

static bool get_next_outgoing_address(meshlink_handle_t *mesh, outgoing_t *outgoing) {
	(void)mesh;

	bool start = false;

	if(outgoing->state == OUTGOING_START) {
		start = true;
		outgoing->state = OUTGOING_CANONICAL;
	}

	if(outgoing->state == OUTGOING_CANONICAL) {
		if(!outgoing->aip) {
			outgoing->ai = get_canonical_address(outgoing->node);
			outgoing->aip = outgoing->ai;
		} else {
			outgoing->aip = outgoing->aip->ai_next;
		}

		if(outgoing->aip) {
			return true;
		}

		freeaddrinfo(outgoing->ai);
		outgoing->ai = NULL;
		outgoing->aip = NULL;
		outgoing->state = OUTGOING_RECENT;
	}

	if(outgoing->state == OUTGOING_RECENT) {
		if(!outgoing->aip) {
			outgoing->ai = get_recent_addresses(outgoing->node);
			outgoing->aip = outgoing->ai;
		} else {
			outgoing->aip = outgoing->aip->ai_next;
		}

		if(outgoing->aip) {
			return true;
		}

		free_known_addresses(outgoing->ai);
		outgoing->ai = NULL;
		outgoing->aip = NULL;
		outgoing->state = OUTGOING_KNOWN;
	}

	if(outgoing->state == OUTGOING_KNOWN) {
		if(!outgoing->aip) {
			outgoing->ai = get_known_addresses(outgoing->node);
			outgoing->aip = outgoing->ai;
		} else {
			outgoing->aip = outgoing->aip->ai_next;
		}

		if(outgoing->aip) {
			return true;
		}

		free_known_addresses(outgoing->ai);
		outgoing->ai = NULL;
		outgoing->aip = NULL;
		outgoing->state = OUTGOING_END;
	}

	if(start) {
		outgoing->state = OUTGOING_NO_KNOWN_ADDRESSES;
	}

	return false;
}

void do_outgoing_connection(meshlink_handle_t *mesh, outgoing_t *outgoing) {
	struct addrinfo *proxyai = NULL;
	int result;

begin:

	if(!get_next_outgoing_address(mesh, outgoing)) {
		if(outgoing->state == OUTGOING_NO_KNOWN_ADDRESSES) {
			logger(mesh, MESHLINK_ERROR, "No known addresses for %s", outgoing->node->name);
		} else {
			logger(mesh, MESHLINK_ERROR, "Could not set up a meta connection to %s", outgoing->node->name);
			retry_outgoing(mesh, outgoing);
		}

		return;
	}

	connection_t *c = new_connection();
	c->outgoing = outgoing;

	memcpy(&c->address, outgoing->aip->ai_addr, outgoing->aip->ai_addrlen);

	char *hostname = sockaddr2hostname(&c->address);

	logger(mesh, MESHLINK_INFO, "Trying to connect to %s at %s", outgoing->node->name, hostname);

	if(!mesh->proxytype) {
		c->socket = socket(c->address.sa.sa_family, SOCK_STREAM, IPPROTO_TCP);
		configure_tcp(c);
	} else {
		proxyai = str2addrinfo(mesh->proxyhost, mesh->proxyport, SOCK_STREAM);

		if(!proxyai) {
			free_connection(c);
			free(hostname);
			goto begin;
		}

		logger(mesh, MESHLINK_INFO, "Using proxy at %s port %s", mesh->proxyhost, mesh->proxyport);
		c->socket = socket(proxyai->ai_family, SOCK_STREAM, IPPROTO_TCP);
		configure_tcp(c);
	}

	if(c->socket == -1) {
		logger(mesh, MESHLINK_ERROR, "Creating socket for %s at %s failed: %s", c->name, hostname, sockstrerror(sockerrno));
		free_connection(c);
		free(hostname);
		goto begin;
	}

	free(hostname);

#ifdef FD_CLOEXEC
	fcntl(c->socket, F_SETFD, FD_CLOEXEC);
#endif

#if defined(IPV6_V6ONLY)

	if(c->address.sa.sa_family == AF_INET6) {
		static const int option = 1;
		setsockopt(c->socket, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&option, sizeof(option));
	}

#endif

	/* Connect */

	if(!mesh->proxytype) {
		result = connect(c->socket, &c->address.sa, SALEN(c->address.sa));
	} else {
		result = connect(c->socket, proxyai->ai_addr, proxyai->ai_addrlen);
		freeaddrinfo(proxyai);
	}

	if(result == -1 && !sockinprogress(sockerrno)) {
		logger(mesh, MESHLINK_ERROR, "Could not connect to %s: %s", outgoing->node->name, sockstrerror(sockerrno));
		free_connection(c);

		goto begin;
	}

	/* Now that there is a working socket, fill in the rest and register this connection. */

	c->status.connecting = true;
	c->status.initiator = true;
	c->name = xstrdup(outgoing->node->name);
	c->last_ping_time = mesh->loop.now.tv_sec;

	connection_add(mesh, c);

	io_add(&mesh->loop, &c->io, handle_meta_io, c, c->socket, IO_READ | IO_WRITE);
}

void reset_outgoing(outgoing_t *outgoing) {
	if(outgoing->ai) {
		if(outgoing->state == OUTGOING_RECENT || outgoing->state == OUTGOING_KNOWN) {
			free_known_addresses(outgoing->ai);
		} else {
			freeaddrinfo(outgoing->ai);
		}
	}

	outgoing->ai = NULL;
	outgoing->aip = NULL;
	outgoing->state = OUTGOING_START;
}

void setup_outgoing_connection(meshlink_handle_t *mesh, outgoing_t *outgoing) {
	timeout_del(&mesh->loop, &outgoing->ev);

	if(outgoing->node->connection) {
		logger(mesh, MESHLINK_INFO, "Already connected to %s", outgoing->node->name);

		outgoing->node->connection->outgoing = outgoing;
		return;
	}

	reset_outgoing(outgoing);

	if(outgoing->node->status.blacklisted) {
		return;
	}

	if(mesh->connection_try_cb) {
		mesh->connection_try_cb(mesh, (meshlink_node_t *)outgoing->node);
	}

	do_outgoing_connection(mesh, outgoing);
}

/// Delayed close of a filedescriptor.
static void tarpit(meshlink_handle_t *mesh, int fd) {
	if(!fd) {
		return;
	}

	if(mesh->pits[mesh->next_pit]) {
		closesocket(mesh->pits[mesh->next_pit]);
	}

	mesh->pits[mesh->next_pit++] = fd;

	if(mesh->next_pit >= (int)(sizeof mesh->pits / sizeof mesh->pits[0])) {
		mesh->next_pit = 0;
	}
}

/*
  accept a new tcp connect and create a
  new connection
*/
void handle_new_meta_connection(event_loop_t *loop, void *data, int flags) {
	(void)flags;
	meshlink_handle_t *mesh = loop->data;
	listen_socket_t *l = data;
	connection_t *c;
	sockaddr_t sa;
	int fd;
	socklen_t len = sizeof(sa);

	memset(&sa, 0, sizeof(sa));

	fd = accept(l->tcp.fd, &sa.sa, &len);

	if(fd < 0) {
		if(errno == EINVAL) { // TODO: check if Windows agrees
			event_loop_stop(loop);
			return;
		}

		logger(mesh, MESHLINK_ERROR, "Accepting a new connection failed: %s", sockstrerror(sockerrno));
		return;
	}

	sockaddrunmap(&sa);

	/* Rate limit incoming connections to max_connection_burst/second. */

	if(mesh->loop.now.tv_sec != mesh->connection_burst_time) {
		mesh->connection_burst_time = mesh->loop.now.tv_sec;
		mesh->connection_burst = 0;
	}

	if(mesh->connection_burst >= max_connection_burst) {
		tarpit(mesh, fd);
		return;
	}

	mesh->connection_burst++;

	// Accept the new connection

	c = new_connection();
	c->name = xstrdup("<unknown>");

	c->address = sa;
	c->socket = fd;
	c->last_ping_time = mesh->loop.now.tv_sec;

	char *hostname = sockaddr2hostname(&sa);
	logger(mesh, MESHLINK_INFO, "Connection from %s", hostname);
	free(hostname);

	io_add(&mesh->loop, &c->io, handle_meta_io, c, c->socket, IO_READ);

	configure_tcp(c);

	connection_add(mesh, c);

	c->allow_request = ID;
	send_id(mesh, c);
}

static void free_outgoing(outgoing_t *outgoing) {
	meshlink_handle_t *mesh = outgoing->node->mesh;

	timeout_del(&mesh->loop, &outgoing->ev);

	if(outgoing->ai) {
		if(outgoing->state == OUTGOING_RECENT || outgoing->state == OUTGOING_KNOWN) {
			free_known_addresses(outgoing->ai);
		} else {
			freeaddrinfo(outgoing->ai);
		}
	}

	free(outgoing);
}

void init_outgoings(meshlink_handle_t *mesh) {
	mesh->outgoings = list_alloc((list_action_t)free_outgoing);
}

void exit_outgoings(meshlink_handle_t *mesh) {
	if(mesh->outgoings) {
		list_delete_list(mesh->outgoings);
		mesh->outgoings = NULL;
	}
}
