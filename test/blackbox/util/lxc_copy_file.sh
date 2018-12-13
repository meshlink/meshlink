#    lxc_copy.sh -- Script to transfer multiple files into an LXC Container
#                   Designed to work on unprivileged Containers
#    Copyright (C) 2018  Guus Sliepen <guus@meshlink.io>
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

# Read Command-line arguments
srcfilepath=$1
containername=$2
dstfilepath=$3

# Copy file into Container
cat ${srcfilepath} | lxc-attach -n ${containername} -- sh -c "cat > ${dstfilepath}"
