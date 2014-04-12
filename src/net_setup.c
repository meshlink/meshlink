/*
    net_setup.c -- Setup.
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

#include "cipher.h"
#include "conf.h"
#include "connection.h"
#include "digest.h"
#include "ecdsa.h"
#include "graph.h"
#include "logger.h"
#include "net.h"
#include "netutl.h"
#include "protocol.h"
#include "route.h"
#include "utils.h"
#include "xalloc.h"

char *myport;

char *proxyhost;
char *proxyport;
char *proxyuser;
char *proxypass;
proxytype_t proxytype;
int autoconnect;
bool disablebuggypeers;

bool node_read_ecdsa_public_key(node_t *n) {
	if(ecdsa_active(n->ecdsa))
		return true;

	splay_tree_t *config_tree;
	FILE *fp;
	char *pubname = NULL;
	char *p;

	init_configuration(&config_tree);
	if(!read_host_config(config_tree, n->name))
		goto exit;

	/* First, check for simple ECDSAPublicKey statement */

	if(get_config_string(lookup_config(config_tree, "ECDSAPublicKey"), &p)) {
		n->ecdsa = ecdsa_set_base64_public_key(p);
		free(p);
		goto exit;
	}

	/* Else, check for ECDSAPublicKeyFile statement and read it */

	if(!get_config_string(lookup_config(config_tree, "ECDSAPublicKeyFile"), &pubname))
		xasprintf(&pubname, "%s" SLASH "hosts" SLASH "%s", confbase, n->name);

	fp = fopen(pubname, "r");

	if(!fp)
		goto exit;

	n->ecdsa = ecdsa_read_pem_public_key(fp);
	fclose(fp);

exit:
	exit_configuration(&config_tree);
	free(pubname);
	return n->ecdsa;
}

bool read_ecdsa_public_key(connection_t *c) {
	if(ecdsa_active(c->ecdsa))
		return true;

	FILE *fp;
	char *fname;
	char *p;

	if(!c->config_tree) {
		init_configuration(&c->config_tree);
		if(!read_host_config(c->config_tree, c->name))
			return false;
	}

	/* First, check for simple ECDSAPublicKey statement */

	if(get_config_string(lookup_config(c->config_tree, "ECDSAPublicKey"), &p)) {
		c->ecdsa = ecdsa_set_base64_public_key(p);
		free(p);
		return c->ecdsa;
	}

	/* Else, check for ECDSAPublicKeyFile statement and read it */

	if(!get_config_string(lookup_config(c->config_tree, "ECDSAPublicKeyFile"), &fname))
		xasprintf(&fname, "%s" SLASH "hosts" SLASH "%s", confbase, c->name);

	fp = fopen(fname, "r");

	if(!fp) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Error reading ECDSA public key file `%s': %s",
			   fname, strerror(errno));
		free(fname);
		return false;
	}

	c->ecdsa = ecdsa_read_pem_public_key(fp);
	fclose(fp);

	if(!c->ecdsa)
		logger(DEBUG_ALWAYS, LOG_ERR, "Parsing ECDSA public key file `%s' failed.", fname);
	free(fname);
	return c->ecdsa;
}

static bool read_ecdsa_private_key(void) {
	FILE *fp;
	char *fname;

	/* Check for PrivateKeyFile statement and read it */

	if(!get_config_string(lookup_config(config_tree, "ECDSAPrivateKeyFile"), &fname))
		xasprintf(&fname, "%s" SLASH "ecdsa_key.priv", confbase);

	fp = fopen(fname, "r");

	if(!fp) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Error reading ECDSA private key file `%s': %s", fname, strerror(errno));
		if(errno == ENOENT)
			logger(DEBUG_ALWAYS, LOG_INFO, "Create an ECDSA keypair with `tinc generate-ecdsa-keys'.");
		free(fname);
		return false;
	}

#if !defined(HAVE_MINGW) && !defined(HAVE_CYGWIN)
	struct stat s;

	if(fstat(fileno(fp), &s)) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Could not stat ECDSA private key file `%s': %s'", fname, strerror(errno));
		free(fname);
		return false;
	}

	if(s.st_mode & ~0100700)
		logger(DEBUG_ALWAYS, LOG_WARNING, "Warning: insecure file permissions for ECDSA private key file `%s'!", fname);
#endif

	myself->connection->ecdsa = ecdsa_read_pem_private_key(fp);
	fclose(fp);

	if(!myself->connection->ecdsa)
		logger(DEBUG_ALWAYS, LOG_ERR, "Reading ECDSA private key file `%s' failed: %s", fname, strerror(errno));
	free(fname);
	return myself->connection->ecdsa;
}

