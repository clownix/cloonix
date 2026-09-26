#!/usr/bin/env python3
"""cntkvm_home - the home box: a kvm access point, a cnt and a kvm station.

    ./cntkvm_home.py [net]          the net defaults to nemo

A kvm plays the box of a home, a container and a kvm are the stations::

      sta1 (cnt)
           (((  cntwifi  )))  ap (kvm) ==br0== lan1 -- srv (cnt, kea)
      sta2 (kvm)                                       10.43.0.1

The radio of ap and its eth0 are one Linux bridge, hostapd holds WPA2 on
the air, and kea on the cable is the DHCP server. Neither station has an
address at the start: sta1 asks with dhcpcd, sta2 with dhclient, and the
answer comes from the cable through the bridge, over the air. The radio of
the container is made by suid-power, the radio of the kvm by its cloonix
agent: the kvm radios are put on cntwifi, the air of the container.

Image: dhcp_trixie, trixie.qcow2
Size:  2 cnt, 2 kvm, 1 air, 1 lan

THE PACKET TO CATCH
-------------------
  The DHCP of both stations, in clear on the cable of srv:

      cloonix cli nemo sniff srv eth0 --dump udp port 67 or udp port 68

  and on the air the same exchange encrypted, only its length readable:

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
    NET, check_net, choose_rootfs, cli, count_on, create, erase_net, hold,
    in_parallel, place, put, ssh, step, story, tools_present, wait_answer)
from wifilib import (
    address, create_radio_cnt, create_radio_kvm, hostapd, hwsim_on_host,
    ping, station, wait_radios, wifi_state)

HISTORY = """\
The home box joins three inventions. The Linux bridge, 1999, Lennert
Buytenhek, makes the radio and the ethernet one lan. hostapd, Jouni Malinen
since 2002, is the access point, WPA2 since 802.11i in 2004. DHCP, RFC 2131
of 1997, Ralph Droms, gives the address: here kea, the server ISC wrote to
follow its dhcpd of 1997."""

ROOTFS_CHOICES = ("dhcp_trixie",)
KVM_ROOTFS = "trixie.qcow2"
AP, STA1, STA2, SRV = "ap", "sta1", "sta2", "srv"
ALL = (AP, STA1, STA2, SRV)
AIR = "cntwifi"
WIRING = {AP: [(0, "lan1", None)], STA1: [(0, None, None)],
          STA2: [(0, None, None)], SRV: [(0, "lan1", "10.43.0.1/24")]}
RADIOS = {AP: 1, STA1: 1, STA2: 1}
LAYOUT = {STA1: (-240, -80), STA2: (-240, 80), AIR: (-120, 0), AP: (0, 0),
          "lan1": (120, 0), SRV: (240, 0)}
ETHS = {(AP, "eth0"): 90, (SRV, "eth0"): 270}

AP_CONF = """interface=wlan0
bridge=br0
driver=nl80211
ssid=cloonix-home
hw_mode=g
channel=6
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

KEA_CONF = """{
  "Dhcp4": {
    "interfaces-config": { "interfaces": [ "eth0" ] },
    "lease-database": { "type": "memfile", "persist": true,
                        "name": "/var/lib/kea/leases4.csv" },
    "valid-lifetime": 3600,
    "subnet4": [ { "id": 1, "subnet": "10.43.0.0/24",
                   "pools": [ { "pool": "10.43.0.100 - 10.43.0.199" } ] } ]
  }
}
"""

def kea_start():
    ssh(SRV, "mkdir -p /etc/kea /var/lib/kea /var/log/kea /run/kea")
    put(SRV, "/etc/kea/kea-dhcp4.conf", KEA_CONF)
    ssh(SRV, "kea-dhcp4 -c /etc/kea/kea-dhcp4.conf "
             "</dev/null >/var/log/kea/out 2>&1 &")
    for _ in range(20):
        if ssh(SRV, "pgrep -x kea-dhcp4 >/dev/null && echo up",
               check=False).strip():
            return
        time.sleep(0.5)
    sys.exit("kea did not start:\n%s"
             % ssh(SRV, "cat /var/log/kea/out", check=False))

