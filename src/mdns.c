// SPDX-FileCopyrightText: 2020 Guus Sliepen <guus@meshlink.io>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "system.h"

#include <stddef.h>

#include "mdns.h"
#include "xalloc.h"

// Creating a buffer

typedef struct {
	uint8_t *ptr;
	ptrdiff_t len;
} buf_t;

static void buf_add(buf_t *buf, const void *data, uint32_t len) {
	if(buf->len >= len) {
		memcpy(buf->ptr, data, len);
		buf->ptr += len;
		buf->len -= len;
	} else {
		buf->len = -1;
	}
}

static void buf_add_uint8(buf_t *buf, uint8_t val) {
	if(buf->len >= 1) {
		buf->ptr[0] = val;
		buf->ptr++;
		buf->len--;
	} else {
		buf->len = -1;
	}
}

static void buf_add_uint16(buf_t *buf, uint16_t val) {
	uint16_t nval = htons(val);
	buf_add(buf, &nval, sizeof(nval));
}

static void buf_add_uint32(buf_t *buf, uint32_t val) {
	uint32_t nval = htonl(val);
	buf_add(buf, &nval, sizeof(nval));
}

static void buf_add_label(buf_t *buf, const char *str) {
	size_t len = strlen(str);

	if(len < 256) {
		buf_add_uint8(buf, len);
		buf_add(buf, str, len);
	} else {
		buf->len = -1;
	}
}

static void buf_add_ulabel(buf_t *buf, const char *str) {
	size_t len = strlen(str);

	if(len + 1 < 256) {
		buf_add_uint8(buf, len + 1);
		buf_add_uint8(buf, '_');
		buf_add(buf, str, len);
	} else {
		buf->len = -1;
	}
}

static void buf_add_kvp(buf_t *buf, const char *key, const char *val) {
	size_t key_len = strlen(key);
	size_t val_len = strlen(val);

	if(key_len + val_len + 1 < 256) {
		buf_add_uint8(buf, key_len + val_len + 1);
		buf_add(buf, key, key_len);
		buf_add_uint8(buf, '=');
		buf_add(buf, val, val_len);
	} else {
		buf->len = -1;
	}
}

static uint8_t *buf_len_start(buf_t *buf) {
	if(buf->len < 2) {
		buf->len = -1;
		return NULL;
	} else {
		uint8_t *ptr = buf->ptr;
		buf->ptr += 2;
		buf->len -= 2;
		return ptr;
	}
}

static void buf_len_end(buf_t *buf, uint8_t *ptr) {
	if(buf->len < 0) {
		return;
	}

	uint16_t len = htons(buf->ptr - ptr - 2);
	memcpy(ptr, &len, sizeof(len));
}

// Functions reading a buffer

typedef struct {
	const uint8_t *ptr;
	ptrdiff_t len;
} cbuf_t;

static void buf_check(cbuf_t *buf, const void *data, uint32_t len) {
	if(buf->len >= len && !memcmp(buf->ptr, data, len)) {
		buf->ptr += len;
		buf->len -= len;
	} else {
		buf->len = -1;
	}
}

static void buf_check_uint8(cbuf_t *buf, uint8_t val) {
	if(buf->len >= 1 && buf->ptr[0] == val) {
		buf->ptr++;
		buf->len--;
	} else {
		buf->len = -1;
	}
}

static void buf_check_uint16(cbuf_t *buf, uint16_t val) {
	uint16_t nval = htons(val);
	buf_check(buf, &nval, sizeof(nval));
}

static uint16_t buf_get_uint16(cbuf_t *buf) {
	uint16_t nval;

	if(buf->len >= 2) {
		memcpy(&nval, buf->ptr, 2);
		buf->ptr += 2;
		buf->len -= 2;
		return ntohs(nval);
	} else {
		buf->len = -1;
		return 0;
	}
}

static void buf_check_uint32(cbuf_t *buf, uint32_t val) {
	uint32_t nval = htonl(val);
	buf_check(buf, &nval, sizeof(nval));
}

static void buf_check_label(cbuf_t *buf, const char *str) {
	size_t len = strlen(str);

	if(len < 256) {
		buf_check_uint8(buf, len);
		buf_check(buf, str, len);
	} else {
		buf->len = -1;
	}
}

static char *buf_get_label(cbuf_t *buf) {
	if(buf->len < 1) {
		buf->len = -1;
		return NULL;
	}

	uint8_t len = buf->ptr[0];
	buf->ptr++;
	buf->len--;

	if(buf->len < len) {
		buf->len = -1;
		return NULL;
	}

	char *label = xmalloc(len + 1);
	memcpy(label, buf->ptr, len);
	label[len] = 0;
	buf->ptr += len;
	buf->len -= len;
	return label;
}

