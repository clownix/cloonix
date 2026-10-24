#!/usr/bin/env python3
r"""ospf_sample - Sample Configuration (OSPF).

    ./ospf_sample.py [net]     the net defaults to nemo

The first two examples of the FRR User Guide, page ospfd, section "Sample
Configuration": "A simple example, with MD5 authentication enabled" and
"An ABR router, with MD5 authentication and performing summarisation of
networks between the areas".
https://docs.frrouting.org/en/latest/ospfd.html#sample-configuration

                 area 0                        area 0.0.0.1
    r0 ---- lan0 192.168.0.0/24 ---- abr ---- lan1 192.168.1.0/24 ---- r1
    MD5 key 1                         |        MD5 key 2                |
                                    lan5 10.0.5.0/24              lan21 10.2.1.0/24
                                      |      (passive)            + 10.2.2.0/24
                                      h0                                h1

  abr is the ABR of the guide: eth0 for its eth0, eth1 for its br0, eth2
  for its ppp0 (passive), the same networks, keys, areas and ranges.
  r1 is the simple example, in area 0.0.0.1.

  ACT 1  the two MD5 adjacencies come up.
  ACT 2  the summaries: r0 learns 10.2.0.0/16 and not the two /24 of area
         1 (area 0.0.0.1 range 10.2.0.0/16); r1 learns 10.0.0.0/16 (area
         0.0.0.0 range 10.0.0.0/16). h0 reaches h1 across the areas.
  ACT 3  r1 is given another key: abr refuses its hellos, the adjacency
         falls.

Image: frr_trixie (zebra, ospfd)
Size:  5 containers, 4 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf abr eth1 sniff=on
    cloonix cli nemo sniff abr eth1 --dump "ip proto 89"

  A hello of ACT 3: authentication type 2, key id 2, the 16-byte MD5
  digest at the end, which abr finds wrong.

Important
  The guide's "password", "log file" and "service advanced-vty" lines,
  which are about the vty, are left out; the hellos are sent every second
  so that a lost adjacency is seen in 4 seconds rather than 40.
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
OSPF was designed at the IETF by John Moy, RFC 1131 in 1989, OSPFv2 in RFC
2328: link states flooded inside an area, summaries between areas through
the area border routers, which is what keeps a large network computable.
The MD5 authentication is in RFC 2328 itself. The ospfd of frr was written
by Toshiaki Takada and Kunihiro Ishiguro for GNU Zebra.

Today: OSPF is the interior protocol of most enterprise networks.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

R0, ABR, R1, H0, H1 = "r0", "abr", "r1", "h0", "h1"
ALL = (ABR, R0, R1, H0, H1)
ROUTERS = (ABR, R0, R1)
L0, L1, L5, L21 = "lan0", "lan1", "lan5", "lan21"

WIRING = {
    R0: [(0, L0, "192.168.0.2/24")],
    ABR: [(0, L0, "192.168.0.1/24"), (1, L1, "192.168.1.1/24"),
          (2, L5, "10.0.5.1/24")],
    R1: [(0, L1, "192.168.1.2/24"), (1, L21, "10.2.1.1/24")],
    H0: [(0, L5, "10.0.5.10/24")],
    H1: [(0, L21, "10.2.1.10/24")],
}
GATEWAY = {H0: "10.0.5.1", H1: "10.2.1.1"}

# The guide's ABR: bge0 -> eth0, br0 -> eth1, ppp0 -> eth2.
ABR_CONF = """\
hostname abr
interface eth0
 ip ospf authentication message-digest
 ip ospf message-digest-key 1 md5 ABCDEFGHIJK
 ip ospf hello-interval 1
 ip ospf dead-interval 4
interface eth2
 ip ospf passive
interface eth1
 ip ospf authentication message-digest
 ip ospf message-digest-key 2 md5 XYZ12345
 ip ospf hello-interval 1
 ip ospf dead-interval 4
router ospf
 ospf router-id 192.168.0.1
 redistribute connected
 network 192.168.0.0/24 area 0.0.0.0
 network 10.0.0.0/16 area 0.0.0.0
 network 192.168.1.0/24 area 0.0.0.1
 area 0.0.0.0 authentication message-digest
 area 0.0.0.0 range 10.0.0.0/16
 area 0.0.0.0 range 192.168.0.0/24
 area 0.0.0.1 authentication message-digest
 area 0.0.0.1 range 10.2.0.0/16"""

R0_CONF = """\
hostname r0
interface eth0
 ip ospf authentication message-digest
 ip ospf message-digest-key 1 md5 ABCDEFGHIJK
 ip ospf hello-interval 1
 ip ospf dead-interval 4
