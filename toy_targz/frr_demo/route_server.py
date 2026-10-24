#!/usr/bin/env python3
r"""route_server - Example of Route Server Configuration.

    ./route_server.py [net]     the net defaults to nemo

The example of the FRR User Guide, page routeserver, section "Example of
Route Server Configuration":
https://docs.frrouting.org/en/latest/routeserver.html#example-of-route-server-configuration

                 ra (AS 65001, 2001:db8::a)
                  |
    rb ---------- ix ---------- rc
    (AS 65002,    |             (AS 65003,
     2001:db8::b) |              2001:db8::c)
                 rs (AS 65000, 2001:db8::ffff, view RS)

  An exchange point: ra, rb and rc peer only with the route server rs,
  "router bgp 65000 view RS", each of them a route-server-client. Each
  router announces two prefixes of its own (2001:db8:aaaa::, bbbb::,
  cccc::) and the two common ones, 2001:db8:0:1::/64 and 2001:db8:0:2::/64.

  ACT 1  every client learns the prefixes of the others from rs alone,
         with the next hop of the router that owns them and an AS path
         where rs does not appear: rs is transparent.
  ACT 2  ra announces a prefix of rb's space, 2001:db8:bbbb:9::/64. The
         import filter of rs, built from the PEER-X-PREFIXES lists of the
         guide, refuses it: rb and rc never see it.

Image: frr_trixie (zebra, staticd, bgpd)
Size:  4 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf rb eth0 sniff=on
    cloonix cli nemo sniff rb eth0 --dump "tcp port 179"

  An UPDATE from rs to rb: its NEXT_HOP is 2001:db8::a, an address of ra,
  and its AS_PATH starts with 65001; nothing in it says it went through rs.

Important
  The clients use the guide's configuration "with Route Server", plus a
  router-id (they have no IPv4 address) and "no neighbor enforce-first-as"
  (the first AS of a path from rs is not 65000). The per-client RIBs and
  their IMPORT/EXPORT route-maps of the guide come from Quagga and no
  longer exist in frr; what stays is the filtering of what each client may
  announce, kept here with the guide's prefix-lists.
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
from frrlib import conf, has, lacks, opening, proof, until, verdict, vtysh

HISTORY = """\
The route server was invented for the Routing Arbiter project of the NSF in
the 1990s, at the network access points that replaced the NSFNET backbone:
with a hundred networks on one exchange, a full mesh is five thousand BGP
sessions; with a route server, a hundred. RFC 7947, in 2016, by Elisa Jasinska,
Nick Hilliard, Robert Raszuk and Niels Bakker, describes it. The route server
code of Quagga was written at the Universidad Politecnica de Madrid and
tested with VNUML, which shows in the guide.

Today: nearly every internet exchange, DE-CIX, AMS-IX, LINX, runs route
servers, mostly BIRD and frr side by side.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

RA, RB, RC, RS = "ra", "rb", "rc", "rs"
ALL = (RA, RB, RC, RS)
IX = "ix"

WIRING = {
    RA: [(0, IX, "2001:db8::a/64")],
    RB: [(0, IX, "2001:db8::b/64")],
    RC: [(0, IX, "2001:db8::c/64")],
    RS: [(0, IX, "2001:db8::ffff/64")],
}
GATEWAY = {}
CLIENT = {RA: ("65001", "aaaa", "10.0.0.1"), RB: ("65002", "bbbb", "10.0.0.2"),
          RC: ("65003", "cccc", "10.0.0.3")}
ADDR = {RA: "2001:db8::a", RB: "2001:db8::b", RC: "2001:db8::c"}
LEAK = "2001:db8:bbbb:9::/64"

