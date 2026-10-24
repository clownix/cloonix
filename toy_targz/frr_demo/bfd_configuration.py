#!/usr/bin/env python3
r"""bfd_configuration - Configuration (BFD).

    ./bfd_configuration.py [net]     the net defaults to nemo

The example of the FRR User Guide, page bfd, section "Configuration":
"Here is an example of BFD configuration".
https://docs.frrouting.org/en/latest/bfd.html#bfd-configuration

    p1 (AS 65531, .1)    p2 (AS 65530, .2)    p3 (AS 65532, .3)
          \                    |                    /
           -------------- lan0 192.168.0.0/24 ------
                               |
                         r (AS 65530, .10)

  r carries the guide's configuration: a bfd peer 192.168.0.1, and BGP
  with "neighbor ... bfd" towards the three others, one of them internal.

  ACT 1  three BGP sessions, three BFD sessions up.
  ACT 2  the cable of p1 is cut: BFD sees it in about a second and BGP
         drops the session at once, not after the 180 seconds of its hold
         timer. p2 and p3 are untouched.
  ACT 3  the cable comes back, BFD and BGP come up again.

Image: frr_trixie (zebra, bfdd, bgpd)
Size:  4 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r eth0 sniff=on
    cloonix cli nemo sniff r eth0 --dump "udp port 3784"

  The BFD control packets, one every 300 ms each way per session; the one
  to catch is the last that p1 sends before the cut.
"""
import os
import sys
import time

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, frr_setup, hold, in_parallel, ssh, step, \
    story
from frrlib import has, opening, proof, until, verdict, vtysh

HISTORY = """\
Bidirectional Forwarding Detection was written by Dave Katz of Juniper and
Dave Ward of Cisco, RFC 5880 in 2010: one tiny protocol that checks a path
every few hundred milliseconds, so that BGP, OSPF or a static route need no
fast hellos of their own. The bfdd of frr was written by Rafael Zalamena
of Network Device Education Foundation, in 2018.

Today: BFD is turned on between almost every pair of routers in a carrier
or data centre network, wherever a failure must be seen in under a second.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

R, P1, P2, P3 = "r", "p1", "p2", "p3"
ALL = (R, P1, P2, P3)
PEERS = (P1, P2, P3)
L0 = "lan0"

WIRING = {
    R: [(0, L0, "192.168.0.10/24")],
    P1: [(0, L0, "192.168.0.1/24")],
    P2: [(0, L0, "192.168.0.2/24")],
    P3: [(0, L0, "192.168.0.3/24")],
}
GATEWAY = {}
ADDR = {P1: "192.168.0.1", P2: "192.168.0.2", P3: "192.168.0.3"}
ASN = {P1: "65531", P2: "65530", P3: "65532"}

# The guide's configuration of r.
R_CONF = """\
hostname r
bfd
 peer 192.168.0.1
   no shutdown
router bgp 65530
 no bgp ebgp-requires-policy
 neighbor 192.168.0.1 remote-as 65531
 neighbor 192.168.0.1 bfd
 neighbor 192.168.0.2 remote-as 65530
 neighbor 192.168.0.2 bfd
 neighbor 192.168.0.3 remote-as 65532
 neighbor 192.168.0.3 bfd"""

def peer_conf(node):
    return "\n".join([
        "hostname %s" % node,
        "router bgp %s" % ASN[node],
        " no bgp ebgp-requires-policy",
        " neighbor 192.168.0.10 remote-as 65530",
        " neighbor 192.168.0.10 bfd"])

CONFS = {R: R_CONF, P1: peer_conf(P1), P2: peer_conf(P2), P3: peer_conf(P3)}

LAYOUT = {P1: (-300, -220), P2: (0, -220), P3: (300, -220), L0: (0, 0),
          R: (0, 220)}
ETHS = {(P1, "eth0"): 135, (P2, "eth0"): 180, (P3, "eth0"): 225,
        (R, "eth0"): 0}

CARD = r"""
       p1        p2        p3
         \       |       /
          ----- lan0 -----
                 |
                 r

  BGP from r to p1, p2, p3, each session watched by BFD.
"""

def configure(node):
    frr_setup(node, ["bfdd", "bgpd"], CONFS[node])

def bgp_up(peer):
    return has(R, "show bgp neighbors %s" % ADDR[peer],
               ["BGP state = Established"])

def bfd_up(peer):
    return has(R, "show bfd peer %s" % ADDR[peer], ["Status: up"])

def all_up(peers):
    return all(bgp_up(p) and bfd_up(p) for p in peers)

def main():
    opening()

    step("act 1: three BGP sessions, each watched by a BFD session")
    in_parallel(configure, list(ALL), "frr start")
    ok_up = until(lambda: all_up(PEERS), 120)
    proof("r, its BFD peers:", vtysh(R, "show bfd peers brief"))
    proof("r, its BGP sessions:", vtysh(R, "show bgp summary"))
    hold()

    step("act 2: the cable of p1 is cut, BFD drops the BGP session at once")
    t0 = time.time()
    got = count_on(R, 0, "udp port 3784", lambda: (
        ssh(P1, "ip link set eth0 down"),
        until(lambda: not bgp_up(P1), 30, period=0.5))[1], settle=0.5)
    gone = time.time() - t0
    ok_fast = (got["out"] if got else not bgp_up(P1)) and gone < 15
    if got is not None:
        story("    %d BFD packet(s) on r eth0 around the cut." % got["count"])
    story("    the session to p1 went down %.1f seconds after the cut;" % gone)
    story("    the BGP hold timer alone would have waited 180 seconds.")
    story()
    proof("r, its BFD peers:", vtysh(R, "show bfd peers brief"))
    ok_others = all_up((P2, P3))
    proof("r, its BGP sessions:", vtysh(R, "show bgp summary"))
    hold()

    step("act 3: the cable comes back, BFD and BGP come up again")
    ssh(P1, "ip link set eth0 up")
    ssh(P1, "ip addr add 192.168.0.1/24 dev eth0", check=False)
    ok_back = until(lambda: all_up(PEERS), 120)
    proof("r, its BFD peers:", vtysh(R, "show bfd peers brief"))

    verdict([("three BGP and three BFD sessions up", ok_up),
             ("the cut of p1 drops its session in under 15 s", ok_fast),
             ("p2 and p3 stay up", ok_others),
             ("p1 comes back", ok_back)])

    story("""
the network is left up. Things to try:

  one BFD session in detail, its timers:
    cloonix cli %s ssh r "vtysh -c 'show bfd peer 192.168.0.1'"

  the counters of the BFD packets:
    cloonix cli %s ssh r "vtysh -c 'show bfd peers counters'"
""" % (NET, NET))

main()
