#    node_step.sh -- Script to send signal to control Mesh Node Simulation
#    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# Read command-line arguments
prog_name=$1
signal=$2

# Find instance of running program and send the named signal to it
pid=`/bin/pidof -s ${prog_name}`
kill -${signal} ${pid}
exit $?
