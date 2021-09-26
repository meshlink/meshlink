/*
    pmtu.c -- PMTU probing
    Copyright (C) 2020 Guus Sliepen <guus@tinc-vpn.org>

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
#include "logger.h"
#include "net.h"
#include "netutl.h"
#include "node.h"
#include "pmtu.h"
#include "protocol.h"
#include "utils.h"

/* PMTU probing serves two purposes:
 *
 * - establishing a working UDP connection between two peers
 * - determining the path MTU (PMTU) between two peers
 *
 * Establishing a working UDP connection requires NAT hole punching and regular
 * packets to keep the NAT mappings alive.  For this, we can use very small UDP
 * packets, and send them rather frequently (once every 10 seconds).  This also
 * allows us to detect connection loss rather quickly.
 *
 * For PMTU discovery, we need to send packets of various size, and determine
 * which ones are received by the other end.  Once the PMTU is established, we
 * want to keep monitoring that the discovered PMTU value is still valid.
 * However, we assume PMTU changes are unlikely, so they do not have to be done
 * very often.
 *
 * To keep track of how far we are in the PMTU probing process, the variable
 * mtuprobes is used. The meaning of its value is:
 *
 * - mtuprobes ==    -4: maxmtu no longer valid, reset minmtu and maxmtu and go to 0
 * - mtuprobes ==-2..-3: send one maxmtu probe every second
 * - mtuprobes ==    -1: send one maxmtu and one maxmtu + 1 probe every pinginterval
 * - mtuprobes == 0..19: initial discovery, send three packets per second, mtuprobes++
 * - mtuprobes ==    20: fix PMTU, and go to -1
 *
 * The first probe is always the maximum MTU supported by the interface,
 * then a binary search is done until the minimum and maximum converge,
 * or until 20 packets have been sent.
 *
 * After the initial discovery, PMTU probing only sends two packets; one with
 * the same size as the discovered PMTU, and one which has a size slightly
 * larger than the currently known PMTU, to test if the PMTU has increased.
 */

static void try_fix_mtu(meshlink_handle_t *mesh, node_t *n) {
	if(n->mtuprobes < 0) {
		return;
	}

	if(n->mtuprobes == 20 || n->minmtu >= n->maxmtu) {
		if(n->minmtu > n->maxmtu) {
			n->minmtu = n->maxmtu;
		} else {
			n->maxmtu = n->minmtu;
		}

		n->mtu = n->minmtu;
		logger(mesh, MESHLINK_INFO, "Fixing PMTU of %s to %d after %d probes", n->name, n->mtu, n->mtuprobes);
		n->mtuprobes = -1;
	}
}

static void udp_probe_timeout_handler(event_loop_t *loop, void *data) {
	node_t *n = data;
	meshlink_handle_t *mesh = loop->data;

	if(!n->status.udp_confirmed) {
		return;
	}

	logger(mesh, MESHLINK_INFO, "Too much time has elapsed since last UDP ping response from %s, stopping UDP communication", n->name);
	n->status.udp_confirmed = false;
	n->mtuprobes = 0;
	n->udpprobes = 0;
	n->minmtu = 0;
	n->maxmtu = MTU;

	// If we also have a meta-connection to this node, send a PING on it as well
	connection_t *c = n->connection;

	if(c && !c->status.pinged) {
		send_ping(mesh, c);
	}
}

static void send_udp_probe_reply(meshlink_handle_t *mesh, node_t *n, vpn_packet_t *packet, uint16_t len) {
	if(!n->status.validkey) {
		logger(mesh, MESHLINK_INFO, "Trying to send UDP probe reply to %s but we don't have his key yet", n->name);
		return;
	}

	packet->data[0] = 1;

	if(packet->data[1]) {
		packet->data[1] = 1;
		memcpy(packet->data + 2, &len, 2);
		len = MIN_PROBE_SIZE;
	}

	/* Temporarily set udp_confirmed, so that the reply is sent
	   back exactly the way it came in. */

	bool udp_confirmed = n->status.udp_confirmed;
	n->status.udp_confirmed = true;
	logger(mesh, MESHLINK_DEBUG, "Sending UDP reply length %d to %s", packet->len, n->name);
	n->out_meta += packet->len + SPTPS_DATAGRAM_OVERHEAD;
	send_udppacket(mesh, n, packet);
	n->status.udp_confirmed = udp_confirmed;
}

