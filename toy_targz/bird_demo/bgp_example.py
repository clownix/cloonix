#!/usr/bin/env python3
r"""bgp_example - BGP, Example (TCP-AO, else TCP-MD5).

    ./bgp_example.py [net]     the net defaults to nemo

The example of the BIRD User's Guide, chapter BGP, section "Example":
https://bird.nic.cz/doc/latest/bird-6.html#bgp-exam

    r (AS 65000, 198.51.100.14) -- lanA -- m -- lanB -- p (AS 64496, 198.51.100.130)
                                   /25    (router)  /25

  The session is protected by the TCP Authentication Option, which linux
  has since 6.7 when built with CONFIG_TCP_AO, and the containers run on
  the host kernel. On a kernel without it the session is signed with
  TCP-MD5 instead, the password of BIRD's protocol bgp, and the demo says
  so; the rest is the same.

  r carries the guide's protocol bgp: a multihop session to a neighbour
  that is not on its lan, TCP-AO with two keys (hmac sha256 and cmac
  aes128), and the export filter that sends only static routes and adds
  the community (65000,64501); next hop self. Its prepend tests the path
  for [= 65000 =], which a static route does not have yet: BIRD adds the
  local AS after the filter, so the route goes out with the path 65000.

  ACT 1  the session comes up through m, signed with TCP-AO (TCP-MD5 on a
         kernel without TCP-AO).
  ACT 2  p receives 203.0.113.0/24 with the AS path 65000 and the
         community (65000,64501).

Image: bird_trixie (bird2)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf p eth0 sniff=on
    cloonix cli nemo sniff p eth0 --dump "tcp port 179"

  Any segment of the session: a TCP option of kind 29, TCP-AO, with its
  key ids and a MAC, where MD5 would be kind 19.

Important
  The guide's igp tables, its ipv6 channel with mylargefilter and its
  multicast channel name tables and filters it does not define: the
  ipv4 channel is played, word for word otherwise.
"""
import os
import re
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and birdlib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, hold, in_parallel, step, story
from birdlib import bird_start, birdc, forwarding, has, has_tcp_ao, \
    opening, proof, until, verdict

HISTORY = """\
BGP was sketched by Kirk Lougheed and Yakov Rekhter on three napkins at the
IETF of January 1989. Its sessions were protected by TCP MD5 since RFC 2385
in 1998; TCP-AO, RFC 5925 in 2010 by Joe Touch, Allison Mankin and Ron
Bonica, replaced it with several keys and modern algorithms, and reached
linux only in 6.7, in 2024, written by Dmitry Safonov.

BIRD began in 1998 as a student project at the Faculty of Mathematics and
Physics of Charles University in Prague, by Martin Mares, Pavel Machek and
Ondrej Filip; CZ.NIC maintains it since 2009.

Today: BIRD carries BGP for internet exchanges, hosting providers and
content networks; TCP-AO is slowly replacing MD5 on their sessions.
"""

ROOTFS_CHOICES = ("bird_trixie",)
TOOL = "birdc"

R, M, P = "r", "m", "p"
ALL = (R, M, P)
LA, LB = "lanA", "lanB"

WIRING = {
    R: [(0, LA, "198.51.100.14/25")],
    M: [(0, LA, "198.51.100.1/25"), (1, LB, "198.51.100.129/25")],
    P: [(0, LB, "198.51.100.130/25")],
}
GATEWAY = {}

# The guide's protocol bgp, its ipv4 channel.
R_CONF = """\
router id 198.51.100.14;
protocol device { }
protocol static { ipv4; route 203.0.113.0/24 blackhole;
                  route 198.51.100.128/25 via 198.51.100.1; }
protocol kernel { ipv4 { export where source = RTS_STATIC; }; }

protocol bgp {
	local 198.51.100.14 as 65000;	     # Use a private AS number
	neighbor 198.51.100.130 as 64496;    # Our neighbor ...
	multihop;			     # ... which is connected indirectly
@AUTH@	ipv4 {
		export filter {			     # We use non-trivial export rules
			if source = RTS_STATIC then { # Export only static routes
				# Assign our community
				bgp_community.add((65000,64501));
				# Artificially increase path length
				# by advertising local AS number twice
				if bgp_path ~ [= 65000 =] then
					bgp_path.prepend(65000);
				accept;
			}
			reject;
		};
		import all;
		next hop self; # advertise this router as next hop
	};
}
"""

