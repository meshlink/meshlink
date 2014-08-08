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
#define VAR_SERVER 1    /* Should be in meshlink.conf */
#define VAR_HOST 2      /* Can be in host config file */
#define VAR_MULTIPLE 4  /* Multiple statements allowed */
#define VAR_OBSOLETE 8  /* Should not be used anymore */
#define VAR_SAFE 16     /* Variable is safe when accepting invitations */
#define MAX_ADDRESS_LENGTH 45 /* Max length of an (IPv6) address */
#define MAX_PORT_LENGTH 5 /* 0-65535 */
typedef struct {
	const char *name;
	int type;
} var_t;

#include "system.h"
#include <pthread.h>

#include "crypto.h"
#include "ecdsagen.h"
#include "meshlink_internal.h"
#include "netutl.h"
#include "node.h"
#include "protocol.h"
#include "route.h"
#include "utils.h"
#include "xalloc.h"
#include "ed25519/sha512.h"
#include "discovery.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

__thread meshlink_errno_t meshlink_errno;

//TODO: this can go away completely
const var_t variables[] = {
	/* Server configuration */
	{"AddressFamily", VAR_SERVER},
	{"AutoConnect", VAR_SERVER | VAR_SAFE},
	{"BindToAddress", VAR_SERVER | VAR_MULTIPLE},
	{"BindToInterface", VAR_SERVER},
	{"Broadcast", VAR_SERVER | VAR_SAFE},
	{"ConnectTo", VAR_SERVER | VAR_MULTIPLE | VAR_SAFE},
	{"DecrementTTL", VAR_SERVER},
	{"Device", VAR_SERVER},
	{"DeviceType", VAR_SERVER},
	{"DirectOnly", VAR_SERVER},
	{"ECDSAPrivateKeyFile", VAR_SERVER},
	{"ExperimentalProtocol", VAR_SERVER},
	{"Forwarding", VAR_SERVER},
	{"GraphDumpFile", VAR_SERVER | VAR_OBSOLETE},
	{"Hostnames", VAR_SERVER},
	{"IffOneQueue", VAR_SERVER},
	{"Interface", VAR_SERVER},
	{"KeyExpire", VAR_SERVER},
	{"ListenAddress", VAR_SERVER | VAR_MULTIPLE},
	{"LocalDiscovery", VAR_SERVER},
	{"MACExpire", VAR_SERVER},
	{"MaxConnectionBurst", VAR_SERVER},
	{"MaxOutputBufferSize", VAR_SERVER},
	{"MaxTimeout", VAR_SERVER},
	{"Mode", VAR_SERVER | VAR_SAFE},
	{"Name", VAR_SERVER},
	{"PingInterval", VAR_SERVER},
	{"PingTimeout", VAR_SERVER},
	{"PriorityInheritance", VAR_SERVER},
	{"PrivateKey", VAR_SERVER | VAR_OBSOLETE},
	{"PrivateKeyFile", VAR_SERVER},
	{"ProcessPriority", VAR_SERVER},
	{"Proxy", VAR_SERVER},
	{"ReplayWindow", VAR_SERVER},
	{"ScriptsExtension", VAR_SERVER},
	{"ScriptsInterpreter", VAR_SERVER},
	{"StrictSubnets", VAR_SERVER},
	{"TunnelServer", VAR_SERVER},
	{"VDEGroup", VAR_SERVER},
	{"VDEPort", VAR_SERVER},
	/* Host configuration */
	{"Address", VAR_HOST | VAR_MULTIPLE},
	{"Cipher", VAR_SERVER | VAR_HOST},
	{"ClampMSS", VAR_SERVER | VAR_HOST},
	{"Compression", VAR_SERVER | VAR_HOST},
	{"Digest", VAR_SERVER | VAR_HOST},
	{"ECDSAPublicKey", VAR_HOST},
	{"ECDSAPublicKeyFile", VAR_SERVER | VAR_HOST},
	{"IndirectData", VAR_SERVER | VAR_HOST},
	{"MACLength", VAR_SERVER | VAR_HOST},
	{"PMTU", VAR_SERVER | VAR_HOST},
	{"PMTUDiscovery", VAR_SERVER | VAR_HOST},
	{"Port", VAR_HOST},
	{"PublicKey", VAR_HOST | VAR_OBSOLETE},
	{"PublicKeyFile", VAR_SERVER | VAR_HOST | VAR_OBSOLETE},
	{"Subnet", VAR_HOST | VAR_MULTIPLE | VAR_SAFE},
	{"TCPOnly", VAR_SERVER | VAR_HOST},
	{"Weight", VAR_HOST | VAR_SAFE},
	{NULL, 0}
};

static bool fcopy(FILE *out, const char *filename) {
	FILE *in = fopen(filename, "r");
	if(!in) {
		fprintf(stderr, "Could not open %s: %s\n", filename, strerror(errno));
		return false;
	}

	char buf[1024];
	size_t len;
	while((len = fread(buf, 1, sizeof buf, in)))
		fwrite(buf, len, 1, out);
	fclose(in);
	return true;
}

static int rstrip(char *value) {
	int len = strlen(value);
	while(len && strchr("\t\r\n ", value[len - 1]))
		value[--len] = 0;
	return len;
}