def client_conf(node):
    asn, tag, rid = CLIENT[node]
    nets = ["2001:db8:%s:1::/64" % tag, "2001:db8:%s:2::/64" % tag,
            "2001:db8:0:1::/64", "2001:db8:0:2::/64"]
    c = ["hostname %s" % node]
    c += ["ipv6 route %s blackhole" % n for n in nets]
    c += ["router bgp %s" % asn,
          " bgp router-id %s" % rid,
          " no bgp ebgp-requires-policy",
          " no bgp default ipv4-unicast",
          " neighbor 2001:db8::ffff remote-as 65000",
          " neighbor 2001:db8::ffff timers 3 10",
          " no neighbor 2001:db8::ffff enforce-first-as",
          " address-family ipv6"]
    c += ["  network %s" % n for n in nets]
    c += ["  neighbor 2001:db8::ffff activate",
          "  neighbor 2001:db8::ffff soft-reconfiguration inbound",
          " exit-address-family"]
    return "\n".join(c)

RS_CONF = """\
hostname rs
router bgp 65000 view RS
 bgp router-id 10.0.0.255
 no bgp ebgp-requires-policy
 no bgp default ipv4-unicast
 neighbor 2001:db8::a remote-as 65001
 neighbor 2001:db8::b remote-as 65002
 neighbor 2001:db8::c remote-as 65003
 neighbor 2001:db8::a timers 3 10
 neighbor 2001:db8::b timers 3 10
 neighbor 2001:db8::c timers 3 10
 address-family ipv6
  neighbor 2001:db8::a activate
  neighbor 2001:db8::a route-server-client
  neighbor 2001:db8::a route-map RSCLIENT-A-IMPORT in
  neighbor 2001:db8::a soft-reconfiguration inbound
  neighbor 2001:db8::b activate
  neighbor 2001:db8::b route-server-client
  neighbor 2001:db8::b route-map RSCLIENT-B-IMPORT in
  neighbor 2001:db8::b soft-reconfiguration inbound
  neighbor 2001:db8::c activate
  neighbor 2001:db8::c route-server-client
  neighbor 2001:db8::c route-map RSCLIENT-C-IMPORT in
  neighbor 2001:db8::c soft-reconfiguration inbound
 exit-address-family
ipv6 prefix-list COMMON-PREFIXES seq  5 permit 2001:0DB8:0000::/48 ge 64 le 64
ipv6 prefix-list COMMON-PREFIXES seq 10 deny any
ipv6 prefix-list PEER-A-PREFIXES seq  5 permit 2001:0DB8:AAAA::/48 ge 64 le 64
ipv6 prefix-list PEER-A-PREFIXES seq 10 deny any
ipv6 prefix-list PEER-B-PREFIXES seq  5 permit 2001:0DB8:BBBB::/48 ge 64 le 64
ipv6 prefix-list PEER-B-PREFIXES seq 10 deny any
ipv6 prefix-list PEER-C-PREFIXES seq  5 permit 2001:0DB8:CCCC::/48 ge 64 le 64
ipv6 prefix-list PEER-C-PREFIXES seq 10 deny any
route-map RSCLIENT-A-IMPORT permit 10
 match ipv6 address prefix-list PEER-A-PREFIXES
route-map RSCLIENT-A-IMPORT permit 20
 match ipv6 address prefix-list COMMON-PREFIXES
route-map RSCLIENT-B-IMPORT permit 10
 match ipv6 address prefix-list PEER-B-PREFIXES
route-map RSCLIENT-B-IMPORT permit 20
 match ipv6 address prefix-list COMMON-PREFIXES
route-map RSCLIENT-C-IMPORT permit 10
 match ipv6 address prefix-list PEER-C-PREFIXES
route-map RSCLIENT-C-IMPORT permit 20
 match ipv6 address prefix-list COMMON-PREFIXES"""

CONFS = {RA: client_conf(RA), RB: client_conf(RB), RC: client_conf(RC),
         RS: RS_CONF}

LAYOUT = {RA: (0, -260), RB: (-300, 0), IX: (0, 0), RC: (300, 0),
          RS: (0, 260)}
