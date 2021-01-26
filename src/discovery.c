#include "system.h"

#include <catta/core.h>
#include <catta/lookup.h>
#include <catta/publish.h>
#include <catta/log.h>
#include <catta/simple-watch.h>
#include <catta/malloc.h>
#include <catta/alternative.h>
#include <catta/error.h>

#if defined(__APPLE__) || defined(__unix) && !defined(__linux)
#include <net/route.h>
#elif defined(__linux)
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

#include "meshlink_internal.h"
#include "event.h"
#include "discovery.h"
#include "sockaddr.h"
#include "logger.h"
#include "node.h"
#include "connection.h"
#include "xalloc.h"

#define MESHLINK_MDNS_SERVICE_TYPE "_%s._tcp"
#define MESHLINK_MDNS_NAME_KEY "name"
#define MESHLINK_MDNS_FINGERPRINT_KEY "fingerprint"

static void generate_rand_string(meshlink_handle_t *mesh, char *buffer, size_t size) {
	assert(size);

	for(size_t i = 0; i < (size - 1); ++i) {
		buffer[i] = 'a' + prng(mesh, 'z' - 'a' + 1);
	}

	buffer[size - 1] = '\0';
}

static void discovery_entry_group_callback(CattaServer *server, CattaSEntryGroup *group, CattaEntryGroupState state, void *userdata) {
	(void)server;
	(void)group;
	meshlink_handle_t *mesh = userdata;

	assert(mesh);
	assert(mesh->catta_server);
	assert(mesh->catta_poll);

	/* Called whenever the entry group state changes */
	switch(state) {
	case CATTA_ENTRY_GROUP_ESTABLISHED:
		/* The entry group has been established successfully */
		logger(mesh, MESHLINK_DEBUG, "Catta Service successfully established.\n");
		break;

	case CATTA_ENTRY_GROUP_COLLISION:
		logger(mesh, MESHLINK_WARNING, "Catta Service collision.\n");
		// @TODO can we just set a new name and retry?
		break;

	case CATTA_ENTRY_GROUP_FAILURE :
		/* Some kind of failure happened while we were registering our services */
		logger(mesh, MESHLINK_ERROR, "Catta Entry group failure: %s\n", catta_strerror(catta_server_errno(mesh->catta_server)));
		catta_simple_poll_quit(mesh->catta_poll);
		break;

	case CATTA_ENTRY_GROUP_UNCOMMITED:
	case CATTA_ENTRY_GROUP_REGISTERING:
		break;
	}
}


static void discovery_create_services(meshlink_handle_t *mesh) {
	char *fingerprint = NULL;
	char *txt_name = NULL;
	char *txt_fingerprint = NULL;

	assert(mesh);
	assert(mesh->name);
	assert(mesh->myport);
	assert(mesh->catta_server);
	assert(mesh->catta_poll);
	assert(mesh->catta_servicetype);
	assert(mesh->self);

	logger(mesh, MESHLINK_DEBUG, "Adding service\n");

	/* Ifthis is the first time we're called, let's create a new entry group */
	if(!(mesh->catta_group = catta_s_entry_group_new(mesh->catta_server, discovery_entry_group_callback, mesh))) {
		logger(mesh, MESHLINK_ERROR, "catta_entry_group_new() failed: %s\n", catta_strerror(catta_server_errno(mesh->catta_server)));
		goto fail;
	}

	/* Create txt records */
	fingerprint = meshlink_get_fingerprint(mesh, (meshlink_node_t *)mesh->self);
	xasprintf(&txt_name, "%s=%s", MESHLINK_MDNS_NAME_KEY, mesh->name);
	xasprintf(&txt_fingerprint, "%s=%s", MESHLINK_MDNS_FINGERPRINT_KEY, fingerprint);

	/* Add the service */
	int ret = 0;

	if((ret = catta_server_add_service(mesh->catta_server, mesh->catta_group, CATTA_IF_UNSPEC, CATTA_PROTO_UNSPEC, 0, fingerprint, mesh->catta_servicetype, NULL, NULL, atoi(mesh->myport), txt_name, txt_fingerprint, NULL)) < 0) {
		logger(mesh, MESHLINK_ERROR, "Failed to add service: %s\n", catta_strerror(ret));
		goto fail;
	}

	/* Tell the server to register the service */
	if((ret = catta_s_entry_group_commit(mesh->catta_group)) < 0) {
		logger(mesh, MESHLINK_ERROR, "Failed to commit entry_group: %s\n", catta_strerror(ret));
		goto fail;
	}

	goto done;

fail:
	catta_simple_poll_quit(mesh->catta_poll);

done:
	free(fingerprint);
	free(txt_name);
	free(txt_fingerprint);
}

