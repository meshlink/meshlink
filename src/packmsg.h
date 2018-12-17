#pragma once

/*
    SPDX-License-Identifier: BSD-3-Clause

    packmsg.h -- Little-endian MessagePack implementation, optimized for speed
    Copyright (C) 2018 Guus Sliepen <guus@tinc-vpn.org>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the University nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
    DAMAGE.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define packmsg_likely(x) __builtin_expect(!!(x), 1)
#define packmsg_unlikely(x) __builtin_expect(!!(x), 0)

/** \mainpage PackMessage, a safe and fast header-only C library for little-endian MessagePack encoding and decoding.
 *
 * This library can encode and decode MessagePack objects, however it differs in one important point
 * from the official MessagePack specification: PackMessage stores all values in little-endian format.
 * PackMessage offers a simple streaming API for encoding and decoding.
 *
 * PackMessage is *safe*:
 *
 * * Reads from and writes to buffers are always bounds checked.
 * * String, binary and extension data can be read into buffers allocated by PackMessage using simple API calls.
 * * Any error will result in null values and pointers being returned, and/or application-allocated buffers for strings will be zero-terminated, so there is no undefined state.
 * * Once an encoding/decoding error occurs, all subsequent operations on the same buffer will also fail.
 * * The API is designed to follow the principle of least surprise, and makes it hard to use in a wrong way.
 *
 * PackMessage is *fast*:
 *
 * * Values are stored in little-endian format, since virtually all mainstream processors are little-endian, or they can switch between endianness and are probably running an operating system that has configured it to be little-endian. This saves the overhead of converting to and from big-endian format.
 * * No memory allocation is done unless requested.
 * * The application can get const pointers to string, binary and extension data pointing into the input buffer if desired, avoiding copies.
 * * The application does not have to check for errors after for every operation; it can be done once after encoding/decoding a buffer if desired.
 * * The library is header-only, allowing the compiler to inline all functions and better optimize your application.
 *
 * ## API overview
 *
 * For encoding, a packmsg_output_t variable must be initialized
 * with a pointer to the start of an output buffer, and its size.
 * Elements can then be encoded using packmsg_add_*() functions.
 * When all desired elements have been added, the length of the encoded message
 * can be retrieved using the packmsg_output_size() function.
 *
 * For decoding, a packmsg_input_t variable must be initialized
 * with a const pointer to the start of an input buffer, and its size.
 * Elements can then be decoded using packmsg_get_*() functions.
 * If the type of elements in a message is not known up front, then
 * the type of the next element can be queried using packmsg_get_type()
 * or packmsg_is_*() functions. To check that the complete message has been decoded
 * correctly, the function packmsg_done() can be called.
 *
 * ## Example code
 *
 * @ref example.c
 *
 * \example example.c
 *
 * This is an example of how to encode and decode the equivalent of the JSON object `{"compact": true, "schema": 0}`
 * using PackMessage.
 */

/* Buffer iterators
 * ================
 */

/** \brief Iterator for PackMessage output.
 *
 * This is an iterator that has to be initialized with a pointer to
 * an output buffer that is allocated by the application,
 * and the length of that buffer. A pointer to it is passed to all
 * packmsg_add_*() functions.
 */
typedef struct packmsg_output {
	uint8_t *ptr;  /**< A pointer into a buffer. */
	ptrdiff_t len; /**< The remaining length of the buffer, or -1 in case of errors. */
} packmsg_output_t;

/** \brief Iterator for PackMessage input.
 *
 * This is an iterator that has to be initialized with a pointer to
 * an input buffer that is allocated by the application,
 * and the length of that buffer. A pointer to it is passed to all
 * packmsg_get_*() functions.
 */
typedef struct packmsg_input {
	const uint8_t *ptr; /**< A pointer into a buffer. */
	ptrdiff_t len;      /**< The remaining length of the buffer, or -1 in case of errors. */
} packmsg_input_t;

/* Checks
 * ======
 */

/** \brief Check if the PackMessage output buffer is in a valid state.
 *  \memberof packmsg_output
 *
 * This function checks if all operations performed on the output buffer so far
 * have all completed succesfully, and the buffer contains a valid PackMessage message.
 *
 * \param buf  A pointer to an output buffer iterator.
 *
 * \return     True if all write operations performed on the output buffer so far have completed successfully,
 *             false if any error has occurred.
 */
static inline bool packmsg_output_ok(const packmsg_output_t *buf) {
	assert(buf);

	return packmsg_likely(buf->len >= 0);
}

/** \brief Calculate the amount of bytes written to the output buffer.
 *  \memberof packmsg_output
 *
 * This function calculates the amount of bytes written to the output buffer
 * based on the current position of the output iterator, and a pointer to the start of the buffer.
 *
 * \param buf    A pointer to an output buffer iterator.
 * \param start  A pointer to the start of the output buffer.
 *
 * \return       The total amount of bytes written to the output buffer,
 *               or 0 if any error has occurred.
 */
static inline size_t packmsg_output_size(const packmsg_output_t *buf, const uint8_t *start) {
	if(packmsg_likely(packmsg_output_ok(buf))) {
		return buf->ptr - start;
	} else {
		return 0;
	}
}

/** \brief Check if the PackMessage input buffer is in a valid state.
 *  \memberof packmsg_input
 *
 * This function checks if all operations performed on the input buffer so far
 * have all completed succesfully, and the buffer contains a valid PackMessage message.
 *
 * \param buf  A pointer to an input buffer iterator.
 *
 * \return     True if all read operations performed on the input buffer so far have completed successfully,
 *             false if any error has occurred.
 */
static inline bool packmsg_input_ok(const packmsg_input_t *buf) {
	assert(buf);

	return packmsg_likely(buf->len >= 0);
}

/** \brief Check if the PackMessage input buffer has been read completely.
 *  \memberof packmsg_input
 *
 * This function checks if all data in the input buffer has been consumed
 * by input operations. This function should always be called after the last
 * input operation, when one expects the whole buffer to have been read.
 *
 * \param buf  A pointer to an input buffer iterator.
 *
 * \return     True if the whole input buffer has been read successfully,
 *             false if there is still data remaining in the input buffer,
 *             or if any error has occurred.
 */
static inline bool packmsg_done(const packmsg_input_t *buf) {
	assert(buf);

	return buf->len == 0;
}

/* Invalidation functions
 * ======================
 */

/** \brief Invalidate an output iterator.
 *  \memberof packmsg_output
 *
 * This function invalidates an output iterator. This signals that an error occurred,
 * and prevents further output to be written.
 *
 * \param buf  A pointer to an output buffer iterator.
 */
static inline void packmsg_output_invalidate(packmsg_output_t *buf) {
	buf->len = -1;
}

/** \brief Invalidate an input iterator.
 *  \memberof packmsg_input
 *
 * This function invalidates an input iterator. This signals that an error occurred,
 * and prevents further input to be read.
 *
 * \param buf  A pointer to an input buffer iterator.
 */
static inline void packmsg_input_invalidate(packmsg_input_t *buf) {
	buf->len = -1;
}

/* Encoding functions
 * ==================
 */

