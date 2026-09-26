#!/usr/bin/env python3
"""kvm_bridge - an access point bridging the air to a wired lan.

    ./kvm_bridge.py [net]           the net defaults to nemo

A station on the air reaches a server on a cable through the AP::

      sta  ((( kvmwifi )))  ap ==br0== lan1 -- srv
  10.40.0.10                                 10.40.0.1

A home box does exactly this: its radio and its ethernet ports are one
bridge, one subnet. The Linux bridge dates from 1999 (Lennert Buytenhek);
hostapd puts wlan0 in it itself with its option bridge=. The frames of
the station cross the air as 802.11, then the cable as ethernet.

The demo wifi/cnt_bridge.py played by kvm: the cloonix agent of each guest
makes its radios in the guest kernel, and they start on the air kvmwifi.

Image: trixie.qcow2
Size:  3 kvm, 1 air, 1 lan

THE PACKET TO CATCH
-------------------
  The ping of sta on the cable of srv, where no radio is:

      cloonix cli nemo sniff srv eth0 --dump icmp

  and the same ping on the air:

      cloonix cli nemo cnf kvmwifi sniff=on
      cloonix cli nemo sniff kvmwifi
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, the wifi part in
# demos/lib/wifilib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    NET, check_net, cli, count_on, erase_net, hold,
    in_parallel, place, ssh, step, story, tools_present, wait_answer)
from wifilib import (
    address, create_radio_kvm, hostapd, wait_radios, ping, station)

KVM_ROOTFS = "trixie.qcow2"
AP, STA, SRV = "ap", "sta", "srv"
ALL = (AP, STA, SRV)
WIRING = {AP: [(0, "lan1", None)], STA: [(0, None, None)],
          SRV: [(0, "lan1", "10.40.0.1/24")]}
RADIOS = {AP: 1, STA: 1}
LAYOUT = {STA: (-240, 0), "kvmwifi": (-120, 0), AP: (0, 0), "lan1": (120, 0),
          SRV: (240, 0)}
ETHS = {(AP, "eth0"): 90, (SRV, "eth0"): 270}

AP_CONF = """interface=wlan0
bridge=br0
driver=nl80211
ssid=cloonix-home
hw_mode=g
channel=11
wpa=2
wpa_passphrase=cloonixdemo
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
"""

STA_CONF = """network={
    ssid="cloonix-home"
    psk="cloonixdemo"
    key_mgmt=WPA-PSK
}
"""

def main():
    check_net()
    rootfs = KVM_ROOTFS
    story("A home box: its radio and its ethernet are one Linux bridge")
    story("(Lennert Buytenhek, 1999), hostapd putting wlan0 in it.")
    story()
    step("erasing the net, then placing the items")
    erase_net()
    place()
    step("an access point with a radio and an eth, a station, a server")
    in_parallel(lambda n: create_radio_kvm(n, rootfs), [AP, STA], "add kvm")
    create_radio_kvm(SRV, rootfs)
    cli("plug", AP, "eth0", "lan1")
    cli("plug", SRV, "eth0", "lan1")
    for n in ALL:
        wait_answer(n, 180)
        wait_radios(n)
    tools_present(AP, "iw", "hostapd")
    tools_present(STA, "iw", "wpa_supplicant")
    step("br0 on ap holds eth0; hostapd adds wlan0 to it")
    ssh(AP, "ip link add br0 type bridge")
    ssh(AP, "ip link set dev eth0 master br0")
    ssh(AP, "ip link set dev eth0 up")
    ssh(AP, "ip link set dev br0 up")
    hostapd(AP, AP_CONF)
    address(SRV, "eth0", "10.40.0.1/24")
    station(STA, STA_CONF)
    address(STA, "wlan0", "10.40.0.10/24")
    step("sta pings srv; the ping counted on the cable of srv")
    got = count_on(SRV, 0, "icmp", lambda: ping(STA, "10.40.0.1"))
    story()
    if got is None:
        sys.exit("the cable of srv could not be sniffed")
    ok, out = got["out"]
    story("  %d ICMP packets on the cable of srv" % got["count"])
    hold()
    if not ok or got["count"] < 8:
        sys.exit("the ping from the air to the cable failed:\n%s" % out)
    story("""
One subnet across the air and the cable: the access point is a bridge, the
station and the server see each other as on one lan.""")
    story("the network is left up. Erase it, the server stays:")
    story("    cloonix cli %s del all" % NET)

main()
