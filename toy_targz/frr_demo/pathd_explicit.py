#!/usr/bin/env python3
r"""pathd_explicit - SR-TE, an explicit segment list chosen at the source.

Needs: mpls_router, mpls_iptunnel.

    ./pathd_explicit.py [net]     the net defaults to nemo

The example of the FRR User Guide, page pathd, section "Explicit Segment
Lists": a segment list SL1, a policy of color 1 with the binding SID 1111
and a candidate path CP1 using SL1; BGP routes marked with the color 1 are
steered into it, as in the "Pathd Configuration" example of the same page.
https://docs.frrouting.org/en/latest/pathd.html#explicit-segment-lists

              r2                       short: r1 r2 r4
            /    \
    h1 -- r1      r4 -- h4
            \    /                     long:  r1 r5 r3 r4
             r5 - r3

  !!! KERNEL MODULES NEEDED ON THE HOST: mpls_router and mpls_iptunnel !!!
  Label switching is done by the kernel, and a container cannot load a
  module. Before playing this demo, on the host, once per boot:

sudo modprobe mpls_router
sudo modprobe mpls_iptunnel

  or do not run this demo. It stops with a big warning if a module is
  missing.

  IS-IS with segment routing in the core: rN has the SID 1600N. iBGP
  between r1 and r4 carries the networks of h1 and h4; r1 colors what it
  learns from r4 with the color 1.

  ACT 1  no policy yet: h1 reaches h4 by the shortest way, over r2, with
         the label 16004.
  ACT 2  r1 sets the policy of color 1 to r4, SL1 = 16003 then 16004: the
         traffic goes r1 r5 r3 r4. Leaving r1, a packet carries the two
         labels, the route it must follow; r5 and r3 only pop them.
  ACT 3  the candidate path is removed: the traffic is back over r2.

Image: frr_trixie (zebra, isisd, bgpd, pathd)
Size:  7 containers, 7 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r5 eth0 sniff=on
    cloonix cli nemo sniff r5 eth0 --dump "mpls"

  An echo of h1 in ACT 2, with a stack of two labels: 16003 above 16004
  above the IP header. Nobody in the core decided this way; r1 wrote it.

Important
  The kernel MPLS forwarding is turned on in each router: platform_labels
  and net.mpls.conf.<eth>.input.
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
from frrlib import conf, forwarding, has, mpls_on, opening, ping, proof, \
    until, verdict, vtysh

HISTORY = """\
Traffic engineering, sending traffic where the shortest path would not,
came to MPLS with RSVP-TE, RFC 3209 in 2001: every router along a tunnel
kept its state. Segment routing moved the path into the packet; the SR
policy, an endpoint, a color and candidate paths, is RFC 9256 of 2022 by
Clarence Filsfils and colleagues. pathd, the SR policy daemon of frr, was
written at NetDEF and Volta Networks in 2020.

Today: SR policies steer the traffic of operators by color, a low latency
color, a cheap one, a disjoint one, the routes of BGP choosing among them.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

H1, H4 = "h1", "h4"
R1, R2, R3, R4, R5 = "r1", "r2", "r3", "r4", "r5"
ROUTERS = (R1, R2, R3, R4, R5)
ALL = ROUTERS + (H1, H4)
L12, L15, L24, L35, L34 = "lan12", "lan15", "lan24", "lan35", "lan34"
LH1, LH4 = "lanh1", "lanh4"

