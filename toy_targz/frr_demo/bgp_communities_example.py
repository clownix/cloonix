#!/usr/bin/env python3
r"""bgp_communities_example - Example Configuration (BGP communities).

    ./bgp_communities_example.py [net]     the net defaults to nemo

The example of the FRR User Guide, page bgp, section "Example
Configuration" under "Communities Attribute":
https://docs.frrouting.org/en/latest/bgp.html#bgp-communities-example

    as100 (192.168.0.1) ---- lan0 ---- as7675 (192.168.0.2)

  AS 7675 is the upstream of AS 100. The guide gives three configurations
  of AS 7675 that act on the communities AS 100 puts on its routes.

  ACT 1  local preference chosen by the customer: AS 100 announces
         10.0.0.0/8 with 7675:80, AS 7675 sets its local preference to 80;
         with 7675:90 it becomes 90.
  ACT 2  filtering: AS 7675 accepts only the routes carrying 0:80 and 0:90
         together, or 0:100. 10.0.0.0/8 (0:80 0:90) is accepted,
         172.16.0.0/16 (0:80 alone) is not.
  ACT 3  filtering the other way: the routes with 1:1 are dropped,
         172.16.0.0/16 (1:1) is refused, 10.0.0.0/8 is accepted.

Image: frr_trixie (zebra, staticd, bgpd)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf as7675 eth0 sniff=on
    cloonix cli nemo sniff as7675 eth0 --dump "tcp port 179"

  The UPDATE of ACT 1: the community attribute, type 8, four bytes
  0x1dfb0050 for 7675:80.

Important
  The guide's last snippet ends with "bgp community-list standard FILTER
  permit" and no value; the CLI of frr wants one (AA:NN...), so the same
  policy is written as a deny entry followed by a permit entry of the
  route-map. "route-map RMAP permit in" of the second snippet is read as
  "permit 10".
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
from frrlib import conf, has, lacks, opening, proof, until, verdict, vtysh

HISTORY = """\
The communities attribute was defined in RFC 1997 in 1996 by Ravi
Chandrasekeran, Paul Traina and Tony Li: 32 bits, an AS number and a value,
a tag that a route carries from network to network. It let a customer tell
its provider what to do with its routes, without a phone call. The bgpd of
frr comes from GNU Zebra, written by Kunihiro Ishiguro from 1996.

Today: every transit provider publishes the list of communities its
customers may set, local preference and selective announcement first.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

A100, A7675 = "as100", "as7675"
ALL = (A100, A7675)
L0 = "lan0"

WIRING = {
    A100: [(0, L0, "192.168.0.1/24")],
    A7675: [(0, L0, "192.168.0.2/24")],
}
GATEWAY = {}

AS100_CONF = """\
hostname as100
ip route 10.0.0.0/8 blackhole
ip route 172.16.0.0/16 blackhole
ip prefix-list PLIST permit 10.0.0.0/8
ip prefix-list PLIST2 permit 172.16.0.0/16
route-map OUT1 permit 10
 match ip address prefix-list PLIST
 set community 7675:80
route-map OUT2 permit 10
 match ip address prefix-list PLIST
 set community 0:80 0:90
route-map OUT2 permit 20
 match ip address prefix-list PLIST2
 set community 0:80
route-map OUT3 permit 10
 match ip address prefix-list PLIST
 set community 7675:80
route-map OUT3 permit 20
 match ip address prefix-list PLIST2
 set community 1:1
router bgp 100
 no bgp ebgp-requires-policy
 neighbor 192.168.0.2 remote-as 7675
 address-family ipv4 unicast
  network 10.0.0.0/8
  network 172.16.0.0/16
  neighbor 192.168.0.2 route-map OUT1 out
 exit-address-family"""

AS7675_CONF = """\
hostname as7675
router bgp 7675
 no bgp ebgp-requires-policy
 neighbor 192.168.0.1 remote-as 100
 address-family ipv4 unicast
  neighbor 192.168.0.1 soft-reconfiguration inbound
  neighbor 192.168.0.1 route-map RMAP in
 exit-address-family
bgp community-list 70 permit 7675:70
bgp community-list 80 permit 7675:80
bgp community-list 90 permit 7675:90
route-map RMAP permit 10
 match community 70
 set local-preference 70
route-map RMAP permit 20
 match community 80
 set local-preference 80
route-map RMAP permit 30
 match community 90
 set local-preference 90
bgp community-list 1 permit 0:80 0:90
bgp community-list 1 permit 0:100
route-map RMAP2 permit 10
 match community 1
bgp community-list standard FILTER permit 1:1
route-map RMAP3 deny 10
 match community FILTER
route-map RMAP3 permit 20"""