static void discovery_server_callback(CattaServer *server, CattaServerState state, void *userdata) {
	(void)server;
	meshlink_handle_t *mesh = userdata;

	assert(mesh);

	switch(state) {
	case CATTA_SERVER_RUNNING:

		/* The serve has startup successfully and registered its host
		 * name on the network, so it's time to create our services */
		if(pthread_mutex_lock(&mesh->mutex) != 0) {
			abort();
		}

		if(!mesh->catta_group) {
			discovery_create_services(mesh);
		}

		pthread_mutex_unlock(&mesh->mutex);

		break;

	case CATTA_SERVER_COLLISION: {
		/* A host name collision happened. Let's pick a new name for the server */
		char hostname[17];
		generate_rand_string(mesh, hostname, sizeof(hostname));

		if(pthread_mutex_lock(&mesh->mutex) != 0) {
			abort();
		}

		assert(mesh->catta_server);
		assert(mesh->catta_poll);

		int result = catta_server_set_host_name(mesh->catta_server, hostname);

		if(result < 0) {
			catta_simple_poll_quit(mesh->catta_poll);
		}

		pthread_mutex_unlock(&mesh->mutex);
	}
	break;

	case CATTA_SERVER_REGISTERING:
		if(pthread_mutex_lock(&mesh->mutex) != 0) {
			abort();
		}

		/* Let's drop our registered services. When the server is back
		 * in CATTA_SERVER_RUNNING state we will register them
		 * again with the new host name. */
		if(mesh->catta_group) {
			catta_s_entry_group_reset(mesh->catta_group);
			mesh->catta_group = NULL;
		}

		pthread_mutex_unlock(&mesh->mutex);

		break;

	case CATTA_SERVER_FAILURE:
		if(pthread_mutex_lock(&mesh->mutex) != 0) {
			abort();
		}

		assert(mesh->catta_server);
		assert(mesh->catta_poll);

		/* Terminate on failure */
		catta_simple_poll_quit(mesh->catta_poll);

		pthread_mutex_unlock(&mesh->mutex);
		break;

	case CATTA_SERVER_INVALID:
		break;
	}
}

static void discovery_resolve_callback(CattaSServiceResolver *resolver, CattaIfIndex interface_, CattaProtocol protocol, CattaResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const CattaAddress *address, uint16_t port, CattaStringList *txt, CattaLookupResultFlags flags, void *userdata) {
	(void)interface_;
	(void)protocol;
	(void)flags;
	(void)name;
	(void)type;
	(void)domain;
	(void)host_name;

	meshlink_handle_t *mesh = userdata;

	assert(mesh);

	if(event != CATTA_RESOLVER_FOUND) {
		catta_s_service_resolver_free(resolver);
		return;
	}

	// retrieve fingerprint
	CattaStringList *node_name_li = catta_string_list_find(txt, MESHLINK_MDNS_NAME_KEY);
	CattaStringList *node_fp_li = catta_string_list_find(txt, MESHLINK_MDNS_FINGERPRINT_KEY);

	if(node_name_li && node_fp_li) {
		char *node_name = (char *)catta_string_list_get_text(node_name_li) + strlen(MESHLINK_MDNS_NAME_KEY);
		char *node_fp = (char *)catta_string_list_get_text(node_fp_li) + strlen(MESHLINK_MDNS_FINGERPRINT_KEY);

		if(node_name[0] == '=' && node_fp[0] == '=') {
			if(pthread_mutex_lock(&mesh->mutex) != 0) {
				abort();
			}

			node_name += 1;

			meshlink_node_t *node = meshlink_get_node(mesh, node_name);

			if(node) {
				logger(mesh, MESHLINK_INFO, "Node %s is part of the mesh network.\n", node->name);

				sockaddr_t naddress;
				memset(&naddress, 0, sizeof(naddress));

				switch(address->proto) {
				case CATTA_PROTO_INET: {
					naddress.in.sin_family = AF_INET;
					naddress.in.sin_port = htons(port);
					naddress.in.sin_addr.s_addr = address->data.ipv4.address;
				}
				break;

				case CATTA_PROTO_INET6: {
					naddress.in6.sin6_family = AF_INET6;
					naddress.in6.sin6_port = htons(port);
					memcpy(naddress.in6.sin6_addr.s6_addr, address->data.ipv6.address, sizeof(naddress.in6.sin6_addr.s6_addr));
				}
				break;

				default:
					naddress.unknown.family = AF_UNKNOWN;
					break;
				}

				if(naddress.unknown.family != AF_UNKNOWN) {
					node_t *n = (node_t *)node;
					connection_t *c = n->connection;

					n->catta_address = naddress;
					node_add_recent_address(mesh, n, &naddress);

					if(c && c->outgoing && !c->status.active) {
						c->outgoing->timeout = 0;

						if(c->outgoing->ev.cb) {
							timeout_set(&mesh->loop, &c->outgoing->ev, &(struct timespec) {
								0, 0
							});
						}

						c->last_ping_time = -3600;
					}

				} else {
					logger(mesh, MESHLINK_WARNING, "Could not resolve node %s to a known address family type.\n", node->name);
				}
			} else {
				logger(mesh, MESHLINK_WARNING, "Node %s is not part of the mesh network.\n", node_name);
			}

			pthread_mutex_unlock(&mesh->mutex);
		}
	}

	catta_s_service_resolver_free(resolver);
}

