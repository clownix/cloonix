#!/usr/bin/env python3
r"""eigrp_sample - Sample configuration (EIGRP).

    ./eigrp_sample.py [net]     the net defaults to nemo

The example of the FRR User Guide, page eigrpd, section "Sample
configuration":
https://docs.frrouting.org/en/latest/eigrpd.html#sample-configuration

    h1 ---- lan10 ---- r1 ---- lan1 192.168.1.0/24 ---- r2 ---- lan20 ---- h2
       192.168.10.0/24                                     192.168.20.0/24

  r1 and r2 carry the guide's "router eigrp 4453, network 192.168.1.0/24",
  each with one more network line for the lan of its host.

  ACT 1  the two routers become EIGRP neighbours over 192.168.1.0/24.
  ACT 2  each learns the host lan of the other, h1 reaches h2.

Image: frr_trixie (zebra, eigrpd)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r1 eth0 sniff=on
    cloonix cli nemo sniff r1 eth0 --dump "ip proto 88"

  An EIGRP update, IP protocol 88 to 224.0.0.10, carrying an internal
  route TLV for 192.168.20.0/24 with its bandwidth and delay.

Important
  The guide's "password", "enable password" and "log stdout" lines are
  about the vty and are left out.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, frr_setup, hold, in_parallel, reached, \
    step, story
from frrlib import forwarding, has, opening, ping, pings, proof, until, \
    verdict, vtysh

HISTORY = """\
EIGRP was written at Cisco in the early 1990s by Dino Farinacci and
others, on the DUAL algorithm of J.J. Garcia-Luna-Aceves, which keeps a
loop-free backup ready before it is needed. Cisco kept it proprietary for
twenty years and published it as RFC 7868 in 2016. The eigrpd of frr was
started in 2017 by Donnie Savage and students of the Brno University of
Technology.

Today: EIGRP lives on in Cisco enterprise networks; eigrpd lets other
routers join them.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

H1, R1, R2, H2 = "h1", "r1", "r2", "h2"
ALL = (R1, R2, H1, H2)
ROUTERS = (R1, R2)
L10, L1, L20 = "lan10", "lan1", "lan20"

WIRING = {
    H1: [(0, L10, "192.168.10.10/24")],
    R1: [(0, L1, "192.168.1.1/24"), (1, L10, "192.168.10.1/24")],
    R2: [(0, L1, "192.168.1.2/24"), (1, L20, "192.168.20.1/24")],
    H2: [(0, L20, "192.168.20.10/24")],
}
GATEWAY = {H1: "192.168.10.1", H2: "192.168.20.1"}
HOSTNET = {R1: "192.168.10.0/24", R2: "192.168.20.0/24"}

def frr_conf(node):
    return "\n".join([
        "hostname %s" % node,
        "router eigrp 4453",
        " network 192.168.1.0/24",
        " network %s" % HOSTNET[node]])

LAYOUT = {
    H1: (-520, 0), L10: (-390, 0), R1: (-220, 0), L1: (0, 0),
    R2: (220, 0), L20: (390, 0), H2: (520, 0),
}
ETHS = {
    (H1, "eth0"): 90, (R1, "eth1"): 270, (R1, "eth0"): 90,
    (R2, "eth0"): 270, (R2, "eth1"): 90, (H2, "eth0"): 270,
}

CARD = r"""
    h1 ---- r1 ---- r2 ---- h2

  EIGRP, autonomous system 4453, between r1 and r2.
"""

def configure(node):
    forwarding(node)
    frr_setup(node, ["eigrpd"], frr_conf(node))

def main():
    opening()

    step("act 1: r1 and r2 become EIGRP neighbours")
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_nbr = until(lambda: has(R1, "show ip eigrp neighbors",
                               ["192.168.1.2"]), 90)
    proof("r1, its EIGRP neighbours:", vtysh(R1, "show ip eigrp neighbors"))
    hold()

    step("act 2: each learns the host lan of the other, h1 reaches h2")
    got = count_on(R1, 0, "ip proto 88", lambda: until(
        lambda: has(R1, "show ip route eigrp", ["192.168.20.0/24"]) and
        has(R2, "show ip route eigrp", ["192.168.10.0/24"]), 90), settle=1.0)
    ok_routes = got["out"] if got else \
        has(R1, "show ip route eigrp", ["192.168.20.0/24"])
    proof("r1, its EIGRP topology:", vtysh(R1, "show ip eigrp topology"))
    proof("r1, the routes EIGRP gave it:", vtysh(R1, "show ip route eigrp"))
    until(lambda: pings(H1, "192.168.20.10"), 30)
    r = ping(H1, "192.168.20.10")
    proof("h1 -> h2:", r)
    ok_ping = reached(r)

    verdict([("r1 and r2 are EIGRP neighbours", ok_nbr),
             ("each learns the other's host lan", ok_routes),
             ("h1 reaches h2", ok_ping)])

    story("""
the network is left up. Things to try:

  the topology table, successors and feasible distances:
    cloonix cli %s ssh r2 "vtysh -c 'show ip eigrp topology all-links'"

  the hellos, every 5 seconds:
    cloonix cli %s sniff r1 eth0 --dump "ip proto 88"
""" % (NET, NET))

main()
