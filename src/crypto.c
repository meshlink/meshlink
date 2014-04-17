/*
    crypto.c -- Cryptographic miscellaneous functions and initialisation
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

#include "crypto.h"

//TODO: use a strict random source once to seed a PRNG?

static int random_fd = -1;

void crypto_init(void) {
	random_fd = open("/dev/urandom", O_RDONLY);
	if(random_fd < 0)
		random_fd = open("/dev/random", O_RDONLY);
	if(random_fd < 0) {
		fprintf(stderr, "Could not open source of random numbers: %s\n", strerror(errno));
		abort();
	}
}

void crypto_exit(void) {
}

void randomize(void *out, size_t outlen) {
	if(read(random_fd, out, outlen) != outlen) {
		fprintf(stderr, "Error reading random numbers: %s\n", strerror(errno));
		abort();
	}
}
