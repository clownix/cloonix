#!/usr/bin/env python3
r"""bgp_misc_examples - Miscellaneous Configuration Examples (BGP).

    ./bgp_misc_examples.py [net]     the net defaults to nemo

The first example of the FRR User Guide, page bgp, section "Miscellaneous
Configuration Examples": "a session to an upstream, advertising only one
prefix to it".
https://docs.frrouting.org/en/latest/bgp.html#bgp-configuration-examples

    me (AS 64512, 10.1.1.2) ---- lan0 ---- isp (AS 64515, 10.1.1.1)
                                           "ACME ISP"

  me puts its upstream in the peer-group "upstream" and filters what it
  sends there with the prefix-list pl-allowed-adv.

  ACT 1  me holds 10.236.87.0/24 and 82.195.133.0/25; the prefix-list lets
         only 82.195.133.0/25 go to the ISP.
  ACT 2  the prefix-list is taken off the peer-group: the ISP gets both.

Image: frr_trixie (zebra, staticd, bgpd)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf isp eth0 sniff=on
    cloonix cli nemo sniff isp eth0 --dump "tcp port 179"

  The UPDATE of ACT 2, carrying 10.236.87.0/24 once the filter is gone.

Important
  As printed, the guide announces 10.236.87.0/24 and allows only
  82.195.133.0/25 out, so nothing would reach the ISP: 82.195.133.0/25 is
  announced too, which gives the "only one prefix" the guide describes.
  The two other examples of the section are not played: the second says
  itself it was never tested and holds 10.123.456.0/24, not an address;
  the third is a 6-Bone session, a network closed in 2006.
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
BGP-4, RFC 1771 in 1995 by Yakov Rekhter and Tony Li, brought CIDR to the
inter-domain routing and with it the prefix as the unit of policy. A
customer network announcing to its provider exactly what it owns, and
nothing else, is the first rule of the trade; a prefix-list on the session
is how it is done. The bgpd of frr descends from GNU Zebra, written by
Kunihiro Ishiguro from 1996.

Today: route leaks, a network announcing what it should not, still cause
outages of a part of the internet several times a year; RFC 7454 lists the
filters every BGP session should have.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

ME, ISP = "me", "isp"
ALL = (ME, ISP)
L0 = "lan0"

WIRING = {
    ME: [(0, L0, "10.1.1.2/24")],
    ISP: [(0, L0, "10.1.1.1/24")],
}
GATEWAY = {}

# The guide's configuration, with 82.195.133.0/25 announced as well.
ME_CONF = """\
hostname me
ip route 10.236.87.0/24 blackhole
ip route 82.195.133.0/25 blackhole
router bgp 64512
 bgp router-id 10.236.87.1
 no bgp ebgp-requires-policy
 neighbor upstream peer-group
 neighbor upstream remote-as 64515
 neighbor upstream capability dynamic
 neighbor 10.1.1.1 peer-group upstream
 neighbor 10.1.1.1 description ACME ISP
 address-family ipv4 unicast
  network 10.236.87.0/24
  network 82.195.133.0/25
  neighbor upstream prefix-list pl-allowed-adv out
 exit-address-family
ip prefix-list pl-allowed-adv seq 5 permit 82.195.133.0/25
ip prefix-list pl-allowed-adv seq 10 deny any"""

ISP_CONF = """\
hostname isp
router bgp 64515
 no bgp ebgp-requires-policy
 neighbor 10.1.1.2 remote-as 64512
 neighbor 10.1.1.2 capability dynamic"""

CONFS = {ME: ME_CONF, ISP: ISP_CONF}
KEPT, FILTERED = "82.195.133.0/25", "10.236.87.0/24"

LAYOUT = {ME: (-300, 0), L0: (0, 0), ISP: (300, 0)}
ETHS = {(ME, "eth0"): 90, (ISP, "eth0"): 270}

CARD = r"""
    me (AS 64512) ---- lan0 ---- isp (AS 64515)

  Two prefixes on me, a prefix-list on the session: one prefix reaches
  the ISP.
"""

def configure(node):
    frr_setup(node, ["staticd", "bgpd"], CONFS[node])

def main():
    opening()

    step("act 1: the prefix-list lets one prefix out of two go upstream")
    in_parallel(configure, list(ALL), "frr start")
    ok_me = until(lambda: has(ME, "show ip bgp", [KEPT, FILTERED]), 60)
    ok_one = until(lambda: has(ISP, "show ip bgp", [KEPT]) and
                   lacks(ISP, "show ip bgp", [FILTERED]), 60)
    proof("me, its own table:", vtysh(ME, "show ip bgp"))
    proof("me, what it sends to ACME ISP:",
          vtysh(ME, "show ip bgp neighbors 10.1.1.1 advertised-routes"))
    proof("isp, what it received:", vtysh(ISP, "show ip bgp"))
    hold()

    step("act 2: the prefix-list is removed, both prefixes go upstream")
    got = count_on(ISP, 0, "tcp port 179", lambda: (
        conf(ME, "router bgp 64512", "address-family ipv4 unicast",
             "no neighbor upstream prefix-list pl-allowed-adv out"),
        until(lambda: has(ISP, "show ip bgp", [KEPT, FILTERED]), 60))[1],
        settle=1.0)
    ok_both = bool(got and got["out"]) or \
        has(ISP, "show ip bgp", [KEPT, FILTERED])
    proof("isp, what it received now:", vtysh(ISP, "show ip bgp"))

    verdict([("me originates the two prefixes", ok_me),
             ("with the filter, only %s upstream" % KEPT, ok_one),
             ("without the filter, both upstream", ok_both)])

    story("""
the network is left up. Things to try:

  the peer-group and its members:
    cloonix cli %s ssh me "vtysh -c 'show bgp peer-group upstream'"

  the dynamic capability negotiated:
    cloonix cli %s ssh me "vtysh -c 'show bgp neighbors 10.1.1.1'"
""" % (NET, NET))

main()