WIRING = {
    R1: [(0, L12, "10.0.12.1/24"), (1, L15, "10.0.15.1/24"),
         (2, LH1, "10.10.1.1/24")],
    R2: [(0, L12, "10.0.12.2/24"), (1, L24, "10.0.24.2/24")],
    R5: [(0, L15, "10.0.15.5/24"), (1, L35, "10.0.35.5/24")],
    R3: [(0, L35, "10.0.35.3/24"), (1, L34, "10.0.34.3/24")],
    R4: [(0, L24, "10.0.24.4/24"), (1, L34, "10.0.34.4/24"),
         (2, LH4, "10.10.4.1/24")],
    H1: [(0, LH1, "10.10.1.10/24")],
    H4: [(0, LH4, "10.10.4.10/24")],
}
GATEWAY = {H1: "10.10.1.1", H4: "10.10.4.1"}
NUM = {R1: 1, R2: 2, R3: 3, R4: 4, R5: 5}
EDGE = {R1: 2, R4: 2}
PEER = {R1: R4, R4: R1}

def lo(node):
    return "%d.%d.%d.%d" % ((NUM[node],) * 4)

def frr_conf(node):
    n = NUM[node]
    c = ["hostname %s" % node,
         "interface lo",
         " ip address %s/32" % lo(node),
         " ip router isis SR",
         " isis passive"]
    for num, _lan, _a in WIRING[node]:
        if EDGE.get(node) == num:
            continue
        c += ["interface eth%d" % num,
              " ip router isis SR",
              " isis network point-to-point",
              " isis hello-interval 1"]
    c += ["router isis SR",
          " net 49.0000.0000.0000.000%d.00" % n,
          " is-type level-1",
          " lsp-gen-interval 1",
          " segment-routing on",
          " segment-routing node-msd 8",
          " segment-routing prefix %s/32 index %d" % (lo(node), n)]
    if node in PEER:
        peer = lo(PEER[node])
        c += ["router bgp 65000",
              " bgp router-id %s" % lo(node),
              " neighbor %s remote-as 65000" % peer,
              " neighbor %s update-source lo" % peer,
              " address-family ipv4 unicast",
              "  network 10.10.%d.0/24" % n]
        if node == R1:
            c += ["  neighbor %s route-map SET_SR_POLICY in" % peer]
        c += [" exit-address-family"]
    if node == R1:
        c += ["route-map SET_SR_POLICY permit 10",
              " set sr-te color 1",
              "segment-routing",
              " traffic-eng",
              "  segment-list SL1",
              "   index 10 mpls label 16003",
              "   index 20 mpls label 16004"]
    return "\n".join(c)

LAYOUT = {
    H1: (-560, 0), LH1: (-440, 0), R1: (-300, 0),
    L12: (-150, -110), R2: (0, -200), L24: (150, -110),
    R4: (300, 0), LH4: (440, 0), H4: (560, 0),
    L15: (-220, 130), R5: (-110, 220), L35: (0, 220), R3: (110, 220),
    L34: (220, 130),
}
ETHS = {
    (H1, "eth0"): 90, (R1, "eth2"): 270, (R1, "eth0"): 45, (R1, "eth1"): 135,
    (R2, "eth0"): 225, (R2, "eth1"): 135, (R4, "eth0"): 315,
    (R4, "eth1"): 225, (R4, "eth2"): 90, (H4, "eth0"): 270,
    (R5, "eth0"): 315, (R5, "eth1"): 90, (R3, "eth0"): 270, (R3, "eth1"): 45,
}

CARD = r"""
              r2
            /    \
    h1 -- r1      r4 -- h4      (IS-IS SR, iBGP r1-r4, pathd on r1)
            \    /
             r5 - r3

  The shortest way is over r2; the policy of r1 sends h4's traffic over
  r5 and r3, by two labels pushed at r1.
"""

POLICY = ("segment-routing", "traffic-eng",
          "policy color 1 endpoint 4.4.4.4", "name example1",
          "binding-sid 1111")
CANDIDATE = "candidate-path preference 100 name CP1 explicit segment-list SL1"

DAEMONS = {R1: ["isisd", "bgpd", "pathd"], R4: ["isisd", "bgpd"]}

def configure(node):
    forwarding(node)
    mpls_on(node, [num for num, _l, _a in WIRING[node]
                   if EDGE.get(node) != num])
    frr_setup(node, DAEMONS.get(node, ["isisd"]), frr_conf(node))