ETHS = {(RA, "eth0"): 180, (RB, "eth0"): 90, (RC, "eth0"): 270,
        (RS, "eth0"): 0}

CARD = r"""
                 ra
                  |
         rb ----- ix ----- rc
                  |
                 rs  (view RS, route-server-client x3)

  Three networks, one session each, to a route server that adds nothing
  to the routes it passes on.
"""

def configure(node):
    frr_setup(node, ["staticd", "bgpd"], CONFS[node])

def entry(node, prefix):
    return vtysh(node, "show bgp ipv6 unicast %s" % prefix)

def transparent(node, prefix, owner):
    """The route came from rs with the owner's next hop, no 65000 in it."""
    out = entry(node, prefix)
    return CLIENT[owner][0] in out and ADDR[owner] in out and \
        "65000" not in out

def main():
    opening()

    step("act 1: every client learns the others' prefixes from rs alone")
    in_parallel(configure, list(ALL), "frr start")
    pa, pb, pc = ("2001:db8:aaaa:1::/64", "2001:db8:bbbb:1::/64",
                  "2001:db8:cccc:1::/64")
    ok_rs = until(lambda: has(RS, "show bgp view RS ipv6 unicast",
                              [pa, pb, pc]), 120)
    proof("rs, the view RS:", vtysh(RS, "show bgp view RS ipv6 unicast"))
    got = count_on(RB, 0, "tcp port 179",
                   lambda: until(lambda: transparent(RB, pa, RA) and
                                 transparent(RB, pc, RC), 90), settle=1.0)
    ok_b = bool(got and got["out"]) or \
        (transparent(RB, pa, RA) and transparent(RB, pc, RC))
    ok_a = until(lambda: transparent(RA, pb, RB) and
                 transparent(RA, pc, RC), 60)
    ok_c = until(lambda: transparent(RC, pa, RA) and
                 transparent(RC, pb, RB), 60)
    proof("rb, the route of ra, learnt from rs:", entry(RB, pa))
    story("    Next hop 2001:db8::a, AS path 65001: rb sends its traffic")
    story("    straight to ra, rs is on the control plane only.")
    hold()

    step("act 2: ra announces a prefix of rb, the import filter of rs refuses it")
    conf(RA, "ipv6 route %s blackhole" % LEAK, "router bgp 65001",
         "address-family ipv6", "network %s" % LEAK)
    ok_recv = until(lambda: has(RS, "show bgp view RS ipv6 unicast neighbors "
                                "2001:db8::a received-routes", [LEAK]), 60)
    ok_refused = ok_recv and \
        lacks(RS, "show bgp view RS ipv6 unicast", [LEAK]) and \
        lacks(RB, "show bgp ipv6 unicast", [LEAK]) and \
        lacks(RC, "show bgp ipv6 unicast", [LEAK])
    proof("rs, what ra sent it:",
          vtysh(RS, "show bgp view RS ipv6 unicast neighbors 2001:db8::a "
                    "received-routes"))
    proof("rc, its table, without %s:" % LEAK,
          vtysh(RC, "show bgp ipv6 unicast"))

    verdict([("rs learns the prefixes of the three clients", ok_rs),
             ("rb learns ra and rc through rs, transparently", ok_b),
             ("ra learns rb and rc through rs, transparently", ok_a),
             ("rc learns ra and rb through rs, transparently", ok_c),
             ("rs receives the leak of ra", ok_recv),
             ("the leak is refused, rb and rc never see it", ok_refused)])

    story("""
the network is left up. Things to try:

  the sessions of the route server:
    cloonix cli %s ssh rs "vtysh -c 'show bgp view RS ipv6 unicast summary'"

  a route as rc sees it:
    cloonix cli %s ssh rc "vtysh -c 'show bgp ipv6 unicast 2001:db8:aaaa:1::/64'"
""" % (NET, NET))

main()
