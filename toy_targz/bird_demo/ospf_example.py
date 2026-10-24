#!/usr/bin/env python3
r"""ospf_example - OSPF, Example.

    ./ospf_example.py [net]     the net defaults to nemo

The example of the BIRD User's Guide, chapter OSPF, section "Example":
https://bird.nic.cz/doc/latest/bird-6.html#ospf-exam

               area 0.0.0.0                     area 120 (stub)
    r2 ---- lan0 192.168.0.0/24 ---- r1 ---- lan120 192.168.120.0/24 ---- r3
    cost 11, hello 15, simple "aaa"  (ABR)                                  |
                                                                  lanS 172.16.1.0/24
                                                                         + 172.16.2.0/24
                                                                            h3

  r1 carries the guide's protocol MyOSPF: its "eth*" interface of area
  0.0.0.0 with cost 11, hello 15, priority 100, retransmit 7 and the simple
  password "aaa"; its area 120, a stub area with the networks
  172.16.1.0/24 and 172.16.2.0/24 hidden; and the export filter that lets
  only BGP routes in, with ospf_metric1 100.

  ACT 1  the two adjacencies come up, each with its area's settings.
  ACT 2  the networks of area 120: r2 learns the summary 172.16.1.0/24 and
         nothing of 172.16.2.0/24, hidden; r3, in a stub area, gets a
         default route from r1 and no external route; h3 reaches r2.

Image: bird_trixie (bird2)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r2 eth0 sniff=on
    cloonix cli nemo sniff r2 eth0 --dump "ip proto 89"

  A hello of r1 in area 0.0.0.0: hello interval 15, priority 100, and the
  authentication type 1 with "aaa" in clear in its header.

Important
  The NBMA interfaces "arc*" of area 120 in the guide, with their polled
  neighbours and the wait of 120 seconds, become one broadcast interface;
  "ppp*" and its cryptographic keys have no interface here.
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
from birdlib import bird_start, birdc, forwarding, has, lacks, opening, \
    ping, pings, proof, until, verdict

HISTORY = """\
OSPF was designed at the IETF by John Moy, RFC 1131 in 1989, OSPFv2 in RFC
2328: link states flooded inside an area, summaries between areas, and stub
areas that get a default route instead of the external routes. BIRD began
in 1998 as a student project at the Faculty of Mathematics and Physics of
Charles University in Prague, by Martin Mares, Pavel Machek and Ondrej
Filip; OSPF in BIRD was written by Ondrej Filip. CZ.NIC maintains BIRD since
2009.