/** \brief Internal function, do not use. */
static inline void packmsg_write_hdr_(packmsg_output_t *buf, uint8_t hdr) {
	assert(buf);
	assert(buf->ptr);

	if(packmsg_likely(buf->len > 0)) {
		*buf->ptr = hdr;
		buf->ptr++;
		buf->len--;
	} else {
		packmsg_output_invalidate(buf);
	}
}

/** \brief Internal function, do not use. */
static inline void packmsg_write_data_(packmsg_output_t *buf, const void *data, uint32_t dlen) {
	assert(buf);
	assert(buf->ptr);
	assert(data);

	if(packmsg_likely(buf->len >= dlen)) {
		memcpy(buf->ptr, data, dlen);
		buf->ptr += dlen;
		buf->len -= dlen;
	} else {
		packmsg_output_invalidate(buf);
	}
}

/** \brief Internal function, do not use. */
static inline void packmsg_write_hdrdata_(packmsg_output_t *buf, uint8_t hdr, const void *data, uint32_t dlen) {
	assert(buf);
	assert(buf->ptr);
	assert(data);

	if(packmsg_likely(buf->len > dlen)) {
		*buf->ptr = hdr;
		buf->ptr++;
		buf->len--;

		memcpy(buf->ptr, data, dlen);
		buf->ptr += dlen;
		buf->len -= dlen;
	} else {
		packmsg_output_invalidate(buf);
	}
}

/** \brief Internal function, do not use. */
static inline void *packmsg_reserve_(packmsg_output_t *buf, uint32_t len) {
	assert(buf);
	assert(buf->ptr);

	if(packmsg_likely(buf->len >= len)) {
		void *ptr = buf->ptr;
		buf->ptr += len;
		buf->len -= len;
		return ptr;
	} else {
		packmsg_output_invalidate(buf);
		return NULL;
	}
}

/** \brief Add a NIL to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 */
static inline void packmsg_add_nil(packmsg_output_t *buf) {
	packmsg_write_hdr_(buf, 0xc0);
}

/** \brief Add a boolean value to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param val  The value to add.
 */
static inline void packmsg_add_bool(packmsg_output_t *buf, bool val) {
	packmsg_write_hdr_(buf, val ? 0xc3 : 0xc2);
}

/** \brief Add an int8 value to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param val  The value to add.
 */
static inline void packmsg_add_int8(packmsg_output_t *buf, int8_t val) {
	if(val >= -32) {        // fixint
		packmsg_write_hdr_(buf, val);
	} else {                // TODO: negative fixint
		packmsg_write_hdrdata_(buf, 0xd0, &val, 1);
	}
}

/** \brief Add an int16 value to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param val  The value to add.
 */
static inline void packmsg_add_int16(packmsg_output_t *buf, int16_t val) {
	if((int8_t) val != val) {
		packmsg_write_hdrdata_(buf, 0xd1, &val, 2);
	} else {
		packmsg_add_int8(buf, val);
	}
}

/** \brief Add an int32 value to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param val  The value to add.
 */
static inline void packmsg_add_int32(packmsg_output_t *buf, int32_t val) {
	if((int16_t) val != val) {
		packmsg_write_hdrdata_(buf, 0xd2, &val, 4);
	} else {
		packmsg_add_int16(buf, val);
	}
}

/** \brief Add an int64 value to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param val  The value to add.
 */
static inline void packmsg_add_int64(packmsg_output_t *buf, int64_t val) {
	if((int32_t) val != val) {
		packmsg_write_hdrdata_(buf, 0xd3, &val, 8);
	} else {
		packmsg_add_int32(buf, val);
	}
}

/** \brief Add a uint8 value to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param val  The value to add.
 */
static inline void packmsg_add_uint8(packmsg_output_t *buf, uint8_t val) {
	if(val < 0x80) {        // fixint
		packmsg_write_hdr_(buf, val);
	} else {
		packmsg_write_hdrdata_(buf, 0xcc, &val, 1);
	}
}

/** \brief Add a uint16 value to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param val  The value to add.
 */
static inline void packmsg_add_uint16(packmsg_output_t *buf, uint16_t val) {
	if(val & 0xff00) {
		packmsg_write_hdrdata_(buf, 0xcd, &val, 2);
	} else {
		packmsg_add_uint8(buf, val);
	}
}

/** \brief Add a int32 value to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param val  The value to add.
 */
static inline void packmsg_add_uint32(packmsg_output_t *buf, uint32_t val) {
	if(val & 0xffff0000) {
		packmsg_write_hdrdata_(buf, 0xce, &val, 4);
	} else {
		packmsg_add_uint16(buf, val);
	}
}

/** \brief Add a int64 value to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param val  The value to add.
 */
static inline void packmsg_add_uint64(packmsg_output_t *buf, uint64_t val) {
	if(val & 0xffffffff00000000) {
		packmsg_write_hdrdata_(buf, 0xcf, &val, 8);
	} else {
		packmsg_add_uint32(buf, val);
	}
}

/** \brief Add a float value to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param val  The value to add.
 */
static inline void packmsg_add_float(packmsg_output_t *buf, float val) {
	packmsg_write_hdrdata_(buf, 0xca, &val, 4);
}

/** \brief Add a double value to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param val  The value to add.
 */
static inline void packmsg_add_double(packmsg_output_t *buf, double val) {
	packmsg_write_hdrdata_(buf, 0xcb, &val, 8);
}

/** \brief Add a string with a given length to the output.
 *  \memberof packmsg_output
 *
 * The string must be at least as long as the given length.
 * Any NUL-bytes within the given length range will be included.
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param str  The string to add.
 * \param len  The length of the string in bytes.
 */
static inline void packmsg_add_str_raw(packmsg_output_t *buf, const char *str, uint32_t len) {
	if(len < 32) {
		packmsg_write_hdr_(buf, 0xa0 | (uint8_t) len);
	} else if(len <= 0xff) {
		packmsg_write_hdrdata_(buf, 0xd9, &len, 1);
	} else if(len <= 0xffff) {
		packmsg_write_hdrdata_(buf, 0xda, &len, 2);
	} else {
		packmsg_write_hdrdata_(buf, 0xdb, &len, 4);
	}

	packmsg_write_data_(buf, str, len);
}

/** \brief Add a string to the output.
 *  \memberof packmsg_output
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param str  The string to add. This must be a NUL-terminated string.
 */
static inline void packmsg_add_str(packmsg_output_t *buf, const char *str) {
	size_t len = strlen(str);

	if(packmsg_likely(len <= 0xffffffff)) {
		packmsg_add_str_raw(buf, str, len);
	} else {
		packmsg_output_invalidate(buf);
	}
}

/** \brief Reserve space for a string with a given length in the output.
 *  \memberof packmsg_output
 *
 * This writes a header for a string with the given length to the output,
 * and reserves space for that string.
 * The caller must fill in that space.
 *
 * \param buf  A pointer to an output buffer iterator.
 * \param len  The length of the string in bytes.
 *
 * \return     A pointer to the reserved space for the string,
 *             or NULL in case of an error.
 */
