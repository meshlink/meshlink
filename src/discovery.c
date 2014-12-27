#include <catta/core.h>
#include <catta/lookup.h>
#include <catta/publish.h>
#include <catta/log.h>
#include <catta/simple-watch.h>
#include <catta/malloc.h>
#include <catta/alternative.h>
#include <catta/error.h>

#include "meshlink_internal.h"
#include "discovery.h"
#include "sockaddr.h"
#include "logger.h"

#include <pthread.h>

#include <netinet/in.h>

#define MESHLINK_MDNS_SERVICE_TYPE "_%s._tcp"
#define MESHLINK_MDNS_NAME_KEY "name"
#define MESHLINK_MDNS_FINGERPRINT_KEY "fingerprint"

static void generate_rand_string(char* buffer, size_t size)
{
    for(size_t i = 0; i < (size - 1); ++i)
    {
        buffer[i] = 'a' + (rand() % ('z' - 'a' + 1));
    }

    buffer[size-1] = '\0';
}

static void discovery_entry_group_callback(CattaServer *server, CattaSEntryGroup *group, CattaEntryGroupState state, void *userdata)
{
    meshlink_handle_t *mesh = userdata;

    // asserts
    assert(mesh != NULL);
    assert(mesh->catta_server != NULL);
    assert(mesh->catta_poll != NULL);

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    /* Called whenever the entry group state changes */
    switch(state)
    {
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
            ;
    }

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
}


static void discovery_create_services(meshlink_handle_t *mesh)
{
    char *txt_name = NULL;

    // asserts
    assert(mesh != NULL);
    assert(mesh->name != NULL);
    assert(mesh->myport != NULL);
    assert(mesh->catta_server != NULL);
    assert(mesh->catta_poll != NULL);
    assert(mesh->catta_servicetype != NULL);
    assert(mesh->self != NULL);

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    logger(mesh, MESHLINK_DEBUG, "Adding service\n");

    /* Ifthis is the first time we're called, let's create a new entry group */
    if(!mesh->catta_group)
    {
        if(!(mesh->catta_group = catta_s_entry_group_new(mesh->catta_server, discovery_entry_group_callback, mesh)))
        {
            logger(mesh, MESHLINK_ERROR, "catta_entry_group_new() failed: %s\n", catta_strerror(catta_server_errno(mesh->catta_server)));
            goto fail;
        }
    }

    /* Create txt records */
    size_t txt_name_len = sizeof(MESHLINK_MDNS_NAME_KEY) + 1 + strlen(mesh->name) + 1;
    txt_name = malloc(txt_name_len);

    if(txt_name == NULL)
    {
        logger(mesh, MESHLINK_ERROR, "Could not allocate memory for TXT record\n");
        goto fail;
    }

    snprintf(txt_name, txt_name_len, "%s=%s", MESHLINK_MDNS_NAME_KEY, mesh->name);

    char txt_fingerprint[sizeof(MESHLINK_MDNS_FINGERPRINT_KEY) + 1 + MESHLINK_FINGERPRINTLEN + 1];
    snprintf(txt_fingerprint, sizeof(txt_fingerprint), "%s=%s", MESHLINK_MDNS_FINGERPRINT_KEY, meshlink_get_fingerprint(mesh, (meshlink_node_t *)mesh->self));

    /* Add the service */
    int ret = 0;
    if((ret = catta_server_add_service(mesh->catta_server, mesh->catta_group, CATTA_IF_UNSPEC, CATTA_PROTO_UNSPEC, 0, meshlink_get_fingerprint(mesh, (meshlink_node_t *)mesh->self), mesh->catta_servicetype, NULL, NULL, atoi(mesh->myport), txt_name, txt_fingerprint, NULL)) < 0)
    {
        logger(mesh, MESHLINK_ERROR, "Failed to add service: %s\n", catta_strerror(ret));
        goto fail;
    }

    /* Tell the server to register the service */
    if((ret = catta_s_entry_group_commit(mesh->catta_group)) < 0)
    {
        logger(mesh, MESHLINK_ERROR, "Failed to commit entry_group: %s\n", catta_strerror(ret));
        goto fail;
    }

    goto done;

fail:
    catta_simple_poll_quit(mesh->catta_poll);

done:
    if(txt_name)
        { free(txt_name); }

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
}

