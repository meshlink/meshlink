/*
    net.h -- header for net.c
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

#ifndef __MESHLINK_NET_DEFINES_H__
#define __MESHLINK_NET_DEFINES_H__

#ifdef ENABLE_JUMBOGRAMS
#define MTU 9018        /* 9000 bytes payload + 14 bytes ethernet header + 4 bytes VLAN tag */
#else
#define MTU 1518        /* 1500 bytes payload + 14 bytes ethernet header + 4 bytes VLAN tag */
#endif

// max payload size
// MTU - (14 bytes ethernet header + 4 bytes VLAN tag)
// - 20 bytes IPv4-Header (IPv4 header is smallest, larger IP header will be compensated for in PMTU probing)
// -  8 bytes UDP-Header
#define PAYLOAD_MTU (MTU - 18 - 20 - 8)

/* MAXSIZE is the maximum size of an encapsulated packet: MTU + seqno + HMAC + compressor overhead */
#define MAXSIZE (MTU + 4 + 32 + MTU/64 + 20)

/* MAXBUFSIZE is the maximum size of a request: enough for a MAXSIZEd packet or a 8192 bits RSA key */
#define MAXBUFSIZE ((MAXSIZE > 2048 ? MAXSIZE : 2048) + 128)

typedef struct vpn_packet_t {
    struct {
        unsigned int probe:1;
        unsigned int tcp:1;
    };
    uint16_t len;           /* the actual number of bytes in the `data' field */
    uint8_t data[MAXSIZE];
} vpn_packet_t;

/* Packet types when using SPTPS */

#define PKT_COMPRESSED 1
#define PKT_PROBE 4

typedef enum packet_type_t {
    PACKET_NORMAL,
    PACKET_COMPRESSED,
    PACKET_PROBE
} packet_type_t;

#endif /* __MESHLINK_NET_DEFINES_H__ */
