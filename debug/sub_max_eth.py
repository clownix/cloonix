#!/usr/bin/env python3
"""sub_max_eth - a machine may have MAX_ETH_VM (32) interfaces: two containers
with 32 each, both kinds mixed, every pair cabled on its own lan and a ping
across each of the 32; 33 is refused with the ceiling in the reason.
"""
from sublib import *

A, B = "tisubEa", "tisubEb"
MAX = 32
LAN = "tisubE%02d"
ETH = ("sv" * 16)


def sub_max_eth():
    clean(A, B)
    try:
        r = cli("add", "cnt", "tisubE33", "eth=%d" % 33, CNT_ROOTFS)
        if r.returncode == 0:
            raise Fail("33 interfaces were accepted")
        if "32" not in (r.stdout + r.stderr):
            raise Fail("the refusal of 33 does not name the ceiling: %s"
                       % (r.stdout + r.stderr).strip())
        add_cnt(A, ETH)
        add_cnt(B, ETH)
        for i in range(MAX):
            must(cli("plug", A, "eth%d" % i, LAN % i),
                 "plug eth%d" % i)
            must(cli("plug", B, "eth%d" % i, LAN % i),
                 "plug eth%d" % i)
        wait_answer(A)
        wait_answer(B)
        for i in range(MAX):
            ssh(A, "ip addr add 10.%d.0.1/24 dev eth%d; ip link set eth%d up"
                % (100 + i, i, i))
            ssh(B, "ip addr add 10.%d.0.2/24 dev eth%d; ip link set eth%d up"
                % (100 + i, i, i))
        n = int(ssh(A, "ls /sys/class/net | grep -c '^eth'").strip())
        if n != MAX:
            raise Fail("%s has %d eth interfaces" % (A, n))
        for i in range(MAX):
            ping(A, "10.%d.0.2" % (100 + i), 2)
        t = ftopo()
        eth = dict((o["name"], o.get("nb_tot_eth")) for o in t["cnt"])
        if eth != {A: MAX, B: MAX}:
            raise Fail("nb_tot_eth in ftopo: %s" % eth)
        delete(A)
        delete(B)
    finally:
        clean(A, B)


if __name__ == "__main__":
    run("32 interfaces", sub_max_eth)
