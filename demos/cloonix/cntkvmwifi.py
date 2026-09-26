#!/usr/bin/env python3
"""cntkvmwifi - a container and a kvm, a radio each, and a ping over the air.

    ./cntkvmwifi.py [net]        the net defaults to nemo

The wifi of cloonix/cntwifi.py and cloonix/kvmwifi.py, played by a container
and a kvm on one air::

      c1  (((  cntwifi  )))  k1
  10.45.0.1              10.45.0.2

wlan=1 gives each a radio, wlan0. The radio of the container is made by
suid-power in the kernel of the host, the radio of the kvm by the cloonix
agent in the kernel of the guest; suid-power is the medium of every air, so
both kinds meet on one air. The container radio starts on the air cntwifi,
the kvm radio on kvmwifi: one plug puts the kvm radio on cntwifi. The two
radios join one ad hoc network, no access point and no key, and ping each
other both ways.

Image: trixie, trixie.qcow2
Size:  1 cnt, 1 kvm, 1 air

THE PACKET TO CATCH
-------------------
  The ping on the air, decoded under the 802.11 header:

      cloonix cli nemo cnf cntwifi sniff=on
      cloonix cli nemo sniff cntwifi

  or double-click the air cntwifi, then the button Data only of webshark.
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
    NET, check_net, choose_rootfs, cli, erase_net, hold, in_parallel, place,
    ssh, step, story, tools_present, wait_answer)
from wifilib import (
    air, count_icmp, create_radio_cnt, create_radio_kvm, hwsim_on_host, ping,
    wait_radios)

ROOTFS_CHOICES = ("trixie",)
KVM_ROOTFS = "trixie.qcow2"
C1, K1 = "c1", "k1"
ALL = (C1, K1)
AIR = "cntwifi"
WIRING = {C1: [(0, None, None)], K1: [(0, None, None)]}
RADIOS = {C1: 1, K1: 1}
LAYOUT = {C1: (-150, 0), AIR: (0, 0), K1: (150, 0)}
ETHS = {(C1, "eth0"): 270, (K1, "eth0"): 90}
ADDR = {C1: "10.45.0.1", K1: "10.45.0.2"}

def bssid(node):
    """The cell wlan0 joined, empty before; iw prints Joined IBSS <bssid>."""
    out = ssh(node, "iw dev wlan0 link 2>&1", check=False).split()
    return out[out.index("IBSS") + 1] if "IBSS" in out else ""

def wait_cell(node, want="", secs=30):
    """The cell of node, any once joined, or want."""
    end = time.time() + secs
    while time.time() < end:
        cell = bssid(node)
        if cell and (not want or cell == want):
            return cell
        time.sleep(2)
    return bssid(node)

def adhoc(node):
    ssh(node, "iw dev wlan0 set type ibss")
    ssh(node, "ip link set dev wlan0 up")
    ssh(node, "iw dev wlan0 ibss join demoadhoc 2412")
    ssh(node, "ip addr add %s/24 dev wlan0" % ADDR[node])

def create(node, rootfs):
    if node == C1:
        create_radio_cnt(node, rootfs)
    else:
        create_radio_kvm(node, KVM_ROOTFS)

def ping_until(a, b, secs=60):
    ok, out = False, ""
    end = time.time() + secs
    while time.time() < end and not ok:
        ok, out = ping(a, ADDR[b])
        if not ok:
            time.sleep(3)
    if not ok:
        sys.exit("%s does not ping %s over the air:\n%s" % (a, b, out))

def main():
    hwsim_on_host()
    check_net()
    rootfs = choose_rootfs()
    step("erasing the net, then placing the items")
    erase_net()
    place()
    step("a container and a kvm, one radio each")
    in_parallel(lambda n: create(n, rootfs), list(ALL), "add cnt, add kvm")
    wait_answer(C1)
    wait_answer(K1, 180)
    tools_present(C1, "iw")
    wait_radios(K1)
    step("the radio of the kvm put on cntwifi, the air of the container")
    r = cli("plug", K1, "wlan0", AIR)
    if r.returncode:
        sys.exit("wlan0 of k1 would not go on %s: %s"
                 % (AIR, (r.stdout + r.stderr).strip()))
    step("c1 creates the ad hoc cell, k1 hears it and joins")
    adhoc(C1)
    cell = wait_cell(C1)
    if not cell:
        sys.exit("c1 made no ad hoc cell:\n%s"
                 % ssh(C1, "iw dev wlan0 link 2>&1", check=False))
    adhoc(K1)
    if wait_cell(K1, cell) != cell:
        sys.exit("k1 is in the cell %s, not in %s of c1: the beacons of c1 "
                 "do not reach k1 over %s" % (bssid(K1) or "none", cell, AIR))
    step("c1 pings k1, k1 pings c1")
    ping_until(C1, K1)
    ping_until(K1, C1)
    step("the same ping, caught on the air cntwifi")
    frames = air(AIR, 6, lambda: ping(K1, ADDR[C1]))
    nb = count_icmp(frames)
    story()
    story("  c1 and k1 ping each other over the air cntwifi: OK")
    story("  %d ICMP frames caught on the air, %d frames in all"
          % (nb, len(frames)))
    story()
    hold()
    if not nb:
        sys.exit("the ping went, but the capture of cntwifi shows no ICMP")
    story("""
A container radio and a kvm radio, one air, a ping. The network is left up.
The capture of the air:
    cloonix cli %s sniff cntwifi""" % NET)

main()