static bool read_invitation_key(void) {
	FILE *fp;
	char *fname;

	if(invitation_key) {
		ecdsa_free(invitation_key);
		invitation_key = NULL;
	}

	xasprintf(&fname, "%s" SLASH "invitations" SLASH "ecdsa_key.priv", confbase);

	fp = fopen(fname, "r");

	if(fp) {
		invitation_key = ecdsa_read_pem_private_key(fp);
		fclose(fp);
		if(!invitation_key)
			logger(DEBUG_ALWAYS, LOG_ERR, "Reading ECDSA private key file `%s' failed: %s", fname, strerror(errno));
	}

	free(fname);
	return invitation_key;
}

static timeout_t keyexpire_timeout;

static void keyexpire_handler(void *data) {
	regenerate_key();
	timeout_set(data, &(struct timeval){keylifetime, rand() % 100000});
}

void regenerate_key(void) {
	logger(DEBUG_STATUS, LOG_INFO, "Expiring symmetric keys");
	send_key_changed();
}

void load_all_nodes(void) {
	DIR *dir;
	struct dirent *ent;
	char *dname;

	xasprintf(&dname, "%s" SLASH "hosts", confbase);
	dir = opendir(dname);
	if(!dir) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Could not open %s: %s", dname, strerror(errno));
		free(dname);
		return;
	}

	while((ent = readdir(dir))) {
		if(!check_id(ent->d_name))
			continue;

		node_t *n = lookup_node(ent->d_name);
		if(n)
			continue;

		n = new_node();
		n->name = xstrdup(ent->d_name);
		node_add(n);
	}

	closedir(dir);
}


char *get_name(void) {
	char *name = NULL;

	get_config_string(lookup_config(config_tree, "Name"), &name);

	if(!name)
		return NULL;

	if(*name == '$') {
		char *envname = getenv(name + 1);
		char hostname[32] = "";
		if(!envname) {
			if(strcmp(name + 1, "HOST")) {
				logger(DEBUG_ALWAYS, LOG_ERR, "Invalid Name: environment variable %s does not exist\n", name + 1);
				return false;
			}
			if(gethostname(hostname, sizeof hostname) || !*hostname) {
				logger(DEBUG_ALWAYS, LOG_ERR, "Could not get hostname: %s\n", strerror(errno));
				return false;
			}
			hostname[31] = 0;
			envname = hostname;
		}
		free(name);
		name = xstrdup(envname);
		for(char *c = name; *c; c++)
			if(!isalnum(*c))
				*c = '_';
	}

	if(!check_id(name)) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Invalid name for myself!");
		free(name);
		return false;
	}

	return name;
}

