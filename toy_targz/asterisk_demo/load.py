#!/usr/bin/env python3
r"""load - SIPp: calls per second.

    ./load.py [net]     the net defaults to nemo

SIPp, the call generator of the SIP world, as its documentation runs it
against a switch: a caller scenario at a rate of calls per second with a
cap on simultaneous calls, playing a recorded voice, against an answering
SIPp that echoes the voice back, asterisk in the middle relaying every leg
and translating A-law to mu-law. 300 calls at 10 per second.
https://sipp.sourceforge.net/doc/reference.html

    callers (10.0.0.2) ---+---- lan1 ---- pbx (10.0.0.1)
    SIPp uac, A-law       |               asterisk, Dial, transcoding
    answerer (10.0.0.3) --+
    SIPp uas, mu-law, echo

  ACT 1  the answering SIPp waits; the callers start: 10 calls a second,
         60 at most at once, 6 s each with voice.
  ACT 2  mid run, the switch counts its channels; at the end, SIPp counts
         300 successful calls and no failure, and exits 0.

Image: asterisk_trixie (asterisk, sipp)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf pbx eth0 sniff=on
    cloonix cli nemo sniff pbx eth0 --dump "udp port 5060 and host 10.0.0.3"
  The INVITE of asterisk to the answering SIPp, one per call, among sixty
  calls of two legs each, 50 RTP packets a second each way: 12000 packets a
  second through eth0 of pbx.
"""
import os
import sys
import time

# What every demo shares is in demos/lib/demolib.py, toylib.py, asterisklib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import step
from asterisklib import (
    asterisk_start, hosts, ip_of, opening, play, proof, put_any, rx, sipp,
    SIPP_PCAP, sipp_stats, sipp_uac_xml, sipp_wait, ssh, trunk,
    UDP_TRANSPORT, until, verdict)

HISTORY = """\
SIPp was written at Hewlett-Packard by Richard Gayraud and Olivier Jacques
and published as free software in 2004: XML scenarios of SIP messages, rates
of calls per second, RTP played from pcap files, statistics on screen. It
became the load generator every SIP equipment is tested with, switches,
proxies and session border controllers, and its exit code tells: 0 when
every call went through.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PBX, UAC, UAS = "pbx", "callers", "answerer"
ALL = (PBX, UAC, UAS)
WIRING = {PBX: [(0, "lan1", "10.0.0.1/24")], UAC: [(0, "lan1", "10.0.0.2/24")],
          UAS: [(0, "lan1", "10.0.0.3/24")]}
GATEWAY = {}
LAYOUT = {UAC: (-220, -80), UAS: (-220, 80), "lan1": (0, 0), PBX: (220, 0)}
ETHS = {(UAC, "eth0"): 120, (UAS, "eth0"): 60, (PBX, "eth0"): 270}
CARD = r"""
    callers (SIPp) ---+---- pbx (asterisk) 
    answerer (SIPp) --+
  300 calls with voice, 60 at once, through the switch.
"""

CALLS, RATE, LIMIT = 300, 10, 60
PJSIP_CONF = UDP_TRANSPORT + trunk("uac", "10.0.0.2", "demo", codecs=("alaw",)) + \
    trunk("uas", "10.0.0.3", "demo", codecs=("ulaw",))
EXTENSIONS_CONF = """\
[demo]
exten => 200,1,Dial(PJSIP/uas,10)
"""


def main():
    opening()
    hosts()
    step("act 1: the answerer waits, the callers start")
    asterisk_start(PBX, {"pjsip.conf": PJSIP_CONF, "extensions.conf": EXTENSIONS_CONF})
    sipp(UAS, "-sn uas -i %s -p 5060 -mp 6000 -rtp_echo" % ip_of(UAS),
         log="/tmp/uas.log", background=True)
    put_any(UAC, "/tmp/load.xml", sipp_uac_xml("200", 6000, play(SIPP_PCAP, 6000)))
    sipp(UAC, "-sf /tmp/load.xml %s:5060 -i %s -p 5060 -mp 6000 -r %d -l %d -m %d "
         "-timeout 180 -timeout_error" % (ip_of(PBX), ip_of(UAC), RATE, LIMIT, CALLS),
         background=True)
    until(lambda: False, 15, 1)
    step("act 2: the switch at full load, then the count of SIPp")
    # the rate from the counters of eth0: a capture would copy every packet
    t0, n0 = time.time(), rx_packets(PBX)
    until(lambda: False, 3, 1)
    t1, n1 = time.time(), rx_packets(PBX)
    pps = (n1 - n0) / (t1 - t0)
    mid = rx(PBX, "core show channels count")
    m = [int(w) for w in mid.replace("\n", " ").split() if w.isdigit()]
    active = m[0] if m else 0
    proof("pbx, mid run, %d packets a second received on eth0:" % pps, mid)
    rc = sipp_wait(UAC, timeout=180)
    ok, ko = sipp_stats(UAC)
    total = rx(PBX, "core show channels count")
    proof("callers, SIPp exit %d, %d successful, %d failed; pbx:" % (rc, ok, ko),
          total)
    verdict([("SIPp: the %d calls went through (exit 0)" % CALLS, rc == 0),
             ("the switch held 40 channels at once or more", active >= 40),
             ("thousands of packets a second reach eth0 of pbx", pps > 2000)])


def rx_packets(node):
    out = ssh(node, "cat /sys/class/net/eth0/statistics/rx_packets", check=False)
    return int(out.strip()) if out.strip().isdigit() else 0

main()