def lease(node):
    """The address the station got on wlan0, or an empty string."""
    out = ssh(node, "ip -4 -o addr show dev wlan0", check=False).split()
    return out[out.index("inet") + 1].split("/")[0] if "inet" in out else ""

def ask_both():
    ssh(STA1, "dhcpcd -4 -1 -t 20 wlan0 </dev/null >/tmp/dhcp.log 2>&1",
        check=False)
    ssh(STA2, "timeout 30 dhclient -1 wlan0 >/tmp/dhcp.log 2>&1",
        check=False)
    return lease(STA1), lease(STA2)

def main():
    hwsim_on_host()
    check_net()
    rootfs = choose_rootfs()
    story(HISTORY)
    story()
    step("erasing the net, then placing the items")
    erase_net()
    place()
    step("a kvm access point, a container station, a kvm station, a server")
    in_parallel(lambda n: create_radio_kvm(n, KVM_ROOTFS), [AP, STA2],
                "add kvm")
    create_radio_cnt(STA1, rootfs)
    create(SRV, rootfs)
    cli("plug", AP, "eth0", "lan1")
    cli("plug", SRV, "eth0", "lan1")
    for n in ALL:
        wait_answer(n, 180)
        wait_radios(n)
    tools_present(SRV, "kea-dhcp4")
    tools_present(STA1, "dhcpcd", "wpa_supplicant")
    step("the kvm radios put on cntwifi, the air of the container")
    for n in (AP, STA2):
        r = cli("plug", n, "wlan0", AIR)
        if r.returncode:
            sys.exit("%s would not go on %s: %s"
                     % (n, AIR, (r.stdout + r.stderr).strip()))
    step("ap bridges its radio to the cable, kea serves the cable")
    ssh(AP, "ip link add br0 type bridge")
    ssh(AP, "ip link set dev eth0 master br0")
    ssh(AP, "ip link set dev eth0 up")
    ssh(AP, "ip link set dev br0 up")
    hostapd(AP, AP_CONF)
    address(SRV, "eth0", "10.43.0.1/24")
    kea_start()
    step("both stations join the access point, with no address yet")
    for n in (STA1, STA2):
        station(n, STA_CONF)
    step("both ask an address, the DHCP counted on the cable of srv")
    got = count_on(SRV, 0, "udp port 67 or udp port 68", ask_both)
    if got is None:
        sys.exit("the cable of srv could not be sniffed")
    a1, a2 = got["out"]
    story()
    story("  sta1, a container, by dhcpcd: %s" % (a1 or "NO ADDRESS"))
    story("  sta2, a kvm, by dhclient:     %s" % (a2 or "NO ADDRESS"))
    story("  %d DHCP packets on the cable of srv" % got["count"])
    story()
    hold()
    if not a1 or not a2:
        wifi_state((AP, STA1, STA2))
        sys.exit("a station got no address over the air")
    step("sta1 pings sta2 across the air, sta2 pings srv across the bridge")
    ok1, _ = ping(STA1, a2)
    ok2, _ = ping(STA2, "10.43.0.1")
    story()
    story("  sta1 (cnt) -> sta2 (kvm), one air:       %s"
          % ("OK" if ok1 else "FAILS"))
    story("  sta2 (kvm) -> srv, air then cable:       %s"
          % ("OK" if ok2 else "FAILS"))
    story()
    hold()
    if not (ok1 and ok2):
        wifi_state((AP, STA1, STA2))
        sys.exit("the pings of the home network failed")
    story("""
The broadcast of DHCP crossed the air encrypted, the bridge, then the cable
in clear: WPA2 protects the air, not the lan behind the box. A container and
a kvm are two stations like any others, the box does not know the
difference.""")
    story("the network is left up. The lease file of kea:")
    story("    cloonix cli %s ssh srv \"cat /var/lib/kea/leases4.csv\"" % NET)

main()
