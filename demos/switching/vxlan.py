#!/usr/bin/env python3
r"""vxlan - one layer 2 network carried over a routed layer 3 core.

    ./vxlan.py [net]               the net defaults to nemo

Two sites, each a host and a VTEP, joined by a routed core they do not
share a subnet with::

    h1 -- lan1 -- vtep1 == core == vtep2 -- lan2 -- h2
   10.0.0.1        192.168.1.1   192.168.2.1        10.0.0.2

  h1 and h2 are on 10.0.0.0/24 and believe they share one wire. They do
  not: their frames are wrapped by the VTEPs in UDP and sent across the
  192.168.0.0/16 core, which knows nothing of 10.0.0.0/24.

  ACT 1  the core alone: vtep1 reaches vtep2 by a routed hop, and h1
         cannot reach h2 -- there is no layer 2 path yet.
  ACT 2  a vxlan interface on each VTEP, vni 100, over the core. h1 and
         h2 are now on one broadcast domain; h1 arps for h2 and pings it.
  ACT 3  the arp of h1 is shown to be a broadcast the core carried as a
         unicast UDP datagram between the VTEPs.

Image: frr_trixie (iproute2)
Size:  5 containers, 3 lans

THE PACKET TO CATCH
-------------------
  VXLAN is UDP to port 4789. On a core interface the inner frame, arp or
  icmp, rides inside it:

    cloonix cli nemo cnf vtep1 eth1 sniff=on
    cloonix cli nemo sniff vtep1 eth1 --dump udp port 4789

  The packet worth catching is the first: h1's arp for h2, a layer 2
  broadcast, crossing the routed core as one UDP datagram to vtep2.

Needs on the host, once per boot
  The vxlan kernel module, as kvm needs kvm_intel. A container run by an
  ordinary user cannot load a module; it uses one already on the host:

    sudo modprobe vxlan

Important
  Nothing is baked into the image: the vxlan interfaces are made at run
  time with ip, no configuration file at all.
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
    NET, build, check_net, choose_rootfs, count_on, erase_net, hold,
    module_on_host, place, reached, ssh, step, story, tools_present)

HISTORY = """\
VXLAN was written by VMware, Cisco, Arista and others and published as RFC
7348 in 2014, with Mallik Mahalingam and Dinesh Dutt among its authors. A
data centre had run out of the 4094 VLAN tags of 802.1Q long before it ran
out of tenants, and moving a virtual machine meant its layer 2 network had
to follow it across routers. VXLAN answers both: a 24-bit identifier, sixteen
million networks, and the frames wrapped in UDP so any routed IP fabric
carries them unchanged. The linux side was written by Stephen Hemminger and
David Stevens.

Today: VXLAN is the overlay of nearly every large data centre and of the
public clouds, usually with EVPN, a BGP family, distributing the addresses in
place of the flood-and-learn this demo shows. It is the transport OVN builds
on, the subject of switching/ovn.py.
"""

ROOTFS_CHOICES = ("frr_trixie",)

H1, V1, V2, H2 = "h1", "vtep1", "vtep2", "h2"
CORE = "core"
ALL = (H1, V1, CORE, V2, H2)
L1, C1, C2, L2 = "lan1", "core1", "core2", "lan2"

WIRING = {
    H1:   [(0, L1, "10.0.0.1/24")],
    V1:   [(0, L1, None), (1, C1, "192.168.1.1/24")],
    CORE: [(0, C1, "192.168.1.254/24"), (1, C2, "192.168.2.254/24")],
    V2:   [(0, L2, None), (1, C2, "192.168.2.1/24")],
    H2:   [(0, L2, "10.0.0.2/24")],
}
GATEWAY = {}
# The far VTEP each VTEP is told to reach on the core.
REMOTE = {V1: "192.168.2.1", V2: "192.168.1.1"}
CORE_HOP = {V1: "192.168.1.254", V2: "192.168.2.254"}
VNI = 100
VXPORT = 4789

def core_up(node):
    ssh(node, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")

def vtep_route(node):
    # The far VTEP is one routed hop away, through the core router.
    ssh(node, "ip route add %s/32 via %s"
        % (REMOTE[node], CORE_HOP[node]))

def vxlan_up(node):
    # The overlay made with ip alone: a vxlan device to the far VTEP on the
    # core, bridged to the site by putting the site interface and the vxlan
    # in one bridge.
    ssh(node, "ip link add vxlan%d type vxlan id %d remote %s "
              "dstport %d dev eth1"
        % (VNI, VNI, REMOTE[node], VXPORT))
    ssh(node, "ip link add br0 type bridge")
    ssh(node, "ip link set eth0 master br0")
    ssh(node, "ip link set vxlan%d master br0" % VNI)
    ssh(node, "ip link set eth0 up")
    ssh(node, "ip link set vxlan%d up" % VNI)
    ssh(node, "ip link set br0 up")

def reach(src, dst, count=5):
    return ssh(src, "ping -c %d -W 1 %s" % (count, dst), check=False)

LAYOUT = {
    H1: (-560, 0), L1: (-410, 0), V1: (-260, 0),
    C1: (-110, 0), CORE: (0, 0), C2: (110, 0),
    V2: (260, 0), L2: (410, 0), H2: (560, 0),
}
ETHS = {
    (H1, "eth0"): 90, (V1, "eth0"): 270, (V1, "eth1"): 90,
    (CORE, "eth0"): 270, (CORE, "eth1"): 90,
    (V2, "eth0"): 90, (V2, "eth1"): 270, (H2, "eth0"): 270,
}

CARD = r"""
   h1 -- lan1 -- vtep1 == core == vtep2 -- lan2 -- h2
  10.0.0.1        192.168.1.1   192.168.2.1        10.0.0.2

  h1 and h2 share no wire; the VTEPs wrap their frames in UDP 4789 and the
  routed core carries them. One layer 2 network over a layer 3 fabric.
