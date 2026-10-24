#!/usr/bin/env python3
r"""ra_example - Router Advertisement Configuration Example.

    ./ra_example.py [net]     the net defaults to nemo

The example of the FRR User Guide, page ipv6, section "Router
Advertisement Configuration Example":
https://docs.frrouting.org/en/latest/ipv6.html#router-advertisement-configuration-example

    h1 ----+
           |
          lan0 ---- r1 (eth0: no ipv6 nd suppress-ra,
           |              ipv6 nd prefix 2001:0DB8:5009::/64)
    h2 ----+

  r1 carries the three lines of the guide, nothing more. h1 and h2 are
  given no address at all.

  ACT 1  before zebra runs: h1 and h2 have only their fe80:: address.
  ACT 2  zebra on r1 advertises the prefix: each host makes an address in
         2001:db8:5009::/64 from its mac, and takes r1 as default router.
  ACT 3  h1 pings the address h2 made for itself, and r1.

Image: frr_trixie (zebra)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r1 eth0 sniff=on
    cloonix cli nemo sniff r1 eth0 --dump "icmp6"

  The router advertisement, icmp6 type 134, to ff02::1: the prefix option
  2001:db8:5009::/64 with its autonomous flag.

Important
  The guide does not set ra-interval: the first advertisements zebra sends
  at start, and the answers to the solicitations of the hosts, suffice.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, frr_setup, hold, reached, ssh, step, story
from frrlib import opening, ping, proof, until, verdict, vtysh

HISTORY = """\
Neighbor Discovery, RFC 1970 in 1996 by Thomas Narten, Erik Nordmark and
Bill Simpson, now RFC 4861, replaced arp, router discovery and redirects
with one icmp6 protocol; stateless autoconfiguration, RFC 4862, lets a
host build its address from the prefix a router advertises. zebra, the
core of frr written by Kunihiro Ishiguro, sends these advertisements
itself, a radvd built in.

Today: SLAAC is how most phones, laptops and home devices get their IPv6
address.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

R1, H1, H2 = "r1", "h1", "h2"
ALL = (R1, H1, H2)
HOSTS = (H1, H2)
L0 = "lan0"

WIRING = {
    R1: [(0, L0, "2001:db8:5009::1/64")],
    H1: [(0, L0, None)],
    H2: [(0, L0, None)],
}
GATEWAY = {}
PREFIX = "2001:db8:5009:"

# The guide's three lines.
R1_CONF = """\
hostname r1
interface eth0
 no ipv6 nd suppress-ra
 ipv6 nd prefix 2001:0DB8:5009::/64"""

LAYOUT = {H1: (-300, -150), H2: (-300, 150), L0: (-80, 0), R1: (220, 0)}
ETHS = {(H1, "eth0"): 135, (H2, "eth0"): 45, (R1, "eth0"): 270}

CARD = r"""
    h1 --+
         +-- lan0 -- r1    (ipv6 nd prefix 2001:0DB8:5009::/64)
    h2 --+

  The hosts get their address and their default router from r1.
"""

def global_addr(node):
    out = ssh(node, "ip -6 -o addr show dev eth0 scope global", check=False)
    if "tentative" in out:
        return ""
    for word in out.split():
        if word.startswith(PREFIX):
            return word.split("/")[0]
    return ""

def default_router(node):
    return "fe80::" in ssh(node, "ip -6 route show default", check=False)

def main():
    opening()
    for h in HOSTS:
        ssh(h, "sysctl -w net.ipv6.conf.eth0.accept_ra=1 "
               "net.ipv6.conf.eth0.autoconf=1 >/dev/null")

    step("act 1: before any advertisement, link-local addresses only")
    ok_none = all(not global_addr(h) for h in HOSTS)
    proof("h1, its addresses:", ssh(H1, "ip -6 addr show dev eth0"))
    hold()

    step("act 2: zebra advertises the prefix, the hosts make their address")
    ssh(R1, "sysctl -w net.ipv6.conf.all.forwarding=1 >/dev/null")

    def start_and_solicit():
        frr_setup(R1, [], R1_CONF)
        for h in HOSTS:
            ssh(h, "ip link set eth0 down; ip link set eth0 up")
        return until(lambda: all(global_addr(h) for h in HOSTS), 60)
    got = count_on(R1, 0, "icmp6", start_and_solicit, settle=1.0)
    ok_slaac = got["out"] if got else all(global_addr(h) for h in HOSTS)
    if got is not None:
        story("    %d icmp6 packet(s) on r1 eth0: solicitations,"
              % got["count"])
        story("    advertisements, duplicate address detection.")
        story()
    ok_dflt = until(lambda: all(default_router(h) for h in HOSTS), 30)
    proof("h1, its addresses now:", ssh(H1, "ip -6 addr show dev eth0"))
    proof("h1, its default router:", ssh(H1, "ip -6 route show default"))
    proof("r1, what zebra advertises:", vtysh(R1, "show interface eth0"))
    hold()

    step("act 3: h1 pings the address h2 made for itself, and r1")
    a2 = global_addr(H2)
    r = ping(H1, a2 or "2001:db8:5009::2")
    proof("h1 -> h2 (%s):" % a2, r)
    ok_h2 = bool(a2) and reached(r)
    r = ping(H1, "2001:db8:5009::1", count=3)
    proof("h1 -> r1:", r)
    ok_r1 = reached(r)

    verdict([("no global address before the advertisement", ok_none),
             ("both hosts make an address in 2001:db8:5009::/64", ok_slaac),
             ("both hosts take r1 as default router", ok_dflt),
             ("h1 reaches h2 on its own address", ok_h2),
             ("h1 reaches r1", ok_r1)])

    story("""
the network is left up. Things to try:

  the advertisement parameters of zebra:
    cloonix cli %s ssh r1 "vtysh -c 'show interface eth0'"

  the advertisements as they pass:
    cloonix cli %s sniff r1 eth0 --dump "icmp6"
""" % (NET, NET))

main()
