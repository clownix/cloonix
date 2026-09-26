#!/usr/bin/env python3
"""kvm_mesh - three radios in an 802.11s mesh, no access point at all.

    ./kvm_mesh.py [net]             the net defaults to nemo

Three mesh points on one air, one mesh name::

      m1  ((( kvmwifi )))  m2  ((( kvmwifi )))  m3
   10.42.0.1          10.42.0.2           10.42.0.3

802.11s (2011) makes every radio a relay: the mesh points peer with the
ones they hear and route frames among themselves with HWMP, the protocol
of the standard. The Linux implementation, open80211s, was written at
Cozybit by Javier Cardona and Luis Carlos Cobo, in mac80211 since 2008.

The demo wifi/cnt_mesh.py played by kvm: the cloonix agent of each guest
makes its radios in the guest kernel, and they start on the air kvmwifi.

Image: trixie.qcow2
Size:  3 kvm, 1 air

THE PACKET TO CATCH
-------------------
  The mesh beacons and the peering, action frames, on the air:

      cloonix cli nemo cnf kvmwifi sniff=on
      cloonix cli nemo sniff kvmwifi
"""
import os
import sys
import time

# What every demo shares is in demos/lib/demolib.py, the wifi part in
# demos/lib/wifilib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    NET, check_net, erase_net, hold, in_parallel, place, ssh, step,
    story, tools_present, wait_answer)
from wifilib import (
    air, count_mgmt, create_radio_kvm, wait_radios, ping)

KVM_ROOTFS = "trixie.qcow2"
M1, M2, M3 = "m1", "m2", "m3"
ALL = (M1, M2, M3)
WIRING = {M1: [(0, None, None)], M2: [(0, None, None)],
          M3: [(0, None, None)]}
RADIOS = {M1: 1, M2: 1, M3: 1}
LAYOUT = {M1: (-200, 0), "kvmwifi": (0, 90), M2: (0, 0), M3: (200, 0)}
ETHS = {}
ADDR = {M1: "10.42.0.1", M2: "10.42.0.2", M3: "10.42.0.3"}

def mesh_join(node):
    ssh(node, "iw dev wlan0 set type mp")
    ssh(node, "ip link set dev wlan0 up")
    ssh(node, "iw dev wlan0 mesh join demomesh freq 2412")
    ssh(node, "ip addr add %s/24 dev wlan0" % ADDR[node])

def peers(node):
    out = ssh(node, "iw dev wlan0 station dump 2>&1", check=False)
    return out.count("ESTAB")

def main():
    check_net()
    rootfs = KVM_ROOTFS
    story("802.11s, 2011: every radio a relay. open80211s, Javier Cardona")
    story("and Luis Carlos Cobo at Cozybit, in mac80211 since 2008.")
    story()
    step("erasing the net, then placing the items")
    erase_net()
    place()
    step("three kvm, one radio each")
    in_parallel(lambda n: create_radio_kvm(n, rootfs), list(ALL), "add kvm")
    for n in ALL:
        wait_answer(n, 180)
        wait_radios(n)
    tools_present(M1, "iw")
    step("each radio a mesh point of demomesh, captured as it peers")
    frames = air("kvmwifi", 6, lambda: [mesh_join(n) for n in ALL])
    story()
    story("  %d beacons, %d action frames of peering on the air"
          % (count_mgmt(frames, 8), count_mgmt(frames, 13)))
    end = time.time() + 30
    while time.time() < end and peers(M1) < 2:
        time.sleep(2)
    story("  m1 has %d peer links established" % peers(M1))
    ok, out = ping(M1, ADDR[M3])
    story("  m1 pings m3: %s" % ("OK" if ok else "FAILS"))
    hold()
    if peers(M1) < 2 or not ok:
        sys.exit("the mesh did not form:\n%s" % out)
    story("""
No access point: the three radios found each other by their mesh name and
peered, and the ping crossed the mesh.""")
    story("the network is left up. Erase it, the server stays:")
    story("    cloonix cli %s del all" % NET)

main()
