#!/usr/bin/env python3
r"""static_example - Static, Example static configs.

    ./static_example.py [net]     the net defaults to nemo

The example of the BIRD User's Guide, chapter Static, section "Example
static configs":
https://bird.nic.cz/doc/latest/bird-6.html#static-example

    g (198.51.100.130, .10, .20, .100,  ---- lan0 ---- r ---- lan1 ---- g2
       2001:db8:1::1, fe80::10)                              (192.0.2.1, fe80::30)

  r carries the guide's two static protocols: in IPv4, into the table
  testable (kernel table 100), a default route, a multipath 10.0.0.0/8
  with a weight and a BFD-controlled next hop, a blackhole, a direct
  route, an onlink route and three routes with OSPF and BGP attributes;
  in IPv6, global and link-local next hops, a direct route, an unreachable
  /32 and a BFD-controlled default route.

  ACT 1  the routes land in the kernel as the guide writes them; the two
         next hops under BFD, 198.51.100.20 and the IPv6 default, stay out:
         nothing answers BFD on g yet.
  ACT 2  g starts BFD: 198.51.100.20 joins the multipath and the IPv6
         default route appears.

Image: bird_trixie (bird2)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r eth0 sniff=on
    cloonix cli nemo sniff r eth0 --dump "udp port 3784"

  The BFD packets of ACT 2, from 198.51.100.1 to 198.51.100.20: when they
  come back Up, a next hop is added to a route.

Important
  The guide's "arc0" is eth1 here; its route through the vlan interface
  'eth1.60' is left out, there is no vlan; "eth2" is eth1.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and birdlib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, hold, ssh, step, story
from birdlib import bird_start, birdc, opening, proof, until, verdict

HISTORY = """\
Static routes are the oldest routes of all, typed by hand. What BIRD adds
is that a static route is a route like any other: it carries attributes
other protocols read, it lives in a table of one's choice, and its next
hops can be checked by the link state or by BFD.

BIRD began in 1998 as a student project at the Faculty of Mathematics and
Physics of Charles University in Prague, by Martin Mares, Pavel Machek and
Ondrej Filip; CZ.NIC maintains it since 2009.

Today: the static protocol is in almost every bird.conf: default routes,
blackholes, and the networks a router originates.
"""

ROOTFS_CHOICES = ("bird_trixie",)
TOOL = "birdc"

R, G, G2 = "r", "g", "g2"
ALL = (R, G, G2)
L0, L1 = "lan0", "lan1"

WIRING = {
    R: [(0, L0, "198.51.100.1/24"), (1, L1, "192.0.2.254/24")],
    G: [(0, L0, "198.51.100.130/24")],
    G2: [(0, L1, "192.0.2.1/24")],
}
GATEWAY = {}

# The guide's two static protocols, "arc0" and "eth2" -> "eth1".
R_CONF = """\
router id 198.51.100.1;
protocol device { }
protocol bfd { }
ipv4 table testable;
protocol kernel { ipv4 { table testable; export all; }; kernel table 100; }
protocol kernel { ipv6 { export all; }; }

protocol static {
	ipv4 { table testable; };	# Connect to a non-default routing table
	check link;			# Advertise routes only if link is up
	route 0.0.0.0/0 via 198.51.100.130; # Default route
	route 10.0.0.0/8		# Multipath route
		via 198.51.100.10 weight 2
		via 198.51.100.20 bfd	# BFD-controlled next hop
		via 192.0.2.1;
	route 203.0.113.0/24 blackhole; # Sink route
	route 10.2.0.0/24 via "eth1";	# Direct route
	route 10.2.2.0/24 via 192.0.2.1 dev "eth0" onlink; # Route with both nexthop and iface
	route 192.168.10.0/24 via 198.51.100.100 {
		ospf_metric1 = 20;	# Set extended attribute
	};
	route 192.168.11.0/24 via 198.51.100.100 {
		ospf_metric2 = 100;	# Set extended attribute
		ospf_tag = 2;		# Set extended attribute
	};
	route 192.168.12.0/24 via 198.51.100.100 {
		bgp_community.add((65535, 65281));	# Set extended BGP attribute
		bgp_large_community.add((64512, 1, 1));	# Set extended BGP attribute
	};
}

