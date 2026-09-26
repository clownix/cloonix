#!/usr/bin/env python3
"""sub_snf_corner - the sniffer corners testitall does not touch: a filter
that matches keeps the traffic and a filter that matches nothing stays near
zero (so the BPF really selects), and two sniffers on the two ends of one
wire both see the same ping at once (so a second reader does not shut the
first out). Counts are conservative and env-tunable:

    SNF_PING       pings driven under each capture   (default 5)
    SNF_NOISE_MAX  most packets a no-match filter may catch (default 3)
"""
import os
from sublib import *

A, B, L = "tisubNa", "tisubNb", "tisubNl"


def env_i(name, default):
    try:
        return int(os.environ.get(name, default))
    except ValueError:
        return default


PING = env_i("SNF_PING", 5)
NOISE_MAX = env_i("SNF_NOISE_MAX", 3)


def sub_snf_corner():
    clean(A, B)
    try:
        add_cnt(A, "s")
        add_cnt(B, "s")
        must(cli("plug", A, "eth0", L), "plug a")
        must(cli("plug", B, "eth0", L), "plug b")
        wait_answer(A)
        wait_answer(B)
        address(A, 0, "10.61.0.1/24")
        address(B, 0, "10.61.0.2/24")
        ping(A, "10.61.0.2")            # warm the arp before the captures

        s = Sniff(A, 0, "icmp").start()
        ping(A, "10.61.0.2", PING)
        n = expect_count(s, PING, what="icmp seen while pinging")
        print("      icmp filter caught %d packets over %d pings" % (n, PING))

        s = Sniff(A, 0, "udp port 65001").start()
        ping(A, "10.61.0.2", PING)
        n = expect_count(s, 0, NOISE_MAX, what="a filter matching nothing")
        print("      unmatched filter caught %d packets (<= %d)"
              % (n, NOISE_MAX))

        sa = Sniff(A, 0, "icmp").start()
        sb = Sniff(B, 0, "icmp").start()
        ping(A, "10.61.0.2", PING)
        na = expect_count(sa, PING, what="end A of a shared ping")
        nb = expect_count(sb, PING, what="end B of a shared ping")
        print("      two sniffers at once: end A %d, end B %d" % (na, nb))

        delete(A)
        delete(B)
    finally:
        clean(A, B)


if __name__ == "__main__":
    run("snf corner cases", sub_snf_corner)
