#!/usr/bin/env python3
r"""ospf6_sample - Sample configuration (OSPFv3).

    ./ospf6_sample.py [net]     the net defaults to nemo

The two examples of the FRR User Guide, page ospf6d, section "Sample
configuration": "ospf6d configured on one interface and area" and the
"Larger example with policy and various options set".
https://docs.frrouting.org/en/latest/ospf6d.html#sample-configuration

    r1 (212.17.55.53) ---- lan12 2001:db8:12::/64 ---- r2 (255.1.1.1)
                                                        statics 2001:db8:a::/64
                                                                2001:db8:b::/64

  r1 is the first example; r2 the larger one: interface timers, priority 0,
  and "redistribute static route-map static-ospf6".

  ACT 1  the adjacency comes up; r2, priority 0, can never be the DR.
  ACT 2  r2 has two static routes, and the guide's policy lets none of
         them in: the prefix-list test-prefix holds only "deny any".
  ACT 3  one permit line is added to test-prefix: 2001:db8:a::/64 enters
         OSPFv3 as an external route, type 2, metric 2000, as the
         route-map says; 2001:db8:b::/64 stays out.

Image: frr_trixie (zebra, staticd, ospf6d)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r1 eth0 sniff=on
    cloonix cli nemo sniff r1 eth0 --dump "ip6 proto 89"

  The link state update of ACT 3, to ff02::5: an AS-external LSA for
  2001:db8:a::/64, metric 2000, the E bit set for type 2.

Important
  fxp0 and lo0 of the guide are eth0 and lo; "router-id" is written
  "ospf6 router-id", its current form; the vty lines (line vty,
  access-class) are left out.
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
OSPFv3, OSPF for IPv6, came in RFC 2740 in 1999 by Rob Coltun, Dennis
Ferguson and John Moy, now RFC 5340: the same link states, but run per link
rather than per subnet, with addresses taken out of the hellos and put in
their own LSAs. The ospf6d of frr was written by Yasuhiro Ohara for GNU
Zebra, one of the first OSPFv3 implementations.

Today: OSPFv3 carries the IPv6 routes of the enterprise networks that run
OSPFv2 for IPv4.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

R1, R2 = "r1", "r2"
ALL = (R1, R2)
L12 = "lan12"

WIRING = {
    R1: [(0, L12, "2001:db8:12::1/64")],
    R2: [(0, L12, "2001:db8:12::2/64")],
}
GATEWAY = {}

R1_CONF = """\
hostname r1
interface eth0
 ipv6 ospf6 area 0.0.0.0
 ipv6 ospf6 instance-id 0
router ospf6
 ospf6 router-id 212.17.55.53
 area 0.0.0.0 range 2001:770:105:2::/64"""

R2_CONF = """\
hostname r2
debug ospf6 neighbor state
ipv6 route 2001:db8:a::/64 blackhole
ipv6 route 2001:db8:b::/64 blackhole
interface eth0
 ipv6 ospf6 area 0.0.0.0
 ipv6 ospf6 cost 1
 ipv6 ospf6 hello-interval 10
 ipv6 ospf6 dead-interval 40
 ipv6 ospf6 retransmit-interval 5
 ipv6 ospf6 priority 0
 ipv6 ospf6 transmit-delay 1
 ipv6 ospf6 instance-id 0
interface lo
 ipv6 ospf6 cost 1
 ipv6 ospf6 hello-interval 10
 ipv6 ospf6 dead-interval 40
 ipv6 ospf6 retransmit-interval 5
 ipv6 ospf6 priority 1
 ipv6 ospf6 transmit-delay 1
 ipv6 ospf6 instance-id 0
router ospf6
 ospf6 router-id 255.1.1.1
 redistribute static route-map static-ospf6
ipv6 prefix-list test-prefix seq 1000 deny any
route-map static-ospf6 permit 10
 match ipv6 address prefix-list test-prefix
 set metric-type type-2
 set metric 2000"""

CONFS = {R1: R1_CONF, R2: R2_CONF}
NA, NB = "2001:db8:a::/64", "2001:db8:b::/64"

LAYOUT = {R1: (-300, 0), L12: (0, 0), R2: (300, 0)}
ETHS = {(R1, "eth0"): 90, (R2, "eth0"): 270}

CARD = r"""
    r1 ---- lan12 ---- r2  (statics, redistribute static route-map)

  OSPFv3 between two routers; a route-map decides which static routes of
  r2 enter it.
"""

def configure(node):
    forwarding(node)
    frr_setup(node, ["staticd", "ospf6d"], CONFS[node])

def main():
    opening()

    step("act 1: the adjacency comes up, r2 has priority 0")
    in_parallel(configure, list(ALL), "frr start")
    ok_adj = until(lambda: has(R1, "show ipv6 ospf6 neighbor", ["Full"]), 90)
    proof("r1, its neighbour:", vtysh(R1, "show ipv6 ospf6 neighbor"))
    ok_dr = has(R1, "show ipv6 ospf6 interface eth0", ["DR: 212.17.55.53"])
    proof("r1, its interface, r1 is the DR:",
          vtysh(R1, "show ipv6 ospf6 interface eth0"))
    hold()

    step("act 2: the policy of the guide lets no static route in")
    ok_none = until(lambda: has(R2, "show ipv6 route static", [NA, NB]), 30) \
        and lacks(R1, "show ipv6 route", [NA, NB])
    proof("r2, its static routes:", vtysh(R2, "show ipv6 route static"))
    proof("r1, its OSPFv3 routes, none of them:",
          vtysh(R1, "show ipv6 ospf6 route"))
    hold()

    step("act 3: test-prefix permits 2001:db8:a::/64, it enters as type 2")
    got = count_on(R1, 0, "ip6 proto 89", lambda: (
        conf(R2, "ipv6 prefix-list test-prefix seq 10 permit %s" % NA),
        until(lambda: has(R1, "show ipv6 route ospf6", [NA]), 60))[1],
        settle=1.0)
    ok_a = got["out"] if got else has(R1, "show ipv6 route ospf6", [NA])
    ok_b = lacks(R1, "show ipv6 route", [NB])
    ok_metric = has(R1, "show ipv6 ospf6 route external-2 detail",
                    ["Metric Type: 2", "(2000)"])
    proof("r1, the external route:",
          vtysh(R1, "show ipv6 ospf6 route external-2 detail"))

    verdict([("the adjacency is Full", ok_adj),
             ("r1 is the DR, r2 having priority 0", ok_dr),
             ("the guide's policy redistributes nothing", ok_none),
             ("2001:db8:a::/64 enters once permitted", ok_a),
             ("2001:db8:b::/64 stays out", ok_b),
             ("it is type 2, metric 2000", ok_metric)])

    story("""
the network is left up. Things to try:

  the link state database of r1:
    cloonix cli %s ssh r1 "vtysh -c 'show ipv6 ospf6 database'"

  the route-map and what it matched:
    cloonix cli %s ssh r2 "vtysh -c 'show route-map static-ospf6'"
""" % (NET, NET))

main()
