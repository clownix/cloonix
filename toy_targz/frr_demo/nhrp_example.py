#!/usr/bin/env python3
r"""nhrp_example - Configuration Example (NHRP) (needs: ip_gre).

    ./nhrp_example.py [net]     the net defaults to nemo

The example of the FRR User Guide, page nhrpd, section "Configuration
Example": its "HUB configuration example", "Spoke1 configuration" and
"Spoke2 configuration", a DMVPN.
https://docs.frrouting.org/en/latest/nhrpd.html#configuration-example

                        hub (gre1 10.0.0.254, nbma 198.51.100.1)
                         |
    lan1 -- spoke1 ---- wan 198.51.100.0/24 ---- spoke2 -- lan2
    h1      gre1 10.0.0.1                        gre1 10.0.0.2   h2
    172.16.1.0/24                                       172.16.2.0/24

  !!! KERNEL MODULE NEEDED ON THE HOST: ip_gre !!!
  The tunnels are multipoint GRE devices, and a container cannot load a
  module. Before playing this demo, on the host, once per boot:

sudo modprobe ip_gre

  or do not run this demo. It stops with a big warning if the module is
  missing.

  Each router carries the guide's configuration: gre1 with key 42,
  network-id 1, the spokes registering with "ip nhrp nhs dynamic nbma
  198.51.100.1", and BGP over the tunnel, the hub redistributing nhrp.

  ACT 1  the spokes register with the hub: its NHRP cache maps 10.0.0.1
         and 10.0.0.2 to their NBMA addresses.
  ACT 2  BGP comes up over the tunnels, each spoke learns the lan of the
         other, h1 reaches h2 through the hub.

Image: frr_trixie (zebra, staticd, nhrpd, bgpd)
Size:  5 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf hub eth0 sniff=on
    cloonix cli nemo sniff hub eth0 --dump "ip proto 47"

  A registration request of a spoke: a GRE packet of protocol type 0x2001,
  NHRP, carrying its tunnel address and its NBMA address.

Important
  The IPsec of the guide ("tunnel protection vici profile dmvpn", the
  strongSwan ipsec.conf) and the iptables NFLOG rule for shortcuts are
  left out: frr_trixie has neither strongSwan nor iptables. The tunnels
  are made as in the nhrp_topo test of frr, with their local address and
  device given.
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
    ssh, step, story
from frrlib import forwarding, has, opening, ping, pings, proof, until, \
    verdict, vtysh

HISTORY = """\
NHRP, RFC 2332 in 1998 by James Luciani, Dave Katz and colleagues, was
written for ATM and frame relay: a router asks a server for the link
layer address of another one, to talk to it directly. Cisco reused it in
DMVPN in the 2000s, with GRE and IPsec over the internet. Timo Teras
wrote OpenNHRP for linux, then the nhrpd of Quagga in 2014, now in frr.

Today: DMVPN joins the branches of many companies to their headquarters
over plain internet links.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

HUB, S1, S2, H1, H2 = "hub", "spoke1", "spoke2", "h1", "h2"
ALL = (HUB, S1, S2, H1, H2)
ROUTERS = (HUB, S1, S2)
WAN, L1, L2 = "wan", "lan1", "lan2"

WIRING = {
    HUB: [(0, WAN, "198.51.100.1/24")],
    S1: [(0, WAN, "198.51.100.11/24"), (1, L1, "172.16.1.1/24")],
    S2: [(0, WAN, "198.51.100.12/24"), (1, L2, "172.16.2.1/24")],
    H1: [(0, L1, "172.16.1.10/24")],
    H2: [(0, L2, "172.16.2.10/24")],
}
GATEWAY = {H1: "172.16.1.1", H2: "172.16.2.1"}
TUN = {HUB: "10.0.0.254", S1: "10.0.0.1", S2: "10.0.0.2"}
NBMA = {HUB: "198.51.100.1", S1: "198.51.100.11", S2: "198.51.100.12"}

HUB_CONF = """\
hostname hub
ip route 172.16.0.0/24 blackhole
interface gre1
 description DMVPN Tunnel Interface
 ip address 10.0.0.254/32
 ip nhrp network-id 1
 ip nhrp redirect
 ip nhrp registration no-unique
 ip nhrp shortcut
 tunnel source eth0
router bgp 65000
 bgp router-id 10.0.0.254
 no bgp ebgp-requires-policy
 neighbor SPOKES peer-group
 neighbor SPOKES disable-connected-check
 neighbor 10.0.0.1 remote-as 65001
 neighbor 10.0.0.1 peer-group SPOKES
 neighbor 10.0.0.2 remote-as 65002
 neighbor 10.0.0.2 peer-group SPOKES
 address-family ipv4 unicast
  network 172.16.0.0/24
  redistribute nhrp
 exit-address-family"""

