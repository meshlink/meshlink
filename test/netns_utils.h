#ifndef MESHLINK_TEST_NETNS_UTILS_H
#define MESHLINK_TEST_NETNS_UTILS_H

typedef struct peer_config {
	const char *name;
	const dev_class_t devclass;

	char *netns_name;
	int netns;
	meshlink_handle_t *mesh;
} peer_config_t;

extern void change_peer_ip(peer_config_t *peer);
extern peer_config_t *setup_relay_peer_nut(const char *prefix);
extern peer_config_t *setup_relay_peer_nut_indirect(const char *prefix);
extern void close_relay_peer_nut(peer_config_t *peers);

#endif
