#ifndef MESHLINK_ECDSA_H
#define MESHLINK_ECDSA_H

/*
    ecdsa.h -- ECDSA key handling
    Copyright (C) 2014, 2017 Guus Sliepen <guus@meshlink.io>

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

#ifndef __MESHLINK_ECDSA_INTERNAL__
typedef struct ecdsa ecdsa_t;
#endif

ecdsa_t *ecdsa_set_private_key(const void *p) __attribute__((__malloc__));
ecdsa_t *ecdsa_set_base64_public_key(const char *p) __attribute__((__malloc__));
ecdsa_t *ecdsa_set_public_key(const void *p) __attribute__((__malloc__));
char *ecdsa_get_base64_public_key(ecdsa_t *ecdsa);
const void *ecdsa_get_public_key(ecdsa_t *ecdsa) __attribute__((__malloc__));
const void *ecdsa_get_private_key(ecdsa_t *ecdsa) __attribute__((__malloc__));
ecdsa_t *ecdsa_read_pem_public_key(FILE *fp) __attribute__((__malloc__));
ecdsa_t *ecdsa_read_pem_private_key(FILE *fp) __attribute__((__malloc__));
size_t ecdsa_size(ecdsa_t *ecdsa);
bool ecdsa_sign(ecdsa_t *ecdsa, const void *in, size_t inlen, void *out) __attribute__((__warn_unused_result__));
bool ecdsa_verify(ecdsa_t *ecdsa, const void *in, size_t inlen, const void *out) __attribute__((__warn_unused_result__));
bool ecdsa_active(ecdsa_t *ecdsa);
void ecdsa_free(ecdsa_t *ecdsa);

#endif
