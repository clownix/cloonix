#!/usr/bin/env python3
r"""codecs - Codecs and transcoding.

    ./codecs.py [net]     the net defaults to nemo

The codecs of a call and the translation Asterisk makes between them, as
the codec pages of the documentation describe them: each endpoint allows
one codec, G.722 (wideband, 7 kHz of audio), G.711 mu-law (the 64 kbit/s
of the telephone network) or GSM 06.10 (13 kbit/s, the codec of 2G), and
asterisk transcodes through its translation paths. On the wire, the size of
the RTP packets tells the codec; the headers make the rest of the bill.
https://docs.asterisk.org/

    phone1 (10.0.0.2) 6001, G.722 ---+
    phone2 (10.0.0.3) 6002, G.711 ---+---- lan1 ---- pbx (10.0.0.1)
    phone3 (10.0.0.4) 6003, GSM -----+

  ACT 1  phone1 (G.722) calls phone2 (G.711): one leg in each codec, the
         translation path of asterisk; 172 bytes of UDP every 20 ms on both,
         G.722 doubling the audio band for the same 85.6 kbit/s of frames.
  ACT 2  phone3 (GSM) calls phone2: 45 bytes of UDP, a payload a fifth of
         G.711's, yet frames of 87 bytes, 40% of the bill: headers dominate.

Image: asterisk_trixie (asterisk, baresip, tcpdump)
Size:  4 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf phone3 eth0 sniff=on
    cloonix cli nemo sniff phone3 eth0 --dump "udp portrange 10000-20000"
  A GSM packet: 12 bytes of RTP, 8 of UDP, 20 of IP and 14 of Ethernet
  around 33 bytes of voice.
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
    asterisk_start, channelstats, endpoint, hosts, ip_of, opening,
    phone_cmd, phone_log, phone_start, proof, registered, rx, SECRET, ssh,
    UDP_TRANSPORT, until, verdict)

HISTORY = """\
G.711, the 64 kbit/s PCM of the telephone network, was standardised by the
ITU in 1972; G.722 brought wideband speech, 7 kHz instead of 3.4, at the
same 64 kbit/s in 1988; GSM 06.10, full rate, squeezed a voice into 13
kbit/s for the mobile phones of 1990-91. Asterisk translates between all
the codecs it knows, the cheapest path through signed linear, and codec
negotiation is the first line of every SDP.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PBX, P1, P2, P3 = "pbx", "phone1", "phone2", "phone3"
ALL = (PBX, P1, P2, P3)
WIRING = {PBX: [(0, "lan1", "10.0.0.1/24")], P1: [(0, "lan1", "10.0.0.2/24")],
          P2: [(0, "lan1", "10.0.0.3/24")], P3: [(0, "lan1", "10.0.0.4/24")]}
GATEWAY = {}
LAYOUT = {P1: (-220, -110), P2: (-220, 0), P3: (-220, 110), "lan1": (0, 0),
          PBX: (220, 0)}
ETHS = {(P1, "eth0"): 135, (P2, "eth0"): 90, (P3, "eth0"): 45, (PBX, "eth0"): 270}
CARD = r"""
    phone1 G.722, phone2 G.711, phone3 GSM ---- pbx (asterisk)
  One call, two codecs, and the bytes each one costs.
"""

PJSIP_CONF = UDP_TRANSPORT + endpoint("6001", "demo", SECRET, codecs=("g722",)) + \
    endpoint("6002", "demo", SECRET, codecs=("ulaw",)) + \
    endpoint("6003", "demo", SECRET, codecs=("gsm",))
EXTENSIONS_CONF = """\
[demo]
exten => 6002,1,Dial(PJSIP/6002,20)
"""


def udp_len(node):
    """The UDP length of the RTP node sends, from 10 packets of tcpdump."""
    out = ssh(node, "timeout 5 tcpdump -nn -c 10 -i eth0 'udp and src host %s "
                    "and portrange 10000-20000' 2>/dev/null" % ip_of(node),
              check=False)
    lens = [int(l.split("length")[-1]) for l in out.split("\n") if "length" in l]
    return (max(set(lens), key=lens.count) if lens else 0), out


def call(src, user):
    phone_cmd(src, "dial", "sip:6002@%s" % ip_of(PBX))
    until(lambda: "established" in phone_log(src), 15, 1)
    size, out = udp_len(src)
    stats = channelstats(PBX)
    phone_cmd(src, "hangup")
    until(lambda: "terminated" in phone_log(src), 10, 1)
    return size, out, stats


def main():
    opening()
    hosts()
    asterisk_start(PBX, {"pjsip.conf": PJSIP_CONF, "extensions.conf": EXTENSIONS_CONF},
                   modules=("codec_g722", "codec_gsm", "codec_resample"))
    phone_start(P1, "6001", ip_of(PBX), codecs=("g722",))
    phone_start(P2, "6002", ip_of(PBX), answer=True, tone=800)
    phone_start(P3, "6003", ip_of(PBX), codecs=("gsm",), tone=600)
    until(lambda: all(registered(PBX, u) for u in ("6001", "6002", "6003")), 20, 1)
    step("act 1: G.722 calls G.711, asterisk translates")
    size1, out1, stats1 = call(P1, "6001")
    codecs1 = sorted(r["codec"] for r in stats1.values())
    ok_two = "g722" in codecs1 and "ulaw" in codecs1
    proof("pbx, the two legs and their codecs; phone1, its RTP on the wire:",
          rx(PBX, "core show translation paths g722") + "\n" + str(codecs1)
          + "\n" + out1)
    hold()
    step("act 2: GSM calls G.711: smaller packets, heavier headers")
    size3, out3, stats3 = call(P3, "6003")
    proof("phone3, its GSM RTP on the wire:", out3)
    story("    G.722 and G.711: %d bytes of UDP, %d bytes of frame, %.1f kbit/s"
          % (size1, size1 + 42, (size1 + 42) * 8 * 50 / 1000.0))
    story("    GSM: %d bytes of UDP, %d bytes of frame, %.1f kbit/s"
          % (size3, size3 + 42, (size3 + 42) * 8 * 50 / 1000.0))
    verdict([("the call holds a G.722 leg and a G.711 leg", ok_two),
             ("G.722 sends 160 bytes of voice a packet (UDP 172)", size1 == 172),
             ("GSM sends 33 bytes of voice a packet (UDP 45)", size3 == 45)])


main()