void udp_probe_h(meshlink_handle_t *mesh, node_t *n, vpn_packet_t *packet, uint16_t len) {
	if(len < MIN_PROBE_SIZE) {
		logger(mesh, MESHLINK_WARNING, "Got too short PMTU probe length %d from %s", packet->len, n->name);
		return;
	}

	n->in_meta += packet->len + SPTPS_DATAGRAM_OVERHEAD;

	if(!packet->data[0]) {
		/* It's a probe request, send back a reply */
		logger(mesh, MESHLINK_DEBUG, "Got PMTU probe length %d from %s", packet->len, n->name);
		send_udp_probe_reply(mesh, n, packet, len);
		return;
	}

	if(packet->data[1]) {
		memcpy(&len, packet->data + 2, 2);
	}

	logger(mesh, MESHLINK_DEBUG, "Got PMTU reply length %d from %s", len, n->name);

	/* It's a valid reply: now we know bidirectional communication
	   is possible using the address and socket that the reply
	   packet used. */
	if(!n->status.udp_confirmed) {
		char *address, *port;
		sockaddr2str(&n->address, &address, &port);
		send_request(mesh, n->nexthop->connection, NULL, "%d %s %s . -1 -1 -1 0 %s %s", ANS_KEY, n->name, n->name, address, port);

		free(address);
		free(port);
		n->status.udp_confirmed = true;
	}

	n->udpprobes = 0;

	// Reset the UDP ping timer.

	timeout_del(&mesh->loop, &n->udp_ping_timeout);
	timeout_add(&mesh->loop, &n->udp_ping_timeout, &udp_probe_timeout_handler, n, &(struct timespec) {
		30, 0
	});

	if(len > n->maxmtu) {
		logger(mesh, MESHLINK_INFO, "Increase in PMTU to %s detected, restarting PMTU discovery", n->name);
		n->minmtu = len;
		n->maxmtu = MTU;
		/* Set mtuprobes to 1 so that try_pmtu() doesn't reset maxmtu */
		n->mtuprobes = 1;
		return;
	} else if(n->mtuprobes < 0 && len == n->maxmtu) {
		/* We got a maxmtu sized packet, confirming the PMTU is still valid. */
		n->mtuprobes = -1;
		n->last_mtu_probe_sent = mesh->loop.now;
	}

	/* If applicable, raise the minimum supported PMTU */

	if(n->minmtu < len) {
		n->minmtu = len;
		update_node_pmtu(mesh, n);
	}

	try_fix_mtu(mesh, n);
}

static void send_udp_probe_packet(meshlink_handle_t *mesh, node_t *n, int len) {
	if(len < MIN_PROBE_SIZE) {
		len = MIN_PROBE_SIZE;
	}

	vpn_packet_t packet;
	memset(packet.data, 0, 4);
	packet.probe = true;
	packet.data[0] = 0;
	packet.data[1] = 1;
	packet.data[2] = 0;
	packet.data[3] = 0;

	if(len > 4) {
		randomize(packet.data + 4, len - 4);
	}

	packet.len = len;

	logger(mesh, MESHLINK_DEBUG, "Sending UDP probe length %d to %s", len, n->name);

	n->out_meta += packet.len + SPTPS_DATAGRAM_OVERHEAD;
	send_udppacket(mesh, n, &packet);
}

static void try_udp(meshlink_handle_t *mesh, node_t *n) {
	/* Probe request */

	if(n->udpprobes < -3) {
		/* We lost three UDP probes, UDP status is no longer unconfirmed */
		udp_probe_timeout_handler(&mesh->loop, n);
	}

	struct timespec elapsed;

	timespec_sub(&mesh->loop.now, &n->last_udp_probe_sent, &elapsed);

	int interval = (n->status.udp_confirmed && n->udpprobes >= 0) ? 10 : 2;

	if(elapsed.tv_sec >= interval) {
		n->last_udp_probe_sent = mesh->loop.now;
		send_udp_probe_packet(mesh, n, MIN_PROBE_SIZE);

		if(n->status.udp_confirmed) {
			n->udpprobes--;
		}

		if(!n->status.udp_confirmed && n->prevedge) {
			n->status.broadcast = true;
			send_udp_probe_packet(mesh, n, MIN_PROBE_SIZE);
			n->status.broadcast = false;
		}
	}
}

