#!/usr/bin/env python3
r"""iax2 - Inter-Asterisk eXchange protocol, version 2 (IAX2).

    ./iax2.py [net]     the net defaults to nemo

IAX2 as its pages in the Asterisk documentation configure it: two switches,
each the PBX of a site, joined by a trunk of iax.conf (type friend, a
secret), the phones of each site on SIP. A call from one site to the other
leaves SIP for IAX2 between the switches: signalling and voice in one UDP
port, 4569, which crosses any NAT and firewall alike.
https://docs.asterisk.org/

    phonea -- lana -- pbxa ===== wan ===== pbxb -- lanb -- phoneb
    2001      10.1.0.0/24   192.0.2.1   192.0.2.2   10.2.0.0/24  3001
    baresip                 asterisk    asterisk                 baresip

  ACT 1  both switches start, the trunk comes up (iax2 show peers), each
         phone registers with its own switch.
  ACT 2  phonea dials 3001: pbxa sends the call over IAX2, pbxb rings
         phoneb, which answers; on the wan, only UDP 4569, no RTP at all.

Image: asterisk_trixie (asterisk, baresip)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf pbxa eth1 sniff=on
    cloonix cli nemo sniff pbxa eth1 --dump "udp port 4569"
  A full frame NEW, then the mini frames of the voice: four bytes of IAX2
  header before each 20 ms of audio, where RTP and UDP would take twenty.
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
    phone_start, proof, registered, rx, SECRET, UDP_TRANSPORT, until,
    verdict)

HISTORY = """\
IAX was written by Mark Spencer for Asterisk in 2002 and rewritten as IAX2
in 2004, against the many ports and the NAT troubles of SIP and RTP: one
UDP port, 4569, signalling and media together, and trunking that packs the
voice of many calls into one packet. It was published as RFC 5456 in 2010
by Mark Spencer, Brian Capouch, Ed Guy, Frank Miller and Kenneth Shumard.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PA, PB, FA, FB = "pbxa", "pbxb", "phonea", "phoneb"
ALL = (PA, PB, FA, FB)
WIRING = {PA: [(0, "lana", "10.1.0.1/24"), (1, "wan", "192.0.2.1/24")],
          PB: [(0, "lanb", "10.2.0.1/24"), (1, "wan", "192.0.2.2/24")],
          FA: [(0, "lana", "10.1.0.2/24")],
          FB: [(0, "lanb", "10.2.0.2/24")]}
GATEWAY = {}
LAYOUT = {FA: (-480, 0), "lana": (-320, 0), PA: (-160, 0), "wan": (0, 0),
          PB: (160, 0), "lanb": (320, 0), FB: (480, 0)}
ETHS = {(FA, "eth0"): 90, (PA, "eth0"): 270, (PA, "eth1"): 90,
        (PB, "eth1"): 270, (PB, "eth0"): 90, (FB, "eth0"): 270}
CARD = r"""
    phonea ---- pbxa ==== IAX2 ==== pbxb ---- phoneb
  Two sites, one trunk, one port.
"""


def iax_conf(me, peer, peer_ip):
    return """\
[general]
bindport = 4569
bindaddr = 0.0.0.0
disallow = all
allow = ulaw

[%s]
type = friend
host = %s
username = %s
secret = %s
context = from-iax
qualify = yes
""" % (peer, peer_ip, me, SECRET)


def extensions(local, remote, peer):
    return """\
[phones]
exten => _%sXXX,1,Dial(PJSIP/${EXTEN},20)
exten => _%sXXX,1,Dial(IAX2/%s/${EXTEN},20)

[from-iax]
exten => _%sXXX,1,Dial(PJSIP/${EXTEN},20)
""" % (local, remote, peer, local)


def main():
    opening()
    hosts()
    step("act 1: two switches, the IAX2 trunk, a phone on each")
    asterisk_start(PA, {"pjsip.conf": UDP_TRANSPORT + endpoint("2001", "phones", SECRET),
                        "iax.conf": iax_conf("sitea", "siteb", "192.0.2.2"),
                        "extensions.conf": extensions("2", "3", "siteb")},
                   modules=("chan_iax2",))
    asterisk_start(PB, {"pjsip.conf": UDP_TRANSPORT + endpoint("3001", "phones", SECRET),
                        "iax.conf": iax_conf("siteb", "sitea", "192.0.2.1"),
                        "extensions.conf": extensions("3", "2", "sitea")},
                   modules=("chan_iax2",))
    phone_start(FA, "2001", ip_of(PA))
    phone_start(FB, "3001", ip_of(PB), answer=True, tone=600)
    ok_reg = until(lambda: registered(PA, "2001") and registered(PB, "3001"), 20, 1)
    ok_peer = until(lambda: "OK (" in rx(PA, "iax2 show peers"), 30, 1)
    proof("pbxa, iax2 show peers:", rx(PA, "iax2 show peers"))
    hold()
    step("act 2: phonea dials 3001 across the trunk")
    def call():
        phone_cmd(FA, "dial", "sip:3001@%s" % ip_of(PA))
        up = until(lambda: "established" in phone_log(FA), 20, 1)
        until(lambda: False, 4, 1)
        return up
    iax = count_on(PA, 1, "udp port 4569", call)
    ok_call = bool(iax) and bool(iax["out"])
    ok_iax = bool(iax) and iax["count"] > 50
    chans = rx(PB, "iax2 show channels")
    phone_cmd(FA, "hangup")
    until(lambda: "terminated" in phone_log(FA), 10, 1)
    rtp = count_on(PA, 1, "udp portrange 10000-20000", call)
    ok_nortp = bool(rtp) and rtp["count"] == 0
    phone_cmd(FA, "hangup")
    proof("pbxb, iax2 show channels during the call:", chans)
    verdict([("both phones registered with their switch", ok_reg),
             ("the IAX2 trunk is up, its peer reachable", bool(ok_peer)),
             ("phonea reaches phoneb across the trunk", ok_call),
             ("the call crosses the wan in UDP 4569", ok_iax),
             ("no RTP on the wan", ok_nortp)])
    story("""
the network is left up. Things to try, in a terminal of pbxa:
the trunk and its round trip:
asterisk -rx "iax2 show peers"
the console of asterisk, its channels as they come:
asterisk -rvvv
""")


main()