static inline char *packmsg_add_str_reserve(packmsg_output_t *buf, uint32_t len) {
	if(len < 32) {
		packmsg_write_hdr_(buf, 0xa0 | (uint8_t) len);
	} else if(len <= 0xff) {
		packmsg_write_hdrdata_(buf, 0xd9, &len, 1);
	} else if(len <= 0xffff) {
		packmsg_write_hdrdata_(buf, 0xda, &len, 2);
	} else {
		packmsg_write_hdrdata_(buf, 0xdb, &len, 4);
	}

	return (char *)packmsg_reserve_(buf, len);
}

/** \brief Add binary data to the output.
 *  \memberof packmsg_output
 *
 * \param buf   A pointer to an output buffer iterator.
 * \param data  A pointer to the data to add.
 * \param dlen  The length of the data in bytes.
 */
static inline void packmsg_add_bin(packmsg_output_t *buf, const void *data, uint32_t dlen) {
	if(dlen <= 0xff) {
		packmsg_write_hdrdata_(buf, 0xc4, &dlen, 1);
	} else if(dlen <= 0xffff) {
		packmsg_write_hdrdata_(buf, 0xc5, &dlen, 2);
	} else {
		packmsg_write_hdrdata_(buf, 0xc6, &dlen, 4);
	}

	packmsg_write_data_(buf, data, dlen);
}

/** \brief Reserve space for binary data in the output.
 *  \memberof packmsg_output
 *
 * This writes a header for a block of data with the given length to the output,
 * and reserves space for that data.
 * The caller must fill in that space.
 *
 * \param buf   A pointer to an output buffer iterator.
 * \param dlen  The length of the data in bytes.
 *
 * \return      A pointer to the reserved space for the data,
 *              or NULL in case of an error.
 */
static inline void *packmsg_add_bin_reserve(packmsg_output_t *buf, uint32_t dlen) {
	if(dlen <= 0xff) {
		packmsg_write_hdrdata_(buf, 0xc4, &dlen, 1);
	} else if(dlen <= 0xffff) {
		packmsg_write_hdrdata_(buf, 0xc5, &dlen, 2);
	} else {
		packmsg_write_hdrdata_(buf, 0xc6, &dlen, 4);
	}

	return packmsg_reserve_(buf, dlen);
}

/** \brief Add extension data to the output.
 *  \memberof packmsg_output
 *
 * \param buf   A pointer to an output buffer iterator.
 * \param type  The extension type. Values between 0 and 127 are application specific,
 *              values between -1 and -128 are reserved for future extensions.
 * \param data  A pointer to the data to add.
 * \param dlen  The length of the data in bytes.
 */
static inline void packmsg_add_ext(packmsg_output_t *buf, int8_t type, const void *data, uint32_t dlen) {
	if(dlen <= 0xff) {
		if(dlen == 16) {
			packmsg_write_hdrdata_(buf, 0xd8, &type, 1);
		} else if(dlen == 8) {
			packmsg_write_hdrdata_(buf, 0xd7, &type, 1);
		} else if(dlen == 4) {
			packmsg_write_hdrdata_(buf, 0xd6, &type, 1);
		} else if(dlen == 2) {
			packmsg_write_hdrdata_(buf, 0xd5, &type, 1);
		} else if(dlen == 1) {
			packmsg_write_hdrdata_(buf, 0xd4, &type, 1);
		} else {
			packmsg_write_hdrdata_(buf, 0xc7, &dlen, 1);
			packmsg_write_data_(buf, &type, 1);
		}
	} else if(dlen <= 0xffff) {
		packmsg_write_hdrdata_(buf, 0xc8, &dlen, 2);
		packmsg_write_data_(buf, &type, 1);
	} else if(dlen <= 0xffffffff) {
		packmsg_write_hdrdata_(buf, 0xc9, &dlen, 4);
		packmsg_write_data_(buf, &type, 1);
	} else {
		packmsg_output_invalidate(buf);
		return;
	}

	packmsg_write_data_(buf, data, dlen);
}

/** \brief Reserve space for extension data in the output.
 *  \memberof packmsg_output
 *
 * This writes a header for extension data with the given type
 * and length to the output,
 * and reserves space for that extension data.
 * The caller must fill in that space.
 *
 * \param buf   A pointer to an output buffer iterator.
 * \param type  The extension type. Values between 0 and 127 are application specific,
 *              values between -1 and -128 are reserved for future extensions.
 * \param dlen  The length of the data in bytes.
 *
 * \return      A pointer to the reserved space for the extension data,
 *              or NULL in case of an error.
 */
static inline void *packmsg_add_ext_reserve(packmsg_output_t *buf, int8_t type, uint32_t dlen) {
	if(dlen <= 0xff) {
		if(dlen == 16) {
			packmsg_write_hdrdata_(buf, 0xd8, &type, 1);
		} else if(dlen == 8) {
			packmsg_write_hdrdata_(buf, 0xd7, &type, 1);
		} else if(dlen == 4) {
			packmsg_write_hdrdata_(buf, 0xd6, &type, 1);
		} else if(dlen == 2) {
			packmsg_write_hdrdata_(buf, 0xd5, &type, 1);
		} else if(dlen == 1) {
			packmsg_write_hdrdata_(buf, 0xd4, &type, 1);
		} else {
			packmsg_write_hdrdata_(buf, 0xc7, &dlen, 1);
			packmsg_write_data_(buf, &type, 1);
		}
	} else if(dlen <= 0xffff) {
		packmsg_write_hdrdata_(buf, 0xc8, &dlen, 2);
		packmsg_write_data_(buf, &type, 1);
	} else  {
		packmsg_write_hdrdata_(buf, 0xc9, &dlen, 4);
		packmsg_write_data_(buf, &type, 1);
	}

	return packmsg_reserve_(buf, dlen);
}

/** \brief Add a map header to the output.
 *  \memberof packmsg_output
 *
 * This function only adds an an indicator that the next 2 * count elements
 * are a sequence of key-value pairs that make up the contents of the map.
 * These key-value pairs have to be added by the application using regular
 * packmsg_add_*() calls.
 *
 * \param buf    A pointer to an output buffer iterator.
 * \param count  The number of elements in the map.
 */
static inline void packmsg_add_map(packmsg_output_t *buf, uint32_t count) {
	if(count <= 0xf) {
		packmsg_write_hdr_(buf, 0x80 | (uint8_t) count);
	} else if(count <= 0xffff) {
		packmsg_write_hdrdata_(buf, 0xde, &count, 2);
	} else {
		packmsg_write_hdrdata_(buf, 0xdf, &count, 4);
	}
}

/** \brief Add an array header to the output.
 *  \memberof packmsg_output
 *
 * This function only adds an an indicator that the next count elements
 * are a sequence of elements that make up the contents of the array.
 * These elements have to be added by the application using regular
 * packmsg_add_*() calls.
 *
 * \param buf    A pointer to an output buffer iterator.
 * \param count  The number of elements in the array.
 */
