
#include "system.h"

#include "logger.h"
#include "meshlink_internal.h"
#include "node.h"
#include "splay_tree.h"
#include "netutl.h"
#include "xalloc.h"

#include "devtools.h"

static int node_compare(const void *a, const void *b) {
	if(a < b)
		return -1;

	if(a > b)
		return 1;

	return 0;
}

static bool fstrwrite(const char* str, FILE* stream) {
	size_t len = strlen(str);

	if(fwrite((void*)str, 1, len, stream) != len)
		return false;

	return true;
}

static const char* __itoa(int value) {
	static char buffer[sizeof(int) * 8 + 1];        // not thread safe

	if(snprintf(buffer, sizeof(buffer), "%d", value) == -1)
		return "";

	return buffer;
}

bool devtool_export_json_all_edges_state(meshlink_handle_t *mesh, FILE* stream) {
	bool result = true;

	pthread_mutex_lock(&(mesh->mesh_mutex));

	// export edges and nodes
	size_t node_count = 0;
	size_t edge_count = 0;

	meshlink_node_t **nodes = meshlink_get_all_nodes(mesh, NULL, &node_count);
	meshlink_edge_t **edges = meshlink_get_all_edges_state(mesh, NULL, &edge_count);

	if((!nodes && node_count != 0) || (!edges && edge_count != 0))
		goto fail;

	// export begin
	if(!fstrwrite("{\n", stream))
		goto fail;

	// export nodes
	if(!fstrwrite("\t\"nodes\": {\n", stream))
		goto fail;

	for(size_t i = 0; i < node_count; ++i) {
		if(!fstrwrite("\t\t\"", stream) || !fstrwrite(((node_t*)nodes[i])->name, stream) || !fstrwrite("\": {\n", stream))
			goto fail;

		if(!fstrwrite("\t\t\t\"name\": \"", stream) || !fstrwrite(((node_t*)nodes[i])->name, stream) || !fstrwrite("\",\n", stream))
			goto fail;

		if(!fstrwrite("\t\t\t\"options\": ", stream) || !fstrwrite(__itoa(((node_t*)nodes[i])->options), stream) || !fstrwrite(",\n", stream))
			goto fail;

		if(!fstrwrite("\t\t\t\"devclass\": ", stream) || !fstrwrite(__itoa(((node_t*)nodes[i])->devclass), stream) || !fstrwrite("\n", stream))
			goto fail;

		if(!fstrwrite((i+1) != node_count ? "\t\t},\n" : "\t\t}\n", stream))
			goto fail;
	}

	if(!fstrwrite("\t},\n", stream))
		goto fail;

	// export edges

	if(!fstrwrite("\t\"edges\": {\n", stream))
		goto fail;

	for(size_t i = 0; i < edge_count; ++i) {
		if(!fstrwrite("\t\t\"", stream) || !fstrwrite(edges[i]->from->name, stream) || !fstrwrite("_to_", stream) || !fstrwrite(edges[i]->to->name, stream) || !fstrwrite("\": {\n", stream))
			goto fail;

		if(!fstrwrite("\t\t\t\"from\": \"", stream) || !fstrwrite(edges[i]->from->name, stream) || !fstrwrite("\",\n", stream))
			goto fail;

		if(!fstrwrite("\t\t\t\"to\": \"", stream) || !fstrwrite(edges[i]->to->name, stream) || !fstrwrite("\",\n", stream))
			goto fail;

		char *host = NULL, *port = NULL, *address = NULL;
		sockaddr2str((const sockaddr_t *)&(edges[i]->address), &host, &port);

		if(host && port)
			xasprintf(&address, "{ \"host\": \"%s\", \"port\": %s }", host, port);

		free(host);
		free(port);

		if(!fstrwrite("\t\t\t\"address\": ", stream) || !fstrwrite(address ? address : "null", stream) || !fstrwrite(",\n", stream)) {
			free(address);
			goto fail;
		}

		free(address);

		if(!fstrwrite("\t\t\t\"options\": ", stream) || !fstrwrite(__itoa(edges[i]->options), stream) || !fstrwrite(",\n", stream))
			goto fail;

		if(!fstrwrite("\t\t\t\"weight\": ", stream) || !fstrwrite(__itoa(edges[i]->weight), stream) || !fstrwrite("\n", stream))
			goto fail;

		if(!fstrwrite((i+1) != edge_count ? "\t\t},\n" : "\t\t}\n", stream))
			goto fail;
	}

	if(!fstrwrite("\t}\n", stream))
		goto fail;

	// DONE!

	if(!fstrwrite("}", stream))
		goto fail;

	goto done;

fail:
	result = false;

done:

	if(nodes)
		free(nodes);

	for(size_t i = 0; edges && i < edge_count; ++i)
		free(edges[i]);

	if(nodes)
		free(edges);

	pthread_mutex_unlock(&(mesh->mesh_mutex));

	return result;
}