static void discovery_server_callback(CattaServer *server, CattaServerState state, void * userdata)
{
	meshlink_handle_t *mesh = userdata;

    // asserts
    assert(mesh != NULL);
    
    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    switch(state)
    {
        case CATTA_SERVER_RUNNING:
            {
                /* The serve has startup successfully and registered its host
                 * name on the network, so it's time to create our services */
                if(!mesh->catta_group)
                {
                    discovery_create_services(mesh);
                }
            }
            break;

        case CATTA_SERVER_COLLISION:
            {
                // asserts
                assert(mesh->catta_server != NULL);
                assert(mesh->catta_poll != NULL);

                /* A host name collision happened. Let's pick a new name for the server */
                char hostname[17];
                generate_rand_string(hostname, sizeof(hostname));

                logger(mesh, MESHLINK_WARNING, "Catta host name collision, retrying with '%s'\n", hostname);
                int result = catta_server_set_host_name(mesh->catta_server, hostname);

                if(result < 0)
                {
                    logger(mesh, MESHLINK_ERROR, "Catta failed to set new host name: %s\n", catta_strerror(result));
                    catta_simple_poll_quit(mesh->catta_poll);
                }
            }
            break;

        case CATTA_SERVER_REGISTERING:
            {
                /* Let's drop our registered services. When the server is back
                 * in CATTA_SERVER_RUNNING state we will register them
                 * again with the new host name. */
                if(mesh->catta_group)
                {
                    catta_s_entry_group_reset(mesh->catta_group);
                    mesh->catta_group = NULL;
                }
            }
            break;

        case CATTA_SERVER_FAILURE:
            {
                // asserts
                assert(mesh->catta_server != NULL);
                assert(mesh->catta_poll != NULL);

                /* Terminate on failure */
                logger(mesh, MESHLINK_ERROR, "Catta server failure: %s\n", catta_strerror(catta_server_errno(mesh->catta_server)));
                catta_simple_poll_quit(mesh->catta_poll);
            }
            break;

        case CATTA_SERVER_INVALID:
            break;
    }

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
}