static inline void packmsg_add_array(packmsg_output_t *buf, uint32_t count) {
	if(count <= 0xf) {
		packmsg_write_hdr_(buf, 0x90 | (uint8_t) count);
	} else if(count <= 0xffff) {
		packmsg_write_hdrdata_(buf, 0xdc, &count, 2);
	} else {
		packmsg_write_hdrdata_(buf, 0xdd, &count, 4);
	}
}

/* Decoding functions
 * ==================
 */

/** \brief Internal function, do not use. */
static inline uint8_t packmsg_read_hdr_(packmsg_input_t *buf) {
	assert(buf);
	assert(buf->ptr);

	if(packmsg_likely(buf->len > 0)) {
		uint8_t hdr = *buf->ptr;
		buf->ptr++;
		buf->len--;
		return hdr;
	} else {
		packmsg_input_invalidate(buf);
		return 0xc1;
	}
}

/** \brief Internal function, do not use. */
static inline void packmsg_read_data_(packmsg_input_t *buf, void *data, uint32_t dlen) {
	assert(buf);
	assert(buf->ptr);
	assert(data);

	if(packmsg_likely(buf->len >= dlen)) {
		memcpy(data, buf->ptr, dlen);
		buf->ptr += dlen;
		buf->len -= dlen;
	} else {
		packmsg_input_invalidate(buf);
	}
}

/** \brief Internal function, do not use. */
static inline uint8_t packmsg_peek_hdr_(const packmsg_input_t *buf) {
	assert(buf);
	assert(buf->ptr);

	if(packmsg_likely(buf->len > 0)) {
		return *buf->ptr;
	} else {
		return 0xc1;
	}
}

/** \brief Get a NIL from the input.
 *  \memberof packmsg_input
 *
 * This function does not return anything, but will invalidate the input interator
 * if no NIL was succesfully consumed from the input.
 *
 * \param buf  A pointer to an input buffer iterator.
 */
static inline void packmsg_get_nil(packmsg_input_t *buf) {
	if(packmsg_read_hdr_(buf) != 0xc0) {
		packmsg_input_invalidate(buf);
	}
}


/** \brief Get a boolean value from the input.
 *  \memberof packmsg_input
 *
 * \param buf  A pointer to an input buffer iterator.
 * \return     The boolean value that was read from the input,
 *             or false in case of an error.
 */
static inline bool packmsg_get_bool(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if(hdr == 0xc2) {
		return false;
	} else if(hdr == 0xc3) {
		return true;
	} else {
		packmsg_input_invalidate(buf);
		return false;
	}
}

/** \brief Get an int8 value from the input.
 *  \memberof packmsg_input
 *
 * \param buf  A pointer to an input buffer iterator.
 * \return     The int8 value that was read from the input,
 *             or 0 in case of an error.
 */
static inline int8_t packmsg_get_int8(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if(hdr < 0x80 || hdr >= 0xe0) {
		return (int8_t)hdr;
	} else if(hdr == 0xd0) {
		return packmsg_read_hdr_(buf);
	} else {
		packmsg_input_invalidate(buf);
		return 0;
	}
}

/** \brief Get an int16 value from the input.
 *  \memberof packmsg_input
 *
 * \param buf  A pointer to an input buffer iterator.
 * \return     The int16 value that was read from the input,
 *             or 0 in case of an error.
 */
static inline int16_t packmsg_get_int16(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if(hdr < 0x80 || hdr >= 0xe0) {
		return (int8_t)hdr;
	} else if(hdr == 0xd0) {
		return (int8_t) packmsg_read_hdr_(buf);
	} else if(hdr == 0xd1) {
		int16_t val = 0;
		packmsg_read_data_(buf, &val, 2);
		return val;
	} else {
		packmsg_input_invalidate(buf);
		return 0;
	}
}

/** \brief Get an int32 value from the input.
 *  \memberof packmsg_input
 *
 * \param buf  A pointer to an input buffer iterator.
 * \return     The int32 value that was read from the input,
 *             or 0 in case of an error.
 */
static inline int32_t packmsg_get_int32(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if(hdr < 0x80 || hdr >= 0xe0) {
		return (int8_t)hdr;
	} else if(hdr == 0xd0) {
		return (int8_t) packmsg_read_hdr_(buf);
	} else if(hdr == 0xd1) {
		int16_t val = 0;
		packmsg_read_data_(buf, &val, 2);
		return val;
	} else if(hdr == 0xd2) {
		int32_t val = 0;
		packmsg_read_data_(buf, &val, 4);
		return val;
	} else {
		packmsg_input_invalidate(buf);
		return 0;
	}
}

/** \brief Get an int64 value from the input.
 *  \memberof packmsg_input
 *
 * \param buf  A pointer to an input buffer iterator.
 * \return     The int64 value that was read from the input,
 *             or 0 in case of an error.
 */
static inline int64_t packmsg_get_int64(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if(hdr < 0x80 || hdr >= 0xe0) {
		return (int8_t)hdr;
	} else if(hdr == 0xd0) {
		return (int8_t) packmsg_read_hdr_(buf);
	} else if(hdr == 0xd1) {
		int16_t val = 0;
		packmsg_read_data_(buf, &val, 2);
		return val;
	} else if(hdr == 0xd2) {
		int32_t val = 0;
		packmsg_read_data_(buf, &val, 4);
		return val;
	} else if(hdr == 0xd3) {
		int64_t val = 0;
		packmsg_read_data_(buf, &val, 8);
		return val;
	} else {
		packmsg_input_invalidate(buf);
		return 0;
	}
}

/** \brief Get an uint8 value from the input.
 *  \memberof packmsg_input
 *
 * \param buf  A pointer to an input buffer iterator.
 * \return     The uint8 value that was read from the input,
 *             or 0 in case of an error.
 */
static inline uint8_t packmsg_get_uint8(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if(hdr < 0x80) {
		return hdr;
	} else if(hdr == 0xcc) {
		return packmsg_read_hdr_(buf);
	} else {
		packmsg_input_invalidate(buf);
		return 0;
	}
}

/** \brief Get an uint16 value from the input.
 *  \memberof packmsg_input
 *
 * \param buf  A pointer to an input buffer iterator.
 * \return     The uint16 value that was read from the input,
 *             or 0 in case of an error.
 */
static inline uint16_t packmsg_get_uint16(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if(hdr < 0x80) {
		return hdr;
	} else if(hdr == 0xcc) {
		return packmsg_read_hdr_(buf);
	} else if(hdr == 0xcd) {
		uint16_t val = 0;
		packmsg_read_data_(buf, &val, 2);
		return val;
	} else {
		packmsg_input_invalidate(buf);
		return 0;
	}
}

/** \brief Get an uint32 value from the input.
 *  \memberof packmsg_input
 *
 * \param buf  A pointer to an input buffer iterator.
 * \return     The uint32 value that was read from the input,
 *             or 0 in case of an error.
 */
static inline uint32_t packmsg_get_uint32(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if(hdr < 0x80) {
		return hdr;
	} else if(hdr == 0xcc) {
		return packmsg_read_hdr_(buf);
	} else if(hdr == 0xcd) {
		uint16_t val = 0;
		packmsg_read_data_(buf, &val, 2);
		return val;
	} else if(hdr == 0xce) {
		uint32_t val = 0;
		packmsg_read_data_(buf, &val, 4);
		return val;
	} else {
		packmsg_input_invalidate(buf);
		return 0;
	}
}

