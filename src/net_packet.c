/*
    net_packet.c -- Handles in- and outgoing VPN packets
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

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "conf.h"
#include "connection.h"
#include "crypto.h"
#include "graph.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "net.h"
#include "netutl.h"
#include "protocol.h"
#include "route.h"
#include "utils.h"
#include "xalloc.h"

int keylifetime = 0;

static void send_udppacket(meshlink_handle_t *mesh, node_t *, vpn_packet_t *);

#define MAX_SEQNO 1073741824

/* mtuprobes == 1..30: initial discovery, send bursts with 1 second interval
   mtuprobes ==    31: sleep pinginterval seconds
   mtuprobes ==    32: send 1 burst, sleep pingtimeout second
   mtuprobes ==    33: no response from other side, restart PMTU discovery process

   Probes are sent in batches of at least three, with random sizes between the
   lower and upper boundaries for the MTU thus far discovered.

   After the initial discovery, a fourth packet is added to each batch with a
   size larger than the currently known PMTU, to test if the PMTU has increased.

   In case local discovery is enabled, another packet is added to each batch,
   which will be broadcast to the local network.

*/

static void send_mtu_probe_handler(event_loop_t *loop, void *data) {
	meshlink_handle_t *mesh = loop->data;
	node_t *n = data;
	int timeout = 1;

	n->mtuprobes++;

	if(!n->status.reachable || !n->status.validkey) {
		logger(mesh, MESHLINK_INFO, "Trying to send MTU probe to unreachable or rekeying node %s (%s)", n->name, n->hostname);
		n->mtuprobes = 0;
		return;
	}

	if(n->mtuprobes > 32) {
		if(!n->minmtu) {
			n->mtuprobes = 31;
			timeout = mesh->pinginterval;
			goto end;
		}

		logger(mesh, MESHLINK_INFO, "%s (%s) did not respond to UDP ping, restarting PMTU discovery", n->name, n->hostname);
		n->status.udp_confirmed = false;
		n->mtuprobes = 1;
		n->minmtu = 0;
		n->maxmtu = sptps_maxmtu(&n->sptps);

		// reduce mtu a bit as well
		if(n->mtu > 1000)
			n->mtu -= 100;
	}

	if(n->mtuprobes >= 10 && n->mtuprobes < 32 && !n->minmtu) {
		logger(mesh, MESHLINK_INFO, "No response to MTU probes from %s (%s)", n->name, n->hostname);
		n->mtuprobes = 31;
	}

	if(n->mtuprobes == 30 || (n->mtuprobes < 30 && n->minmtu >= n->maxmtu)) {
		if(n->minmtu > n->maxmtu)
			n->minmtu = n->maxmtu;
		else
			n->maxmtu = n->minmtu;
		n->mtu = n->minmtu;
		logger(mesh, MESHLINK_INFO, "Fixing MTU of %s (%s) to %d after %d probes", n->name, n->hostname, n->mtu, n->mtuprobes);
		n->mtuprobes = 31;

		// update meshlink and utcp for the mtu change
		update_node_mtu(mesh, n);
	}

	if(n->mtuprobes == 31) {
		timeout = mesh->pinginterval;
		goto end;
	} else if(n->mtuprobes == 32) {
		timeout = mesh->pingtimeout;
	}

	for(int i = 0; i < 4 + mesh->localdiscovery; i++) {
		int len;

		if(i == 0) {
			if(n->mtuprobes < 30 || n->maxmtu + 8 >= sptps_maxmtu(&n->sptps))
				continue;
			len = n->maxmtu + 8;
		} else if(n->maxmtu <= n->minmtu) {
			len = n->maxmtu;
		} else {
			len = n->minmtu + 1 + rand() % (n->maxmtu - n->minmtu);
		}

		if(len < 64)
			len = 64;

		vpn_packet_t packet;
		packet.probe = true;
		memset(packet.data, 0, 14);
		randomize(packet.data + 14, len - 14);
		packet.len = len;
		n->status.broadcast = i >= 4 && n->mtuprobes <= 10 && n->prevedge;

		logger(mesh, MESHLINK_DEBUG, "Sending MTU probe length %d to %s (%s)", len, n->name, n->hostname);

		send_udppacket(mesh, n, &packet);
	}

	n->status.broadcast = false;

end:
	timeout_set(&mesh->loop, &n->mtutimeout, &(struct timeval){timeout, rand() % 100000});
}

