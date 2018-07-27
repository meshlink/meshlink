#!/bin/sh
meshlinkrootpath="/home/sairoop/meshlink"
lxcpath="/var/lib/lxc"
lxcbridge="lxcbr0"
ethifname="wlp2s0"
arch="amd64" #amd64(64 bit)/i386(32 bit)

blackbox/run_blackbox_tests/run_blackbox_tests ${meshlinkrootpath} ${lxcpath} ${lxcbridge} ${ethifname} ${arch} 2> run_blackbox_test_cases.log
