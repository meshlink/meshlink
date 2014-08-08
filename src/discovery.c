
#include "meshlink_internal.h"
#include "discovery.h"
#include "sockaddr.h"

#include <pthread.h>

#include <avahi-core/core.h>
#include <avahi-core/lookup.h>
#include <avahi-core/publish.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>

#include <netinet/in.h>

#include <uuid/uuid.h>

#define MESHLINK_MDNS_SERVICE_TYPE "_meshlink._tcp"
#define MESHLINK_MDNS_NAME_KEY "name"
#define MESHLINK_MDNS_FINGERPRINT_KEY "fingerprint"

static void discovery_entry_group_callback(AvahiServer *server, AvahiSEntryGroup *group, AvahiEntryGroupState state, void *userdata)
{
    meshlink_handle_t *mesh = userdata;

    // asserts
    assert(mesh != NULL);
    assert(mesh->avahi_server != NULL);
    assert(mesh->avahi_poll != NULL);

    /* Called whenever the entry group state changes */
    switch(state)
    {
        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            /* The entry group has been established successfully */
            fprintf(stderr, "Service successfully established.\n");
            break;

        case AVAHI_ENTRY_GROUP_COLLISION:
            fprintf(stderr, "Service collision\n");
            // @TODO can we just set a new name and retry?
            break;

        case AVAHI_ENTRY_GROUP_FAILURE :
            /* Some kind of failure happened while we were registering our services */
            fprintf(stderr, "Entry group failure: %s\n", avahi_strerror(avahi_server_errno(mesh->avahi_server)));
            avahi_simple_poll_quit(mesh->avahi_poll);
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            ;
    }
}


static void discovery_create_services(meshlink_handle_t *mesh)
{
    char *txt_name = NULL;

    // asserts
    assert(mesh != NULL);
    assert(mesh->name != NULL);
    assert(mesh->myport != NULL);
    assert(mesh->avahi_server != NULL);
    assert(mesh->avahi_poll != NULL);

    fprintf(stderr, "Adding service\n");

    /* Ifthis is the first time we're called, let's create a new entry group */
    if(!mesh->avahi_group)
    {
        if(!(mesh->avahi_group = avahi_s_entry_group_new(mesh->avahi_server, discovery_entry_group_callback, mesh)))
        {
            fprintf(stderr, "avahi_entry_group_new() failed: %s\n", avahi_strerror(avahi_server_errno(mesh->avahi_server)));
            goto fail;
        }
    }

    /* Create txt records */
    size_t txt_name_len = sizeof(MESHLINK_MDNS_NAME_KEY) + 1 + strlen(mesh->name) + 1;
    txt_name = malloc(txt_name_len);
    snprintf(txt_name, txt_name_len, "%s=%s", MESHLINK_MDNS_NAME_KEY, mesh->name);

    char txt_fingerprint[sizeof(MESHLINK_MDNS_FINGERPRINT_KEY) + 1 + MESHLINK_FINGERPRINTLEN + 1];
    snprintf(txt_fingerprint, sizeof(txt_fingerprint), "%s=%s", MESHLINK_MDNS_FINGERPRINT_KEY, meshlink_get_fingerprint(mesh, (meshlink_node_t *)mesh->self));

    // Generate a name for the service (actually we do not care)
    uuid_t srvname;
    uuid_generate(srvname);

    char srvnamestr[36+1];
    uuid_unparse_lower(srvname, srvnamestr);

    /* Add the service */
    int ret = 0;
    if((ret = avahi_server_add_service(mesh->avahi_server, mesh->avahi_group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, srvnamestr, MESHLINK_MDNS_SERVICE_TYPE, NULL, NULL, atoi(mesh->myport), txt_name, txt_fingerprint, NULL)) < 0)
    {
        fprintf(stderr, "Failed to add service: %s\n", avahi_strerror(ret));
        goto fail;
    }

    /* Tell the server to register the service */
    if((ret = avahi_s_entry_group_commit(mesh->avahi_group)) < 0)
    {
        fprintf(stderr, "Failed to commit entry_group: %s\n", avahi_strerror(ret));
        goto fail;
    }

    goto done;

fail:
    avahi_simple_poll_quit(mesh->avahi_poll);

done:
    if(txt_name)
        free(txt_name);
}

