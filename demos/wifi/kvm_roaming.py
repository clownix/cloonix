#!/usr/bin/env python3
"""kvm_roaming - one network, two access points, a station moving between them.

    ./kvm_roaming.py [net]          the net defaults to nemo

Two access points with the same name and key, each on an air of its own,
both bridged to one lan where the server is::

     cell1 )))  ap1 ==\
                       lan1 -- srv 10.41.0.1
     cell2 )))  ap2 ==/
      sta 10.41.0.10  starts in cell1, is moved to cell2

An office covers its floors with many access points under one SSID: a
station keeps its address and goes to whichever it hears. Here the move is
cloonix putting its radio on the other air: ap1 goes silent for it, it
finds ap2, joins again with the same key, and reaches the server with the
same address.

The demo wifi/cnt_roaming.py played by kvm: the cloonix agent of each guest
makes its radios in the guest kernel, and they start on the air kvmwifi.

Image: trixie.qcow2
Size:  4 kvm, 2 airs, 1 lan

THE PACKET TO CATCH
-------------------
  The join of sta on the air cell2, after its move:

      cloonix cli nemo cnf cell2 sniff=on
      cloonix cli nemo sniff cell2
      cloonix cli nemo plug sta wlan0 cell2
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
    NET, check_net, cli, erase_net, hold, in_parallel,
    place, ssh, step, story, tools_present, wait_answer)
from wifilib import (
    address, bssid, create_radio_kvm, hostapd, joins, wait_radios, ping,
    station, wait_joined)

KVM_ROOTFS = "trixie.qcow2"
AP1, AP2, STA, SRV = "ap1", "ap2", "sta", "srv"
ALL = (AP1, AP2, STA, SRV)
WIRING = {AP1: [(0, "lan1", None)], AP2: [(0, "lan1", None)],
          STA: [(0, None, None)], SRV: [(0, "lan1", "10.41.0.1/24")]}
RADIOS = {AP1: 1, AP2: 1, STA: 1}
LAYOUT = {"cell1": (-120, -100), AP1: (0, -100), "cell2": (-120, 100),
          AP2: (0, 100), STA: (-240, 0), "lan1": (120, 0), SRV: (240, 0)}
ETHS = {(AP1, "eth0"): 135, (AP2, "eth0"): 45, (SRV, "eth0"): 270}

AP_CONF = """interface=wlan0
bridge=br0
driver=nl80211
ssid=cloonix-office
hw_mode=g
channel=1
wpa=2
wpa_passphrase=cloonixdemo
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
"""

STA_CONF = """network={
    ssid="cloonix-office"
    psk="cloonixdemo"
    key_mgmt=WPA-PSK
}
"""

def mac(node):
    return ssh(node, "cat /sys/class/net/wlan0/address").strip()

def main():
    check_net()
    rootfs = KVM_ROOTFS
    story("Many access points, one SSID: a station keeps its address and")
    story("joins whichever it hears.")
    story()
    step("erasing the net, then placing the items")
    erase_net()
    place()
    step("two access points, a station, a server")
    in_parallel(lambda n: create_radio_kvm(n, rootfs), [AP1, AP2, STA],
                "add kvm")
    create_radio_kvm(SRV, rootfs)
    for n in (AP1, AP2, SRV):
        cli("plug", n, "eth0", "lan1")
    for n in ALL:
        wait_answer(n, 180)
        wait_radios(n)
    tools_present(AP1, "iw", "hostapd")
    tools_present(STA, "iw", "wpa_supplicant")
    step("each access point on an air of its own, sta on cell1")
    for n, cell in ((AP1, "cell1"), (AP2, "cell2"), (STA, "cell1")):
        r = cli("plug", n, "wlan0", cell)
        if r.returncode:
            sys.exit("%s would not go on %s: %s"
                     % (n, cell, (r.stdout + r.stderr).strip()))
    for n in (AP1, AP2):
        ssh(n, "ip link add br0 type bridge")
        ssh(n, "ip link set dev eth0 master br0")
        ssh(n, "ip link set dev eth0 up")
        ssh(n, "ip link set dev br0 up")
        hostapd(n, AP_CONF)
    address(SRV, "eth0", "10.41.0.1/24")
    station(STA, STA_CONF)
    address(STA, "wlan0", "10.41.0.10/24")
    m1, m2 = mac(AP1), mac(AP2)
    ok, _ = ping(STA, "10.41.0.1")
    story()
    story("  sta joined %s (%s), ping of srv %s"
          % (bssid(STA), "ap1" if bssid(STA) == m1 else "not ap1",
             "OK" if ok else "FAILS"))
    hold()
    if bssid(STA) != m1 or not ok:
        sys.exit("sta is not on ap1 in cell1, or srv is not reached")
    step("sta moved to cell2: ap1 is gone for it, it finds ap2")
    before = joins(STA)
    cli("plug", STA, "wlan0", "cell2")
    end = time.time() + 60
    while time.time() < end and bssid(STA) != m2:
        time.sleep(2)
    wait_joined(STA, before, 30)
    ok, _ = ping(STA, "10.41.0.1")
    story()
    story("  sta joined %s (%s), same address, ping of srv %s"
          % (bssid(STA), "ap2" if bssid(STA) == m2 else "not ap2",
             "OK" if ok else "FAILS"))
    hold()
    if bssid(STA) != m2 or not ok:
        sys.exit("sta did not roam to ap2, or lost srv")
    story("""
The station changed access point and kept its address: both cells are one
lan behind their bridges.""")
    story("the network is left up. Erase it, the server stays:")
    story("    cloonix cli %s del all" % NET)

main()
