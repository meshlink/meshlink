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

int addressfamily = AF_UNSPEC;
int seconds_till_retry = 5;
int max_connection_burst = 100;

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

static bool bind_to_address(meshlink_handle_t *mesh, connection_t *c) {
	int s = -1;

	for(int i = 0; i < mesh->listen_sockets && mesh->listen_socket[i].bindto; i++) {
		if(mesh->listen_socket[i].sa.sa.sa_family != c->address.sa.sa_family) {
			continue;
		}

		if(s >= 0) {
			return false;
		}

		s = i;
	}

	if(s < 0) {
		return false;
	}

	sockaddr_t sa = mesh->listen_socket[s].sa;

	if(sa.sa.sa_family == AF_INET) {
		sa.in.sin_port = 0;
	} else if(sa.sa.sa_family == AF_INET6) {
		sa.in6.sin6_port = 0;
	}

	if(bind(c->socket, &sa.sa, SALEN(sa.sa))) {
		logger(mesh, MESHLINK_WARNING, "Can't bind outgoing socket: %s", strerror(errno));
		return false;
	}

	return true;
}

int setup_listen_socket(const sockaddr_t *sa) {
	int nfd;
	char *addrstr;
	int option;

	nfd = socket(sa->sa.sa_family, SOCK_STREAM, IPPROTO_TCP);

	if(nfd < 0) {
		logger(NULL, MESHLINK_ERROR, "Creating metasocket failed: %s", sockstrerror(sockerrno));
		return -1;
	}

#ifdef FD_CLOEXEC
	fcntl(nfd, F_SETFD, FD_CLOEXEC);
#endif

	/* Optimize TCP settings */

	option = 1;
	setsockopt(nfd, SOL_SOCKET, SO_REUSEADDR, (void *)&option, sizeof(option));

#if defined(IPV6_V6ONLY)

	if(sa->sa.sa_family == AF_INET6) {
		setsockopt(nfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&option, sizeof(option));
	}

#else
#warning IPV6_V6ONLY not defined
#endif

	if(bind(nfd, &sa->sa, SALEN(sa->sa))) {
		closesocket(nfd);
		addrstr = sockaddr2hostname(sa);
		logger(NULL, MESHLINK_ERROR, "Can't bind to %s/tcp: %s", addrstr, sockstrerror(sockerrno));
		free(addrstr);
		return -1;
	}

	if(listen(nfd, 3)) {
		closesocket(nfd);
		logger(NULL, MESHLINK_ERROR, "System call `%s' failed: %s", "listen", sockstrerror(sockerrno));
		return -1;
	}

	return nfd;
}

