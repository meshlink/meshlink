/*
    meshlink.c -- Implementation of the MeshLink API.
    Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>

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
#include <pthread.h>

#include "crypto.h"
#include "ecdsagen.h"
#include "meshlink_internal.h"
#include "node.h"
#include "protocol.h"
#include "route.h"
#include "xalloc.h"

meshlink_handle_t *mesh;

static const char *errstr[] = {
	[MESHLINK_OK] = "No error",
	[MESHLINK_ENOMEM] = "Out of memory",
	[MESHLINK_ENOENT] = "No such node",
};

const char *meshlink_strerror(meshlink_errno_t errno) {
	return errstr[errno];
}

// TODO: hack, remove once all global variables are gone.
static void set_mesh(meshlink_handle_t *localmesh) {
	mesh = localmesh;
}

static bool ecdsa_keygen(meshlink_handle_t *mesh) {
	ecdsa_t *key;
	FILE *f;
	char pubname[PATH_MAX], privname[PATH_MAX];

	fprintf(stderr, "Generating ECDSA keypair:\n");

	if(!(key = ecdsa_generate())) {
		fprintf(stderr, "Error during key generation!\n");
		return false;
	} else
		fprintf(stderr, "Done.\n");

	snprintf(privname, sizeof privname, "%s" SLASH "ecdsa_key.priv", mesh->confbase);
	f = fopen(privname, "w");

	if(!f)
		return false;

#ifdef HAVE_FCHMOD
	fchmod(fileno(f), 0600);
#endif

	if(!ecdsa_write_pem_private_key(key, f)) {
		fprintf(stderr, "Error writing private key!\n");
		ecdsa_free(key);
		fclose(f);
		return false;
	}

	fclose(f);


	snprintf(pubname, sizeof pubname, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, mesh->name);
	f = fopen(pubname, "a");

	if(!f)
		return false;

	char *pubkey = ecdsa_get_base64_public_key(key);
	fprintf(f, "ECDSAPublicKey = %s\n", pubkey);
	free(pubkey);

	fclose(f);
	ecdsa_free(key);

	return true;
}

static bool try_bind(int port) {
	struct addrinfo *ai = NULL;
	struct addrinfo hint = {
		.ai_flags = AI_PASSIVE,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP,
	};

	char portstr[16];
	snprintf(portstr, sizeof portstr, "%d", port);

	if(getaddrinfo(NULL, portstr, &hint, &ai) || !ai)
		return false;

	while(ai) {
		int fd = socket(ai->ai_family, SOCK_STREAM, IPPROTO_TCP);
		if(!fd)
			return false;
		int result = bind(fd, ai->ai_addr, ai->ai_addrlen);
		closesocket(fd);
		if(result)
			return false;
		ai = ai->ai_next;
	}

	return true;
}

static int check_port(meshlink_handle_t *mesh) {
	if(try_bind(655))
		return 655;

	fprintf(stderr, "Warning: could not bind to port 655.\n");

	for(int i = 0; i < 100; i++) {
		int port = 0x1000 + (rand() & 0x7fff);
		if(try_bind(port)) {
			char filename[PATH_MAX];
			snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, mesh->name);
			FILE *f = fopen(filename, "a");
			if(!f) {
				fprintf(stderr, "Please change MeshLink's Port manually.\n");
				return 0;
			}

			fprintf(f, "Port = %d\n", port);
			fclose(f);
			fprintf(stderr, "MeshLink will instead listen on port %d.\n", port);
			return port;
		}
	}

	fprintf(stderr, "Please change MeshLink's Port manually.\n");
	return 0;
}

static bool meshlink_setup(meshlink_handle_t *mesh) {
	char meshlink_conf[PATH_MAX];
	char hosts_dir[PATH_MAX];

	if(mkdir(mesh->confbase, 0777) && errno != EEXIST) {
		fprintf(stderr, "Could not create directory %s: %s\n", mesh->confbase, strerror(errno));
		return false;
	}

	snprintf(hosts_dir, sizeof hosts_dir, "%s" SLASH "hosts", mesh->confbase);

	if(mkdir(hosts_dir, 0777) && errno != EEXIST) {
		fprintf(stderr, "Could not create directory %s: %s\n", hosts_dir, strerror(errno));
		return false;
	}

	snprintf(meshlink_conf, sizeof meshlink_conf, "%s" SLASH "meshlink.conf", mesh->confbase);

	if(!access(meshlink_conf, F_OK)) {
		fprintf(stderr, "Configuration file %s already exists!\n", meshlink_conf);
		return false;
	}

	FILE *f = fopen(meshlink_conf, "w");
	if(!f) {
		fprintf(stderr, "Could not create file %s: %s\n", meshlink_conf, strerror(errno));
		return 1;
	}

	fprintf(f, "Name = %s\n", mesh->name);
	fclose(f);

	if(!ecdsa_keygen(mesh))
		return false;

	check_port(mesh);

	return true;
}

meshlink_handle_t *meshlink_open(const char *confbase, const char *name) {
	// Validate arguments provided by the application

	if(!confbase || !*confbase) {
		fprintf(stderr, "No confbase given!\n");
		return NULL;
	}

	if(!name || !*name) {
		fprintf(stderr, "No name given!\n");
		return NULL;
	}

	if(!check_id(name)) {
		fprintf(stderr, "Invalid name given!\n");
		return NULL;
	}

	meshlink_handle_t *mesh = xzalloc(sizeof *mesh);
	mesh->confbase = xstrdup(confbase);
	mesh->name = xstrdup(name);
	event_loop_init(&mesh->loop);
	mesh->loop.data = mesh;
	set_mesh(mesh);

	// TODO: should be set by a function.
	mesh->debug_level = 5;

	// Check whether meshlink.conf already exists

	char filename[PATH_MAX];
	snprintf(filename, sizeof filename, "%s" SLASH "meshlink.conf", confbase);

	if(access(filename, R_OK)) {
		if(errno == ENOENT) {
			// If not, create it
			meshlink_setup(mesh);
		} else {
			fprintf(stderr, "Cannot not read from %s: %s\n", filename, strerror(errno));
			return meshlink_close(mesh), NULL;
		}
	}

	// Read the configuration

	init_configuration(&mesh->config);

	if(!read_server_config(mesh))
		return meshlink_close(mesh), NULL;

	// Setup up everything
	// TODO: we should not open listening sockets yet

	if(!setup_network(mesh))
		return meshlink_close(mesh), NULL;

	return mesh;
}

void *meshlink_main_loop(void *arg) {
	meshlink_handle_t *mesh = arg;

	try_outgoing_connections(mesh);

	main_loop();

	return NULL;
}

bool meshlink_start(meshlink_handle_t *mesh) {
	// TODO: open listening sockets first

	// Start the main thread

	if(pthread_create(&mesh->thread, NULL, meshlink_main_loop, mesh) != 0) {
		fprintf(stderr, "Could not start thread: %s\n", strerror(errno));
		memset(&mesh->thread, 0, sizeof mesh->thread);
		return false;
	}

	return true;
}

void meshlink_stop(meshlink_handle_t *mesh) {
	// TODO: close the listening sockets to signal the main thread to shut down

	// Wait for the main thread to finish

	pthread_join(mesh->thread, NULL);
}

void meshlink_close(meshlink_handle_t *mesh) {
	// Close and free all resources used.

	close_network_connections();

	logger(DEBUG_ALWAYS, LOG_NOTICE, "Terminating");

	exit_configuration(&mesh->config);
	event_loop_exit(&mesh->loop);
}

void meshlink_set_receive_cb(meshlink_handle_t *mesh, meshlink_receive_cb_t cb) {
	mesh->receive_cb = cb;
}

void meshlink_set_node_status_cb(meshlink_handle_t *mesh, meshlink_node_status_cb_t cb) {
	mesh->node_status_cb = cb;
}

void meshlink_set_log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, meshlink_log_cb_t cb) {
	mesh->log_cb = cb;
	mesh->log_level = level;
}

bool meshlink_send(meshlink_handle_t *mesh, meshlink_node_t *destination, const void *data, unsigned int len) {
	vpn_packet_t packet;
	meshlink_packethdr_t *hdr = (meshlink_packethdr_t *)packet.data;
	if (sizeof(meshlink_packethdr_t) + len > MAXSIZE) {
		//log something
		return false;
	}

	packet.probe = false;
	memset(hdr, 0, sizeof *hdr);
	memcpy(hdr->destination, destination->name, sizeof hdr->destination);
	memcpy(hdr->source, mesh->self->name, sizeof hdr->source);

	packet.len = sizeof *hdr + len;
	memcpy(packet.data + sizeof *hdr, data, len);

        mesh->self->in_packets++;
        mesh->self->in_bytes += packet.len;
        route(mesh->self, &packet);
	return false;
}

meshlink_node_t *meshlink_get_node(meshlink_handle_t *mesh, const char *name) {
	return (meshlink_node_t *)lookup_node(name);
	return NULL;
}

size_t meshlink_get_all_nodes(meshlink_handle_t *mesh, meshlink_node_t **nodes, size_t nmemb) {
	return 0;
}

char *meshlink_sign(meshlink_handle_t *mesh, const char *data, size_t len) {
	return NULL;
}

bool meshlink_verify(meshlink_handle_t *mesh, meshlink_node_t *source, const char *data, size_t len, const char *signature) {
	return false;
}

char *meshlink_invite(meshlink_handle_t *mesh, const char *name) {
	return NULL;
}

bool meshlink_join(meshlink_handle_t *mesh, const char *invitation) {
	return false;
}

char *meshlink_export(meshlink_handle_t *mesh) {
	return NULL;
}

bool meshlink_import(meshlink_handle_t *mesh, const char *data) {
	return false;
}

void meshlink_blacklist(meshlink_handle_t *mesh, meshlink_node_t *node) {
}

static void __attribute__((constructor)) meshlink_init(void) {
	crypto_init();
}

static void __attribute__((destructor)) meshlink_exit(void) {
	crypto_exit();
}