static void discovery_browse_callback(CattaSServiceBrowser *browser, CattaIfIndex interface_, CattaProtocol protocol, CattaBrowserEvent event, const char *name, const char *type, const char *domain, CattaLookupResultFlags flags, void *userdata) {
	(void)browser;
	(void)flags;
	meshlink_handle_t *mesh = userdata;

	/* Called whenever a new services becomes available on the LAN or is removed from the LAN */
	switch(event) {
	case CATTA_BROWSER_FAILURE:
		if(pthread_mutex_lock(&mesh->mutex) != 0) {
			abort();
		}

		catta_simple_poll_quit(mesh->catta_poll);
		pthread_mutex_unlock(&mesh->mutex);
		break;

	case CATTA_BROWSER_NEW:
		if(pthread_mutex_lock(&mesh->mutex) != 0) {
			abort();
		}

		catta_s_service_resolver_new(mesh->catta_server, interface_, protocol, name, type, domain, CATTA_PROTO_UNSPEC, 0, discovery_resolve_callback, mesh);
		handle_network_change(mesh, ++mesh->catta_interfaces);
		pthread_mutex_unlock(&mesh->mutex);
		break;

	case CATTA_BROWSER_REMOVE:
		if(pthread_mutex_lock(&mesh->mutex) != 0) {
			abort();
		}

		handle_network_change(mesh, --mesh->catta_interfaces);
		pthread_mutex_unlock(&mesh->mutex);
		break;

	case CATTA_BROWSER_ALL_FOR_NOW:
	case CATTA_BROWSER_CACHE_EXHAUSTED:
		break;
	}
}

static void discovery_log_cb(CattaLogLevel level, const char *txt) {
	meshlink_log_level_t mlevel = MESHLINK_CRITICAL;

	switch(level) {
	case CATTA_LOG_ERROR:
		mlevel = MESHLINK_ERROR;
		break;

	case CATTA_LOG_WARN:
		mlevel = MESHLINK_WARNING;
		break;

	case CATTA_LOG_NOTICE:
	case CATTA_LOG_INFO:
		mlevel = MESHLINK_INFO;
		break;

	case CATTA_LOG_DEBUG:
	default:
		mlevel = MESHLINK_DEBUG;
		break;
	}

	logger(NULL, mlevel, "%s\n", txt);
}

