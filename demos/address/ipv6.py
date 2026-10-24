#!/usr/bin/env python3
r"""ipv6 - an address nobody configured: router advertisement and SLAAC.

    ./ipv6.py [net]                the net defaults to nemo

Two hosts, one router, and not one address typed on a host::

    h1 ---- lan1 ---- r1 ---- lan2 ---- h2
       2001:db8:1::/64   2001:db8:2::/64

  r1 holds ::1 of each prefix and runs zebra, the daemon of frr that sends
  router advertisements. h1 and h2 are given nothing.

  ACT 1  before any advertisement: each host has a link-local address,
         fe80::, made from its own mac, and no route out of its lan.
  ACT 2  zebra starts on r1. Each host hears the advertisement, makes a
         global address from the prefix and its mac, and takes r1 as its
         default router: SLAAC, stateless address autoconfiguration.
  ACT 3  h1 pings the address h2 made for itself, through r1.

Image: frr_trixie (zebra)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
  Everything here is icmp6: the advertisement (type 134), the neighbour
  solicitation that replaces arp (135), the echo.

    cloonix cli nemo cnf r1 eth0 sniff=on
    cloonix cli nemo sniff r1 eth0 --dump icmp6

  The packet to catch is the router advertisement, sent to ff02::1,
  every host of the lan: it carries the prefix and nothing about any host.

Important
  Nothing is baked into the image: the configuration of zebra is written
  at run time.
"""
import os
import sys
import time

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    NET, build, check_net, choose_rootfs, count_on, erase_net, frr_setup,
    hold, place, reached, ssh, step, story, tools_present)

HISTORY = """\
IPv6 was specified in RFC 1883 in 1995 by Steve Deering and Robert Hinden,
when the end of the 32-bit IPv4 address space was already in sight: 128-bit
addresses, no broadcast, no arp. Stateless autoconfiguration, RFC 1971 in
1996 by Susan Thomson and Thomas Narten, now RFC 4862, lets a host build its
own address from a prefix the router announces, with no server keeping a
lease. The KAME project in Japan wrote the stack of the BSDs; Pedro Roque
wrote the first one of linux, and the USAGI project of Hideaki Yoshifuji
completed it.

Today: more than forty percent of the traffic reaching Google arrives over
IPv6, and several mobile networks hand out nothing else. The router side is
zebra, of frr, the descendant of GNU Zebra written by Kunihiro Ishiguro.
"""

ROOTFS_CHOICES = ("frr_trixie",)

H1, R1, H2 = "h1", "r1", "h2"
ALL = (H1, R1, H2)
L1, L2 = "lan1", "lan2"

WIRING = {
    H1: [(0, L1, None)],
    R1: [(0, L1, "2001:db8:1::1/64"), (1, L2, "2001:db8:2::1/64")],
    H2: [(0, L2, None)],
}
GATEWAY = {}
PREFIX = {H1: "2001:db8:1:", H2: "2001:db8:2:"}

ZEBRA_CONF = """\
hostname r1
interface eth0
 no ipv6 nd suppress-ra
 ipv6 nd ra-interval 3
 ipv6 nd prefix 2001:db8:1::/64
interface eth1
 no ipv6 nd suppress-ra
 ipv6 nd ra-interval 3
 ipv6 nd prefix 2001:db8:2::/64"""

LAYOUT = {
    H1: (-440, 0), L1: (-220, 0), R1: (0, 0), L2: (220, 0), H2: (440, 0),
}
ETHS = {
    (H1, "eth0"): 90, (R1, "eth0"): 270, (R1, "eth1"): 90, (H2, "eth0"): 270
}

CARD = r"""
    h1 ---- lan1 ---- r1 ---- lan2 ---- h2
       2001:db8:1::/64   2001:db8:2::/64

  r1 announces a prefix on each lan. The hosts are given no address: each
  makes its own from the prefix it hears and its mac.
"""

