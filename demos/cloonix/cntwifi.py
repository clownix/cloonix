#!/usr/bin/env python3
"""cntwifi - two containers with a radio each, and a ping over the air.

    ./cntwifi.py [net]           the net defaults to nemo

The first wifi network, the smallest one::

      w1  (((  cntwifi  )))  w2
  10.43.0.1              10.43.0.2

wlan=1 gives a container a radio, wlan0. Every radio starts on the air
"cntwifi", the amber lan of the canvas. The two radios join one ad hoc
network, no access point and no key, and ping each other. Package 10 of the
course, wifi/, has the access points, WPA2, WPA3, a bridge, roaming and a
mesh.

Image: trixie
Size:  2 containers, 1 air

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
    NET, check_net, choose_rootfs, erase_net, hold, in_parallel, place, ssh,
    step, story, tools_present, wait_answer)
from wifilib import create_radio_cnt, hwsim_on_host, ping

ROOTFS_CHOICES = ("trixie",)
W1, W2 = "w1", "w2"
ALL = (W1, W2)
WIRING = {W1: [(0, None, None)], W2: [(0, None, None)]}
RADIOS = {W1: 1, W2: 1}
LAYOUT = {W1: (-150, 0), "cntwifi": (0, 0), W2: (150, 0)}
ETHS = {}
ADDR = {W1: "10.43.0.1", W2: "10.43.0.2"}

def adhoc(node):
    ssh(node, "iw dev wlan0 set type ibss")
    ssh(node, "ip link set dev wlan0 up")
    ssh(node, "iw dev wlan0 ibss join demoadhoc 2412")
    ssh(node, "ip addr add %s/24 dev wlan0" % ADDR[node])

def main():
    hwsim_on_host()
    check_net()
    rootfs = choose_rootfs()
    step("erasing the net, then placing the items")
    erase_net()
    place()
    step("two containers, one radio each, both on the air cntwifi")
    in_parallel(lambda n: create_radio_cnt(n, rootfs), list(ALL), "add cnt")
    for n in ALL:
        wait_answer(n)
    tools_present(W1, "iw")
    step("both radios join one ad hoc network, then w1 pings w2")
    for n in ALL:
        adhoc(n)
    ok, out = False, ""
    end = time.time() + 30
    while time.time() < end and not ok:
        ok, out = ping(W1, ADDR[W2])
    story()
    story("  w1 pings w2 over the air: %s" % ("OK" if ok else "FAILS"))
    hold()
    if not ok:
        sys.exit("no ping over the air:\n%s" % out)
    story("""
Two radios, one air, a ping. The network is left up. The capture of the
air, and the other wifi demos:
    cloonix cli %s cnf cntwifi sniff=on
    cloonix cli %s sniff cntwifi
    cloonix cli %s demo wifi/cnt_wpa2.py""" % (NET, NET, NET))

main()