# The other end: the same keys, ids crossed.
P_CONF = """\
router id 198.51.100.130;
protocol device { }
protocol static { ipv4; route 198.51.100.0/25 via 198.51.100.129; }
protocol kernel { ipv4 { export where source = RTS_STATIC; }; }

protocol bgp {
	local 198.51.100.130 as 64496;
	neighbor 198.51.100.14 as 65000;
	multihop;
@AUTH@	ipv4 { import all; export none; };
}
"""

# The guide's TCP-AO keys, the ids of the second crossed on p.
AO = """\
\tauthentication ao;\t\t     # We use TCP-AO authentication
\tkeys {
\t\tkey {
\t\t\tid 0;
\t\t\tsecret "hello321";
\t\t\talgorithm hmac sha256;
\t\t\tpreferred;
\t\t};
\t\tkey {
\t\t\tsend id %d;
\t\t\trecv id %d;
\t\t\tsecret "bye123";
\t\t\talgorithm cmac aes128;
\t\t};
\t};
"""
# A kernel without TCP-AO: the session signed with TCP-MD5.
MD5 = """\
\tauthentication md5;\t\t     # TCP-MD5, this kernel has no TCP-AO
\tpassword "hello321";
"""

def confs(ao):
    return {R: R_CONF.replace("@AUTH@", AO % (2, 1) if ao else MD5),
            P: P_CONF.replace("@AUTH@", AO % (1, 2) if ao else MD5)}

LAYOUT = {R: (-440, 0), LA: (-220, 0), M: (0, 0), LB: (220, 0), P: (440, 0)}
ETHS = {(R, "eth0"): 90, (M, "eth0"): 270, (M, "eth1"): 90, (P, "eth0"): 270}

CARD = r"""
    r (AS 65000) ---- m ---- p (AS 64496)

  A multihop eBGP session signed with TCP-AO, and an export filter that
  tags.
"""

def route_of_r():
    """p has 203.0.113.0/24 with the path 65000 and the community."""
    out = birdc(P, "show route 203.0.113.0/24 all")
    return (bool(re.search(r"as_path:\s*65000\s*$", out, re.M)) and
            "(65000,64501)" in out)

def main():
    opening()
    ao = has_tcp_ao()
    sig = "TCP-AO" if ao else "TCP-MD5"
    conf = confs(ao)
    if not ao:
        story("  this host kernel has no TCP-AO (CONFIG_TCP_AO off): the "
              "session is signed\n  with TCP-MD5, the password of the "
              "protocol bgp, instead of the guide's keys.")
    forwarding(M)

    step("act 1: the multihop session comes up, signed with %s" % sig)
    got = count_on(P, 0, "tcp port 179", lambda: (
        in_parallel(lambda n: bird_start(n, conf[n]), [R, P], "bird start"),
        until(lambda: has(P, "show protocols", ["Established"]), 90))[1],
        settle=1.0)
    ok_est = got["out"] if got else has(P, "show protocols", ["Established"])
    proof("p, the session:", birdc(P, "show protocols all bgp1"))
    hold()

    step("act 2: p receives the route, tagged, its path 65000")
    ok_route = until(route_of_r, 60)
    proof("p, the route of r:", birdc(P, "show route 203.0.113.0/24 all"))
    story("    the path is 65000 alone: at export, the path of a static "
          "route is empty,\n    the prepend of the guide waits for "
          "[= 65000 =], and BIRD adds 65000 after it.")

    verdict([("the %s session is Established" % sig, ok_est),
             ("p gets 203.0.113.0/24, path 65000, community "
              "(65000,64501)", ok_route)])

    story("""
the network is left up. Things to try:

  the keys in use:
    cloonix cli %s ssh r "birdc show protocols all bgp1"

  the %s option on the wire, TCP option kind %d:
    cloonix cli %s sniff p eth0 --dump "tcp port 179"
""" % (NET, sig, 29 if ao else 19, NET))

main()