static void discovery_resolve_callback(CattaSServiceResolver *resolver, CattaIfIndex interface_, CattaProtocol protocol, CattaResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const CattaAddress *address, uint16_t port, CattaStringList *txt, CattaLookupResultFlags flags, void *userdata)
{
    meshlink_handle_t *mesh = userdata;

    // asserts
    assert(resolver != NULL);
    assert(mesh != NULL);
    assert(mesh->catta_server != NULL);

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    /* Called whenever a service has been resolved successfully or timed out */
    switch(event)
    {
        case CATTA_RESOLVER_FAILURE:
            {
                // asserts
                assert(name != NULL);
                assert(type != NULL);
                assert(domain != NULL);

                logger(mesh, MESHLINK_WARNING, "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, catta_strerror(catta_server_errno(mesh->catta_server)));
            }
            break;

        case CATTA_RESOLVER_FOUND:
            {
                // asserts
                assert(name != NULL);
                assert(type != NULL);
                assert(domain != NULL);
                assert(host_name != NULL);
                assert(address != NULL);
                assert(txt != NULL);
        
                char straddr[CATTA_ADDRESS_STR_MAX], *strtxt;

                logger(mesh, MESHLINK_DEBUG, "(Resolver) Service '%s' of type '%s' in domain '%s':\n", name, type, domain);

                catta_address_snprint(straddr, sizeof(straddr), address);
                strtxt = catta_string_list_to_string(txt);
                logger(mesh, MESHLINK_DEBUG,
                        "\t%s:%u (%s)\n"
                        "\tTXT=%s\n"
                        "\tcookie is %u\n"
                        "\tis_local: %i\n"
                        "\twide_area: %i\n"
                        "\tmulticast: %i\n"
                        "\tcached: %i\n",
                        host_name, port, straddr,
                        strtxt,
                        catta_string_list_get_service_cookie(txt),
                        !!(flags & CATTA_LOOKUP_RESULT_LOCAL),
                        !!(flags & CATTA_LOOKUP_RESULT_WIDE_AREA),
                        !!(flags & CATTA_LOOKUP_RESULT_MULTICAST),
                        !!(flags & CATTA_LOOKUP_RESULT_CACHED));
                catta_free(strtxt);

                // retrieve fingerprint
                CattaStringList *node_name_li = catta_string_list_find(txt, MESHLINK_MDNS_NAME_KEY);
                CattaStringList *node_fp_li = catta_string_list_find(txt, MESHLINK_MDNS_FINGERPRINT_KEY);

                if(node_name_li != NULL && node_fp_li != NULL)
                {
                    char *node_name = (char*)catta_string_list_get_text(node_name_li) + strlen(MESHLINK_MDNS_NAME_KEY);
                    char *node_fp = (char*)catta_string_list_get_text(node_fp_li) + strlen(MESHLINK_MDNS_FINGERPRINT_KEY);

                    if(node_name[0] == '=' && node_fp[0] == '=')
                    {
                        node_name += 1;
                        node_fp += 1;

                        meshlink_node_t *node = meshlink_get_node(mesh, node_name);

                        if(node != NULL)
                        {
                            logger(mesh, MESHLINK_INFO, "Node %s is part of the mesh network.\n", node->name);

                            sockaddr_t naddress;
                            memset(&naddress, 0, sizeof(naddress));

                            switch(address->proto)
                            {
                                case CATTA_PROTO_INET:
                                    {
                                        naddress.in.sin_family = AF_INET;
                                        naddress.in.sin_port = htons(port);
                                        naddress.in.sin_addr.s_addr = address->data.ipv4.address;
                                    }
                                    break;

                                case CATTA_PROTO_INET6:
                                    {
                                        naddress.in6.sin6_family = AF_INET6;
                                        naddress.in6.sin6_port = htons(port);
                                        memcpy(naddress.in6.sin6_addr.s6_addr, address->data.ipv6.address, sizeof(naddress.in6.sin6_addr.s6_addr));
                                    }
                                    break;

                                default:
                                    naddress.unknown.family = AF_UNKNOWN;
                                    break;
                            }

                            if(naddress.unknown.family != AF_UNKNOWN)
                            {
                                meshlink_hint_address(mesh, (meshlink_node_t *)node, (struct sockaddr*)&naddress);
                            }
                            else
                            {
                                logger(mesh, MESHLINK_WARNING, "Could not resolve node %s to a known address family type.\n", node->name);
                            }
                        }
                        else
                        {
                            logger(mesh, MESHLINK_WARNING, "Node %s is not part of the mesh network.\n", node_name);
                        }
                    }
                    else
                    {
                        logger(mesh, MESHLINK_WARNING, "TXT records invalid.\n");
                    }
                }
                else
                {
                    logger(mesh, MESHLINK_WARNING, "TXT records missing.\n");
                }
            }
            break;
    }

    catta_s_service_resolver_free(resolver);

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
}

static void discovery_browse_callback(CattaSServiceBrowser *browser, CattaIfIndex interface_, CattaProtocol protocol, CattaBrowserEvent event, const char *name, const char *type, const char *domain, CattaLookupResultFlags flags, void* userdata)
{
	meshlink_handle_t *mesh = userdata;

    // asserts
    assert(mesh != NULL);
    assert(mesh->catta_server != NULL);
    assert(mesh->catta_poll != NULL);

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    /* Called whenever a new services becomes available on the LAN or is removed from the LAN */
    switch (event)
    {
        case CATTA_BROWSER_FAILURE:
            {
                logger(mesh, MESHLINK_ERROR, "(Browser) %s\n", catta_strerror(catta_server_errno(mesh->catta_server)));
                catta_simple_poll_quit(mesh->catta_poll);
            }
            break;

        case CATTA_BROWSER_NEW:
            {
                // asserts
                assert(name != NULL);
                assert(type != NULL);
                assert(domain != NULL);

                logger(mesh, MESHLINK_DEBUG, "(Browser) NEW: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
                /* We ignore the returned resolver object. In the callback
                   function we free it. Ifthe server is terminated before
                   the callback function is called the server will free
                   the resolver for us. */
                if(!(catta_s_service_resolver_new(mesh->catta_server, interface_, protocol, name, type, domain, CATTA_PROTO_UNSPEC, 0, discovery_resolve_callback, mesh)))
                {
                    logger(mesh, MESHLINK_DEBUG, "Failed to resolve service '%s': %s\n", name, catta_strerror(catta_server_errno(mesh->catta_server)));
                }
            }
            break;

        case CATTA_BROWSER_REMOVE:
            {
                // asserts
                assert(name != NULL);
                assert(type != NULL);
                assert(domain != NULL);

                logger(mesh, MESHLINK_DEBUG, "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
            }
            break;

        case CATTA_BROWSER_ALL_FOR_NOW:
        case CATTA_BROWSER_CACHE_EXHAUSTED:
            {
                logger(mesh, MESHLINK_DEBUG, "(Browser) %s\n", event == CATTA_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
            }
            break;
    }
    
    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
}

static void *discovery_loop(void *userdata)
{
	meshlink_handle_t *mesh = userdata;

    // asserts
    assert(mesh != NULL);
    assert(mesh->catta_poll != NULL);

    catta_simple_poll_loop(mesh->catta_poll);
	
    return NULL;
}

static void discovery_log_cb(CattaLogLevel level, const char *txt)
{
    meshlink_log_level_t mlevel = MESHLINK_CRITICAL;

    switch(level)
    {
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
        mlevel = MESHLINK_DEBUG;
        break;
    }

    logger(NULL, mlevel, "%s\n", txt);
}

bool discovery_start(meshlink_handle_t *mesh)
{
    logger(mesh, MESHLINK_DEBUG, "discovery_start called\n");

    assert(mesh != NULL);
    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    // asserts
    assert(mesh->catta_poll == NULL);
    assert(mesh->catta_server == NULL);
    assert(mesh->catta_browser == NULL);
    assert(mesh->discovery_threadstarted == false);
    assert(mesh->catta_servicetype == NULL);

    // handle catta logs
    catta_set_log_function(discovery_log_cb);

    // create service type string
    size_t servicetype_strlen = sizeof(MESHLINK_MDNS_SERVICE_TYPE) + strlen(mesh->appname) + 1;
    mesh->catta_servicetype = malloc(servicetype_strlen);

    if(mesh->catta_servicetype == NULL)
    {
        logger(mesh, MESHLINK_ERROR, "Failed to allocate memory for service type string.\n");
        goto fail;
    }

    snprintf(mesh->catta_servicetype, servicetype_strlen, MESHLINK_MDNS_SERVICE_TYPE, mesh->appname);

    // Allocate discovery loop object
    if(!(mesh->catta_poll = catta_simple_poll_new()))
    {
        logger(mesh, MESHLINK_ERROR, "Failed to create discovery poll object.\n");
		goto fail;
    }

    // generate some unique host name (we actually do not care about it)
    char hostname[17];
    generate_rand_string(hostname, sizeof(hostname));

    // Let's set the host name for this server.
    CattaServerConfig config;
    catta_server_config_init(&config);
    config.host_name = catta_strdup(hostname);
    config.publish_workstation = 0;
    config.disallow_other_stacks = 0;
    config.publish_hinfo = 0;
    config.publish_addresses = 1;
    config.publish_no_reverse = 1;

    /* Allocate a new server */
    int error;
    mesh->catta_server = catta_server_new(catta_simple_poll_get(mesh->catta_poll), &config, discovery_server_callback, mesh, &error);

    /* Free the configuration data */
    catta_server_config_free(&config);

    /* Check wether creating the server object succeeded */
    if(!mesh->catta_server)
    {
        logger(mesh, MESHLINK_ERROR, "Failed to create discovery server: %s\n", catta_strerror(error));
        goto fail;
    }

    // Create the service browser
    if(!(mesh->catta_browser = catta_s_service_browser_new(mesh->catta_server, CATTA_IF_UNSPEC, CATTA_PROTO_UNSPEC, mesh->catta_servicetype, NULL, 0, discovery_browse_callback, mesh)))
    {
        logger(mesh, MESHLINK_ERROR, "Failed to create discovery service browser: %s\n", catta_strerror(catta_server_errno(mesh->catta_server)));
        goto fail;
    }

	// Start the discovery thread
	if(pthread_create(&mesh->discovery_thread, NULL, discovery_loop, mesh) != 0)
    {
		logger(mesh, MESHLINK_ERROR, "Could not start discovery thread: %s\n", strerror(errno));
		memset(&mesh->discovery_thread, 0, sizeof mesh->discovery_thread);
		goto fail;
	}

	mesh->discovery_threadstarted = true;

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
	return true;

fail:
    if(mesh->catta_browser != NULL)
    {
        catta_s_service_browser_free(mesh->catta_browser);
        mesh->catta_browser = NULL;
    }

    if(mesh->catta_server != NULL)
    {
        catta_server_free(mesh->catta_server);
        mesh->catta_server = NULL;
    }

    if(mesh->catta_poll != NULL)
    {
        catta_simple_poll_free(mesh->catta_poll);
        mesh->catta_poll = NULL;
    }

    if(mesh->catta_servicetype != NULL)
    {
        free(mesh->catta_servicetype);
        mesh->catta_servicetype = NULL;
    }

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return false;
}

void discovery_stop(meshlink_handle_t *mesh)
{
    logger(mesh, MESHLINK_DEBUG, "discovery_stop called\n");

    // asserts
    assert(mesh != NULL);

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

	// Shut down
    if(mesh->catta_poll)
    {
        catta_simple_poll_quit(mesh->catta_poll);
    }

	// Wait for the discovery thread to finish
    if(mesh->discovery_threadstarted == true)
    {
        pthread_t thread = mesh->discovery_thread;

        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        pthread_join(thread, NULL);
        MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

        mesh->discovery_threadstarted = false;
    }

	// Clean up resources
    if(mesh->catta_browser != NULL)
    {
        catta_s_service_browser_free(mesh->catta_browser);
        mesh->catta_browser = NULL;
    }

    if(mesh->catta_group)
    {
        catta_s_entry_group_reset(mesh->catta_group);
        catta_s_entry_group_free(mesh->catta_group);
        mesh->catta_group = NULL;
    }

    if(mesh->catta_server != NULL)
    {
        catta_server_free(mesh->catta_server);
        mesh->catta_server = NULL;
    }

    if(mesh->catta_poll != NULL)
    {
        catta_simple_poll_free(mesh->catta_poll);
        mesh->catta_poll = NULL;
    }

    if(mesh->catta_servicetype != NULL)
    {
        free(mesh->catta_servicetype);
        mesh->catta_servicetype = NULL;
    }

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
}
