#!/usr/bin/env python3
r"""bgp_conditional_advertisement - BGP Conditional Advertisement.

    ./bgp_conditional_advertisement.py [net]     the net defaults to nemo

The example of the FRR User Guide, page bgp, section "BGP Conditional
Advertisement", its "Sample Configuration" and "Sample Output":
https://docs.frrouting.org/en/latest/bgp.html#bgp-conditional-advertisement

    r1 (AS 1) ---- 10.10.10.0/24 ---- r2 (AS 2) ---- 10.10.20.0/24 ---- r3 (AS 3)

  r1 announces 0.0.0.0/0 (community 65013:200), 10.139.224.0/20,
  192.0.2.1/32 (community 64952:3008) and 192.0.2.5/32. r2 carries the
  configuration of the guide word for word: towards r3, the routes of
  ADV-MAP are announced only while EXIST-MAP, the default route, is absent.

  ACT 1  the default route is there: r3 gets 0.0.0.0/0 and 192.0.2.5/32,
         not 10.139.224.0/20 nor 192.0.2.1/32 (status Withdraw).
  ACT 2  r1 stops announcing the default route: r2 announces the two
         others to r3 (status Advertise).
  ACT 3  the default route comes back: the two are withdrawn again.

Image: frr_trixie (zebra, staticd, bgpd)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r3 eth0 sniff=on
    cloonix cli nemo sniff r3 eth0 --dump "tcp port 179"

  The UPDATE r2 sends to r3 at the end of ACT 2: two prefixes appear, the
  scanner of r2 having seen the default route disappear.

Important
  The guide's interfaces enp0s9 and enp0s10 are eth0 and eth1 here. The
  scanner runs every 60 seconds by default; "bgp conditional-advertisement
  timer 5", a command of the same section, makes the demo shorter.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, frr_setup, hold, in_parallel, step, story
from frrlib import conf, forwarding, has, lacks, opening, proof, until, \
    verdict, vtysh

HISTORY = """\
BGP was sketched by Kirk Lougheed of Cisco and Yakov Rekhter of IBM on
three napkins at the IETF of January 1989, RFC 1105. Conditional
advertisement came from the operators: announce a backup path only when the
primary one is gone. It reached frr in 2021, written by Madhuri Kuruganti of
VMware, in the bgpd that descends from GNU Zebra, the work of Kunihiro
Ishiguro started in 1996.

Today: frr carries the routes of data centres, internet exchanges and
cloud providers; conditional advertisement is the classic way to keep a
backup link silent while the primary link works.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

R1, R2, R3 = "r1", "r2", "r3"
ALL = (R1, R2, R3)
L12, L23 = "lan12", "lan23"

WIRING = {
    R1: [(0, L12, "10.10.10.1/24")],
    R2: [(0, L12, "10.10.10.2/24"), (1, L23, "10.10.20.2/24")],
    R3: [(0, L23, "10.10.20.3/24")],
}
GATEWAY = {}

R1_CONF = """\
hostname r1
ip route 0.0.0.0/0 blackhole
ip route 10.139.224.0/20 blackhole
ip route 192.0.2.1/32 blackhole
ip route 192.0.2.5/32 blackhole
ip prefix-list DEFAULT-ROUTE seq 5 permit 0.0.0.0/0
ip prefix-list DC seq 5 permit 192.0.2.1/32
ip prefix-list IP1 seq 5 permit 10.139.224.0/20
route-map SET-COMM permit 10
 match ip address prefix-list DEFAULT-ROUTE
 set community 65013:200
route-map SET-COMM permit 20
 match ip address prefix-list DC
 set community 64952:3008
route-map SET-COMM permit 30
route-map ONLY-IP1 permit 10
 match ip address prefix-list IP1
router bgp 1
 no bgp ebgp-requires-policy
 neighbor 10.10.10.2 remote-as 2
 address-family ipv4 unicast
  network 0.0.0.0/0 route-map SET-COMM
  network 192.0.2.1/32 route-map SET-COMM
  network 192.0.2.5/32
  redistribute static route-map ONLY-IP1
 exit-address-family"""

