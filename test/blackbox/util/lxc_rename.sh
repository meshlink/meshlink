#    lxc_rename.sh - Script to rename an LXC Container
#		  Designed to work on unprivileged Containers
#    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>
#                        Manav Kumar Mehta <manavkumarm@yahoo.com>
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
lxcpath=$1
oldname=$2
newname=$3

# Run command inside Container by attaching to the Container and sending it the command
mv ${lxcpath}/${oldname} ${lxcpath}/${newname}
sed {s/${oldname}/${newname}/} ${lxcpath}/${newname}/config > ${lxcpath}/${newname}/config1
mv ${lxcpath}/${newname}/config1 ${lxcpath}/${newname}/config
exit $?