static void scan_for_hostname(const char *filename, char **hostname, char **port) {
	char line[4096];
	if(!filename || (*hostname && *port))
		return;

	FILE *f = fopen(filename, "r");
	if(!f)
		return;

	while(fgets(line, sizeof line, f)) {
		if(!rstrip(line))
			continue;
		char *p = line, *q;
		p += strcspn(p, "\t =");
		if(!*p)
			continue;
		q = p + strspn(p, "\t ");
		if(*q == '=')
			q += 1 + strspn(q + 1, "\t ");
		*p = 0;
		p = q + strcspn(q, "\t ");
		if(*p)
			*p++ = 0;
		p += strspn(p, "\t ");
		p[strcspn(p, "\t ")] = 0;

		if(!*port && !strcasecmp(line, "Port")) {
			*port = xstrdup(q);
		} else if(!*hostname && !strcasecmp(line, "Address")) {
			*hostname = xstrdup(q);
			if(*p) {
				free(*port);
				*port = xstrdup(p);
			}
		}

		if(*hostname && *port)
			break;
	}

	fclose(f);
}
static char *get_my_hostname(meshlink_handle_t* mesh) {
	char *hostname = NULL;
	char *port = NULL;
	char *hostport = NULL;
	char *name = mesh->self->name;
	char filename[PATH_MAX] = "";
	char line[4096];
	FILE *f;

	// Use first Address statement in own host config file
	snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, name);
	scan_for_hostname(filename, &hostname, &port);

	if(hostname)
		goto done;

	// If that doesn't work, guess externally visible hostname
	fprintf(stderr, "Trying to discover externally visible hostname...\n");
	struct addrinfo *ai = str2addrinfo("meshlink.io", "80", SOCK_STREAM);
	struct addrinfo *aip = ai;
	static const char request[] = "GET http://www.meshlink.io/host.cgi HTTP/1.0\r\n\r\n";

	while(aip) {
		int s = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
		if(s >= 0) {
			if(connect(s, aip->ai_addr, aip->ai_addrlen)) {
				closesocket(s);
				s = -1;
			}
		}
		if(s >= 0) {
			send(s, request, sizeof request - 1, 0);
			int len = recv(s, line, sizeof line - 1, MSG_WAITALL);
			if(len > 0) {
				line[len] = 0;
				if(line[len - 1] == '\n')
					line[--len] = 0;
				char *p = strrchr(line, '\n');
				if(p && p[1])
					hostname = xstrdup(p + 1);
			}
			closesocket(s);
			if(hostname)
				break;
		}
		aip = aip->ai_next;
		continue;
	}

	if(ai)
		freeaddrinfo(ai);

	// Check that the hostname is reasonable
	if(hostname) {
		for(char *p = hostname; *p; p++) {
			if(isalnum(*p) || *p == '-' || *p == '.' || *p == ':')
				continue;
			// If not, forget it.
			free(hostname);
			hostname = NULL;
			break;
		}
	}

	if(!hostname)
		return NULL;

	f = fopen(filename, "a");
	if(f) {
		fprintf(f, "\nAddress = %s\n", hostname);
		fclose(f);
	} else {
		fprintf(stderr, "Could not append Address to %s: %s\n", filename, strerror(errno));
	}

done:
	if(port) {
		if(strchr(hostname, ':'))
			xasprintf(&hostport, "[%s]:%s", hostname, port);
		else
			xasprintf(&hostport, "%s:%s", hostname, port);
	} else {
		if(strchr(hostname, ':'))
			xasprintf(&hostport, "[%s]", hostname);
		else
			hostport = xstrdup(hostname);
	}

	free(hostname);
	free(port);
	return hostport;
}

static char *get_line(const char **data) {
	if(!data || !*data)
		return NULL;

	if(!**data) {
		*data = NULL;
		return NULL;
	}

	static char line[1024];
	const char *end = strchr(*data, '\n');
	size_t len = end ? end - *data : strlen(*data);
	if(len >= sizeof line) {
		fprintf(stderr, "Maximum line length exceeded!\n");
		return NULL;
	}
	if(len && !isprint(**data))
		abort();

	memcpy(line, *data, len);
	line[len] = 0;

	if(end)
		*data = end + 1;
	else
		*data = NULL;

	return line;
}

static char *get_value(const char *data, const char *var) {
	char *line = get_line(&data);
	if(!line)
		return NULL;

	char *sep = line + strcspn(line, " \t=");
	char *val = sep + strspn(sep, " \t");
	if(*val == '=')
		val += 1 + strspn(val + 1, " \t");
	*sep = 0;
	if(strcasecmp(line, var))
		return NULL;
	return val;
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
		if(!fd) {
			freeaddrinfo(ai);
			return false;
		}
		int result = bind(fd, ai->ai_addr, ai->ai_addrlen);
		closesocket(fd);
		if(result) {
			freeaddrinfo(ai);
			return false;
		}
		ai = ai->ai_next;
	}

	freeaddrinfo(ai);
	return true;
}

static int check_port(meshlink_handle_t *mesh) {
	for(int i = 0; i < 1000; i++) {
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
			return port;
		}
	}

	fprintf(stderr, "Please change MeshLink's Port manually.\n");
	return 0;
}

