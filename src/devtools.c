/*
    devtools.c -- Debugging and quality control functions.
    Copyright (C) 2014, 2017 Guus Sliepen <guus@meshlink.io>

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
#include <assert.h>

#include "logger.h"
#include "meshlink_internal.h"
#include "node.h"
#include "submesh.h"
#include "splay_tree.h"
#include "netutl.h"
#include "xalloc.h"

#include "devtools.h"

static void nop_probe(void) {
	return;
}

static void keyrotate_nop_probe(int stage) {
	(void)stage;
	return;
}

static void inviter_commits_first_nop_probe(bool stage) {
	(void)stage;
	return;
}

static void sptps_renewal_nop_probe(meshlink_node_t *node) {
	(void)node;
	return;
}

void (*devtool_trybind_probe)(void) = nop_probe;
void (*devtool_keyrotate_probe)(int stage) = keyrotate_nop_probe;
void (*devtool_set_inviter_commits_first)(bool inviter_commited_first) = inviter_commits_first_nop_probe;
void (*devtool_adns_resolve_probe)(void) = nop_probe;
void (*devtool_sptps_renewal_probe)(meshlink_node_t *node) = sptps_renewal_nop_probe;

/* Return an array of edges in the current network graph.
 * Data captures the current state and will not be updated.
 * Caller must deallocate data when done.
 */
devtool_edge_t *devtool_get_all_edges(meshlink_handle_t *mesh, devtool_edge_t *edges, size_t *nmemb) {
	if(!mesh || !nmemb || (*nmemb && !edges)) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	devtool_edge_t *result = NULL;
	unsigned int result_size = 0;

	result_size = mesh->edges->count / 2;

	// if result is smaller than edges, we have to dealloc all the excess devtool_edge_t
	if((size_t)result_size > *nmemb) {
		result = xrealloc(edges, result_size * sizeof(*result));
	} else {
		result = edges;
	}

	if(result) {
		devtool_edge_t *p = result;
		unsigned int n = 0;

		for splay_each(edge_t, e, mesh->edges) {
			// skip edges that do not represent a two-directional connection
			if(!e->reverse || e->reverse->to != e->from) {
				continue;
			}

			// don't count edges twice
			if(e->to < e->from) {
				continue;
			}

			assert(n < result_size);

			p->from = (meshlink_node_t *)e->from;
			p->to = (meshlink_node_t *)e->to;
			p->address = e->address.storage;
			p->weight = e->weight;

			n++;
			p++;
		}

		// shrink result to the actual amount of memory used
		result = xrealloc(result, n * sizeof(*result));
		*nmemb = n;
	} else {
		*nmemb = 0;
		meshlink_errno = MESHLINK_ENOMEM;
	}

	pthread_mutex_unlock(&mesh->mutex);

	return result;
}

static bool fstrwrite(const char *str, FILE *stream) {
	assert(stream);

	size_t len = strlen(str);

	if(fwrite((void *)str, 1, len, stream) != len) {
		return false;
	}

	return true;
}

