#!/usr/bin/env python3
"""cntkvm_monitor - what a listener on the air sees: open, then WPA2.

    ./cntkvm_monitor.py [net]       the net defaults to nemo

Two containers run a network, a kvm listens with its radio in monitor mode::

      ap (cnt)  (((  cntwifi  )))  sta (cnt)
      10.47.0.1        |          10.47.0.10
                  spy (kvm), mon0

The capture of cloonix, sniff cntwifi, is the view of the medium: every
frame. spy is the view of an eavesdropper: a radio of its own, in monitor
mode, tcpdump inside its guest. The network is open first, then WPA2. On
the open network spy reads the ping itself; on WPA2 it still sees the
beacons, the 4-way handshake, the addresses and the length and timing of
every data frame, and not one byte of IP.

Image: trixie, trixie.qcow2
Size:  2 cnt, 1 kvm, 1 air

THE PACKET TO CATCH
-------------------
  From inside spy, the view of the listener:

      cloonix cli nemo ssh spy "tcpdump -n -i mon0 -c 20"

  and the view of the medium, for comparison:

      cloonix cli nemo cnf cntwifi sniff=on
      cloonix cli nemo sniff cntwifi
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
    address, create_radio_cnt, create_radio_kvm, hostapd, hwsim_on_host,
    ping, station, wait_radios, wifi_state)

HISTORY = """\
A radio hears every frame on its channel: monitor mode only stops it from
throwing away those not for it. WEP, 1997, was broken by Fluhrer, Mantin
and Shamir in 2001 from such listening; WPA2, 802.11i of 2004, keeps the
payload secret with CCMP, but no encryption hides who talks, when and how
much. tcpdump was written at Lawrence Berkeley Laboratory in 1988 by Van
Jacobson, Craig Leres and Steven McCanne."""

ROOTFS_CHOICES = ("trixie",)
KVM_ROOTFS = "trixie.qcow2"
AP, STA, SPY = "ap", "sta", "spy"
ALL = (AP, STA, SPY)
AIR = "cntwifi"
WIRING = {AP: [(0, None, None)], STA: [(0, None, None)],
          SPY: [(0, None, None)]}
RADIOS = {AP: 1, STA: 1, SPY: 1}
LAYOUT = {AP: (-150, 0), AIR: (0, 0), STA: (150, 0), SPY: (0, 120)}
ETHS = {}
CHANNEL, FREQ = 6, 2437

OPEN_CONF = """interface=wlan0
driver=nl80211
ssid=cloonix-cafe
hw_mode=g
channel=%d
""" % CHANNEL

WPA_CONF = OPEN_CONF + """wpa=2
wpa_passphrase=cloonixdemo
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
"""

OPEN_STA = """network={
    ssid="cloonix-cafe"
    key_mgmt=NONE
}
"""

WPA_STA = """network={
    ssid="cloonix-cafe"
    psk="cloonixdemo"
    key_mgmt=WPA-PSK
}
"""

def monitor():
    """mon0 beside wlan0 on the phy of spy, on the channel of the network."""
    phy = ssh(SPY, "cat /sys/class/net/wlan0/phy80211/name").strip()
    ssh(SPY, "iw phy %s interface add mon0 type monitor" % phy)
    ssh(SPY, "ip link set dev mon0 up")
    ssh(SPY, "iw dev mon0 set freq %d" % FREQ)

def listen(secs, during):
    """What tcpdump on mon0 decodes while during() runs, one line a frame."""
    ssh(SPY, "rm -f /tmp/mon.pcap; timeout %d tcpdump -n -i mon0 "
             "-w /tmp/mon.pcap >/dev/null 2>&1 &" % (secs + 4))
    time.sleep(2)
    during()
    time.sleep(secs)
    return ssh(SPY, "tcpdump -n -r /tmp/mon.pcap 2>/dev/null",
               check=False).splitlines()

def seen(lines):
    return {"frames": len(lines),
            "beacons": sum(1 for ln in lines if "Beacon" in ln),
            "icmp": sum(1 for ln in lines if "ICMP echo" in ln),
            "eapol": sum(1 for ln in lines if "EAPOL" in ln)}

def network(conf, sta_conf):
    ssh(AP, "pkill hostapd; true", check=False)
    ssh(STA, "pkill wpa_supplicant; true", check=False)
    time.sleep(1)
    hostapd(AP, conf)
    station(STA, sta_conf)

def main():
    hwsim_on_host()
    check_net()
    rootfs = choose_rootfs()
    story(HISTORY)
    story()
    step("erasing the net, then placing the items")
    erase_net()
    place()
    step("an access point and a station in containers, a kvm listening")
    in_parallel(lambda n: create_radio_cnt(n, rootfs), [AP, STA], "add cnt")
    create_radio_kvm(SPY, KVM_ROOTFS)
    for n in ALL:
        wait_answer(n, 180)
        wait_radios(n)
    tools_present(AP, "iw", "hostapd")
    step("the radio of spy put on cntwifi, then in monitor mode")
    r = cli("plug", SPY, "wlan0", AIR)
    if r.returncode:
        sys.exit("spy would not go on %s: %s"
                 % (AIR, (r.stdout + r.stderr).strip()))
    monitor()
    step("act 1: an open network, spy listens to a ping")
    network(OPEN_CONF, OPEN_STA)
    address(AP, "wlan0", "10.47.0.1/24")
    address(STA, "wlan0", "10.47.0.10/24")
    s1 = seen(listen(6, lambda: ping(STA, "10.47.0.1")))
    story()
    story("  open: %d frames heard by spy, %d beacons, %d ICMP echo read"
          % (s1["frames"], s1["beacons"], s1["icmp"]))
    story()
    hold()
    if not s1["beacons"]:
        wifi_state((AP, STA))
        sys.exit("spy hears nothing: the monitor radio of the guest gets "
                 "no frame")
    if not s1["icmp"]:
        sys.exit("spy hears the air but not the ping of the open network")
    step("act 2: the same network with WPA2, spy listens again")
    network(WPA_CONF, WPA_STA)
    s2 = seen(listen(6, lambda: ping(STA, "10.47.0.1")))
    story()
    story("  WPA2: %d frames heard, %d beacons, %d EAPOL, %d ICMP echo read"
          % (s2["frames"], s2["beacons"], s2["eapol"], s2["icmp"]))
    story()
    hold()
    if s2["icmp"]:
        sys.exit("spy read the ping through WPA2")
    story("""
The same ping, twice: in clear to anyone in range on the open network,
unreadable under WPA2. What stays visible is the traffic itself: who
talks to whom, when, how much. The 4-way handshake spy recorded is also
what an offline guess of a weak passphrase starts from: WPA2 is as strong
as its passphrase.""")
    story("the network is left up, WPA2. Listen yourself:")
    story("    cloonix cli %s ssh spy \"tcpdump -n -i mon0 -c 20\"" % NET)

main()