def route_via(addr, labels="16004"):
    """r1 sends to h4 through addr, pushing labels (a stack shows as
    16003/16004)."""
    return lambda: has(R1, "show ip route 10.10.4.0/24",
                       [addr, "label " + labels])

def count(node, num, filt):
    got = count_on(node, num, filt, lambda: ping(H1, "10.10.4.10"),
                   settle=1.0)
    if got is None:
        return False, ping(H1, "10.10.4.10")
    story("    %d packet(s) \"%s\" on %s eth%d during the ping."
          % (got["count"], filt, node, num))
    return got["count"] > 0, got["out"]

def main():
    opening("mpls_router", "mpls_iptunnel")

    step("act 1: no policy yet, h1 reaches h4 over r2")
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_bgp = until(lambda: has(R1, "show ip route 10.10.4.0/24", ["bgp"]),
                   120)
    ok_short = until(route_via("10.0.12.2"), 60)
    proof("r1, its route to h4, colored 1, no policy of that color:",
          vtysh(R1, "show ip route 10.10.4.0/24"))
    ok_r2, out = count(R2, 0, "mpls 16004")
    proof("h1 -> h4:", out)
    ok_ping1 = reached(out)
    hold()

    step("act 2: r1 sets the policy, SL1 = 16003 16004")
    conf(R1, *(POLICY + (CANDIDATE,)))
    ok_active = until(lambda: has(R1, "show sr-te policy", ["Active"]), 30)
    proof("r1, its SR policies:", vtysh(R1, "show sr-te policy detail"))
    ok_long = until(route_via("10.0.15.5", "16003/16004"), 30)
    proof("r1, its route to h4, now through the policy:",
          vtysh(R1, "show ip route 10.10.4.0/24"))
    ok_stack, out = count(R5, 0, "mpls 16003 and mpls 16004")
    ok_r3, _o = count(R3, 0, "mpls 16004")
    proof("h1 -> h4, the long way:", out)
    ok_ping2 = reached(out)
    ok_bsid = has(R1, "show mpls table", ["1111"])
    hold()

    step("act 3: the candidate path goes, the traffic is back over r2")
    conf(R1, *(POLICY[:3] + ("no candidate-path preference 100",)))
    ok_back = until(route_via("10.0.12.2"), 30)
    proof("r1, its route to h4:", vtysh(R1, "show ip route 10.10.4.0/24"))
    ok_ping3 = reached(ping(H1, "10.10.4.10"))

    verdict([("iBGP gives r1 the network of h4", ok_bgp),
             ("without a policy, the route goes over r2, label 16004",
              ok_short),
             ("16004 crosses r1-r2", ok_r2),
             ("h1 reaches h4", ok_ping1),
             ("the policy of color 1 to 4.4.4.4 is Active", ok_active),
             ("the route of h4 now leaves r1 towards r5", ok_long),
             ("two labels, 16003 above 16004, on r1-r5", ok_stack),
             ("one label, 16004, on r5-r3", ok_r3),
             ("h1 reaches h4 the long way", ok_ping2),
             ("the binding SID 1111 is in the label table", ok_bsid),
             ("without the candidate path, back over r2", ok_back),
             ("h1 reaches h4", ok_ping3)])

    story("""
the network is left up, the policy without candidate path. Things worth
trying:

  the policy back:
    cloonix cli %s ssh r1 "vtysh -c 'conf t' -c 'segment-routing' -c 'traffic-eng' -c 'policy color 1 endpoint 4.4.4.4' -c '%s'"

  the label stack pushed by the kernel of r1:
    cloonix cli %s ssh r1 "ip route show 10.10.4.0/24"

  the two labels on the wire:
    cloonix cli %s sniff r5 eth0 --dump "mpls"
""" % (NET, CANDIDATE, NET, NET))

main()
