#!/usr/bin/env python3
r"""kernel_example - Kernel, Example.

    ./kernel_example.py [net]     the net defaults to nemo

The example of the BIRD User's Guide, chapter Kernel, section "Example":
its "system with two routing tables".
https://bird.nic.cz/doc/latest/bird-6.html#krt-exam

    r (10.0.0.1) ---- lan0 10.0.0.0/24 ---- g (10.0.0.2)

  r carries the guide's two kernel protocols word for word: the primary
  table learns the alien routes, persists, scans every 10 seconds, imports
  and exports all; the secondary is kernel table 100, fed from auxtable.

  ACT 1  learn: a route added by hand with ip route, outside BIRD, shows in
         BIRD within the scan time.
  ACT 2  the secondary table: a static route of auxtable lands in kernel
         table 100, not in the main table.
  ACT 3  persist: bird is stopped, the route it exported stays in the
         kernel.

Image: bird_trixie (bird2)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
  None: everything here is between BIRD and the kernel of r, by netlink.
  A ping from r to 10.7.0.1 shows the route at work:
    cloonix cli nemo cnf r eth0 sniff=on
    cloonix cli nemo sniff r eth0 --dump "icmp"
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
from birdlib import bird_start, bird_stop, birdc, has, opening, proof, \
    until, verdict

HISTORY = """\
The linux kernel has several routing tables since 2.2 in 1999, the work of
Alexey Kuznetsov, and netlink to talk to them. BIRD speaks netlink to keep
its tables and the kernel's in step: what it exports is written, what it
learns is read, and what it leaves on exit is chosen.

BIRD began in 1998 as a student project at the Faculty of Mathematics and
Physics of Charles University in Prague, by Martin Mares, Pavel Machek and
Ondrej Filip; CZ.NIC maintains it since 2009.

Today: every BIRD router uses the kernel protocol; learn, persist and the
extra tables are what policy routing setups are made of.
"""

ROOTFS_CHOICES = ("bird_trixie",)
TOOL = "birdc"

R, G = "r", "g"
ALL = (R, G)
L0 = "lan0"

WIRING = {
    R: [(0, L0, "10.0.0.1/24")],
    G: [(0, L0, "10.0.0.2/24")],
}
GATEWAY = {}

# The guide's two kernel protocols, plus the routes they carry.
R_CONF = """\
router id 10.0.0.1;
protocol device { }
ipv4 table auxtable;

protocol static { ipv4; route 10.7.0.0/16 via 10.0.0.2; }
protocol static { ipv4 { table auxtable; }; route 10.8.0.0/16 via 10.0.0.2; }

protocol kernel {		# Primary routing table
	learn;			# Learn alien routes from the kernel
	persist;		# Do not remove routes on bird shutdown
	scan time 10;		# Scan kernel routing table every 10 seconds
	ipv4 {
		import all;
		export all;
	};
}

protocol kernel {		# Secondary routing table
	kernel table 100;
	ipv4 {
		table auxtable;
		export all;
	};
}
"""

LAYOUT = {R: (-260, 0), L0: (0, 0), G: (260, 0)}
ETHS = {(R, "eth0"): 90, (G, "eth0"): 270}

CARD = r"""
    r ---- lan0 ---- g

  BIRD and the kernel of r: learn, a second table, persist.
"""

def main():
    opening()
    ssh(G, "ip addr add 10.7.0.1/32 dev lo")

    step("act 1: a route added by hand is learnt by BIRD")
    bird_start(R, R_CONF)
    ssh(R, "ip route add 10.9.0.0/16 via 10.0.0.2")
    ok_learn = until(lambda: has(R, "show route 10.9.0.0/16", ["kernel1"]),
                     40)
    proof("r, BIRD sees the alien route:", birdc(R, "show route"))
    hold()

    step("act 2: auxtable lands in kernel table 100")
    t100 = ssh(R, "ip route show table 100", check=False)
    main_t = ssh(R, "ip route show", check=False)
    ok_aux = "10.8.0.0/16" in t100 and "10.8.0.0/16" not in main_t
    proof("r, kernel table 100:", t100)
    proof("r, the main table:", main_t)
    hold()

    step("act 3: bird stops, its routes stay in the kernel")
    bird_stop(R)
    ok_gone = until(lambda: "Daemon is up" not in birdc(R, "show status"),
                    20)
    after = ssh(R, "ip route show", check=False)
    ok_persist = ok_gone and "10.7.0.0/16" in after
    proof("r, the main table with bird stopped:", after)
    r_out = ssh(R, "ping -c 3 -W 1 10.7.0.1", check=False)
    ok_ping = " 0% packet loss" in r_out

    verdict([("an alien route is learnt by BIRD", ok_learn),
             ("auxtable is in kernel table 100 only", ok_aux),
             ("bird stopped, its route persists", ok_persist),
             ("and still carries traffic", ok_ping)])

    story("""
the network is left up, bird stopped on r. Things to try:

  bird started again, it takes its routes back:
    cloonix cli %s ssh r "bird -c /etc/bird/bird.conf"

  the routes of table 100:
    cloonix cli %s ssh r "ip route show table 100"
""" % (NET, NET))

main()
