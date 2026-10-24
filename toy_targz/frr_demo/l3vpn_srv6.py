#!/usr/bin/env python3
r"""l3vpn_srv6 - BGP L3VPN over SRv6, an IPv6 core without MPLS.

Needs: vrf, a host kernel 5.11 or later.

    ./l3vpn_srv6.py [net]     the net defaults to nemo

The FRR User Guide, page bgp, section "L3VPN SRv6": the VPN routes carry,
instead of a label, an SRv6 SID taken from a locator of zebra; the far
edge decapsulates and looks up the VRF (End.DT4). The configuration is the
one of the frr test bgp_srv6l3vpn_to_bgp_vrf2.
https://docs.frrouting.org/en/latest/bgp.html#l3vpn-srv6

    red1  --\                                /--  red2   (192.168.2.10)
             pe1 ------- p ------- pe2
    blue1 --/  2001:db8:12::/64  2001:db8:23::/64  \--  blue2  (192.168.2.20)

  The customers of l3vpn_vrfs.py, red1 and blue1 both 192.168.1.10. The
  core is IPv6 only. pe1 owns the locator 2001:db8:1:1::/64, pe2 the
  locator 2001:db8:3:3::/64; p has two static routes to them and knows
  nothing else of SRv6.

  !!! KERNEL MODULE NEEDED ON THE HOST: vrf !!!
  A container cannot load a module. Before playing this demo, on the host,
  once per boot:

sudo modprobe vrf

  or do not run this demo. It stops with a big warning if the module is
  missing, or if the host kernel is older than 5.11 (End.DT4).

  ACT 1  iBGP pe1-pe2 over IPv6 for the VPNv4 family: each VPN route
         carries the SID of its VRF at its edge.
  ACT 2  red1 reaches red2, blue1 reaches blue2, and not across. On p-pe2
         a packet of red1 is IPv6 towards the SID of RED at pe2, the IPv4
         of the customer inside.

Image: frr_trixie (zebra, staticd, bgpd)
Size:  7 containers, 6 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf p eth1 sniff=on
    cloonix cli nemo sniff p eth1 --dump "ip6"

  An echo of red1, inside an IPv6 header whose destination is a SID of
  pe2, 2001:db8:3:3:...: the address is the instruction.

Important
  net.vrf.strict_mode=1 in the edges, needed by End.DT4, before the VRFs
  are made: RED table 10, BLUE table 20; seg6_enabled on their
  interfaces.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, cut_off, frr_setup, hold, in_parallel, \
    reached, ssh, step, story
from frrlib import forwarding, has, need_kernel, opening, ping, proof, \
    until, verdict, vtysh
from toylib import words_out

HISTORY = """\
SRv6 writes the segments of segment routing as IPv6 addresses: a locator
routed like any prefix, and in it functions, End, End.X, End.DT4. The
segment routing header is RFC 8754 of 2020, the network programming RFC
8986 of 2021, by Clarence Filsfils and colleagues; the VPN services over
it are RFC 9252 of 2022. Linux got SRv6 in 4.10 in 2017, by David Lebrun
at UCLouvain, and End.DT4 in 5.11. bgpd learned SRv6 VPNs from Hiroki
Shirokura at LINE in 2020.

Today: SRv6 runs in the backbones of SoftBank, China Telecom, Bell
Canada, Iliad; one IPv6 network instead of IP plus MPLS.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

PE1, P, PE2 = "pe1", "p", "pe2"
RED1, BLUE1, RED2, BLUE2 = "red1", "blue1", "red2", "blue2"
ROUTERS = (PE1, P, PE2)
ALL = ROUTERS + (RED1, BLUE1, RED2, BLUE2)
LC1, LC2 = "core1", "core2"
LR1, LB1, LR2, LB2 = "lanred1", "lanblue1", "lanred2", "lanblue2"

WIRING = {
    PE1: [(0, LC1, "2001:db8:12::1/64"), (1, LR1, ""), (2, LB1, "")],
    P: [(0, LC1, "2001:db8:12::2/64"), (1, LC2, "2001:db8:23::2/64")],
    PE2: [(0, LC2, "2001:db8:23::3/64"), (1, LR2, ""), (2, LB2, "")],
    RED1: [(0, LR1, "192.168.1.10/24")],
    BLUE1: [(0, LB1, "192.168.1.10/24")],
    RED2: [(0, LR2, "192.168.2.10/24")],
    BLUE2: [(0, LB2, "192.168.2.20/24")],
}
GATEWAY = {RED1: "192.168.1.1", BLUE1: "192.168.1.1",
           RED2: "192.168.2.1", BLUE2: "192.168.2.1"}
EDGE = {PE1: ("1.1.1.1", "2001:db8:12::1", "2001:db8:23::3",
              "2001:db8:12::2", "2001:db8:1:1::/64", "2001:db8:3:3::/64",
              "2001:db8:23::/64"),
        PE2: ("3.3.3.3", "2001:db8:23::3", "2001:db8:12::1",
              "2001:db8:23::2", "2001:db8:3:3::/64", "2001:db8:1:1::/64",
              "2001:db8:12::/64")}
