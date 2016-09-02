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
#include "logger.h"
#include "meshlink_internal.h"
#include "netutl.h"
#include "node.h"
#include "protocol.h"
#include "route.h"
#include "sockaddr.h"
#include "utils.h"
#include "xalloc.h"
#include "ed25519/sha512.h"
#include "discovery.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#if defined(__APPLE__) || defined(_WIN32)
// iOS does not support __thread
// Windows doesn't support to use __declspec(thread) variables with dynamic linkage
meshlink_errno_t meshlink_errno;
#else
__thread meshlink_errno_t meshlink_errno;
#endif

meshlink_log_cb_t global_log_cb;
meshlink_log_level_t global_log_level;

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
    {"CanonicalAddress", VAR_HOST | VAR_MULTIPLE},
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
    FILE *in = fopen(filename, "rb");
    if(!in) {
        logger(NULL, MESHLINK_ERROR, "Could not open %s: %s\n", filename, strerror(errno));
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

    FILE *f = fopen(filename, "rb");
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

static bool is_valid_hostname(const char *hostname) {
    for(const char *p = hostname; *p; p++) {
        if(!(isalnum(*p) || *p == '-' || *p == '.' || *p == ':'))
            return false;
    }

    return true;
}

char *meshlink_get_external_address(meshlink_handle_t *mesh) {
    char *hostname = NULL;

    logger(mesh, MESHLINK_DEBUG, "Trying to discover externally visible hostname...\n");
    struct addrinfo *ai = str2addrinfo("meshlink.io", "80", SOCK_STREAM);
    struct addrinfo *aip = ai;
    static const char request[] = "GET http://www.meshlink.io/host.cgi HTTP/1.0\r\n\r\n";
    char line[256];

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
    if(hostname && !is_valid_hostname(hostname)) {
        free(hostname);
        hostname = NULL;
    }

    if(!hostname)
        meshlink_errno = MESHLINK_ERESOLV;

    return hostname;
}

static char *get_my_hostname(meshlink_handle_t* mesh) {
    char *hostname = NULL;
    char *port = NULL;
    char *hostport = NULL;
    char *name = mesh->self->name;
    char filename[PATH_MAX] = "";
    FILE *f;

    // Use first Address statement in own host config file
    snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, name);
    scan_for_hostname(filename, &hostname, &port);

    if(hostname)
        goto done;

    hostname = meshlink_get_external_address(mesh);
    if(!hostname)
        return NULL;

    f = fopen(filename, "ab");
    if(f) {
        fprintf(f, "\nAddress = %s\n", hostname);
        fclose(f);
    } else {
        logger(mesh, MESHLINK_DEBUG, "Could not append Address to %s: %s\n", filename, strerror(errno));
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
        logger(NULL, MESHLINK_ERROR, "Maximum line length exceeded!\n");
        return NULL;
    }
    if(len && !isprint(**data)) {
        logger(NULL, MESHLINK_ERROR, "Error: get_line len && !isprint(**data)");
        abort();
    }

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

static bool try_bind(meshlink_handle_t *mesh, int port) {
    struct addrinfo *ai = NULL;
    struct addrinfo hint = {
        .ai_flags = AI_PASSIVE,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
    };

    char portstr[16];
    snprintf(portstr, sizeof portstr, "%d", port);

    if(getaddrinfo(NULL, portstr, &hint, &ai) || !ai) {
        logger(mesh, MESHLINK_DEBUG, "Failed to bind port: could not parse address info.\n");
        return false;
    }

    struct addrinfo *ai_first = ai;

    while(ai) {
        int fd = socket(ai->ai_family, SOCK_STREAM, IPPROTO_TCP);
        if(!fd) {
            logger(mesh, MESHLINK_DEBUG, "Failed to bind port: could not initialize socket.\n");
            freeaddrinfo(ai_first);
            return false;
        }
        if(bind(fd, ai->ai_addr, ai->ai_addrlen)) {
            logger(mesh, MESHLINK_DEBUG, "Failed to bind port: failed to bind socket, %s\n", sockstrerror(sockerrno));
            closesocket(fd);
            freeaddrinfo(ai_first);
            return false;
        }
        closesocket(fd);
        ai = ai->ai_next;
    }

    freeaddrinfo(ai_first);
    return true;
}

static int check_port(meshlink_handle_t *mesh) {
    for(int i = 0; i < 1000; i++) {
        int port = 0x1000 + (rand() & 0x7fff);
        if(try_bind(mesh, port)) {
            char filename[PATH_MAX];
            snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, mesh->name);
            FILE *f = fopen(filename, "ab");
            if(!f) {
                logger(mesh, MESHLINK_DEBUG, "Please change MeshLink's Port manually.\n");
                return 0;
            }

            fprintf(f, "Port = %d\n", port);
            fclose(f);
            return port;
        }
    }

    logger(mesh, MESHLINK_DEBUG, "Please change MeshLink's Port manually.\n");
    return 0;
}

