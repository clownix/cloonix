#!/usr/bin/env python3
r"""pbr_sample - Sample configuration (PBR).

    ./pbr_sample.py [net]     the net defaults to nemo

The example of the FRR User Guide, page pbr, section "Sample
configuration":
https://docs.frrouting.org/en/latest/pbr.html#sample-configuration

                                       +-- lan4 -- n1 (4.5.6.7, 9.9.9.9)
    h (10.10.10.10) -- lan0 -- r --+
                                       +-- lan5 -- n2 (5.6.7.8, 9.9.9.9)

  r carries the guide's configuration: the nexthop-group TEST (4.5.6.7
  and 5.6.7.8) and the pbr-map BLUE, which sends what comes from
  10.10.10.0/24 towards 9.9.9.0/24 to that group; its swp1 is eth0 here.
  r has no route at all to 9.9.9.0/24: only the policy gets there.

  ACT 1  the policy is installed; h pings 9.9.9.9 from 10.10.10.10, the
         packets leave r towards n1 or n2.
  ACT 2  h pings from 10.10.20.10, outside the match src-ip: the policy
         does not apply and r has no route, the ping fails.

Image: frr_trixie (zebra, pbrd)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r eth1 sniff=on
    cloonix cli nemo sniff r eth1 --dump "icmp and host 9.9.9.9"

  An echo request towards 9.9.9.9 leaving r on the link to n1, sent there
  by an ip rule pbrd installed, not by the routing table.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, frr_setup, hold, reached, ssh, step, story, Tap
from frrlib import forwarding, has, opening, proof, until, verdict, vtysh

HISTORY = """\
Policy based routing, choosing a path by something else than the
destination, came with the multiple routing tables and ip rules Alexey
Kuznetsov put in linux 2.2, in 1999. The pbrd of frr, written by Donald
Sharp of Cumulus Networks in 2018, drives those rules from a route-map
like configuration, with nexthop-groups for several paths.

Today: PBR steers traffic to firewalls, load balancers and particular
uplinks, by source, port or mark.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

R, H, N1, N2 = "r", "h", "n1", "n2"
ALL = (R, H, N1, N2)
L0, L4, L5 = "lan0", "lan4", "lan5"

WIRING = {
    R: [(0, L0, "10.10.10.1/24"), (1, L4, "4.5.6.1/24"),
        (2, L5, "5.6.7.1/24")],
    H: [(0, L0, "10.10.10.10/24")],
    N1: [(0, L4, "4.5.6.7/24")],
    N2: [(0, L5, "5.6.7.8/24")],
}
GATEWAY = {H: "10.10.10.1", N1: "4.5.6.1", N2: "5.6.7.1"}

# The guide's configuration, swp1 -> eth0.
R_CONF = """\
hostname r
nexthop-group TEST
  nexthop 4.5.6.7
  nexthop 5.6.7.8
pbr-map BLUE seq 100
  match dst-ip 9.9.9.0/24
  match src-ip 10.10.10.0/24
  set nexthop-group TEST
int eth0
  pbr-policy BLUE"""

LAYOUT = {
    H: (-460, 0), L0: (-260, 0), R: (-40, 0), L4: (180, -120),
    N1: (400, -120), L5: (180, 120), N2: (400, 120),
}
ETHS = {
    (H, "eth0"): 90, (R, "eth0"): 270, (R, "eth1"): 45, (R, "eth2"): 135,
    (N1, "eth0"): 270, (N2, "eth0"): 270,
}

CARD = r"""
                         +-- n1 (4.5.6.7)
    h -- r (pbr BLUE) ---+
                         +-- n2 (5.6.7.8)

  From 10.10.10.0/24 to 9.9.9.0/24, r follows the nexthop-group TEST.
"""

def ping_from(src, count=5):
    return ssh(H, "ping -c %d -W 1 -I %s 9.9.9.9" % (count, src),
               check=False)

def main():
    opening()
    for n in (N1, N2):
        ssh(n, "ip addr add 9.9.9.9/32 dev lo")
        ssh(n, "ip route add 10.10.0.0/16 via %s" % GATEWAY[n])
    ssh(H, "ip addr add 10.10.20.10/24 dev eth0")
    ssh(R, "ip addr add 10.10.20.1/24 dev eth0")

    step("act 1: the policy BLUE sends 10.10.10.0/24 -> 9.9.9.0/24 to TEST")
    forwarding(R)
    frr_setup(R, ["pbrd"], R_CONF)
    ok_inst = until(lambda: has(R, "show pbr map BLUE",
                                ["Installed: yes Reason: Valid"]), 60)
    proof("r, the pbr-map:", vtysh(R, "show pbr map BLUE"))
    proof("r, the ip rule pbrd installed:", ssh(R, "ip rule show",
                                                check=False))
    no_route = "9.9.9" not in ssh(R, "ip route show", check=False)
    taps = (Tap(R, 1, "icmp and host 9.9.9.9"),
            Tap(R, 2, "icmp and host 9.9.9.9"))
    live = all(t.ready() for t in taps)
    for t in taps:
        t.clear()
    until(lambda: reached(ping_from("10.10.10.10", 1)), 30)
    r = ping_from("10.10.10.10")
    seen = sum(t.count for t in taps)
    for t in taps:
        t.stop()
    proof("h -> 9.9.9.9 from 10.10.10.10:", r)
    if live:
        story("    %d echo packet(s) seen on the links to n1 and n2." % seen)
    ok_pol = reached(r) and (seen > 0 or not live)
    hold()

    step("act 2: from 10.10.20.10 the policy does not apply, no route")
    r = ping_from("10.10.20.10", 3)
    proof("h -> 9.9.9.9 from 10.10.20.10:", r)
    ok_miss = not reached(r)
    story("    The source is outside match src-ip: r falls back on its")
    story("    routing table, which has no route to 9.9.9.0/24.")

    verdict([("the pbr-map BLUE is installed", ok_inst),
             ("r has no route to 9.9.9.0/24 of its own", no_route),
             ("from 10.10.10.0/24, 9.9.9.9 answers through TEST", ok_pol),
             ("from 10.10.20.0/24, nothing answers", ok_miss)])

    story("""
the network is left up. Things to try:

  the nexthop-group and the table pbrd made:
    cloonix cli %s ssh r "vtysh -c 'show pbr nexthop-groups'"

  the policy table in the kernel:
    cloonix cli %s ssh r "ip route show table all | grep 9.9.9"
""" % (NET, NET))

main()