def spoke_conf(node, asn, lan):
    return "\n".join([
        "hostname %s" % node,
        "interface gre1",
        " description DMVPN Tunnel Interface",
        " ip address %s/32" % TUN[node],
        " ip nhrp network-id 1",
        " ip nhrp nhs dynamic nbma 198.51.100.1",
        " ip nhrp redirect",
        " ip nhrp registration no-unique",
        " ip nhrp shortcut",
        " no link-detect",
        " tunnel source eth0",
        "router bgp %s" % asn,
        " no bgp ebgp-requires-policy",
        " neighbor 10.0.0.254 remote-as 65000",
        " neighbor 10.0.0.254 disable-connected-check",
        " address-family ipv4 unicast",
        "  network %s" % lan,
        " exit-address-family"])

CONFS = {HUB: HUB_CONF, S1: spoke_conf(S1, "65001", "172.16.1.0/24"),
         S2: spoke_conf(S2, "65002", "172.16.2.0/24")}
DAEMONS = {HUB: ["staticd", "nhrpd", "bgpd"], S1: ["nhrpd", "bgpd"],
           S2: ["nhrpd", "bgpd"]}

LAYOUT = {
    HUB: (0, -220), WAN: (0, 0), S1: (-260, 0), S2: (260, 0),
    L1: (-260, 160), H1: (-260, 300), L2: (260, 160), H2: (260, 300),
}
ETHS = {
    (HUB, "eth0"): 180, (S1, "eth0"): 90, (S2, "eth0"): 270,
    (S1, "eth1"): 180, (S2, "eth1"): 180, (H1, "eth0"): 0, (H2, "eth0"): 0,
}

CARD = r"""
                 hub
                  |
    h1 - spoke1 - wan - spoke2 - h2

  Multipoint GRE over the wan: the spokes register their address with the
  hub, BGP runs over the tunnels.
"""

def configure(node):
    forwarding(node)
    ssh(node, "ip tunnel add gre1 mode gre ttl 64 key 42 dev eth0 "
              "local %s remote 0.0.0.0" % NBMA[node])
    ssh(node, "ip addr add %s/32 dev gre1" % TUN[node])
    ssh(node, "ip link set gre1 up")
    frr_setup(node, DAEMONS[node], CONFS[node])

def bgp_up(node, peer):
    return has(node, "show bgp neighbors %s" % peer,
               ["BGP state = Established"])

def main():
    opening("ip_gre")

    step("act 1: the spokes register with the hub")
    got = count_on(HUB, 0, "ip proto 47", lambda: (
        in_parallel(configure, list(ROUTERS), "frr start"),
        until(lambda: has(HUB, "show ip nhrp cache",
                          ["10.0.0.1", "10.0.0.2"]), 90))[1], settle=1.0)
    ok_reg = got["out"] if got else \
        has(HUB, "show ip nhrp cache", ["10.0.0.1", "10.0.0.2"])
    if got is not None:
        story("    %d GRE packet(s) on the wan side of the hub meanwhile."
              % got["count"])
    ok_nhs = has(S1, "show ip nhrp cache", ["10.0.0.254", "198.51.100.1"])
    proof("hub, its NHRP cache:", vtysh(HUB, "show ip nhrp cache"))
    proof("spoke1, its NHRP cache:", vtysh(S1, "show ip nhrp cache"))
    hold()

    step("act 2: BGP over the tunnels, h1 reaches h2 through the hub")
    ok_bgp = until(lambda: bgp_up(HUB, "10.0.0.1") and
                   bgp_up(HUB, "10.0.0.2"), 120)
    ok_routes = until(lambda: has(S1, "show ip route bgp",
                                  ["172.16.2.0/24"]), 60)
    proof("hub, its BGP sessions:", vtysh(HUB, "show bgp summary"))
    proof("spoke1, its BGP routes:", vtysh(S1, "show ip route bgp"))
    until(lambda: pings(H1, "172.16.2.10"), 30)
    r = ping(H1, "172.16.2.10")
    proof("h1 -> h2:", r)
    ok_ping = reached(r)

    verdict([("the hub maps both spokes in its cache", ok_reg),
             ("spoke1 knows its NHS, 10.0.0.254 at 198.51.100.1", ok_nhs),
             ("BGP is up over both tunnels", ok_bgp),
             ("spoke1 learns 172.16.2.0/24", ok_routes),
             ("h1 reaches h2", ok_ping)])

    story("""
the network is left up. Things to try:

  the next hop servers of a spoke:
    cloonix cli %s ssh spoke1 "vtysh -c 'show ip nhrp nhs'"

  the routes nhrpd installed:
    cloonix cli %s ssh hub "vtysh -c 'show ip route nhrp'"
""" % (NET, NET))

main()