static bool finalize_join(meshlink_handle_t *mesh) {
    char *name = xstrdup(get_value(mesh->data, "Name"));
    if(!name) {
        logger(mesh, MESHLINK_DEBUG, "No Name found in invitation!\n");
        return false;
    }

    if(!check_id(name)) {
        logger(mesh, MESHLINK_DEBUG, "Invalid Name found in invitation: %s!\n", name);
        return false;
    }

    char filename[PATH_MAX];
    snprintf(filename, sizeof filename, "%s" SLASH "meshlink.conf", mesh->confbase);

    FILE *f = fopen(filename, "wb");
    if(!f) {
        logger(mesh, MESHLINK_DEBUG, "Could not create file %s: %s\n", filename, strerror(errno));
        return false;
    }

    fprintf(f, "Name = %s\n", name);

    snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, name);
    FILE *fh = fopen(filename, "wb");
    if(!fh) {
        logger(mesh, MESHLINK_DEBUG, "Could not create file %s: %s\n", filename, strerror(errno));
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
            logger(mesh, MESHLINK_DEBUG, "Ignoring unknown variable '%s' in invitation.\n", l);
            continue;
        } else if(!(variables[i].type & VAR_SAFE)) {
            logger(mesh, MESHLINK_DEBUG, "Ignoring unsafe variable '%s' in invitation.\n", l);
            continue;
        }

        // Copy the safe variable to the right config file
        fprintf(variables[i].type & VAR_HOST ? fh : f, "%s = %s\n", l, value);
    }

    fclose(f);

    while(l && !strcasecmp(l, "Name")) {
        if(!check_id(value)) {
            logger(mesh, MESHLINK_DEBUG, "Invalid Name found in invitation.\n");
            return false;
        }

        if(!strcmp(value, name)) {
            logger(mesh, MESHLINK_DEBUG, "Secondary chunk would overwrite our own host config file.\n");
            return false;
        }

        snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, value);
        f = fopen(filename, "wb");

        if(!f) {
            logger(mesh, MESHLINK_DEBUG, "Could not create file %s: %s\n", filename, strerror(errno));
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
    if(!b64key) {
        fclose(fh);
        return false;
        }

    fprintf(fh, "ECDSAPublicKey = %s\n", b64key);
    fprintf(fh, "Port = %s\n", mesh->myport);

    fclose(fh);

    sptps_send_record(&(mesh->sptps), 1, b64key, strlen(b64key));
    free(b64key);

    free(mesh->self->name);
    free(mesh->self->connection->name);
    mesh->self->name = xstrdup(name);
    mesh->self->connection->name = name;

    logger(mesh, MESHLINK_DEBUG, "Configuration stored in: %s\n", mesh->confbase);

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
            return !sptps_send_record(&(mesh->sptps), 0, mesh->cookie, sizeof mesh->cookie);

        case 0:
            mesh->data = xrealloc(mesh->data, mesh->thedatalen + len + 1);
            memcpy(mesh->data + mesh->thedatalen, msg, len);
            mesh->thedatalen += len;
            mesh->data[mesh->thedatalen] = 0;
            break;

        case 1:
            mesh->thedatalen = 0;
            return finalize_join(mesh);

        case 2:
            logger(mesh, MESHLINK_DEBUG, "Invitation succesfully accepted.\n");
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

    if(!mesh->sock) {
        logger(mesh, MESHLINK_ERROR, "Error: recvline !mesh->sock");
        abort();
    }

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

void meshlink_free(void *ptr) {
    free(ptr);
}

static bool ecdsa_keygen(meshlink_handle_t *mesh) {
    ecdsa_t *key;
    FILE *f;
    char pubname[PATH_MAX], privname[PATH_MAX];

    logger(mesh, MESHLINK_DEBUG, "Generating ECDSA keypair:\n");

    if(!(key = ecdsa_generate())) {
        logger(mesh, MESHLINK_DEBUG, "Error during key generation!\n");
        meshlink_errno = MESHLINK_EINTERNAL;
        return false;
    } else
        logger(mesh, MESHLINK_DEBUG, "Done.\n");

    snprintf(privname, sizeof privname, "%s" SLASH "ecdsa_key.priv", mesh->confbase);
    f = fopen(privname, "wb");

    if(!f) {
        meshlink_errno = MESHLINK_ESTORAGE;
        return false;
    }

#ifdef HAVE_FCHMOD
    fchmod(fileno(f), 0600);
#endif

    if(!ecdsa_write_pem_private_key(key, f)) {
        logger(mesh, MESHLINK_DEBUG, "Error writing private key!\n");
        ecdsa_free(key);
        fclose(f);
        meshlink_errno = MESHLINK_EINTERNAL;
        return false;
    }

    fclose(f);

    snprintf(pubname, sizeof pubname, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, mesh->name);
    f = fopen(pubname, "ab");

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

static struct timeval idle(event_loop_t *loop, void *data) {
    meshlink_handle_t *mesh = data;
    struct timeval t, tmin = {3600, 0};
    for splay_each(node_t, n, mesh->nodes) {
        if(!n->utcp)
            continue;
        t = utcp_timeout(n->utcp);
        if(timercmp(&t, &tmin, <))
            tmin = t;
    }
    return tmin;
}

// Find out what local address a socket would use if we connect to the given address.
// We do this using connect() on a UDP socket, so the kernel has to resolve the address
// of both endpoints, but this will actually not send any UDP packet.
static bool getlocaladdrname(char *destaddr, char *host, socklen_t hostlen) {
    struct addrinfo *rai = NULL;
    const struct addrinfo hint = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
        .ai_protocol = IPPROTO_UDP,
    };

    if(getaddrinfo(destaddr, "80", &hint, &rai) || !rai)
        return false;

    int sock = socket(rai->ai_family, rai->ai_socktype, rai->ai_protocol);
    if(sock == -1) {
        freeaddrinfo(rai);
        return false;
    }

    if(connect(sock, rai->ai_addr, rai->ai_addrlen) && !sockwouldblock(errno)) {
        freeaddrinfo(rai);
        return false;
    }

    freeaddrinfo(rai);

    struct sockaddr_storage sn;
    socklen_t sl = sizeof sn;

    if(getsockname(sock, (struct sockaddr *)&sn, &sl))
        return false;

    if(getnameinfo((struct sockaddr *)&sn, sl, host, hostlen, NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV))
        return false;

    return true;
}

// Get our local address(es) by simulating connecting to an Internet host.
static void add_local_addresses(meshlink_handle_t *mesh) {
    char host[NI_MAXHOST];
    char entry[MAX_STRING_SIZE];

    // IPv4 example.org

    if(getlocaladdrname("93.184.216.34", host, sizeof host)) {
        snprintf(entry, sizeof entry, "%s", host);
        append_config_file(mesh, mesh->name, "Address", entry);
    }

    // IPv6 example.org

    if(getlocaladdrname("2606:2800:220:1:248:1893:25c8:1946", host, sizeof host)) {
        snprintf(entry, sizeof entry, "%s", host);
        append_config_file(mesh, mesh->name, "Address", entry);
    }
}

static bool meshlink_setup(meshlink_handle_t *mesh) {

    MESHLINK_MUTEX_LOCK(&mesh->mesh_mutex);

    if(mkdir(mesh->confbase, 0777) && errno != EEXIST) {
        logger(mesh, MESHLINK_DEBUG, "Could not create directory %s: %s\n", mesh->confbase, strerror(errno));
        meshlink_errno = MESHLINK_ESTORAGE;
        MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
        return false;
    }

    char filename[PATH_MAX];
    snprintf(filename, sizeof filename, "%s" SLASH "hosts", mesh->confbase);

    if(mkdir(filename, 0777) && errno != EEXIST) {
        logger(mesh, MESHLINK_DEBUG, "Could not create directory %s: %s\n", filename, strerror(errno));
        meshlink_errno = MESHLINK_ESTORAGE;
        MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
        return false;
    }

    snprintf(filename, sizeof filename, "%s" SLASH "meshlink.conf", mesh->confbase);

    if(!access(filename, F_OK)) {
        logger(mesh, MESHLINK_DEBUG, "Configuration file %s already exists!\n", filename);
        meshlink_errno = MESHLINK_EEXIST;
        MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
        return false;
    }

    FILE *f = fopen(filename, "wb");
    if(!f) {
        logger(mesh, MESHLINK_DEBUG, "Could not create file %s: %s\n", filename, strerror(errno));
        meshlink_errno = MESHLINK_ESTORAGE;
        MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
        return false;
    }

    fprintf(f, "Name = %s\n", mesh->name);
    fclose(f);

    if(!ecdsa_keygen(mesh)) {
        meshlink_errno = MESHLINK_EINTERNAL;
        MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
        return false;
    }

    check_port(mesh);

    MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);

    return true;
}

