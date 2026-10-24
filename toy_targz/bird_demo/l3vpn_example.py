#!/usr/bin/env python3
r"""l3vpn_example - L3VPN, Example (needs: mpls_router, mpls_iptunnel, vrf).

    ./l3vpn_example.py [net]     the net defaults to nemo

The example of the BIRD User's Guide, chapter L3VPN, section "Example":
one VPN, a BGP uplink, the VPN routes in the kernel VRF vrf0, the traffic
carried by MPLS.
https://bird.nic.cz/doc/latest/bird-6.html#l3vpn-exam

    h1 -- lan1 -- pe1 (10.0.0.1) ==== core ==== pe2 (10.0.0.2) -- lan2 -- h2
    10.1.0.0/24   [vrf0]           AS 10, MPLS            [vrf0]   10.2.0.0/24

  !!! KERNEL MODULES NEEDED ON THE HOST: mpls_router, mpls_iptunnel, vrf !!!
  The VPN lives in a linux VRF and its traffic is label-switched by the
  kernel, and a container cannot load a module. Before playing this demo,
  on the host, once per boot:

sudo modprobe mpls_router
sudo modprobe mpls_iptunnel
sudo modprobe vrf

  or do not run this demo. It stops with a big warning if a module is
  missing.

  pe1 carries the guide's configuration word for word: the MPLS domain and
  table, the vpn4 and vpn6 tables exchanged over iBGP in AS 10 with
  aggregate labels, the tables vrf0v4 and vrf0v6 mapped to the kernel VRF
  vrf0, table 100, and protocol l3vpn with rd 10:12 and its import and
  export targets. pe2 is its mirror, rd 10:22, its targets chosen to match.
  What the guide leaves out, the routes of the customer, is a direct
  protocol in vrf0.

  ACT 1  the iBGP session comes up and carries the customer lans as vpn4
         routes with their labels.
  ACT 2  each PE installs the lan of the other in vrf0, with an MPLS
         encapsulation; h1 reaches h2, labelled across the core.

Image: bird_trixie (bird2)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf pe1 eth0 sniff=on
    cloonix cli nemo sniff pe1 eth0 --dump "mpls"

  An echo of h1 crossing the core: ethertype 0x8847, the label of the VPN,
  then the IP header of h1.

Important
  The two PEs are neighbours on the core, so no transport label is needed
  and the guide's "MPLS-aware IGP" is a direct protocol; the kernel
  forwarding of MPLS is turned on in each PE. The BGP channels the guide
  writes "vpn4" and "vpn6" are "vpn4 mpls" and "vpn6 mpls" for the bird
  2.17 of the image, which refuses the short form.
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
from birdlib import bird_start, birdc, has, opening, ping, pings, proof, \
    until, verdict

HISTORY = """\
BGP/MPLS IP VPNs, RFC 2547 in 1999 by Eric Rosen and Yakov Rekhter, now
RFC 4364, let a carrier keep the routes of many customers apart, each in
its own VRF, distinguished by a route distinguisher and filtered by route
targets, the traffic carried by labels. The L3VPN of BIRD came in 2.15, in
2024, by Ondrej Zajicek, on the VRF devices of linux.

BIRD began in 1998 as a student project at the Faculty of Mathematics and
Physics of Charles University in Prague, by Martin Mares, Pavel Machek and
Ondrej Filip; CZ.NIC maintains it since 2009.

Today: L3VPN is the business VPN most carriers sell.
"""

ROOTFS_CHOICES = ("bird_trixie",)
TOOL = "birdc"

PE1, PE2, H1, H2 = "pe1", "pe2", "h1", "h2"
ALL = (PE1, PE2, H1, H2)
PES = (PE1, PE2)
L1, CORE, L2 = "lan1", "core", "lan2"

WIRING = {
    PE1: [(0, CORE, "10.0.0.1/24"), (1, L1, "10.1.0.1/24")],
    PE2: [(0, CORE, "10.0.0.2/24"), (1, L2, "10.2.0.1/24")],
    H1: [(0, L1, "10.1.0.10/24")],
    H2: [(0, L2, "10.2.0.10/24")],
}
GATEWAY = {H1: "10.1.0.1", H2: "10.2.0.1"}
SIDE = {PE1: ("10.0.0.1", "10.0.0.2", "10:12", "(rt, 10, 32..40)",
              "(rt, 10, 30), (rt, 10, 31)"),
        PE2: ("10.0.0.2", "10.0.0.1", "10:22", "(rt, 10, 30..31)",
              "(rt, 10, 32), (rt, 10, 33)")}

def pe_conf(node):
    me, peer, rd, imp, exp = SIDE[node]
    return """\