router ospf
 ospf router-id 192.168.0.2
 network 192.168.0.0/24 area 0.0.0.0
 area 0.0.0.0 authentication message-digest"""

# The guide's simple example, with the key of the br0 of the ABR.
R1_CONF = """\
hostname r1
interface eth0
 ip ospf authentication message-digest
 ip ospf message-digest-key 2 md5 XYZ12345
 ip ospf hello-interval 1
 ip ospf dead-interval 4
router ospf
 ospf router-id 192.168.1.2
 network 192.168.0.0/16 area 0.0.0.1
 network 10.2.0.0/16 area 0.0.0.1
 area 0.0.0.1 authentication message-digest"""

CONFS = {ABR: ABR_CONF, R0: R0_CONF, R1: R1_CONF}

LAYOUT = {
    R0: (-480, -80), L0: (-260, -80), ABR: (-40, -80), L1: (180, -80),
    R1: (400, -80), L5: (-40, 100), H0: (-40, 240), L21: (400, 100),
    H1: (400, 240),
}
ETHS = {
    (R0, "eth0"): 90, (ABR, "eth0"): 270, (ABR, "eth1"): 90,
    (ABR, "eth2"): 180, (R1, "eth0"): 270, (R1, "eth1"): 180,
    (H0, "eth0"): 0, (H1, "eth0"): 0,
}

CARD = r"""
    r0 ---- abr ---- r1         area 0 | area 0.0.0.1
             |        |
             h0       h1

  An ABR summarising each area into the other, MD5 on both sides.
"""

def configure(node):
    forwarding(node)
    frr_setup(node, ["ospfd"], CONFS[node])

def full(node, count):
    return vtysh(node, "show ip ospf neighbor").count("Full") >= count

def main():
    opening()
    ssh(R1, "ip addr add 10.2.2.1/24 dev eth1")

    step("act 1: the two MD5 adjacencies come up")
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_adj = until(lambda: full(ABR, 2), 90)
    proof("abr, its neighbours:", vtysh(ABR, "show ip ospf neighbor"))
    hold()

    step("act 2: each area is summarised into the other")
    ok_sum0 = until(lambda: has(R0, "show ip route ospf", ["10.2.0.0/16"]) and
                    lacks(R0, "show ip route ospf",
                          ["10.2.1.0/24", "10.2.2.0/24"]), 60)
    ok_sum1 = until(lambda: has(R1, "show ip route ospf", ["10.0.0.0/16"]),
                    60)
    proof("r0, its OSPF routes: one /16 for area 0.0.0.1:",
          vtysh(R0, "show ip route ospf"))
    proof("r1, its OSPF routes:", vtysh(R1, "show ip route ospf"))
    until(lambda: pings(H0, "10.2.1.10"), 30)
    r = ping(H0, "10.2.1.10")
    proof("h0 -> h1, across the areas:", r)
    ok_ping = reached(r)
    hold()

    step("act 3: r1 is given another key, abr refuses its hellos")
    got = count_on(ABR, 1, "ip proto 89", lambda: (
        conf(R1, "interface eth0", "no ip ospf message-digest-key 2",
             "ip ospf message-digest-key 2 md5 WRONGKEY"),
        until(lambda: not full(ABR, 2), 30, period=1))[1], settle=1.0)
    ok_fell = got["out"] if got else not full(ABR, 2)
    if got is not None:
        story("    %d OSPF packet(s) on abr eth1 meanwhile." % got["count"])
    proof("abr, its neighbours:", vtysh(ABR, "show ip ospf neighbor"))
    ok_r0 = full(ABR, 1)

    verdict([("two MD5 adjacencies on abr", ok_adj),
             ("r0 sees area 0.0.0.1 as 10.2.0.0/16 only", ok_sum0),
             ("r1 sees 10.0.0.0/16", ok_sum1),
             ("h0 reaches h1 across the areas", ok_ping),
             ("a wrong key drops the adjacency with r1", ok_fell),
             ("the adjacency with r0 stays", ok_r0)])

    story("""
the network is left up. Things to try:

  the summary LSAs abr originates:
    cloonix cli %s ssh abr "vtysh -c 'show ip ospf database summary self-originate'"

  the authentication of an interface:
    cloonix cli %s ssh abr "vtysh -c 'show ip ospf interface eth1'"
""" % (NET, NET))

main()