Today: BIRD runs the route servers of most internet exchanges and many
operator networks; its OSPF is used wherever BIRD is the router.
"""

ROOTFS_CHOICES = ("bird_trixie",)
TOOL = "birdc"

R1, R2, R3, H3 = "r1", "r2", "r3", "h3"
ALL = (R1, R2, R3, H3)
ROUTERS = (R1, R2, R3)
L0, L120, LS = "lan0", "lan120", "lanS"

WIRING = {
    R2: [(0, L0, "192.168.0.2/24")],
    R1: [(0, L0, "192.168.0.1/24"), (1, L120, "192.168.120.1/24")],
    R3: [(0, L120, "192.168.120.3/24"), (1, LS, "172.16.1.1/24")],
    H3: [(0, LS, "172.16.1.10/24")],
}
GATEWAY = {H3: "172.16.1.1"}
RID = {R1: "192.168.0.1", R2: "192.168.0.2", R3: "192.168.120.3"}

# Every OSPF route to the kernel: the summary r2 learns and the default of
# the stub area are inter-area, RTS_OSPF_IA, not RTS_OSPF.
COMMON = """\
router id %s;
protocol device { }
protocol direct { ipv4; }
protocol kernel { ipv4 { export where source = RTS_OSPF || source = RTS_OSPF_IA
\t|| source = RTS_OSPF_EXT1 || source = RTS_OSPF_EXT2; }; }
"""

# The guide's protocol MyOSPF, "eth*" of area 0 made "eth0", area 120 on eth1.
R1_OSPF = """\
protocol ospf MyOSPF {
	ipv4 {
		export filter {
			if source = RTS_BGP then {
				ospf_metric1 = 100;
				accept;
			}
			reject;
		};
	};
	area 0.0.0.0 {
		interface "eth0" {
			cost 11;
			hello 15;
			priority 100;
			retransmit 7;
			authentication simple;
			password "aaa";
		};
	};
	area 120 {
		stub yes;
		networks {
			172.16.1.0/24;
			172.16.2.0/24 hidden;
		};
		interface "eth1";
	};
}
"""

R2_OSPF = """\
protocol ospf {
	area 0.0.0.0 {
		interface "eth0" {
			cost 11;
			hello 15;
			authentication simple;
			password "aaa";
		};
	};
}
"""

R3_OSPF = """\
protocol ospf {
	area 120 {
		stub yes;
		interface "eth0";
		interface "eth1" { stub yes; };
	};
}
"""

CONFS = {R1: R1_OSPF, R2: R2_OSPF, R3: R3_OSPF}

LAYOUT = {
    R2: (-480, -60), L0: (-260, -60), R1: (-40, -60), L120: (180, -60),
    R3: (400, -60), LS: (400, 100), H3: (400, 240),
}
ETHS = {
    (R2, "eth0"): 90, (R1, "eth0"): 270, (R1, "eth1"): 90,
    (R3, "eth0"): 270, (R3, "eth1"): 180, (H3, "eth0"): 0,
}

CARD = r"""
    r2 ---- r1 ---- r3 ---- h3      area 0 | area 120, stub
                     (ABR)

  An area border router that summarises one network, hides another, and
  feeds a stub area with a default route.
"""

def configure(node):
    forwarding(node)
    bird_start(node, COMMON % RID[node] + CONFS[node])

def full(node, count):
    return birdc(node, "show ospf neighbors").count("Full") >= count

def main():
    opening()
    ssh(R3, "ip addr add 172.16.2.1/24 dev eth1")

    step("act 1: the two adjacencies come up")
    in_parallel(configure, list(ROUTERS), "bird start")
    ok_adj = until(lambda: full(R1, 2), 120)
    proof("r1, its neighbours:", birdc(R1, "show ospf neighbors"))
    proof("r1, its interface of area 0.0.0.0:",
          birdc(R1, "show ospf interface MyOSPF"))
    hold()

    step("act 2: one network summarised, one hidden, a default for the stub")
    got = count_on(R2, 0, "ip proto 89", lambda: until(
        lambda: has(R2, "show route", ["172.16.1.0/24"]), 90), settle=1.0)
    ok_sum = got["out"] if got else has(R2, "show route", ["172.16.1.0/24"])
    ok_hidden = lacks(R2, "show route", ["172.16.2.0/24"])
    proof("r2, its routes:", birdc(R2, "show route"))
    ok_dflt = until(lambda: has(R3, "show route", ["0.0.0.0/0"]), 60)
    proof("r3, its routes, the default from r1:", birdc(R3, "show route"))
    until(lambda: pings(H3, "192.168.0.2"), 30)
    r = ping(H3, "192.168.0.2")
    proof("h3 -> r2:", r)
    ok_ping = reached(r)

    verdict([("both adjacencies are Full", ok_adj),
             ("r2 learns the summary 172.16.1.0/24", ok_sum),
             ("172.16.2.0/24, hidden, stays out of area 0", ok_hidden),
             ("r3 gets a default route in its stub area", ok_dflt),
             ("h3 reaches r2", ok_ping)])

    story("""
the network is left up. Things to try:

  the link state database of area 0:
    cloonix cli %s ssh r2 "birdc show ospf state"

  the summary LSAs of the ABR:
    cloonix cli %s ssh r1 "birdc show ospf lsadb"
""" % (NET, NET))

main()
