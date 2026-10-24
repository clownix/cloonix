#!/usr/bin/env python3
r"""babel_example - Babel, Example.

    ./babel_example.py [net]     the net defaults to nemo

The example of the BIRD User's Guide, chapter Babel, section "Example":
https://bird.nic.cz/doc/latest/bird-6.html#babel-exam

    h1 ---- lan1 ---- r1 ---- lan12 ---- r2 ---- lan2 ---- h2
       10.1.0.0/24                              10.2.0.0/24
       2001:db8:1::/64                          2001:db8:2::/64

  r1 and r2 carry the guide's protocol babel word for word: "eth*" wired,
  the wireless and tap interfaces it names (absent here, so unused), and
  the export of the device routes and the Babel routes, IPv4 and IPv6, the
  default of babeld.

  ACT 1  r1 and r2 hear each other's hellos.
  ACT 2  each learns the two lans of the other, IPv4 and IPv6, over one
         protocol; h1 reaches h2 in both families.

Image: bird_trixie (bird2)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r1 eth0 sniff=on
    cloonix cli nemo sniff r1 eth0 --dump "udp port 6696"

  An update to ff02::1:6 over IPv6 link-local, carrying an IPv4 prefix as
  well: Babel is one protocol for both families.

Important
  The guide writes "hello interval 1;"; the bird 2.17 of the image wants a
  unit there and refuses the line, so it is "hello interval 1 s;".
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and birdlib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, hold, in_parallel, reached, ssh, step, \
    story
from birdlib import bird_start, birdc, forwarding, has, opening, ping, \
    pings, proof, until, verdict

HISTORY = """\
Babel was designed by Juliusz Chroboczek at the University Paris Diderot
in 2007, a distance vector that does not loop even while it converges,
made for wireless meshes and good on wires: RFC 6126, now RFC 8966. Its
BIRD implementation was written by Toke Hoiland-Jorgensen in 2016.

Today: Babel runs community meshes, home networks and overlays.
"""

ROOTFS_CHOICES = ("bird_trixie",)
TOOL = "birdc"

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
V6 = {H1: ("eth0", "2001:db8:1::10/64"), R1: ("eth1", "2001:db8:1::1/64"),
      R2: ("eth1", "2001:db8:2::1/64"), H2: ("eth0", "2001:db8:2::10/64")}
GW6 = {H1: "2001:db8:1::1", H2: "2001:db8:2::1"}
RID = {R1: "10.0.12.1", R2: "10.0.12.2"}

# The guide's protocol babel.
CONF = """\
router id %s;
protocol device { }
protocol direct { ipv4; ipv6; }
protocol kernel { ipv4 { export where source = RTS_BABEL; }; }
protocol kernel { ipv6 { export where source = RTS_BABEL; }; }
protocol babel {
	interface "eth*" {
		type wired;
	};
	interface "wlan0", "wlan1" {
		type wireless;
		hello interval 1 s;
		rxcost 512;
	};
	interface "tap0";

	# This matches the default of babeld: redistribute all addresses
	# configured on local interfaces, plus re-distribute all routes received
	# from other babel peers.

	ipv4 {
		export where (source = RTS_DEVICE) || (source = RTS_BABEL);
	};
	ipv6 {
		export where (source = RTS_DEVICE) || (source = RTS_BABEL);
	};
}
"""

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

  Babel on the wire, both families in one protocol.
"""

def configure(node):
    forwarding(node)
    bird_start(node, CONF % RID[node])

def main():
    opening()
    for node, (eth, addr) in V6.items():
        ssh(node, "ip -6 addr add %s dev %s" % (addr, eth))
    for node, gw in GW6.items():
        ssh(node, "ip -6 route add default via %s" % gw)

    step("act 1: r1 and r2 hear each other's hellos")
    in_parallel(configure, list(ROUTERS), "bird start")
    ok_nbr = until(lambda: has(R1, "show babel neighbors", ["fe80"]), 90)
    proof("r1, its Babel neighbours:", birdc(R1, "show babel neighbors"))
    hold()

    step("act 2: the lans of the other, IPv4 and IPv6, h1 reaches h2")
    got = count_on(R1, 0, "udp port 6696", lambda: until(
        lambda: has(R1, "show route protocol babel1",
                    ["10.2.0.0/24", "2001:db8:2::/64"]), 90), settle=1.0)
    ok_routes = got["out"] if got else has(
        R1, "show route protocol babel1", ["10.2.0.0/24", "2001:db8:2::/64"])
    proof("r1, the routes Babel gave it:",
          birdc(R1, "show route protocol babel1"))
    until(lambda: pings(H1, "10.2.0.10") and pings(H1, "2001:db8:2::10"), 30)
    r4 = ping(H1, "10.2.0.10")
    r6 = ping(H1, "2001:db8:2::10", count=3)
    proof("h1 -> h2, IPv4:", r4)
    proof("h1 -> h2, IPv6:", r6)

    verdict([("r1 and r2 are Babel neighbours", ok_nbr),
             ("r1 learns 10.2.0.0/24 and 2001:db8:2::/64", ok_routes),
             ("h1 reaches h2 in IPv4", reached(r4)),
             ("h1 reaches h2 in IPv6", reached(r6))])

    story("""
the network is left up. Things to try:

  the Babel interfaces and their type:
    cloonix cli %s ssh r1 "birdc show babel interfaces"

  the Babel route entries, with their metric:
    cloonix cli %s ssh r1 "birdc show babel entries"
""" % (NET, NET))

main()
