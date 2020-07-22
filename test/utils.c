#define _GNU_SOURCE 1

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include "utils.h"

void init_sync_flag(struct sync_flag *s) {
	assert(pthread_mutex_init(&s->mutex, NULL) == 0);
	assert(pthread_cond_init(&s->cond, NULL) == 0);
}

void set_sync_flag(struct sync_flag *s, bool value) {
	assert(pthread_mutex_lock(&s->mutex) == 0);
	s->flag = value;
	assert(pthread_cond_broadcast(&s->cond) == 0);
	assert(pthread_mutex_unlock(&s->mutex) == 0);
}

void reset_sync_flag(struct sync_flag *s) {
	assert(pthread_mutex_lock(&s->mutex) == 0);
	s->flag = false;
	assert(pthread_mutex_unlock(&s->mutex) == 0);
}

bool check_sync_flag(struct sync_flag *s) {
	bool flag;
	assert(pthread_mutex_lock(&s->mutex) == 0);
	flag = s->flag;
	assert(pthread_mutex_unlock(&s->mutex) == 0);
	return flag;
}

bool wait_sync_flag(struct sync_flag *s, int seconds) {
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += seconds;

	assert(pthread_mutex_lock(&s->mutex) == 0);

	while(!s->flag) {
		if(!pthread_cond_timedwait(&s->cond, &s->mutex, &timeout) || errno != EINTR) {
			break;
		}
	}

	assert(pthread_mutex_unlock(&s->mutex) == 0);

	return s->flag;
}

void link_meshlink_pair(meshlink_handle_t *a, meshlink_handle_t *b) {
	// Import and export both side's data

	assert(meshlink_set_canonical_address(a, meshlink_get_self(a), "localhost", NULL));
	assert(meshlink_set_canonical_address(b, meshlink_get_self(b), "localhost", NULL));

	char *data = meshlink_export(a);
	assert(data);
	assert(meshlink_import(b, data));
	free(data);

	data = meshlink_export(b);
	assert(data);
	assert(meshlink_import(a, data));
	free(data);
}

void open_meshlink_pair(meshlink_handle_t **pa, meshlink_handle_t **pb, const char *prefix) {
	// Create two new MeshLink instances

	*pa = *pb = NULL;

	char *a_name, *b_name;

	assert(asprintf(&a_name, "%s_conf.1", prefix) > 0);
	assert(a_name);

	assert(asprintf(&b_name, "%s_conf.2", prefix) > 0);
	assert(b_name);

	assert(meshlink_destroy(a_name));
	assert(meshlink_destroy(b_name));

	meshlink_handle_t *a = meshlink_open(a_name, "a", prefix, DEV_CLASS_BACKBONE);
	assert(a);

	meshlink_handle_t *b = meshlink_open(b_name, "b", prefix, DEV_CLASS_BACKBONE);
	assert(b);

	free(a_name);
	free(b_name);

	meshlink_enable_discovery(a, false);
	meshlink_enable_discovery(b, false);

	link_meshlink_pair(a, b);

	*pa = a;
	*pb = b;
}

// Don't poll in the application thread, use a condition variable to signal when the peer is online.
static void pair_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)node;

	if(reachable) {
		set_sync_flag(mesh->priv, true);
	}
}

void start_meshlink_pair(meshlink_handle_t *a, meshlink_handle_t *b) {
	struct sync_flag pair_status = {.flag = false};
	init_sync_flag(&pair_status);

	a->priv = &pair_status;
	meshlink_set_node_status_cb(a, pair_status_cb);

	assert(meshlink_start(a));
	assert(meshlink_start(b));

	assert(wait_sync_flag(&pair_status, 5));

	meshlink_set_node_status_cb(a, NULL);
	a->priv = NULL;
}

void stop_meshlink_pair(meshlink_handle_t *a, meshlink_handle_t *b) {
	meshlink_stop(a);
	meshlink_stop(b);
}

void close_meshlink_pair(meshlink_handle_t *a, meshlink_handle_t *b) {
	meshlink_close(a);
	meshlink_close(b);
}

void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	static const char *levelstr[] = {
		[MESHLINK_DEBUG] = "DEBUG",
		[MESHLINK_INFO] = "INFO",
		[MESHLINK_WARNING] = "WARNING",
		[MESHLINK_ERROR] = "ERROR",
		[MESHLINK_CRITICAL] = "CRITICAL",
	};

	static struct timespec ts0;
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	if(ts0.tv_sec == 0) {
		ts0 = ts;
	}

	float diff = (ts.tv_sec - ts0.tv_sec) + (ts.tv_nsec - ts0.tv_nsec) * 1e-9;

	fprintf(stderr, "%7.3f (%s) [%s] %s\n",
	        diff,
	        mesh ? mesh->name : "",
	        levelstr[level],
	        text);
}