void send_mtu_probe(meshlink_handle_t *mesh, node_t *n) {
	timeout_add(&mesh->loop, &n->mtutimeout, send_mtu_probe_handler, n, &(struct timeval){1, 0});
	send_mtu_probe_handler(&mesh->loop, n);
}

static void mtu_probe_h(meshlink_handle_t *mesh, node_t *n, vpn_packet_t *packet, uint16_t len) {
	logger(mesh, MESHLINK_DEBUG, "Got MTU probe length %d from %s (%s)", packet->len, n->name, n->hostname);

	if(!packet->data[0]) {
		/* It's a probe request, send back a reply */

		packet->data[0] = 1;

		/* Temporarily set udp_confirmed, so that the reply is sent
		   back exactly the way it came in. */

		bool udp_confirmed = n->status.udp_confirmed;
		n->status.udp_confirmed = true;
		send_udppacket(mesh, n, packet);
		n->status.udp_confirmed = udp_confirmed;
	} else {
		/* It's a valid reply: now we know bidirectional communication
		   is possible using the address and socket that the reply
		   packet used. */

		n->status.udp_confirmed = true;

		/* If we haven't established the PMTU yet, restart the discovery process. */

		if(n->mtuprobes > 30) {
			if (len == n->maxmtu + 8) {
				logger(mesh, MESHLINK_INFO, "Increase in PMTU to %s (%s) detected, restarting PMTU discovery", n->name, n->hostname);
				n->maxmtu = sptps_maxmtu(&n->sptps);
				n->mtuprobes = 10;
				return;
			}

			if(n->minmtu)
				n->mtuprobes = 30;
			else
				n->mtuprobes = 1;
		}

		/* If applicable, raise the minimum supported MTU */

		if(len > n->maxmtu)
			len = n->maxmtu;
		if(n->minmtu < len)
			n->minmtu = len;

		// raise mtu along with the minmtu
		if(n->mtu < n->minmtu) {
			n->mtu = n->minmtu;

			// update meshlink and utcp for the mtu change
			update_node_mtu(mesh, n);
		}
	}
}

static uint16_t compress_packet(uint8_t *dest, const uint8_t *source, uint16_t len, int level) {
	if(level == 0) {
		memcpy(dest, source, len);
		return len;
	} else if(level == 10) {
		return -1;
	} else if(level < 10) {
#ifdef HAVE_ZLIB
		unsigned long destlen = MAXSIZE;
		if(compress2(dest, &destlen, source, len, level) == Z_OK)
			return destlen;
		else
#endif
			return -1;
	} else {
		return -1;
	}

	return -1;
}

static uint16_t uncompress_packet(uint8_t *dest, const uint8_t *source, uint16_t len, int level) {
	if(level == 0) {
		memcpy(dest, source, len);
		return len;
	} else if(level > 9) {
			return -1;
	}
#ifdef HAVE_ZLIB
	else {
		unsigned long destlen = MAXSIZE;
		if(uncompress(dest, &destlen, source, len) == Z_OK)
			return destlen;
		else
			return -1;
	}
#endif

	return -1;
}

/* VPN packet I/O */

static void receive_packet(meshlink_handle_t *mesh, node_t *n, vpn_packet_t *packet) {
	logger(mesh, MESHLINK_DEBUG, "Received packet of %d bytes from %s (%s)",
			   packet->len, n->name, n->hostname);

    if (n->status.blacklisted) {
        logger(mesh, MESHLINK_WARNING, "Dropping packet from blacklisted node %s", n->name);
    } else {
	n->in_packets++;
	n->in_bytes += packet->len;

	route(mesh, n, packet);
    }
}

static bool try_mac(meshlink_handle_t *mesh, node_t *n, const vpn_packet_t *inpkt) {
	return sptps_verify_datagram(&n->sptps, inpkt->data, inpkt->len);
}

static void receive_udppacket(meshlink_handle_t *mesh, node_t *n, vpn_packet_t *inpkt) {
	if(!n->sptps.state) {
		if(!n->status.waitingforkey) {
			logger(mesh, MESHLINK_DEBUG, "Got packet from %s (%s) but we haven't exchanged keys yet", n->name, n->hostname);
			send_req_key(mesh, n);
		} else {
			logger(mesh, MESHLINK_DEBUG, "Got packet from %s (%s) but he hasn't got our key yet", n->name, n->hostname);
		}
		return;
	}
	sptps_receive_data(&n->sptps, inpkt->data, inpkt->len);
}

