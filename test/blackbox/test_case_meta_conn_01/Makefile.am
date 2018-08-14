check_PROGRAMS = node_sim_relay node_sim_peer node_sim_nut 

node_sim_peer_SOURCES = node_sim_peer.c ../common/common_handlers.c ../common/test_step.c ../common/mesh_event_handler.c
node_sim_peer_LDADD = ../../../src/libmeshlink.la
node_sim_peer_CFLAGS = -D_GNU_SOURCE

node_sim_relay_SOURCES = node_sim_relay.c ../common/common_handlers.c ../common/test_step.c ../common/mesh_event_handler.c
node_sim_relay_LDADD = ../../../src/libmeshlink.la
node_sim_relay_CFLAGS = -D_GNU_SOURCE

node_sim_nut_SOURCES = node_sim_nut.c ../common/common_handlers.c ../common/test_step.c ../common/mesh_event_handler.c
node_sim_nut_LDADD = ../../../src/libmeshlink.la
node_sim_nut_CFLAGS = -D_GNU_SOURCE