bool setup_myself_reloadable(void) {
	char *proxy = NULL;
	char *rmode = NULL;
	char *fmode = NULL;
	char *bmode = NULL;
	char *afname = NULL;
	char *address = NULL;
	char *space;
	bool choice;

	get_config_string(lookup_config(config_tree, "Proxy"), &proxy);
	if(proxy) {
		if((space = strchr(proxy, ' ')))
			*space++ = 0;

		if(!strcasecmp(proxy, "none")) {
			proxytype = PROXY_NONE;
		} else if(!strcasecmp(proxy, "socks4")) {
			proxytype = PROXY_SOCKS4;
		} else if(!strcasecmp(proxy, "socks4a")) {
			proxytype = PROXY_SOCKS4A;
		} else if(!strcasecmp(proxy, "socks5")) {
			proxytype = PROXY_SOCKS5;
		} else if(!strcasecmp(proxy, "http")) {
			proxytype = PROXY_HTTP;
		} else if(!strcasecmp(proxy, "exec")) {
			proxytype = PROXY_EXEC;
		} else {
			logger(DEBUG_ALWAYS, LOG_ERR, "Unknown proxy type %s!", proxy);
			return false;
		}

		switch(proxytype) {
			case PROXY_NONE:
			default:
				break;

			case PROXY_EXEC:
				if(!space || !*space) {
					logger(DEBUG_ALWAYS, LOG_ERR, "Argument expected for proxy type exec!");
					return false;
				}
				proxyhost =  xstrdup(space);
				break;

			case PROXY_SOCKS4:
			case PROXY_SOCKS4A:
			case PROXY_SOCKS5:
			case PROXY_HTTP:
				proxyhost = space;
				if(space && (space = strchr(space, ' ')))
					*space++ = 0, proxyport = space;
				if(space && (space = strchr(space, ' ')))
					*space++ = 0, proxyuser = space;
				if(space && (space = strchr(space, ' ')))
					*space++ = 0, proxypass = space;
				if(!proxyhost || !*proxyhost || !proxyport || !*proxyport) {
					logger(DEBUG_ALWAYS, LOG_ERR, "Host and port argument expected for proxy!");
					return false;
				}
				proxyhost = xstrdup(proxyhost);
				proxyport = xstrdup(proxyport);
				if(proxyuser && *proxyuser)
					proxyuser = xstrdup(proxyuser);
				if(proxypass && *proxypass)
					proxypass = xstrdup(proxypass);
				break;
		}

		free(proxy);
	}

	if(get_config_bool(lookup_config(config_tree, "IndirectData"), &choice) && choice)
		myself->options |= OPTION_INDIRECT;

	if(get_config_bool(lookup_config(config_tree, "TCPOnly"), &choice) && choice)
		myself->options |= OPTION_TCPONLY;

	if(myself->options & OPTION_TCPONLY)
		myself->options |= OPTION_INDIRECT;

	get_config_bool(lookup_config(config_tree, "DirectOnly"), &directonly);
	get_config_bool(lookup_config(config_tree, "LocalDiscovery"), &localdiscovery);

	memset(&localdiscovery_address, 0, sizeof localdiscovery_address);
	if(get_config_string(lookup_config(config_tree, "LocalDiscoveryAddress"), &address)) {
		struct addrinfo *ai = str2addrinfo(address, myport, SOCK_DGRAM);
		free(address);
		if(!ai)
			return false;
		memcpy(&localdiscovery_address, ai->ai_addr, ai->ai_addrlen);
	}


	if(get_config_string(lookup_config(config_tree, "Mode"), &rmode)) {
		if(!strcasecmp(rmode, "router"))
			routing_mode = RMODE_ROUTER;
		else if(!strcasecmp(rmode, "switch"))
			routing_mode = RMODE_SWITCH;
		else if(!strcasecmp(rmode, "hub"))
			routing_mode = RMODE_HUB;
		else {
			logger(DEBUG_ALWAYS, LOG_ERR, "Invalid routing mode!");
			return false;
		}
		free(rmode);
	}

	if(get_config_string(lookup_config(config_tree, "Forwarding"), &fmode)) {
		if(!strcasecmp(fmode, "off"))
			forwarding_mode = FMODE_OFF;
		else if(!strcasecmp(fmode, "internal"))
			forwarding_mode = FMODE_INTERNAL;
		else if(!strcasecmp(fmode, "kernel"))
			forwarding_mode = FMODE_KERNEL;
		else {
			logger(DEBUG_ALWAYS, LOG_ERR, "Invalid forwarding mode!");
			return false;
		}
		free(fmode);
	}

	choice = true;
	get_config_bool(lookup_config(config_tree, "PMTUDiscovery"), &choice);
	if(choice)
		myself->options |= OPTION_PMTU_DISCOVERY;

	choice = true;
	get_config_bool(lookup_config(config_tree, "ClampMSS"), &choice);
	if(choice)
		myself->options |= OPTION_CLAMP_MSS;

	get_config_bool(lookup_config(config_tree, "PriorityInheritance"), &priorityinheritance);
	get_config_bool(lookup_config(config_tree, "DecrementTTL"), &decrement_ttl);
	if(get_config_string(lookup_config(config_tree, "Broadcast"), &bmode)) {
		if(!strcasecmp(bmode, "no"))
			broadcast_mode = BMODE_NONE;
		else if(!strcasecmp(bmode, "yes") || !strcasecmp(bmode, "mst"))
			broadcast_mode = BMODE_MST;
		else if(!strcasecmp(bmode, "direct"))
			broadcast_mode = BMODE_DIRECT;
		else {
			logger(DEBUG_ALWAYS, LOG_ERR, "Invalid broadcast mode!");
			return false;
		}
		free(bmode);
	}

#if !defined(SOL_IP) || !defined(IP_TOS)
	if(priorityinheritance)
		logger(DEBUG_ALWAYS, LOG_WARNING, "%s not supported on this platform", "PriorityInheritance");
#endif

	if(!get_config_int(lookup_config(config_tree, "MACExpire"), &macexpire))
		macexpire = 600;

	if(get_config_int(lookup_config(config_tree, "MaxTimeout"), &maxtimeout)) {
		if(maxtimeout <= 0) {
			logger(DEBUG_ALWAYS, LOG_ERR, "Bogus maximum timeout!");
			return false;
		}
	} else
		maxtimeout = 900;

	if(get_config_string(lookup_config(config_tree, "AddressFamily"), &afname)) {
		if(!strcasecmp(afname, "IPv4"))
			addressfamily = AF_INET;
		else if(!strcasecmp(afname, "IPv6"))
			addressfamily = AF_INET6;
		else if(!strcasecmp(afname, "any"))
			addressfamily = AF_UNSPEC;
		else {
			logger(DEBUG_ALWAYS, LOG_ERR, "Invalid address family!");
			return false;
		}
		free(afname);
	}

	get_config_bool(lookup_config(config_tree, "Hostnames"), &hostnames);

	if(!get_config_int(lookup_config(config_tree, "KeyExpire"), &keylifetime))
		keylifetime = 3600;

	get_config_int(lookup_config(config_tree, "AutoConnect"), &autoconnect);
	if(autoconnect < 0)
		autoconnect = 0;

	get_config_bool(lookup_config(config_tree, "DisableBuggyPeers"), &disablebuggypeers);

	read_invitation_key();

	return true;
}