# The guide's configuration of Router2.
R2_CONF = """\
hostname r2
interface lo
 ip address 203.0.113.1/32
router bgp 2
 bgp log-neighbor-changes
 no bgp ebgp-requires-policy
 bgp conditional-advertisement timer 5
 neighbor 10.10.10.1 remote-as 1
 neighbor 10.10.20.3 remote-as 3
 address-family ipv4 unicast
  neighbor 10.10.10.1 soft-reconfiguration inbound
  neighbor 10.10.20.3 soft-reconfiguration inbound
  neighbor 10.10.20.3 advertise-map ADV-MAP non-exist-map EXIST-MAP
 exit-address-family
ip prefix-list DEFAULT seq 5 permit 192.0.2.5/32
ip prefix-list DEFAULT seq 10 permit 192.0.2.1/32
ip prefix-list EXIST seq 5 permit 10.10.10.10/32
ip prefix-list DEFAULT-ROUTE seq 5 permit 0.0.0.0/0
ip prefix-list IP1 seq 5 permit 10.139.224.0/20
bgp community-list standard DC-ROUTES seq 5 permit 64952:3008
bgp community-list standard DC-ROUTES seq 10 permit 64671:501
bgp community-list standard DC-ROUTES seq 15 permit 64950:3009
bgp community-list standard DEFAULT-ROUTE seq 5 permit 65013:200
route-map ADV-MAP permit 10
 match ip address prefix-list IP1
route-map ADV-MAP permit 20
 match community DC-ROUTES
route-map EXIST-MAP permit 10
 match community DEFAULT-ROUTE
 match ip address prefix-list DEFAULT-ROUTE"""

R3_CONF = """\
hostname r3
router bgp 3
 no bgp ebgp-requires-policy
 neighbor 10.10.20.2 remote-as 2"""

CONFS = {R1: R1_CONF, R2: R2_CONF, R3: R3_CONF}
ALWAYS = ("0.0.0.0/0", "192.0.2.5/32")
COND = ("10.139.224.0/20", "192.0.2.1/32")
ALL_FOUR = ALWAYS + COND
ADV = "show ip bgp neighbors 10.10.20.3 advertised-routes"

LAYOUT = {
    R1: (-440, 0), L12: (-220, 0), R2: (0, 0), L23: (220, 0), R3: (440, 0),
}
ETHS = {
    (R1, "eth0"): 90, (R2, "eth0"): 270, (R2, "eth1"): 90, (R3, "eth0"): 270,
}

CARD = r"""
    r1 (AS 1) ---- r2 (AS 2) ---- r3 (AS 3)

  r2 announces 10.139.224.0/20 and 192.0.2.1/32 to r3 only while the
  default route from r1 is absent: advertise-map with a non-exist-map.
"""

def configure(node):
    forwarding(node)
    frr_setup(node, ["staticd", "bgpd"], CONFS[node])

def status(word):
    return word in vtysh(R2, "show ip bgp neighbors 10.10.20.3")

def main():
    opening()

    step("act 1: the default route is there, two routes are kept from r3")
    in_parallel(configure, list(ALL), "frr start")
    ok_r2 = until(lambda: has(R2, "show ip bgp", ALL_FOUR), 90)
    proof("r2, its BGP table, the four routes of r1:",
          vtysh(R2, "show ip bgp"))
    ok_wd = until(lambda: has(R3, "show ip bgp", ALWAYS) and
                  lacks(R3, "show ip bgp", COND) and status("Withdraw"), 90)
    proof("r2, what it announces to r3:", vtysh(R2, ADV))
    proof("r3, its BGP table:", vtysh(R3, "show ip bgp"))
    story("    The default route is in the table of r2: EXIST-MAP matches,")
    story("    the non-exist condition is false, ADV-MAP routes are held.")
    hold()

    step("act 2: r1 withdraws the default route, r2 announces the two others")
    got = count_on(R3, 0, "tcp port 179", lambda: (
        conf(R1, "router bgp 1", "address-family ipv4 unicast",
             "no network 0.0.0.0/0 route-map SET-COMM"),
        until(lambda: has(R3, "show ip bgp", COND), 90))[1], settle=2.0)
    ok_adv = bool(got and got["out"]) and status("Advertise")
    if got is not None:
        story("    %d BGP packet(s) seen on r3 eth0 during the change."
              % got["count"])
    proof("r2, what it announces to r3 now:", vtysh(R2, ADV))
    proof("r3, its BGP table:", vtysh(R3, "show ip bgp"))
    hold()

    step("act 3: the default route comes back, the two are withdrawn")
    conf(R1, "router bgp 1", "address-family ipv4 unicast",
         "network 0.0.0.0/0 route-map SET-COMM")
    ok_back = until(lambda: has(R3, "show ip bgp", ALWAYS) and
                    lacks(R3, "show ip bgp", COND), 90)
    proof("r3, its BGP table:", vtysh(R3, "show ip bgp"))

    verdict([("r2 learns the four routes of r1", ok_r2),
             ("default present: r3 lacks the ADV-MAP routes", ok_wd),
             ("default absent: r3 gets the ADV-MAP routes", ok_adv),
             ("default back: the ADV-MAP routes are withdrawn", ok_back)])

    story("""
the network is left up. Things to try:

  the condition and its status, on r2:
    cloonix cli %s ssh r2 "vtysh -c 'show ip bgp neighbors 10.10.20.3'"

  what r2 announces to r3:
    cloonix cli %s ssh r2 "vtysh -c '%s'"
""" % (NET, NET, ADV))

main()
