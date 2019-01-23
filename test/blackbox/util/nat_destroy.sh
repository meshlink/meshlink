#!/bin/bash
router=${1}
router_bridge="${1}_bridge"

echo + Stopping router......
lxc-stop -n ${router}

echo + Removing NATs bridge....

ifconfig ${router_bridge} down

brctl delbr ${router_bridge}

echo + Destroing the routers.....

lxc-destroy -n ${router} >> /dev/null
