#!/usr/bin/env python3
"""cntkvm_roaming - one network, a cnt access point and a kvm access point.

    ./cntkvm_roaming.py [net]       the net defaults to nemo

One SSID, one passphrase, an access point per cell, not of the same kind::

      ap1 (cnt)  (((  cell1  )))            (((  cell2  )))  ap2 (kvm)
            \\                    sta (kvm)                  /
             ===================== lan1 ===================
                                   srv (cnt) 10.46.0.1

Both access points bridge their cell to lan1. sta, a kvm, pings srv without
a stop while it is moved from cell1 to cell2: it loses ap1, finds ap2 on
the same SSID and key, keeps its address, and the ping goes on after a gap
the demo measures. The station neither knows nor cares what runs the
access point: roaming is an event of layer 2, IP does not see it.

Image: trixie, trixie.qcow2
Size:  2 cnt, 2 kvm, 2 airs, 1 lan

THE PACKET TO CATCH
-------------------
  sta arriving in cell2, Authentication then Association to ap2:

      cloonix cli nemo cnf cell2 sniff=on
      cloonix cli nemo sniff cell2

  the move itself:

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
    NET, check_net, choose_rootfs, cli, create, erase_net, hold, in_parallel,
    place, ssh, step, story, tools_present, wait_answer)
from wifilib import (
    address, bssid, create_radio_cnt, create_radio_kvm, hostapd,
    hwsim_on_host, joins, ping, station, wait_joined, wait_radios,
    wifi_state)

HISTORY = """\
Roaming came with the cells of 802.11 in 1997: a station keeps its
network by changing access point, the ESS of the standard. 802.11r of 2008
made the change fast for voice. hostapd and wpa_supplicant, Jouni Malinen
since 2002, do it here the plain way: lose one access point, find the next
with the same SSID and key."""

ROOTFS_CHOICES = ("trixie",)
KVM_ROOTFS = "trixie.qcow2"
AP1, AP2, STA, SRV = "ap1", "ap2", "sta", "srv"
ALL = (AP1, AP2, STA, SRV)
WIRING = {AP1: [(0, "lan1", None)], AP2: [(0, "lan1", None)],
          STA: [(0, None, None)], SRV: [(0, "lan1", "10.46.0.1/24")]}
RADIOS = {AP1: 1, AP2: 1, STA: 1}
LAYOUT = {AP1: (-150, -100), "cell1": (-270, -100), AP2: (150, -100),
          "cell2": (270, -100), STA: (0, -200), "lan1": (0, 0),
          SRV: (0, 100)}
ETHS = {(AP1, "eth0"): 135, (AP2, "eth0"): 225, (SRV, "eth0"): 0}

AP_CONF = """interface=wlan0
bridge=br0
driver=nl80211
ssid=cloonix-campus
hw_mode=g
channel=1
wpa=2
wpa_passphrase=cloonixdemo
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
"""

STA_CONF = """network={
    ssid="cloonix-campus"
    psk="cloonixdemo"
    key_mgmt=WPA-PSK
}
"""

PING_S = 40

def mac(node):
    return ssh(node, "cat /sys/class/net/wlan0/address").strip()

def main():
    hwsim_on_host()
    check_net()
    rootfs = choose_rootfs()
    story(HISTORY)
    story()
    step("erasing the net, then placing the items")
    erase_net()
    place()
    step("a container access point, a kvm access point, a kvm station")
    in_parallel(lambda n: create_radio_kvm(n, KVM_ROOTFS), [AP2, STA],
                "add kvm")
    create_radio_cnt(AP1, rootfs)
    create(SRV, rootfs)
    for n in (AP1, AP2, SRV):
        cli("plug", n, "eth0", "lan1")
    for n in ALL:
        wait_answer(n, 180)
        wait_radios(n)
    tools_present(AP1, "iw", "hostapd")
    step("each access point on a cell of its own, sta on cell1")
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
    address(SRV, "eth0", "10.46.0.1/24")
    station(STA, STA_CONF)
    address(STA, "wlan0", "10.46.0.10/24")
    m1, m2 = mac(AP1), mac(AP2)
    ok, _ = ping(STA, "10.46.0.1")
    story()
    story("  sta joined %s, the container ap1: %s, ping of srv %s"
          % (bssid(STA), "yes" if bssid(STA) == m1 else "NO",
             "OK" if ok else "FAILS"))
    story()
    hold()
    if bssid(STA) != m1 or not ok:
        wifi_state((AP1, STA))
        sys.exit("sta is not on ap1 in cell1, or srv is not reached")
    step("a ping of %d s, five a second; sta moved to cell2 meanwhile"
         % PING_S)
    ssh(STA, "ping -i 0.2 -w %d 10.46.0.1 >/tmp/roam.log 2>&1 &" % PING_S)
    time.sleep(5)
    before = joins(STA)
    cli("plug", STA, "wlan0", "cell2")
    end = time.time() + 30
    while time.time() < end and bssid(STA) != m2:
        time.sleep(1)
    wait_joined(STA, before, 30)
    time.sleep(PING_S)
    out = ssh(STA, "tail -n 3 /tmp/roam.log", check=False)
    sent = got = 0
    for ln in out.splitlines():
        if "packets transmitted" in ln:
            f = ln.replace(",", "").split()
            sent, got = int(f[0]), int(f[3])
    story()
    story("  sta joined %s, the kvm ap2: %s, same address 10.46.0.10"
          % (bssid(STA), "yes" if bssid(STA) == m2 else "NO"))
    story("  %d pings sent, %d answered: a gap of about %.1f s"
          % (sent, got, (sent - got) * 0.2))
    story()
    hold()
    if bssid(STA) != m2 or not sent or got < sent // 2:
        wifi_state((AP2, STA))
        sys.exit("sta did not roam to ap2, or lost srv:\n%s" % out)
    story("""
The ping went on across the change: the station kept its address, both
cells being one lan behind their bridges, and whether an access point is a
container or a kvm is nothing a station can see.""")
    story("the network is left up. Send sta back to cell1 and watch:")
    story("    cloonix cli %s plug sta wlan0 cell1" % NET)

main()
