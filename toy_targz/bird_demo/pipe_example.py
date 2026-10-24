#!/usr/bin/env python3
r"""pipe_example - Pipe, Example.

    ./pipe_example.py [net]     the net defaults to nemo

The example of the BIRD User's Guide, chapter Pipe, section "Example": one
router, boundary of two autonomous systems, two routing tables, and a pipe
between them that passes each AS the routes of the other as a backup.
https://bird.nic.cz/doc/latest/bird-6.html#pipe-exam

    up1 (AS 1001, 192.168.0.1) ---- lan1 ----+
                                             r (tables as1, as2)
    up2 (AS 1002, 10.0.0.1)    ---- lan2 ----+

  r carries the guide's configuration word for word: tables as1 and as2,
  kernel tables 1 and 2, bgp1 in AS 1 towards 192.168.0.1 (AS 1001), bgp2
  in AS 2 towards 10.0.0.1 (AS 1002), and the pipe whose export lets
  1.0.0.0/8+ into as2 and whose import lets 2.0.0.0/8+ into as1, each with
  its preference lowered by 10 and its BGP path prepended.

  ACT 1  up1 announces 1.1.0.0/16, up2 announces 2.2.0.0/16; each lands in
         its own table.
  ACT 2  through the pipe, as2 holds 1.1.0.0/16 with the path "1 1001" and
         a preference of 90, and as1 holds 2.2.0.0/16 with "2 1002"; the
         kernel tables 1 and 2 follow.

Image: bird_trixie (bird2)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r eth0 sniff=on
    cloonix cli nemo sniff r eth0 --dump "tcp port 179"

  The UPDATE of up1 carrying 1.1.0.0/16: what the pipe later copies into
  the other table, inside r, never goes on any wire.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and birdlib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, hold, in_parallel, ssh, step, story
from birdlib import bird_start, birdc, has, opening, proof, until, verdict

HISTORY = """\
BIRD was the first routing daemon built around several routing tables from
the start, at Charles University in Prague in 1998, by Martin Mares,
Pavel Machek and Ondrej Filip; the pipe protocol, which passes routes
between two tables through filters, is as old. CZ.NIC maintains BIRD since
2009.

Today: tables and pipes are how BIRD route servers keep one table per
peer, and how a router serves several customers apart.
"""

ROOTFS_CHOICES = ("bird_trixie",)
TOOL = "birdc"

R, UP1, UP2 = "r", "up1", "up2"
ALL = (R, UP1, UP2)
L1, L2 = "lan1", "lan2"

WIRING = {
    R: [(0, L1, "192.168.0.2/24"), (1, L2, "10.0.0.2/24")],
    UP1: [(0, L1, "192.168.0.1/24")],
    UP2: [(0, L2, "10.0.0.1/24")],
}
GATEWAY = {}

# The guide's configuration.
R_CONF = """\
router id 192.168.0.2;
protocol device { }
ipv4 table as1;				# Define the tables
ipv4 table as2;

protocol kernel kern1 {			# Synchronize them with the kernel
	ipv4 { table as1; export all; };
	kernel table 1;
}

protocol kernel kern2 {
	ipv4 { table as2; export all; };
	kernel table 2;
}

protocol bgp bgp1 {			# The outside connections
	ipv4 { table as1; import all; export all; };
	local as 1;
	neighbor 192.168.0.1 as 1001;
}

protocol bgp bgp2 {
	ipv4 { table as2; import all; export all; };
	local as 2;
	neighbor 10.0.0.1 as 1002;
}

protocol pipe {				# The Pipe
	table as1;
	peer table as2;
	export filter {
		if net ~ [ 1.0.0.0/8+] then {	# Only AS1 networks
			if preference>10 then preference = preference-10;
			if source=RTS_BGP then bgp_path.prepend(1);
			accept;
		}
		reject;
	};
	import filter {
		if net ~ [ 2.0.0.0/8+] then {	# Only AS2 networks
			if preference>10 then preference = preference-10;
			if source=RTS_BGP then bgp_path.prepend(2);
			accept;
		}
		reject;
	};
}
"""

def up_conf(rid, asn, peer, peer_as, prefix):
    return """\
router id %s;
protocol device { }
protocol static { ipv4; route %s blackhole; }
protocol bgp {
	local as %s;
	neighbor %s as %s;
	ipv4 { import all; export all; };
}
""" % (rid, prefix, asn, peer, peer_as)

CONFS = {R: R_CONF,
         UP1: up_conf("192.168.0.1", 1001, "192.168.0.2", 1, "1.1.0.0/16"),
         UP2: up_conf("10.0.0.1", 1002, "10.0.0.2", 2, "2.2.0.0/16")}

LAYOUT = {UP1: (-420, -130), L1: (-200, -130), R: (40, 0),
          UP2: (-420, 130), L2: (-200, 130)}
ETHS = {(UP1, "eth0"): 90, (UP2, "eth0"): 90, (R, "eth0"): 315,
        (R, "eth1"): 225}

CARD = r"""
    up1 (AS 1001) --+
                    +-- r   table as1 <== pipe ==> table as2
    up2 (AS 1002) --+

  One router, two logical routers, each the backup of the other.
"""

def configure(node):
    bird_start(node, CONFS[node])

def main():
    opening()

    step("act 1: each upstream fills its own table")
    got = count_on(R, 0, "tcp port 179", lambda: (
        in_parallel(configure, list(ALL), "bird start"),
        until(lambda: has(R, "show route table as1", ["1.1.0.0/16"]) and
              has(R, "show route table as2", ["2.2.0.0/16"]), 90))[1],
        settle=1.0)
    ok_own = got["out"] if got else \
        has(R, "show route table as1", ["1.1.0.0/16"])
    proof("r, its protocols:", birdc(R, "show protocols"))
    hold()

    step("act 2: through the pipe, each table holds the other's routes")
    ok_12 = until(lambda: has(R, "show route 1.1.0.0/16 table as2 all",
                              ["1 1001", "(90)"]), 60)
    ok_21 = until(lambda: has(R, "show route 2.2.0.0/16 table as1 all",
                              ["2 1002", "(90)"]), 60)
    proof("r, table as2, the route of AS 1001 through the pipe:",
          birdc(R, "show route 1.1.0.0/16 table as2 all"))
    proof("r, table as1:", birdc(R, "show route table as1"))
    k1 = ssh(R, "ip route show table 1", check=False)
    k2 = ssh(R, "ip route show table 2", check=False)
    proof("r, kernel table 1:", k1)
    proof("r, kernel table 2:", k2)
    ok_kernel = "2.2.0.0/16" in k1 and "1.1.0.0/16" in k2

    verdict([("each upstream fills its own table", ok_own),
             ("as2 holds 1.1.0.0/16, path 1 1001, preference 90", ok_12),
             ("as1 holds 2.2.0.0/16, path 2 1002, preference 90", ok_21),
             ("the kernel tables 1 and 2 follow", ok_kernel)])

    story("""
the network is left up. Things to try:

  the pipe and what it passed each way:
    cloonix cli %s ssh r "birdc show protocols all pipe1"

  both tables at once:
    cloonix cli %s ssh r "birdc show route table all"
""" % (NET, NET))

main()
