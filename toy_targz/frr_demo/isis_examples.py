#!/usr/bin/env python3
r"""isis_examples - ISIS Configuration Examples.

    ./isis_examples.py [net]     the net defaults to nemo

The first example of the FRR User Guide, page isisd, section "ISIS
Configuration Examples": "A simple example, with MD5 authentication
enabled".
https://docs.frrouting.org/en/latest/isisd.html#isis-config-examples

    h1 ---- lan1 ---- r1 ==== lan12 ==== r2 ---- lan2 ---- h2
       10.1.0.0/24        point-to-point        10.2.0.0/24

  r1 and r2 carry the guide's configuration on their eth0: area 47.0023,
  level-2-only, wide metrics, a point-to-point circuit, with the MD5
  authentication of the hellos the title announces.

  ACT 1  the adjacency comes up, the host lans are learnt, h1 reaches h2.
  ACT 2  r2 is given another password: its hellos are refused, the
         adjacency falls, h1 no longer reaches h2.
  ACT 3  the right password is back, so is the adjacency.

Image: frr_trixie (zebra, isisd)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r1 eth0 sniff=on
    cloonix cli nemo sniff r1 eth0 --dump "not ip and not ip6 and not arp"

  A hello of ACT 2: it carries the authentication TLV, type 10, an MD5
  digest r1 cannot verify.

Important
  The printed example has no password line; "isis password md5" is the
  command of the same page that gives the MD5 of the title. The host lans
  are passive circuits, and the hellos are sent every second instead of
  ten to keep the demo short.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, frr_setup, hold, in_parallel, reached, \
    step, story
from frrlib import conf, forwarding, has, opening, ping, pings, proof, \
    until, verdict, vtysh

HISTORY = """\
IS-IS was written at Digital Equipment Corporation for DECnet Phase V,
largely by Radia Perlman and Dave Oran, and became ISO 10589; RFC 1195 by
Ross Callon taught it IP in 1990. Its authentication, a clear password at
first, got its HMAC-MD5 in RFC 5304 in 2008, by Tony Li and Ran Atkinson.
The isisd of frr began as the isisd of Sampo Saaristo and Ofer Wald in 2001,
taken into Quagga, then frr.

Today: IS-IS is the interior protocol of most large carriers and of many
data centre fabrics.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

H1, R1, R2, H2 = "h1", "r1", "r2", "h2"
ALL = (R1, R2, H1, H2)
ROUTERS = (R1, R2)
L1, L12, L2 = "lan1", "lan12", "lan2"

WIRING = {
    H1: [(0, L1, "10.1.0.10/24")],
    R1: [(0, L12, "10.0.12.1/24"), (1, L1, "10.1.0.1/24")],
    R2: [(0, L12, "10.0.12.2/24"), (1, L2, "10.2.0.1/24")],
    H2: [(0, L2, "10.2.0.10/24")],
}
GATEWAY = {H1: "10.1.0.1", H2: "10.2.0.1"}
SYSID = {R1: "1900.0004", R2: "1900.0005"}
KEY = "ABCDEFGHIJK"

def frr_conf(node):
    return "\n".join([
        "hostname %s" % node,
        "interface eth0",
        " ip router isis FOO",
        " isis network point-to-point",
        " isis circuit-type level-2-only",
        " isis password md5 %s" % KEY,
        " isis hello-interval 1",
        "interface eth1",
        " ip router isis FOO",
        " isis passive",
        "router isis FOO",
        " net 47.0023.0000.0000.0000.0000.0000.0000.%s.00" % SYSID[node],
        " metric-style wide",
        " is-type level-2-only"])

LAYOUT = {
    H1: (-520, 0), L1: (-390, 0), R1: (-220, 0), L12: (0, 0),
    R2: (220, 0), L2: (390, 0), H2: (520, 0),
}
ETHS = {
    (H1, "eth0"): 90, (R1, "eth1"): 270, (R1, "eth0"): 90,
    (R2, "eth0"): 270, (R2, "eth1"): 90, (H2, "eth0"): 270,
}

CARD = r"""
    h1 ---- r1 ==== r2 ---- h2

  One IS-IS adjacency, level 2, point-to-point, its hellos signed with
  HMAC-MD5.
"""

def configure(node):
    forwarding(node)
    frr_setup(node, ["isisd"], frr_conf(node))

def adjacent():
    return has(R1, "show isis neighbor", ["Up"])

def password(key):
    conf(R2, "interface eth0", "isis password md5 %s" % key)

def main():
    opening()

    step("act 1: the adjacency comes up, h1 reaches h2")
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_adj = until(adjacent, 90)
    ok_route = until(lambda: has(R1, "show ip route isis", ["10.2.0.0/24"]),
                     60)
    until(lambda: pings(H1, "10.2.0.10"), 30)
    proof("r1, its neighbour:", vtysh(R1, "show isis neighbor"))
    proof("r1, the routes isis gave it:", vtysh(R1, "show ip route isis"))
    r = ping(H1, "10.2.0.10")
    proof("h1 -> h2:", r)
    ok_ping = reached(r)
    hold()

    step("act 2: r2 is given another password, the adjacency falls")
    got = count_on(R1, 0, "not ip and not ip6 and not arp", lambda: (
        password("WRONGWRONG"),
        until(lambda: not adjacent(), 60, period=1))[1], settle=1.0)
    ok_fell = got["out"] if got else not adjacent()
    if got is not None:
        story("    %d IS-IS frame(s) on r1 eth0 meanwhile, refused by r1."
              % got["count"])
    proof("r1, its neighbours:", vtysh(R1, "show isis neighbor"))
    ok_cut = until(lambda: not pings(H1, "10.2.0.10"), 30)
    story("    h1 no longer reaches h2: the route left with the adjacency.")
    hold()

    step("act 3: the right password is back, so is the adjacency")
    password(KEY)
    ok_back = until(adjacent, 60) and until(lambda: pings(H1, "10.2.0.10"),
                                            60)
    proof("r1, its neighbour:", vtysh(R1, "show isis neighbor"))

    verdict([("the MD5 adjacency comes up", ok_adj),
             ("r1 learns 10.2.0.0/24", ok_route),
             ("h1 reaches h2", ok_ping),
             ("a wrong password drops the adjacency", ok_fell),
             ("h1 is cut from h2", ok_cut),
             ("the right password brings it back", ok_back)])

    story("""
the network is left up. Things to try:

  the link state database:
    cloonix cli %s ssh r2 "vtysh -c 'show isis database detail'"

  the hellos, signed:
    cloonix cli %s sniff r1 eth0 --dump "not ip and not ip6 and not arp"
""" % (NET, NET))

main()
