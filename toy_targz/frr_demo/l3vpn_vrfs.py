#!/usr/bin/env python3
r"""l3vpn_vrfs - BGP/MPLS L3VPN over a segment routing core.

Needs: vrf, mpls_router, mpls_iptunnel.

    ./l3vpn_vrfs.py [net]     the net defaults to nemo

The FRR User Guide, page bgp, sections "L3VPN VRFs" and "VRF Route
Leaking": each customer in a VRF of the provider edge, its routes exported
to the VPN table of the default instance with a route distinguisher, a
route target and a label, imported at the far edge by the route target.
The core labels come from IS-IS segment routing, not LDP.
https://docs.frrouting.org/en/latest/bgp.html#l3vpn-vrfs

    red1  --\                         /--  red2   (192.168.2.10)
             pe1 ---- p ---- pe2
    blue1 --/                         \--  blue2  (192.168.2.20)

  red1 and blue1 are both 192.168.1.10, in the same 192.168.1.0/24.

  !!! KERNEL MODULES NEEDED ON THE HOST: vrf, mpls_router, mpls_iptunnel !!!
  A container cannot load a module. Before playing this demo, on the host,
  once per boot:

sudo modprobe vrf
sudo modprobe mpls_router
sudo modprobe mpls_iptunnel

  or do not run this demo. It stops with a big warning if a module is
  missing.

  ACT 1  IS-IS SR in the core, iBGP pe1-pe2 for the VPN family: the VPN
         table holds 192.168.1.0/24 twice, told apart by the RD.
  ACT 2  red1 reaches red2, blue1 reaches blue2; red1 does not reach blue2,
         nor blue1 red2. On pe1-p a packet carries two labels: 16003, the
         way to pe2, and under it the label of the VRF at pe2.

Image: frr_trixie (zebra, isisd, bgpd)
Size:  7 containers, 6 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf p eth0 sniff=on
    cloonix cli nemo sniff p eth0 --dump "mpls"

  An echo of red1: 16003 above the VPN label above an IP header whose
  addresses exist twice in this network. p never looks below 16003.

Important
  The VRFs are made in each edge before frr starts: RED table 10, BLUE
  table 20. The kernel MPLS forwarding is turned on in the core.
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
from frrlib import forwarding, has, lacks, mpls_on, opening, ping, proof, \
    until, verdict, vtysh

HISTORY = """\
BGP/MPLS VPNs were RFC 2547 in 1999, by Eric Rosen and Yakov Rekhter at
Cisco, and RFC 4364 in 2006: a customer's routes travel in BGP with a
route distinguisher that keeps equal prefixes apart, a route target that
says who imports them, and a label that names the VRF at the far edge.
The core forwards on an outer label and never sees the customers. The
L3VPN of bgpd came from LabN Consulting around 2017.

Today: the "MPLS circuit" a company buys between two sites is this
service, on the shared backbone of a carrier.
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
    PE1: [(0, LC1, "10.0.12.1/24"), (1, LR1, ""), (2, LB1, "")],
    P: [(0, LC1, "10.0.12.2/24"), (1, LC2, "10.0.23.2/24")],
    PE2: [(0, LC2, "10.0.23.3/24"), (1, LR2, ""), (2, LB2, "")],
    RED1: [(0, LR1, "192.168.1.10/24")],
    BLUE1: [(0, LB1, "192.168.1.10/24")],
    RED2: [(0, LR2, "192.168.2.10/24")],
    BLUE2: [(0, LB2, "192.168.2.20/24")],
}
GATEWAY = {RED1: "192.168.1.1", BLUE1: "192.168.1.1",
           RED2: "192.168.2.1", BLUE2: "192.168.2.1"}
NUM = {PE1: 1, P: 2, PE2: 3}
CORE = {PE1: (0,), P: (0, 1), PE2: (0,)}
PEER = {PE1: PE2, PE2: PE1}
# vrf: name, table, interface, its address, route target
VRFS = {
    PE1: [("RED", 10, 1, "192.168.1.1/24", "65000:1"),
          ("BLUE", 20, 2, "192.168.1.1/24", "65000:2")],
    PE2: [("RED", 10, 1, "192.168.2.1/24", "65000:1"),
          ("BLUE", 20, 2, "192.168.2.1/24", "65000:2")],
}

def lo(node):
    return "%d.%d.%d.%d" % ((NUM[node],) * 4)