static void buf_check_ulabel(cbuf_t *buf, const char *str) {
	size_t len = strlen(str);

	if(len + 1 < 256) {
		buf_check_uint8(buf, len + 1);
		buf_check_uint8(buf, '_');
		buf_check(buf, str, len);
	} else {
		buf->len = -1;
	}
}

static void buf_get_kvp(cbuf_t *buf, const char *key, char **val) {
	char *kvp = buf_get_label(buf);

	if(buf->len == -1) {
		return;
	}

	char *split = strchr(kvp, '=');

	if(!split) {
		buf->len = -1;
		return;
	}

	*split++ = 0;

	if(strcmp(kvp, key)) {
		buf->len = -1;
		return;
	}

	memmove(kvp, split, strlen(split) + 1);
	*val = kvp;
}

static const uint8_t *buf_check_len_start(cbuf_t *buf) {
	if(buf->len < 2) {
		buf->len = -1;
		return NULL;
	} else {
		const uint8_t *ptr = buf->ptr;
		buf->ptr += 2;
		buf->len -= 2;
		return ptr;
	}
}

static void buf_check_len_end(cbuf_t *buf, const uint8_t *ptr) {
	if(buf->len < 0) {
		return;
	}

	uint16_t len = htons(buf->ptr - ptr - 2);

	if(memcmp(ptr, &len, sizeof(len))) {
		buf->len = -1;
	}
}

size_t prepare_request(void *vdata, size_t size, const char *protocol, const char *transport) {
	uint8_t *data = vdata;
	buf_t buf = {data, size};

	// Header
	buf_add_uint16(&buf, 0); // TX ID
	buf_add_uint16(&buf, 0); // flags
	buf_add_uint16(&buf, 1); // 1 question
	buf_add_uint16(&buf, 0); // 0 answer RR
	buf_add_uint16(&buf, 0); // 0 authority RRs
	buf_add_uint16(&buf, 0); // 0 additional RR

	// Question section: _protocol._transport.local PTR IN
	buf_add_ulabel(&buf, protocol);
	buf_add_ulabel(&buf, transport);
	buf_add_label(&buf, "local");
	buf_add_uint8(&buf, 0);
	buf_add_uint16(&buf, 0xc); // PTR
	buf_add_uint16(&buf, 0x1); // IN

	// Done.
	if(buf.len < 0) {
		return 0;
	} else {
		return buf.ptr - data;
	}
}

bool parse_request(const void *vdata, size_t size, const char *protocol, const char *transport) {
	const uint8_t *data = vdata;
	cbuf_t buf = {data, size};

	// Header
	buf_get_uint16(&buf); // TX ID
	buf_check_uint16(&buf, 0); // flags
	buf_check_uint16(&buf, 1); // 1 question
	buf_get_uint16(&buf); // ? answer RR
	buf_get_uint16(&buf); // ? authority RRs
	buf_get_uint16(&buf); // ? additional RR

	if(buf.len == -1) {
		return false;
	}

	// Question section: _protocol._transport.local PTR IN
	buf_check_ulabel(&buf, protocol);
	buf_check_ulabel(&buf, transport);
	buf_check_label(&buf, "local");
	buf_check_uint8(&buf, 0);
	buf_check_uint16(&buf, 0xc); // PTR
	buf_check_uint16(&buf, 0x1); // IN

	if(buf.len == -1) {
		return false;
	}

	// Done.
	return buf.len != -1;
}