int setup_vpn_in_socket(meshlink_handle_t *mesh, const sockaddr_t *sa) {
	int nfd;
	char *addrstr;
	int option;

	nfd = socket(sa->sa.sa_family, SOCK_DGRAM, IPPROTO_UDP);

	if(nfd < 0) {
		logger(mesh, MESHLINK_ERROR, "Creating UDP socket failed: %s", sockstrerror(sockerrno));
		return -1;
	}

#ifdef FD_CLOEXEC
	fcntl(nfd, F_SETFD, FD_CLOEXEC);
#endif

#ifdef O_NONBLOCK
	{
		int flags = fcntl(nfd, F_GETFL);

		if(fcntl(nfd, F_SETFL, flags | O_NONBLOCK) < 0) {
			closesocket(nfd);
			logger(mesh, MESHLINK_ERROR, "System call `%s' failed: %s", "fcntl",
			       strerror(errno));
			return -1;
		}
	}
#elif defined(WIN32)
	{
		unsigned long arg = 1;

		if(ioctlsocket(nfd, FIONBIO, &arg) != 0) {
			closesocket(nfd);
			logger(mesh, MESHLINK_ERROR, "Call to `%s' failed: %s", "ioctlsocket", sockstrerror(sockerrno));
			return -1;
		}
	}
#endif

	option = 1;
	setsockopt(nfd, SOL_SOCKET, SO_REUSEADDR, (void *)&option, sizeof(option));
	setsockopt(nfd, SOL_SOCKET, SO_BROADCAST, (void *)&option, sizeof(option));

#if defined(IPV6_V6ONLY)

	if(sa->sa.sa_family == AF_INET6) {
		setsockopt(nfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&option, sizeof(option));
	}

#endif

#if defined(IP_DONTFRAG) && !defined(IP_DONTFRAGMENT)
#define IP_DONTFRAGMENT IP_DONTFRAG
#endif

#if defined(IP_MTU_DISCOVER) && defined(IP_PMTUDISC_DO)

	if(mesh->self->options & OPTION_PMTU_DISCOVERY) {
		option = IP_PMTUDISC_DO;
		setsockopt(nfd, IPPROTO_IP, IP_MTU_DISCOVER, (void *)&option, sizeof(option));
	}

#elif defined(IP_DONTFRAGMENT)

	if(mesh->self->options & OPTION_PMTU_DISCOVERY) {
		option = 1;
		setsockopt(nfd, IPPROTO_IP, IP_DONTFRAGMENT, (void *)&option, sizeof(option));
	}

#else
#warning No way to disable IPv4 fragmentation
#endif

#if defined(IPV6_MTU_DISCOVER) && defined(IPV6_PMTUDISC_DO)

	if(mesh->self->options & OPTION_PMTU_DISCOVERY) {
		option = IPV6_PMTUDISC_DO;
		setsockopt(nfd, IPPROTO_IPV6, IPV6_MTU_DISCOVER, (void *)&option, sizeof(option));
	}

#elif defined(IPV6_DONTFRAG)

	if(mesh->self->options & OPTION_PMTU_DISCOVERY) {
		option = 1;
		setsockopt(nfd, IPPROTO_IPV6, IPV6_DONTFRAG, (void *)&option, sizeof(option));
	}

#else
#warning No way to disable IPv6 fragmentation
#endif

	if(bind(nfd, &sa->sa, SALEN(sa->sa))) {
		closesocket(nfd);
		addrstr = sockaddr2hostname(sa);
		logger(mesh, MESHLINK_ERROR, "Can't bind to %s/udp: %s", addrstr, sockstrerror(sockerrno));
		free(addrstr);
		return -1;
	}

	return nfd;
} /* int setup_vpn_in_socket */

static void retry_outgoing_handler(event_loop_t *loop, void *data) {
	meshlink_handle_t *mesh = loop->data;
	outgoing_t *outgoing = data;
	setup_outgoing_connection(mesh, outgoing);
}

