#!/usr/bin/env python3
r"""voice_quality - RTCP and the jitter buffer.

    ./voice_quality.py [net]     the net defaults to nemo

The quality of a voice stream and what Asterisk does about it: RTCP reports
the loss and the jitter of each stream (pjsip show channelstats), and
JITTERBUFFER, a dialplan function of the documentation, puts a jitter
buffer on a channel. The path is degraded by an a2b of cloonix, with loss,
then with jitter, on the way from the phone to the switch only.
https://docs.asterisk.org/

    phone (10.1.0.2) -- lana -- cable (a2b) -- lanb -- pbx (10.1.0.1)
    baresip, 440 Hz          A2B: loss, jitter        echo test 100,
                                                      and 101 with a jitter buffer

  ACT 1  a clean call to the echo test: no loss, the echo comes back to
         the phone every 20 ms.
  ACT 2  loss=500 (5%) from the phone: the switch counts the lost packets.
  ACT 3  jitter=60 ms from the phone: the switch measures the jitter, and
         its echo carries it back, irregular.
  ACT 4  the same jitter, extension 101 with JITTERBUFFER(fixed): the
         echo comes back every 20 ms again, the buffer absorbed the jitter.

Image: asterisk_trixie (asterisk, baresip, tcpdump)
Size:  2 containers, 2 lans, 1 a2b

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf pbx eth0 sniff=on
    cloonix cli nemo sniff pbx eth0 --dump "udp portrange 10001-20001"
  An RTCP receiver report of the switch: fraction lost, cumulative lost and
  interarrival jitter, what the phone learns of its own stream.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py, asterisklib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, cli, hold, run, step
from asterisklib import (
    asterisk_start, channelstats, endpoint, hosts, ip_of, opening,
    phone_cmd, phone_log, phone_start, proof, registered, rx, SECRET, ssh,
    UDP_TRANSPORT, until, verdict)

HISTORY = """\
RTP and its control protocol RTCP were written by Henning Schulzrinne,
Stephen Casner, Ron Frederick and Van Jacobson as RFC 1889 in 1996, RFC 3550
in 2003: each receiver reports the share of packets lost and the
interarrival jitter, a running estimate of how irregular the stream is.
The jitter buffer, holding packets a few tens of ms to play them evenly,
is as old as packet voice; Asterisk has one on a channel by JITTERBUFFER.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PBX, PHONE, CABLE = "pbx", "phone", "cable"
ALL = (PBX, PHONE)
WIRING = {PBX: [(0, "lanb", "10.1.0.1/24")], PHONE: [(0, "lana", "10.1.0.2/24")]}
GATEWAY = {}
LAYOUT = {PHONE: (-440, 0), "lana": (-220, 0), CABLE: (0, 0), "lanb": (220, 0),
          PBX: (440, 0)}
ETHS = {(PHONE, "eth0"): 90, (PBX, "eth0"): 270}
CARD = r"""
    phone ---- cable (a2b: loss, jitter) ---- pbx (echo, jitter buffer)
  Loss and jitter measured by RTCP, jitter absorbed by a buffer.
"""

PJSIP_CONF = UDP_TRANSPORT + endpoint("6001", "demo", SECRET)
EXTENSIONS_CONF = """\
[demo]
exten => 100,1,Answer()
 same => n,Echo()
exten => 101,1,Set(JITTERBUFFER(fixed)=default)
 same => n,Answer()
 same => n,Echo()
"""


def impair(cmd, val):
    run("cloonix cli %s cnf %s A2B %s=%d" % (NET, CABLE, cmd, val))


def echo_call(exten):
    """A call to exten: (stats of pbx at 8 s, spread in ms of the echo
    arrivals at the phone, its mean interval in ms)."""
    phone_cmd(PHONE, "dial", "sip:%s@%s" % (exten, ip_of(PBX)))
    until(lambda: "established" in phone_log(PHONE), 15, 1)
    out = ssh(PHONE, "timeout 8 tcpdump -tt -nn -c 200 -i eth0 'udp and src host "
                     "%s and portrange 10000-20000' 2>/dev/null" % ip_of(PBX),
              check=False)
    stats = channelstats(PBX)
    phone_cmd(PHONE, "hangup")
    until(lambda: "terminated" in phone_log(PHONE), 10, 1)
    ssh(PHONE, "> /tmp/baresip.log")
    t = [float(l.split()[0]) for l in out.split("\n") if l[:1].isdigit()]
    d = [1000.0 * (b - a) for a, b in zip(t, t[1:])][20:]
    if len(d) < 20:
        return stats, None, None
    mean = sum(d) / len(d)
    spread = (sum((x - mean) ** 2 for x in d) / len(d)) ** 0.5
    return stats, spread, mean


def rxmax(stats, key):
    return max([r[key] for r in stats.values()] or [0])


def main():
    opening()
    hosts()
    cli("add", "a2b", CABLE)
    cli("plug", CABLE, "A", "lana")
    cli("plug", CABLE, "B", "lanb")
    asterisk_start(PBX, {"pjsip.conf": PJSIP_CONF, "extensions.conf": EXTENSIONS_CONF},
                   modules=("func_jitterbuffer",))
    phone_start(PHONE, "6001", ip_of(PBX))
    until(lambda: registered(PBX, "6001"), 30, 1)
    step("act 1: a clean call to the echo test")
    s1, spread1, mean1 = echo_call("100")
    ok_clean = rxmax(s1, "rx") > 100 and rxmax(s1, "lost") == 0
    proof("pbx, channelstats; phone, the echo every %.1f ms, spread %.1f ms:"
          % (mean1 or 0, spread1 or 0), rx(PBX, "pjsip show channelstats"))
    hold()
    step("act 2: 5% loss from the phone, RTCP counts it")
    impair("loss", 500)
    s2, _, _ = echo_call("100")
    impair("loss", 0)
    rxn, lost = rxmax(s2, "rx"), rxmax(s2, "lost")
    share = 100.0 * lost / (rxn + lost) if rxn + lost else 0
    ok_loss = 1.0 <= share <= 10.0
    proof("pbx, %d received, %d lost, %.1f%%:" % (rxn, lost, share), str(s2))
    hold()
    step("act 3: 60 ms of jitter from the phone, the echo brings it back")
    impair("jitter", 60)
    s3, spread3, mean3 = echo_call("100")
    ok_jit = rxmax(s3, "jitter") > rxmax(s1, "jitter")
    ok_irregular = spread3 is not None and spread1 is not None and spread3 > 3 * spread1 + 2
    proof("pbx, the jitter it measures; phone, the echo spread %.1f ms:"
          % (spread3 or 0), str(s3))
    hold()
    step("act 4: the same jitter, extension 101 with a jitter buffer")
    s4, spread4, mean4 = echo_call("101")
    impair("jitter", 0)
    ok_buffer = spread4 is not None and spread3 is not None and spread4 < spread3 / 2
    proof("phone, the echo through the jitter buffer every %.1f ms, spread %.1f ms:"
          % (mean4 or 0, spread4 or 0), "")
    verdict([("the clean call loses nothing", ok_clean),
             ("5% of loss set, between 1 and 10% measured", ok_loss),
             ("the switch measures more jitter", ok_jit),
             ("the plain echo comes back irregular", ok_irregular),
             ("the jitter buffer makes it regular again", ok_buffer)])


main()
