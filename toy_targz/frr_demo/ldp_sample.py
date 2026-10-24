#!/usr/bin/env python3
r"""ldp_sample - Sample configuration (LDP) (needs: mpls_router, mpls_iptunnel).

    ./ldp_sample.py [net]     the net defaults to nemo

The example of the FRR User Guide, page ldpd, section "Sample
configuration": a device of an MPLS backbone, LDP on two interfaces,
peering with 1.1.1.1 and 3.3.3.3 with a password, OSPF underneath.
https://docs.frrouting.org/en/latest/ldpd.html#sample-configuration

    r1 (1.1.1.1) ---- lan12 ---- r2 (2.2.2.2) ---- lan23 ---- r3 (3.3.3.3)

  !!! KERNEL MODULES NEEDED ON THE HOST: mpls_router and mpls_iptunnel !!!
  Label switching is done by the kernel, and a container cannot load a
  module. Before playing this demo, on the host, once per boot:

sudo modprobe mpls_router
sudo modprobe mpls_iptunnel

  or do not run this demo. It stops with a big warning if a module is
  missing.

  r2 is the router of the guide (its eth1 and eth3 are eth0 and eth1
  here); r1 and r3 are its two neighbours, configured the same way.

  ACT 1  OSPF converges, the LDP sessions come up (password test).
  ACT 2  the routes to the loopbacks carry labels: r1 pushes a label to
         reach 3.3.3.3, r2 swaps or pops it; r1 pings 3.3.3.3 over the LSP.

Image: frr_trixie (zebra, ospfd, ldpd)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r2 eth0 sniff=on
    cloonix cli nemo sniff r2 eth0 --dump "mpls"

  An echo of r1 towards 3.3.3.3, ethertype 0x8847, a label stack of one
  entry above the IP header: r2 forwards it by the label, not by the
  address.

Important
  The kernel MPLS forwarding is turned on in each container:
  net.mpls.platform_labels and net.mpls.conf.<eth>.input.
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
from frrlib import forwarding, has, mpls_on, opening, proof, until, verdict, \
    vtysh

HISTORY = """\
MPLS grew out of the tag switching of Cisco and the IP switching of
Ipsilon in 1996; LDP, RFC 3036 in 2001 by Loa Andersson and colleagues,
now RFC 5036, hands out the labels hop by hop along the routes of the IGP.
The ldpd of frr is the ldpd of OpenBSD, written by Michele Marchetto and
Claudio Jeker, ported to frr by Renato Westphal in 2016.

Today: most carrier backbones still forward on labels, and LDP is how
many of them distribute those labels.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

R1, R2, R3 = "r1", "r2", "r3"
ALL = (R1, R2, R3)
L12, L23 = "lan12", "lan23"

WIRING = {
    R1: [(0, L12, "10.0.12.1/24")],
    R2: [(0, L12, "10.0.12.2/24"), (1, L23, "10.0.23.2/24")],
    R3: [(0, L23, "10.0.23.3/24")],
}
GATEWAY = {}
RID = {R1: "1.1.1.1", R2: "2.2.2.2", R3: "3.3.3.3"}
NBRS = {R1: ("2.2.2.2",), R2: ("1.1.1.1", "3.3.3.3"), R3: ("2.2.2.2",)}

def frr_conf(node):
    rid = RID[node]
    c = ["hostname %s" % node,
         "interface lo",
         " ip address %s/32" % rid,
         "mpls ldp",
         " router-id %s" % rid]
    c += [" neighbor %s password test" % n for n in NBRS[node]]
    c += [" address-family ipv4",
          "  discovery transport-address %s" % rid]
    c += ["  interface eth%d" % num for num, _l, _a in WIRING[node]]
    c += [" exit-address-family",
          "router ospf",
          " ospf router-id %s" % rid,
          " network 0.0.0.0/0 area 0"]
    return "\n".join(c)

LAYOUT = {
    R1: (-440, 0), L12: (-220, 0), R2: (0, 0), L23: (220, 0), R3: (440, 0),
}
ETHS = {
    (R1, "eth0"): 90, (R2, "eth0"): 270, (R2, "eth1"): 90, (R3, "eth0"): 270,
}

CARD = r"""
    r1 ---- r2 ---- r3      (OSPF + LDP)

  Labels for every loopback, handed out hop by hop.
"""

def configure(node):
    forwarding(node)
    mpls_on(node, [num for num, _l, _a in WIRING[node]])
    frr_setup(node, ["ospfd", "ldpd"], frr_conf(node))

def operational(node, count):
    return vtysh(node, "show mpls ldp neighbor").count("OPERATIONAL") >= count

def main():
    opening("mpls_router", "mpls_iptunnel")

    step("act 1: OSPF converges, the LDP sessions come up")
    in_parallel(configure, list(ALL), "frr start")
    ok_ospf = until(lambda: has(R1, "show ip route ospf", ["3.3.3.3/32"]), 90)
    ok_ldp = until(lambda: operational(R2, 2) and operational(R1, 1), 90)
    proof("r2, its LDP neighbours:", vtysh(R2, "show mpls ldp neighbor"))
    hold()

    step("act 2: the loopbacks are reached over labels")
    ok_label = until(lambda: has(R1, "show ip route 3.3.3.3", ["label"]), 60)
    proof("r1, its route to 3.3.3.3:", vtysh(R1, "show ip route 3.3.3.3"))
    proof("r2, its label table:", vtysh(R2, "show mpls table"))
    kernel = ssh(R1, "ip route show 3.3.3.3", check=False)
    proof("r1, the kernel route, with its MPLS encapsulation:", kernel)
    ok_encap = "encap mpls" in kernel
    got = count_on(R2, 0, "mpls", lambda: ssh(
        R1, "ping -c 5 -W 1 -I 1.1.1.1 3.3.3.3", check=False), settle=1.0)
    out = got["out"] if got else ssh(R1, "ping -c 5 -W 1 -I 1.1.1.1 3.3.3.3",
                                     check=False)
    proof("r1 -> 3.3.3.3:", out)
    ok_ping = reached(out)
    ok_mpls = bool(got and got["count"] > 0)
    if got is not None:
        story("    %d MPLS packet(s) on r2 eth0 during the ping." % got["count"])

    verdict([("OSPF gives r1 the loopback of r3", ok_ospf),
             ("the LDP sessions are OPERATIONAL", ok_ldp),
             ("r1's route to 3.3.3.3 carries a label", ok_label),
             ("the kernel route is MPLS-encapsulated", ok_encap),
             ("r1 reaches 3.3.3.3", ok_ping),
             ("the packets cross r2 as MPLS", ok_mpls)])

    story("""
the network is left up. Things to try:

  the labels LDP bound to each prefix:
    cloonix cli %s ssh r2 "vtysh -c 'show mpls ldp binding'"

  the label forwarding table in the kernel:
    cloonix cli %s ssh r2 "ip -f mpls route show"
""" % (NET, NET))

main()