def global_addr(node, timeout=30):
    """The global address the host made, once duplicate detection is over."""
    end = time.time() + timeout
    while time.time() < end:
        out = ssh(node, "ip -6 -o addr show dev eth0 scope global",
                  check=False)
        for word in out.split():
            if word.startswith(PREFIX[node]) and "tentative" not in out:
                return word.split("/")[0]
        time.sleep(1)
    return ""

def main():
    story(HISTORY)
    step("checking the server")
    check_net()
    step("looking for the image")
    rootfs = choose_rootfs()
    erase_net()
    place()
    story(CARD)
    story("  image: %s" % rootfs)

    step("creating and cabling the three machines")
    build(rootfs)
    tools_present(R1, "vtysh")
    for h in (H1, H2):
        ssh(h, "sysctl -w net.ipv6.conf.eth0.disable_ipv6=0 "
               "net.ipv6.conf.eth0.accept_ra=1 "
               "net.ipv6.conf.eth0.autoconf=1 "
               "net.ipv6.conf.eth0.addr_gen_mode=0 >/dev/null")
    ssh(R1, "sysctl -w net.ipv6.conf.all.forwarding=1 >/dev/null")

    step("act 1: no advertisement yet, a link-local address and no route")
    story()
    story("    h1, its mac and the address the kernel made from it:")
    story(ssh(H1, "ip -o link show dev eth0 | grep -o 'link/ether [0-9a-f:]*'",
              check=False).rstrip())
    story(ssh(H1, "ip -6 -o addr show dev eth0", check=False).rstrip())
    story()
    story("    h1, its ipv6 routes out of the lan:")
    story(ssh(H1, "ip -6 route show default", check=False).rstrip()
          or "      (none)")
    story()
    story("    fe80:: works on the lan only. The mac is inside the address,")
    story("    ff:fe put in its middle and one bit flipped: EUI-64.")
    hold()

    step("act 2: zebra advertises the prefixes, the hosts make their address")
    story()
    got = count_on(R1, 0, "icmp6",
                   lambda: frr_setup(R1, [], ZEBRA_CONF), settle=8.0)
    a1, a2 = global_addr(H1), global_addr(H2)
    if not (a1 and a2):
        sys.exit("a host made no global address: h1=%r h2=%r\n%s"
                 % (a1, a2, ssh(R1, "vtysh -c 'show interface eth0'",
                                check=False)))
    if got is not None:
        story("    %d icmp6 packet(s) on r1 eth0 while zebra started:" % got["count"])
        story("    advertisements, and the neighbour solicitations of h1")
        story("    checking that nobody holds the address it is about to take.")
        story()
    story("    h1 made   %s" % a1)
    story("    h2 made   %s" % a2)
    story()
    story("    h1, its default route, learnt from the same advertisement:")
    story(ssh(H1, "ip -6 route show default", check=False).rstrip())
    story()
    story("    The router is known by its link-local address: a default route")
    story("    never needs a global one. No server holds a lease; r1 does not")
    story("    know which addresses exist on its lans.")
    hold()

    step("act 3: h1 pings the address h2 made, through r1")
    story()
    r = ssh(H1, "ping -6 -c 5 %s" % a2, check=False)
    story(r.strip())
    ok = reached(r)
    story()
    story("    h1, the neighbour table that replaces the arp table:")
    story(ssh(H1, "ip -6 neigh show dev eth0", check=False).rstrip())
    story()
    if not ok:
        sys.exit("h1 did not reach %s" % a2)

    story("""
the network is left up. Things to try:

  the advertisement, every three seconds:
    cloonix cli %s sniff r1 eth0 --dump icmp6

  what zebra announces on a lan:
    cloonix cli %s ssh r1 "vtysh -c 'show interface eth0'"

  take the address away and watch it come back at the next advertisement:
    cloonix cli %s ssh h1 "ip -6 addr flush dev eth0 scope global"
    cloonix cli %s ssh h1 "ip -6 addr show dev eth0"
""" % (NET, NET, NET, NET))

main()
