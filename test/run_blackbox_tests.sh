#!/bin/sh
host=$(hostname)
meshlinkrootpath="/home/${host}/meshlink"
lxcpath="/var/lib/lxc"
lxcbridge="lxcbr0"
ethifname="wlp2s0"
arch=$(dpkg --print-architecture)

blackbox/run_blackbox_tests/run_blackbox_tests ${meshlinkrootpath} ${lxcpath} ${lxcbridge} ${ethifname} ${arch} 2> run_blackbox_test_cases.log