/*
  Add listening sockets.
*/
static bool add_listen_address(char *address, bool bindto) {
	char *port = myport;

	if(address) {
		char *space = strchr(address, ' ');
		if(space) {
			*space++ = 0;
			port = space;
		}

		if(!strcmp(address, "*"))
			*address = 0;
	}

	struct addrinfo *ai, hint = {0};
	hint.ai_family = addressfamily;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = IPPROTO_TCP;
	hint.ai_flags = AI_PASSIVE;

	int err = getaddrinfo(address && *address ? address : NULL, port, &hint, &ai);
	free(address);

	if(err || !ai) {
		logger(DEBUG_ALWAYS, LOG_ERR, "System call `%s' failed: %s", "getaddrinfo", err == EAI_SYSTEM ? strerror(err) : gai_strerror(err));
		return false;
	}

	for(struct addrinfo *aip = ai; aip; aip = aip->ai_next) {
		// Ignore duplicate addresses
		bool found = false;

		for(int i = 0; i < listen_sockets; i++)
			if(!memcmp(&listen_socket[i].sa, aip->ai_addr, aip->ai_addrlen)) {
				found = true;
				break;
			}

		if(found)
			continue;

		if(listen_sockets >= MAXSOCKETS) {
			logger(DEBUG_ALWAYS, LOG_ERR, "Too many listening sockets");
			return false;
		}

		int tcp_fd = setup_listen_socket((sockaddr_t *) aip->ai_addr);

		if(tcp_fd < 0)
			continue;

		int udp_fd = setup_vpn_in_socket((sockaddr_t *) aip->ai_addr);

		if(tcp_fd < 0) {
			close(tcp_fd);
			continue;
		}

		io_add(&listen_socket[listen_sockets].tcp, handle_new_meta_connection, &listen_socket[listen_sockets], tcp_fd, IO_READ);
		io_add(&listen_socket[listen_sockets].udp, handle_incoming_vpn_data, &listen_socket[listen_sockets], udp_fd, IO_READ);

		if(debug_level >= DEBUG_CONNECTIONS) {
			char *hostname = sockaddr2hostname((sockaddr_t *) aip->ai_addr);
			logger(DEBUG_CONNECTIONS, LOG_NOTICE, "Listening on %s", hostname);
			free(hostname);
		}

		listen_socket[listen_sockets].bindto = bindto;
		memcpy(&listen_socket[listen_sockets].sa, aip->ai_addr, aip->ai_addrlen);
		listen_sockets++;
	}

	freeaddrinfo(ai);
	return true;
}