void receive_tcppacket(meshlink_handle_t *mesh, connection_t *c, const char *buffer, int len) {
	vpn_packet_t outpkt;

	if(len > sizeof outpkt.data)
		return;

	outpkt.len = len;
	outpkt.tcp = true;
	memcpy(outpkt.data, buffer, len);

	receive_packet(mesh, c->node, &outpkt);
}

static void send_sptps_packet(meshlink_handle_t *mesh, node_t *n, vpn_packet_t *origpkt) {
	if(!n->status.validkey) {
		logger(mesh, MESHLINK_INFO, "No valid key known yet for %s (%s)", n->name, n->hostname);
		if(!n->status.waitingforkey)
			send_req_key(mesh, n);
		else if(n->last_req_key + 10 < mesh->loop.now.tv_sec) {
			logger(mesh, MESHLINK_DEBUG, "No key from %s after 10 seconds, restarting SPTPS", n->name);
			sptps_stop(&n->sptps);
			n->status.waitingforkey = false;
			send_req_key(mesh, n);
		}
		return;
	}

	uint8_t type = 0;

	// If it's a probe, send it immediately without trying to compress it.
	if(origpkt->probe) {
		sptps_send_record(&n->sptps, PKT_PROBE, origpkt->data, origpkt->len);
		return;
	}

	vpn_packet_t outpkt;

	if(n->outcompression) {
		int len = compress_packet(outpkt.data, origpkt->data, origpkt->len, n->outcompression);
		if(len < 0) {
			logger(mesh, MESHLINK_ERROR, "Error while compressing packet to %s (%s)", n->name, n->hostname);
		} else if(len < origpkt->len) {
			outpkt.len = len;
			origpkt = &outpkt;
			type |= PKT_COMPRESSED;
		}
	}

	sptps_send_record(&n->sptps, type, origpkt->data, origpkt->len);
	return;
}

static void choose_udp_address(meshlink_handle_t *mesh, const node_t *n, const sockaddr_t **sa, int *sock) {
	/* Latest guess */
	*sa = &n->address;
	*sock = n->sock;

	/* If the UDP address is confirmed, use it. */
	if(n->status.udp_confirmed)
		return;

	/* Send every third packet to n->address; that could be set
	   to the node's reflexive UDP address discovered during key
	   exchange. */

	static int x = 0;
	if(++x >= 3) {
		x = 0;
		return;
	}

	/* Otherwise, address are found in edges to this node.
	   So we pick a random edge and a random socket. */

	int i = 0;
	int j = rand() % n->edge_tree->count;
	edge_t *candidate = NULL;

	for splay_each(edge_t, e, n->edge_tree) {
		if(i++ == j) {
			candidate = e->reverse;
			break;
		}
	}

	if(candidate) {
		*sa = &candidate->address;
		*sock = rand() % mesh->listen_sockets;
	}

	/* Make sure we have a suitable socket for the chosen address */
	if(mesh->listen_socket[*sock].sa.sa.sa_family != (*sa)->sa.sa_family) {
		for(int i = 0; i < mesh->listen_sockets; i++) {
			if(mesh->listen_socket[i].sa.sa.sa_family == (*sa)->sa.sa_family) {
				*sock = i;
				break;
			}
		}
	}
}

static void choose_broadcast_address(meshlink_handle_t *mesh, const node_t *n, const sockaddr_t **sa, int *sock) {
	static sockaddr_t broadcast_ipv4 = {
		.in = {
			.sin_family = AF_INET,
			.sin_addr.s_addr = -1,
		}
	};

	static sockaddr_t broadcast_ipv6 = {
		.in6 = {
			.sin6_family = AF_INET6,
			.sin6_addr.s6_addr[0x0] = 0xff,
			.sin6_addr.s6_addr[0x1] = 0x02,
			.sin6_addr.s6_addr[0xf] = 0x01,
		}
	};

	*sock = rand() % mesh->listen_sockets;

	if(mesh->listen_socket[*sock].sa.sa.sa_family == AF_INET6) {
		if(mesh->localdiscovery_address.sa.sa_family == AF_INET6) {
			mesh->localdiscovery_address.in6.sin6_port = n->prevedge->address.in.sin_port;
			*sa = &mesh->localdiscovery_address;
		} else {
			broadcast_ipv6.in6.sin6_port = n->prevedge->address.in.sin_port;
			broadcast_ipv6.in6.sin6_scope_id = mesh->listen_socket[*sock].sa.in6.sin6_scope_id;
			*sa = &broadcast_ipv6;
		}
	} else {
		if(mesh->localdiscovery_address.sa.sa_family == AF_INET) {
			mesh->localdiscovery_address.in.sin_port = n->prevedge->address.in.sin_port;
			*sa = &mesh->localdiscovery_address;
		} else {
			broadcast_ipv4.in.sin_port = n->prevedge->address.in.sin_port;
			*sa = &broadcast_ipv4;
		}
	}
}