/** \brief Get an uint64 value from the input.
 *  \memberof packmsg_input
 *
 * \param buf  A pointer to an input buffer iterator.
 * \return     The uint64 value that was read from the input,
 *             or 0 in case of an error.
 */
static inline uint64_t packmsg_get_uint64(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if(hdr < 0x80) {
		return hdr;
	} else if(hdr == 0xcc) {
		return packmsg_read_hdr_(buf);
	} else if(hdr == 0xcd) {
		uint16_t val = 0;
		packmsg_read_data_(buf, &val, 2);
		return val;
	} else if(hdr == 0xce) {
		uint32_t val = 0;
		packmsg_read_data_(buf, &val, 4);
		return val;
	} else if(hdr == 0xcf) {
		uint64_t val = 0;
		packmsg_read_data_(buf, &val, 8);
		return val;
	} else {
		packmsg_input_invalidate(buf);
		return 0;
	}
}

/** \brief Get a float value from the input.
 *  \memberof packmsg_input
 *
 * \param buf  A pointer to an input buffer iterator.
 * \return     The float value that was read from the input,
 *             or 0 in case of an error.
 */
static inline float packmsg_get_float(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if(hdr == 0xca) {
		float val;
		packmsg_read_data_(buf, &val, 4);
		return val;
	} else {
		packmsg_input_invalidate(buf);
		return 0;
	}
}

/** \brief Get a double value from the input.
 *  \memberof packmsg_input
 *
 * \param buf  A pointer to an input buffer iterator.
 * \return     The float value that was read from the input,
 *             or 0 in case of an error.
 */
static inline double packmsg_get_double(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if(hdr == 0xcb) {
		double val;
		packmsg_read_data_(buf, &val, 8);
		return val;
	} else if(hdr == 0xca) {
		float val;
		packmsg_read_data_(buf, &val, 4);
		return val;
	} else {
		packmsg_input_invalidate(buf);
		return 0;
	}
}

/** \brief Get a raw pointer to a string from the input.
 *  \memberof packmsg_input
 *
 * This function returns the size of a string and a pointer into the input buffer itself,
 * to a string that is *not NUL-terminated!* This function avoids making a copy of the string,
 * but the application must take care to not read more than the returned number of bytes.
 *
 * \param buf       A pointer to an input buffer iterator.
 * \param[out] str  A pointer to a const char pointer that will be set to the start of the string,
 *                  or will be set to NULL in case of an error.
 * \return          The size of the string in bytes,
 *                  or 0 in case of an error.
 */
static inline uint32_t packmsg_get_str_raw(packmsg_input_t *buf, const char **str) {
	assert(str);

	uint8_t hdr = packmsg_read_hdr_(buf);
	uint32_t slen = 0;

	if((hdr & 0xe0) == 0xa0) {
		slen = hdr & 0x1f;
	} else if(hdr == 0xd9) {
		packmsg_read_data_(buf, &slen, 1);
	} else if(hdr == 0xda) {
		packmsg_read_data_(buf, &slen, 2);
	} else if(hdr == 0xdb) {
		packmsg_read_data_(buf, &slen, 4);
	} else {
		packmsg_input_invalidate(buf);
		*str = NULL;
		return 0;
	}

	if(packmsg_likely(buf->len >= slen)) {
		*str = (const char *)buf->ptr;
		buf->ptr += slen;
		buf->len -= slen;
		return slen;
	} else {
		packmsg_input_invalidate(buf);
		*str = NULL;
		return 0;
	}
}

/** \brief Copy a string from the input into a newly allocated buffer.
 *  \memberof packmsg_input
 *
 * This function copies a string from the input into a buffer allocated by the library
 * using malloc(). The copy will be NUL-terminated.
 * The application is responsible for freeing the memory of the buffer using free().
 *
 * \param buf   A pointer to an input buffer iterator.
 *
 * \return      A pointer to the newly allocated buffer containing a NUL-terminated string,
 *              or NULL in case of an error.
 */
static inline char *packmsg_get_str_dup(packmsg_input_t *buf) {
	const char *str;
	uint32_t slen = packmsg_get_str_raw(buf, &str);

	if(packmsg_likely(packmsg_input_ok(buf))) {
		char *dup = (char *)malloc((size_t) slen + 1);

		if(packmsg_likely(dup)) {
			memcpy(dup, str, slen);
			dup[slen] = 0;
			return dup;
		} else {
			packmsg_input_invalidate(buf);
			return NULL;
		}
	} else {
		return NULL;
	}
}

/** \brief Copy a string from the input into another buffer.
 *  \memberof packmsg_input
 *
 * This function copies a string from the input another buffer provided by the application.
 * The buffer must be long enough to hold the complete string plus a terminating NUL-byte.
 * If the buffer is not long enough, or another error occured,
 * a single NUL-byte will be written to the start of the buffer (if its size is at least one byte).
 *
 * \param buf   A pointer to an input buffer iterator.
 * \param data  A pointer to a buffer allocated by the application.
 * \param dlen  The size of the buffer pointed to by data.
 *
 * \return      The size of the string in bytes,
 *              or 0 in case of an error.
 */
static inline uint32_t packmsg_get_str_copy(packmsg_input_t *buf, void *data, uint32_t dlen) {
	assert(data);

	const char *str;
	uint32_t slen = packmsg_get_str_raw(buf, &str);

	if(packmsg_likely(packmsg_input_ok(buf))) {
		if(packmsg_likely(slen < dlen)) {
			memcpy(data, str, slen);
			((char *)data)[slen] = 0;
			return slen;
		} else {
			if(dlen) {
				*(char *)data = 0;
			}

			packmsg_input_invalidate(buf);
			return 0;
		}
	} else {
		if(dlen) {
			*(char *)data = 0;
		}

		return 0;
	}
}

/** \brief Get a raw pointer to binary data from the input.
 *  \memberof packmsg_input
 *
 * This function returns the size of the binary data and a pointer into the input buffer itself.
 * This function avoids making a copy of the binary data,
 * but the application must take care to not read more than the returned number of bytes.
 *
 * \param buf        A pointer to an input buffer iterator.
 * \param[out] data  A pointer to a const void pointer that will be set to the start of the data,
 *                   or will be set to NULL in case of an error.
 * \return           The size of the data in bytes,
 *                   or 0 in case of an error.
 */
static inline uint32_t packmsg_get_bin_raw(packmsg_input_t *buf, const void **data) {
	assert(data);

	uint8_t hdr = packmsg_read_hdr_(buf);
	uint32_t dlen = 0;

	if(hdr == 0xc4) {
		packmsg_read_data_(buf, &dlen, 1);
	} else if(hdr == 0xc5) {
		packmsg_read_data_(buf, &dlen, 2);
	} else if(hdr == 0xc6) {
		packmsg_read_data_(buf, &dlen, 4);
	} else {
		packmsg_input_invalidate(buf);
		*data = NULL;
		return 0;
	}

	if(packmsg_likely(buf->len >= dlen)) {
		*data = buf->ptr;
		buf->ptr += dlen;
		buf->len -= dlen;
		return dlen;
	} else {
		packmsg_input_invalidate(buf);
		*data = NULL;
		return 0;
	}
}

