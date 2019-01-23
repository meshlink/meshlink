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
if [ $# -ne 3 ] 
	then
	echo "enter valid arguments"
	exit 1
fi
router_container=$1
router_bridge="${router_container}_bridge"
router_conf_path="${2}/${router_container}/config"
meshlinkrootpath=$3

MAXCOUNT=10
RANGE=16
number1_1=$RANDOM
number1_2=$RANDOM
number2_1=$RANDOM
number2_2=$RANDOM

let "number1_1 %= $RANGE"
let "number1_2 %= $RANGE"
let "number2_1 %= $RANGE"
let "number2_2 %= $RANGE"

number1_1="$((echo "obase=16; ${number1_1}") | bc)"
number1_2="$((echo "obase=16; ${number1_2}") | bc)"
number2_1="$((echo "obase=16; ${number2_1}") | bc)"
number2_2="$((echo "obase=16; ${number2_2}") | bc)"

echo + Creating nat bridge
ifconfig ${router_bridge} down >/dev/null 2>/dev/null
brctl delbr ${router_bridge} >/dev/null 2>/dev/null
brctl addbr ${router_bridge}
ifconfig ${router_bridge} up

# Destroying the existing router if already exists
lxc-stop -n ${router_container} >/dev/null 2>/dev/null
lxc-destroy -n ${router_container} >/dev/null 2>/dev/null

echo + Creating router
lxc-create -t download -n ${router_container}  -- -d ubuntu -r trusty -a amd64 >> /dev/null
echo + Creating config file for router
echo "lxc.net.0.name = eth0" >> ${router_conf_path}
echo " " >> ${router_conf_path}
echo "lxc.net.1.type = veth" >> ${router_conf_path}
echo "lxc.net.1.flags = up" >> ${router_conf_path}
echo "lxc.net.1.link = ${router_bridge}" >> ${router_conf_path}
echo "lxc.net.1.name = eth1" >> ${router_conf_path}
echo "lxc.net.1.hwaddr = 00:16:3e:ab:32:2a" >> ${router_conf_path}

echo + Starting Router
lxc-start -n ${router_container}

echo + Waiting for IP address..
while [ -z `lxc-info -n ${router_container} -iH` ]
do 
	sleep 1
done
eth0_ip=`lxc-info -n ${router_container} -iH`
echo "Obtained IP address: ${eth0_ip}"

###############################################################################################################

echo "Installing and Configuring iptables, dnsmasq  conntrack packages in ${1}"
${meshlinkrootpath}/test/blackbox/util/install_packages.sh ${1} iptables dnsmasq conntrack 
if [ $? -ne 0 ] 
then
	exit 1
fi

cmd="echo \"interface=eth1\" >> /etc/dnsmasq.conf"
echo "${cmd}" | lxc-attach -n ${router_container} --
cmd="echo \"bind-interfaces\" >> /etc/dnsmasq.conf"
echo "${cmd}" | lxc-attach -n ${router_container} --
cmd="echo \"listen-address=172.16.0.1\" >> /etc/dnsmasq.conf"
echo "${cmd}" | lxc-attach -n ${router_container} --
cmd="echo \"dhcp-range=172.16.0.2,172.16.0.254,12h\" >> /etc/dnsmasq.conf"
echo "${cmd}" | lxc-attach -n ${router_container} --
cmd="ifconfig eth1 172.16.0.1 netmask 255.255.255.0 up"
echo "${cmd}" | lxc-attach -n ${router_container} --
if [ $? -ne 0 ] 
then
	echo "Failed to configure eth1 interface"
	exit 1
fi
cmd="service dnsmasq restart >> /dev/null"
echo "${cmd}" | lxc-attach -n ${router_container} --
if [ $? -ne 0 ] 
then
	echo "Failed to restart service"
	exit 1
fi

echo + Configuring NAT for ${1}....
cmd="iptables -t nat -A POSTROUTING -o eth0 -j SNAT --to-source ${eth0_ip} "
echo "${cmd}" | sudo lxc-attach -n ${router_container} -- 
if [ $? -ne 0 ] 
then
	echo "Failed to apply NAT rule"
	exit 1
fi
cmd="iptables -t nat -A PREROUTING -i eth0 -j DNAT --to-destination 172.16.0.1 "
echo "${cmd}" | sudo lxc-attach -n ${router_container} -- 
if [ $? -ne 0 ] 
then
	echo "Failed to apply NAT rule"
	exit 1
fi
echo "Router created and configured with Full-cone NAT"

exit 0
