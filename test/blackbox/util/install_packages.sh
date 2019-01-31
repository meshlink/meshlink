#!/bin/bash

#    nat.sh - Script to create a NAT using LXC Container
#		  Designed to work on unprivileged Containers
#    Copyright (C) 2019  Guus Sliepen <guus@meshlink.io>
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

if [ $# -le 1 ] 
	then
	echo "enter valid arguments"
	exit 1
fi

container=$1	
update_cmd="apt-get update -y >> /dev/null"
echo "${update_cmd}" | lxc-attach -n ${container} -- 

while test $# -gt 1
do
    shift
    pkg_name=$1
		install_cmd="apt-get install ${pkg_name} -y >> /dev/null"
		echo "${install_cmd}" | lxc-attach -n ${container} -- 
		if [ $? -ne 0 ] 
		then
			 echo "${pkg_name} installation failed in ${container} retrying to install again"
			 sleep 1
			 echo "${update_cmd}" | lxc-attach -n ${container} -- 
			 sleep 1
			 echo "${install_cmd}" | lxc-attach -n ${container} --
			 if [ $? -ne 0 ] 
			 then
				echo "${pkg_name} installation failed in ${container} container"
			 	exit 1
			 fi
		fi
		echo "Installed ${pkg_name} in container ${container}"
done

exit 0