/*
  Configure node_t myself and set up the local sockets (listen only)
*/
bool setup_myself(void) {
	char *name, *hostname, *cipher, *digest, *type;
	char *address = NULL;
	bool port_specified = false;

	if(!(name = get_name())) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Name for tinc daemon required!");
		return false;
	}

	myself = new_node();
	myself->connection = new_connection();
	myself->name = name;
	myself->connection->name = xstrdup(name);
	read_host_config(config_tree, name);

	if(!get_config_string(lookup_config(config_tree, "Port"), &myport))
		myport = xstrdup("655");
	else
		port_specified = true;

	myself->connection->options = 0;
	myself->connection->protocol_major = PROT_MAJOR;
	myself->connection->protocol_minor = PROT_MINOR;

	myself->options |= PROT_MINOR << 24;

	if(!read_ecdsa_private_key())
		return false;

	/* Ensure myport is numeric */

	if(!atoi(myport)) {
		struct addrinfo *ai = str2addrinfo("localhost", myport, SOCK_DGRAM);
		sockaddr_t sa;
		if(!ai || !ai->ai_addr)
			return false;
		free(myport);
		memcpy(&sa, ai->ai_addr, ai->ai_addrlen);
		sockaddr2str(&sa, NULL, &myport);
	}

	/* Check some options */

	if(!setup_myself_reloadable())
		return false;

	if(get_config_int(lookup_config(config_tree, "MaxConnectionBurst"), &max_connection_burst)) {
		if(max_connection_burst <= 0) {
			logger(DEBUG_ALWAYS, LOG_ERR, "MaxConnectionBurst cannot be negative!");
			return false;
		}
	}

	int replaywin_int;
	if(get_config_int(lookup_config(config_tree, "ReplayWindow"), &replaywin_int)) {
		if(replaywin_int < 0) {
			logger(DEBUG_ALWAYS, LOG_ERR, "ReplayWindow cannot be negative!");
			return false;
		}
		replaywin = (unsigned)replaywin_int;
		sptps_replaywin = replaywin;
	}

	timeout_add(&keyexpire_timeout, keyexpire_handler, &keyexpire_timeout, &(struct timeval){keylifetime, rand() % 100000});

	/* Compression */

	if(get_config_int(lookup_config(config_tree, "Compression"), &myself->incompression)) {
		if(myself->incompression < 0 || myself->incompression > 11) {
			logger(DEBUG_ALWAYS, LOG_ERR, "Bogus compression level!");
			return false;
		}
	} else
		myself->incompression = 0;

	myself->connection->outcompression = 0;

	/* Done */

	myself->nexthop = myself;
	myself->via = myself;
	myself->status.reachable = true;
	myself->last_state_change = now.tv_sec;
	node_add(myself);

	graph();

	if(autoconnect)
		load_all_nodes();

	/* Open sockets */

	listen_sockets = 0;
	int cfgs = 0;

	for(config_t *cfg = lookup_config(config_tree, "BindToAddress"); cfg; cfg = lookup_config_next(config_tree, cfg)) {
		cfgs++;
		get_config_string(cfg, &address);
		if(!add_listen_address(address, true))
			return false;
	}

	for(config_t *cfg = lookup_config(config_tree, "ListenAddress"); cfg; cfg = lookup_config_next(config_tree, cfg)) {
		cfgs++;
		get_config_string(cfg, &address);
		if(!add_listen_address(address, false))
			return false;
	}

	if(!cfgs)
		if(!add_listen_address(address, NULL))
			return false;

	if(!listen_sockets) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Unable to create any listening socket!");
		return false;
	}

	/* If no Port option was specified, set myport to the port used by the first listening socket. */

	if(!port_specified) {
		sockaddr_t sa;
		socklen_t salen = sizeof sa;
		if(!getsockname(listen_socket[0].udp.fd, &sa.sa, &salen)) {
			free(myport);
			sockaddr2str(&sa, NULL, &myport);
			if(!myport)
				myport = xstrdup("655");
		}
	}

	xasprintf(&myself->hostname, "MYSELF port %s", myport);
	myself->connection->hostname = xstrdup(myself->hostname);

	/* Done. */

	last_config_check = now.tv_sec;

	return true;
}

/*
  initialize network
*/
bool setup_network(void) {
	init_connections();
	init_nodes();
	init_edges();
	init_requests();

	if(get_config_int(lookup_config(config_tree, "PingInterval"), &pinginterval)) {
		if(pinginterval < 1) {
			pinginterval = 86400;
		}
	} else
		pinginterval = 60;

	if(!get_config_int(lookup_config(config_tree, "PingTimeout"), &pingtimeout))
		pingtimeout = 5;
	if(pingtimeout < 1 || pingtimeout > pinginterval)
		pingtimeout = pinginterval;

	if(!get_config_int(lookup_config(config_tree, "MaxOutputBufferSize"), &maxoutbufsize))
		maxoutbufsize = 10 * MTU;

	if(!setup_myself())
		return false;

	return true;
}

/*
  close all open network connections
*/
void close_network_connections(void) {
	for(list_node_t *node = connection_list->head, *next; node; node = next) {
		next = node->next;
		connection_t *c = node->data;
		c->outgoing = NULL;
		terminate_connection(c, false);
	}

	if(outgoing_list)
		list_delete_list(outgoing_list);

	if(myself && myself->connection) {
		terminate_connection(myself->connection, false);
		free_connection(myself->connection);
	}

	for(int i = 0; i < listen_sockets; i++) {
		io_del(&listen_socket[i].tcp);
		io_del(&listen_socket[i].udp);
		close(listen_socket[i].tcp.fd);
		close(listen_socket[i].udp.fd);
	}

	exit_requests();
	exit_edges();
	exit_nodes();
	exit_connections();

	if(myport) free(myport);

	return;
}