void retry_outgoing(meshlink_handle_t *mesh, outgoing_t *outgoing) {
	outgoing->timeout += 5;

	if(outgoing->timeout > mesh->maxtimeout) {
		outgoing->timeout = mesh->maxtimeout;
	}

	timeout_add(&mesh->loop, &outgoing->ev, retry_outgoing_handler, outgoing, &(struct timeval) {
		outgoing->timeout, rand() % 100000
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

// Free struct addrinfo list from get_known_addresses().
static void free_known_addresses(struct addrinfo *ai) {
	for(struct addrinfo *aip = ai, *next; aip; aip = next) {
		next = aip->ai_next;
		free(aip);
	}
}

static bool get_recent(meshlink_handle_t *mesh, outgoing_t *outgoing) {
	node_t *n = lookup_node(mesh, outgoing->name);

	if(!n) {
		return false;
	}

	outgoing->ai = get_known_addresses(n);
	outgoing->aip = outgoing->ai;
	return outgoing->aip;
}

static bool get_next_ai(meshlink_handle_t *mesh, outgoing_t *outgoing) {
	if(!outgoing->ai) {
		char *address = NULL;

		if(get_config_string(outgoing->cfg, &address)) {
			char *port;
			char *space = strchr(address, ' ');

			if(space) {
				port = xstrdup(space + 1);
				*space = 0;
			} else {
				if(!get_config_string(lookup_config(outgoing->config_tree, "Port"), &port)) {
					logger(mesh, MESHLINK_ERROR, "No Port known for %s", outgoing->name);
					return false;
				}
			}

			outgoing->ai = str2addrinfo(address, port, SOCK_STREAM);
			free(port);
			free(address);
		}

		outgoing->aip = outgoing->ai;
	} else {
		outgoing->aip = outgoing->aip->ai_next;
	}

	return outgoing->aip;
}

static bool get_next_cfg(meshlink_handle_t *mesh, outgoing_t *outgoing, char *variable) {
	(void)mesh;

	if(!outgoing->cfg) {
		outgoing->cfg = lookup_config(outgoing->config_tree, variable);
	} else {
		outgoing->cfg = lookup_config_next(outgoing->config_tree, outgoing->cfg);
	}

	return outgoing->cfg;
}

static bool get_next_outgoing_address(meshlink_handle_t *mesh, outgoing_t *outgoing) {
	bool start = false;

	if(outgoing->state == OUTGOING_START) {
		start = true;
		outgoing->state = OUTGOING_CANONICAL;
	}

	if(outgoing->state == OUTGOING_CANONICAL) {
		while(outgoing->aip || get_next_cfg(mesh, outgoing, "CanonicalAddress")) {
			if(get_next_ai(mesh, outgoing)) {
				return true;
			} else {
				freeaddrinfo(outgoing->ai);
				outgoing->ai = NULL;
				outgoing->aip = NULL;
			}
		}

		outgoing->state = OUTGOING_RECENT;
	}

	if(outgoing->state == OUTGOING_RECENT) {
		while(outgoing->aip || get_next_cfg(mesh, outgoing, "Address")) {
			if(get_next_ai(mesh, outgoing)) {
				return true;
			} else {
				freeaddrinfo(outgoing->ai);
				outgoing->ai = NULL;
				outgoing->aip = NULL;
			}
		}

		outgoing->state = OUTGOING_KNOWN;
	}

	if(outgoing->state == OUTGOING_KNOWN) {
		if(outgoing->aip || get_recent(mesh, outgoing)) {
			if(get_next_ai(mesh, outgoing)) {
				return true;
			} else {
				free_known_addresses(outgoing->ai);
				outgoing->ai = NULL;
				outgoing->aip = NULL;
			}
		}

		outgoing->state = OUTGOING_END;
	}

	if(start) {
		outgoing->state = OUTGOING_NO_KNOWN_ADDRESSES;
	}

	return false;
}

bool do_outgoing_connection(meshlink_handle_t *mesh, outgoing_t *outgoing) {
	struct addrinfo *proxyai = NULL;
	int result;

begin:

	if(!get_next_outgoing_address(mesh, outgoing)) {
		if(outgoing->state == OUTGOING_NO_KNOWN_ADDRESSES) {
			logger(mesh, MESHLINK_ERROR, "No known addresses for %s", outgoing->name);
		} else {
			logger(mesh, MESHLINK_ERROR, "Could not set up a meta connection to %s", outgoing->name);
			retry_outgoing(mesh, outgoing);
		}

		return false;
	}

	connection_t *c = new_connection();
	c->outgoing = outgoing;

	memcpy(&c->address, outgoing->aip->ai_addr, outgoing->aip->ai_addrlen);

	char *hostname = sockaddr2hostname(&c->address);

	logger(mesh, MESHLINK_INFO, "Trying to connect to %s at %s", outgoing->name, hostname);

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

	bind_to_address(mesh, c);

	/* Connect */

	if(!mesh->proxytype) {
		result = connect(c->socket, &c->address.sa, SALEN(c->address.sa));
	} else {
		result = connect(c->socket, proxyai->ai_addr, proxyai->ai_addrlen);
		freeaddrinfo(proxyai);
	}

	if(result == -1 && !sockinprogress(sockerrno)) {
		logger(mesh, MESHLINK_ERROR, "Could not connect to %s: %s", outgoing->name, sockstrerror(sockerrno));
		free_connection(c);

		goto begin;
	}

	/* Now that there is a working socket, fill in the rest and register this connection. */

	c->status.connecting = true;
	c->name = xstrdup(outgoing->name);
	c->outcompression = mesh->self->connection->outcompression;
	c->last_ping_time = mesh->loop.now.tv_sec;

	connection_add(mesh, c);

	io_add(&mesh->loop, &c->io, handle_meta_io, c, c->socket, IO_READ | IO_WRITE);

	return true;
}

void setup_outgoing_connection(meshlink_handle_t *mesh, outgoing_t *outgoing) {
	bool blacklisted = false;
	timeout_del(&mesh->loop, &outgoing->ev);

	node_t *n = lookup_node(mesh, outgoing->name);

	if(n && n->connection) {
		logger(mesh, MESHLINK_INFO, "Already connected to %s", outgoing->name);

		n->connection->outgoing = outgoing;
		return;
	}


	if(outgoing->ai) {
		if(outgoing->state == OUTGOING_KNOWN) {
			free_known_addresses(outgoing->ai);
		} else {
			freeaddrinfo(outgoing->ai);
		}
	}

	outgoing->cfg = NULL;

	exit_configuration(&outgoing->config_tree); // discard old configuration if present
	init_configuration(&outgoing->config_tree);
	read_host_config(mesh, outgoing->config_tree, outgoing->name);
	get_config_bool(lookup_config(outgoing->config_tree, "blacklisted"), &blacklisted);

	outgoing->state = OUTGOING_START;

	if(blacklisted) {
		return;
	}

	do_outgoing_connection(mesh, outgoing);
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
	sockaddr_t sa = {0};
	int fd;
	socklen_t len = sizeof(sa);

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

	// Check if we get many connections from the same host

	static sockaddr_t prev_sa;
	static int tarpit = -1;

	if(tarpit >= 0) {
		closesocket(tarpit);
		tarpit = -1;
	}

	if(!sockaddrcmp_noport(&sa, &prev_sa)) {
		static int samehost_burst;
		static int samehost_burst_time;

		if(mesh->loop.now.tv_sec - samehost_burst_time > samehost_burst) {
			samehost_burst = 0;
		} else {
			samehost_burst -= mesh->loop.now.tv_sec - samehost_burst_time;
		}

		samehost_burst_time = mesh->loop.now.tv_sec;
		samehost_burst++;

		if(samehost_burst > max_connection_burst) {
			tarpit = fd;
			return;
		}
	}

	memcpy(&prev_sa, &sa, sizeof(sa));

	// Check if we get many connections from different hosts

	static int connection_burst;
	static int connection_burst_time;

	if(mesh->loop.now.tv_sec - connection_burst_time > connection_burst) {
		connection_burst = 0;
	} else {
		connection_burst -= mesh->loop.now.tv_sec - connection_burst_time;
	}

	connection_burst_time = mesh->loop.now.tv_sec;
	connection_burst++;

	if(connection_burst >= max_connection_burst) {
		connection_burst = max_connection_burst;
		tarpit = fd;
		return;
	}

	// Accept the new connection

	c = new_connection();
	c->name = xstrdup("<unknown>");
	c->outcompression = mesh->self->connection->outcompression;

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
	meshlink_handle_t *mesh = outgoing->mesh;

	timeout_del(&mesh->loop, &outgoing->ev);

	if(outgoing->ai) {
		if(outgoing->state == OUTGOING_KNOWN) {
			free_known_addresses(outgoing->ai);
		} else {
			freeaddrinfo(outgoing->ai);
		}
	}

	if(outgoing->config_tree) {
		exit_configuration(&outgoing->config_tree);
	}

	if(outgoing->name) {
		free(outgoing->name);
	}

	free(outgoing);
}

void try_outgoing_connections(meshlink_handle_t *mesh) {
	/* If there is no outgoing list yet, create one. Otherwise, mark all outgoings as deleted. */

	if(!mesh->outgoings) {
		mesh->outgoings = list_alloc((list_action_t)free_outgoing);
	} else {
		for list_each(outgoing_t, outgoing, mesh->outgoings) {
			outgoing->timeout = -1;
		}
	}

	/* Make sure there is one outgoing_t in the list for each ConnectTo. */

	// TODO: Drop support for ConnectTo since AutoConnect is now always on?
	for(config_t *cfg = lookup_config(mesh->config, "ConnectTo"); cfg; cfg = lookup_config_next(mesh->config, cfg)) {
		char *name;
		get_config_string(cfg, &name);

		if(!check_id(name)) {
			logger(mesh, MESHLINK_ERROR,
			       "Invalid name for outgoing connection in line %d",
			       cfg->line);
			free(name);
			continue;
		}

		bool found = false;

		for list_each(outgoing_t, outgoing, mesh->outgoings) {
			if(!strcmp(outgoing->name, name)) {
				found = true;
				outgoing->timeout = 0;
				break;
			}
		}

		if(!found) {
			outgoing_t *outgoing = xzalloc(sizeof(*outgoing));
			outgoing->mesh = mesh;
			outgoing->name = name;
			list_insert_tail(mesh->outgoings, outgoing);
			setup_outgoing_connection(mesh, outgoing);
		}
	}

	/* Terminate any connections whose outgoing_t is to be deleted. */

	for list_each(connection_t, c, mesh->connections) {
		if(c->outgoing && c->outgoing->timeout == -1) {
			c->outgoing = NULL;
			logger(mesh, MESHLINK_INFO, "No more outgoing connection to %s", c->name);
			terminate_connection(mesh, c, c->status.active);
		}
	}

	/* Delete outgoing_ts for which there is no ConnectTo. */

	for list_each(outgoing_t, outgoing, mesh->outgoings)
		if(outgoing->timeout == -1) {
			list_delete_node(mesh->outgoings, node);
		}
}