bool devtool_export_json_all_edges_state(meshlink_handle_t *mesh, FILE *stream) {
	assert(stream);

	bool result = true;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	// export edges and nodes
	size_t node_count = 0;
	size_t edge_count = 0;

	meshlink_node_t **nodes = meshlink_get_all_nodes(mesh, NULL, &node_count);
	devtool_edge_t *edges = devtool_get_all_edges(mesh, NULL, &edge_count);

	if((!nodes && node_count != 0) || (!edges && edge_count != 0)) {
		goto fail;
	}

	// export begin
	if(!fstrwrite("{\n", stream)) {
		goto fail;
	}

	// export nodes
	if(!fstrwrite("\t\"nodes\": {\n", stream)) {
		goto fail;
	}

	char buf[16];

	for(size_t i = 0; i < node_count; ++i) {
		if(!fstrwrite("\t\t\"", stream) || !fstrwrite(((node_t *)nodes[i])->name, stream) || !fstrwrite("\": {\n", stream)) {
			goto fail;
		}

		if(!fstrwrite("\t\t\t\"name\": \"", stream) || !fstrwrite(((node_t *)nodes[i])->name, stream) || !fstrwrite("\",\n", stream)) {
			goto fail;
		}

		snprintf(buf, sizeof(buf), "%d", ((node_t *)nodes[i])->devclass);

		if(!fstrwrite("\t\t\t\"devclass\": ", stream) || !fstrwrite(buf, stream) || !fstrwrite("\n", stream)) {
			goto fail;
		}

		if(!fstrwrite((i + 1) != node_count ? "\t\t},\n" : "\t\t}\n", stream)) {
			goto fail;
		}
	}

	if(!fstrwrite("\t},\n", stream)) {
		goto fail;
	}

	// export edges

	if(!fstrwrite("\t\"edges\": {\n", stream)) {
		goto fail;
	}

	for(size_t i = 0; i < edge_count; ++i) {
		if(!fstrwrite("\t\t\"", stream) || !fstrwrite(edges[i].from->name, stream) || !fstrwrite("_to_", stream) || !fstrwrite(edges[i].to->name, stream) || !fstrwrite("\": {\n", stream)) {
			goto fail;
		}

		if(!fstrwrite("\t\t\t\"from\": \"", stream) || !fstrwrite(edges[i].from->name, stream) || !fstrwrite("\",\n", stream)) {
			goto fail;
		}

		if(!fstrwrite("\t\t\t\"to\": \"", stream) || !fstrwrite(edges[i].to->name, stream) || !fstrwrite("\",\n", stream)) {
			goto fail;
		}

		char *host = NULL, *port = NULL, *address = NULL;
		sockaddr2str((const sockaddr_t *)&edges[i].address, &host, &port);

		if(host && port) {
			xasprintf(&address, "{ \"host\": \"%s\", \"port\": %s }", host, port);
		}

		free(host);
		free(port);

		if(!fstrwrite("\t\t\t\"address\": ", stream) || !fstrwrite(address ? address : "null", stream) || !fstrwrite(",\n", stream)) {
			free(address);
			goto fail;
		}

		free(address);

		snprintf(buf, sizeof(buf), "%d", edges[i].weight);

		if(!fstrwrite("\t\t\t\"weight\": ", stream) || !fstrwrite(buf, stream) || !fstrwrite("\n", stream)) {
			goto fail;
		}

		if(!fstrwrite((i + 1) != edge_count ? "\t\t},\n" : "\t\t}\n", stream)) {
			goto fail;
		}
	}

	if(!fstrwrite("\t}\n", stream)) {
		goto fail;
	}

	// DONE!

	if(!fstrwrite("}", stream)) {
		goto fail;
	}

	goto done;

fail:
	result = false;

done:
	free(nodes);
	free(edges);

	pthread_mutex_unlock(&mesh->mutex);

	return result;
}

static void devtool_get_reset_node_status(meshlink_handle_t *mesh, meshlink_node_t *node, devtool_node_status_t *status, bool reset) {
	node_t *internal = (node_t *)node;

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	if(status) {
		memcpy(&status->status, &internal->status, sizeof status->status);
		memcpy(&status->address, &internal->address, sizeof status->address);
		status->mtu = internal->mtu;
		status->minmtu = internal->minmtu;
		status->maxmtu = internal->maxmtu;
		status->mtuprobes = internal->mtuprobes;
		status->in_data = internal->in_data;
		status->out_data = internal->out_data;
		status->in_forward = internal->in_forward;
		status->out_forward = internal->out_forward;
		status->in_meta = internal->in_meta;
		status->out_meta = internal->out_meta;

		// Derive UDP connection status
		if(internal == mesh->self) {
			status->udp_status = DEVTOOL_UDP_WORKING;
		} else if(!internal->status.reachable) {
			status->udp_status = DEVTOOL_UDP_IMPOSSIBLE;
		} else if(!internal->status.validkey) {
			status->udp_status = DEVTOOL_UDP_UNKNOWN;
		} else if(internal->status.udp_confirmed) {
			status->udp_status = DEVTOOL_UDP_WORKING;
		} else if(internal->mtuprobes > 30) {
			status->udp_status = DEVTOOL_UDP_FAILED;
		} else if(internal->mtuprobes > 0) {
			status->udp_status = DEVTOOL_UDP_TRYING;
		} else {
			status->udp_status = DEVTOOL_UDP_UNKNOWN;
		}
	}

	if(reset) {
		internal->in_data = 0;
		internal->out_data = 0;
		internal->in_forward = 0;
		internal->out_forward = 0;
		internal->in_meta = 0;
		internal->out_meta = 0;
	}

	pthread_mutex_unlock(&mesh->mutex);
}

