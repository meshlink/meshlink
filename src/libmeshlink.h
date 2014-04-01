/*
    libmeshlink.h -- Tincd Library
    Copyright (C) 2014 Guus Sliepen <guus@tinc-vpn.org> Saverio Proto <zioproto@gmail.com>

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
#include "node.h"

/* OLD: tinc_configuration_t provides all information required to setup "/etc/tinc"
I think tinc_setup() should basically do what cmd_init() from src/tincctl.c does, except it doesn't have to generate a tinc-up script.
*/
bool tinc_setup(const char* path);

bool tinc_start(const char* path);

bool tinc_stop();

// can be called from any thread
bool tinc_send_packet(node_t *receiver, const char* buf, unsigned int len);

// handler runs in tinc thread and should return immediately
bool tinc_set_packet_receive_handler(void (*handler)(const char* sender, const char* buf, unsigned int len));


//It might also be a good idea to add the option of looking up hosts by public
//key (fingerprints) instead of names.

node_t *tinc_get_host(const char *name);

bool tinc_get_hosts(node_t** hosts);
//This tinc_host_t is redundant because node_t should be ok
/*
struct tinc_host_t
{
    char* name;
    sockaddr_t address;
    char *hostname;
    char *fingerprint;
    unsigned int maxpacketsize;
    //TODO: anything else?
}
*/
bool tinc_sign(const char* payload, unsigned int len, const char** signature);

int tinc_verify(const char* sender, const char* payload, unsigned int plen, const char* signature, unsigned int slen);

/*
TODO: It would be good to add a void pointer here that will be passed on to the
handler function whenever it is called, or have a void pointer in node_t
that can be filled in by the application. That way, you can easily link an
application-specific data structure to a node_t.
*/
void channel_set_packet_send_handler(int (*handler)(const char* receiver, const char* buf, unsigned int len));
void channel_packet_receive_handler(const char* sender, const char* buf, unsigned int len);

bool channel_open(const char* partner, void(*read)(int id, const char* buf, unsigned int len), void(*result)(int result, int id));
void channel_close(int id);
bool channel_write(int id, const char* buf, unsigned int len, void(*result)(int result, int id, unsigned int written));


//We do need some more functions. First of all, we need to be able to add nodes
//to a VPN. To do that, either an invitation protocol should be used:

bool tinc_join_network(const char *invitation);
const char *tinc_generate_invitation(const char *name);

/*
Or two nodes should exchange some information (their name, address, public
key). If the application provides a way to exchange some data with another
node, then:
*/

bool tinc_export(char *buf, size_t *len);
node_t *tinc_import(const char *buf, size_t len);
/*
Last but not least, some way to get rid of unwanted nodes. Simplest is a
function that just blacklists a node.
Which should immediately cause the local tincd to ignore any data from that
host from that point on. Of course, a somewhat centrally managed,
automatically distributed blacklist or whitelist would be the next step.
*/
bool tinc_blacklist(node_t *host);