static void *discovery_loop(void *userdata) {
	bool status = false;
	meshlink_handle_t *mesh = userdata;
	assert(mesh);

	if(pthread_mutex_lock(&mesh->discovery_mutex) != 0) {
		abort();
	}

	// handle catta logs
	catta_set_log_function(discovery_log_cb);

	// create service type string
	char appname[strlen(mesh->appname) + 2];
	strcpy(appname, mesh->appname);

	for(char *p = appname; *p; p++) {
		if(!isalnum(*p) && *p != '_' && *p != '-') {
			*p = '_';
		}
	}

	if(!appname[1]) {
		appname[1] = '_';
		appname[2] = '\0';
	}

	size_t servicetype_strlen = sizeof(MESHLINK_MDNS_SERVICE_TYPE) + strlen(appname) + 1;
	mesh->catta_servicetype = malloc(servicetype_strlen);

	if(mesh->catta_servicetype == NULL) {
		logger(mesh, MESHLINK_ERROR, "Failed to allocate memory for service type string.\n");
		goto fail;
	}

	snprintf(mesh->catta_servicetype, servicetype_strlen, MESHLINK_MDNS_SERVICE_TYPE, appname);

	// Allocate discovery loop object
	if(!(mesh->catta_poll = catta_simple_poll_new())) {
		logger(mesh, MESHLINK_ERROR, "Failed to create discovery poll object.\n");
		goto fail;
	}

	// generate some unique host name (we actually do not care about it)
	char hostname[17];
	generate_rand_string(mesh, hostname, sizeof(hostname));

	// Let's set the host name for this server.
	CattaServerConfig config;
	catta_server_config_init(&config);
	config.host_name = catta_strdup(hostname);
	config.publish_workstation = 0;
	config.disallow_other_stacks = 0;
	config.publish_hinfo = 0;
	config.publish_addresses = 1;
	config.publish_no_reverse = 1;
	config.allow_point_to_point = 1;

	/* Allocate a new server */
	int error;
	const CattaPoll *poller = catta_simple_poll_get(mesh->catta_poll);

	if(!poller) {
		logger(mesh, MESHLINK_ERROR, "Failed to create discovery server: %s\n", catta_strerror(error));
		goto fail;
	}

	mesh->catta_server = catta_server_new(poller, &config, discovery_server_callback, mesh, &error);

	/* Free the configuration data */
	catta_server_config_free(&config);

	/* Check whether creating the server object succeeded */
	if(!mesh->catta_server) {
		logger(mesh, MESHLINK_ERROR, "Failed to create discovery server: %s\n", catta_strerror(error));
		goto fail;
	}

	// Create the service browser
	if(!(mesh->catta_browser = catta_s_service_browser_new(mesh->catta_server, CATTA_IF_UNSPEC, CATTA_PROTO_UNSPEC, mesh->catta_servicetype, NULL, 0, discovery_browse_callback, mesh))) {
		logger(mesh, MESHLINK_ERROR, "Failed to create discovery service browser: %s\n", catta_strerror(catta_server_errno(mesh->catta_server)));
		goto fail;
	}

	status = true;

fail:

	pthread_cond_broadcast(&mesh->discovery_cond);
	pthread_mutex_unlock(&mesh->discovery_mutex);

	if(status) {
		catta_simple_poll_loop(mesh->catta_poll);
	}

	if(mesh->catta_browser) {
		catta_s_service_browser_free(mesh->catta_browser);
		mesh->catta_browser = NULL;
	}

	if(mesh->catta_group) {
		catta_s_entry_group_reset(mesh->catta_group);
		catta_s_entry_group_free(mesh->catta_group);
		mesh->catta_group = NULL;
	}

	if(mesh->catta_server) {
		catta_server_free(mesh->catta_server);
		mesh->catta_server = NULL;
	}

	if(mesh->catta_poll) {
		catta_simple_poll_free(mesh->catta_poll);
		mesh->catta_poll = NULL;
	}

	if(mesh->catta_servicetype) {
		free(mesh->catta_servicetype);
		mesh->catta_servicetype = NULL;
	}

	return NULL;
}