meshlink_handle_t *meshlink_open(const char *confbase, const char *name, const char* appname, dev_class_t devclass) {
    // Validate arguments provided by the application

    logger(NULL, MESHLINK_DEBUG, "meshlink_open called\n");

    if(!confbase || !*confbase) {
        logger(NULL, MESHLINK_ERROR, "No confbase given!\n");
        meshlink_errno = MESHLINK_EINVAL;
        return NULL;
    }

    if(!appname || !*appname) {
        logger(NULL, MESHLINK_ERROR, "No appname given!\n");
        meshlink_errno = MESHLINK_EINVAL;
        return NULL;
    }

    if(name && !check_id(name)) {
        logger(NULL, MESHLINK_ERROR, "Invalid name given!\n");
        meshlink_errno = MESHLINK_EINVAL;
        return NULL;
    }

    if(devclass < 0 || devclass > _DEV_CLASS_MAX) {
        logger(NULL, MESHLINK_ERROR, "Invalid devclass given!\n");
        meshlink_errno = MESHLINK_EINVAL;
        return NULL;
    }

    meshlink_handle_t *mesh = xzalloc(sizeof(meshlink_handle_t));
    mesh->confbase = xstrdup(confbase);
    mesh->appname = xstrdup(appname);
    mesh->devclass = devclass;
    if(name)
        mesh->name = xstrdup(name);

    // initialize mutex
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&(mesh->mesh_mutex), &attr);

    MESHLINK_MUTEX_LOCK(&mesh->mesh_mutex);

    mesh->threadstarted = false;
    event_loop_init(&mesh->loop);
    mesh->loop.data = mesh;

    // Check whether meshlink.conf already exists

    char filename[PATH_MAX];
    snprintf(filename, sizeof filename, "%s" SLASH "meshlink.conf", confbase);

    if(access(filename, R_OK)) {
        if(errno == ENOENT) {
            // If not, create it, but only if someone gave us a name
            if(!name) {
                logger(NULL, MESHLINK_ERROR, "Configuration file %s does not exist", filename);
                meshlink_close(mesh);
                meshlink_errno = MESHLINK_ENOENT;
                return NULL;
            }

            if(!meshlink_setup(mesh)) {
                // meshlink_errno is set by meshlink_setup()
                meshlink_close(mesh);
                return NULL;
            }
        } else {
            logger(NULL, MESHLINK_ERROR, "Cannot not read from %s: %s\n", filename, strerror(errno));
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

    if(mesh->name) {
        char* existing_name = get_name(mesh);
        if(strcmp(mesh->name, existing_name)) {
            logger(NULL, MESHLINK_ERROR, "Given name does not match the one in %s\n", filename);
            free(existing_name);
            meshlink_close(mesh);
            meshlink_errno = MESHLINK_EINVAL;
            return NULL;
        }
        free(existing_name);
    } else {
        mesh->name = get_name(mesh);
    }

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

    add_local_addresses(mesh);

    idle_set(&mesh->loop, idle, mesh);

    logger(NULL, MESHLINK_DEBUG, "meshlink_open returning\n");
    MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
    return mesh;
}

static void *meshlink_main_loop(void *arg) {
    meshlink_handle_t *mesh = arg;

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    try_outgoing_connections(mesh);

    logger(mesh, MESHLINK_DEBUG, "Starting main_loop...\n");
    main_loop(mesh);
    logger(mesh, MESHLINK_DEBUG, "main_loop returned.\n");

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return NULL;
}

bool meshlink_start(meshlink_handle_t *mesh) {
    if(!mesh) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    logger(mesh, MESHLINK_DEBUG, "meshlink_start called\n");

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    if(mesh->threadstarted) {
        logger(mesh, MESHLINK_DEBUG, "thread was already running\n");
        return true;
    }

    if(mesh->listen_socket[0].tcp.fd < 0) {
        logger(mesh, MESHLINK_ERROR, "Listening socket not open\n");
        meshlink_errno = MESHLINK_ENETWORK;
        return false;
    }

    mesh->thedatalen = 0;

    // TODO: open listening sockets first

    //Check that a valid name is set
    if(!mesh->name ) {
        logger(mesh, MESHLINK_DEBUG, "No name given!\n");
        meshlink_errno = MESHLINK_EINVAL;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    // Start the main thread

    event_loop_start(&mesh->loop);

    if(pthread_create(&mesh->thread, NULL, meshlink_main_loop, mesh) != 0) {
        logger(mesh, MESHLINK_DEBUG, "Could not start thread: %s\n", strerror(errno));
        memset(&mesh->thread, 0, sizeof mesh->thread);
        meshlink_errno = MESHLINK_EINTERNAL;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    mesh->threadstarted=true;

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));

    discovery_start(mesh);

    return true;
}

void meshlink_stop(meshlink_handle_t *mesh) {
    if(!mesh) {
        meshlink_errno = MESHLINK_EINVAL;
        return;
    }
    if(!mesh->threadstarted) {
        return;
    }

    // Stop discovery
    discovery_stop(mesh);

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));
    logger(mesh, MESHLINK_DEBUG, "meshlink_stop called\n");

    // Shut down the main thread
    event_loop_stop(&mesh->loop);

    // Send ourselves a UDP packet to kick the event loop
    listen_socket_t *s = &mesh->listen_socket[0];
    sockaddr_t self;
    memset(&self, 0, sizeof(sockaddr_t));
    memcpy(&self, &s->sa, sizeof(sockaddr_t));
    if(s->sa.sa.sa_family == AF_INET) {
        self.in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else if(s->sa.sa.sa_family == AF_INET6) {
        self.in6.sin6_addr = in6addr_loopback;
    } else {
        abort();
    }
    // send at least 1 byte, otherwise receive function throws error
    if(sendto(s->udp.fd, "\0", 1, MSG_NOSIGNAL, &self.sa, SALEN(self.sa)) == -1) {
        logger(mesh, MESHLINK_ERROR, "Could not send a UDP packet to ourself. Error: %s", sockstrerror(sockerrno));
    }

    // Wait for the main thread to finish
    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    pthread_join(mesh->thread, NULL);
    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    mesh->threadstarted = false;

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
}

void meshlink_close(meshlink_handle_t *mesh) {
    if(!mesh || !mesh->confbase) {
        meshlink_errno = MESHLINK_EINVAL;
        return;
    }

    // stop can be called even if mesh has not been started
    meshlink_stop(mesh);

    // lock is not released after this
    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    // Close and free all resources used.

    close_network_connections(mesh);

    logger(mesh, MESHLINK_INFO, "Terminating");

    exit_meshlink_queue(&mesh->outpacketqueue, free);
    exit_configuration(&mesh->config);
    event_loop_exit(&mesh->loop);

#ifdef HAVE_MINGW
    if(mesh->confbase)
        WSACleanup();
#endif

    ecdsa_free(mesh->invitation_key);

    free(mesh->name);
    free(mesh->appname);
    free(mesh->confbase);
    pthread_mutex_destroy(&(mesh->mesh_mutex));

    memset(mesh, 0, sizeof *mesh);

    free(mesh);
}

static void deltree(const char *dirname) {
    DIR *d = opendir(dirname);
    if(d) {
        struct dirent *ent;
        while((ent = readdir(d))) {
            if(ent->d_name[0] == '.')
                continue;
            char filename[PATH_MAX];
            snprintf(filename, sizeof filename, "%s" SLASH "%s", dirname, ent->d_name);
            if(unlink(filename))
                deltree(filename);
        }
        closedir(d);
    }
    rmdir(dirname);
    return;
}

bool meshlink_destroy(const char *confbase) {
    if(!confbase) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    char filename[PATH_MAX];
    snprintf(filename, sizeof filename, "%s" SLASH "meshlink.conf", confbase);

    if(unlink(filename)) {
        if(errno == ENOENT) {
            meshlink_errno = MESHLINK_ENOENT;
            return false;
        } else {
            logger(NULL, MESHLINK_ERROR, "Cannot delete %s: %s\n", filename, strerror(errno));
            meshlink_errno = MESHLINK_ESTORAGE;
            return false;
        }
    }

    deltree(confbase);

    return true;
}

void meshlink_set_receive_cb(meshlink_handle_t *mesh, meshlink_receive_cb_t cb) {
    if(!mesh) {
        meshlink_errno = MESHLINK_EINVAL;
        return;
    }

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));
    mesh->receive_cb = cb;
    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
}

void meshlink_set_node_status_cb(meshlink_handle_t *mesh, meshlink_node_status_cb_t cb) {
    if(!mesh) {
        meshlink_errno = MESHLINK_EINVAL;
        return;
    }

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));
    mesh->node_status_cb = cb;
    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
}

void meshlink_set_log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, meshlink_log_cb_t cb) {
    if(mesh) {
        MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));
        mesh->log_cb = cb;
        mesh->log_level = cb ? level : 0;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    } else {
        global_log_cb = cb;
        global_log_level = cb ? level : 0;
    }
}

void meshlink_set_node_pmtu_cb(meshlink_handle_t *mesh, meshlink_node_pmtu_cb_t cb) {
    if(!mesh) {
        meshlink_errno = MESHLINK_EINVAL;
        return;
    }

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));
    mesh->node_pmtu_cb = cb;
    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
}

