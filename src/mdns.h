#pragma once

// SPDX-FileCopyrightText: 2020 Guus Sliepen <guus@meshlink.io>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdint.h>
#include <unistd.h>

size_t prepare_packet(void *buf, size_t size, const char *name, const char *protocol, const char *transport, uint16_t port, int nkeys, const char **keys, const char **values, bool response);
bool parse_packet(const void *buf, size_t size, char **name, const char *protocol, const char *transport, uint16_t *port, int nkeys, const char **keys, char **values, bool *response);