#if defined(__linux)
static void netlink_io_handler(event_loop_t *loop, void *data, int flags) {
	(void)flags;
	static time_t prev_update;
	meshlink_handle_t *mesh = data;

	struct {
		struct nlmsghdr nlm;
		char data[2048];
	} msg;

	while(true) {
		ssize_t result = recv(mesh->pfroute_io.fd, &msg, sizeof(msg), MSG_DONTWAIT);

		if(result <= 0) {
			if(result == 0 || errno == EAGAIN || errno == EINTR) {
				break;
			}

			logger(mesh, MESHLINK_ERROR, "Reading from Netlink socket failed: %s\n", strerror(errno));
			io_set(loop, &mesh->pfroute_io, 0);
		}

		if((size_t)result < sizeof(msg.nlm)) {
			logger(mesh, MESHLINK_ERROR, "Invalid Netlink message\n");
			break;
		}

		switch(msg.nlm.nlmsg_type) {
		case RTM_NEWLINK:
		case RTM_DELLINK:
		case RTM_NEWADDR:
		case RTM_DELADDR:
			if(loop->now.tv_sec > prev_update + 5) {
				prev_update = loop->now.tv_sec;
				handle_network_change(mesh, 1);
			}

			break;

		default:
			break;
		}
	}
}
#elif defined(RTM_NEWADDR)
static void pfroute_io_handler(event_loop_t *loop, void *data, int flags) {
	(void)flags;
	static time_t prev_update;
	meshlink_handle_t *mesh = data;

	struct {
		struct rt_msghdr rtm;
		char data[2048];
	} msg;

	while(true) {
		msg.rtm.rtm_version = 0;
		ssize_t result = recv(mesh->pfroute_io.fd, &msg, sizeof(msg), MSG_DONTWAIT);

		if(result <= 0) {
			if(result == 0 || errno == EAGAIN || errno == EINTR) {
				break;
			}

			logger(mesh, MESHLINK_ERROR, "Reading from PFROUTE socket failed: %s\n", strerror(errno));
			io_set(loop, &mesh->pfroute_io, 0);
		}

		if(msg.rtm.rtm_version != RTM_VERSION) {
			logger(mesh, MESHLINK_ERROR, "Invalid PFROUTE message version\n");
			break;
		}

		switch(msg.rtm.rtm_type) {
		case RTM_IFINFO:
		case RTM_NEWADDR:
		case RTM_DELADDR:
			if(loop->now.tv_sec > prev_update + 5) {
				prev_update = loop->now.tv_sec;
				handle_network_change(mesh, 1);
			}

			break;

		default:
			break;
		}
	}
}
#endif

bool discovery_start(meshlink_handle_t *mesh) {
	logger(mesh, MESHLINK_DEBUG, "discovery_start called\n");

	assert(mesh);
	assert(!mesh->catta_poll);
	assert(!mesh->catta_server);
	assert(!mesh->catta_browser);
	assert(!mesh->discovery_threadstarted);
	assert(!mesh->catta_servicetype);

	if(pthread_mutex_lock(&mesh->discovery_mutex) != 0) {
		abort();
	}

	// Start the discovery thread
	if(pthread_create(&mesh->discovery_thread, NULL, discovery_loop, mesh) != 0) {
		pthread_mutex_unlock(&mesh->discovery_mutex);
		logger(mesh, MESHLINK_ERROR, "Could not start discovery thread: %s\n", strerror(errno));
		memset(&mesh->discovery_thread, 0, sizeof(mesh)->discovery_thread);
		return false;
	}

	pthread_cond_wait(&mesh->discovery_cond, &mesh->discovery_mutex);
	pthread_mutex_unlock(&mesh->discovery_mutex);

	mesh->discovery_threadstarted = true;

#if defined(__linux)
	int sock = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);

	if(sock != -1) {
		struct sockaddr_nl sa;
		memset(&sa, 0, sizeof(sa));
		sa.nl_family = AF_NETLINK;
		sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;

		if(bind(sock, (struct sockaddr *)&sa, sizeof(sa)) != -1) {
			io_add(&mesh->loop, &mesh->pfroute_io, netlink_io_handler, mesh, sock, IO_READ);
		} else {
			logger(mesh, MESHLINK_WARNING, "Could not bind AF_NETLINK socket: %s", strerror(errno));
		}
	} else {
		logger(mesh, MESHLINK_WARNING, "Could not open AF_NETLINK socket: %s", strerror(errno));
	}

#elif defined(RTM_NEWADDR)
	int sock = socket(PF_ROUTE, SOCK_RAW, AF_UNSPEC);

	if(sock != -1) {
		io_add(&mesh->loop, &mesh->pfroute_io, pfroute_io_handler, mesh, sock, IO_READ);
	} else {
		logger(mesh, MESHLINK_WARNING, "Could not open PF_ROUTE socket: %s", strerror(errno));
	}

#endif

	return true;
}

void discovery_stop(meshlink_handle_t *mesh) {
	logger(mesh, MESHLINK_DEBUG, "discovery_stop called\n");

	assert(mesh);

	if(mesh->pfroute_io.cb) {
		close(mesh->pfroute_io.fd);
		io_del(&mesh->loop, &mesh->pfroute_io);
	}

	// Shut down
	if(mesh->catta_poll) {
		catta_simple_poll_quit(mesh->catta_poll);
	}

	// Wait for the discovery thread to finish
	if(mesh->discovery_threadstarted == true) {
		if(pthread_join(mesh->discovery_thread, NULL) != 0) {
			abort();
		}

		mesh->discovery_threadstarted = false;
	}

	mesh->catta_interfaces = 0;
}
