/*
    prf.c -- Pseudo-Random Function for key material generation
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

#include "prf.h"
#include "ed25519/sha512.h"

static void memxor(char *buf, char c, size_t len) {
	assert(buf);
	assert(len);

	for(size_t i = 0; i < len; i++) {
		buf[i] ^= c;
	}
}

#define mdlen 64

// TODO: separate key setup from hmac_sha512

static bool hmac_sha512(const char *key, size_t keylen, const char *msg, size_t msglen, char *out) {
	assert(msg);
	assert(msglen);

	char tmp[2 * mdlen];
	sha512_context md;

	if(keylen <= mdlen) {
		memcpy(tmp, key, keylen);
		memset(tmp + keylen, 0, mdlen - keylen);
	} else {
		if(sha512(key, keylen, tmp) != 0) {
			return false;
		}
	}

	if(sha512_init(&md) != 0) {
		return false;
	}

	// ipad
	memxor(tmp, 0x36, mdlen);

	if(sha512_update(&md, tmp, mdlen) != 0) {
		return false;
	}

	// message
	if(sha512_update(&md, msg, msglen) != 0) {
		return false;
	}

	if(sha512_final(&md, tmp + mdlen) != 0) {
		return false;
	}

	// opad
	memxor(tmp, 0x36 ^ 0x5c, mdlen);

	if(sha512(tmp, sizeof(tmp), out) != 0) {
		return false;
	}

	return true;
}

/* Generate key material from a master secret and a seed, based on RFC 4346 section 5.
   We use SHA512 instead of MD5 and SHA1.
 */

bool prf(const char *secret, size_t secretlen, const char *seed, size_t seedlen, char *out, size_t outlen) {
	assert(secret);
	assert(secretlen);
	assert(seed);
	assert(seedlen);
	assert(out);
	assert(outlen);

	/* Data is what the "inner" HMAC function processes.
	   It consists of the previous HMAC result plus the seed.
	 */

	char data[mdlen + seedlen];
	memset(data, 0, mdlen);
	memcpy(data + mdlen, seed, seedlen);

	char hash[mdlen];

	while(outlen > 0) {
		/* Inner HMAC */
		if(!hmac_sha512(data, sizeof(data), secret, secretlen, data)) {
			return false;
		}

		/* Outer HMAC */
		if(outlen >= mdlen) {
			if(!hmac_sha512(data, sizeof(data), secret, secretlen, out)) {
				return false;
			}

			out += mdlen;
			outlen -= mdlen;
		} else {
			if(!hmac_sha512(data, sizeof(data), secret, secretlen, hash)) {
				return false;
			}

			memcpy(out, hash, outlen);
			out += outlen;
			outlen = 0;
		}
	}

	return true;
}
