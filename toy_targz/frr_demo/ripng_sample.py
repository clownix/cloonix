#!/usr/bin/env python3
r"""ripng_sample - Sample configuration (RIPng).

    ./ripng_sample.py [net]     the net defaults to nemo

The example of the FRR User Guide, page ripngd, section "Sample
configuration":
https://docs.frrouting.org/en/latest/ripngd.html#sample-configuration

    h1 ---- lan1 2001:db8:1::/64 ---- r1 ---- lan12 2001:db8:12::/64 ---- r2

  r1 carries the guide's configuration on eth0: RIPng, a static RIPng
  route 3ffe:506::/32, and the distribute-list local-only, which lets out
  of eth0 nothing but 3ffe:506::/32. r1 also redistributes its connected
  lan 2001:db8:1::/64.

  ACT 1  r2 learns 3ffe:506::/32 and nothing else from r1.
  ACT 2  the distribute-list is removed: 2001:db8:1::/64 goes out too.

Image: frr_trixie (zebra, ripngd)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r2 eth0 sniff=on
    cloonix cli nemo sniff r2 eth0 --dump "udp port 521"

  A response of r1, to ff02::9: one route entry, 3ffe:506::/32, during
  ACT 1; two after ACT 2.

Important
  The guide runs RIPng on sit1, a 6in4 tunnel of the 6-Bone days; eth0
  plays it here. 3ffe::/16 was the 6-Bone space, returned in 2006, kept
  as the guide wrote it. The guide's "distribute-list" names an ipv4
  list in today's frr, which ripngd does not apply: the line here is
  "ipv6 distribute-list", the one that filters the ipv6 routes.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, frr_setup, hold, in_parallel, step, story
from frrlib import conf, forwarding, has, lacks, opening, proof, until, \
    verdict, vtysh

HISTORY = """\
RIPng, RIP for IPv6, was written in RFC 2080 in 1997 by Gary Malkin and
Robert Minnear: the same distance vector, sent over UDP port 521 to the
multicast group ff02::9, with link-local next hops. The ripngd of frr was
written by Kunihiro Ishiguro and Yasuhiro Ohara for GNU Zebra, among the
first daemons to route IPv6 on linux.

Today: RIPng is still found in small IPv6 networks and labs, and in the
home network protocols that need a routing protocol with no setup.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

R1, R2, H1 = "r1", "r2", "h1"
ALL = (R1, R2, H1)
ROUTERS = (R1, R2)
L1, L12 = "lan1", "lan12"

WIRING = {
    R1: [(0, L12, "2001:db8:12::1/64"), (1, L1, "2001:db8:1::1/64")],
    R2: [(0, L12, "2001:db8:12::2/64")],
    H1: [(0, L1, "2001:db8:1::10/64")],
}
GATEWAY = {}

# The guide's configuration, sit1 -> eth0.
# The guide's lines, the filter first: they apply one by one, and an update
# sent before the filter is kept by r2 for 180 s.
R1_CONF = """\
hostname r1
debug ripng events
debug ripng packet
ipv6 access-list local-only permit 3ffe:506::0/32
ipv6 access-list local-only deny any
router ripng
 ipv6 distribute-list local-only out eth0
 route 3ffe:506::0/32
 redistribute connected
 network eth0"""

R2_CONF = """\
hostname r2
router ripng
 network eth0"""

CONFS = {R1: R1_CONF, R2: R2_CONF}
STATIC, LAN = "3ffe:506::/32", "2001:db8:1::/64"

LAYOUT = {H1: (-480, 0), L1: (-300, 0), R1: (-100, 0), L12: (120, 0),
          R2: (340, 0)}
ETHS = {(H1, "eth0"): 90, (R1, "eth1"): 270, (R1, "eth0"): 90,
        (R2, "eth0"): 270}

CARD = r"""
    h1 ---- lan1 ---- r1 ---- lan12 ---- r2

  RIPng from r1 to r2; out of eth0, r1 sends only 3ffe:506::/32.
"""

def configure(node):
    forwarding(node)
    frr_setup(node, ["ripngd"], CONFS[node])

def main():
    opening()

    step("act 1: out of eth0, r1 sends only 3ffe:506::/32")
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_static = until(lambda: has(R2, "show ipv6 ripng", [STATIC]), 90)
    ok_kept = lacks(R2, "show ipv6 ripng", [LAN])
    proof("r1, its RIPng table:", vtysh(R1, "show ipv6 ripng"))
    proof("r2, what it learnt:", vtysh(R2, "show ipv6 ripng"))
    hold()

    step("act 2: the distribute-list is removed, 2001:db8:1::/64 goes out")
    got = count_on(R2, 0, "udp port 521", lambda: (
        conf(R1, "router ripng",
             "no ipv6 distribute-list local-only out eth0"),
        until(lambda: has(R2, "show ipv6 ripng", [LAN]), 90))[1],
        settle=1.0)
    ok_lan = got["out"] if got else has(R2, "show ipv6 ripng", [LAN])
    if got is not None:
        story("    %d RIPng packet(s) on r2 eth0 meanwhile." % got["count"])
    proof("r2, what it learnt now:", vtysh(R2, "show ipv6 ripng"))

    verdict([("r2 learns 3ffe:506::/32", ok_static),
             ("the filter keeps 2001:db8:1::/64 in", ok_kept),
             ("without the filter, 2001:db8:1::/64 goes out", ok_lan)])

    story("""
the network is left up. Things to try:

  the RIPng state and its filters:
    cloonix cli %s ssh r1 "vtysh -c 'show ipv6 ripng status'"

  the routes RIPng gave the kernel of r2:
    cloonix cli %s ssh r2 "vtysh -c 'show ipv6 route ripng'"
""" % (NET, NET))

main()