void devtool_get_node_status(meshlink_handle_t *mesh, meshlink_node_t *node, devtool_node_status_t *status) {
	if(!mesh || !node || !status) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	devtool_get_reset_node_status(mesh, node, status, false);
}

void devtool_reset_node_counters(meshlink_handle_t *mesh, meshlink_node_t *node, devtool_node_status_t *status) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	devtool_get_reset_node_status(mesh, node, status, true);
}

meshlink_submesh_t **devtool_get_all_submeshes(meshlink_handle_t *mesh, meshlink_submesh_t **submeshes, size_t *nmemb) {
	if(!mesh || !nmemb || (*nmemb && !submeshes)) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	meshlink_submesh_t **result;

	//lock mesh->nodes
	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	*nmemb = mesh->submeshes->count;
	result = realloc(submeshes, *nmemb * sizeof(*submeshes));

	if(result) {
		meshlink_submesh_t **p = result;

		for list_each(submesh_t, s, mesh->submeshes) {
			*p++ = (meshlink_submesh_t *)s;
		}
	} else {
		*nmemb = 0;
		free(submeshes);
		meshlink_errno = MESHLINK_ENOMEM;
	}

	pthread_mutex_unlock(&mesh->mutex);

	return result;
}

meshlink_handle_t *devtool_open_in_netns(const char *confbase, const char *name, const char *appname, dev_class_t devclass, int netns) {
	meshlink_open_params_t *params = meshlink_open_params_init(confbase, name, appname, devclass);
	params->netns = dup(netns);
	meshlink_handle_t *handle;

	if(params->netns == -1) {
		handle = NULL;
		meshlink_errno = MESHLINK_EINVAL;
	} else {
		handle = meshlink_open_ex(params);
	}

	meshlink_open_params_free(params);

	return handle;
}

void devtool_force_sptps_renewal(meshlink_handle_t *mesh, meshlink_node_t *node) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	node_t *n = (node_t *)node;
	connection_t *c = n->connection;

	n->last_req_key = -3600;

	if(c) {
		c->last_key_renewal = -3600;
	}
}

void devtool_set_meta_status_cb(meshlink_handle_t *mesh, meshlink_node_status_cb_t cb) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->meta_status_cb = cb;
	pthread_mutex_unlock(&mesh->mutex);
}

void devtool_set_global_metering_cb(meshlink_handle_t *mesh, meshlink_global_metering_cb_t cb, uint64_t threshold, int timeout) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
	}

	if(pthread_mutex_lock(&mesh->mutex) != 0) {
		abort();
	}

	mesh->global_metering_cb = cb;
	mesh->metering_threshold = threshold;
	mesh->metering_timeout = timeout;
	pthread_mutex_unlock(&mesh->mutex);
}

void check_global_metering(meshlink_handle_t *mesh) {
	uint64_t sum =
	        mesh->self->in_data + mesh->self->out_data +
	        mesh->self->in_forward + mesh->self->out_forward +
	        mesh->self->in_meta + mesh->self->out_meta;

	if(sum >= mesh->metering_threshold || mesh->loop.now.tv_sec >= mesh->last_metering_cb + mesh->metering_timeout) {
		devtool_node_status_t status;
		memset(&status, 0, sizeof status);
		status.in_data = mesh->self->in_data;
		status.out_data = mesh->self->out_data;
		status.in_forward = mesh->self->in_forward;
		status.out_forward = mesh->self->out_forward;
		status.in_meta = mesh->self->in_meta;
		status.out_meta = mesh->self->out_meta;

		mesh->global_metering_cb(mesh, &status);

		mesh->self->in_data = 0;
		mesh->self->out_data = 0;
		mesh->self->in_forward = 0;
		mesh->self->out_forward = 0;
		mesh->self->in_meta = 0;
		mesh->self->out_meta = 0;
		mesh->last_metering_cb = mesh->loop.now.tv_sec;
	}
}