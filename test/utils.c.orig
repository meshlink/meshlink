#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

#include "utils.h"

void set_sync_flag(struct sync_flag *s) {
	pthread_mutex_lock(&s->mutex);
	s->flag = true;
	pthread_cond_broadcast(&s->cond);
	pthread_mutex_unlock(&s->mutex);
}

bool wait_sync_flag(struct sync_flag *s, int seconds) {
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += seconds;
  pthread_mutex_lock(&s->mutex);
	while(!s->flag)
		if(!pthread_cond_timedwait(&s->cond, &s->mutex, &timeout)) {
			break;
		}
  pthread_mutex_unlock(&s->mutex);
	return s->flag;
}

void open_meshlink_pair(meshlink_handle_t **pa, meshlink_handle_t **pb, const char *prefix) {
	// Create two new MeshLink instances

	*pa = *pb = NULL;

	char *a_name, *b_name;

	asprintf(&a_name, "%s_conf.1", prefix);
	assert(a_name);

	asprintf(&b_name, "%s_conf.2", prefix);
	assert(b_name);

	meshlink_handle_t *a = meshlink_open(a_name, "a", prefix, DEV_CLASS_BACKBONE);
	assert(a);

	meshlink_handle_t *b = meshlink_open(b_name, "b", prefix, DEV_CLASS_BACKBONE);
	assert(b);

	meshlink_enable_discovery(a, false);
	meshlink_enable_discovery(b, false);

	// Import and export both side's data

	meshlink_add_address(a, "localhost");

	char *data = meshlink_export(a);
	assert(data);
	assert(meshlink_import(b, data));
	free(data);

	data = meshlink_export(b);
	assert(data);
	assert(meshlink_import(a, data));
	free(data);

	*pa = a;
	*pb = b;
}

// Don't poll in the application thread, use a condition variable to signal when the peer is online.
static void pair_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	set_sync_flag(mesh->priv);
}

void start_meshlink_pair(meshlink_handle_t *a, meshlink_handle_t *b) {
	struct sync_flag pair_status = {};

	a->priv = &pair_status;
	meshlink_set_node_status_cb(a, pair_status_cb);

	pthread_mutex_lock(&pair_status.mutex);

	meshlink_start(a);
	meshlink_start(b);

	assert(wait_sync_flag(&pair_status, 5));

	pthread_mutex_unlock(&pair_status.mutex);

	meshlink_set_node_status_cb(a, NULL);
	a->priv = NULL;
}

void stop_meshlink_pair(meshlink_handle_t *a, meshlink_handle_t *b) {
	meshlink_stop(a);
	meshlink_stop(b);
}

void close_meshlink_pair(meshlink_handle_t *a, meshlink_handle_t *b, const char *prefix) {
	meshlink_close(a);
	meshlink_close(b);

	if(prefix) {
		char *a_name, *b_name;

		asprintf(&a_name, "%s_conf.1", prefix);
		assert(a_name);
		assert(meshlink_destroy(a_name));

		asprintf(&b_name, "%s_conf.2", prefix);
		assert(b_name);
		assert(meshlink_destroy(b_name));
	}
}
