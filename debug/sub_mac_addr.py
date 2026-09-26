#!/usr/bin/env python3
"""sub_mac_addr - mac_addr=eth<n>:<mac> on a cnt and on a kvm: ip link in
the guest shows it, the other interface keeps a random one, dump commands echoes
the forced one only.
"""
from sublib import *

C, K = "tisubMc", "tisubMk"
MAC = "02:11:22:33:44:55"


def check(node):
    got = ssh(node, "cat /sys/class/net/eth1/address").strip()
    if got != MAC:
        raise Fail("%s eth1 is %s, asked %s" % (node, got, MAC))
    other = ssh(node, "cat /sys/class/net/eth0/address").strip()
    if other == MAC:
        raise Fail("%s eth0 got the forced mac too" % node)
    lst = dump_list()
    if "mac_addr=eth1:%s" % MAC not in lst:
        raise Fail("dump commands does not echo the mac of %s" % node)
    if "mac_addr=eth0:" in lst:
        raise Fail("dump commands echoes a mac for the random eth0 of %s" % node)


def sub_mac_addr():
    clean(C, K)
    try:
        add_cnt(C, "ss", ("mac_addr=eth1:" + MAC,))
        wait_answer(C)
        check(C)
        delete(C)
        kvm(K, "mac_addr=eth1:" + MAC, eth="sv")
        check(K)
        delete(K)
    finally:
        clean(C, K)


if __name__ == "__main__":
    run("mac_addr", sub_mac_addr)
