#!/usr/bin/env python3
r"""evpn_sample - Sample Configuration (EVPN) (needs: vxlan, bridge).

    ./evpn_sample.py [net]     the net defaults to nemo

The example of the FRR User Guide, page evpn, sections "Sample
Configuration" and "Traditional Bridges and Traditional VXLAN Devices",
reduced to its L2VNI 110 (br10, vni110):
https://docs.frrouting.org/en/latest/evpn.html#evpn-sample-configuration

    h1 -- lan1 -- vtep1 ===== core 192.168.122.0/24 ===== vtep2 -- lan2 -- h2
    10.0.10.11    100.64.0.1                       100.64.0.2    10.0.10.12
                  br10 + vni110                    br10 + vni110

  !!! KERNEL MODULES NEEDED ON THE HOST: vxlan and bridge !!!
  The VTEPs make a vxlan device and a bridge, and a container cannot load
  a module. Before playing this demo, on the host, once per boot:

sudo modprobe vxlan
sudo modprobe bridge

  or do not run this demo. It stops with a big warning if a module is
  missing.

  Each VTEP carries the guide's iBGP session in AS 4200000000, its VTEP-IP
  announced with "network", "advertise-all-vni" in l2vpn evpn, and the
  guide's iproute2 lines for br10 and vni110 (VTEP-IP, nolearning,
  neigh_suppress, learning off), with the host port enslaved directly.

  ACT 1  the iBGP session comes up, each VTEP learns the VTEP-IP of the
         other, and the type-3 routes build the flood list of VNI 110.
  ACT 2  h1 pings h2, in the same subnet across the routed core: the MACs
         are learnt as type-2 routes, the frames ride VXLAN.

Image: frr_trixie (zebra, bgpd)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf vtep1 eth0 sniff=on
    cloonix cli nemo sniff vtep1 eth0 --dump "udp port 4789"

  An echo of h1 inside a VXLAN packet, VNI 110, from 100.64.0.1 to
  100.64.0.2: an ethernet frame of lan1 carried over the routed core.

Important
  The VRFs of the guide (vrf1, vrf2 and their L3VNIs) need the vrf module
  too and are left out; with no SVI address, "advertise-svi-ip" has
  nothing to say and is left out; the static bridge MAC is not set.
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
    ssh, step, story, tools_present
from frrlib import has, opening, ping, pings, proof, until, verdict, vtysh

HISTORY = """\
EVPN, RFC 7432 in 2015 by Ali Sajassi and colleagues, puts the MAC
addresses of an ethernet segment into BGP, so that a layer 2 network
spread over a routed core learns them by routing rather than by
flooding; RFC 8365 married it to VXLAN, the encapsulation of RFC 7348.
The EVPN of frr was written at Cumulus Networks from 2017, by Mitesh
Kanjariya, Vivek Venkatraman, Chirag Shah and others.

Today: EVPN-VXLAN is the standard overlay of data centre fabrics, from
the large clouds to the enterprise.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

V1, V2, H1, H2 = "vtep1", "vtep2", "h1", "h2"
ALL = (V1, V2, H1, H2)
VTEPS = (V1, V2)
L1, CORE, L2 = "lan1", "core", "lan2"

WIRING = {
    V1: [(0, CORE, "192.168.122.11/24"), (1, L1, None)],
    V2: [(0, CORE, "192.168.122.12/24"), (1, L2, None)],
    H1: [(0, L1, "10.0.10.11/24")],
    H2: [(0, L2, "10.0.10.12/24")],
}
GATEWAY = {}
VTEP_IP = {V1: "100.64.0.1", V2: "100.64.0.2"}
PEER = {V1: "192.168.122.12", V2: "192.168.122.11"}

def linux_lines(node):
    ip = VTEP_IP[node]
    return (
        "ip addr add %s/32 dev lo" % ip,
        "ip link add br10 type bridge",
        "ip link add vni110 type vxlan local %s dstport 4789 id 110 "
        "nolearning" % ip,
        "ip link set vni110 master br10 addrgenmode none",
        "ip link set vni110 type bridge_slave neigh_suppress on learning off",
        "ip link set vni110 up",
        "ip link set br10 up",
        "ip link set eth1 master br10")

def frr_conf(node):
    return "\n".join([
        "hostname %s" % node,
        "router bgp 4200000000",
        " neighbor %s remote-as internal" % PEER[node],
        " address-family ipv4 unicast",
        "  network %s/32" % VTEP_IP[node],
        " exit-address-family",
        " address-family l2vpn evpn",
        "  neighbor %s activate" % PEER[node],
        "  advertise-all-vni",
        " exit-address-family"])

LAYOUT = {
    H1: (-560, 0), L1: (-420, 0), V1: (-240, 0), CORE: (0, 0),
    V2: (240, 0), L2: (420, 0), H2: (560, 0),
}
ETHS = {
    (H1, "eth0"): 90, (V1, "eth1"): 270, (V1, "eth0"): 90,
    (V2, "eth0"): 270, (V2, "eth1"): 90, (H2, "eth0"): 270,
}

CARD = r"""
    h1 -- vtep1 ===== core ===== vtep2 -- h2

  One layer 2 segment, VNI 110, over a routed core; the MACs travel in BGP.
"""

def configure(node):
    for line in linux_lines(node):
        ssh(node, line)
    ssh(node, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    frr_setup(node, ["bgpd"], frr_conf(node))

def main():
    opening("vxlan", "bridge")
    tools_present(V1, "bridge")

    step("act 1: iBGP comes up, VNI 110 gets its remote VTEP")
    in_parallel(configure, list(VTEPS), "frr start")
    ok_vtep = until(lambda: has(V1, "show ip route bgp", ["100.64.0.2/32"]),
                    90)
    ok_imet = until(lambda: has(V1, "show evpn vni 110", ["100.64.0.2"]), 90)
    proof("vtep1, the VNI:", vtysh(V1, "show evpn vni 110"))
    proof("vtep1, the type-3 routes:",
          vtysh(V1, "show bgp l2vpn evpn route type multicast"))
    hold()

    step("act 2: h1 pings h2 across the core, the MACs ride BGP")
    got = count_on(V1, 0, "udp port 4789",
                   lambda: until(lambda: pings(H1, "10.0.10.12"), 60),
                   settle=1.0)
    ok_vxlan = bool(got and got["count"] > 0)
    r = ping(H1, "10.0.10.12")
    proof("h1 -> h2:", r)
    ok_ping = reached(r)
    ok_mac = until(lambda: "remote" in vtysh(V1, "show evpn mac vni 110"),
                   30)
    proof("vtep1, the MACs of VNI 110:", vtysh(V1, "show evpn mac vni 110"))
    proof("vtep1, the type-2 routes:",
          vtysh(V1, "show bgp l2vpn evpn route type macip"))
    if got is not None:
        story("    %d VXLAN packet(s) on the core side of vtep1."
              % got["count"])

    verdict([("vtep1 learns the VTEP-IP of vtep2", ok_vtep),
             ("VNI 110 has vtep2 as remote VTEP", ok_imet),
             ("h1 reaches h2 across the core", ok_ping),
             ("the frames cross the core in VXLAN", ok_vxlan),
             ("vtep1 holds the remote MAC of h2", ok_mac)])

    story("""
the network is left up. Things to try:

  the forwarding database zebra wrote into the bridge:
    cloonix cli %s ssh vtep1 "bridge fdb show dev vni110"

  the EVPN routes:
    cloonix cli %s ssh vtep1 "vtysh -c 'show bgp l2vpn evpn'"
""" % (NET, NET))

main()