CONFS = {A100: AS100_CONF, A7675: AS7675_CONF}
NET10, NET172 = "10.0.0.0/8", "172.16.0.0/16"

LAYOUT = {A100: (-300, 0), L0: (0, 0), A7675: (300, 0)}
ETHS = {(A100, "eth0"): 90, (A7675, "eth0"): 270}

CARD = r"""
    as100 ---- lan0 ---- as7675

  AS 100 tags its routes with communities, AS 7675 acts on them:
  local preference, then two kinds of filtering.
"""

def configure(node):
    frr_setup(node, ["staticd", "bgpd"], CONFS[node])

def policy(out_map, in_map):
    """The route-maps of one act, applied by a soft refresh both ways."""
    conf(A100, "router bgp 100", "address-family ipv4 unicast",
         "neighbor 192.168.0.2 route-map %s out" % out_map)
    conf(A7675, "router bgp 7675", "address-family ipv4 unicast",
         "neighbor 192.168.0.1 route-map %s in" % in_map)
    vtysh(A100, "clear bgp ipv4 unicast 192.168.0.2 soft out")
    vtysh(A7675, "clear bgp ipv4 unicast 192.168.0.1 soft in")

def pref(value):
    return has(A7675, "show bgp ipv4 unicast %s" % NET10,
               ["localpref %d" % value])

def main():
    opening()

    step("act 1: the community 7675:80 sets the local preference to 80")
    in_parallel(configure, list(ALL), "frr start")
    got = count_on(A7675, 0, "tcp port 179",
                   lambda: until(lambda: pref(80), 90), settle=1.0)
    ok_80 = bool(got and got["out"]) or pref(80)
    proof("as7675, the route of as100:",
          vtysh(A7675, "show bgp ipv4 unicast %s" % NET10))
    conf(A100, "route-map OUT1 permit 10", "set community 7675:90")
    vtysh(A100, "clear bgp ipv4 unicast 192.168.0.2 soft out")
    ok_90 = until(lambda: pref(90), 60)
    proof("as7675, the same route tagged 7675:90:",
          vtysh(A7675, "show bgp ipv4 unicast %s" % NET10))
    story("    AS 100 decided the preference AS 7675 gives its route.")
    hold()

    step("act 2: only 0:80 and 0:90 together, or 0:100, are accepted")
    policy("OUT2", "RMAP2")
    ok_and = until(lambda: has(A7675, "show bgp ipv4 unicast", [NET10]) and
                   lacks(A7675, "show bgp ipv4 unicast", [NET172]), 60)
    proof("as7675, what it received from as100:",
          vtysh(A7675, "show bgp ipv4 unicast neighbors 192.168.0.1 "
                       "received-routes"))
    proof("as7675, what it accepted:", vtysh(A7675, "show bgp ipv4 unicast"))
    hold()

    step("act 3: the routes tagged 1:1 are dropped")
    policy("OUT3", "RMAP3")
    ok_deny = until(lambda: has(A7675, "show bgp ipv4 unicast", [NET10]) and
                    lacks(A7675, "show bgp ipv4 unicast", [NET172]) and
                    has(A7675, "show bgp ipv4 unicast neighbors 192.168.0.1 "
                        "received-routes", [NET172]), 60)
    proof("as7675, what it accepted:", vtysh(A7675, "show bgp ipv4 unicast"))

    verdict([("7675:80 gives local preference 80", ok_80),
             ("7675:90 gives local preference 90", ok_90),
             ("0:80 0:90 accepted, 0:80 alone refused", ok_and),
             ("1:1 refused, the rest accepted", ok_deny)])

    story("""
the network is left up. Things to try:

  the communities of a route:
    cloonix cli %s ssh as7675 "vtysh -c 'show bgp ipv4 unicast %s'"

  the routes by community:
    cloonix cli %s ssh as7675 "vtysh -c 'show bgp ipv4 unicast community 7675:80'"
""" % (NET, NET10, NET))

main()
