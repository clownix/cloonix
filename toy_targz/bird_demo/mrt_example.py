#!/usr/bin/env python3
r"""mrt_example - MRT, Example.

    ./mrt_example.py [net]     the net defaults to nemo

The example of the BIRD User's Guide, chapter MRT, section "Example":
https://bird.nic.cz/doc/latest/bird-6.html#mrt-exam

    r (AS 65000, table tab1) ---- lan0 ---- p (AS 65001)

  r carries the guide's protocol mrt word for word: the tables "tab*", the
  BGP routes only, a file name built from the protocol, the table and the
  time, a dump every 300 seconds. Its BGP session to p fills tab1.

  ACT 1  p announces two prefixes into tab1 of r.
  ACT 2  a dump is asked at once with "mrt dump", the command of the same
         chapter: the file is in MRT format, type 13, TABLE_DUMP_V2, the
         format of the RouteViews and RIPE RIS archives.

Image: bird_trixie (bird2)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r eth0 sniff=on
    cloonix cli nemo sniff r eth0 --dump "tcp port 179"

  The UPDATE of p whose two prefixes end up in the dump file.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and birdlib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, hold, ssh, step, story
from birdlib import bird_start, birdc, has, opening, proof, until, verdict

HISTORY = """\
MRT, the format of Merit Network for routing data, was written down in RFC
6396 in 2011 by Larry Blunk, Manish Karir and Craig Labovitz; the
University of Oregon RouteViews project and RIPE RIS publish the tables of
the internet in it since the end of the 1990s. MRT dumps in BIRD came in
2.0.8, by Ondrej Zajicek.

BIRD began in 1998 as a student project at the Faculty of Mathematics and
Physics of Charles University in Prague, by Martin Mares, Pavel Machek and
Ondrej Filip; CZ.NIC maintains it since 2009.

Today: MRT archives are how the history of the internet routing table is
studied, hijacks and leaks included.
"""

ROOTFS_CHOICES = ("bird_trixie",)
TOOL = "birdc"

R, P = "r", "p"
ALL = (R, P)
L0 = "lan0"

WIRING = {
    R: [(0, L0, "10.0.0.1/24")],
    P: [(0, L0, "10.0.0.2/24")],
}
GATEWAY = {}
DUMP = "/var/log/bird/now.mrt"

# The guide's protocol mrt, with the table tab1 a BGP session fills.
R_CONF = """\
router id 10.0.0.1;
protocol device { }
ipv4 table tab1;
protocol bgp {
	local as 65000;
	neighbor 10.0.0.2 as 65001;
	ipv4 { table tab1; import all; export none; };
}

protocol mrt {
	table "tab*";
	where source = RTS_BGP;
	filename "/var/log/bird/%N_%F_%T.mrt";
	period 300;
}
"""

P_CONF = """\
router id 10.0.0.2;
protocol device { }
protocol static { ipv4; route 192.0.2.0/24 blackhole;
                  route 203.0.113.0/24 blackhole; }
protocol bgp {
	local as 65001;
	neighbor 10.0.0.1 as 65000;
	ipv4 { import none; export all; };
}
"""

LAYOUT = {R: (-260, 0), L0: (0, 0), P: (260, 0)}
ETHS = {(R, "eth0"): 90, (P, "eth0"): 270}

CARD = r"""
    r (table tab1, protocol mrt) ---- p

  The BGP table of r written to a file in the format of the internet
  routing archives.
"""

def main():
    opening()

    step("act 1: p fills the table tab1 of r")
    bird_start(R, R_CONF)
    bird_start(P, P_CONF)
    ok_tab = until(lambda: has(R, "show route table tab1",
                               ["192.0.2.0/24", "203.0.113.0/24"]), 90)
    proof("r, the table tab1:", birdc(R, "show route table tab1"))
    hold()

    step("act 2: a dump at once, the file is MRT TABLE_DUMP_V2")
    ssh(R, "birdc 'mrt dump table \"tab1\" to \"%s\"'" % DUMP, check=False)
    ok_file = until(lambda: ssh(R, "test -s %s && echo yes" % DUMP,
                                check=False).strip() == "yes", 30)
    mtype = ssh(R, "od -An -tu2 -j4 -N2 --endian=big %s" % DUMP,
                check=False).strip()
    proof("r, the dump file:", ssh(R, "ls -l %s" % DUMP, check=False))
    story("    MRT type of its first record: %s (13 is TABLE_DUMP_V2)" % mtype)
    story()

    verdict([("tab1 holds the two prefixes of p", ok_tab),
             ("the dump file is written", ok_file),
             ("its records are MRT type 13", mtype == "13")])

    story("""
the network is left up. Things to try:

  the periodic dumps of the protocol, every 300 seconds:
    cloonix cli %s ssh r "ls -l /var/log/bird"

  the mrt protocol and its next dump:
    cloonix cli %s ssh r "birdc show protocols all mrt1"
""" % (NET, NET))

main()
