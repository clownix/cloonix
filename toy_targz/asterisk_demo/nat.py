#!/usr/bin/env python3
r"""nat - Configuring res_pjsip to work through NAT.

    ./nat.py [net]     the net defaults to nemo

The page "Configuring res_pjsip to work through NAT" of the Asterisk
documentation, its section on phones behind NAT: the phone gives its
private address in its SDP, asterisk sends the voice there, and nothing
comes back to the phone. The endpoint option rtp_symmetric of the page
makes asterisk send the voice back to where it comes from.
https://docs.asterisk.org/Configuration/Channel-Drivers/SIP/Configuring-res_pjsip/Configuring-res_pjsip-to-work-through-NAT/

    phone (192.168.1.2) -- lan1 -- router -- lan2 -- pbx (203.0.113.10)
    baresip, 6001                 nft masquerade      asterisk
                       192.168.1.1     203.0.113.1

  ACT 1  router masquerades lan1 behind 203.0.113.1; the phone registers,
         force_rport and rewrite_contact keep the signalling working.
  ACT 2  rtp_symmetric = no: the phone calls the echo test, asterisk
         receives the voice and sends its own to 192.168.1.2, an address
         no route of pbx knows: one-way audio, the classic of VoIP.
  ACT 3  rtp_symmetric = yes, pjsip reload: asterisk answers to the address
         and port the voice comes from, the router's, and the phone hears.

Image: asterisk_trixie (asterisk, baresip, nftables)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf router eth1 sniff=on
    cloonix cli nemo sniff router eth1 --dump "udp portrange 10000-20000"
  In act 2, the RTP of pbx goes to 192.168.1.2 and never crosses the
  router; in act 3 it goes to 203.0.113.1, the address the voice of the
  phone came from.
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
    asterisk_start, channelstats, endpoint, hosts, ip_of, opening,
    phone_cmd, phone_log, phone_start, proof, put_any, registered, rx,
    SECRET, ssh, UDP_TRANSPORT, until, verdict)
from toylib import forwarding

HISTORY = """\
NAT, Network Address Translation, was described by Kjeld Egevang and Paul
Francis in RFC 1631 in 1994 as a stopgap against the shortage of IPv4
addresses; it became the rule of every home. SIP suffers twice from it: the
private address written in its messages, and the RTP ports chosen by each
side. Symmetric RTP, sending the voice back to where it comes from, was
written down as RFC 4961 by Dan Wing in 2007; STUN, ICE and TURN are the
other answers.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PBX, ROUTER, PHONE = "pbx", "router", "phone"
ALL = (PBX, ROUTER, PHONE)
WIRING = {PHONE: [(0, "lan1", "192.168.1.2/24")],
          ROUTER: [(0, "lan1", "192.168.1.1/24"), (1, "lan2", "203.0.113.1/24")],
          PBX: [(0, "lan2", "203.0.113.10/24")]}
GATEWAY = {PHONE: "192.168.1.1"}
LAYOUT = {PHONE: (-440, 0), "lan1": (-220, 0), ROUTER: (0, 0),
          "lan2": (220, 0), PBX: (440, 0)}
ETHS = {(PHONE, "eth0"): 90, (ROUTER, "eth0"): 270, (ROUTER, "eth1"): 90,
        (PBX, "eth0"): 270}
CARD = r"""
    phone ---- router (masquerade) ---- pbx
  One-way audio through a NAT, then rtp_symmetric.
"""

EXTENSIONS_CONF = """\
[demo]
exten => 100,1,Answer()
 same => n,Echo()
"""


def pjsip(symmetric):
    return UDP_TRANSPORT + endpoint(
        "6001", "demo", SECRET,
        extra=("force_rport = yes", "rewrite_contact = yes",
               "rtp_symmetric = %s" % ("yes" if symmetric else "no")))


def echo_call():
    """The phone calls the echo test 8 s; (stats of pbx mid call, phone log)."""
    ssh(PHONE, "> /tmp/baresip.log")
    phone_cmd(PHONE, "dial", "sip:100@%s" % ip_of(PBX))
    until(lambda: "established" in phone_log(PHONE), 15, 1)
    until(lambda: False, 4, 1)
    stats = channelstats(PBX)
    phone_cmd(PHONE, "hangup")
    until(lambda: "terminated" in phone_log(PHONE), 10, 1)
    return stats, phone_log(PHONE)


def main():
    opening()
    hosts()
    step("act 1: the router masquerades lan1, the phone registers through it")
    forwarding(ROUTER)
    ssh(ROUTER, "nft add table ip nat && nft add chain ip nat post "
                "'{ type nat hook postrouting priority 100 ; }' && "
                "nft add rule ip nat post oifname eth1 masquerade")
    asterisk_start(PBX, {"pjsip.conf": pjsip(False),
                         "extensions.conf": EXTENSIONS_CONF})
    phone_start(PHONE, "6001", ip_of(PBX))
    ok_reg = until(lambda: registered(PBX, "6001"), 20, 1)
    contacts = rx(PBX, "pjsip show contacts")
    ok_nat = "203.0.113.1" in contacts
    proof("pbx, the contact of 6001, rewritten to the router:", contacts)
    hold()
    step("act 2: rtp_symmetric = no, one-way audio")
    got = count_on(ROUTER, 1, "udp portrange 10000-20000 and src host "
                   "203.0.113.10", echo_call)
    stats, log = got["out"] if got else echo_call()
    rx_pbx = max([r["rx"] for r in stats.values()] or [0])
    ok_oneway = rx_pbx > 0 and "incoming rtp" not in log
    ok_nothing = bool(got) and got["count"] == 0
    proof("pbx, the channel mid call (asterisk receives, sends in vain):",
          rx(PBX, "pjsip show channelstats") or str(stats))
    hold()
    step("act 3: rtp_symmetric = yes, the voice goes back where it comes from")
    put_any(PBX, "/etc/asterisk/pjsip.conf", pjsip(True))
    rx(PBX, "module reload res_pjsip.so")
    got = count_on(ROUTER, 1, "udp portrange 10000-20000 and src host "
                   "203.0.113.10", echo_call)
    stats, log = got["out"] if got else echo_call()
    ok_twoway = "incoming rtp" in log
    ok_back = bool(got) and got["count"] > 0
    proof("phone, what baresip says now:",
          "\n".join(l for l in log.split("\n") if "rtp" in l or "establ" in l))
    verdict([("6001 registers through the router", ok_reg and ok_nat),
             ("rtp_symmetric = no: asterisk hears, the phone does not", ok_oneway),
             ("the RTP of pbx never crosses the router", ok_nothing),
             ("rtp_symmetric = yes: the phone hears", ok_twoway),
             ("the RTP of pbx goes to the router", ok_back)])
    story("""
the network is left up. Things to try, in a terminal of pbx:
the SIP dialogs as ladders:
sngrep
the console of asterisk, its channels as they come:
asterisk -rvvv
the translations the router keeps, in a terminal of router:
nft list ruleset
""")


main()
