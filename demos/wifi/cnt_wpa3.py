#!/usr/bin/env python3
"""cnt_wpa3 - WPA3-Personal: SAE, then only encrypted data on the air.

    ./cnt_wpa3.py [net]             the net defaults to nemo

The network of cnt_open.py, with a password::

      ap  ((( cntwifi )))  sta
  192.168.8.1         192.168.8.10

WPA2 (2004) derives the key of a station from the password in a way that a
captured handshake lets anyone test offline. WPA3 (Wi-Fi Alliance, 2018)
replaces that with SAE, Dan Harkins' Dragonfly exchange: a password guess
costs one exchange with the access point. SAE is two authentication
frames each way, commit and confirm, before the 4-way EAPOL handshake of
WPA2; management frames are protected (802.11w).

Image: trixie
Size:  2 containers, 1 air

THE PACKET TO CATCH
-------------------
  The SAE commit and confirm, algorithm 3 of the authentication frames:

      cloonix cli nemo cnf cntwifi sniff=on
      cloonix cli nemo sniff cntwifi

  In webshark, double-click the air cntwifi: Authentication, then EAPOL
  Message 1 of 4 to Message 4 of 4, then QoS Data (encrypted).
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
    address, air, count_eapol, count_icmp, count_protected, count_sae,
    create_radio_cnt, hostapd, hwsim_on_host, ping, station,
    wifi_state)

ROOTFS_CHOICES = ("trixie",)
AP, STA = "ap", "sta"
ALL = (AP, STA)
WIRING = {AP: [(0, None, None)], STA: [(0, None, None)]}
RADIOS = {AP: 1, STA: 1}
LAYOUT = {AP: (-150, 0), "cntwifi": (0, 0), STA: (150, 0)}
ETHS = {}

AP_CONF = """interface=wlan0
driver=nl80211
ssid=cloonix-wpa3
hw_mode=g
channel=6
wpa=2
wpa_key_mgmt=SAE
rsn_pairwise=CCMP
ieee80211w=2
sae_password=cloonixdemo
sae_pwe=2
"""

STA_CONF = """sae_pwe=2
network={
    ssid="cloonix-wpa3"
    key_mgmt=SAE
    sae_password="cloonixdemo"
    ieee80211w=2
}
"""

def main():
    hwsim_on_host()
    check_net()
    rootfs = choose_rootfs()
    story("WPA3, Wi-Fi Alliance 2018: SAE, the Dragonfly exchange of Dan")
    story("Harkins, makes a password guess cost one exchange with the AP.")
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
    step("hostapd with SAE on ap")
    hostapd(AP, AP_CONF)
    step("the air captured while sta joins: SAE, then the 4-way handshake")
    frames = air("cntwifi", 3, lambda: station(STA, STA_CONF))
    sae, eapol = count_sae(frames), count_eapol(frames)
    story()
    story("  %d SAE authentication frames, %d EAPOL frames" % (sae, eapol))
    hold()
    if sae < 4 or eapol < 4:
        sys.exit("the join did not show SAE and the 4-way handshake")
    address(AP, "wlan0", "192.168.8.1/24")
    address(STA, "wlan0", "192.168.8.10/24")
    step("the air captured while sta pings: nothing readable")
    result = {}
    frames = air("cntwifi", 3,
                 lambda: result.update(ping=ping(STA, "192.168.8.1")))
    ok, _ = result["ping"]
    story()
    story("  %d encrypted data frames, %d ICMP readable"
          % (count_protected(frames), count_icmp(frames)))
    hold()
    if not ok:
        wifi_state(ALL)
        sys.exit("the ping failed, its output is above")
    if count_icmp(frames) or count_protected(frames) < 8:
        sys.exit("the ping was readable on the air, or not on it at all")
    story("""
The same ping as in cnt_open.py, and the air shows only its length: the
payload is CCMP ciphertext, the key never crossed the air.""")
    story("the network is left up. Erase it, the server stays:")
    story("    cloonix cli %s del all" % NET)

main()