size_t prepare_response(void *vdata, size_t size, const char *name, const char *protocol, const char *transport, uint16_t port, int nkeys, const char **keys, const char **values) {
	uint8_t *data = vdata;
	buf_t buf = {data, size};

	// Header
	buf_add_uint16(&buf, 0); // TX ID
	buf_add_uint16(&buf, 0x8400); // flags
	buf_add_uint16(&buf, 0); // 1 question
	buf_add_uint16(&buf, 3); // 1 answer RR
	buf_add_uint16(&buf, 0); // 0 authority RRs
	buf_add_uint16(&buf, 0); // 1 additional RR

	// Add the TXT record: _protocol._transport local TXT IN 3600 name._protocol._transport key=value...
	uint16_t full_name = buf.ptr - data; // remember start of full name
	buf_add_label(&buf, name);
	uint16_t protocol_offset = buf.ptr - data; // remember start of _protocol
	buf_add_ulabel(&buf, protocol);
	buf_add_ulabel(&buf, transport);
	uint16_t local_offset = buf.ptr - data; // remember start of local
	buf_add_label(&buf, "local");
	buf_add_uint8(&buf, 0);
	buf_add_uint16(&buf, 0x10); // TXT
	buf_add_uint16(&buf, 0x1); // IN
	buf_add_uint32(&buf, 3600); // TTL

	uint8_t *len_ptr = buf_len_start(&buf);

	for(int i = 0; i < nkeys; i++) {
		buf_add_kvp(&buf, keys[i], values[i]);
	}

	buf_len_end(&buf, len_ptr);

	// Add the PTR record: _protocol._transport.local PTR IN 3600 name._protocol._transport.local
	buf_add_uint16(&buf, 0xc000 | protocol_offset);
	buf_add_uint16(&buf, 0xc); // PTR
	buf_add_uint16(&buf, 0x8001); // IN (flush)
	buf_add_uint32(&buf, 3600); // TTL
	len_ptr = buf_len_start(&buf);
	buf_add_uint16(&buf, 0xc000 | full_name);
	buf_len_end(&buf, len_ptr);

	// Add the SRV record: name._protocol._transport.local SRV IN 120 0 0 port name.local
	buf_add_uint16(&buf, 0xc000 | full_name);
	buf_add_uint16(&buf, 0x21); // SRV
	buf_add_uint16(&buf, 0x8001); // IN (flush)
	buf_add_uint32(&buf, 120); // TTL
	len_ptr = buf_len_start(&buf);
	buf_add_uint16(&buf, 0); // priority
	buf_add_uint16(&buf, 0); // weight
	buf_add_uint16(&buf, port); // port
	buf_add_label(&buf, name);
	buf_add_uint16(&buf, 0xc000 | local_offset);
	buf_len_end(&buf, len_ptr);

	// Done.
	if(buf.len < 0) {
		return 0;
	} else {
		return buf.ptr - data;
	}
}

bool parse_response(const void *vdata, size_t size, char **name, const char *protocol, const char *transport, uint16_t *port, int nkeys, const char **keys, char **values) {
	const uint8_t *data = vdata;
	cbuf_t buf = {data, size};

	// Header
	buf_check_uint16(&buf, 0); // TX ID
	buf_check_uint16(&buf, 0x8400); // flags
	buf_check_uint16(&buf, 0); // 0 question
	buf_check_uint16(&buf, 3); // 1 answer RR
	buf_check_uint16(&buf, 0); // 0 authority RRs
	buf_check_uint16(&buf, 0); // 0 checkitional RR

	if(buf.len == -1) {
		return false;
	}

	// Check the TXT record: _protocol._transport local TXT IN 3600 name._protocol._transport key=value...
	uint16_t full_name = buf.ptr - data; // remember start of full name
	*name = buf_get_label(&buf);
	uint16_t protocol_offset = buf.ptr - data; // remember start of _protocol
	buf_check_ulabel(&buf, protocol);
	buf_check_ulabel(&buf, transport);
	uint16_t local_offset = buf.ptr - data; // remember start of local
	buf_check_label(&buf, "local");
	buf_check_uint8(&buf, 0);
	buf_check_uint16(&buf, 0x10); // TXT
	buf_check_uint16(&buf, 0x1); // IN
	buf_check_uint32(&buf, 3600); // TTL
	const uint8_t *len_ptr = buf_check_len_start(&buf);

	for(int i = 0; i < nkeys; i++) {
		buf_get_kvp(&buf, keys[i], &values[i]);
	}

	buf_check_len_end(&buf, len_ptr);

	if(buf.len == -1) {
		return false;
	}

	// Check the PTR record: _protocol._transport.local PTR IN 3600 name._protocol._transport.local
	buf_check_uint16(&buf, 0xc000 | protocol_offset);
	buf_check_uint16(&buf, 0xc); // PTR
	buf_check_uint16(&buf, 0x8001); // IN (flush)
	buf_check_uint32(&buf, 3600); // TTL
	len_ptr = buf_check_len_start(&buf);
	buf_check_uint16(&buf, 0xc000 | full_name);
	buf_check_len_end(&buf, len_ptr);

	if(buf.len == -1) {
		return false;
	}

	// Check the SRV record: name._protocol._transport.local SRV IN 120 0 0 port name.local
	buf_check_uint16(&buf, 0xc000 | full_name);
	buf_check_uint16(&buf, 0x21); // SRV
	buf_check_uint16(&buf, 0x8001); // IN (flush)
	buf_check_uint32(&buf, 120); // TTL
	len_ptr = buf_check_len_start(&buf);
	buf_check_uint16(&buf, 0); // priority
	buf_check_uint16(&buf, 0); // weight
	*port = buf_get_uint16(&buf); // port
	buf_check_label(&buf, *name);
	buf_check_uint16(&buf, 0xc000 | local_offset);
	buf_check_len_end(&buf, len_ptr);

	// Done.
	return buf.len == 0;
}