/** \brief Copy binary data from the input into a newly allocated buffer.
 *  \memberof packmsg_input
 *
 * This function copies binary data from the input into a buffer allocated by the library
 * using malloc().
 * The application is responsible for freeing the memory of the buffer using free().
 *
 * \param buf        A pointer to an input buffer iterator.
 * \param[out] dlen  A pointer to an uint32_t that will be set to the size of the binary data.
 *
 * \return           A pointer to the newly allocated buffer containing the binary data,
 *                   or NULL in case of an error.
 */
static inline void *packmsg_get_bin_dup(packmsg_input_t *buf, uint32_t *dlen) {
	const void *data;
	*dlen = packmsg_get_bin_raw(buf, &data);

	if(packmsg_likely(packmsg_input_ok(buf))) {
		char *dup = (char *)malloc(*dlen);

		if(packmsg_likely(dup)) {
			memcpy(dup, data, *dlen);
			return dup;
		} else {
			*dlen = 0;
			packmsg_input_invalidate(buf);
			return NULL;
		}
	} else {
		return NULL;
	}
}

/** \brief Copy binary data from the input into another buffer.
 *  \memberof packmsg_input
 *
 * This function copies binary data from the input another buffer provided by the application.
 * The buffer must be long enough to hold all the binary data.
 *
 * \param buf     A pointer to an input buffer iterator.
 * \param rawbuf  A pointer to a buffer allocated by the application.
 * \param rlen    The size of the buffer pointed to by data.
 *
 * \return      The size of the binary data in bytes,
 *              or 0 in case of an error.
 */
static inline uint32_t packmsg_get_bin_copy(packmsg_input_t *buf, void *rawbuf, uint32_t rlen) {
	assert(rawbuf);

	const void *data;
	uint32_t dlen = packmsg_get_bin_raw(buf, &data);

	if(packmsg_likely(packmsg_input_ok(buf))) {
		if(packmsg_likely(dlen <= rlen)) {
			memcpy(rawbuf, data, dlen);
			return dlen;
		} else {
			packmsg_input_invalidate(buf);
			return 0;
		}
	} else {
		return 0;
	}
}

/** \brief Get a raw pointer to extension data from the input.
 *  \memberof packmsg_input
 *
 * This function returns the type of the extension, the size of the data
 * and a pointer into the input buffer itself.
 * This function avoids making a copy of the binary data,
 * but the application must take care to not read more than the returned number of bytes.
 *
 * \param buf        A pointer to an input buffer iterator.
 * \param[out] type  A pointer to an int8_t that will be set to the type of the extension.
 *                   or will be set to 0 in case of an error.
 * \param[out] data  A pointer to a const void pointer that will be set to the start of the data,
 *                   or will be set to NULL in case of an error.
 *
 * \return           The size of the data in bytes,
 *                   or 0 in case of an error.
 */
static inline uint32_t packmsg_get_ext_raw(packmsg_input_t *buf, int8_t *type, const void **data) {
	assert(type);
	assert(data);

	uint8_t hdr = packmsg_read_hdr_(buf);
	uint32_t dlen = 0;

	if(hdr == 0xc7) {
		packmsg_read_data_(buf, &dlen, 1);
	} else if(hdr == 0xc8) {
		packmsg_read_data_(buf, &dlen, 2);
	} else if(hdr == 0xc9) {
		packmsg_read_data_(buf, &dlen, 4);
	} else if(hdr >= 0xd4 && hdr <= 0xd8) {
		dlen = 1 << (hdr - 0xd4);
	} else {
		packmsg_input_invalidate(buf);
		*type = 0;
		*data = NULL;
		return 0;
	}

	*type = packmsg_read_hdr_(buf);

	if(packmsg_likely(buf->len >= dlen)) {
		*data = buf->ptr;
		buf->ptr += dlen;
		buf->len -= dlen;
		return dlen;
	} else {
		packmsg_input_invalidate(buf);
		*type = 0;
		*data = NULL;
		return 0;
	}
}

/** \brief Copy extension data from the input into a newly allocated buffer.
 *  \memberof packmsg_input
 *
 * This function copies extension data from the input into a buffer allocated by the library
 * using malloc().
 * The application is responsible for freeing the memory of the buffer using free().
 *
 * \param buf        A pointer to an input buffer iterator.
 * \param[out] type  A pointer to an int8_t that will be set to the type of the extension.
 *                   or will be set to 0 in case of an error.
 * \param[out] dlen  A pointer to an uint32_t that will be set to the size of the extension data,
 *                   or will be set to 0 in case of an error.
 *
 * \return           A pointer to the newly allocated buffer containing the extension data,
 *                   or NULL in case of an error.
 */
static inline void *packmsg_get_ext_dup(packmsg_input_t *buf, int8_t *type, uint32_t *dlen) {
	assert(type);

	const void *data;
	*dlen = packmsg_get_ext_raw(buf, type, &data);

	if(packmsg_likely(packmsg_input_ok(buf))) {
		char *dup = (char *)malloc(*dlen);

		if(packmsg_likely(dup)) {
			memcpy(dup, data, *dlen);
			return dup;
		} else {
			*type = 0;
			*dlen = 0;
			packmsg_input_invalidate(buf);
			return NULL;
		}
	} else {
		*type = 0;
		*dlen = 0;
		return NULL;
	}
}

/** \brief Copy extension data from the input into another buffer.
 *  \memberof packmsg_input
 *
 * This function copies extension data from the input another buffer provided by the application.
 * The buffer must be long enough to hold all the extension data.
 *
 * \param buf        A pointer to an input buffer iterator.
 * \param[out] type  A pointer to an int8_t that will be set to the type of the extension.
 *                   or will be set to 0 in case of an error.
 * \param rawbuf     A pointer to a buffer allocated by the application.
 * \param rlen       The size of the buffer pointed to by data.
 *
 * \return           The size of the extension data in bytes,
 *                   or 0 in case of an error.
 */
static inline uint32_t packmsg_get_ext_copy(packmsg_input_t *buf, int8_t *type, void *rawbuf, uint32_t rlen) {
	assert(type);
	assert(rawbuf);

	const void *data;
	uint32_t dlen = packmsg_get_ext_raw(buf, type, &data);

	if(packmsg_likely(packmsg_input_ok(buf))) {
		if(packmsg_likely(dlen <= rlen)) {
			memcpy(rawbuf, data, dlen);
			return dlen;
		} else {
			*type = 0;
			packmsg_input_invalidate(buf);
			return 0;
		}
	} else {
		*type = 0;
		return 0;
	}
}