static void send_udppacket(meshlink_handle_t *mesh, node_t *n, vpn_packet_t *origpkt) {
	if(!n->status.reachable) {
		logger(mesh, MESHLINK_INFO, "Trying to send UDP packet to unreachable node %s (%s)", n->name, n->hostname);
		return;
	}

	return send_sptps_packet(mesh, n, origpkt);
}

bool send_sptps_data(void *handle, uint8_t type, const void *data, size_t len) {
	node_t *to = handle;
	meshlink_handle_t *mesh = to->mesh;

	/* Send it via TCP if it is a handshake packet, TCPOnly is in use, or this packet is larger than the MTU. */

	if(type >= SPTPS_HANDSHAKE || ((mesh->self->options | to->options) & OPTION_TCPONLY) || (type != PKT_PROBE && to->mtu && len > to->mtu)) {
		char buf[len * 4 / 3 + 5];
		b64encode(data, buf, len);
		/* If no valid key is known yet, send the packets using ANS_KEY requests,
		   to ensure we get to learn the reflexive UDP address. */
		if(!to->status.validkey) {
			to->incompression = mesh->self->incompression;
			return send_request(mesh, to->nexthop->connection, "%d %s %s %s -1 -1 -1 %d", ANS_KEY, mesh->self->name, to->name, buf, to->incompression);
		} else {
			return send_request(mesh, to->nexthop->connection, "%d %s %s %d %s", REQ_KEY, mesh->self->name, to->name, REQ_SPTPS, buf);
		}
	}

	/* Otherwise, send the packet via UDP */

	const sockaddr_t *sa;
	int sock;

	if(to->status.broadcast)
		choose_broadcast_address(mesh, to, &sa, &sock);
	else
		choose_udp_address(mesh, to, &sa, &sock);

	if(sendto(mesh->listen_socket[sock].udp.fd, data, len, 0, &sa->sa, SALEN(sa->sa)) < 0 && !sockwouldblock(sockerrno)) {
		if(sockmsgsize(sockerrno)) {
			// if failed, lessen mtu to at least one less than the failed length
			if(to->maxmtu >= len)
				to->maxmtu = len - 1;
			if(to->mtu >= len) {
				to->mtu = len - 1;
				// update meshlink and utcp for the mtu change
				update_node_mtu(mesh, to);
			}
		} else {
			logger(mesh, MESHLINK_WARNING, "Error sending UDP SPTPS packet to %s (%s): %s", to->name, to->hostname, sockstrerror(sockerrno));
			return false;
		}
	}

	return true;
}

bool receive_sptps_record(void *handle, uint8_t type, const void *data, uint16_t len) {
	node_t *from = handle;
	meshlink_handle_t *mesh = from->mesh;

	if(type == SPTPS_HANDSHAKE) {
		if(!from->status.validkey) {
			from->status.validkey = true;
			from->status.waitingforkey = false;
			logger(mesh, MESHLINK_INFO, "SPTPS key exchange with %s (%s) succesful", from->name, from->hostname);
		}
		return true;
	}

	if(len > sptps_maxmtu(&from->sptps)) {
		logger(mesh, MESHLINK_ERROR, "Packet from %s (%s) larger than maximum supported size (%d > %d)", from->name, from->hostname, len, MTU);
		return false;
	}

	vpn_packet_t inpkt;

	if(type == PKT_PROBE) {
		inpkt.len = len;
		inpkt.probe = true;
		memcpy(inpkt.data, data, len);
		mtu_probe_h(mesh, from, &inpkt, len);
		return true;
	} else {
		inpkt.probe = false;
	}

	if(type & ~(PKT_COMPRESSED)) {
		logger(mesh, MESHLINK_ERROR, "Unexpected SPTPS record type %d len %d from %s (%s)", type, len, from->name, from->hostname);
		return false;
	}

	if(type & PKT_COMPRESSED) {
		uint16_t ulen = uncompress_packet(inpkt.data, (const uint8_t *)data, len, from->incompression);
		if(ulen < 0) {
			return false;
		} else {
			inpkt.len = ulen;
		}
		if(inpkt.len > MAXSIZE) {
			logger(mesh, MESHLINK_ERROR, "Error: SPTPS uncompressed packet len %d > MAXSIZE %d", inpkt.len, MAXSIZE);
			abort();
		}
	} else {
		memcpy(inpkt.data, data, len);
		inpkt.len = len;
	}

	receive_packet(mesh, from, &inpkt);
	return true;
}