VRFS = {
    PE1: [("RED", 10, 1, "192.168.1.1/24", "65000:1"),
          ("BLUE", 20, 2, "192.168.1.1/24", "65000:2")],
    PE2: [("RED", 10, 1, "192.168.2.1/24", "65000:1"),
          ("BLUE", 20, 2, "192.168.2.1/24", "65000:2")],
}

def frr_conf(node):
    c = ["hostname %s" % node]
    if node == P:
        c += ["ipv6 route 2001:db8:1:1::/64 2001:db8:12::1",
              "ipv6 route 2001:db8:3:3::/64 2001:db8:23::3"]
        return "\n".join(c)
    rid, me, peer, gw, mine, far, farnet = EDGE[node]
    c += ["ipv6 route %s %s" % (far, gw),
          "ipv6 route %s %s" % (farnet, gw),
          "segment-routing",
          " srv6",
          "  locators",
          "   locator loc1",
          "    prefix %s func-bits 8" % mine,
          "router bgp 65000",
          " bgp router-id %s" % rid,
          " no bgp default ipv4-unicast",
          " neighbor %s remote-as 65000" % peer,
          " neighbor %s update-source %s" % (peer, me),
          " neighbor %s capability extended-nexthop" % peer,
          " address-family ipv4 vpn",
          "  neighbor %s activate" % peer,
          " exit-address-family",
          " segment-routing srv6",
          "  locator loc1"]
    for k, (vrf, _t, _e, _a, rt) in enumerate(VRFS[node]):
        c += ["router bgp 65000 vrf %s" % vrf,
              " bgp router-id %s" % rid,
              " address-family ipv4 unicast",
              "  redistribute connected",
              "  sid vpn export auto",
              "  nexthop vpn export %s" % me,
              "  rd vpn export %s:%d" % (rid, k + 1),
              "  rt vpn both %s" % rt,
              "  export vpn",
              "  import vpn",
              " exit-address-family"]
    return "\n".join(c)

LAYOUT = {
    RED1: (-560, -120), LR1: (-430, -80), BLUE1: (-560, 120),
    LB1: (-430, 80), PE1: (-300, 0), LC1: (-150, 0), P: (0, 0),
    LC2: (150, 0), PE2: (300, 0), LR2: (430, -80), RED2: (560, -120),
    LB2: (430, 80), BLUE2: (560, 120),
}
ETHS = {
    (RED1, "eth0"): 90, (BLUE1, "eth0"): 90, (PE1, "eth1"): 300,
    (PE1, "eth2"): 240, (PE1, "eth0"): 90, (P, "eth0"): 270, (P, "eth1"): 90,
    (PE2, "eth0"): 270, (PE2, "eth1"): 60, (PE2, "eth2"): 120,
    (RED2, "eth0"): 270, (BLUE2, "eth0"): 270,
}

CARD = r"""
    red1  --\                    /--  red2
             pe1 ---- p ---- pe2           (IPv6 only core, iBGP VPNv4)
    blue1 --/                    \--  blue2

  No MPLS: the far VRF is an IPv6 address, a SID of pe2.
"""

def vrfs_up(node):
    """strict mode, the VRF devices, their interfaces moved in and
    addressed."""
    ssh(node, "sysctl -w net.vrf.strict_mode=1 "
              "net.ipv6.conf.all.seg6_enabled=1 "
              "net.ipv6.conf.default.seg6_enabled=1 >/dev/null")
    for num in (0, 1, 2):
        ssh(node, "sysctl -w net.ipv6.conf.eth%d.seg6_enabled=1 >/dev/null"
                  % num)
    for vrf, table, num, addr, _rt in VRFS[node]:
        ssh(node, "ip link add %s type vrf table %d" % (vrf, table))
        ssh(node, "ip link set %s up" % vrf)
        ssh(node, "ip link set eth%d master %s" % (num, vrf))
        ssh(node, "ip addr add %s dev eth%d" % (addr, num))

def srv6_route(vrf):
    """pe1 sends 192.168.2.0/24 of vrf in an SRv6 encapsulation to a SID of
    pe2, as its kernel holds it (vtysh shows the function as a label)."""
    out = ssh(PE1, "ip route show vrf %s 192.168.2.0/24" % vrf, check=False)
    return "encap seg6" in out and "2001:db8:3:3:" in out

def configure(node):
    forwarding(node)
    if node in VRFS:
        vrfs_up(node)
    frr_setup(node, ["bgpd"] if node in VRFS else ["staticd"],
              frr_conf(node))