/** \brief Get a map header from the output.
 *  \memberof packmsg_input
 *
 * This function only reads a map header, and returns the number of key-value
 * pairs in the map.
 * These key-value pairs have to be read by the application using regular
 * packmsg_get_*() calls.
 *
 * \param buf    A pointer to an input buffer iterator.
 *
 * \return       The number of key-value pairs in the map.
 */
static inline uint32_t packmsg_get_map(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if((hdr & 0xf0) == 0x80) {
		return hdr & 0xf;
	} else if(hdr == 0xde) {
		uint32_t dlen = 0;
		packmsg_read_data_(buf, &dlen, 2);
		return dlen;
	} else if(hdr == 0xdf) {
		uint32_t dlen = 0;
		packmsg_read_data_(buf, &dlen, 4);
		return dlen;
	} else {
		packmsg_input_invalidate(buf);
		return 0;
	}
}

/** \brief Get an array header from the output.
 *  \memberof packmsg_input
 *
 * This function only reads an array header, and returns the number of elements
 * in the array.
 * These elements have to be read by the application using regular
 * packmsg_get_*() calls.
 *
 * \param buf    A pointer to an input buffer iterator.
 *
 * \return       The number of elements in the array.
 */
static inline uint32_t packmsg_get_array(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);

	if((hdr & 0xf0) == 0x90) {
		return hdr & 0xf;
	} else if(hdr == 0xdc) {
		uint32_t dlen = 0;
		packmsg_read_data_(buf, &dlen, 2);
		return dlen;
	} else if(hdr == 0xdd) {
		uint32_t dlen = 0;
		packmsg_read_data_(buf, &dlen, 4);
		return dlen;
	} else {
		packmsg_input_invalidate(buf);
		return 0;
	}
}

/* Type checking
 * =============
 */

/** \brief An enum describing the type of an element in a PackMessage message.
 *
 * This enum describes the type of an element in a PackMessage message.
 * In case of integers and floating point values, the type normally represents
 * the smallest type that can succesfully hold the value of the element;
 * i.e. an element of type PACKMSG_INT32 can only succesfully be read by
 * packmsg_get_int32() or packmsg_get_int64(). However, the converse it not true;
 * for an element of type PACKMSG_INT32, there is no guarantee
 * that the value is larger than would fit into an int16_t.
 *
 * PackMessage makes a clear distinction between signed and unsigned integers,
 * except in the case of positive fixints (values between 0 and 127 inclusive),
 * which can be read as both signed and unsigned.
 */
enum packmsg_type {
	PACKMSG_ERROR,            /**< An invalid element was found or the input buffer is in an invalid state. */
	PACKMSG_NIL,              /**< The next element is a NIL. */
	PACKMSG_BOOL,             /**< The next element is a boolean. */
	PACKMSG_POSITIVE_FIXINT,  /**< The next element is an integer between 0 and 127 inclusive. */
	PACKMSG_INT8,             /**< The next element is a signed integer that fits in an int8_t. */
	PACKMSG_INT16,            /**< The next element is a signed integer that fits in an int16_t. */
	PACKMSG_INT32,            /**< The next element is a signed integer that fits in an int32_t. */
	PACKMSG_INT64,            /**< The next element is a signed integer that fits in an int64_t. */
	PACKMSG_UINT8,            /**< The next element is an unsigned integer that fits in an uint8_t. */
	PACKMSG_UINT16,           /**< The next element is an unsigned integer that fits in an uint16_t. */
	PACKMSG_UINT32,           /**< The next element is an unsigned integer that fits in an uint32_t. */
	PACKMSG_UINT64,           /**< The next element is an unsigned integer that fits in an uint64_t. */
	PACKMSG_FLOAT,            /**< The next element is a single precision floating point value. */
	PACKMSG_DOUBLE,           /**< The next element is a double precision floating point value. */
	PACKMSG_STR,              /**< The next element is a string. */
	PACKMSG_BIN,              /**< The next element is binary data. */
	PACKMSG_EXT,              /**< The next element is extension data. */
	PACKMSG_MAP,              /**< The next element is a map header. */
	PACKMSG_ARRAY,            /**< The next element is an array header. */
	PACKMSG_DONE,             /**< There are no more elements in the input buffer. */
};

/** \brief Checks if the next element is a NIL.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_nil(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_nil(const packmsg_input_t *buf) {
	return packmsg_peek_hdr_(buf) == 0xc0;
}

/** \brief Checks if the next element is a bool.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_nil(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_bool(const packmsg_input_t *buf) {
	return (packmsg_peek_hdr_(buf) & 0xfe) == 0xc2;
}

/** \brief Checks if the next element is a signed integer that fits in an int8_t.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_int8(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_int8(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return hdr < 0x80 || hdr == 0xd0;
}

/** \brief Checks if the next element is a signed integer that fits in an int16_t.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_int16(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_int16(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return hdr < 0x80 || hdr == 0xd0 || hdr == 0xd1;
}

/** \brief Checks if the next element is a signed integer that fits in an int32_t.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_int32(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_int32(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return hdr < 0x80 || hdr == 0xd0 || hdr == 0xd1 || hdr == 0xd2;
}

/** \brief Checks if the next element is a signed integer that fits in an int64_t.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_int64(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_int64(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return hdr < 0x80 || hdr == 0xd0 || hdr == 0xd1 || hdr == 0xd2 || hdr == 0xd3;
}

/** \brief Checks if the next element is an unsigned integer that fits in an uint8_t.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_uint8(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_uint8(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return hdr < 0x80 || hdr == 0xcc;
}

/** \brief Checks if the next element is an unsigned integer that fits in an uint16_t.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_uint16(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_uint16(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return hdr < 0x80 || hdr == 0xcc || hdr == 0xcd;
}

/** \brief Checks if the next element is an unsigned integer that fits in an uint32_t.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_uint32(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_uint32(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return hdr < 0x80 || hdr == 0xcc || hdr == 0xcd || hdr == 0xce;
}

/** \brief Checks if the next element is an unsigned integer that fits in an uint64_t.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_uint64(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_uint64(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return hdr < 0x80 || hdr == 0xcc || hdr == 0xcd || hdr == 0xce || hdr == 0xcf;
}

/** \brief Checks if the next element is a single precision floating point value.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_float(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_float(const packmsg_input_t *buf) {
	return packmsg_peek_hdr_(buf) == 0xca;
}

/** \brief Checks if the next element is a single or double precision floating point value.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_double(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_double(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return hdr == 0xcb || hdr == 0xca;
}

/** \brief Checks if the next element is a string.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_str_*(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_str(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return (hdr & 0xe0) == 0xa0 || hdr == 0xd9 || hdr == 0xda || hdr == 0xdb;
}

/** \brief Checks if the next element is binary data.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_bin_*(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_bin(const packmsg_input_t *buf) {
	return (packmsg_peek_hdr_(buf) & 0xfc) == 0xc4;
}

/** \brief Checks if the next element is extension data.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_ext_*(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_ext(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return (hdr >= 0xc7 && hdr <= 0xc9) || (hdr >= 0xd4 && hdr <= 0xd8);
}

/** \brief Checks if the next element is a map header.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_map(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_map(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return (hdr & 0xf0) == 0x80 || hdr == 0xde || hdr == 0xdf;
}

/** \brief Checks if the next element is an array header.
 *  \memberof packmsg_input
 *
 * \param buf A pointer to an input buffer iterator.
 *
 * \return True if the next element can be read by packmsg_get_array(),
 *         false if not or if any other error occurred.
 */
