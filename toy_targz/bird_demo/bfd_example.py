#!/usr/bin/env python3
r"""bfd_example - BFD, Example.

    ./bfd_example.py [net]     the net defaults to nemo

The example of the BIRD User's Guide, chapter BFD, section "Example":
https://bird.nic.cz/doc/latest/bird-6.html#bfd-exam

    n10 (192.168.1.10, lo 192.168.10.1) ---- lan1 ---- r ---- lan2 ---- n2
                                                             (192.168.2.2)

  r carries the guide's protocol bfd: "eth*" with min rx 20 ms, min tx 50
  ms, idle tx 300 ms; the multihop defaults of 200 ms and 10; and its three
  neighbors, 192.168.1.10, 192.168.2.2 on its second interface, and
  192.168.10.1 multihop from 192.168.1.1. n10 and n2 answer with their own
  protocol bfd.

  ACT 1  the three sessions come up, two direct, one multihop.
  ACT 2  the cable of n2 is cut: its session goes Down in a fraction of a
         second, the two others stay Up.

Image: bird_trixie (bird2)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r eth0 sniff=on
    cloonix cli nemo sniff r eth0 --dump "udp port 3784 or udp port 4784"

  Two kinds of BFD packets on the same cable: port 3784 for the direct
  session with n10, port 4784 for the multihop one to its loopback.

Important
  The guide's "eth2" is eth1 here; its "gre*" interfaces do not exist, so
  their settings stay unused.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and birdlib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
import time
from demolib import NET, count_on, hold, in_parallel, ssh, step, story
from birdlib import bird_start, birdc, opening, proof, until, verdict

HISTORY = """\
Bidirectional Forwarding Detection was written by Dave Katz of Juniper and
Dave Ward of Cisco, RFC 5880 and RFC 5881 in 2010, with RFC 5883 for the
multihop sessions: tiny packets every few milliseconds, so that a routing
protocol learns of a failure without hellos of its own. The BFD of BIRD
was written by Ondrej Zajicek in 2013.

BIRD began in 1998 as a student project at the Faculty of Mathematics and
Physics of Charles University in Prague, by Martin Mares, Pavel Machek and
Ondrej Filip; CZ.NIC maintains it since 2009.

Today: BFD runs between almost every pair of routers in carrier and data
centre networks.
"""

ROOTFS_CHOICES = ("bird_trixie",)
TOOL = "birdc"

R, N10, N2 = "r", "n10", "n2"
ALL = (R, N10, N2)
L1, L2 = "lan1", "lan2"

WIRING = {
    R: [(0, L1, "192.168.1.1/24"), (1, L2, "192.168.2.1/24")],
    N10: [(0, L1, "192.168.1.10/24")],
    N2: [(0, L2, "192.168.2.2/24")],
}
GATEWAY = {}

# The guide's protocol bfd, "eth2" -> "eth1".
R_CONF = """\
router id 192.168.1.1;
protocol device { }
protocol static { ipv4; route 192.168.10.1/32 via 192.168.1.10; }
protocol kernel { ipv4 { export where source = RTS_STATIC; }; }
protocol bfd {
	interface "eth*" {
		min rx interval 20 ms;
		min tx interval 50 ms;
		idle tx interval 300 ms;
	};
	interface "gre*" {
		interval 200 ms;
		multiplier 10;
		passive;
	};
	multihop {
		interval 200 ms;
		multiplier 10;
	};

	neighbor 192.168.1.10;
	neighbor 192.168.2.2 dev "eth1";
	neighbor 192.168.10.1 local 192.168.1.1 multihop;
}
"""

N10_CONF = """\
router id 192.168.1.10;
protocol device { }
protocol bfd {
	neighbor 192.168.1.1;
	neighbor 192.168.1.1 local 192.168.10.1 multihop;
}
"""

N2_CONF = """\
router id 192.168.2.2;
protocol device { }
protocol bfd {
	neighbor 192.168.2.1;
}
"""

CONFS = {R: R_CONF, N10: N10_CONF, N2: N2_CONF}

LAYOUT = {N10: (-440, 0), L1: (-220, 0), R: (0, 0), L2: (220, 0),
          N2: (440, 0)}
ETHS = {(N10, "eth0"): 90, (R, "eth0"): 270, (R, "eth1"): 90,
        (N2, "eth0"): 270}

CARD = r"""
    n10 ---- r ---- n2

  Three BFD sessions from r: two direct, one multihop to a loopback.
"""

def configure(node):
    bird_start(node, CONFS[node])

def up(addr):
    for line in birdc(R, "show bfd sessions").split("\n"):
        if line.split()[:1] == [addr]:
            return "Up" in line.split()
    return False

def main():
    opening()
    ssh(N10, "ip addr add 192.168.10.1/32 dev lo")

    step("act 1: three sessions come up, two direct, one multihop")
    in_parallel(configure, list(ALL), "bird start")
    ok_up = until(lambda: up("192.168.1.10") and up("192.168.2.2") and
                  up("192.168.10.1"), 90)
    proof("r, its BFD sessions:", birdc(R, "show bfd sessions"))
    hold()

    step("act 2: the cable of n2 is cut, its session goes Down at once")
    t0 = time.time()
    got = count_on(R, 1, "udp port 3784", lambda: (
        ssh(N2, "ip link set eth0 down"),
        until(lambda: not up("192.168.2.2"), 20, period=0.2))[1], settle=0.5)
    gone = time.time() - t0
    ok_down = (got["out"] if got else not up("192.168.2.2")) and gone < 5
    story("    the session to n2 went Down %.1f seconds after the cut." % gone)
    story()
    ok_others = up("192.168.1.10") and up("192.168.10.1")
    proof("r, its BFD sessions:", birdc(R, "show bfd sessions"))

    verdict([("three sessions Up, one of them multihop", ok_up),
             ("the cut of n2 is seen in under 5 seconds", ok_down),
             ("the two other sessions stay Up", ok_others)])

    story("""
the network is left up. Things to try:

  the sessions with their timers:
    cloonix cli %s ssh r "birdc show bfd sessions all"

  the cable of n2 back:
    cloonix cli %s ssh n2 "ip link set eth0 up"
""" % (NET, NET))

main()