static void discovery_server_callback(AvahiServer *server, AvahiServerState state, void * userdata)
{
	meshlink_handle_t *mesh = userdata;

    // asserts
    assert(mesh != NULL);

    switch(state)
    {
        case AVAHI_SERVER_RUNNING:
            {
                /* The serve has startup successfully and registered its host
                 * name on the network, so it's time to create our services */
                if(!mesh->avahi_group)
                {
                    discovery_create_services(mesh);
                }
            }
            break;

        case AVAHI_SERVER_COLLISION:
            {
                // asserts
                assert(mesh->avahi_server != NULL);
                assert(mesh->avahi_poll != NULL);

                /* A host name collision happened. Let's pick a new name for the server */
                uuid_t hostname;
                uuid_generate(hostname);

                char hostnamestr[36+1];
                uuid_unparse_lower(hostname, hostnamestr);

                fprintf(stderr, "Host name collision, retrying with '%s'\n", hostnamestr);
                int result = avahi_server_set_host_name(mesh->avahi_server, hostnamestr);

                if(result < 0)
                {
                    fprintf(stderr, "Failed to set new host name: %s\n", avahi_strerror(result));
                    avahi_simple_poll_quit(mesh->avahi_poll);
                    return;
                }
            }
            break;

        case AVAHI_SERVER_REGISTERING:
            {
                /* Let's drop our registered services. When the server is back
                 * in AVAHI_SERVER_RUNNING state we will register them
                 * again with the new host name. */
                if(mesh->avahi_group)
                {
                    avahi_s_entry_group_reset(mesh->avahi_group);
                    mesh->avahi_group = NULL;
                }
            }
            break;

        case AVAHI_SERVER_FAILURE:
            {
                // asserts
                assert(mesh->avahi_server != NULL);
                assert(mesh->avahi_poll != NULL);

                /* Terminate on failure */
                fprintf(stderr, "Server failure: %s\n", avahi_strerror(avahi_server_errno(mesh->avahi_server)));
                avahi_simple_poll_quit(mesh->avahi_poll);
            }
            break;

        case AVAHI_SERVER_INVALID:
            break;
    }
}

