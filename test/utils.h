#ifndef MESHLINK_TEST_UTILS_H
#define MESHLINK_TEST_UTILS_H

#include "../src/meshlink.h"

// Simple synchronisation between threads
struct sync_flag {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool flag;
};

extern void set_sync_flag(struct sync_flag *s);
extern bool wait_sync_flag(struct sync_flag *s, int seconds);

/// Create a pair of meshlink instances that are already joined together.
extern void open_meshlink_pair(meshlink_handle_t **a, meshlink_handle_t **b, const char *prefix);

/// Start a pair of meshlink instances and wait for them to connect together.
extern void start_meshlink_pair(meshlink_handle_t *a, meshlink_handle_t *b);

/// Stop a pair of meshlink instances.
extern void stop_meshlink_pair(meshlink_handle_t *a, meshlink_handle_t *b);

/// Stop and cleanup a pair of meshlink instances.
extern void close_meshlink_pair(meshlink_handle_t *a, meshlink_handle_t *b, const char *prefix);

#define assert_after(cond, timeout)\
	do {\
		for(int i = 0; i++ <= timeout;) {\
			if(cond)\
				break;\
			if(i == timeout)\
				assert(cond);\
			sleep(1);\
		}\
	} while(0)
#endif
