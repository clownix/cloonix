#!/usr/bin/env python3
"""sub_ftopo_consistency - dump json holds exactly what was built and nothing
else: the object arrays, the interface count, the lans under each endpoint,
and dump ovs-vsctl list-ports shows the ports each endpoint puts on the
bridge: a cnt eth of kind s is a _k_ port and its s_k_ sniff port, a nat and
a tap are one _k_ port each, and nothing else.
"""
from sublib import *

A, B, T, N = "tisubTa", "tisubTb", "tisubTt", "tisubTn"
L1, L2 = "tisubT1", "tisubT2"


def lans_of(t):
    out = {}
    for e in t.get("endp", []) or []:
        for lan in e.get("lans", []) or []:
            out.setdefault(lan, set()).add((e.get("name"), e.get("num")))
    return out


def ports(lan, cnt_eth, others):
    """The bridge holds exactly the ports of these endpoints."""
    out = must(cli("dump", "ovs-vsctl", "list-ports", "_b_" + lan),
               "list-ports")
    names = [l.strip() for l in out.splitlines() if l.strip()]
    k = [n for n in names if n.startswith("_k_")]
    sk = [n for n in names if n.startswith("s_k_")]
    if len(k) != cnt_eth + others or len(sk) != cnt_eth or \
            len(names) != len(k) + len(sk):
        raise Fail("ports of _b_%s: %s, for %d cnt eth and %d nat or tap"
                   % (lan, names, cnt_eth, others))


def sub_ftopo_consistency():
    clean(A, B, T, N)
    try:
        add_cnt(A, "ss")
        add_cnt(B, "s")
        must(cli("add", "tap", T), "add tap")
        must(cli("add", "nat", N), "add nat")
        for n in (T, N):
            wait_topo(n, True)
        must(cli("plug", A, "eth0", L1), "plug")
        must(cli("plug", B, "eth0", L1), "plug")
        must(cli("plug", A, "eth1", L2), "plug")
        must(cli("plug", T, L2), "plug")
        must(cli("plug", N, L2), "plug")
        t = ftopo()
        names = {}
        for kind in ("cnt", "kvm", "c2c", "tap", "nat", "a2b", "phy"):
            names[kind] = sorted(o.get("name") for o in t.get(kind, []) or [])
        want = {"cnt": [A, B], "kvm": [], "c2c": [], "tap": [T], "nat": [N],
                "a2b": [], "phy": []}
        if names != want:
            raise Fail("objects in ftopo %s, built %s" % (names, want))
        eth = dict((o["name"], o.get("nb_tot_eth")) for o in t["cnt"])
        if eth != {A: 2, B: 1}:
            raise Fail("nb_tot_eth in ftopo %s" % eth)
        got = lans_of(t)
        if got.get(L1) != {(A, 0), (B, 0)}:
            raise Fail("%s in ftopo has %s" % (L1, got.get(L1)))
        if got.get(L2) != {(A, 1), (T, 0), (N, 0)}:
            raise Fail("%s in ftopo has %s" % (L2, got.get(L2)))
        if set(got) != {L1, L2}:
            raise Fail("lans in ftopo: %s" % sorted(got))
        ports(L1, 2, 0)
        ports(L2, 1, 2)
        must(cli("unplug", A, "eth1", L2), "unplug")
        got = lans_of(ftopo())
        if got.get(L2) != {(T, 0), (N, 0)}:
            raise Fail("after unplug: %s has %s" % (L2, got.get(L2)))
        ports(L2, 0, 2)
        for n in (A, B, T, N):
            delete(n)
        t = ftopo()
        if any(t.get(k) for k in want) or lans_of(t):
            raise Fail("ftopo not empty after the deletes")
    finally:
        clean(A, B, T, N)


if __name__ == "__main__":
    run("ftopo consistency", sub_ftopo_consistency)