static bool finalize_join(meshlink_handle_t *mesh) {
	char *name = xstrdup(get_value(mesh->data, "Name"));
	if(!name) {
		fprintf(stderr, "No Name found in invitation!\n");
		return false;
	}

	if(!check_id(name)) {
		fprintf(stderr, "Invalid Name found in invitation: %s!\n", name);
		return false;
	}

	char filename[PATH_MAX];
	snprintf(filename, sizeof filename, "%s" SLASH "meshlink.conf", mesh->confbase);

	FILE *f = fopen(filename, "w");
	if(!f) {
		fprintf(stderr, "Could not create file %s: %s\n", filename, strerror(errno));
		return false;
	}

	fprintf(f, "Name = %s\n", name);

	snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, name);
	FILE *fh = fopen(filename, "w");
	if(!fh) {
		fprintf(stderr, "Could not create file %s: %s\n", filename, strerror(errno));
		fclose(f);
		return false;
	}

	// Filter first chunk on approved keywords, split between meshlink.conf and hosts/Name
	// Other chunks go unfiltered to their respective host config files
	const char *p = mesh->data;
	char *l, *value;

	while((l = get_line(&p))) {
		// Ignore comments
		if(*l == '#')
			continue;

		// Split line into variable and value
		int len = strcspn(l, "\t =");
		value = l + len;
		value += strspn(value, "\t ");
		if(*value == '=') {
			value++;
			value += strspn(value, "\t ");
		}
		l[len] = 0;

		// Is it a Name?
		if(!strcasecmp(l, "Name"))
			if(strcmp(value, name))
				break;
			else
				continue;
		else if(!strcasecmp(l, "NetName"))
			continue;

		// Check the list of known variables //TODO: most variables will not be available in meshlink, only name and key will be absolutely necessary
		bool found = false;
		int i;
		for(i = 0; variables[i].name; i++) {
			if(strcasecmp(l, variables[i].name))
				continue;
			found = true;
			break;
		}

		// Ignore unknown and unsafe variables
		if(!found) {
			fprintf(stderr, "Ignoring unknown variable '%s' in invitation.\n", l);
			continue;
		} else if(!(variables[i].type & VAR_SAFE)) {
			fprintf(stderr, "Ignoring unsafe variable '%s' in invitation.\n", l);
			continue;
		}

		// Copy the safe variable to the right config file
		fprintf(variables[i].type & VAR_HOST ? fh : f, "%s = %s\n", l, value);
	}

	fclose(f);

	while(l && !strcasecmp(l, "Name")) {
		if(!check_id(value)) {
			fprintf(stderr, "Invalid Name found in invitation.\n");
			return false;
		}

		if(!strcmp(value, name)) {
			fprintf(stderr, "Secondary chunk would overwrite our own host config file.\n");
			return false;
		}

		snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, value);
		f = fopen(filename, "w");

		if(!f) {
			fprintf(stderr, "Could not create file %s: %s\n", filename, strerror(errno));
			return false;
		}

		while((l = get_line(&p))) {
			if(!strcmp(l, "#---------------------------------------------------------------#"))
				continue;
			int len = strcspn(l, "\t =");
			if(len == 4 && !strncasecmp(l, "Name", 4)) {
				value = l + len;
				value += strspn(value, "\t ");
				if(*value == '=') {
					value++;
					value += strspn(value, "\t ");
				}
				l[len] = 0;
				break;
			}

			fputs(l, f);
			fputc('\n', f);
		}

		fclose(f);
	}

	char *b64key = ecdsa_get_base64_public_key(mesh->self->connection->ecdsa);
	if(!b64key)
		return false;

	fprintf(fh, "ECDSAPublicKey = %s\n", b64key);
	fprintf(fh, "Port = %s\n", mesh->myport);

	fclose(fh);

	sptps_send_record(&(mesh->sptps), 1, b64key, strlen(b64key));
	free(b64key);

	free(mesh->self->name);
	free(mesh->self->connection->name);
	mesh->self->name = xstrdup(name);
	mesh->self->connection->name = name;

	fprintf(stderr, "Configuration stored in: %s\n", mesh->confbase);

	load_all_nodes(mesh);

	return true;
}

static bool invitation_send(void *handle, uint8_t type, const void *data, size_t len) {
	meshlink_handle_t* mesh = handle;
	while(len) {
		int result = send(mesh->sock, data, len, 0);
		if(result == -1 && errno == EINTR)
			continue;
		else if(result <= 0)
			return false;
		data += result;
		len -= result;
	}
	return true;
}

static bool invitation_receive(void *handle, uint8_t type, const void *msg, uint16_t len) {
	meshlink_handle_t* mesh = handle;
	switch(type) {
		case SPTPS_HANDSHAKE:
			return sptps_send_record(&(mesh->sptps), 0, mesh->cookie, sizeof mesh->cookie);

		case 0:
			mesh->data = xrealloc(mesh->data, mesh->thedatalen + len + 1);
			memcpy(mesh->data + mesh->thedatalen, msg, len);
			mesh->thedatalen += len;
			mesh->data[mesh->thedatalen] = 0;
			break;

		case 1:
			return finalize_join(mesh);

		case 2:
			fprintf(stderr, "Invitation succesfully accepted.\n");
			shutdown(mesh->sock, SHUT_RDWR);
			mesh->success = true;
			break;

		default:
			return false;
	}

	return true;
}

static bool recvline(meshlink_handle_t* mesh, size_t len) {
	char *newline = NULL;

	if(!mesh->sock)
		abort();

	while(!(newline = memchr(mesh->buffer, '\n', mesh->blen))) {
		int result = recv(mesh->sock, mesh->buffer + mesh->blen, sizeof mesh->buffer - mesh->blen, 0);
		if(result == -1 && errno == EINTR)
			continue;
		else if(result <= 0)
			return false;
		mesh->blen += result;
	}

	if(newline - mesh->buffer >= len)
		return false;

	len = newline - mesh->buffer;

	memcpy(mesh->line, mesh->buffer, len);
	mesh->line[len] = 0;
	memmove(mesh->buffer, newline + 1, mesh->blen - len - 1);
	mesh->blen -= len + 1;

	return true;
}
static bool sendline(int fd, char *format, ...) {
	static char buffer[4096];
	char *p = buffer;
	int blen = 0;
	va_list ap;

	va_start(ap, format);
	blen = vsnprintf(buffer, sizeof buffer, format, ap);
	va_end(ap);

	if(blen < 1 || blen >= sizeof buffer)
		return false;

	buffer[blen] = '\n';
	blen++;

	while(blen) {
		int result = send(fd, p, blen, MSG_NOSIGNAL);
		if(result == -1 && errno == EINTR)
			continue;
		else if(result <= 0)
			return false;
		p += result;
		blen -= result;
	}

	return true;
}

static const char *errstr[] = {
	[MESHLINK_OK] = "No error",
	[MESHLINK_EINVAL] = "Invalid argument",
	[MESHLINK_ENOMEM] = "Out of memory",
	[MESHLINK_ENOENT] = "No such node",
	[MESHLINK_EEXIST] = "Node already exists",
	[MESHLINK_EINTERNAL] = "Internal error",
	[MESHLINK_ERESOLV] = "Could not resolve hostname",
	[MESHLINK_ESTORAGE] = "Storage error",
	[MESHLINK_ENETWORK] = "Network error",
	[MESHLINK_EPEER] = "Error communicating with peer",
};

