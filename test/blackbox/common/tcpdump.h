/*
    tcpdump.h -- Declarations of common callback handlers and signal handlers for
                            black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>

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

#ifndef TCPDUMP_H
#define TCPDUMP_H

#define TCPDUMP_LOG_FILE "tcpdump.log"

extern pid_t tcpdump_start(char *);
extern void tcpdump_stop(pid_t tcpdump_pid);

#endif // TCPDUMP_H
