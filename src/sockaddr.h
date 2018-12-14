#ifndef MESHLINK_SOCKADDR_H
#define MESHLINK_SOCKADDR_H

#define AF_UNKNOWN 255

#ifdef SA_LEN
#define SALEN(s) SA_LEN(&(s))
#else
#define SALEN(s) ((s).sa_family==AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))
#endif

struct sockaddr_unknown {
	uint16_t family;
	uint16_t pad1;
	uint32_t pad2;
	char *address;
	char *port;
};

typedef union sockaddr_t {
	struct sockaddr sa;
	struct sockaddr_in in;
	struct sockaddr_in6 in6;
	struct sockaddr_unknown unknown;
	struct sockaddr_storage storage;
} sockaddr_t;

#endif