bool meshlink_send(meshlink_handle_t *mesh, meshlink_node_t *destination, const void *data, size_t len) {
    meshlink_packethdr_t *hdr;

    // Validate arguments
    if(!mesh || !destination || len >= MAXSIZE - sizeof *hdr) {
        meshlink_errno = MESHLINK_EINVAL;
        logger(mesh, MESHLINK_ERROR, "Error: meshlink_send invalid arguments");
        return false;
    }

    if(!len)
    {
        logger(mesh, MESHLINK_WARNING, "Warning: meshlink_send empty packet dropped");
        return true;
    }

    if(!data) {
        meshlink_errno = MESHLINK_EINVAL;
        logger(mesh, MESHLINK_ERROR, "Error: meshlink_send missing data");
        return false;
    }

    // Prepare the packet
    vpn_packet_t *packet = malloc(sizeof *packet);
    if(!packet) {
        meshlink_errno = MESHLINK_ENOMEM;
        logger(mesh, MESHLINK_ERROR, "Error: meshlink_send packet memory allocation failed");
        return false;
    }

    packet->probe = false;
    packet->tcp = false;
    packet->len = len + sizeof *hdr;

    MESHLINK_MUTEX_LOCK(&mesh->mesh_mutex);

    hdr = (meshlink_packethdr_t *)packet->data;
    memset(hdr, 0, sizeof *hdr);
    // leave the last byte as 0 to make sure strings are always
    // null-terminated if they are longer than the buffer
    strncpy(hdr->destination, destination->name, (sizeof hdr->destination) - 1);
    strncpy(hdr->source, mesh->self->name, (sizeof hdr->source) -1 );

    memcpy(packet->data + sizeof *hdr, data, len);

    // Queue it
    if(!signalio_queue(&(mesh->loop), &(mesh->datafromapp), packet)) {
        free(packet);
        meshlink_errno = MESHLINK_ENOMEM;
        MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
        logger(mesh, MESHLINK_ERROR, "Error: meshlink_send failed to queue packet");
        return false;
    }

    MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);

    return true;
}

bool meshlink_send_from_queue(event_loop_t *loop, meshlink_handle_t *mesh, vpn_packet_t *packet) {
    mesh->self->in_packets++;
    mesh->self->in_bytes += packet->len;
    int err = route(mesh, mesh->self, packet);
    if(0 != err) {
        if(sockwouldblock(err)) {
            logger(mesh, MESHLINK_WARNING, "Warning: socket would block, retrying to send packet from queue later");
            return false;
        }
        else {
            logger(mesh, MESHLINK_ERROR, "Error: failed to send packet from queue, dropping the packet");
        }
    }

    return true;
}

ssize_t meshlink_get_pmtu(meshlink_handle_t *mesh, meshlink_node_t *destination) {
    if(!mesh || !destination) {
        meshlink_errno = MESHLINK_EINVAL;
        return -1;
    }
    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    node_t *n = (node_t *)destination;
    if(!n->status.reachable) {
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return 0;

    }
    else if(n->utcp) {
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return utcp_get_mtu(n->utcp);
    }
    else {
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        // return the usable payload size for API users
        return sptps_maxmtu(&n->sptps) - sizeof(meshlink_packethdr_t);
    }
}

char *meshlink_get_fingerprint(meshlink_handle_t *mesh, meshlink_node_t *node) {
    if(!mesh || !node) {
        meshlink_errno = MESHLINK_EINVAL;
        return NULL;
    }
    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    node_t *n = (node_t *)node;

    if(!node_read_ecdsa_public_key(mesh, n) || !n->ecdsa) {
        meshlink_errno = MESHLINK_EINTERNAL;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    char *fingerprint = ecdsa_get_base64_public_key(n->ecdsa);

    if(!fingerprint)
        meshlink_errno = MESHLINK_EINTERNAL;

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return fingerprint;
}

meshlink_node_t *meshlink_get_self(meshlink_handle_t *mesh) {
    if(!mesh) {
        meshlink_errno = MESHLINK_EINVAL;
        return NULL;
    }

    return (meshlink_node_t *)mesh->self;
}

meshlink_node_t *meshlink_get_node(meshlink_handle_t *mesh, const char *name) {
    if(!mesh || !name) {
        meshlink_errno = MESHLINK_EINVAL;
        return NULL;
    }

    meshlink_node_t *node = NULL;

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));
    node = (meshlink_node_t *)lookup_node(mesh, (char *)name); // TODO: make lookup_node() use const
    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return node;
}

meshlink_node_t **meshlink_get_all_nodes(meshlink_handle_t *mesh, meshlink_node_t **nodes, size_t *nmemb) {
    if(!mesh || !nmemb || (*nmemb && !nodes)) {
        meshlink_errno = MESHLINK_EINVAL;
        return NULL;
    }

    meshlink_node_t **result;

    //lock mesh->nodes
    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

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

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));

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

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    if(!ecdsa_sign(mesh->self->connection->ecdsa, data, len, signature)) {
        meshlink_errno = MESHLINK_EINTERNAL;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    *siglen = MESHLINK_SIGLEN;
    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
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

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    bool rval = false;

    struct node_t *n = (struct node_t *)source;
    node_read_ecdsa_public_key(mesh, n);
    if(!n->ecdsa) {
        meshlink_errno = MESHLINK_EINTERNAL;
        rval = false;
    } else {
        rval = ecdsa_verify(((struct node_t *)source)->ecdsa, data, len, signature);
    }
    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return rval;
}

static bool refresh_invitation_key(meshlink_handle_t *mesh) {
    char filename[PATH_MAX];

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    snprintf(filename, sizeof filename, "%s" SLASH "invitations", mesh->confbase);
    if(mkdir(filename, 0700) && errno != EEXIST) {
        logger(mesh, MESHLINK_DEBUG, "Could not create directory %s: %s\n", filename, strerror(errno));
        meshlink_errno = MESHLINK_ESTORAGE;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    // Count the number of valid invitations, clean up old ones
    DIR *dir = opendir(filename);
    if(!dir) {
        logger(mesh, MESHLINK_DEBUG, "Could not read directory %s: %s\n", filename, strerror(errno));
        meshlink_errno = MESHLINK_ESTORAGE;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
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
            logger(mesh, MESHLINK_DEBUG, "Could not stat %s: %s\n", invname, strerror(errno));
            errno = 0;
        }
    }

    if(errno) {
        logger(mesh, MESHLINK_DEBUG, "Error while reading directory %s: %s\n", filename, strerror(errno));
        closedir(dir);
        meshlink_errno = MESHLINK_ESTORAGE;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
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

    if(mesh->invitation_key) {
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return true;
    }

    // Create a new key if necessary.
    FILE *f = fopen(filename, "rb");
    if(!f) {
        if(errno != ENOENT) {
            logger(mesh, MESHLINK_DEBUG, "Could not read %s: %s\n", filename, strerror(errno));
            meshlink_errno = MESHLINK_ESTORAGE;
            MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
            return false;
        }

        mesh->invitation_key = ecdsa_generate();
        if(!mesh->invitation_key) {
            logger(mesh, MESHLINK_DEBUG, "Could not generate a new key!\n");
            meshlink_errno = MESHLINK_EINTERNAL;
            MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
            return false;
        }
        f = fopen(filename, "wb");
        if(!f) {
            logger(mesh, MESHLINK_DEBUG, "Could not write %s: %s\n", filename, strerror(errno));
            meshlink_errno = MESHLINK_ESTORAGE;
            MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
            return false;
        }
        chmod(filename, 0600);
        ecdsa_write_pem_private_key(mesh->invitation_key, f);
        fclose(f);
    } else {
        mesh->invitation_key = ecdsa_read_pem_private_key(f);
        fclose(f);
        if(!mesh->invitation_key) {
            logger(mesh, MESHLINK_DEBUG, "Could not read private key from %s\n", filename);
            meshlink_errno = MESHLINK_ESTORAGE;
        }
    }

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return mesh->invitation_key;
}

bool meshlink_set_canonical_addresses(meshlink_handle_t *mesh, meshlink_node_t *node, const meshlink_canonical_address_t **addresses, size_t nmemb) {
    if(!mesh || !node || !addresses) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    bool rval = false;
    char *hostport = NULL;

    for(size_t i = 0; i < nmemb; i++) {
        meshlink_canonical_address_t const *address = addresses[i];

        if(!is_valid_hostname(address->hostname)) {
            logger(mesh, MESHLINK_DEBUG, "Invalid character in address: %s\n", address->hostname);
            meshlink_errno = MESHLINK_EINVAL;
            return false;
        }

        xasprintf(&hostport, "%s %d", address->hostname, address->port);

        MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));
        rval = append_config_file(mesh, node->name, "Address", hostport);
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));

        free(hostport);
        hostport = NULL;

        if(!rval)
            break;
    }

    return rval;
}

