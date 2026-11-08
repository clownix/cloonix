#!/usr/bin/env python3
r"""transfers - Feature Code Call Transfers and Call Parking.

    ./transfers.py [net]     the net defaults to nemo

Two pages of the Asterisk documentation on the features of a call: the
feature codes of features.conf, #1 for a blind transfer, typed as DTMF
during the call (the Dial options t and T allow them), and call parking,
res_parking, whose lot answers on 700 and parks on 701 to 720.
https://docs.asterisk.org/

    A, SIPp (10.0.0.4) ----+
    B, baresip (10.0.0.2) -+---- lan1 ---- pbx (10.0.0.1)
    C, baresip (10.0.0.3) -+               features, parking lot 700

  ACT 1  A calls B, then types #1 and 7003: B is sent to C, A leaves; the
         bridge of the switch now holds B and C.
  ACT 2  A calls B again, types #1 and 700: B is parked on 701, waiting;
         C dials 701 and takes the call.

Image: asterisk_trixie (asterisk, baresip, sipp)
Size:  4 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf pbx eth0 sniff=on
    cloonix cli nemo sniff pbx eth0 --dump "udp portrange 10000-20000 and host 10.0.0.4"
  The RTP events of A, # then 1: the DTMF reaches asterisk as RFC 4733
  packets beside the voice, and the switch acts on them.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py, asterisklib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import hold, step, story
from asterisklib import (
    asterisk_start, endpoint, hosts, ip_of, opening, phone_cmd,
    phone_start, play, proof, put_any, put_dtmf, registered, rx, SECRET,
    sipp, sipp_uac_xml, trunk, UDP_TRANSPORT, until, verdict)

HISTORY = """\
Transferring a call and parking it are features of the PBX since the
electromechanical switches; the stored program PBX of the 1970s made them
codes typed on the keypad. SIP added its own transfer, REFER, RFC 3515 of
2003 by Robert Sparks; Asterisk keeps both, the feature codes of
features.conf acting on the DTMF of either party, and a parking lot,
res_parking since Asterisk 12, built on holding bridges.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PBX, B, C, A = "pbx", "phoneb", "phonec", "callera"
ALL = (PBX, B, C, A)
WIRING = {PBX: [(0, "lan1", "10.0.0.1/24")], B: [(0, "lan1", "10.0.0.2/24")],
          C: [(0, "lan1", "10.0.0.3/24")], A: [(0, "lan1", "10.0.0.4/24")]}
GATEWAY = {}
LAYOUT = {A: (-220, -110), B: (-220, 0), C: (-220, 110), "lan1": (0, 0),
          PBX: (220, 0)}
ETHS = {(A, "eth0"): 135, (B, "eth0"): 90, (C, "eth0"): 45, (PBX, "eth0"): 270}
CARD = r"""
    A (SIPp), B, C (baresip) ---- pbx (asterisk)
  A blind transfer by #1, a call parked on 701 and taken back.
"""

PJSIP_CONF = UDP_TRANSPORT + endpoint("7002", "demo", SECRET) + \
    endpoint("7003", "demo", SECRET) + trunk("sipp", "10.0.0.4", "demo",
                                             codecs=("alaw", "ulaw"))
FEATURES_CONF = """\
[general]
transferdigittimeout = 3

[featuremap]
blindxfer = #1
"""
PARKING_CONF = """\
[general]

[default]
parkext = 700
parkpos = 701-720
context = parkedcalls
parkingtime = 60
"""
EXTENSIONS_CONF = """\
[demo]
include => parkedcalls
exten => 7002,1,Dial(PJSIP/7002,20,tT)
exten => 7003,1,Dial(PJSIP/7003,20,tT)
"""


def a_calls_b_and_types(keys):
    put_dtmf(A, "/tmp/keys.pcap", keys, 500)
    put_any(A, "/tmp/xfer.xml", sipp_uac_xml(
        "7002", 6000, '  <pause milliseconds="2000"/>\n' + play("/tmp/keys.pcap"),
        switch_hangs_up=True))
    return sipp(A, "-sf /tmp/xfer.xml %s:5060 -i %s -p 5060 -mp 6000 -m 1 "
                "-timeout 40 -timeout_error" % (ip_of(PBX), ip_of(A)))


def main():
    opening()
    hosts()
    asterisk_start(PBX, {"pjsip.conf": PJSIP_CONF, "features.conf": FEATURES_CONF,
                         "res_parking.conf": PARKING_CONF,
                         "extensions.conf": EXTENSIONS_CONF},
                   modules=("res_parking", "bridge_holding",
                            "bridge_builtin_features"))
    phone_start(B, "7002", ip_of(PBX), answer=True, tone=500)
    phone_start(C, "7003", ip_of(PBX), answer=True, tone=900)
    until(lambda: registered(PBX, "7002") and registered(PBX, "7003"), 20, 1)
    step("act 1: A calls B, types #1 7003, B goes to C")
    rc = a_calls_b_and_types("#17003")
    ok_bc = until(lambda: "PJSIP/7002" in rx(PBX, "core show channels")
                  and "PJSIP/7003" in rx(PBX, "core show channels"), 15, 1)
    chans = rx(PBX, "core show channels")
    ok_a_gone = "PJSIP/sipp" not in chans
    proof("pbx, the channels after the transfer:", chans)
    phone_cmd(C, "hangup")
    until(lambda: "PJSIP/7002" not in rx(PBX, "core show channels"), 15, 1)
    hold()
    step("act 2: A calls B, types #1 700, B parked on 701, C takes it")
    a_calls_b_and_types("#1700")
    ok_parked = until(lambda: "PJSIP/7002" in rx(PBX, "parking show default"), 15, 1)
    proof("pbx, parking show default:", rx(PBX, "parking show default"))
    phone_cmd(C, "dial", "sip:701@%s" % ip_of(PBX))
    ok_taken = until(lambda: "PJSIP/7002" not in rx(PBX, "parking show default")
                     and "PJSIP/7003" in rx(PBX, "core show channels")
                     and "PJSIP/7002" in rx(PBX, "core show channels"), 15, 1)
    proof("pbx, the channels once C dialled 701:", rx(PBX, "core show channels"))
    verdict([("A types #1 7003 as DTMF, its call ends", rc == 0 and ok_a_gone),
             ("B and C are in a call after the transfer", ok_bc),
             ("#1 700 parks B on 701", ok_parked),
             ("C dials 701 and takes B", ok_taken)])
    story("""
the network is left up. Things to try, in a terminal of pbx:
the SIP dialogs as ladders:
sngrep
the console of asterisk, its channels as they come:
asterisk -rvvv
the parked calls:
asterisk -rx "parking show default"
""")


main()