router id %s;
protocol device { }
protocol direct { ipv4; interface "eth0"; }

# MPLS basics
mpls domain mdom;
mpls table  mtab;

protocol kernel krt_mpls {
	mpls { table mtab; export all; };
}

vpn4 table vpntab4;
vpn6 table vpntab6;

# Exchange VPN routes through BGP
protocol bgp {
	vpn4 mpls { table vpntab4; import all; export all; };
	vpn6 mpls { table vpntab6; import all; export all; };
	mpls { label policy aggregate; };
	local %s as 10;
	neighbor %s as 10;
}

# VRF 0
ipv4 table vrf0v4;
ipv6 table vrf0v6;

protocol direct direct0 {
	vrf "vrf0";
	ipv4 { table vrf0v4; };
}

protocol kernel kernel0v4 {
	vrf "vrf0";
	ipv4 { table vrf0v4; export all; };
	kernel table 100;
}

protocol kernel kernel0v6 {
	vrf "vrf0";
	ipv6 { table vrf0v6; export all; };
	kernel table 100;
}

protocol l3vpn l3vpn0 {
	vrf "vrf0";
	ipv4 { table vrf0v4; };
	ipv6 { table vrf0v6; };
	vpn4 { table vpntab4; };
	vpn6 { table vpntab6; };
	mpls { label policy vrf; };

	rd %s;
	import target [%s];
	export target [%s];
}
""" % (me, me, peer, rd, imp, exp)

LAYOUT = {
    H1: (-560, 0), L1: (-420, 0), PE1: (-240, 0), CORE: (0, 0),
    PE2: (240, 0), L2: (420, 0), H2: (560, 0),
}
ETHS = {
    (H1, "eth0"): 90, (PE1, "eth1"): 270, (PE1, "eth0"): 90,
    (PE2, "eth0"): 270, (PE2, "eth1"): 90, (H2, "eth0"): 270,
}

CARD = r"""
    h1 -- pe1 [vrf0] ==== MPLS core ==== [vrf0] pe2 -- h2

  One VPN, rd and route targets, its routes in a linux VRF, its traffic
  labelled.
"""

def configure(node):
    addr = WIRING[node][1][2]
    ssh(node, "sysctl -w net.ipv4.ip_forward=1 "
              "net.mpls.platform_labels=1048575 "
              "net.mpls.conf.eth0.input=1 >/dev/null")
    ssh(node, "ip link add vrf0 type vrf table 100")
    ssh(node, "ip link set vrf0 up")
    ssh(node, "ip link set eth1 master vrf0")
    ssh(node, "ip addr add %s dev eth1" % addr, check=False)
    bird_start(node, pe_conf(node))

def main():
    opening("mpls_router", "mpls_iptunnel", "vrf")

    step("act 1: iBGP carries the customer lans as vpn4 routes")
    in_parallel(configure, list(PES), "bird start")
    ok_vpn = until(lambda: has(PE1, "show route table vpntab4",
                               ["10:22 10.2.0.0/24"]), 120)
    proof("pe1, the table vpntab4:", birdc(PE1, "show route table vpntab4 all"))
    hold()

    step("act 2: the remote lan in vrf0, labelled; h1 reaches h2")
    ok_vrf = until(lambda: "encap mpls" in ssh(
        PE1, "ip route show table 100 10.2.0.0/24", check=False), 60)
    proof("pe1, kernel table 100 (vrf0):",
          ssh(PE1, "ip route show table 100", check=False))
    proof("pe2, its MPLS table:", ssh(PE2, "ip -f mpls route show",
                                      check=False))
    got = count_on(PE1, 0, "mpls", lambda: until(
        lambda: pings(H1, "10.2.0.10"), 30), settle=1.0)
    r = ping(H1, "10.2.0.10")
    proof("h1 -> h2:", r)
    ok_ping = reached(r)
    ok_lab = bool(got and got["count"] > 0)

    verdict([("pe1 holds the vpn4 route of pe2's lan", ok_vpn),
             ("vrf0 of pe1 routes 10.2.0.0/24 with MPLS", ok_vrf),
             ("h1 reaches h2", ok_ping),
             ("the packets cross the core labelled", ok_lab)])

    story("""
the network is left up. Things to try:

  the labels BIRD allocated:
    cloonix cli %s ssh pe2 "birdc show route table mtab"

  the VRF of a PE:
    cloonix cli %s ssh pe1 "ip route show vrf vrf0"
""" % (NET, NET))

main()
