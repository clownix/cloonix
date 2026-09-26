#!/usr/bin/env python3
"""cnt_open - an open wifi, and every frame readable on the air.

    ./cnt_open.py [net]             the net defaults to nemo

One access point, one station, no encryption::

      ap  ((( cntwifi )))  sta
  192.168.7.1         192.168.7.10

In 1997 the IEEE published 802.11: a lan with no cable, the frames carried
by radio between stations and an access point. The first networks were
open, and many still are, in cafes and trains: anyone on the channel reads
every frame. The access point here is hostapd, the station wpa_supplicant,
both written by Jouni Malinen since 2002, and the radios are
mac80211_hwsim, the kernel's simulated card.

Image: trixie
Size:  2 containers, 1 air

THE PACKET TO CATCH
-------------------
  The ping itself, in clear inside 802.11 data frames:

      cloonix cli nemo cnf cntwifi sniff=on
      cloonix cli nemo sniff cntwifi

  In webshark, double-click the air cntwifi: the ICMP echo is decoded under
  the 802.11 header and the LLC/SNAP header, as it would be on a lan.
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
    NET, check_net, choose_rootfs, erase_net, hold, in_parallel, place, step,
    story, tools_present, wait_answer)
from wifilib import (
    address, air, count_icmp, create_radio_cnt, hostapd, hwsim_on_host,
    ping, station, wifi_state)

ROOTFS_CHOICES = ("trixie",)
AP, STA = "ap", "sta"
ALL = (AP, STA)
WIRING = {AP: [(0, None, None)], STA: [(0, None, None)]}
RADIOS = {AP: 1, STA: 1}
LAYOUT = {AP: (-150, 0), "cntwifi": (0, 0), STA: (150, 0)}
ETHS = {}

AP_CONF = """interface=wlan0
driver=nl80211
ssid=cloonix-open
hw_mode=g
channel=1
auth_algs=1
"""

STA_CONF = """network={
    ssid="cloonix-open"
    key_mgmt=NONE
}
"""

def main():
    hwsim_on_host()
    check_net()
    rootfs = choose_rootfs()
    story("802.11, 1997: a lan with no cable. hostapd and wpa_supplicant,")
    story("Jouni Malinen, since 2002. An open network: no key at all.")
    story()
    step("erasing the net, then placing the items")
    erase_net()
    place()
    step("an access point and a station, one radio each")
    in_parallel(lambda n: create_radio_cnt(n, rootfs), list(ALL), "add cnt")
    for n in ALL:
        wait_answer(n)
    tools_present(AP, "iw", "hostapd")
    tools_present(STA, "iw", "wpa_supplicant")
    step("hostapd makes wlan0 of ap an access point, sta joins it")
    hostapd(AP, AP_CONF)
    station(STA, STA_CONF)
    address(AP, "wlan0", "192.168.7.1/24")
    address(STA, "wlan0", "192.168.7.10/24")
    step("the air cntwifi captured while sta pings the access point")
    result = {}
    frames = air("cntwifi", 3,
                 lambda: result.update(ping=ping(STA, "192.168.7.1")))
    ok, out = result["ping"]
    icmp = count_icmp(frames)
    story()
    story("  %d frames on the air, %d of them ICMP readable in clear"
          % (len(frames), icmp))
    story(out.strip().splitlines()[-2] if out.strip() else "no ping output")
    hold()
    if not ok or icmp < 8:
        wifi_state(ALL)
        sys.exit("the ping or its capture failed: %d icmp frames" % icmp)
    story("""
Nothing hides the traffic of an open network: the capture of the air reads
the ping as a sniff of a lan would. The next demo, cnt_wpa3.py, is the same
network with a key.""")
    story("the network is left up. Erase it, the server stays:")
    story("    cloonix cli %s del all" % NET)

main()
