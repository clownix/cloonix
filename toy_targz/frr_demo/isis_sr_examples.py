#!/usr/bin/env python3
r"""isis_sr_examples - Segment Routing over MPLS with IS-IS.

Needs: mpls_router, mpls_iptunnel.

    ./isis_sr_examples.py [net]     the net defaults to nemo

The example of the FRR User Guide, page isisd, section "ISIS Configuration
Examples": "A Segment Routing configuration, with IPv4, IPv6, SRGB and MSD
configuration", on four routers in a square. No LDP: IS-IS itself carries
the labels.
https://docs.frrouting.org/en/latest/isisd.html#isis-config-examples

                 r2
               /    \
             r1      r4          r1-r2, r2-r4: metric 10
               \    /            r1-r3, r3-r4: metric 20
                 r3

  !!! KERNEL MODULES NEEDED ON THE HOST: mpls_router and mpls_iptunnel !!!
  Label switching is done by the kernel, and a container cannot load a
  module. Before playing this demo, on the host, once per boot:

sudo modprobe mpls_router
sudo modprobe mpls_iptunnel

  or do not run this demo. It stops with a big warning if a module is
  missing.

  Each router announces a prefix SID for its loopback, IPv4 and IPv6,
  with explicit-null as in the guide: rN has 10.1.1.N index N00 and
  2001:db8:1000::N index N01. The label of a SID is the start of the
  SRGB, 16000, plus its index: r4 is 16400 everywhere.

  ACT 1  the adjacencies come up; each router knows the SRGB of the three
         others and has a label for every loopback.
  ACT 2  r1 reaches r4 over r2: 16400 on the first link, label 0 (IPv4
         explicit-null) on the last, IPv6 alike with 16401 and label 2.
  ACT 3  the link r1-r2 is cut: the traffic goes over r3 with the same
         label 16400. A SID names a router, not a path.

Image: frr_trixie (zebra, isisd)
Size:  4 containers, 4 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r4 eth0 sniff=on
    cloonix cli nemo sniff r4 eth0 --dump "mpls 0"

  An echo of r1 towards 10.1.1.4 arriving at r4 with the label 0 above
  the IP header: r4 asked its neighbours for explicit-null.

Important
  The guide makes eth0 point-to-point and leaves eth1 broadcast; here every
  link is point-to-point. The kernel MPLS forwarding is turned on in each
  container: net.mpls.platform_labels and net.mpls.conf.<eth>.input.
"""
import os
import sys
import time

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, frr_setup, hold, in_parallel, reached, \
    ssh, step, story
from frrlib import forwarding, has, mpls_on, opening, proof, until, verdict, \
    vtysh

HISTORY = """\
Segment routing was started at Cisco in 2012 by Clarence Filsfils and
became the SPRING work of the IETF: RFC 8402 in 2018 for the architecture,
RFC 8667 in 2019 for its IS-IS extensions. The source puts the path in the
packet as a list of segments; on MPLS a segment is a label, and the labels
are carried by the IGP itself, so LDP and RSVP-TE are no longer needed.
The segment routing of isisd was written by Olivier Dugeon at Orange and
Renato Westphal in 2019 and 2020.

Today: segment routing over MPLS is what new carrier backbones are built
with, and what many old ones migrate to, LDP alongside during the move.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

R1, R2, R3, R4 = "r1", "r2", "r3", "r4"
ALL = (R1, R2, R3, R4)
L12, L13, L24, L34 = "lan12", "lan13", "lan24", "lan34"

WIRING = {
    R1: [(0, L12, "10.0.12.1/24"), (1, L13, "10.0.13.1/24")],
    R2: [(0, L12, "10.0.12.2/24"), (1, L24, "10.0.24.2/24")],
    R3: [(0, L13, "10.0.13.3/24"), (1, L34, "10.0.34.3/24")],
    R4: [(0, L24, "10.0.24.4/24"), (1, L34, "10.0.34.4/24")],
}
GATEWAY = {}
NUM = {R1: 1, R2: 2, R3: 3, R4: 4}
LONG = (L13, L34)

def lo4(node):
    return "10.1.1.%d" % NUM[node]

def lo6(node):
    return "2001:db8:1000::%d" % NUM[node]

def frr_conf(node):
    n = NUM[node]
    c = ["hostname %s" % node,
         "interface lo",
         " ip address %s/32" % lo4(node),
         " ipv6 address %s/128" % lo6(node),
         " ip router isis SR",
         " ipv6 router isis SR",
         " isis passive"]
    for num, lan, _a in WIRING[node]:
        c += ["interface eth%d" % num,
              " ip router isis SR",
              " ipv6 router isis SR",
              " isis network point-to-point",
              " isis hello-interval 1"]
        if lan in LONG:
            c.append(" isis metric 20")
    c += ["router isis SR",
          " net 49.0000.0000.0000.000%d.00" % n,
          " is-type level-1",
          " topology ipv6-unicast",
          " lsp-gen-interval 2",
          " segment-routing on",
          " segment-routing node-msd 8",
          " segment-routing prefix %s/32 index %d00 explicit-null"
          % (lo4(node), n),
          " segment-routing prefix %s/128 index %d01 explicit-null"
          % (lo6(node), n)]
    return "\n".join(c)

LAYOUT = {
    R1: (-300, 0), L12: (-150, -110), R2: (0, -200), L24: (150, -110),
    R4: (300, 0), L13: (-150, 110), R3: (0, 200), L34: (150, 110),
}
ETHS = {
    (R1, "eth0"): 45, (R1, "eth1"): 135, (R2, "eth0"): 225, (R2, "eth1"): 135,
    (R3, "eth0"): 315, (R3, "eth1"): 45, (R4, "eth0"): 315, (R4, "eth1"): 225,
}

CARD = r"""
            r2
          /    \
        r1      r4      (IS-IS, segment routing, no LDP)
          \    /
            r3

  r4 is the label 16400 for every router, whatever the way.
