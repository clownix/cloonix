#!/usr/bin/env python3
"""kvmwifi - two virtual machines with a radio each, and a ping over the air.

    ./kvmwifi.py [net]           the net defaults to nemo

The wifi of cloonix/cntwifi.py, played by two kvm::

      k1  (((  kvmwifi  )))  k2
  10.44.0.1              10.44.0.2

wlan=1 gives a kvm a radio, wlan0: the cloonix agent of the guest loads
mac80211_hwsim of the guest kernel and makes the radio, then carries its
frames to the host, where doorways relays them to suid-power, the medium of
every air, which gives them to the other radios of the air. The air is
kvmwifi, the amber lan of the canvas, joined by every kvm radio. The host
loads no module. The two radios join one ad hoc network, no access
point and no key, and ping each other.

Image: trixie.qcow2
Size:  2 kvm, 1 air

THE PACKET TO CATCH
-------------------
  The ping on the air, decoded under the 802.11 header:

      cloonix cli nemo cnf kvmwifi sniff=on
      cloonix cli nemo sniff kvmwifi

  or double-click the air kvmwifi, then the button Data only of webshark.
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
    NET, check_net, cli, erase_net, hold, in_parallel, place, ssh, step,
    story, wait_answer)
from wifilib import air, count_icmp, ping

KVM_ROOTFS = "trixie.qcow2"
K1, K2 = "k1", "k2"
ALL = (K1, K2)
AIR = "kvmwifi"
LAYOUT = {K1: (-150, 0), AIR: (0, 0), K2: (150, 0)}
ETHS = {(K1, "eth0"): 270, (K2, "eth0"): 90}
ADDR = {K1: "10.44.0.1", K2: "10.44.0.2"}

def create(node):
    """A kvm with one radio, its wlan0; eth0 stays cabled to nothing."""
    r = cli("add", "kvm", node, "ram=1000", "cpu=2", "eth=1", KVM_ROOTFS,
            "wlan=1")
    if r.returncode:
        raise RuntimeError("%s: %s\n  fetch the image into "
                           "/var/lib/cloonix/bulk from clownix.net, or build "
                           "it: cd vm_create/cnt_kvm && ./kvm_trixie.sh"
                           % (node, (r.stdout + r.stderr).strip()))

def wait_radio(node, secs=60):
    """The agent makes wlan0 a few seconds after the guest answers."""
    end = time.time() + secs
    while time.time() < end:
        if "radio" in ssh(node, "test -d /sys/class/net/wlan0 && echo radio",
                          check=False):
            return
        time.sleep(2)
    sys.exit("%s has no wlan0 after %d s: its agent did not make the "
             "radio, the reason is in the syslog of the host (kvmwifi)"
             % (node, secs))

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

def main():
    check_net()
    step("erasing the net, then placing the items")
    erase_net()
    place()
    step("two kvm, one radio each, both on the air kvmwifi")
    in_parallel(create, list(ALL), "add kvm")
    for n in ALL:
        wait_answer(n, 180)
    for n in ALL:
        wait_radio(n)
    step("k1 creates the ad hoc cell, k2 hears it and joins, then k1 pings k2")
    adhoc(K1)
    cell = wait_cell(K1)
    if not cell:
        sys.exit("k1 made no ad hoc cell:\n%s"
                 % ssh(K1, "iw dev wlan0 link 2>&1", check=False))
    adhoc(K2)
    if wait_cell(K2, cell) != cell:
        sys.exit("k2 is in the cell %s, not in %s of k1: the beacons of k1 "
                 "do not reach k2 over kvmwifi" % (bssid(K2) or "none", cell))
    ok, out = False, ""
    end = time.time() + 60
    while time.time() < end and not ok:
        ok, out = ping(K1, ADDR[K2])
        if not ok:
            time.sleep(3)
    if not ok:
        sys.exit("no ping over the air:\n%s" % out)
    step("the same ping, caught on the air kvmwifi")
    frames = air(AIR, 6, lambda: ping(K1, ADDR[K2]))
    nb = count_icmp(frames)
    story()
    story("  k1 pings k2 over the air kvmwifi: OK")
    story("  %d ICMP frames caught on the air, %d frames in all"
          % (nb, len(frames)))
    story()
    hold()
    if not nb:
        sys.exit("the ping went, but the capture of kvmwifi shows no ICMP")
    story("""
Two kvm radios, one air, a ping. The network is left up. The capture of
the air:
    cloonix cli %s sniff kvmwifi""" % NET)

main()
