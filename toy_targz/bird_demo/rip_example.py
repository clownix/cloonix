#!/usr/bin/env python3
r"""rip_example - RIP, Example.

    ./rip_example.py [net]     the net defaults to nemo

The example of the BIRD User's Guide, chapter RIP, section "Example":
https://bird.nic.cz/doc/latest/bird-6.html#rip-exam

    h1 ---- lan1 ---- r1 ---- lan12 ---- r2 ---- lan2 ---- h2
       10.1.0.0/24                              10.2.0.0/24

  r1 and r2 carry the guide's protocol rip word for word: on "eth*",
  metric 2, port 1520, multicast, an update every 12 seconds, a timeout of
  60, and cryptographic authentication, HMAC-SHA256 with "secret".

  ACT 1  each learns the lan of the other, metric included; h1 reaches h2.
  ACT 2  r2 is given another password: r1 refuses its updates, the routes
         of r2 time out after 60 seconds, h1 no longer reaches h2.

Image: bird_trixie (bird2)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r1 eth0 sniff=on
    cloonix cli nemo sniff r1 eth0 --dump "udp port 1520"

  An update on port 1520, not 520 as RIP usually is, its authentication
  entry first and the HMAC-SHA256 trailer at its end.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and birdlib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, hold, in_parallel, reached, step, story
from birdlib import bird_start, birdc, forwarding, has, lacks, opening, \
    ping, pings, proof, reconfigure, until, verdict

HISTORY = """\
RIP comes from the routing of the Xerox PARC Universal Packet, then routed
of 4.2BSD in 1982, written down by Charles Hedrick in RFC 1058 in 1988;
RIPv2, RFC 2453 by Gary Malkin, added masks and multicast, and RFC 4822 by
Ran Atkinson and Manav Bhatia its cryptographic authentication with SHA.
The RIP of BIRD was rewritten by Ondrej Zajicek of CZ.NIC in 2015.

Today: RIP lives on in small networks and industrial equipment, and in the
first lesson of every routing course.
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
RID = {R1: "10.0.12.1", R2: "10.0.12.2"}

# The guide's protocol rip, with the password as a parameter for ACT 2.
CONF = """\
router id %s;
protocol device { }
protocol direct { ipv4; }
protocol kernel { ipv4 { export where source = RTS_RIP; }; }
protocol rip {
	ipv4 {
		import all;
		export all;
	};
	interface "eth*" {
		metric 2;
		port 1520;
		mode multicast;
		update time 12;
		timeout time 60;
		authentication cryptographic;
		password "%s" { algorithm hmac sha256; };
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

  RIP on port 1520, every 12 seconds, its updates signed with HMAC-SHA256.
"""

def configure(node):
    forwarding(node)
    bird_start(node, CONF % (RID[node], "secret"))

def main():
    opening()

    step("act 1: each router learns the lan of the other")
    in_parallel(configure, list(ROUTERS), "bird start")
    got = count_on(R1, 0, "udp port 1520", lambda: until(
        lambda: has(R1, "show route protocol rip1", ["10.2.0.0/24"]) and
        has(R2, "show route protocol rip1", ["10.1.0.0/24"]), 90), settle=1.0)
    ok_routes = got["out"] if got else \
        has(R1, "show route protocol rip1", ["10.2.0.0/24"])
    proof("r1, its RIP neighbours:", birdc(R1, "show rip neighbors"))
    proof("r1, the routes RIP gave it:",
          birdc(R1, "show route protocol rip1 all"))
    until(lambda: pings(H1, "10.2.0.10"), 30)
    r = ping(H1, "10.2.0.10")
    proof("h1 -> h2:", r)
    ok_ping = reached(r)
    hold()

    step("act 2: r2 is given another password, its routes time out")
    reconfigure(R2, CONF % (RID[R2], "wrong"))
    ok_gone = until(lambda: lacks(R1, "show route", ["10.2.0.0/24"]), 120,
                    period=3)
    proof("r1, its routes, without 10.2.0.0/24:", birdc(R1, "show route"))
    ok_cut = until(lambda: not pings(H1, "10.2.0.10"), 30)

    verdict([("each router learns the other's lan", ok_routes),
             ("h1 reaches h2", ok_ping),
             ("a wrong password: the routes of r2 time out", ok_gone),
             ("h1 is cut from h2", ok_cut)])

    story("""
the network is left up. Things to try:

  the RIP interfaces, port and timers:
    cloonix cli %s ssh r1 "birdc show rip interfaces"

  the updates, every 12 seconds:
    cloonix cli %s sniff r1 eth0 --dump "udp port 1520"
""" % (NET, NET))

main()