"""

def configure(node):
    forwarding(node)
    mpls_on(node, [num for num, _l, _a in WIRING[node]])
    frr_setup(node, ["isisd"], frr_conf(node))

def via(addr):
    return lambda: has(R1, "show ip route %s/32" % lo4(R4),
                       [addr, "label 16400"])

def ping_from(node, dst, v6=False):
    return ssh(node, "ping %s-c 5 -W 1 -I %s %s"
               % ("-6 " if v6 else "", lo6(node) if v6 else lo4(node), dst),
               check=False)

def count(node, num, filt, v6=False):
    """Packets matching filt on node eth<num> while r1 pings r4."""
    got = count_on(node, num, filt,
                   lambda: ping_from(R1, lo6(R4) if v6 else lo4(R4), v6),
                   settle=1.0)
    if got is None:
        return None, ""
    story("    %d packet(s) \"%s\" on %s eth%d during the ping."
          % (got["count"], filt, node, num))
    return got["count"] > 0, got["out"]

def main():
    opening("mpls_router", "mpls_iptunnel")

    step("act 1: IS-IS comes up and hands out the prefix SIDs")
    in_parallel(configure, list(ALL), "frr start")
    ok_nodes = until(lambda: has(R1, "show isis segment-routing node", [
        "0000.0000.0002", "0000.0000.0003", "0000.0000.0004"]), 90)
    proof("r1, the SR nodes it knows, their SRGB:",
          vtysh(R1, "show isis segment-routing node"))
    ok_lfib = until(lambda: has(R2, "show mpls table",
                                ["16100", "16300", "16400", "16401"]), 60)
    proof("r2, its label table, one entry per SID:",
          vtysh(R2, "show mpls table"))
    hold()

    step("act 2: r1 reaches r4 over r2 with the label 16400")
    ok_path = until(via("10.0.12.2"), 60)
    proof("r1, its route to the loopback of r4:",
          vtysh(R1, "show ip route %s/32" % lo4(R4)))
    kernel = ssh(R1, "ip route show %s" % lo4(R4), check=False)
    proof("r1, the kernel route, with its MPLS encapsulation:", kernel)
    ok_encap = "encap mpls" in kernel and "16400" in kernel
    ok_first, out = count(R2, 0, "mpls 16400")
    ok_last, _o = count(R4, 0, "mpls 0")
    proof("r1 -> r4:", out or ping_from(R1, lo4(R4)))
    ok_ping = reached(out or ping_from(R1, lo4(R4)))
    ok_v6route = until(lambda: has(R1, "show ipv6 route %s/128" % lo6(R4),
                                   ["label 16401"]), 30)
    ok_last6, out6 = count(R4, 0, "mpls 2", v6=True)
    proof("r1 -> r4 in IPv6:", out6 or ping_from(R1, lo6(R4), True))
    ok_ping6 = reached(out6 or ping_from(R1, lo6(R4), True))
    hold()

    step("act 3: the link r1-r2 is cut, the same label goes over r3")
    ssh(R1, "ip link set eth0 down")
    ok_moved = until(via("10.0.13.3"), 60)
    proof("r1, its route to r4 now:",
          vtysh(R1, "show ip route %s/32" % lo4(R4)))
    # r1 saw its link go down at once; r2 keeps its way to r1 over the cut
    # link, and r4 its way back through r2, until their adjacencies time out
    ok_ping3 = until(lambda: reached(ping_from(R1, lo4(R4))), 90)
    ok_other, out = count(R3, 0, "mpls 16400")
    ok_last3, _o = count(R4, 1, "mpls 0")
    ssh(R1, "ip link set eth0 up")
    ssh(R1, "ip addr add 10.0.12.1/24 dev eth0", check=False)
    time.sleep(1)

    verdict([("r1 knows the SRGB of r2, r3 and r4", ok_nodes),
             ("r2 has a label per SID, 16100 to 16401", ok_lfib),
             ("r1 reaches r4 over r2 with the label 16400", ok_path),
             ("the kernel route pushes 16400", ok_encap),
             ("16400 crosses r1-r2", ok_first),
             ("explicit-null, label 0, on the last link", ok_last),
             ("r1 reaches r4", ok_ping),
             ("the IPv6 route carries 16401", ok_v6route),
             ("explicit-null, label 2, in IPv6", ok_last6),
             ("r1 reaches r4 in IPv6", ok_ping6),
             ("after the cut, the route goes over r3, label 16400", ok_moved),
             ("16400 crosses r1-r3", ok_other),
             ("label 0 on the link r3-r4", ok_last3),
             ("r1 still reaches r4", ok_ping3)])

    story("""
the network is left up, the link r1-r2 restored. Things to try:

  the prefix SIDs IS-IS carries, in the database of r4:
    cloonix cli %s ssh r1 "vtysh -c 'show isis database detail r4.00-00'"

  the label forwarding table of the kernel of r2:
    cloonix cli %s ssh r2 "ip -f mpls route show"

  the explicit-null labels arriving at r4:
    cloonix cli %s sniff r4 eth0 --dump "mpls"
""" % (NET, NET, NET))

main()
