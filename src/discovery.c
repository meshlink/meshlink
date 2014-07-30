
#include "meshlink_internal.h"
#include "discovery.h"

#include <pthread.h>

#include <avahi-core/core.h>
#include <avahi-core/lookup.h>
#include <avahi-core/publish.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>

#define MESHLINK_MDNS_SERVICE_TYPE "_meshlink._tcp"
//#define MESHLINK_MDNS_SERVICE_NAME "Meshlink"
#define MESHLINK_MDNS_FINGERPRINT_KEY "fingerprint"

static void discovery_resolve_callback(
    AvahiSServiceResolver *resolver,
    AVAHI_GCC_UNUSED AvahiIfIndex interface,
    AVAHI_GCC_UNUSED AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *address,
    uint16_t port,
    AvahiStringList *txt,
    AvahiLookupResultFlags flags,
    AVAHI_GCC_UNUSED void* userdata)
{

    meshlink_handle_t *mesh = userdata;

    /* Called whenever a service has been resolved successfully or timed out */

    switch (event)
    {
        case AVAHI_RESOLVER_FAILURE:
            fprintf(stderr, "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror(avahi_server_errno(mesh->avahi_server)));
            break;

        case AVAHI_RESOLVER_FOUND:
        {
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
            AvahiStringList *fgli = avahi_string_list_find(txt, MESHLINK_MDNS_FINGERPRINT_KEY);
            meshlink_node_t *node = meshlink_get_node(mesh, name);

            fprintf(stderr, "%p, %p, %s, %s\n", fgli, node, avahi_string_list_get_text(fgli), meshlink_get_fingerprint(mesh, node));

            if( node && fgli && strcmp(avahi_string_list_get_text(fgli)+strlen(MESHLINK_MDNS_FINGERPRINT_KEY)+1, meshlink_get_fingerprint(mesh, node)) == 0 )
            {
                fprintf(stderr, "Node %s is part of the mesh network - updating ip address.\n", node->name);
            }
            else
            {
                fprintf(stderr, "Node %s is not part of the mesh network - ignoring ip address.\n", node->name);
            }
        }
    }

    avahi_s_service_resolver_free(resolver);
}

static void discovery_entry_group_callback(AvahiServer *server, AvahiSEntryGroup *group, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata)
{
    meshlink_handle_t *mesh = userdata;

    /* Called whenever the entry group state changes */
    switch (state)
    {
        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            /* The entry group has been established successfully */
            fprintf(stderr, "Service '%s' successfully established.\n", /*MESHLINK_MDNS_SERVICE_NAME*/ mesh->name);
            break;

        case AVAHI_ENTRY_GROUP_COLLISION:
            fprintf(stderr, "Service name collision '%s'\n", /*MESHLINK_MDNS_SERVICE_NAME*/ mesh->name);
            break;

        case AVAHI_ENTRY_GROUP_FAILURE :
            fprintf(stderr, "Entry group failure: %s\n", avahi_strerror(avahi_server_errno(mesh->avahi_server)));

            /* Some kind of failure happened while we were registering our services */
            avahi_simple_poll_quit(mesh->avahi_poll);
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            ;
    }
}


static void discovery_create_services(meshlink_handle_t *mesh)
{
    fprintf(stderr, "Adding service '%s'\n", /*MESHLINK_MDNS_SERVICE_NAME*/ mesh->name);

    /* If this is the first time we're called, let's create a new entry group */
    if (!mesh->avahi_group)
        if (!(mesh->avahi_group = avahi_s_entry_group_new(mesh->avahi_server, discovery_entry_group_callback, mesh))) {
            fprintf(stderr, "avahi_entry_group_new() failed: %s\n", avahi_strerror(avahi_server_errno(mesh->avahi_server)));
            goto fail;
        }

    /* Create some random TXT data */
    char fingerprint[1024] = "";
    snprintf(fingerprint, sizeof(fingerprint), "%s=%s", MESHLINK_MDNS_FINGERPRINT_KEY, meshlink_get_fingerprint(mesh, meshlink_get_node(mesh, mesh->name)));

    /* Add the service for IPP */
    int ret = 0;
    if ((ret = avahi_server_add_service(mesh->avahi_server, mesh->avahi_group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, /*MESHLINK_MDNS_SERVICE_NAME*/ mesh->name, MESHLINK_MDNS_SERVICE_TYPE, NULL, NULL, mesh->myport ? atoi(mesh->myport) : 655, fingerprint, NULL)) < 0) {
        fprintf(stderr, "Failed to add _ipp._tcp service: %s\n", avahi_strerror(ret));
        goto fail;
    }

    /* Tell the server to register the service */
    if ((ret = avahi_s_entry_group_commit(mesh->avahi_group)) < 0) {
        fprintf(stderr, "Failed to commit entry_group: %s\n", avahi_strerror(ret));
        goto fail;
    }

    return;

fail:
    avahi_simple_poll_quit(mesh->avahi_poll);
}

static void discovery_server_callback(AvahiServer *server, AvahiServerState state, AVAHI_GCC_UNUSED void * userdata)
{
	meshlink_handle_t *mesh = userdata;

    switch (state)
    {
        case AVAHI_SERVER_RUNNING:
            /* The serve has startup successfully and registered its host
             * name on the network, so it's time to create our services */
            if (!mesh->avahi_group)
                discovery_create_services(mesh);
            break;

        case AVAHI_SERVER_COLLISION:
            /* A host name collision happened. Let's do nothing */
            break;

        case AVAHI_SERVER_REGISTERING:
	    	/* Let's drop our registered services. When the server is back
             * in AVAHI_SERVER_RUNNING state we will register them
             * again with the new host name. */
            //if (mesh->avahi_group)
            //    avahi_s_entry_group_reset(mesh->avahi_group);
            break;

        case AVAHI_SERVER_FAILURE:
            /* Terminate on failure */
            fprintf(stderr, "Server failure: %s\n", avahi_strerror(avahi_server_errno(mesh->avahi_server)));
            avahi_simple_poll_quit(mesh->avahi_poll);
            break;

        case AVAHI_SERVER_INVALID:
            break;
    }
}

static void discovery_browse_callback(
    AvahiSServiceBrowser *browser,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void* userdata)
{
	meshlink_handle_t *mesh = userdata;

    /* Called whenever a new services becomes available on the LAN or is removed from the LAN */

    switch (event)
    {
        case AVAHI_BROWSER_FAILURE:
            fprintf(stderr, "(Browser) %s\n", avahi_strerror(avahi_server_errno(mesh->avahi_server)));
            avahi_simple_poll_quit(mesh->avahi_poll);
            return;

        case AVAHI_BROWSER_NEW:
            fprintf(stderr, "(Browser) NEW: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
            /* We ignore the returned resolver object. In the callback
               function we free it. If the server is terminated before
               the callback function is called the server will free
               the resolver for us. */
            if (!(avahi_s_service_resolver_new(mesh->avahi_server, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, discovery_resolve_callback, mesh)))
                fprintf(stderr, "Failed to resolve service '%s': %s\n", name, avahi_strerror(avahi_server_errno(mesh->avahi_server)));
            break;

        case AVAHI_BROWSER_REMOVE:
            fprintf(stderr, "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            fprintf(stderr, "(Browser) %s\n", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
            break;
    }
}

static void *discovery_loop(void *arg)
{
	meshlink_handle_t *mesh = arg;

    avahi_simple_poll_loop(mesh->avahi_poll);

	return NULL;
}

bool discovery_start(meshlink_handle_t *mesh)
{
    // Allocate discovery loop object
    if (!(mesh->avahi_poll = avahi_simple_poll_new())) {
        fprintf(stderr, "Failed to create discovery poll object.\n");
		goto fail;
    }

    // Let's set the host name for this server.
    AvahiServerConfig config;
    avahi_server_config_init(&config);
    config.host_name = avahi_strdup(mesh->name);
    config.publish_workstation = 0;

    /* Allocate a new server */
    int error;
    mesh->avahi_server = avahi_server_new(avahi_simple_poll_get(mesh->avahi_poll), &config, discovery_server_callback, mesh, &error);

    /* Free the configuration data */
    avahi_server_config_free(&config);

    /* Check wether creating the server object succeeded */
    if (!mesh->avahi_server) {
        fprintf(stderr, "Failed to create discovery server: %s\n", avahi_strerror(error));
        goto fail;
    }

    // Create the service browser
    if (!(mesh->avahi_browser = avahi_s_service_browser_new(mesh->avahi_server, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, MESHLINK_MDNS_SERVICE_TYPE, NULL, 0, discovery_browse_callback, mesh))) {
        fprintf(stderr, "Failed to create discovery service browser: %s\n", avahi_strerror(avahi_server_errno(mesh->avahi_server)));
        goto fail;
    }

	// Start the discovery thread
	if(pthread_create(&mesh->discovery_thread, NULL, discovery_loop, mesh) != 0) {
		fprintf(stderr, "Could not start discovery thread: %s\n", strerror(errno));
		memset(&mesh->discovery_thread, 0, sizeof mesh->discovery_thread);
		goto fail;
	}

	mesh->discovery_threadstarted = true;

	return true;

fail:
    if (mesh->avahi_browser)
        avahi_s_service_browser_free(mesh->avahi_browser);

    if (mesh->avahi_server)
        avahi_server_free(mesh->avahi_server);

    if (mesh->avahi_poll)
        avahi_simple_poll_free(mesh->avahi_poll);

    return false;
}

void discovery_stop(meshlink_handle_t *mesh)
{
	// @TODO: Shut down 
	avahi_simple_poll_quit(mesh->avahi_poll);

	// Wait for the discovery thread to finish

	pthread_join(mesh->discovery_thread, NULL);

	// Clean up resources
    if (mesh->avahi_browser)
        avahi_s_service_browser_free(mesh->avahi_browser);

    if (mesh->avahi_server)
        avahi_server_free(mesh->avahi_server);

    if (mesh->avahi_poll)
        avahi_simple_poll_free(mesh->avahi_poll);
}
