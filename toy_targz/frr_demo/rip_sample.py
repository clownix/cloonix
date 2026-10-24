#!/usr/bin/env python3
r"""rip_sample - Sample configuration (RIP).

    ./rip_sample.py [net]     the net defaults to nemo

The example of the FRR User Guide, page ripd, section "Sample
configuration":
https://docs.frrouting.org/en/latest/ripd.html#sample-configuration

    h1 ---- lan1 11.1.1.0/24 ---- r1 ---- lan12 ---- r2 ---- lan2 ---- h2
                                                            10.20.0.0/16
                                                           172.16.20.0/24

  r1 carries the guide's configuration word for word: RIP on 11.0.0.0/8
  and on eth0, a static RIP route 10.0.0.0/8, and the distribute-list
  private-only, which accepts on eth0 nothing but 10.0.0.0/8.

  ACT 1  r1 accepts 10.20.0.0/16 from r2 and refuses 172.16.20.0/24; r2
         learns 11.1.1.0/24 and the static 10.0.0.0/8. h1 reaches h2.
  ACT 2  the distribute-list is removed: 172.16.20.0/24 comes in.

Image: frr_trixie (zebra, ripd)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r1 eth0 sniff=on
    cloonix cli nemo sniff r1 eth0 --dump "udp port 520"

  A response of r2, to 224.0.0.9: it carries 172.16.20.0/24, which r1
  receives and throws away while the distribute-list is there.
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
    ssh, step, story
from frrlib import conf, forwarding, has, lacks, opening, ping, pings, \
    proof, until, verdict, vtysh

HISTORY = """\
RIP comes from the Xerox PARC Universal Packet routing of the 1970s, then
routed, written by Mike Karels for 4.2BSD in 1982, and was written down
afterwards by Charles Hedrick in RFC 1058, 1988; RIPv2, RFC 2453 by Gary
Malkin, added masks and multicast. A distance-vector protocol, metric 16 is
infinity. The ripd of frr was written by Kunihiro Ishiguro for GNU Zebra.

Today: RIP survives in small networks, in some industrial equipment, and in
every networking course, where it is the first routing protocol taught.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

H1, R1, R2, H2 = "h1", "r1", "r2", "h2"
ALL = (R1, R2, H1, H2)
ROUTERS = (R1, R2)
L1, L12, L2 = "lan1", "lan12", "lan2"

WIRING = {
    H1: [(0, L1, "11.1.1.10/24")],
    R1: [(0, L12, "192.168.12.1/24"), (1, L1, "11.1.1.1/24")],
    R2: [(0, L12, "192.168.12.2/24"), (1, L2, "10.20.0.1/16")],
    H2: [(0, L2, "10.20.0.10/16")],
}
GATEWAY = {H1: "11.1.1.1", H2: "10.20.0.1"}

# The guide's configuration.
# The guide's lines, the filter first: they apply one by one, and a route
# taken before the filter stays 180 s.
R1_CONF = """\
hostname r1
debug rip events
debug rip packet
access-list private-only permit 10.0.0.0/8
access-list private-only deny any
router rip
 distribute-list private-only in eth0
 route 10.0.0.0/8
 network 11.0.0.0/8
 network eth0"""

R2_CONF = """\
hostname r2
router rip
 network eth0
 redistribute connected"""

CONFS = {R1: R1_CONF, R2: R2_CONF}
PRIV, PUB = "10.20.0.0/16", "172.16.20.0/24"

LAYOUT = {
    H1: (-520, 0), L1: (-390, 0), R1: (-220, 0), L12: (0, 0),
    R2: (220, 0), L2: (390, 0), H2: (520, 0),
}
ETHS = {
    (H1, "eth0"): 90, (R1, "eth1"): 270, (R1, "eth0"): 90,
    (R2, "eth0"): 270, (R2, "eth1"): 90, (H2, "eth0"): 270,
}

CARD = r"""
    h1 ---- r1 ---- r2 ---- h2

  RIP between r1 and r2; on eth0, r1 accepts only what is in 10.0.0.0/8.
"""

def configure(node):
    forwarding(node)
    frr_setup(node, ["ripd"], CONFS[node])

def main():
    opening()
    ssh(R2, "ip addr add 172.16.20.1/24 dev eth1")

    step("act 1: r1 accepts 10.0.0.0/8 only from eth0")
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_in = until(lambda: has(R1, "show ip rip", [PRIV]), 90)
    ok_out = lacks(R1, "show ip rip", [PUB])
    ok_r2 = until(lambda: has(R2, "show ip rip", ["11.1.1.0/24",
                                                  "10.0.0.0/8"]), 90)
    proof("r1, its RIP routes:", vtysh(R1, "show ip rip"))
    proof("r2, its RIP routes:", vtysh(R2, "show ip rip"))
    until(lambda: pings(H1, "10.20.0.10"), 30)
    r = ping(H1, "10.20.0.10")
    proof("h1 -> h2:", r)
    ok_ping = reached(r)
    hold()

    step("act 2: the distribute-list is removed, 172.16.20.0/24 comes in")
    got = count_on(R1, 0, "udp port 520", lambda: (
        conf(R1, "router rip", "no distribute-list private-only in eth0"),
        until(lambda: has(R1, "show ip rip", [PUB]), 90))[1], settle=1.0)
    ok_pub = got["out"] if got else has(R1, "show ip rip", [PUB])
    if got is not None:
        story("    %d RIP packet(s) on r1 eth0 meanwhile." % got["count"])
    proof("r1, its RIP routes now:", vtysh(R1, "show ip rip"))

    verdict([("r1 accepts 10.20.0.0/16", ok_in),
             ("r1 refuses 172.16.20.0/24", ok_out),
             ("r2 learns 11.1.1.0/24 and the static 10.0.0.0/8", ok_r2),
             ("h1 reaches h2", ok_ping),
             ("without the filter, 172.16.20.0/24 comes in", ok_pub)])

    story("""
the network is left up. Things to try:

  the RIP state, timers and filters:
    cloonix cli %s ssh r1 "vtysh -c 'show ip rip status'"

  the responses, every 30 seconds:
    cloonix cli %s sniff r1 eth0 --dump "udp port 520"
""" % (NET, NET))

main()