/*
  send a packet to the given vpn ip.
*/
void send_packet(meshlink_handle_t *mesh, node_t *n, vpn_packet_t *packet) {
	if(n == mesh->self) {
		n->out_packets++;
		n->out_bytes += packet->len;
		// TODO: send to application
		return;
	}

	logger(mesh, MESHLINK_DEBUG, "Sending packet of %d bytes to %s (%s)",
			   packet->len, n->name, n->hostname);

	if(!n->status.reachable) {
		logger(mesh, MESHLINK_WARNING, "Node %s (%s) is not reachable",
				   n->name, n->hostname);
		return;
	}

	n->out_packets++;
	n->out_bytes += packet->len;

	send_sptps_packet(mesh, n, packet);
	return;
}

/* Broadcast a packet using the minimum spanning tree */

void broadcast_packet(meshlink_handle_t *mesh, const node_t *from, vpn_packet_t *packet) {
	// Always give ourself a copy of the packet.
	if(from != mesh->self)
		send_packet(mesh, mesh->self, packet);

	logger(mesh, MESHLINK_INFO, "Broadcasting packet of %d bytes from %s (%s)",
			   packet->len, from->name, from->hostname);

	for list_each(connection_t, c, mesh->connections)
		if(c->status.active && c->status.mst && c != from->nexthop->connection)
			send_packet(mesh, c->node, packet);
}

static node_t *try_harder(meshlink_handle_t *mesh, const sockaddr_t *from, const vpn_packet_t *pkt) {
	node_t *n = NULL;
	bool hard = false;
	static time_t last_hard_try = 0;

	for splay_each(edge_t, e, mesh->edges) {
		if(!e->to->status.reachable || e->to == mesh->self)
			continue;

		if(sockaddrcmp_noport(from, &e->address)) {
			if(last_hard_try == mesh->loop.now.tv_sec)
				continue;
			hard = true;
		}

		if(!try_mac(mesh, e->to, pkt))
			continue;

		n = e->to;
		break;
	}

	if(hard)
		last_hard_try = mesh->loop.now.tv_sec;

	last_hard_try = mesh->loop.now.tv_sec;
	return n;
}

void handle_incoming_vpn_data(event_loop_t *loop, void *data, int flags) {
	meshlink_handle_t *mesh = loop->data;
	listen_socket_t *ls = data;
	vpn_packet_t pkt;
	char *hostname;
	sockaddr_t from = {{0}};
	socklen_t fromlen = sizeof from;
	node_t *n;
	int len;

	len = recvfrom(ls->udp.fd, pkt.data, MAXSIZE, 0, &from.sa, &fromlen);

	if(len <= 0 || len > MAXSIZE) {
		if(!sockwouldblock(sockerrno))
			logger(mesh, MESHLINK_ERROR, "Receiving packet failed: %s", sockstrerror(sockerrno));
		return;
	}

	pkt.len = len;
	logger(mesh, MESHLINK_DEBUG, "Received %d bytes of vpn data.", pkt.len);

	sockaddrunmap(&from); /* Some braindead IPv6 implementations do stupid things. */

	n = lookup_node_udp(mesh, &from);

	if(!n) {
		n = try_harder(mesh, &from, &pkt);
		if(n)
			update_node_udp(mesh, n, &from);
		else if(mesh->log_level >= MESHLINK_WARNING) {
			hostname = sockaddr2hostname(&from);
			logger(mesh, MESHLINK_WARNING, "Received UDP packet from unknown source %s", hostname);
			free(hostname);
			return;
		}
		else
			return;
	}

    if (n->status.blacklisted) {
			logger(mesh, MESHLINK_WARNING, "Dropping packet from blacklisted node %s", n->name);
            return;
    }
	n->sock = ls - mesh->listen_socket;

	receive_udppacket(mesh, n, &pkt);
}
