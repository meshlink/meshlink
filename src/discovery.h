
#ifndef __MESHLINK_DISCOVERY_H__
#define __MESHLINK_DISCOVERY_H__

#include <stdbool.h>

extern bool discovery_start(meshlink_handle_t *mesh);
extern void discovery_stop(meshlink_handle_t *mesh);

#endif
