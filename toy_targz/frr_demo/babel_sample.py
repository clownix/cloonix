#!/usr/bin/env python3
r"""babel_sample - Babel sample configuration file.

    ./babel_sample.py [net]     the net defaults to nemo

The example of the FRR User Guide, page babeld, section "Babel sample
configuration file":
https://docs.frrouting.org/en/latest/babeld.html#babel-sample-configuration-file

    h1 ---- lan1 ---- r1 ---- lan12 ---- r2 ---- lan2 ---- h2
       10.1.0.0/24                              10.2.0.0/24

  The guide's file has its network and wired lines commented out: here
  they are taken in, "network eth0", and on eth0 "babel wired", "babel
  split-horizon", "babel hello-interval 12000", "babel update-interval
  36000", the tweaks the guide recommends for a wired interface. The
  commented "redistribute ipv4 kernel" becomes "redistribute ipv4
  connected", so the host lans are announced.

  ACT 1  r1 and r2 hear each other's hellos and become neighbours.
  ACT 2  each learns the host lan of the other, h1 reaches h2.

Image: frr_trixie (zebra, babeld)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r1 eth0 sniff=on
    cloonix cli nemo sniff r1 eth0 --dump "udp port 6696"

  An update of r2 to ff02::1:6, over IPv6 link-local even for IPv4
  routes: Babel is one protocol for both families.

Important
  With hellos every 12 seconds and updates every 36, as the guide sets
  them, convergence takes up to a minute: the demo waits for it.
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
from frrlib import forwarding, has, opening, ping, pings, proof, until, \
    verdict, vtysh

HISTORY = """\
Babel was designed by Juliusz Chroboczek at the University Paris Diderot
in 2007, a distance-vector protocol that does not loop even while it
converges, made for wireless meshes and good on wires too: RFC 6126 in
2011, now RFC 8966. The babeld of frr was taken from his own babeld by
Matthieu Boutier and Juliusz Chroboczek, and merged in Quagga in 2012.

Today: Babel runs community meshes, home networks (the IETF homenet work)
and overlay networks such as those of some VPN providers.
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

def frr_conf(node):
    return "\n".join([
        "hostname %s" % node,
        "debug babel common",
        "router babel",
        " network eth0",
        " redistribute ipv4 connected",
        "interface eth0",
        " babel wired",
        " babel split-horizon",
        " babel hello-interval 12000",
        " babel update-interval 36000",
        "log stdout"])

LAYOUT = {
    H1: (-520, 0), L1: (-390, 0), R1: (-220, 0), L12: (0, 0),
    R2: (220, 0), L2: (390, 0), H2: (520, 0),
}
ETHS = {
    (H1, "eth0"): 90, (R1, "eth1"): 270, (R1, "eth0"): 90,
    (R2, "eth0"): 270, (R2, "eth1"): 90, (H2, "eth0"): 270,
}

CARD = r"""
    h1 ---- r1 ---- r2 ---- h2

  Babel on the wire between r1 and r2, with the wired tweaks of the guide.
"""

def configure(node):
    forwarding(node)
    frr_setup(node, ["babeld"], frr_conf(node))

def main():
    opening()

    step("act 1: r1 and r2 become Babel neighbours")
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_nbr = until(lambda: has(R1, "show babel neighbor", ["fe80"]), 90)
    proof("r1, its Babel neighbours:", vtysh(R1, "show babel neighbor"))
    hold()

    step("act 2: each learns the host lan of the other, h1 reaches h2")
    got = count_on(R1, 0, "udp port 6696", lambda: until(
        lambda: has(R1, "show ip route babel", ["10.2.0.0/24"]) and
        has(R2, "show ip route babel", ["10.1.0.0/24"]), 150), settle=1.0)
    ok_routes = got["out"] if got else \
        has(R1, "show ip route babel", ["10.2.0.0/24"])
    proof("r1, its Babel routes:", vtysh(R1, "show babel route"))
    until(lambda: pings(H1, "10.2.0.10"), 30)
    r = ping(H1, "10.2.0.10")
    proof("h1 -> h2:", r)
    ok_ping = reached(r)

    verdict([("r1 and r2 are Babel neighbours", ok_nbr),
             ("each learns the other's host lan", ok_routes),
             ("h1 reaches h2", ok_ping)])

    story("""
the network is left up. Things to try:

  the parameters of the interface:
    cloonix cli %s ssh r1 "vtysh -c 'show babel interface eth0'"

  the Babel packets:
    cloonix cli %s sniff r1 eth0 --dump "udp port 6696"
""" % (NET, NET))

main()