static uint16_t choose_initial_maxmtu(meshlink_handle_t *mesh, node_t *n) {
#ifdef IP_MTU

	int sock = -1;

	sockaddr_t sa_buf;
	const sockaddr_t *sa;
	int sockindex;
	choose_udp_address(mesh, n, &sa, &sockindex, &sa_buf);

	if(!sa) {
		return MTU;
	}

	sock = socket(sa->sa.sa_family, SOCK_DGRAM, IPPROTO_UDP);

	if(sock < 0) {
		logger(mesh, MESHLINK_ERROR, "Creating MTU assessment socket for %s failed: %s", n->name, sockstrerror(sockerrno));
		return MTU;
	}

	if(connect(sock, &sa->sa, SALEN(sa->sa))) {
		logger(mesh, MESHLINK_ERROR, "Connecting MTU assessment socket for %s failed: %s", n->name, sockstrerror(sockerrno));
		close(sock);
		return MTU;
	}

	int ip_mtu;
	socklen_t ip_mtu_len = sizeof(ip_mtu);

	if(getsockopt(sock, IPPROTO_IP, IP_MTU, &ip_mtu, &ip_mtu_len)) {
		logger(mesh, MESHLINK_ERROR, "getsockopt(IP_MTU) on %s failed: %s", n->name, sockstrerror(sockerrno));
		close(sock);
		return MTU;
	}

	close(sock);

	/* Calculate the maximum SPTPS payload based on the interface MTU */
	uint16_t mtu = ip_mtu;
	mtu -= (sa->sa.sa_family == AF_INET6) ? 40 : 20; /* IPv6 or IPv4 */
	mtu -= 8; /* UDP */
	mtu -= 21; /* SPTPS */

	if(mtu < 512) {
		logger(mesh, MESHLINK_ERROR, "getsockopt(IP_MTU) on %s returned absurdly small value: %d", n->name, ip_mtu);
		return MTU;
	}

	if(mtu > MTU) {
		return MTU;
	}

	logger(mesh, MESHLINK_INFO, "Using system-provided maximum MTU for %s: %hd", n->name, mtu);
	return mtu;

#else
	(void)mesh;
	(void)n;
	return MTU;
#endif
}

/* This function tries to determines the PMTU of a node.
   By calling this function repeatedly, n->minmtu will be progressively
   increased, and at some point, n->mtu will be fixed to n->minmtu.  If the PMTU
   is already fixed, this function checks if it can be increased.
*/

static void try_pmtu(meshlink_handle_t *mesh, node_t *n) {
	if(!n->status.udp_confirmed) {
		n->mtuprobes = 0;
		n->minmtu = 0;
		n->maxmtu = MTU;
		return;
	}

	struct timespec elapsed;

	timespec_sub(&mesh->loop.now, &n->last_mtu_probe_sent, &elapsed);

	if(n->mtuprobes >= 0) {
		/* Fast probing, send three packets per second */
		if(n->mtuprobes != 0 && elapsed.tv_sec == 0 && elapsed.tv_nsec < 333333333) {
			return;
		}
	} else {
		if(n->mtuprobes < -1) {
			/* We didn't get an answer to the last probe, try again once every second */
			if(elapsed.tv_sec < 1) {
				return;
			}
		} else {
			/* Slow probing, send one packet every pinginterval */
			int pinginterval = mesh->dev_class_traits[n->devclass].pinginterval;

			if(elapsed.tv_sec < pinginterval) {
				return;
			}
		}
	}

	n->last_mtu_probe_sent = mesh->loop.now;

	if(n->mtuprobes < -3) {
		/* We lost three PMTU probes, restart discovery */
		logger(mesh, MESHLINK_INFO, "Decrease in PMTU to %s detected, restarting PMTU discovery", n->name);
		n->mtuprobes = 0;
		n->minmtu = 0;
	}

	if(n->mtuprobes < 0) {
		/* After the initial discovery, we only send one maxmtu and one
		   maxmtu + 1 probe to detect PMTU increases. */
		send_udp_probe_packet(mesh, n, n->maxmtu);

		if(n->mtuprobes == -1 && n->maxmtu + 1 < MTU) {
			send_udp_probe_packet(mesh, n, n->maxmtu + 1);
		}

		n->mtuprobes--;
	} else {
		/* Starting parameters. */
		uint16_t len;

		if(n->mtuprobes == 0) {
			/* First packet is always the maximum MTU size */
			n->maxmtu = choose_initial_maxmtu(mesh, n);
			len = n->maxmtu;
		} else {
			if(n->last_mtu_len == n->minmtu) {
				/* The previous probe was succesful, increase the size */
				len = n->minmtu + (n->maxmtu - n->minmtu + 1) / 2;
			} else {
				/* The previous probe was unsuccesful, decrease the size */
				len = n->minmtu + (n->last_mtu_len - n->minmtu) / 2;
			}
		}

		n->last_mtu_len = len;
		send_udp_probe_packet(mesh, n, len);
		n->mtuprobes++;
	}

	try_fix_mtu(mesh, n);
}

/* Keep the connection to the given node alive.
 * Ensure we have a valid key, and check whether UDP is working.
 */

void keepalive(meshlink_handle_t *mesh, node_t *n, bool traffic) {
	if(!n->status.reachable || !n->status.validkey) {
		return;
	}

	try_udp(mesh, n);

	if(traffic) {
		try_pmtu(mesh, n);
	}

	/* If we want to send traffic but we don't have a working UDP
	 * connection, we are going to forward the traffic to the nexthop, so
	 * try to keep that one alive as well. */

	if(traffic && !n->status.udp_confirmed && n != n->nexthop) {
		keepalive(mesh, n->nexthop, traffic);
	}
}