static void discovery_resolve_callback(AvahiSServiceResolver *resolver, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event, const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *address, uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags, void* userdata)
{
    meshlink_handle_t *mesh = userdata;

    // asserts
    assert(resolver != NULL);
    assert(mesh != NULL);
    assert(mesh->avahi_server != NULL);

    /* Called whenever a service has been resolved successfully or timed out */
    switch(event)
    {
        case AVAHI_RESOLVER_FAILURE:
            {
                // asserts
                assert(name != NULL);
                assert(type != NULL);
                assert(domain != NULL);

                fprintf(stderr, "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror(avahi_server_errno(mesh->avahi_server)));
            }
            break;

        case AVAHI_RESOLVER_FOUND:
            {
                // asserts
                assert(name != NULL);
                assert(type != NULL);
                assert(domain != NULL);
                assert(host_name != NULL);
                assert(address != NULL);
                assert(txt != NULL);
        
                char straddr[AVAHI_ADDRESS_STR_MAX], *strtxt;

                fprintf(stderr, "(Resolver) Service '%s' of type '%s' in domain '%s':\n", name, type, domain);

                avahi_address_snprint(straddr, sizeof(straddr), address);
                strtxt = avahi_string_list_to_string(txt);
                fprintf(stderr,
                        "\t%s:%u (%s)\n"
                        "\tTXT=%s\n"
                        "\tcookie is %u\n"
                        "\tis_local: %i\n"
                        "\twide_area: %i\n"
                        "\tmulticast: %i\n"
                        "\tcached: %i\n",
                        host_name, port, straddr,
                        strtxt,
                        avahi_string_list_get_service_cookie(txt),
                        !!(flags & AVAHI_LOOKUP_RESULT_LOCAL),
                        !!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA),
                        !!(flags & AVAHI_LOOKUP_RESULT_MULTICAST),
                        !!(flags & AVAHI_LOOKUP_RESULT_CACHED));
                avahi_free(strtxt);

                // retrieve fingerprint
                AvahiStringList *node_name_li = avahi_string_list_find(txt, MESHLINK_MDNS_NAME_KEY);
                AvahiStringList *node_fp_li = avahi_string_list_find(txt, MESHLINK_MDNS_FINGERPRINT_KEY);

                if(node_name_li != NULL && node_fp_li != NULL)
                {
                    char *node_name = avahi_string_list_get_text(node_name_li) + strlen(MESHLINK_MDNS_NAME_KEY) + 1;
                    char *node_fp = avahi_string_list_get_text(node_fp_li) + strlen(MESHLINK_MDNS_FINGERPRINT_KEY) + 1;

                    meshlink_node_t *node = meshlink_get_node(mesh, node_name);

                    if(node != NULL)
                    {
                        fprintf(stderr, "Node %s is part of the mesh network.\n", node->name);

                        sockaddr_t naddress;
                        memset(&naddress, 0, sizeof(naddress));

                        switch(address->proto)
                        {
                            case AVAHI_PROTO_INET:
                                {
                                    naddress.in.sin_family = AF_INET;
                                    naddress.in.sin_port = port;
                                    naddress.in.sin_addr.s_addr = address->data.ipv4.address;
                                }
                                break;

                            case AVAHI_PROTO_INET6:
                                {
                                    naddress.in6.sin6_family = AF_INET6;
                                    naddress.in6.sin6_port = port;
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
                            fprintf(stderr, "Could not resolve node %s to a known address family type.\n", node->name);
                        }
                    }
                    else
                    {
                        fprintf(stderr, "Node %s is not part of the mesh network.\n", node_name);
                    }
                }
                else
                {
                    fprintf(stderr, "TXT records missing.\n");
                }
            }
            break;
    }

    avahi_s_service_resolver_free(resolver);
}

static void discovery_browse_callback(AvahiSServiceBrowser *browser, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name, const char *type, const char *domain, AvahiLookupResultFlags flags, void* userdata)
{
	meshlink_handle_t *mesh = userdata;

    // asserts
    assert(mesh != NULL);
    assert(mesh->avahi_server != NULL);
    assert(mesh->avahi_poll != NULL);

    /* Called whenever a new services becomes available on the LAN or is removed from the LAN */
    switch (event)
    {
        case AVAHI_BROWSER_FAILURE:
            {
                fprintf(stderr, "(Browser) %s\n", avahi_strerror(avahi_server_errno(mesh->avahi_server)));
                avahi_simple_poll_quit(mesh->avahi_poll);
                return;
            }

        case AVAHI_BROWSER_NEW:
            {
                // asserts
                assert(name != NULL);
                assert(type != NULL);
                assert(domain != NULL);

                fprintf(stderr, "(Browser) NEW: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
                /* We ignore the returned resolver object. In the callback
                   function we free it. Ifthe server is terminated before
                   the callback function is called the server will free
                   the resolver for us. */
                if(!(avahi_s_service_resolver_new(mesh->avahi_server, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, discovery_resolve_callback, mesh)))
                {
                    fprintf(stderr, "Failed to resolve service '%s': %s\n", name, avahi_strerror(avahi_server_errno(mesh->avahi_server)));
                }
            }
            break;

        case AVAHI_BROWSER_REMOVE:
            {
                // asserts
                assert(name != NULL);
                assert(type != NULL);
                assert(domain != NULL);

                fprintf(stderr, "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
            }
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            {
                fprintf(stderr, "(Browser) %s\n", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
            }
            break;
    }
}

static void *discovery_loop(void *userdata)
{
	meshlink_handle_t *mesh = userdata;

    // asserts
    assert(mesh != NULL);
    assert(mesh->avahi_poll != NULL);

    avahi_simple_poll_loop(mesh->avahi_poll);
	
    return NULL;
}

bool discovery_start(meshlink_handle_t *mesh)
{
    // asserts
    assert(mesh != NULL);
    assert(mesh->avahi_poll == NULL);
    assert(mesh->avahi_server == NULL);
    assert(mesh->avahi_browser == NULL);
    assert(mesh->discovery_threadstarted == false);

    // Allocate discovery loop object
    if(!(mesh->avahi_poll = avahi_simple_poll_new()))
    {
        fprintf(stderr, "Failed to create discovery poll object.\n");
		goto fail;
    }

    // generate some unique host name (we actually do not care about it)
    uuid_t hostname;
    uuid_generate(hostname);

    char hostnamestr[36+1];
    uuid_unparse_lower(hostname, hostnamestr);

    // Let's set the host name for this server.
    AvahiServerConfig config;
    avahi_server_config_init(&config);
    config.host_name = avahi_strdup(hostnamestr);
    config.publish_workstation = 0;
    config.disallow_other_stacks = 0;
    config.publish_hinfo = 0;
    config.publish_addresses = 1;
    config.publish_no_reverse = 1;

    /* Allocate a new server */
    int error;
    mesh->avahi_server = avahi_server_new(avahi_simple_poll_get(mesh->avahi_poll), &config, discovery_server_callback, mesh, &error);

    /* Free the configuration data */
    avahi_server_config_free(&config);

    /* Check wether creating the server object succeeded */
    if(!mesh->avahi_server)
    {
        fprintf(stderr, "Failed to create discovery server: %s\n", avahi_strerror(error));
        goto fail;
    }

    // Create the service browser
    if(!(mesh->avahi_browser = avahi_s_service_browser_new(mesh->avahi_server, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, MESHLINK_MDNS_SERVICE_TYPE, NULL, 0, discovery_browse_callback, mesh)))
    {
        fprintf(stderr, "Failed to create discovery service browser: %s\n", avahi_strerror(avahi_server_errno(mesh->avahi_server)));
        goto fail;
    }

	// Start the discovery thread
	if(pthread_create(&mesh->discovery_thread, NULL, discovery_loop, mesh) != 0)
    {
		fprintf(stderr, "Could not start discovery thread: %s\n", strerror(errno));
		memset(&mesh->discovery_thread, 0, sizeof mesh->discovery_thread);
		goto fail;
	}

	mesh->discovery_threadstarted = true;

	return true;

fail:
    if(mesh->avahi_browser)
    {
        avahi_s_service_browser_free(mesh->avahi_browser);
        mesh->avahi_browser = NULL;
    }

    if(mesh->avahi_server)
    {
        avahi_server_free(mesh->avahi_server);
        mesh->avahi_server = NULL;
    }

    if(mesh->avahi_poll)
    {
        avahi_simple_poll_free(mesh->avahi_poll);
        mesh->avahi_poll = NULL;
    }

    return false;
}

void discovery_stop(meshlink_handle_t *mesh)
{
    // asserts
    assert(mesh != NULL);
    assert(mesh->avahi_poll != NULL);
    assert(mesh->avahi_server != NULL);
    assert(mesh->avahi_browser != NULL);
    assert(mesh->discovery_threadstarted == true);

	// Shut down 
	avahi_simple_poll_quit(mesh->avahi_poll);

	// Wait for the discovery thread to finish
	pthread_join(mesh->discovery_thread, NULL);

	// Clean up resources
    avahi_s_service_browser_free(mesh->avahi_browser);
    mesh->avahi_browser = NULL;

    avahi_server_free(mesh->avahi_server);
    mesh->avahi_server = NULL;

    avahi_simple_poll_free(mesh->avahi_poll);
    mesh->avahi_poll = NULL;
}