const char *meshlink_strerror(meshlink_errno_t err) {
	if(err < 0 || err >= sizeof errstr / sizeof *errstr)
		return "Invalid error code";
	return errstr[err];
}

static bool ecdsa_keygen(meshlink_handle_t *mesh) {
	ecdsa_t *key;
	FILE *f;
	char pubname[PATH_MAX], privname[PATH_MAX];

	fprintf(stderr, "Generating ECDSA keypair:\n");

	if(!(key = ecdsa_generate())) {
		fprintf(stderr, "Error during key generation!\n");
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	} else
		fprintf(stderr, "Done.\n");

	snprintf(privname, sizeof privname, "%s" SLASH "ecdsa_key.priv", mesh->confbase);
	f = fopen(privname, "w");

	if(!f) {
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

#ifdef HAVE_FCHMOD
	fchmod(fileno(f), 0600);
#endif

	if(!ecdsa_write_pem_private_key(key, f)) {
		fprintf(stderr, "Error writing private key!\n");
		ecdsa_free(key);
		fclose(f);
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	fclose(f);

	snprintf(pubname, sizeof pubname, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, mesh->name);
	f = fopen(pubname, "a");

	if(!f) {
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	char *pubkey = ecdsa_get_base64_public_key(key);
	fprintf(f, "ECDSAPublicKey = %s\n", pubkey);
	free(pubkey);

	fclose(f);
	ecdsa_free(key);

	return true;
}

static bool meshlink_setup(meshlink_handle_t *mesh) {
	if(mkdir(mesh->confbase, 0777) && errno != EEXIST) {
		fprintf(stderr, "Could not create directory %s: %s\n", mesh->confbase, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	char filename[PATH_MAX];
	snprintf(filename, sizeof filename, "%s" SLASH "hosts", mesh->confbase);

	if(mkdir(filename, 0777) && errno != EEXIST) {
		fprintf(stderr, "Could not create directory %s: %s\n", filename, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	snprintf(filename, sizeof filename, "%s" SLASH "meshlink.conf", mesh->confbase);

	if(!access(filename, F_OK)) {
		fprintf(stderr, "Configuration file %s already exists!\n", filename);
		meshlink_errno = MESHLINK_EEXIST;
		return false;
	}

	FILE *f = fopen(filename, "w");
	if(!f) {
		fprintf(stderr, "Could not create file %s: %s\n", filename, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	fprintf(f, "Name = %s\n", mesh->name);
	fclose(f);

	if(!ecdsa_keygen(mesh)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	check_port(mesh);

	return true;
}

meshlink_handle_t *meshlink_open(const char *confbase, const char *name, const char* appname) {
	return meshlink_open_with_size(confbase, name, appname, sizeof(meshlink_handle_t));
}

meshlink_handle_t *meshlink_open_with_size(const char *confbase, const char *name, const char* appname, size_t size) {

	// Validate arguments provided by the application
	bool usingname = false;

	if(!confbase || !*confbase) {
		fprintf(stderr, "No confbase given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(!appname || !*appname) {
		fprintf(stderr, "No appname given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(!name || !*name) {
		fprintf(stderr, "No name given!\n");
		//return NULL;
	}
	else { //check name only if there is a name != NULL

		if(!check_id(name)) {
			fprintf(stderr, "Invalid name given!\n");
			meshlink_errno = MESHLINK_EINVAL;
			return NULL;
		} else { usingname = true;}
	}

	meshlink_handle_t *mesh = xzalloc(size);
	mesh->confbase = xstrdup(confbase);
	mesh->appname = xstrdup(appname);
	if (usingname) mesh->name = xstrdup(name);
	pthread_mutex_init ( &(mesh->nodes_mutex), NULL);
	mesh->threadstarted = false;
	event_loop_init(&mesh->loop);
	mesh->loop.data = mesh;

	// TODO: should be set by a function.
	mesh->debug_level = 5;

	// Check whether meshlink.conf already exists

	char filename[PATH_MAX];
	snprintf(filename, sizeof filename, "%s" SLASH "meshlink.conf", confbase);

	if(access(filename, R_OK)) {
		if(errno == ENOENT) {
			// If not, create it
			if(!meshlink_setup(mesh)) {
				// meshlink_errno is set by meshlink_setup()
				return NULL;
			}
		} else {
			fprintf(stderr, "Cannot not read from %s: %s\n", filename, strerror(errno));
			meshlink_close(mesh);
			meshlink_errno = MESHLINK_ESTORAGE;
			return NULL;
		}
	}

	// Read the configuration

	init_configuration(&mesh->config);

	if(!read_server_config(mesh)) {
		meshlink_close(mesh);
		meshlink_errno = MESHLINK_ESTORAGE;
		return NULL;
	};

#ifdef HAVE_MINGW
	struct WSAData wsa_state;
	WSAStartup(MAKEWORD(2, 2), &wsa_state);
#endif

	// Setup up everything
	// TODO: we should not open listening sockets yet

	if(!setup_network(mesh)) {
		meshlink_close(mesh);
		meshlink_errno = MESHLINK_ENETWORK;
		return NULL;
	}

	return mesh;
}

static void *meshlink_main_loop(void *arg) {
	meshlink_handle_t *mesh = arg;

	try_outgoing_connections(mesh);

	fprintf(stderr, "Starting main_loop...\n");
	main_loop(mesh);
	fprintf(stderr, "main_loop returned.\n");

	return NULL;
}

bool meshlink_start(meshlink_handle_t *mesh) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	// TODO: open listening sockets first

	//Check that a valid name is set
	if(!mesh->name ) {
		fprintf(stderr, "No name given!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	// Start the main thread

	if(pthread_create(&mesh->thread, NULL, meshlink_main_loop, mesh) != 0) {
		fprintf(stderr, "Could not start thread: %s\n", strerror(errno));
		memset(&mesh->thread, 0, sizeof mesh->thread);
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	mesh->threadstarted=true;

	// Start discovery
	if(!discovery_start(mesh))
		return false;

	return true;
}

void meshlink_stop(meshlink_handle_t *mesh) {
	
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	// Stop discovery
	discovery_stop(mesh);

	// Shut down a listening socket to signal the main thread to shut down

	listen_socket_t *s = &mesh->listen_socket[0];
	shutdown(s->tcp.fd, SHUT_RDWR);

	// Wait for the main thread to finish

	pthread_join(mesh->thread, NULL);
	mesh->threadstarted = false;

	// Fix the socket
	
	closesocket(s->tcp.fd);
	io_del(&mesh->loop, &s->tcp);
	s->tcp.fd = setup_listen_socket(&s->sa);
	if(s->tcp.fd < 0)
		logger(DEBUG_ALWAYS, LOG_ERR, "Could not repair listenen socket!");
	else
		io_add(&mesh->loop, &s->tcp, handle_new_meta_connection, s, s->tcp.fd, IO_READ);
}

void meshlink_close(meshlink_handle_t *mesh) {
	if(!mesh || !mesh->confbase) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	// Close and free all resources used.

	close_network_connections(mesh);

	logger(DEBUG_ALWAYS, LOG_NOTICE, "Terminating");

	exit_configuration(&mesh->config);
	event_loop_exit(&mesh->loop);

#ifdef HAVE_MINGW
	if(mesh->confbase)
		WSACleanup();
#endif

	ecdsa_free(mesh->invitation_key);

	free(mesh->name);
	free(mesh->confbase);

	memset(mesh, 0, sizeof *mesh);

	free(mesh);
}

void meshlink_set_receive_cb(meshlink_handle_t *mesh, meshlink_receive_cb_t cb) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	mesh->receive_cb = cb;
}

void meshlink_set_node_status_cb(meshlink_handle_t *mesh, meshlink_node_status_cb_t cb) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	mesh->node_status_cb = cb;
}

void meshlink_set_log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, meshlink_log_cb_t cb) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	mesh->log_cb = cb;
	mesh->log_level = level;
}

bool meshlink_send(meshlink_handle_t *mesh, meshlink_node_t *destination, const void *data, size_t len) {
	if(!mesh || !destination) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(!len)
		return true;

	if(!data) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	//add packet to the queue
	outpacketqueue_t *packet_in_queue = xzalloc(sizeof *packet_in_queue);
	packet_in_queue->destination=destination;
	packet_in_queue->data=data;
	packet_in_queue->len=len;
	if(!meshlink_queue_push(&mesh->outpacketqueue, packet_in_queue)) {
		free(packet_in_queue);
		return false;
	}

	//notify event loop
	signal_trigger(&(mesh->loop),&(mesh->datafromapp));
	return true;
}

void meshlink_send_from_queue(event_loop_t* el,meshlink_handle_t *mesh) {
	vpn_packet_t packet;
	meshlink_packethdr_t *hdr = (meshlink_packethdr_t *)packet.data;

	outpacketqueue_t* p = meshlink_queue_pop(&mesh->outpacketqueue);
	if(!p)
		return;

	if (sizeof(meshlink_packethdr_t) + p->len > MAXSIZE) {
		//log something
		return ;
	}

	packet.probe = false;
	memset(hdr, 0, sizeof *hdr);
	memcpy(hdr->destination, p->destination->name, sizeof hdr->destination);
	memcpy(hdr->source, mesh->self->name, sizeof hdr->source);

	packet.len = sizeof *hdr + p->len;
	memcpy(packet.data + sizeof *hdr, p->data, p->len);

        mesh->self->in_packets++;
        mesh->self->in_bytes += packet.len;
        route(mesh, mesh->self, &packet);
	return ;
}

ssize_t meshlink_get_pmtu(meshlink_handle_t *mesh, meshlink_node_t *destination) {
	if(!mesh || !destination) {
		meshlink_errno = MESHLINK_EINVAL;
		return -1;
	}

	node_t *n = (node_t *)destination;
	if(!n->status.reachable)
		return 0;
	else if(n->mtuprobes > 30 && n->minmtu)
		return n->minmtu;
	else
		return MTU;
}

char *meshlink_get_fingerprint(meshlink_handle_t *mesh, meshlink_node_t *node) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	node_t *n = (node_t *)node;

	if(!node_read_ecdsa_public_key(mesh, n) || !n->ecdsa) {
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	char *fingerprint = ecdsa_get_base64_public_key(n->ecdsa);

	if(!fingerprint)
		meshlink_errno = MESHLINK_EINTERNAL;

	return fingerprint;
}

meshlink_node_t *meshlink_get_node(meshlink_handle_t *mesh, const char *name) {
	if(!mesh || !name) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	return (meshlink_node_t *)lookup_node(mesh, (char *)name); // TODO: make lookup_node() use const
}

meshlink_node_t **meshlink_get_all_nodes(meshlink_handle_t *mesh, meshlink_node_t **nodes, size_t *nmemb) {
	if(!mesh || !nmemb || (*nmemb && !nodes)) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	meshlink_node_t **result;

	//lock mesh->nodes
	pthread_mutex_lock(&(mesh->nodes_mutex));

	*nmemb = mesh->nodes->count;
	result = realloc(nodes, *nmemb * sizeof *nodes);

	if(result) {
		meshlink_node_t **p = result;
		for splay_each(node_t, n, mesh->nodes)
			*p++ = (meshlink_node_t *)n;
	} else {
		*nmemb = 0;
		free(nodes);
		meshlink_errno = MESHLINK_ENOMEM;
	}

	pthread_mutex_unlock(&(mesh->nodes_mutex));

	return result;
}

bool meshlink_sign(meshlink_handle_t *mesh, const void *data, size_t len, void *signature, size_t *siglen) {
	if(!mesh || !data || !len || !signature || !siglen) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(*siglen < MESHLINK_SIGLEN) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(!ecdsa_sign(mesh->self->connection->ecdsa, data, len, signature)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	*siglen = MESHLINK_SIGLEN;
	return true;
}

bool meshlink_verify(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len, const void *signature, size_t siglen) {
	if(!mesh || !data || !len || !signature) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(siglen != MESHLINK_SIGLEN) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	struct node_t *n = (struct node_t *)source;
	node_read_ecdsa_public_key(mesh, n);
	if(!n->ecdsa) {
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	return ecdsa_verify(((struct node_t *)source)->ecdsa, data, len, signature);
}

static bool refresh_invitation_key(meshlink_handle_t *mesh) {
	char filename[PATH_MAX];

	snprintf(filename, sizeof filename, "%s" SLASH "invitations", mesh->confbase);
	if(mkdir(filename, 0700) && errno != EEXIST) {
		fprintf(stderr, "Could not create directory %s: %s\n", filename, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	// Count the number of valid invitations, clean up old ones
	DIR *dir = opendir(filename);
	if(!dir) {
		fprintf(stderr, "Could not read directory %s: %s\n", filename, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	errno = 0;
	int count = 0;
	struct dirent *ent;
	time_t deadline = time(NULL) - 604800; // 1 week in the past

	while((ent = readdir(dir))) {
		if(strlen(ent->d_name) != 24)
			continue;
		char invname[PATH_MAX];
		struct stat st;
		snprintf(invname, sizeof invname, "%s" SLASH "%s", filename, ent->d_name);
		if(!stat(invname, &st)) {
			if(mesh->invitation_key && deadline < st.st_mtime)
				count++;
			else
				unlink(invname);
		} else {
			fprintf(stderr, "Could not stat %s: %s\n", invname, strerror(errno));
			errno = 0;
		}
	}

	if(errno) {
		fprintf(stderr, "Error while reading directory %s: %s\n", filename, strerror(errno));
		closedir(dir);
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	closedir(dir);

	snprintf(filename, sizeof filename, "%s" SLASH "invitations" SLASH "ecdsa_key.priv", mesh->confbase);

	// Remove the key if there are no outstanding invitations.
	if(!count) {
		unlink(filename);
		if(mesh->invitation_key) {
			ecdsa_free(mesh->invitation_key);
			mesh->invitation_key = NULL;
		}
	}

	if(mesh->invitation_key)
		return true;

	// Create a new key if necessary.
	FILE *f = fopen(filename, "r");
	if(!f) {
		if(errno != ENOENT) {
			fprintf(stderr, "Could not read %s: %s\n", filename, strerror(errno));
			meshlink_errno = MESHLINK_ESTORAGE;
			return false;
		}

		mesh->invitation_key = ecdsa_generate();
		if(!mesh->invitation_key) {
			fprintf(stderr, "Could not generate a new key!\n");
			meshlink_errno = MESHLINK_EINTERNAL;
			return false;
		}
		f = fopen(filename, "w");
		if(!f) {
			fprintf(stderr, "Could not write %s: %s\n", filename, strerror(errno));
			meshlink_errno = MESHLINK_ESTORAGE;
			return false;
		}
		chmod(filename, 0600);
		ecdsa_write_pem_private_key(mesh->invitation_key, f);
		fclose(f);
	} else {
		mesh->invitation_key = ecdsa_read_pem_private_key(f);
		fclose(f);
		if(!mesh->invitation_key) {
			fprintf(stderr, "Could not read private key from %s\n", filename);
			meshlink_errno = MESHLINK_ESTORAGE;
		}
	}

	return mesh->invitation_key;
}

bool meshlink_add_address(meshlink_handle_t *mesh, const char *address) {
	if(!mesh || !address) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	for(const char *p = address; *p; p++) {
		if(isalnum(*p) || *p == '-' || *p == '.' || *p == ':')
			continue;
		fprintf(stderr, "Invalid character in address: %s\n", address);
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	return append_config_file(mesh, mesh->self->name, "Address", address);
}

char *meshlink_invite(meshlink_handle_t *mesh, const char *name) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	// Check validity of the new node's name
	if(!check_id(name)) {
		fprintf(stderr, "Invalid name for node.\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	// Ensure no host configuration file with that name exists
	char filename[PATH_MAX];
	snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, name);
	if(!access(filename, F_OK)) {
		fprintf(stderr, "A host config file for %s already exists!\n", name);
		meshlink_errno = MESHLINK_EEXIST;
		return NULL;
	}

	// Ensure no other nodes know about this name
	if(meshlink_get_node(mesh, name)) {
		fprintf(stderr, "A node with name %s is already known!\n", name);
		meshlink_errno = MESHLINK_EEXIST;
		return NULL;
	}

	// Get the local address
	char *address = get_my_hostname(mesh);
	if(!address) {
		fprintf(stderr, "No Address known for ourselves!\n");
		meshlink_errno = MESHLINK_ERESOLV;
		return NULL;
	}

	if(!refresh_invitation_key(mesh)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		return NULL;
	}

	char hash[64];

	// Create a hash of the key.
	char *fingerprint = ecdsa_get_base64_public_key(mesh->invitation_key);
	sha512(fingerprint, strlen(fingerprint), hash);
	b64encode_urlsafe(hash, hash, 18);

	// Create a random cookie for this invitation.
	char cookie[25];
	randomize(cookie, 18);

	// Create a filename that doesn't reveal the cookie itself
	char buf[18 + strlen(fingerprint)];
	char cookiehash[64];
	memcpy(buf, cookie, 18);
	memcpy(buf + 18, fingerprint, sizeof buf - 18);
	sha512(buf, sizeof buf, cookiehash);
	b64encode_urlsafe(cookiehash, cookiehash, 18);

	b64encode_urlsafe(cookie, cookie, 18);

	free(fingerprint);

	// Create a file containing the details of the invitation.
	snprintf(filename, sizeof filename, "%s" SLASH "invitations" SLASH "%s", mesh->confbase, cookiehash);
	int ifd = open(filename, O_RDWR | O_CREAT | O_EXCL, 0600);
	if(!ifd) {
		fprintf(stderr, "Could not create invitation file %s: %s\n", filename, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return NULL;
	}
	FILE *f = fdopen(ifd, "w");
	if(!f)
		abort();

	// Fill in the details.
	fprintf(f, "Name = %s\n", name);
	//if(netname)
	//	fprintf(f, "NetName = %s\n", netname);
	fprintf(f, "ConnectTo = %s\n", mesh->self->name);

	// Copy Broadcast and Mode
	snprintf(filename, sizeof filename, "%s" SLASH "meshlink.conf", mesh->confbase);
	FILE *tc = fopen(filename,  "r");
	if(tc) {
		char buf[1024];
		while(fgets(buf, sizeof buf, tc)) {
			if((!strncasecmp(buf, "Mode", 4) && strchr(" \t=", buf[4]))
					|| (!strncasecmp(buf, "Broadcast", 9) && strchr(" \t=", buf[9]))) {
				fputs(buf, f);
				// Make sure there is a newline character.
				if(!strchr(buf, '\n'))
					fputc('\n', f);
			}
		}
		fclose(tc);
	} else {
		fprintf(stderr, "Could not create %s: %s\n", filename, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return NULL;
	}

	fprintf(f, "#---------------------------------------------------------------#\n");
	fprintf(f, "Name = %s\n", mesh->self->name);

	snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, mesh->self->name);
	fcopy(f, filename);
	fclose(f);

	// Create an URL from the local address, key hash and cookie
	char *url;
	xasprintf(&url, "%s/%s%s", address, hash, cookie);
	free(address);

	return url;
}

bool meshlink_join(meshlink_handle_t *mesh, const char *invitation) {
	if(!mesh || !invitation) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	//TODO: think of a better name for this variable, or of a different way to tokenize the invitation URL.
	char copy[strlen(invitation) + 1];
	strcpy(copy, invitation);

	// Split the invitation URL into hostname, port, key hash and cookie.

	char *slash = strchr(copy, '/');
	if(!slash)
		goto invalid;

	*slash++ = 0;

	if(strlen(slash) != 48)
		goto invalid;

	char *address = copy;
	char *port = NULL;
	if(*address == '[') {
		address++;
		char *bracket = strchr(address, ']');
		if(!bracket)
			goto invalid;
		*bracket = 0;
		if(bracket[1] == ':')
			port = bracket + 2;
	} else {
		port = strchr(address, ':');
		if(port)
			*port++ = 0;
	}

	if(!port)
		goto invalid;

	if(!b64decode(slash, mesh->hash, 18) || !b64decode(slash + 24, mesh->cookie, 18))
		goto invalid;

	// Generate a throw-away key for the invitation.
	ecdsa_t *key = ecdsa_generate();
	if(!key) {
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	char *b64key = ecdsa_get_base64_public_key(key);

	//Before doing meshlink_join make sure we are not connected to another mesh
	if ( mesh->threadstarted ){
		goto invalid;
	}

	// Connect to the meshlink daemon mentioned in the URL.
	struct addrinfo *ai = str2addrinfo(address, port, SOCK_STREAM);
	if(!ai) {
		meshlink_errno = MESHLINK_ERESOLV;
		return false;
	}

	mesh->sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if(mesh->sock <= 0) {
		fprintf(stderr, "Could not open socket: %s\n", strerror(errno));
		freeaddrinfo(ai);
		meshlink_errno = MESHLINK_ENETWORK;
		return false;
	}

	if(connect(mesh->sock, ai->ai_addr, ai->ai_addrlen)) {
		fprintf(stderr, "Could not connect to %s port %s: %s\n", address, port, strerror(errno));
		closesocket(mesh->sock);
		freeaddrinfo(ai);
		meshlink_errno = MESHLINK_ENETWORK;
		return false;
	}

	freeaddrinfo(ai);

	fprintf(stderr, "Connected to %s port %s...\n", address, port);

	// Tell him we have an invitation, and give him our throw-away key.

	mesh->blen = 0;

	if(!sendline(mesh->sock, "0 ?%s %d.%d", b64key, PROT_MAJOR, 1)) {
		fprintf(stderr, "Error sending request to %s port %s: %s\n", address, port, strerror(errno));
		closesocket(mesh->sock);
		meshlink_errno = MESHLINK_ENETWORK;
		return false;
	}

	free(b64key);

	char hisname[4096] = "";
	int code, hismajor, hisminor = 0;

	if(!recvline(mesh, sizeof mesh->line) || sscanf(mesh->line, "%d %s %d.%d", &code, hisname, &hismajor, &hisminor) < 3 || code != 0 || hismajor != PROT_MAJOR || !check_id(hisname) || !recvline(mesh, sizeof mesh->line) || !rstrip(mesh->line) || sscanf(mesh->line, "%d ", &code) != 1 || code != ACK || strlen(mesh->line) < 3) {
		fprintf(stderr, "Cannot read greeting from peer\n");
		closesocket(mesh->sock);
		meshlink_errno = MESHLINK_ENETWORK;
		return false;
	}

	// Check if the hash of the key he gave us matches the hash in the URL.
	char *fingerprint = mesh->line + 2;
	char hishash[64];
	if(sha512(fingerprint, strlen(fingerprint), hishash)) {
		fprintf(stderr, "Could not create hash\n%s\n", mesh->line + 2);
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}
	if(memcmp(hishash, mesh->hash, 18)) {
		fprintf(stderr, "Peer has an invalid key!\n%s\n", mesh->line + 2);
		meshlink_errno = MESHLINK_EPEER;
		return false;

	}

	ecdsa_t *hiskey = ecdsa_set_base64_public_key(fingerprint);
	if(!hiskey) {
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	// Start an SPTPS session
	if(!sptps_start(&mesh->sptps, mesh, true, false, key, hiskey, "meshlink invitation", 15, invitation_send, invitation_receive)) {
		meshlink_errno = MESHLINK_EINTERNAL;
		return false;
	}

	// Feed rest of input buffer to SPTPS
	if(!sptps_receive_data(&mesh->sptps, mesh->buffer, mesh->blen)) {
		meshlink_errno = MESHLINK_EPEER;
		return false;
	}

	int len;

	while((len = recv(mesh->sock, mesh->line, sizeof mesh->line, 0))) {
		if(len < 0) {
			if(errno == EINTR)
				continue;
			fprintf(stderr, "Error reading data from %s port %s: %s\n", address, port, strerror(errno));
			meshlink_errno = MESHLINK_ENETWORK;
			return false;
		}

		if(!sptps_receive_data(&mesh->sptps, mesh->line, len)) {
			meshlink_errno = MESHLINK_EPEER;
			return false;
		}
	}

	sptps_stop(&mesh->sptps);
	ecdsa_free(hiskey);
	ecdsa_free(key);
	closesocket(mesh->sock);

	if(!mesh->success) {
		fprintf(stderr, "Connection closed by peer, invitation cancelled.\n");
		meshlink_errno = MESHLINK_EPEER;
		return false;
	}

	return true;

invalid:
	fprintf(stderr, "Invalid invitation URL or you are already connected to a Mesh ?\n");
	meshlink_errno = MESHLINK_EINVAL;
	return false;
}

char *meshlink_export(meshlink_handle_t *mesh) {
	if(!mesh) {
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	char filename[PATH_MAX];
	snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, mesh->self->name);
	FILE *f = fopen(filename, "r");
	if(!f) {
		fprintf(stderr, "Could not open %s: %s\n", filename, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	rewind(f);

	size_t len = fsize + 9 + strlen(mesh->self->name);
	char *buf = xmalloc(len);
	snprintf(buf, len, "Name = %s\n", mesh->self->name);
	if(fread(buf + len - fsize - 1, fsize, 1, f) != 1) {
		fprintf(stderr, "Error reading from %s: %s\n", filename, strerror(errno));
		fclose(f);
		meshlink_errno = MESHLINK_ESTORAGE;
		return NULL;
	}

	fclose(f);
	buf[len - 1] = 0;
	return buf;
}

bool meshlink_import(meshlink_handle_t *mesh, const char *data) {
	if(!mesh || !data) {
		meshlink_errno = MESHLINK_EINVAL;
		return false;
	}

	if(strncmp(data, "Name = ", 7)) {
		fprintf(stderr, "Invalid data\n");
		meshlink_errno = MESHLINK_EPEER;
		return false;
	}

	char *end = strchr(data + 7, '\n');
	if(!end) {
		fprintf(stderr, "Invalid data\n");
		meshlink_errno = MESHLINK_EPEER;
		return false;
	}

	int len = end - (data + 7);
	char name[len + 1];
	memcpy(name, data + 7, len);
	name[len] = 0;
	if(!check_id(name)) {
		fprintf(stderr, "Invalid Name\n");
		meshlink_errno = MESHLINK_EPEER;
		return false;
	}

	char filename[PATH_MAX];
	snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, name);
	if(!access(filename, F_OK)) {
		fprintf(stderr, "File %s already exists, not importing\n", filename);
		meshlink_errno = MESHLINK_EEXIST;
		return false;
	}

	if(errno != ENOENT) {
		fprintf(stderr, "Error accessing %s: %s\n", filename, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	FILE *f = fopen(filename, "w");
	if(!f) {
		fprintf(stderr, "Could not create %s: %s\n", filename, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	fwrite(end + 1, strlen(end + 1), 1, f);
	fclose(f);

	load_all_nodes(mesh);

	return true;
}

void meshlink_blacklist(meshlink_handle_t *mesh, meshlink_node_t *node) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	node_t *n;
	n = (node_t*)node;
	n->status.blacklisted=true;
	fprintf(stderr, "Blacklisted %s.\n",node->name);

	//Make blacklisting persistent in the config file
	append_config_file(mesh, n->name, "blacklisted", "yes");

	return;
}

void meshlink_whitelist(meshlink_handle_t *mesh, meshlink_node_t *node) {
	if(!mesh || !node) {
		meshlink_errno = MESHLINK_EINVAL;
		return;
	}

	node_t *n = (node_t *)node;
	n->status.blacklisted = false;

	//TODO: remove blacklisted = yes from the config file

	return;
}

/* Hint that a hostname may be found at an address
 * See header file for detailed comment.
 */
extern void meshlink_hint_address(meshlink_handle_t *mesh, meshlink_node_t *node, struct sockaddr *addr) {
	if(!mesh || !node || !addr)
		return;
	
	char *addr_str = malloc(MAX_ADDRESS_LENGTH*sizeof(char));
	memset(addr_str, 0, MAX_ADDRESS_LENGTH*sizeof(char));

	char *port_str = malloc(MAX_PORT_LENGTH*sizeof(char));
	memset(port_str, 0, MAX_PORT_LENGTH*sizeof(char));
	
	// extra byte for a space, and one to make sure string is null-terminated
	int full_addr_len = MAX_ADDRESS_LENGTH + MAX_PORT_LENGTH + 2;

	char *full_addr_str = malloc(full_addr_len*sizeof(char));
	memset(full_addr_str, 0, full_addr_len*sizeof(char));
	
	// get address and port number
	if(!get_ip_str(addr, addr_str, MAX_ADDRESS_LENGTH))
		goto fail;
	if(!get_port_str(addr, port_str, MAX_ADDRESS_LENGTH))
		goto fail;

	// append_config_file expects an address, a space, and then a port number
	strcat(full_addr_str, addr_str);
	strcat(full_addr_str, " ");
	strcat(full_addr_str, port_str);
	
	append_config_file(mesh, node->name, "Address", full_addr_str);

fail:
done:
	free(addr_str);
	free(port_str);
	free(full_addr_str);

	// @TODO do we want to fire off a connection attempt right away?
}

static void __attribute__((constructor)) meshlink_init(void) {
	crypto_init();
}

static void __attribute__((destructor)) meshlink_exit(void) {
	crypto_exit();
}
