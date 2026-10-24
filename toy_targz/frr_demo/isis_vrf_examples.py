#!/usr/bin/env python3
r"""isis_vrf_examples - ISIS Vrf Configuration Examples (needs: vrf).

    ./isis_vrf_examples.py [net]     the net defaults to nemo

The example of the FRR User Guide, page isisd, section "ISIS Vrf
Configuration Examples": "A simple vrf example".
https://docs.frrouting.org/en/latest/isisd.html#isis-vrf-config-examples

    h1 ---- lan1 ---- r1 ==== lan12 ==== r2 ---- lan2 ---- h2
       10.1.0.0/24     [ vrf RED on r1 and r2 ]     10.2.0.0/24

  !!! KERNEL MODULE NEEDED ON THE HOST: vrf !!!
  The routers put their interfaces in a linux vrf device, and a container
  cannot load a module. Before playing this demo, on the host, once per
  boot:

sudo modprobe vrf

  or do not run this demo. It stops with a big warning if the module is
  missing.

  r1 and r2 carry the guide's configuration: "interface eth0 vrf RED",
  "router isis FOO vrf RED", level-2-only, wide metrics, point-to-point.
  The host lans are in RED too, passive.

  ACT 1  the adjacency comes up inside RED; the routes land in the table
         of RED, not in the main table; h1 reaches h2 through RED.

Image: frr_trixie (zebra, isisd)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r1 eth0 sniff=on
    cloonix cli nemo sniff r1 eth0 --dump "not ip and not ip6 and not arp"

  A hello of the instance FOO of RED: on the wire nothing says vrf, the
  separation is entirely inside the router.

Important
  The vrf device is made with iproute2, table 10; the hellos are sent
  every second to keep the demo short.
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
from frrlib import forwarding, has, opening, ping, pings, proof, until, \
    verdict, vtysh
from toylib import words_out

HISTORY = """\
Virtual routing and forwarding, one router holding several separate
routing tables, came from the MPLS VPNs of RFC 2547 in 1999 by Eric Rosen
and Yakov Rekhter. Linux got its l3mdev vrf device in kernel 4.3, in 2015,
written by David Ahern and Shrijeet Mukherjee at Cumulus Networks; the
isisd of frr learnt to run one instance per vrf in 2020.

Today: vrfs separate customers, management and production traffic on the
same routers, in carriers and data centres.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

H1, R1, R2, H2 = "h1", "r1", "r2", "h2"
ALL = (R1, R2, H1, H2)
ROUTERS = (R1, R2)
L1, L12, L2 = "lan1", "lan12", "lan2"

WIRING = {
    H1: [(0, L1, "10.1.0.10/24")],
    R1: [(0, L12, "10.0.12.1/24"), (1, L1, "10.1.0.1/24")],
    R2: [(0, L12, "10.0.12.2/24"), (1, L2, "10.2.0.1/24")],
    H2: [(0, L2, "10.2.0.10/24")],
}
GATEWAY = {H1: "10.1.0.1", H2: "10.2.0.1"}
SYSID = {R1: "1900.0004", R2: "1900.0005"}
FAR = {R1: "10.2.0.0/24", R2: "10.1.0.0/24"}

def frr_conf(node):
    return "\n".join([
        "hostname %s" % node,
        "interface eth0 vrf RED",
        " ip router isis FOO",
        " isis network point-to-point",
        " isis circuit-type level-2-only",
        " isis hello-interval 1",
        "interface eth1 vrf RED",
        " ip router isis FOO",
        " isis passive",
        "router isis FOO vrf RED",
        " net 47.0023.0000.0000.0000.0000.0000.0000.%s.00" % SYSID[node],
        " metric-style wide",
        " is-type level-2-only"])

LAYOUT = {
    H1: (-520, 0), L1: (-390, 0), R1: (-220, 0), L12: (0, 0),
    R2: (220, 0), L2: (390, 0), H2: (520, 0),
}
ETHS = {
    (H1, "eth0"): 90, (R1, "eth1"): 270, (R1, "eth0"): 90,
    (R2, "eth0"): 270, (R2, "eth1"): 90, (H2, "eth0"): 270,
}

CARD = r"""
    h1 ---- r1 ==== r2 ---- h2      (all router ports in vrf RED)

  One IS-IS instance inside the vrf RED of each router.
"""

def configure(node):
    forwarding(node)
    ssh(node, "ip link add RED type vrf table 10")
    ssh(node, "ip link set RED up")
    for num, _lan, addr in WIRING[node]:
        ssh(node, "ip link set eth%d master RED" % num)
        ssh(node, "ip addr add %s dev eth%d" % (addr, num), check=False)
    frr_setup(node, ["isisd"], frr_conf(node))

def main():
    opening("vrf")

    step("act 1: IS-IS comes up inside RED, h1 reaches h2 through it")
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_adj = until(lambda: has(R1, "show isis vrf RED neighbor", ["Up"]), 90)
    ok_vrf = until(lambda: has(R1, "show ip route vrf RED isis",
                               [FAR[R1]]), 60)
    # Every interface of r1 is in RED: its main table is empty, and vtysh
    # prints nothing for an empty table. Its summary says it.
    main_tbl = ssh(R1, "vtysh -c 'show ip route' -c 'show ip route summary'",
                   check=False)
    ok_main = words_out(main_tbl, [FAR[R1]])
    proof("r1, its neighbour in RED:", vtysh(R1, "show isis vrf RED neighbor"))
    proof("r1, the table of RED:", vtysh(R1, "show ip route vrf RED"))
    proof("r1, the main table, without 10.2.0.0/24, and its summary:",
          main_tbl)
    got = count_on(R1, 0, "not ip and not ip6 and not arp",
                   lambda: until(lambda: pings(H1, "10.2.0.10"), 30),
                   settle=1.0)
    if got is not None:
        story("    %d IS-IS frame(s) on r1 eth0 meanwhile." % got["count"])
    r = ping(H1, "10.2.0.10")
    proof("h1 -> h2, through RED:", r)
    ok_ping = reached(r)
    hold()

    verdict([("the adjacency is Up in RED", ok_adj),
             ("10.2.0.0/24 is in the table of RED", ok_vrf),
             ("and not in the main table", ok_main),
             ("h1 reaches h2", ok_ping)])

    story("""
the network is left up. Things to try:

  the link state database of RED:
    cloonix cli %s ssh r2 "vtysh -c 'show isis vrf RED database'"

  the vrf seen by the kernel:
    cloonix cli %s ssh r1 "ip route show vrf RED"
""" % (NET, NET))

main()
