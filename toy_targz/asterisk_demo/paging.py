#!/usr/bin/env python3
r"""paging - Paging and Intercom.

    ./paging.py [net]     the net defaults to nemo

Paging as its page in the Asterisk documentation sets it up, Page() calling
a MulticastRTP channel: who dials the paging extension speaks to every
phone of the lan at once, in one stream sent to a multicast group,
239.1.1.1 port 5000, whatever the number of listeners. The listeners join
the group (ip address ... autojoin, the IGMP report of a desk phone).
https://docs.asterisk.org/

    caller (10.0.0.2) ---+---- lan1 ---- pbx (10.0.0.1)
    baresip, 6001        |               Page(MulticastRTP/basic/239.1.1.1:5000)
    l1 l2 l3 (10.0.0.11..13), listening to 239.1.1.1

  ACT 1  the three listeners join 239.1.1.1; the caller registers.
  ACT 2  the caller dials 999 and speaks (its 440 Hz tone): one multicast
         stream reaches the three listeners, counted on each of them.

Image: asterisk_trixie (asterisk, baresip)
Size:  5 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf l1 eth0 sniff=on
    cloonix cli nemo sniff l1 eth0 --dump "dst host 239.1.1.1"
  An RTP packet to 239.1.1.1: one packet of asterisk, copied by the switch
  of the lan to every port that joined the group.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py, asterisklib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import count_on, hold, step, story
from asterisklib import (
    asterisk_start, endpoint, hosts, ip_of, opening, phone_cmd, phone_log,
    phone_start, proof, registered, rx, SECRET, ssh, UDP_TRANSPORT, until,
    verdict)

HISTORY = """\
Steve Deering defined IP multicast in his Stanford thesis and in RFC 1112,
1989: one packet sent to a group, delivered to every member that joined it
with IGMP. Paging, the loudspeakers of the office, became a multicast
stream on IP phones. app_page has paged in Asterisk since its first
versions; it mixes its channels in a conference, ConfBridge today.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PBX, CALLER = "pbx", "caller"
LISTENERS = ("l1", "l2", "l3")
ALL = (PBX, CALLER) + LISTENERS
WIRING = {PBX: [(0, "lan1", "10.0.0.1/24")], CALLER: [(0, "lan1", "10.0.0.2/24")]}
WIRING.update({l: [(0, "lan1", "10.0.0.%d/24" % (11 + i))]
               for i, l in enumerate(LISTENERS)})
GATEWAY = {}
LAYOUT = {CALLER: (-220, -110), "l1": (-220, 0), "l2": (-110, 110),
          "l3": (0, 110), "lan1": (0, 0), PBX: (220, 0)}
ETHS = {(CALLER, "eth0"): 135, ("l1", "eth0"): 90, ("l2", "eth0"): 45,
        ("l3", "eth0"): 0, (PBX, "eth0"): 270}
CARD = r"""
    caller ----+---- pbx (asterisk, Page to 239.1.1.1)
    l1 l2 l3 --+
  One voice, one stream, three listeners.
"""

GROUP = "239.1.1.1"
PJSIP_CONF = UDP_TRANSPORT + endpoint("6001", "demo", SECRET)
EXTENSIONS_CONF = """\
[demo]
exten => 999,1,Page(MulticastRTP/basic/%s:5000,q)
""" % GROUP


def main():
    opening()
    hosts()
    step("act 1: three listeners join %s, the caller registers" % GROUP)
    asterisk_start(PBX, {"pjsip.conf": PJSIP_CONF, "confbridge.conf": "[general]\n",
                         "extensions.conf": EXTENSIONS_CONF},
                   modules=("app_page", "app_confbridge", "bridge_softmix",
                            "chan_rtp", "res_rtp_multicast"))
    ssh(PBX, "ip route add 224.0.0.0/4 dev eth0")
    for l in LISTENERS:
        ssh(l, "ip addr add %s/32 dev eth0 autojoin" % GROUP)
    phone_start(CALLER, "6001", ip_of(PBX))
    ok_reg = until(lambda: registered(PBX, "6001"), 20, 1)
    proof("l1, its groups:", ssh("l1", "ip maddr show dev eth0", check=False))
    hold()
    step("act 2: the caller dials 999, the page reaches every listener")
    phone_cmd(CALLER, "dial", "sip:999@%s" % ip_of(PBX))
    until(lambda: "established" in phone_log(CALLER), 15, 1)
    counts = {}
    for l in LISTENERS:
        got = count_on(l, 0, "udp and dst host %s" % GROUP,
                       lambda: until(lambda: False, 2, 1), settle=0.5)
        counts[l] = got["count"] if got else 0
    chans = rx(PBX, "core show channels")
    phone_cmd(CALLER, "hangup")
    ok_page = "MulticastRTP" in chans
    ok_all = all(n > 20 for n in counts.values())
    proof("pbx, the channels of the page; the listeners, packets in 2 s:",
          chans + "\n" + "\n".join("%s: %d" % (l, n) for l, n in counts.items()))
    verdict([("the caller is registered", ok_reg),
             ("Page() opens a MulticastRTP channel", ok_page),
             ("the stream reaches the three listeners", ok_all)])
    story("""
the network is left up. Things to try, in a terminal of pbx:
the SIP dialogs as ladders:
sngrep
the console of asterisk, its channels as they come:
asterisk -rvvv
the group on a listener, in a terminal of l1:
ip maddr show dev eth0
""")


main()
