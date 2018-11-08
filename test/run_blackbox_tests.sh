#!/bin/sh
meshlinkrootpath=$(realpath ${0%/*}/..)
host=$(hostname)
lxcpath=$(lxc-config lxc.lxcpath)
lxcbridge="lxcbr0"
ethifname=$(ip route show default | awk '{print $5}' | head -1)
arch=$(dpkg --print-architecture)

test -f $HOME/.config/meshlink_blackbox.conf && . $HOME/.config/meshlink_blackbox.conf

${0%/*}/blackbox/run_blackbox_tests/run_blackbox_tests ${meshlinkrootpath} ${lxcpath} ${lxcbridge} ${ethifname} ${arch} 2> run_blackbox_test_cases.log