def frr_conf(node):
    n = NUM[node]
    c = ["hostname %s" % node,
         "interface lo",
         " ip address %s/32" % lo(node),
         " ip router isis SR",
         " isis passive"]
    for num in CORE[node]:
        c += ["interface eth%d" % num,
              " ip router isis SR",
              " isis network point-to-point",
              " isis hello-interval 1"]
    c += ["router isis SR",
          " net 49.0000.0000.0000.000%d.00" % n,
          " is-type level-1",
          " lsp-gen-interval 1",
          " segment-routing on",
          " segment-routing prefix %s/32 index %d" % (lo(node), n)]
    if node not in PEER:
        return "\n".join(c)
    peer = lo(PEER[node])
    c += ["router bgp 65000",
          " bgp router-id %s" % lo(node),
          " no bgp default ipv4-unicast",
          " neighbor %s remote-as 65000" % peer,
          " neighbor %s update-source lo" % peer,
          " address-family ipv4 vpn",
          "  neighbor %s activate" % peer,
          " exit-address-family"]
    for k, (vrf, _t, _e, _a, rt) in enumerate(VRFS[node]):
        c += ["router bgp 65000 vrf %s" % vrf,
              " bgp router-id %s" % lo(node),
              " address-family ipv4 unicast",
              "  redistribute connected",
              "  label vpn export auto",
              "  rd vpn export %s:%d" % (lo(node), k + 1),
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
             pe1 ---- p ---- pe2           (IS-IS SR core, iBGP VPNv4)
    blue1 --/                    \--  blue2

  red1 and blue1 have the same address; each reaches its own far site.
"""

def vrfs_up(node):
    """The VRF devices, their interfaces moved in, then addressed."""
    for vrf, table, num, addr, _rt in VRFS[node]:
        ssh(node, "ip link add %s type vrf table %d" % (vrf, table))
        ssh(node, "ip link set %s up" % vrf)
        ssh(node, "ip link set eth%d master %s" % (num, vrf))
        ssh(node, "ip addr add %s dev eth%d" % (addr, num))

def configure(node):
    forwarding(node)
    mpls_on(node, CORE[node])
    if node in VRFS:
        vrfs_up(node)
    frr_setup(node, ["isisd"] + (["bgpd"] if node in PEER else []),
              frr_conf(node))

def count(node, num, filt, src, dst):
    got = count_on(node, num, filt, lambda: ping(src, dst), settle=1.0)
    if got is None:
        return False, ping(src, dst)
    story("    %d packet(s) \"%s\" on %s eth%d during the ping."
          % (got["count"], filt, node, num))
    return got["count"] > 0, got["out"]

def main():
    opening("vrf", "mpls_router", "mpls_iptunnel")

    step("act 1: the core and the VPN table come up")
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_core = until(lambda: has(PE1, "show ip route 3.3.3.3/32",
                                ["label 16003"]), 90)
    ok_vpn = until(lambda: has(PE2, "show bgp ipv4 vpn", [
        "1.1.1.1:1", "1.1.1.1:2", "192.168.1.0/24"]), 120)
    proof("pe2, the VPN table, the same prefix under two RDs:",
          vtysh(PE2, "show bgp ipv4 vpn"))
    ok_red = until(lambda: has(PE1, "show ip route vrf RED",
                               ["192.168.2.0/24"]), 60)
    ok_blue = until(lambda: has(PE1, "show ip route vrf BLUE",
                                ["192.168.2.0/24"]), 60)
    proof("pe1, the table of RED:", vtysh(PE1, "show ip route vrf RED"))
    ok_main = lacks(PE1, "show ip route", ["192.168.2.0/24"])
    hold()

    step("act 2: each customer reaches its own site, and only it")
    ok_stack, out = count(P, 0, "mpls 16003 and mpls", RED1, "192.168.2.10")
    proof("red1 -> red2:", out)
    ok_r = reached(out)
    ok_vlabel, _o = count(P, 1, "mpls", RED1, "192.168.2.10")
    out = ping(BLUE1, "192.168.2.20")
    proof("blue1 -> blue2, from the same address as red1:", out)
    ok_b = reached(out)
    out = ping(RED1, "192.168.2.20", count=2)
    proof("red1 -> 192.168.2.20, an address of BLUE:", out)
    ok_iso1 = cut_off(out)
    ok_iso2 = cut_off(ping(BLUE1, "192.168.2.10", count=2))
    kernel = ssh(PE1, "ip route show vrf RED 192.168.2.0/24", check=False)
    proof("pe1, the kernel route of RED, two labels pushed:", kernel)

    verdict([("pe1 reaches pe2 over the label 16003", ok_core),
             ("the VPN table holds 192.168.1.0/24 under two RDs", ok_vpn),
             ("RED learns the far site of RED", ok_red),
             ("BLUE learns the far site of BLUE", ok_blue),
             ("the main table of pe1 holds no customer route", ok_main),
             ("two labels, 16003 and the VPN one, on pe1-p", ok_stack),
             ("one label, the VPN one, on p-pe2", ok_vlabel),
             ("red1 reaches red2", ok_r),
             ("blue1 reaches blue2", ok_b),
             ("red1 does not reach BLUE", ok_iso1),
             ("blue1 does not reach RED", ok_iso2)])

    story("""
the network is left up. Things to try:

  the labels of the VRFs, given by bgp, beside those of the SIDs:
    cloonix cli %s ssh pe2 "vtysh -c 'show mpls table'"

  the route targets of each route:
    cloonix cli %s ssh pe1 "vtysh -c 'show bgp ipv4 vpn detail'"

  the core, which holds no customer route:
    cloonix cli %s ssh p "ip route"
""" % (NET, NET, NET))

main()