bool meshlink_add_external_address(meshlink_handle_t *mesh) {
    if(!mesh) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    char *address = meshlink_get_external_address(mesh);
    if(!address)
        return false;

    bool rval = false;

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));
    rval = append_config_file(mesh, mesh->self->name, "Address", address);
    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));

    free(address);
    return rval;
}

int meshlink_get_port(meshlink_handle_t *mesh) {
    if(!mesh) {
        meshlink_errno = MESHLINK_EINVAL;
        return -1;
    }

    if(!mesh->myport) {
        meshlink_errno = MESHLINK_EINTERNAL;
        return -1;
    }

    return atoi(mesh->myport);
}

bool meshlink_set_port(meshlink_handle_t *mesh, int port) {
    if(!mesh || port < 0 || port >= 65536 || mesh->threadstarted) {
        meshlink_errno = MESHLINK_EINVAL;
        logger(mesh, MESHLINK_DEBUG, "Failed to set port: port invalid, or thread already started.\n");
        return false;
    }

    if(mesh->myport && port == atoi(mesh->myport)) {
        return true;
    }

    if(!try_bind(mesh, port)) {
        meshlink_errno = MESHLINK_ENETWORK;
        logger(mesh, MESHLINK_DEBUG, "Failed to set port: could not bind port.\n");
        return false;
    }

    bool rval = false;

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));
    if(mesh->threadstarted) {
        meshlink_errno = MESHLINK_EINVAL;
        logger(mesh, MESHLINK_DEBUG, "Failed to set port: thread already started.\n");
        goto done;
    }

    close_network_connections(mesh);
    exit_configuration(&mesh->config);

    char portstr[10];
    snprintf(portstr, sizeof portstr, "%d", port);
    portstr[sizeof portstr - 1] = 0;

    modify_config_file(mesh, mesh->name, "Port", portstr, true);

    init_configuration(&mesh->config);

    if(!read_server_config(mesh)) {
        logger(mesh, MESHLINK_DEBUG, "Failed to set port: could not read config.\n");
        meshlink_errno = MESHLINK_ESTORAGE;
    } else if(!setup_network(mesh)) {
        logger(mesh, MESHLINK_DEBUG, "Failed to set port: could not set up network.\n");
        meshlink_errno = MESHLINK_ENETWORK;
    } else {
        rval = true;
    }

done:
    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));

    return rval;
}

char *meshlink_invite(meshlink_handle_t *mesh, const char *name) {
    if(!mesh) {
        meshlink_errno = MESHLINK_EINVAL;
        return NULL;
    }

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    // Check validity of the new node's name
    if(!check_id(name)) {
        logger(mesh, MESHLINK_DEBUG, "Invalid name for node.\n");
        meshlink_errno = MESHLINK_EINVAL;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return NULL;
    }

    // Ensure no host configuration file with that name exists
    char filename[PATH_MAX];
    snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, name);
    if(!access(filename, F_OK)) {
        logger(mesh, MESHLINK_DEBUG, "A host config file for %s already exists!\n", name);
        meshlink_errno = MESHLINK_EEXIST;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return NULL;
    }

    // Ensure no other nodes know about this name
    if(meshlink_get_node(mesh, name)) {
        logger(mesh, MESHLINK_DEBUG, "A node with name %s is already known!\n", name);
        meshlink_errno = MESHLINK_EEXIST;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return NULL;
    }

    // Get the local address
    char *address = get_my_hostname(mesh);
    if(!address) {
        logger(mesh, MESHLINK_DEBUG, "No Address known for ourselves!\n");
        meshlink_errno = MESHLINK_ERESOLV;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return NULL;
    }

    if(!refresh_invitation_key(mesh)) {
        meshlink_errno = MESHLINK_EINTERNAL;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
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
        logger(mesh, MESHLINK_DEBUG, "Could not create invitation file %s: %s\n", filename, strerror(errno));
        meshlink_errno = MESHLINK_ESTORAGE;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return NULL;
    }
    FILE *f = fdopen(ifd, "w");
    if(!f) {
        logger(mesh, MESHLINK_ERROR, "Error: meshlink_invite failed to open file");
        abort();
    }

    // Fill in the details.
    fprintf(f, "Name = %s\n", name);
    //if(netname)
    //    fprintf(f, "NetName = %s\n", netname);
    fprintf(f, "ConnectTo = %s\n", mesh->self->name);

    // Copy Broadcast and Mode
    snprintf(filename, sizeof filename, "%s" SLASH "meshlink.conf", mesh->confbase);
    FILE *tc = fopen(filename,  "rb");
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
        logger(mesh, MESHLINK_DEBUG, "Could not create %s: %s\n", filename, strerror(errno));
        meshlink_errno = MESHLINK_ESTORAGE;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
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

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return url;
}