protocol static {
	ipv6;						# Channel is mandatory
	route 2001:db8:10::/48 via 2001:db8:1::1;	# Route with global nexthop
	route 2001:db8:20::/48 via fe80::10%eth0;	# Route with link-local nexthop
	route 2001:db8:40::/48 via fe80::30 dev "eth1";	# Another link-local nexthop
	route 2001:db8:50::/48 via "eth1";		# Direct route to eth1
	route 2001:db8::/32 unreachable;		# Unreachable route
	route ::/0 via 2001:db8:1::1 bfd;		# BFD-controlled default route
}
"""

G_BFD = """\
router id 198.51.100.130;
protocol device { }
protocol bfd {
	neighbor 198.51.100.1 local 198.51.100.20;
	neighbor 2001:db8:1::2 local 2001:db8:1::1;
}
"""

LAYOUT = {G: (-420, 0), L0: (-210, 0), R: (0, 0), L1: (210, 0),
          G2: (420, 0)}
ETHS = {(G, "eth0"): 90, (R, "eth0"): 270, (R, "eth1"): 90,
        (G2, "eth0"): 270}

CARD = r"""
    g ---- r ---- g2

  Every kind of static route the guide shows, checked in the kernel of r.
"""

def t100():
    return ssh(R, "ip route show table 100", check=False)

def six():
    return ssh(R, "ip -6 route show", check=False)

def main():
    opening()
    for a in ("198.51.100.10/24", "198.51.100.20/24", "198.51.100.100/24"):
        ssh(G, "ip addr add %s dev eth0" % a)
    ssh(G, "ip -6 addr add 2001:db8:1::1/64 dev eth0")
    ssh(G, "ip -6 addr add fe80::10/64 dev eth0")
    ssh(G2, "ip -6 addr add fe80::30/64 dev eth0")
    ssh(R, "ip -6 addr add 2001:db8:1::2/64 dev eth0")

    step("act 1: the static routes in the kernel, the BFD next hops out")
    bird_start(R, R_CONF)
    want4 = ["default via 198.51.100.130", "weight 2", "192.0.2.1",
             "blackhole 203.0.113.0/24", "10.2.0.0/24 dev eth1", "onlink"]
    ok_v4 = until(lambda: all(w in t100() for w in want4), 60)
    ok_bfd4 = "198.51.100.20" not in t100()
    proof("r, kernel table 100:", t100())
    want6 = ["2001:db8:10::/48 via 2001:db8:1::1", "2001:db8:20::/48 via fe80::10",
             "2001:db8:40::/48 via fe80::30", "2001:db8:50::/48 dev eth1",
             "unreachable 2001:db8::/32"]
    ok_v6 = until(lambda: all(w in six() for w in want6), 60)
    ok_bfd6 = "default via 2001:db8:1::1" not in six()
    proof("r, the IPv6 routes:", six())
    proof("r, the attributes of 192.168.12.0/24:",
          birdc(R, "show route 192.168.12.0/24 table testable all"))
    hold()

    step("act 2: g answers BFD, the two next hops come in")
    got = count_on(R, 0, "udp port 3784", lambda: (
        bird_start(G, G_BFD),
        until(lambda: "198.51.100.20" in t100() and
              "default via 2001:db8:1::1" in six(), 60))[1], settle=1.0)
    ok_in = got["out"] if got else "198.51.100.20" in t100()
    proof("r, its BFD sessions:", birdc(R, "show bfd sessions"))
    proof("r, kernel table 100:", t100())

    verdict([("the IPv4 routes in kernel table 100", ok_v4),
             ("198.51.100.20 out while BFD is down", ok_bfd4),
             ("the IPv6 routes in the kernel", ok_v6),
             ("the IPv6 default out while BFD is down", ok_bfd6),
             ("BFD up: both next hops come in", ok_in)])

    story("""
the network is left up. Things to try:

  the static routes as BIRD holds them:
    cloonix cli %s ssh r "birdc show static"

  the multipath route in the kernel:
    cloonix cli %s ssh r "ip route show table 100 10.0.0.0/8"
""" % (NET, NET))

main()