"""

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

    step("creating and cabling the five machines")
    build(rootfs)
    tools_present(V1, "ip", "bridge")

    if not module_on_host(V1, "vxlan"):
        sys.exit("the vxlan module is not on the host.\n"
                 "  a container run as your normal user cannot load a kernel\n"
                 "  module, but it can use one already on the host. On the\n"
                 "  host, once per boot, run as yourself:\n"
                 "    sudo modprobe vxlan\n"
                 "  then start this demo again as your normal user.")

    step("act 1: the core routes; h1 cannot yet reach h2")
    story()
    core_up(CORE)
    vtep_route(V1)
    vtep_route(V2)
    r = reach(V1, REMOTE[V1], count=3)
    story("    vtep1 -> vtep2, across the routed core:")
    story(r.strip())
    ok_core = reached(r)
    story()
    r = reach(H1, "10.0.0.2", count=2)
    story("    h1 -> h2, with no overlay yet:")
    story(r.strip())
    no_l2 = not reached(r)
    story()
    story("    The two sites are islands: 10.0.0.0/24 exists on lan1 and on")
    story("    lan2, and nothing joins them, the core not knowing that subnet.")
    hold()

    step("act 2: a vxlan on each VTEP, and the two sites become one")
    story()
    vxlan_up(V1)
    vxlan_up(V2)
    time.sleep(2)
    r = reach(H1, "10.0.0.2")
    story("    h1 -> h2, now on one broadcast domain over the overlay:")
    story(r.strip())
    ok_overlay = reached(r)
    story()
    story("    h1, its arp table: h2 is a neighbour on the same wire, as far")
    story("    as h1 can tell.")
    story(ssh(H1, "ip neigh show dev eth0", check=False).rstrip())
    hold()

    step("act 3: h1's arp broadcast, carried as a unicast UDP datagram")
    story()
    ssh(H1, "ip neigh flush dev eth0", check=False)
    got = count_on(V1, 1, "udp port %d" % VXPORT,
                   lambda: reach(H1, "10.0.0.2", count=3), settle=3.0)
    if got is not None:
        story("    %d vxlan datagram(s) on vtep1's core interface while h1"
              % got["count"])
        story("    resolved and pinged h2: the arp broadcast and the echo,")
        story("    each wrapped in UDP to port %d and sent to vtep2." % VXPORT)
        story()
    story("    the outer addresses are the VTEPs on the core; the inner frame")
    story("    is h1 to h2 on 10.0.0.0/24, which the core never sees.")
    hold()

    if not (ok_core and no_l2 and ok_overlay):
        sys.exit("a check failed: core=%s isolated_before=%s overlay=%s"
                 % (ok_core, no_l2, ok_overlay))

    story("""
the network is left up. Things worth trying:

  the overlay traffic on the core, inner frame and all:
    cloonix cli %s sniff vtep1 eth1 --dump udp port 4789

  the forwarding database vxlan learnt, mac to remote VTEP:
    cloonix cli %s ssh vtep1 "bridge fdb show dev vxlan100"

  take the overlay down and watch the two sites part again:
    cloonix cli %s ssh vtep1 "ip link set vxlan100 down"
    cloonix cli %s ssh h1 "ping -c 2 10.0.0.2"
""" % (NET, NET, NET, NET))

main()