def main():
    opening("vrf")
    need_kernel(5, 11, "End.DT4, the SRv6 decapsulation into a VRF")

    step("act 1: the VPN routes come up, each with a SID")
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_loc = until(lambda: has(PE2, "show segment-routing srv6 locator",
                               ["loc1", "2001:db8:3:3::/64"]), 60)
    proof("pe2, its locator:", vtysh(PE2, "show segment-routing srv6 locator"))
    ok_vpn = until(lambda: has(PE1, "show bgp ipv4 vpn", [
        "3.3.3.3:1", "3.3.3.3:2", "192.168.2.0/24"]), 120)
    proof("pe1, the VPN table:", vtysh(PE1, "show bgp ipv4 vpn"))
    ok_red = until(lambda: srv6_route("RED"), 60)
    proof("pe1, the route of RED to the far site, a SID of pe2:",
          vtysh(PE1, "show ip route vrf RED 192.168.2.0/24"))
    ok_blue = until(lambda: srv6_route("BLUE"), 60)
    ok_dt4 = until(lambda: "End.DT4" in ssh(
        PE2, "ip -6 route show table all", check=False), 30)
    proof("pe2, the SIDs in its kernel:",
          ssh(PE2, "ip -6 route show table all | grep seg6local",
              check=False))
    # p has no ipv4 route at all, and vtysh prints nothing for an empty
    # table: its summary says it
    ok_p = words_out(ssh(P, "vtysh -c 'show ip route' -c 'show ip route "
                            "summary'", check=False), ["192.168.", "B>"])
    hold()

    step("act 2: each customer reaches its own site, over IPv6")
    got = count_on(P, 1, "ip6 and dst net 2001:db8:3:3::/64",
                   lambda: ping(RED1, "192.168.2.10"), settle=1.0)
    out = got["out"] if got else ping(RED1, "192.168.2.10")
    ok_wire = bool(got and got["count"] > 0)
    if got is not None:
        story("    %d IPv6 packet(s) towards 2001:db8:3:3::/64 on p eth1."
              % got["count"])
    proof("red1 -> red2:", out)
    ok_r = reached(out)
    # the answers, encapsulated by pe2 towards a SID of pe1: seen on p, the
    # way out and back is whole; not seen, pe2 lost the echo
    back = count_on(P, 1, "ip6 and dst net 2001:db8:1:1::/64",
                    lambda: ping(RED1, "192.168.2.10"), settle=1.0)
    ok_back = bool(back and back["count"] > 0)
    if back is not None:
        story("    %d IPv6 packet(s) back towards 2001:db8:1:1::/64 on p "
              "eth1." % back["count"])
    # on pe2 itself: the echo out of the decapsulation on the side of red2,
    # and its route of RED back to the site of pe1
    dec = count_on(PE2, 1, "icmp", lambda: ping(RED1, "192.168.2.10"),
                   settle=1.0)
    ok_dec = bool(dec and dec["count"] > 0)
    if dec is not None:
        story("    %d ICMP packet(s) on pe2 eth1, the side of red2." %
              dec["count"])
    proof("pe2, the route of RED back to the site of pe1:",
          ssh(PE2, "ip route show vrf RED 192.168.1.0/24", check=False))
    ok_ret = "encap seg6" in ssh(PE2, "ip route show vrf RED 192.168.1.0/24",
                                 check=False)
    out = ping(BLUE1, "192.168.2.20")
    proof("blue1 -> blue2, from the same address as red1:", out)
    ok_b = reached(out)
    ok_iso1 = cut_off(ping(RED1, "192.168.2.20", count=2))
    ok_iso2 = cut_off(ping(BLUE1, "192.168.2.10", count=2))
    proof("pe1, the kernel route of RED, an SRv6 encapsulation:",
          ssh(PE1, "ip route show vrf RED 192.168.2.0/24", check=False))

    verdict([("pe2 holds the locator 2001:db8:3:3::/64", ok_loc),
             ("the VPN table holds the routes of pe2 under two RDs", ok_vpn),
             ("RED reaches the far site through a SID of pe2", ok_red),
             ("BLUE too, through another SID", ok_blue),
             ("pe2 has End.DT4 SIDs in its kernel", ok_dt4),
             ("p holds no customer route", ok_p),
             ("IPv6 towards the locator of pe2 on p-pe2", ok_wire),
             ("pe2 decapsulates the echo into RED, towards red2", ok_dec),
             ("pe2 has its way back to pe1, an SRv6 encapsulation", ok_ret),
             ("the answers come back over SRv6, towards pe1", ok_back),
             ("red1 reaches red2", ok_r),
             ("blue1 reaches blue2", ok_b),
             ("red1 does not reach BLUE", ok_iso1),
             ("blue1 does not reach RED", ok_iso2)])

    story("""
the network is left up. Things to try:

  the SIDs bgp took from the locator:
    cloonix cli %s ssh pe2 "vtysh -c 'show bgp segment-routing srv6'"

  the SID reachability bgp checks:
    cloonix cli %s ssh pe1 "vtysh -c 'show bgp nexthop detail'"

  the packets on the core, IPv6 around IPv4:
    cloonix cli %s sniff p eth1 --dump "ip6"
""" % (NET, NET, NET))

main()
