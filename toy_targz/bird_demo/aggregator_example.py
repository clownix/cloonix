#!/usr/bin/env python3
r"""aggregator_example - Aggregator, Example.

    ./aggregator_example.py [net]     the net defaults to nemo

The example of the BIRD User's Guide, chapter Aggregator, section
"Example":
https://bird.nic.cz/doc/latest/bird-6.html#aggregator-example

    p1 (AS 65001) ----+
                      +---- lan0 2001:db8:ff::/64 ---- r (AS 65000)
    p2 (AS 65002) ----+

  r carries the guide's protocol aggregator word for word: from master6,
  the routes with an AS path, grouped by prefix and by AS path length,
  each group merged into one route whose communities are those of all its
  members, written into the table agr_result.

  ACT 1  p1 and p2 announce 2001:db8:a::/48, each with its own community,
         with AS paths of the same length; p2 also announces 2001:db8:b::/48
         with its path prepended, p1 without.
  ACT 2  in agr_result, 2001:db8:a::/48 is one route carrying both
         communities; 2001:db8:b::/48 stays two routes, the lengths differ.

Image: bird_trixie (bird2)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r eth0 sniff=on
    cloonix cli nemo sniff r eth0 --dump "tcp port 179"

  The two UPDATEs for 2001:db8:a::/48, one from each peer: what the
  aggregator merges inside r.

Important
  The bird 2.17 of the image refuses two lines of the guide as printed:
  "aggregate on net, bgp_path.len" wants the expression in parentheses,
  "(bgp_path.len)", and "bgp_path = r.bgp_path" its semicolon.
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
from demolib import NET, hold, in_parallel, ssh, step, story
from birdlib import bird_start, birdc, has, opening, proof, until, verdict

HISTORY = """\
Route aggregation is as old as CIDR, RFC 1519 in 1993; what the aggregator
of BIRD adds is aggregation by any key the filter language can compute,
with a merge rule of one's own. It came in BIRD 2.16, in 2024, by Igor
Putovny and Ondrej Zajicek of CZ.NIC.

BIRD began in 1998 as a student project at the Faculty of Mathematics and
Physics of Charles University in Prague, by Martin Mares, Pavel Machek and
Ondrej Filip; CZ.NIC maintains it since 2009.

Today: aggregation keeps tables small; the aggregator of BIRD lets an
operator decide what counts as the same route.
"""

ROOTFS_CHOICES = ("bird_trixie",)
TOOL = "birdc"

R, P1, P2 = "r", "p1", "p2"
ALL = (R, P1, P2)
L0 = "lan0"

WIRING = {
    R: [(0, L0, "10.0.0.1/24")],
    P1: [(0, L0, "10.0.0.2/24")],
    P2: [(0, L0, "10.0.0.3/24")],
}
GATEWAY = {}
V6 = {R: "2001:db8:ff::1/64", P1: "2001:db8:ff::2/64", P2: "2001:db8:ff::3/64"}

# The guide's protocol aggregator, fed by two BGP sessions.
R_CONF = """\
router id 10.0.0.1;
protocol device { }
ipv6 table agr_result;
protocol bgp p1 {
	local as 65000;
	neighbor 2001:db8:ff::2 as 65001;
	ipv6 { import all; export none; };
}
protocol bgp p2 {
	local as 65000;
	neighbor 2001:db8:ff::3 as 65002;
	ipv6 { import all; export none; };
}

protocol aggregator {
  table master6;
  export where defined(bgp_path);
  /* Merge all routes with the same AS Path length */
  aggregate on net, (bgp_path.len);
  merge by {
    for route r in routes do {
      if ! defined(bgp_path) then { bgp_path = r.bgp_path; }
      bgp_community = bgp_community.add(r.bgp_community);
    }
    accept;
  };
  import all;
  peer table agr_result;
}
"""

def peer_conf(rid, asn, b_prepend):
    pre = "bgp_path.prepend(%d); " % asn if b_prepend else ""
    return """\
router id %s;
protocol device { }
protocol static { ipv6; route 2001:db8:a::/48 blackhole;
                  route 2001:db8:b::/48 blackhole; }
protocol bgp {
	local as %d;
	neighbor 2001:db8:ff::1 as 65000;
	ipv6 {
		import none;
		export filter {
			bgp_community.add((%d, 1));
			if net = 2001:db8:b::/48 then { %s}
			accept;
		};
	};
}
""" % (rid, asn, asn, pre)

CONFS = {R: R_CONF, P1: peer_conf("10.0.0.2", 65001, False),
         P2: peer_conf("10.0.0.3", 65002, True)}

LAYOUT = {P1: (-320, -130), P2: (-320, 130), L0: (-80, 0), R: (200, 0)}
ETHS = {(P1, "eth0"): 135, (P2, "eth0"): 45, (R, "eth0"): 270}

CARD = r"""
    p1 --+
         +-- lan0 -- r   (aggregate on net, bgp_path.len)
    p2 --+

  Routes of the same prefix and the same AS path length become one.
"""

def configure(node):
    bird_start(node, CONFS[node])

# One line per route, the first with the prefix. BIRD 2.17 prints no count
# for a prefix, and its output starts with "BIRD ... ready.".
ROUTE = re.compile(r"^\S*\s+(unicast|unreachable|blackhole|prohibit)\s+\[",
                   re.M)

def routes(cmd):
    """How many routes birdc shows for cmd."""
    return len(ROUTE.findall(birdc(R, cmd)))

def main():
    opening()
    for node, addr in V6.items():
        ssh(node, "ip -6 addr add %s dev eth0" % addr)

    step("act 1: p1 and p2 announce their routes")
    in_parallel(configure, list(ALL), "bird start")
    ok_in = until(lambda: routes("show route 2001:db8:a::/48 table master6")
                  >= 2, 120)
    proof("r, master6, two routes per prefix:",
          birdc(R, "show route table master6"))
    hold()

    step("act 2: agr_result merges same prefix, same AS path length")
    ok_a = until(lambda: has(R, "show route 2001:db8:a::/48 table "
                                "agr_result all",
                             ["(65001,1)", "(65002,1)"]), 60)
    ok_one = routes("show route 2001:db8:a::/48 table agr_result") == 1
    ok_b = routes("show route 2001:db8:b::/48 table agr_result") == 2
    proof("r, agr_result:", birdc(R, "show route table agr_result all"))

    verdict([("both peers announce 2001:db8:a::/48", ok_in),
             ("agr_result holds one route with both communities",
              ok_a and ok_one),
             ("2001:db8:b::/48 stays apart, its lengths differ", ok_b)])

    story("""
the network is left up. Things to try:

  the aggregator and its counters:
    cloonix cli %s ssh r "birdc show protocols all aggregator1"

  the merged route with all its attributes:
    cloonix cli %s ssh r "birdc show route table agr_result all"
""" % (NET, NET))

main()