bool meshlink_join(meshlink_handle_t *mesh, const char *invitation) {
    if(!mesh || !invitation) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

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
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
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
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    mesh->sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if(mesh->sock <= 0) {
        logger(mesh, MESHLINK_DEBUG, "Could not open socket: %s\n", strerror(errno));
        freeaddrinfo(ai);
        meshlink_errno = MESHLINK_ENETWORK;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    if(connect(mesh->sock, ai->ai_addr, ai->ai_addrlen)) {
        logger(mesh, MESHLINK_DEBUG, "Could not connect to %s port %s: %s\n", address, port, strerror(errno));
        closesocket(mesh->sock);
        freeaddrinfo(ai);
        meshlink_errno = MESHLINK_ENETWORK;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    freeaddrinfo(ai);

    logger(mesh, MESHLINK_DEBUG, "Connected to %s port %s...\n", address, port);

    // Tell him we have an invitation, and give him our throw-away key.

    mesh->blen = 0;

    if(!sendline(mesh->sock, "0 ?%s %d.%d", b64key, PROT_MAJOR, 1)) {
        logger(mesh, MESHLINK_DEBUG, "Error sending request to %s port %s: %s\n", address, port, strerror(errno));
        closesocket(mesh->sock);
        meshlink_errno = MESHLINK_ENETWORK;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    free(b64key);

    char hisname[4096] = "";
    int code, hismajor, hisminor = 0;

    if(!recvline(mesh, sizeof mesh->line) || sscanf(mesh->line, "%d %s %d.%d", &code, hisname, &hismajor, &hisminor) < 3 || code != 0 || hismajor != PROT_MAJOR || !check_id(hisname) || !recvline(mesh, sizeof mesh->line) || !rstrip(mesh->line) || sscanf(mesh->line, "%d ", &code) != 1 || code != ACK || strlen(mesh->line) < 3) {
        logger(mesh, MESHLINK_DEBUG, "Cannot read greeting from peer\n");
        closesocket(mesh->sock);
        meshlink_errno = MESHLINK_ENETWORK;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    // Check if the hash of the key he gave us matches the hash in the URL.
    char *fingerprint = mesh->line + 2;
    char hishash[64];
    if(sha512(fingerprint, strlen(fingerprint), hishash)) {
        logger(mesh, MESHLINK_DEBUG, "Could not create hash\n%s\n", mesh->line + 2);
        meshlink_errno = MESHLINK_EINTERNAL;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }
    if(memcmp(hishash, mesh->hash, 18)) {
        logger(mesh, MESHLINK_DEBUG, "Peer has an invalid key!\n%s\n", mesh->line + 2);
        meshlink_errno = MESHLINK_EPEER;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;

    }

    ecdsa_t *hiskey = ecdsa_set_base64_public_key(fingerprint);
    if(!hiskey) {
        meshlink_errno = MESHLINK_EINTERNAL;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    // Start an SPTPS session
    if(!sptps_start(&mesh->sptps, mesh, true, false, key, hiskey, "meshlink invitation", 15, invitation_send, invitation_receive)) {
        meshlink_errno = MESHLINK_EINTERNAL;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    // Feed rest of input buffer to SPTPS
    if(!sptps_receive_data(&mesh->sptps, mesh->buffer, mesh->blen)) {
        meshlink_errno = MESHLINK_EPEER;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    int len;

    while((len = recv(mesh->sock, mesh->line, sizeof mesh->line, 0))) {
        if(len < 0) {
            if(errno == EINTR)
                continue;
            logger(mesh, MESHLINK_DEBUG, "Error reading data from %s port %s: %s\n", address, port, strerror(errno));
            meshlink_errno = MESHLINK_ENETWORK;
            MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
            return false;
        }

        if(!sptps_receive_data(&mesh->sptps, mesh->line, len)) {
            meshlink_errno = MESHLINK_EPEER;
            MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
            return false;
        }
    }

    sptps_stop(&mesh->sptps);
    ecdsa_free(hiskey);
    ecdsa_free(key);
    closesocket(mesh->sock);

    if(!mesh->success) {
        logger(mesh, MESHLINK_DEBUG, "Connection closed by peer, invitation cancelled.\n");
        meshlink_errno = MESHLINK_EPEER;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return true;

invalid:
    logger(mesh, MESHLINK_DEBUG, "Invalid invitation URL or you are already connected to a Mesh ?\n");
    meshlink_errno = MESHLINK_EINVAL;
    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return false;
}

char *meshlink_export(meshlink_handle_t *mesh) {
    if(!mesh) {
        meshlink_errno = MESHLINK_EINVAL;
        return NULL;
    }

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    char filename[PATH_MAX];
    snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, mesh->self->name);
    FILE *f = fopen(filename, "rb");
    if(!f) {
        logger(mesh, MESHLINK_DEBUG, "Could not open %s: %s\n", filename, strerror(errno));
        meshlink_errno = MESHLINK_ESTORAGE;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    int fsize = ftell(f);
    rewind(f);

    size_t len = fsize + 9 + strlen(mesh->self->name);
    char *buf = xmalloc(len);
    snprintf(buf, len, "Name = %s\n", mesh->self->name);
    if(fread(buf + len - fsize - 1, 1, fsize, f) == 0) {
        logger(mesh, MESHLINK_DEBUG, "Error reading from %s: %s\n", filename, strerror(errno));
        fclose(f);
        meshlink_errno = MESHLINK_ESTORAGE;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return NULL;
    }

    fclose(f);
    buf[len - 1] = 0;

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return buf;
}

bool meshlink_import(meshlink_handle_t *mesh, const char *data) {
    if(!mesh || !data) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    if(strncmp(data, "Name = ", 7)) {
        logger(mesh, MESHLINK_DEBUG, "Invalid data\n");
        meshlink_errno = MESHLINK_EPEER;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    char *end = strchr(data + 7, '\n');
    if(!end) {
        logger(mesh, MESHLINK_DEBUG, "Invalid data\n");
        meshlink_errno = MESHLINK_EPEER;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    int len = end - (data + 7);
    char name[len + 1];
    memcpy(name, data + 7, len);
    name[len] = 0;
    if(!check_id(name)) {
        logger(mesh, MESHLINK_DEBUG, "Invalid Name\n");
        meshlink_errno = MESHLINK_EPEER;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    char filename[PATH_MAX];
    snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, name);
    if(!access(filename, F_OK)) {
        logger(mesh, MESHLINK_DEBUG, "File %s already exists, not importing\n", filename);
        meshlink_errno = MESHLINK_EEXIST;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    if(errno != ENOENT) {
        logger(mesh, MESHLINK_DEBUG, "Error accessing %s: %s\n", filename, strerror(errno));
        meshlink_errno = MESHLINK_ESTORAGE;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    FILE *f = fopen(filename, "wb");
    if(!f) {
        logger(mesh, MESHLINK_DEBUG, "Could not create %s: %s\n", filename, strerror(errno));
        meshlink_errno = MESHLINK_ESTORAGE;
        MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
        return false;
    }

    fwrite(end + 1, strlen(end + 1), 1, f);
    fclose(f);

    load_all_nodes(mesh);

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return true;
}

void meshlink_blacklist(meshlink_handle_t *mesh, meshlink_node_t *node) {
    if(!mesh || !node) {
        meshlink_errno = MESHLINK_EINVAL;
        return;
    }

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    node_t *n;
    n = (node_t*)node;
    n->status.blacklisted=true;
    logger(mesh, MESHLINK_DEBUG, "Blacklisted %s.\n",node->name);

    //Make blacklisting persistent in the config file
    append_config_file(mesh, n->name, "blacklisted", "yes");

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return;
}

void meshlink_whitelist(meshlink_handle_t *mesh, meshlink_node_t *node) {
    if(!mesh || !node) {
        meshlink_errno = MESHLINK_EINVAL;
        return;
    }

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    node_t *n = (node_t *)node;
    n->status.blacklisted = false;

    //TODO: remove blacklisted = yes from the config file

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    return;
}

/* Hint that a hostname may be found at an address
 * See header file for detailed comment.
 */
void meshlink_add_address_hint(meshlink_handle_t *mesh, meshlink_node_t *node, const struct sockaddr *addr) {
    if(!mesh || !node || !addr)
        return;

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    char *host = NULL, *port = NULL, *str = NULL;
    sockaddr2str((const sockaddr_t *)addr, &host, &port);

    if(host && port) {
        xasprintf(&str, "%s %s", host, port);
        if ( (strncmp ("fe80",host,4) != 0) && ( strncmp("127.",host,4) != 0 ) && ( strcmp("localhost",host) !=0 ) )
            append_config_file(mesh, node->name, "Address", str);
        else
            logger(mesh, MESHLINK_DEBUG, "Not adding Link Local IPv6 Address to config\n");
    }

    free(str);
    free(host);
    free(port);

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));
    // @TODO do we want to fire off a connection attempt right away?
}

/* Return an array of edges in the current network graph.
 * Data captures the current state and will not be updated.
 * Caller must deallocate data when done.
 */
meshlink_edge_t **meshlink_get_all_edges_state(meshlink_handle_t *mesh, meshlink_edge_t **edges, size_t *nmemb) {
    if(!mesh || !nmemb || (*nmemb && !edges)) {
        meshlink_errno = MESHLINK_EINVAL;
        return NULL;
    }

    MESHLINK_MUTEX_LOCK(&(mesh->mesh_mutex));

    meshlink_edge_t **result = NULL;
    meshlink_edge_t *copy = NULL;
    int result_size = 0;

    result_size = mesh->edges->count;

    // if result is smaller than edges, we have to dealloc all the excess meshlink_edge_t
    if(result_size > *nmemb) {
        result = realloc(edges, result_size * sizeof (meshlink_edge_t*));
    } else {
        result = edges;
    }

    if(result) {
        meshlink_edge_t **p = result;
        int n = 0;
        for splay_each(edge_t, e, mesh->edges) {
            // skip edges that do not represent a two-directional connection
            if((!e->reverse) || (e->reverse->to != e->from)) {
                result_size--;
                continue;
            }
            n++;
            // the first *nmemb members of result can be re-used
            if(n > *nmemb) {
                copy = xzalloc(sizeof *copy);
            }
            else {
                copy = *p;
            }
            copy->from = (meshlink_node_t*)e->from;
            copy->to = (meshlink_node_t*)e->to;
            copy->address = e->address.storage;
            copy->options = e->options;
            copy->weight = e->weight;
            *p++ = copy;
        }
        // shrink result to the actual amount of memory used
        for(int i = *nmemb; i > result_size; i--) {
            free(result[i - 1]);
        }
        result = realloc(result, result_size * sizeof (meshlink_edge_t*));
        *nmemb = result_size;
    } else {
        *nmemb = 0;
        meshlink_errno = MESHLINK_ENOMEM;
    }

    MESHLINK_MUTEX_UNLOCK(&(mesh->mesh_mutex));

    return result;
}

static bool channel_pre_accept(struct utcp *utcp, uint16_t port) {
    //TODO: implement
    return true;
}

static ssize_t channel_recv(struct utcp_connection *connection, const void *data, size_t len) {
    meshlink_channel_t *channel = connection->priv;
    if(!channel) {
        logger(NULL, MESHLINK_ERROR, "Error: channel_recv no channel");
        abort();
    }
    node_t *n = channel->node;
    meshlink_handle_t *mesh = n->mesh;
    meshlink_aio_buffer_t *aio;
    size_t done = 0;

    // If we have AIO buffers, use those first.
    while((aio = channel->aio_receive)) {
        // Call all outstanding AIO callbacks in case of an error
        if(len <= 0) {
            if(aio->cb)
                aio->cb(mesh, channel, aio->data, 0, aio->priv);
            channel->aio_receive = aio->next;
            free(aio);
            continue;
        };

        // Fill the current buffer up as much as possible
        size_t left = aio->len - aio->done;
        if(left > (len - done))
            left = len - done;
        memcpy((char *)aio->data + aio->done, (char *)data + done, left);
        aio->done += left;
        done += left;

        // AIO buffer full?
        if(aio->done >= aio->len) {
            if(aio->cb)
                aio->cb(mesh, channel, aio->data, aio->len, aio->priv);
            channel->aio_receive = aio->next;
            free(aio);
        }

        // Everything received processed?
        if(done >= len)
            return len;
    }

    if(!channel->receive_cb) {
        return done;
    } else {
        channel->receive_cb(mesh, channel, data + done, len - done);
        return len;
    }
}

static void channel_accept(struct utcp_connection *utcp_connection, uint16_t port) {
    node_t *n = utcp_connection->utcp->priv;
    if(!n) {
        logger(NULL, MESHLINK_ERROR, "Error: channel_accept no node");
        abort();
    }
    meshlink_handle_t *mesh = n->mesh;
    if(!mesh->channel_accept_cb)
        return;
    meshlink_channel_t *channel = xzalloc(sizeof *channel);
    channel->node = n;
    channel->c = utcp_connection;
    if(mesh->channel_accept_cb(mesh, channel, port, NULL, 0))
        utcp_accept(utcp_connection, channel_recv, channel);
    else
        free(channel);
}

static ssize_t channel_send(struct utcp *utcp, const void *data, size_t len) {
    node_t *n = utcp->priv;
    meshlink_handle_t *mesh = n->mesh;

    // check log level before calling bin2hex since that's an expensive call
    if(mesh->log_level <= MESHLINK_DEBUG) {
        // only do this if it will be logged
        char* hex = xzalloc(len * 2 + 1);
        bin2hex(data, hex, len);
        logger(mesh, MESHLINK_DEBUG, "channel_send(%p, %p, " PRINT_SIZE_T "): %s\n", utcp, data, len, hex);
        free(hex);
    }

    return meshlink_send(mesh, (meshlink_node_t *)n, data, len) ? len : -1;
}

void meshlink_set_channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, meshlink_channel_receive_cb_t cb) {
    if(!mesh || !channel) {
        meshlink_errno = MESHLINK_EINVAL;
        return;
    }

    MESHLINK_MUTEX_LOCK(&mesh->mesh_mutex);

    channel->receive_cb = cb;

    MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
}

static void channel_receive(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len) {
    node_t *n = (node_t *)source;
    if(!n->utcp) {
        logger(NULL, MESHLINK_ERROR, "Error: channel_receive !n->utcp");
        abort();
    }

    // check log level before calling bin2hex since that's an expensive call
    if(mesh->log_level <= MESHLINK_DEBUG) {
        char* hex = xzalloc(len * 2 + 1);
        bin2hex(data, hex, len);
        logger(mesh, MESHLINK_DEBUG, "channel_receive(%p, %p, " PRINT_SIZE_T "): %s\n", n->utcp, data, len, hex);
        free(hex);
    }

    utcp_recv(n->utcp, data, len);
}

static void channel_poll(struct utcp_connection *connection, size_t len) {
    meshlink_channel_t *channel = connection->priv;
    if(!channel) {
        logger(NULL, MESHLINK_ERROR, "Error: channel_poll no channel");
        abort();
    }

    node_t *n = channel->node;
    meshlink_handle_t *mesh = n->mesh;
    meshlink_aio_buffer_t *aio = channel->aio_send;

    logger(mesh, MESHLINK_DEBUG, "channel_poll(%p, " PRINT_SIZE_T ")\n", connection, len);

    // If we have AIO buffers queued, use those.
    if(aio) {
        while(aio && len > 0) {
            // AIO buffers are kept until they are ACKd, so some
            // buffers might be completely sent already
            if(aio->done >= aio->len) {
                aio = aio->next;
                continue;
            }

            // Send as much as possible.
            size_t left = aio->len - aio->done;
            if(len < left)
                left = len;
            ssize_t sent = utcp_send(connection, aio->data + aio->done, left);
            if(sent == -1) {
                logger(mesh, MESHLINK_ERROR, "Error: channel_poll could not pass data to utcp: utcp_send returned -1");
                break;
            } else if(sent == 0) {
                // utcp send buffer is full
                break;
            } else {
                aio->done += sent;
                len = sent > len ? 0 : len - sent;
            }

            aio = aio->next;
        }
    } else {
        if(channel->poll_cb)
            channel->poll_cb(mesh, channel, len);
        else
            utcp_set_poll_cb(connection, NULL);
    }
}

static void channel_ack(struct utcp_connection *connection, size_t len) {
    meshlink_channel_t *channel = connection->priv;
    if(!channel) {
        logger(NULL, MESHLINK_ERROR, "Error: channel_ack no channel");
        abort();
    }

    node_t *n = channel->node;
    meshlink_handle_t *mesh = n->mesh;
    meshlink_aio_buffer_t *aio = channel->aio_send;
    meshlink_aio_buffer_t *next = NULL;
    if(!aio)
        return;

    while(aio && len > 0)
    {
        size_t unackd = aio->len - aio->ackd;

        // ACK may cover some of aio and some of aio->next
        size_t ackd = len <= unackd ? len : unackd;
        aio->ackd += ackd;
        len -= ackd;

        next = aio->next;
        // If all data has been ACKd, call the callback and dispose of it.
        if(aio->ackd >= aio->len) {
            // TODO differentiate between 'all ACKd' and 'all sent' callbacks
            // to allow freeing aio->data once it is in utcp send buffer
            if(aio->cb)
                aio->cb(mesh, channel, aio->data, aio->len, aio->priv);
            channel->aio_send = aio->next;
            free(aio);
        }
        aio = next;
    }
}

static bool init_utcp(meshlink_handle_t *mesh, node_t *n) {
    logger(mesh, MESHLINK_WARNING, "utcp_init on node %s", n->name);

    n->utcp = utcp_init(channel_accept, channel_pre_accept, channel_send, n);
    if(!n->utcp) {
        meshlink_errno = errno == ENOMEM ? MESHLINK_ENOMEM : MESHLINK_EINTERNAL;
        return false;
    }

    update_node_mtu(mesh, n);

    return true;
}

void meshlink_set_channel_poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, meshlink_channel_poll_cb_t cb) {

    MESHLINK_MUTEX_LOCK(&mesh->mesh_mutex);

    channel->poll_cb = cb;
    utcp_set_poll_cb(channel->c, (cb || channel->aio_send) ? channel_poll : NULL);

    MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
}

void meshlink_set_channel_accept_cb(meshlink_handle_t *mesh, meshlink_channel_accept_cb_t cb) {
    if(!mesh) {
        meshlink_errno = MESHLINK_EINVAL;
        return;
    }

    MESHLINK_MUTEX_LOCK(&mesh->mesh_mutex);
    mesh->channel_accept_cb = cb;
    mesh->receive_cb = channel_receive;
    for splay_each(node_t, n, mesh->nodes) {
        if(!n->utcp && n != mesh->self) {
            init_utcp(mesh, n);
        }
    }
    MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
}

meshlink_channel_t *meshlink_channel_open(meshlink_handle_t *mesh, meshlink_node_t *node, uint16_t port, meshlink_channel_receive_cb_t cb, const void *data, size_t len) {
    if(!mesh || !node) {
        meshlink_errno = MESHLINK_EINVAL;
        return NULL;
    }

    MESHLINK_MUTEX_LOCK(&mesh->mesh_mutex);

    logger(mesh, MESHLINK_WARNING, "meshlink_channel_open(%p, %s, %u, %p, %p, " PRINT_SIZE_T ")\n", mesh, node->name, port, cb, data, len);
    node_t *n = (node_t *)node;
    if(!n->utcp) {
        if(!init_utcp(mesh, n)) {
            MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
            return NULL;
        }

        mesh->receive_cb = channel_receive;
    }
    meshlink_channel_t *channel = xzalloc(sizeof *channel);
    channel->node = n;
    channel->receive_cb = cb;
    channel->c = utcp_connect(n->utcp, port, channel_recv, channel);
    if(!channel->c) {
        meshlink_errno = errno == ENOMEM ? MESHLINK_ENOMEM : MESHLINK_EINTERNAL;
        free(channel);
        MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
        return NULL;
    }
    MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
    return channel;
}

bool meshlink_channel_set_cwnd_max(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint32_t max) {
    if(!mesh || !channel) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    return utcp_set_cwnd_max(channel->c, max);
}

bool meshlink_channel_get_cwnd_max(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint32_t *max) {
    if(!mesh || !channel || !channel->c || !max) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    return utcp_get_cwnd_max(channel->c, max);
}

bool meshlink_channel_set_rtrx_tolerance(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint32_t tolerance) {
    if(!mesh || !channel) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    return utcp_set_rtrx_tolerance(channel->c, tolerance);
}

bool meshlink_channel_get_rtrx_tolerance(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint32_t *tolerance) {
    if(!mesh || !channel || !channel->c) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    return utcp_get_rtrx_tolerance(channel->c, tolerance);
}

void meshlink_channel_shutdown(meshlink_handle_t *mesh, meshlink_channel_t *channel, int direction) {
    if(!mesh || !channel) {
        meshlink_errno = MESHLINK_EINVAL;
        return;
    }

    MESHLINK_MUTEX_LOCK(&mesh->mesh_mutex);

    utcp_shutdown(channel->c, direction);

    MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
}

void meshlink_channel_close(meshlink_handle_t *mesh, meshlink_channel_t *channel) {
    if(!mesh || !channel) {
        meshlink_errno = MESHLINK_EINVAL;
        return;
    }

    MESHLINK_MUTEX_LOCK(&mesh->mesh_mutex);

    utcp_close(channel->c);
    for(meshlink_aio_buffer_t *aio = channel->aio_send, *next; aio; aio = next) {
        next = aio->next;
        if(aio->cb)
            aio->cb(mesh, channel, aio->data, 0, aio->priv);
        free(aio);
    }
    for(meshlink_aio_buffer_t *aio = channel->aio_receive, *next; aio; aio = next) {
        next = aio->next;
        if(aio->cb)
            aio->cb(mesh, channel, aio->data, 0, aio->priv);
        free(aio);
    }
    free(channel);

    MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);
}

ssize_t meshlink_channel_send(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
    if(!mesh || !channel) {
        meshlink_errno = MESHLINK_EINVAL;
        return -1;
    }

    if(!len)
        return 0;

    if(!data) {
        meshlink_errno = MESHLINK_EINVAL;
        return -1;
    }

    // TODO: more finegrained locking.
    // Ideally we want to put the data into the UTCP connection's send buffer.
    // Then, preferrably only if there is room in the receiver window,
    // kick the meshlink thread to go send packets.

    ssize_t retval;

    MESHLINK_MUTEX_LOCK(&mesh->mesh_mutex);
    if(channel->aio_send)
        retval = 0; // Don't allow direct calls to utcp_send() while we are processing AIO.
    else
        retval = utcp_send(channel->c, data, len);
    MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);

    if(retval < 0)
        meshlink_errno = MESHLINK_ENETWORK;
    return retval;
}

bool meshlink_channel_aio_send(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, meshlink_aio_cb_t cb, void *priv) {
    if(!mesh || !channel) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    if(!len || !data) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    struct meshlink_aio_buffer *aio = xzalloc(sizeof *aio);

    aio->data = (void *)data;
    aio->len = len;
    aio->cb = cb;
    aio->priv = priv;

    MESHLINK_MUTEX_LOCK(&mesh->mesh_mutex);
    struct meshlink_aio_buffer **p = &channel->aio_send;
    while(*p)
        p = &(*p)->next;
    *p = aio;

    utcp_set_poll_cb(channel->c, channel_poll);
    utcp_set_ack_cb(channel->c, channel_ack);
    channel_poll(channel->c, len);
    MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);

    return true;
}

bool meshlink_channel_aio_receive(meshlink_handle_t *mesh, meshlink_channel_t *channel, void *data, size_t len, meshlink_aio_cb_t cb, void *priv) {
    if(!mesh || !channel) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    if(!len || !data) {
        meshlink_errno = MESHLINK_EINVAL;
        return false;
    }

    struct meshlink_aio_buffer *aio = xzalloc(sizeof *aio);

    aio->data = data;
    aio->len = len;
    aio->cb = cb;
    aio->priv = priv;

    MESHLINK_MUTEX_LOCK(&mesh->mesh_mutex);
    struct meshlink_aio_buffer **p = &channel->aio_receive;
    while(*p)
        p = &(*p)->next;
    *p = aio;
    MESHLINK_MUTEX_UNLOCK(&mesh->mesh_mutex);

    return true;
}

void update_node_status(meshlink_handle_t *mesh, node_t *n) {
    if(n->status.reachable && mesh->channel_accept_cb && !n->utcp)
        init_utcp(mesh, n);
    if(mesh->node_status_cb)
        mesh->node_status_cb(mesh, (meshlink_node_t *)n, n->status.reachable);
}

void update_node_mtu(meshlink_handle_t *mesh, node_t *n) {
    if(!mesh||!n)
        return;

    uint16_t mtu = n->mtu > sizeof(meshlink_packethdr_t)? n->mtu - sizeof(meshlink_packethdr_t): 0;

    // set utcp maximum transmission unit size, determined by net_packet.c probing packets via send_sptps_packet
    // 1500 bytes usable space for the ethernet frame
    // - 20 bytes IPv4-Header
    // -  8 bytes UDP-Header
    // - 19 to 21 bytes encryption (sptps.c send_record_priv / send_record_priv_datagram)
    // - 66 bytes Meshlink packet header ( source & destination node names )
    // - 20 bytes UTCP-Header size subtracted internally by utcp
    // = about 1365 bytes payload left
    if(n->utcp)
        mtu = utcp_update_mtu(n->utcp, mtu);

    if(mesh->node_pmtu_cb)
        mesh->node_pmtu_cb(mesh, (meshlink_node_t *)n, mtu);
}

static void __attribute__((constructor)) meshlink_init(void) {
    crypto_init();
}

static void __attribute__((destructor)) meshlink_exit(void) {
    crypto_exit();
}

/// Device class traits
dev_class_traits_t dev_class_traits[_DEV_CLASS_MAX +1] = {
    { .min_connects = 3, .max_connects = 10000, .edge_weight = 1 },    // DEV_CLASS_BACKBONE
    { .min_connects = 3, .max_connects = 100, .edge_weight = 3 },    // DEV_CLASS_STATIONARY
    { .min_connects = 3, .max_connects = 3, .edge_weight = 6 },        // DEV_CLASS_PORTABLE
    { .min_connects = 1, .max_connects = 1, .edge_weight = 9 },        // DEV_CLASS_UNKNOWN
};