static inline bool packmsg_is_array(const packmsg_input_t *buf) {
	uint8_t hdr = packmsg_peek_hdr_(buf);
	return (hdr & 0xf0) == 0x90 || hdr == 0xdc || hdr == 0xdd;
}

/** \brief Checks the type of the next element.
 *  \memberof packmsg_input
 *
 * This function checks the next element and returns an enum packmsg_type
 * that describes the type of the element. If the input buffer was fully consumed
 * and there are no more elements left, this function will return PACKMSG_DONE.
 *
 * \param buf A pointer to an output buffer iterator.
 *
 * \return    The type of the next element, or PACKMSG_DONE if no more elements
 *            are present in the input buffer, or PACKMSG_ERROR if the next element
 *            is invalid, or if any other error occurred.
 */
static inline enum packmsg_type packmsg_get_type(const packmsg_input_t *buf) {
	if(packmsg_unlikely(packmsg_done(buf))) {
		return PACKMSG_DONE;
	}

	uint8_t hdr = packmsg_peek_hdr_(buf);

	switch(hdr >> 4) {
	case 0x0:
	case 0x1:
	case 0x2:
	case 0x3:
	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
		return PACKMSG_POSITIVE_FIXINT;

	case 0x8:
		return PACKMSG_MAP;

	case 0x9:
		return PACKMSG_ARRAY;

	case 0xa:
	case 0xb:
		return PACKMSG_STR;

	case 0xc:
		switch(hdr & 0xf) {
		case 0x0:
			return PACKMSG_NIL;

		case 0x1:
			return PACKMSG_ERROR;

		case 0x2:
		case 0x3:
			return PACKMSG_BOOL;

		case 0x4:
		case 0x5:
		case 0x6:
			return PACKMSG_BIN;

		case 0x7:
		case 0x8:
		case 0x9:
			return PACKMSG_EXT;

		case 0xa:
			return PACKMSG_FLOAT;

		case 0xb:
			return PACKMSG_DOUBLE;

		case 0xc:
			return PACKMSG_UINT8;

		case 0xd:
			return PACKMSG_UINT16;

		case 0xe:
			return PACKMSG_UINT32;

		case 0xf:
			return PACKMSG_UINT64;

		default:
			return PACKMSG_ERROR;
		}

	case 0xd:
		switch(hdr & 0xf) {
		case 0x0:
			return PACKMSG_INT8;

		case 0x1:
			return PACKMSG_INT16;

		case 0x2:
			return PACKMSG_INT32;

		case 0x3:
			return PACKMSG_INT64;

		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x8:
			return PACKMSG_EXT;

		case 0x9:
		case 0xa:
		case 0xb:
			return PACKMSG_STR;

		case 0xc:
		case 0xd:
			return PACKMSG_ARRAY;

		case 0xe:
		case 0xf:
			return PACKMSG_MAP;

		default:
			return PACKMSG_ERROR;
		}

	case 0xe:
	case 0xf:
		return PACKMSG_INT8;

	default:
		return PACKMSG_ERROR;
	}
}

/** \brief Skip one element in the input
 *  \memberof packmsg_input
 *
 *  This function skips the next element in the input.
 *  If the element is a map or an array, only the map or array header is skipped,
 *  but not the contents of the map or array.
 *
 * \param buf A pointer to an output buffer iterator.
 */
static inline void packmsg_skip_element(packmsg_input_t *buf) {
	uint8_t hdr = packmsg_read_hdr_(buf);
	int32_t skip = 0;

	switch(hdr >> 4) {
	case 0x0:
	case 0x1:
	case 0x2:
	case 0x3:
	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
	case 0x8:
	case 0x9:
		return;

	case 0xa:
	case 0xb:
		skip = hdr & 0x1f;
		break;

	case 0xc:
		switch(hdr & 0xf) {
		case 0x0:
		case 0x1:
		case 0x2:
		case 0x3:
			return;

		case 0x4:
			skip = -1;
			break;

		case 0x5:
			skip = -2;
			break;

		case 0x6:
			skip = -4;
			break;

		case 0x7:
			skip = -1;
			break;

		case 0x8:
			skip = -2;
			break;

		case 0x9:
			skip = -4;
			break;

		case 0xa:
			skip = 4;
			break;

		case 0xb:
			skip = 8;
			break;

		case 0xc:
			skip = 1;
			break;

		case 0xd:
			skip = 2;
			break;

		case 0xe:
			skip = 4;
			break;

		case 0xf:
			skip = 8;
			break;
		}

		break;

	case 0xd:
		switch(hdr & 0xf) {
		case 0x0:
			skip = 1;
			break;

		case 0x1:
			skip = 2;
			break;

		case 0x2:
			skip = 4;
			break;

		case 0x3:
			skip = 8;
			break;

		case 0x4:
			skip = 2;
			break;

		case 0x5:
			skip = 3;
			break;

		case 0x6:
			skip = 5;
			break;

		case 0x7:
			skip = 9;
			break;

		case 0x8:
			skip = 17;
			break;

		case 0x9:
			skip = -1;
			break;

		case 0xa:
			skip = -2;
			break;

		case 0xb:
			skip = -4;
			break;

		case 0xc:
			skip = 2;
			break;

		case 0xd:
			skip = 4;
			break;

		case 0xe:
			skip = 2;
			break;

		case 0xf:
			skip = 4;
			break;
		}

		break;

	case 0xe:
	case 0xf:
		return;
	}

	uint32_t dlen = 0;

	if(skip < 0) {
		packmsg_read_data_(buf, &dlen, -skip);

		if(hdr >= 0xc7 && hdr <= 0xc9) {
			dlen++;
		}
	} else {
		dlen = skip;
	}

	if(packmsg_likely(buf->len >= dlen)) {
		buf->ptr += dlen;
		buf->len -= dlen;
	} else {
		packmsg_input_invalidate(buf);
	}
}

/** \brief Skip one object in the input
 *  \memberof packmsg_input
 *
 *  This function checks the type of the next element.
 *  In case it is a scalar value (for example, an int or a string),
 *  it skips just that scalar. If the next element is a map or an array,
 *  it will recursively skip as many objects as there are in that map or array.
 *
 * \param buf A pointer to an output buffer iterator.
 */
static inline void packmsg_skip_object(packmsg_input_t *buf) {
	if(packmsg_is_array(buf)) {
		uint32_t count = packmsg_get_array(buf);

		while(count-- && buf->len >= 0) {
			packmsg_skip_object(buf);
		}
	} else if(packmsg_is_map(buf)) {
		uint32_t count = packmsg_get_map(buf);

		while(count-- && buf->len >= 0) {
			packmsg_skip_object(buf);
			packmsg_skip_object(buf);
		}
	} else {
		packmsg_skip_element(buf);
	}
}

#ifdef __cplusplus
}
#endif
