#!/usr/bin/env python3
r"""ovn - one logical switch spread across two hosts by an overlay.

    ./ovn.py [net]                 the net defaults to nemo

Three machines on one underlay lan, a controller and two hypervisors::

  hv1 ---+           +--- hv2
         |  underlay |
         +--- ovnc --+
             10.0.0.0/24

ovnc runs ovn-northd over the northbound and southbound databases; hv1 and
hv2 each run Open vSwitch and ovn-controller. One logical switch, sw0, is
defined once, in the northbound database; its ports p1 and p2 land on two
hypervisors, and OVN builds a Geneve tunnel between them, so p1 and p2 share
a layer 2 network with no cable between them.

  ACT 1  ovs and ovn come up on all three, the two controllers connect to
         the southbound database, and the Geneve tunnel appears.
  ACT 2  sw0 is created with p1 and p2, each hypervisor given its port; p1
         pings p2 through the tunnel.
  ACT 3  the underlay link of hv2 is cut: the tunnel has no second path,
         and p1 no longer reaches p2.

Image: ovn_trixie (openvswitch, ovn)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
  Geneve is UDP port 6081, all there is on the underlay, the ping of the
  tenant inside it:

    cloonix cli nemo cnf hv1 eth0 sniff=on
    cloonix cli nemo sniff hv1 eth0 --dump udp port 6081

Important
  Open vSwitch needs its kernel module, which a container of a user can use
  and cannot load. On the host, once per boot, then the demo as a user, the
  net not run as root:

    sudo modprobe openvswitch

  The databases and the logical switch are made at run time.
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
    NET, build, check_net, choose_rootfs, count_on, cut_off, erase_net, hold,
    in_parallel, need_host_modules, place, reached, run, ssh, step, story,
    tools_present)

HISTORY = """\
OVN, the Open Virtual Network, was started around 2015 by the Open vSwitch
team, Ben Pfaff, Justin Pettit and others, to put a control plane on top of
Open vSwitch: a logical network described once, in a northbound database,
and realised across many hosts by a controller on each. It is the network
layer under much of OpenStack and Kubernetes networking.
"""

ROOTFS_CHOICES = ("ovn_trixie",)
HOST_MODULES = ("openvswitch",)

OVNC, HV1, HV2 = "ovnc", "hv1", "hv2"
ALL = (OVNC, HV1, HV2)
UL = "ul"

WIRING = {
    OVNC: [(0, UL, "10.0.0.1/24")],
    HV1:  [(0, UL, "10.0.0.11/24")],
    HV2:  [(0, UL, "10.0.0.12/24")],
}
GATEWAY = {}

# The tenant ports and their fixed addresses on the logical switch sw0.
PORT = {HV1: ("p1", "00:00:00:00:00:01", "10.9.0.1"),
        HV2: ("p2", "00:00:00:00:00:02", "10.9.0.2")}
OVS_CTL = "/usr/share/openvswitch/scripts/ovs-ctl"
OVN_CTL = "/usr/share/ovn/scripts/ovn-ctl"

def nb(cmd):
    return ssh(OVNC, "ovn-nbctl " + cmd, check=False)

def start_central():
    ssh(OVNC, "%s start_northd" % OVN_CTL)
    # Let the two hypervisors reach the databases over tcp.
    ssh(OVNC, "ovn-nbctl set-connection ptcp:6641:0.0.0.0", check=False)
    ssh(OVNC, "ovn-sbctl set-connection ptcp:6642:0.0.0.0", check=False)

def start_hv(node):
    ssh(node, "%s start" % OVS_CTL)
    under = WIRING[node][0][2].split("/")[0]
    ssh(node, "ovs-vsctl set open . external-ids:system-id=%s" % node)
    ssh(node, "ovs-vsctl set open . external-ids:ovn-remote=tcp:10.0.0.1:6642")
    ssh(node, "ovs-vsctl set open . external-ids:ovn-encap-type=geneve")
    ssh(node, "ovs-vsctl set open . external-ids:ovn-encap-ip=%s" % under)
    ssh(node, "%s start_controller" % OVN_CTL)

def make_switch():
    nb("ls-add sw0")
    for node, (p, mac, ip) in PORT.items():
        nb("lsp-add sw0 %s" % p)
        nb('lsp-set-addresses %s "%s %s"' % (p, mac, ip))

def bind_port(node):
    p, mac, ip = PORT[node]
    ssh(node, "ovs-vsctl add-port br-int v%s -- set interface v%s "
              "type=internal external-ids:iface-id=%s" % (p, p, p))
    ssh(node, "ip link set v%s address %s" % (p, mac))
    ssh(node, "ip addr add %s/24 dev v%s" % (ip, p))
    ssh(node, "ip link set v%s up" % p)

def tunnel_up(timeout=40):
    end = time.time() + timeout
    while time.time() < end:
        r = ssh(HV1, "ovs-vsctl show", check=False)
        if "geneve" in r and "10.0.0.12" in r:
            return True
        time.sleep(2)
    return False

def reachable(src_node, dst_ip, timeout=40):
    p = PORT[src_node][0]
    end = time.time() + timeout
    while time.time() < end:
        r = run('cloonix cli %s ssh %s "ping -c1 -W1 -I v%s %s"'
                % (NET, src_node, p, dst_ip), quiet=True)
        if r.returncode == 0:
            return True
        time.sleep(2)
    return False

def ping(src_node, dst_ip, count=5):
    p = PORT[src_node][0]
    return ssh(src_node, "ping -c %d -I v%s %s" % (count, p, dst_ip),
               check=False)

LAYOUT = {HV1: (-300, -120), OVNC: (0, 140), HV2: (300, -120),
          UL: (0, -40)}
ETHS = {(HV1, "eth0"): 120, (HV2, "eth0"): 180, (OVNC, "eth0"): 90}

CARD = r"""
   hv1 ---+   underlay 10.0.0.0/24   +--- hv2
          +-------- ovnc ------------+
   p1 10.9.0.1                       p2 10.9.0.2   (logical switch sw0)

  One logical switch, two hypervisors. p1 and p2 share a layer-2 network
  carried between the hosts by a Geneve tunnel; no cable joins them.
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

    step("creating and cabling the three machines")
    build(rootfs)
    tools_present(OVNC, "ovn-nbctl", "ovn-northd")
    tools_present(HV1, "ovs-vsctl", "ovn-controller")

    step("act 1: bring up ovs and ovn, wait for the tunnel")
    story()
    start_central()
    in_parallel(start_hv, [HV1, HV2], "start hv")
    need_host_modules(HV1, HOST_MODULES)
    if not tunnel_up():
        sys.exit("the Geneve tunnel between hv1 and hv2 never formed")
    story("    hv1 sees the tunnel to hv2 (10.0.0.12):")
    for line in ssh(HV1, "ovs-vsctl show", check=False).strip().split("\n"):
        if "geneve" in line or "Port" in line or "Bridge" in line:
            story("      %s" % line.strip())
    hold()

    step("act 2: one logical switch, a port on each host")
    story()
    make_switch()
    in_parallel(bind_port, [HV1, HV2], "bind port")
    if not reachable(HV1, "10.9.0.2"):
        sys.exit("p1 never reached p2 across the overlay")
    r = ping(HV1, "10.9.0.2")
    story("    p1 (hv1) -> p2 (hv2), same logical switch, over the tunnel:")
    story(r.strip())
    ok_over = reached(r)
    story()

    step("act 3: cut hv2's underlay, and the overlay goes with it")
    story()
    got = count_on(HV1, 0, "udp port 6081",
                   lambda: (ssh(HV2, "ip link set eth0 down", check=False),
                            time.sleep(6))[0], settle=6.0)
    story("    hv2's underlay interface is taken down. Geneve on hv1 eth0")
    story("    during the cut:")
    if got is not None:
        story("      %d geneve packet(s) seen as it broke" % got["count"])
    story()
    lost = ping(HV1, "10.9.0.2", count=3)
    story("    p1 -> p2 with the underlay cut:")
    story(lost.strip())
    ok_broken = cut_off(lost)
    story()
    story("    The logical switch is intact in the database, but its two")
    story("    ports were only joined by the tunnel, and the tunnel needs")
    story("    the underlay. Cut it and the overlay has nowhere to go.")
    story()

    if not (ok_over and ok_broken):
        sys.exit("a check failed: over=%s broken=%s" % (ok_over, ok_broken))

    story("""
the network is left up. Things to try:

  the logical network, and where OVN placed each port:
    cloonix cli %s ssh ovnc "ovn-nbctl show"
    cloonix cli %s ssh ovnc "ovn-sbctl show"

  the tunnel and bridges Open vSwitch built on a host:
    cloonix cli %s ssh hv1 "ovs-vsctl show"
""" % (NET, NET, NET))

main()
