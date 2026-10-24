#!/usr/bin/env python3
r"""openfabric_sample - Sample configuration (OpenFabric).

    ./openfabric_sample.py [net]     the net defaults to nemo

The first example of the FRR User Guide, page fabricd, section "Sample
configuration": "A simple example".
https://docs.frrouting.org/en/latest/fabricd.html#sample-configuration

                  r2  (192.0.2.2, 2001:db8::2)
                /    \
              r1 ---- r3
    (192.0.2.1,        (192.0.2.3,
     2001:db8::1)       2001:db8::3)

  Each router carries the guide's configuration: its loopback addresses
  and its two links in "openfabric 1", net 49.0000.0000.000N.00.

  ACT 1  the adjacencies come up; r1 reaches the loopbacks of r3, IPv4 and
         IPv6, over the direct link.
  ACT 2  the link r1-r3 is cut: the fabric goes through r2.

Image: frr_trixie (zebra, fabricd)
Size:  3 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r1 eth0 sniff=on
    cloonix cli nemo sniff r1 eth0 --dump "not ip and not ip6 and not arp"

  An OpenFabric link state, an IS-IS PDU on the wire: OpenFabric is IS-IS
  with the flooding reduced for fabrics, so the filter is the same.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, frr_setup, hold, in_parallel, \
    ssh, step, story
from frrlib import forwarding, opening, ping, pings, proof, until, verdict, \
    vtysh

HISTORY = """\
OpenFabric was designed by Russ White and Christian Franke, an IS-IS
reduced to what a data centre fabric needs, with flooding optimised for
the dense meshes of spine and leaf; it was published as an IETF draft in
2018. Christian Franke wrote fabricd in frr the same year, on the isisd
code it shares.

Today: OpenFabric is one of the answers, with BGP and IS-IS itself, to the
routing of large Clos fabrics.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

R1, R2, R3 = "r1", "r2", "r3"
ALL = (R1, R2, R3)
L12, L23, L13 = "lan12", "lan23", "lan13"

WIRING = {
    R1: [(0, L12, "10.0.12.1/24"), (1, L13, "10.0.13.1/24")],
    R2: [(0, L12, "10.0.12.2/24"), (1, L23, "10.0.23.2/24")],
    R3: [(0, L23, "10.0.23.3/24"), (1, L13, "10.0.13.3/24")],
}
GATEWAY = {}
NUM = {R1: 1, R2: 2, R3: 3}

def frr_conf(node):
    n = NUM[node]
    return "\n".join([
        "hostname %s" % node,
        "interface lo",
        " ip address 192.0.2.%d/32" % n,
        " ip router openfabric 1",
        " ipv6 address 2001:db8::%d/128" % n,
        " ipv6 router openfabric 1",
        "interface eth0",
        " ip router openfabric 1",
        " ipv6 router openfabric 1",
        "interface eth1",
        " ip router openfabric 1",
        " ipv6 router openfabric 1",
        "router openfabric 1",
        " net 49.0000.0000.000%d.00" % n])

LAYOUT = {
    R1: (-300, 150), L13: (0, 150), R3: (300, 150),
    L12: (-150, 0), R2: (0, -150), L23: (150, 0),
}
ETHS = {
    (R1, "eth0"): 45, (R1, "eth1"): 90, (R2, "eth0"): 225,
    (R2, "eth1"): 135, (R3, "eth0"): 315, (R3, "eth1"): 270,
}

CARD = r"""
          r2
        /    \
      r1 ---- r3

  A three-router fabric, loopbacks in IPv4 and IPv6.
"""

def configure(node):
    forwarding(node)
    frr_setup(node, ["fabricd"], frr_conf(node))

def via(node, dst):
    return ssh(node, "ip route get %s 2>/dev/null | head -1" % dst,
               check=False).strip()

def main():
    opening()

    step("act 1: the fabric comes up, r1 reaches the loopbacks of r3")
    in_parallel(configure, list(ALL), "frr start")
    ok_adj = until(lambda: vtysh(R1, "show openfabric neighbor").count("Up")
                   >= 2, 90)
    ok_v4 = until(lambda: pings(R1, "192.0.2.3") and
                  "10.0.13.3" in via(R1, "192.0.2.3"), 90)
    ok_v6 = until(lambda: pings(R1, "2001:db8::3"), 60)
    proof("r1, its neighbours:", vtysh(R1, "show openfabric neighbor"))
    proof("r1, the routes of the fabric:",
          vtysh(R1, "show ip route openfabric") +
          vtysh(R1, "show ipv6 route openfabric"))
    proof("r1 -> 192.0.2.3:", ping(R1, "192.0.2.3"))
    proof("r1 -> 2001:db8::3:", ping(R1, "2001:db8::3", count=3))
    hold()

    step("act 2: the link r1-r3 is cut, the fabric goes through r2")
    got = count_on(R1, 0, "not ip and not ip6 and not arp", lambda: (
        ssh(R1, "ip link set eth1 down"),
        until(lambda: "10.0.12.2" in via(R1, "192.0.2.3"), 60))[1],
        settle=1.0)
    ok_reroute = got["out"] if got else "10.0.12.2" in via(R1, "192.0.2.3")
    proof("r1, its way to 192.0.2.3:", via(R1, "192.0.2.3"))
    # r1 saw its link go down at once; r3 keeps its way back through r1
    # until its own adjacency to r1 times out
    ok_ping = until(lambda: pings(R1, "192.0.2.3"), 90)
    proof("r1 -> 192.0.2.3, through r2:", ping(R1, "192.0.2.3"))

    verdict([("two adjacencies on r1", ok_adj),
             ("r1 reaches 192.0.2.3 directly", ok_v4),
             ("r1 reaches 2001:db8::3", ok_v6),
             ("the cut moves the path through r2", ok_reroute),
             ("r1 still reaches 192.0.2.3", ok_ping)])

    story("""
the network is left up. Things to try:

  the topology OpenFabric computed:
    cloonix cli %s ssh r2 "vtysh -c 'show openfabric topology'"

  the link state database:
    cloonix cli %s ssh r2 "vtysh -c 'show openfabric database'"
""" % (NET, NET))

main()
