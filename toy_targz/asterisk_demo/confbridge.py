#!/usr/bin/env python3
r"""confbridge - ConfBridge.

    ./confbridge.py [net]     the net defaults to nemo

The conference bridge of Asterisk, app_confbridge, as its page in the
documentation configures it: a bridge profile that records the conference,
a user profile, and an extension that sends callers in. Eight phones join
800, each with its own tone as its voice, 400 to 1100 Hz; the mixer,
bridge_softmix, sums them, and the recording of the conference holds the
eight tones at once.
https://docs.asterisk.org/

    p1 p2 p3 p4 (10.0.0.11..14) ----+
                                    +---- lan1 ---- pbx (10.0.0.1)
    p5 p6 p7 p8 (10.0.0.15..18) ----+               ConfBridge 800

  ACT 1  the eight phones register and dial 800: confbridge list 800.
  ACT 2  the conference is recorded, then ended: the recording, analysed
         here frequency by frequency, holds the eight tones, each well above
         a ninth frequency nobody sent.

Image: asterisk_trixie (asterisk, baresip)
Size:  9 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf pbx eth0 sniff=on
    cloonix cli nemo sniff pbx eth0 --dump "udp portrange 10000-20000"
  Eight RTP streams come in, eight go out, each the mix of the seven others:
  one packet every 20 ms per phone and per direction, 800 packets a second.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py, asterisklib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import count_on, hold, in_parallel, step, story
from asterisklib import (
    asterisk_start, endpoint, hosts, ip_of, opening, phone_cmd,
    phone_start, proof, registered, rx, SECRET, tone_power, UDP_TRANSPORT,
    until, verdict, wav_samples)

HISTORY = """\
Telephone conferences began on operator switchboards; digital mixing came
with the bridges of the 1970s and 80s. In Asterisk, MeetMe mixed calls on
DAHDI timing hardware until ConfBridge replaced it in Asterisk 1.6.2, 2009,
rewritten on the bridging framework of Asterisk 12 by Digium (Joshua Colp,
Richard Mudgett, Matt Jordan and others): a softmix bridge that needs only a
software timer, here res_timing_timerfd.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PBX = "pbx"
PHONES = tuple("p%d" % i for i in range(1, 9))
TONES = {p: 300 + 100 * i for i, p in enumerate(PHONES, 1)}
NOBODY = 1550
ALL = (PBX,) + PHONES
WIRING = {PBX: [(0, "lan1", "10.0.0.1/24")]}
WIRING.update({p: [(0, "lan1", "10.0.0.%d/24" % (10 + i))]
               for i, p in enumerate(PHONES, 1)})
GATEWAY = {}
LAYOUT = {PBX: (300, 0), "lan1": (100, 0)}
LAYOUT.update({p: (-200 + 100 * ((i - 1) % 4), -90 if i <= 4 else 90)
               for i, p in enumerate(PHONES, 1)})
ETHS = {(PBX, "eth0"): 270}
ETHS.update({(p, "eth0"): 90 if i <= 4 else 270
             for i, p in enumerate(PHONES, 1)})
CARD = r"""
    p1 .. p8 (baresip, 8 tones) ---- pbx (asterisk, ConfBridge 800)
  Eight voices summed by the mixer, found again in its recording.
"""

REC = "/tmp/conf800.wav"
PJSIP_CONF = UDP_TRANSPORT + "".join(
    endpoint("70%02d" % i, "demo", SECRET) for i in range(1, 9))
CONFBRIDGE_CONF = """\
[general]

[default_bridge]
type = bridge
record_conference = yes
record_file = %s
record_file_timestamp = no

[default_user]
type = user
quiet = yes
""" % REC
EXTENSIONS_CONF = """\
[demo]
exten => 800,1,Answer()
 same => n,ConfBridge(800,default_bridge,default_user)
 same => n,Hangup()
"""


def main():
    opening()
    hosts()
    step("act 1: eight phones register and dial 800")
    asterisk_start(PBX, {"pjsip.conf": PJSIP_CONF, "confbridge.conf": CONFBRIDGE_CONF,
                         "extensions.conf": EXTENSIONS_CONF},
                   modules=("app_confbridge", "bridge_softmix"))
    in_parallel(lambda p: phone_start(p, "70%02d" % (PHONES.index(p) + 1),
                                      ip_of(PBX), tone=TONES[p]),
                list(PHONES), "baresip")
    until(lambda: all(registered(PBX, "70%02d" % i) for i in range(1, 9)), 30, 1)
    in_parallel(lambda p: phone_cmd(p, "dial", "sip:800@%s" % ip_of(PBX)),
                list(PHONES), "dial")
    ok_all = until(lambda: rx(PBX, "confbridge list 800").count("PJSIP/") == 8, 30, 1)
    proof("pbx, confbridge list 800:", rx(PBX, "confbridge list 800"))
    hold()
    step("act 2: the conference recorded, ended, its recording analysed")
    got = count_on(PBX, 0, "udp portrange 10000-20000",
                   lambda: until(lambda: False, 3, 1), settle=0.5)
    ok_rtp = bool(got) and got["count"] > 8 * 50 * 2
    until(lambda: False, 5, 1)
    rx(PBX, "confbridge kick 800 all")
    until(lambda: "800" not in rx(PBX, "confbridge list"), 15, 1)
    rate, samples = wav_samples(PBX, REC)
    powers = {f: tone_power(rate, samples, f) for f in TONES.values()}
    # eight full-scale tones summed clip in the mixer: their shares differ, and
    # the clipping puts energy on the sums of 100 Hz steps, so the frequency
    # nobody sends is off that grid
    absent = tone_power(rate, samples, NOBODY)
    ok_tones = bool(samples) and all(v > max(0.01, 3 * absent)
                                     for v in powers.values())
    ok_absent = absent < 0.01
    proof("the recording %s, %d samples at %d Hz, power share by frequency:"
          % (REC, len(samples), rate),
          "\n".join("%5d Hz  %.3f   (%s)" % (TONES[p], powers[TONES[p]], p)
                    for p in PHONES) + "\n %4d Hz  %.3f   (nobody)" % (NOBODY, absent))
    verdict([("the eight phones are in conference 800", ok_all),
             ("RTP of the conference crosses eth0 of pbx", ok_rtp),
             ("the recording holds the eight tones", ok_tones),
             ("nothing at %d Hz, sent by nobody" % NOBODY, ok_absent)])
    story("""
the network is left up. Things to try, in a terminal of pbx:
the SIP dialogs as ladders:
sngrep
the console of asterisk, its channels as they come:
asterisk -rvvv
the conference and its members:
asterisk -rx "confbridge list"
""")


main()
