#!/usr/bin/env python3
r"""secure_calling - Secure Calling Tutorial.

    ./secure_calling.py [net]     the net defaults to nemo

The page "Secure Calling Tutorial" of the Asterisk documentation: TLS for
the signalling, part 1, then SRTP for the voice, part 2, keyed by SDES (the
key goes in the SDP, inside the TLS). The certificate of asterisk is made
by openssl on pbx; the phones take it without checking it, as the page
does with Blink.
https://docs.asterisk.org/Deployment/Secure-Calling/Secure-Calling-Tutorial/

    phone1 (10.0.0.2) ---+
    TLS + SRTP           |
                         +--- lan1 --- pbx (10.0.0.1)
    phone2 (10.0.0.3) ---+             asterisk, TLS on 5061,
    TLS, no SRTP                       media_encryption = sdes

  ACT 1  the transport-tls of the page on 5061; both phones register over
         TLS: the capture of pbx sees TLS on 5061 and nothing on 5060.
  ACT 2  phone1 calls 100 with SRTP: the call works, nothing readable on
         the wire, while the log of asterisk, past the TLS, shows the SDP
         of the call with RTP/SAVP and its a=crypto key.
  ACT 3  phone2 offers plain RTP: the endpoint wants SDES, asterisk refuses
         the call with 488 Not Acceptable Here.

Image: asterisk_trixie (asterisk, baresip, openssl)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf pbx eth0 sniff=on
    cloonix cli nemo sniff pbx eth0 --dump "tcp port 5061"
  The Client Hello of a phone, the only readable thing of a call: TLS
  hides the INVITE, SRTP the voice.
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
    asterisk_log, asterisk_start, endpoint, hosts, ip_of, opening,
    phone_cmd, phone_log, phone_start, proof, put_any, registered, rx,
    SECRET, ssh, until, verdict)

HISTORY = """\
Netscape designed SSL in 1994 for the web; the IETF made it TLS, RFC 2246,
in 1999 (Tim Dierks and Christopher Allen), and SIP over TLS, port 5061,
came with RFC 3261 in 2002. The voice needed its own protection, RTP
packets too small and too many for TLS: SRTP, RFC 3711 of 2004, by Mark
Baugher and David McGrew of Cisco and Mats Naslund, Elisabetta Carrara and
Karl Norrman of Ericsson, encrypts each packet with AES; SDES, RFC 4568 of
2006, carries its key in the SDP, which only TLS then protects.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PBX, P1, P2 = "pbx", "phone1", "phone2"
ALL = (PBX, P1, P2)
WIRING = {PBX: [(0, "lan1", "10.0.0.1/24")],
          P1: [(0, "lan1", "10.0.0.2/24")],
          P2: [(0, "lan1", "10.0.0.3/24")]}
GATEWAY = {}
LAYOUT = {P1: (-220, -80), P2: (-220, 80), "lan1": (0, 0), PBX: (220, 0)}
ETHS = {(P1, "eth0"): 120, (P2, "eth0"): 60, (PBX, "eth0"): 270}
CARD = r"""
    phone1 (TLS + SRTP) ----+
                            +---- pbx (asterisk, TLS 5061, SDES)
    phone2 (TLS only) ------+
  Calls with nothing readable on the wire, and a call refused for its
  clear voice.
"""

KEYS = "/etc/asterisk/keys"
# rewrite_contact (res_pjsip_nat): the BYE of asterisk goes back over the TLS
# connection of the phone, which accepts none of its own (no certificate)
TLS_PHONE = ("media_encryption = sdes", "rewrite_contact = yes")
REQ_CNF = """\
[req]
distinguished_name = dn
prompt = no

[dn]
CN = pbx
"""
PJSIP_CONF = """\
[transport-tls]
type = transport
protocol = tls
bind = 0.0.0.0:5061
cert_file = %s/asterisk.crt
priv_key_file = %s/asterisk.key
method = tlsv1_2

""" % (KEYS, KEYS) + endpoint("6001", "local", SECRET, extra=TLS_PHONE) + \
    endpoint("6002", "local", SECRET, extra=TLS_PHONE)
EXTENSIONS_CONF = """\
[local]
exten => 100,1,Answer()
 same => n,Playback(hello-world)
 same => n,Hangup()
"""


def main():
    opening()
    hosts()
    step("act 1: the certificate, the transport-tls, two phones over TLS")
    # openssl req reads its configuration from req.cnf, the toy root
    # carrying no openssl.cnf
    ssh(PBX, "mkdir -p %s" % KEYS)
    put_any(PBX, KEYS + "/req.cnf", REQ_CNF)
    ssh(PBX, "openssl req -x509 -newkey rsa:2048 -nodes -days 365 -config "
             "{0}/req.cnf -keyout {0}/asterisk.key -out {0}/asterisk.crt".format(KEYS))
    asterisk_start(PBX, {"pjsip.conf": PJSIP_CONF,
                         "extensions.conf": EXTENSIONS_CONF},
                   modules=("res_srtp",))
    rx(PBX, "pjsip set logger on")
    def both():
        phone_start(P1, "6001", ip_of(PBX), transport="tls", srtp=True)
        phone_start(P2, "6002", ip_of(PBX), transport="tls")
        until(lambda: registered(PBX, "6001") and registered(PBX, "6002"), 20, 1)
    got = count_on(PBX, 0, "tcp port 5061", both)
    ok_tls = bool(got) and got["count"] > 0
    ok_reg = registered(PBX, "6001") and registered(PBX, "6002")
    proof("pbx, the registrar, both contacts over TLS:",
          rx(PBX, "pjsip show contacts"))
    hold()
    step("act 2: phone1 calls 100, signalling in TLS, voice in SRTP")
    def call():
        phone_cmd(P1, "dial", "sip:100@%s" % ip_of(PBX))
        return until(lambda: "terminated" in phone_log(P1), 30, 1)
    got = count_on(PBX, 0, "port 5060", call)
    ok_clear = bool(got) and got["count"] == 0
    ok_call = bool(got) and bool(got["out"])
    sdp = asterisk_log(PBX, "RTP/SAVP") + "\n" + asterisk_log(PBX, "a=crypto")
    ok_savp = "RTP/SAVP" in sdp and "a=crypto" in sdp
    ok_play = "hello-world" in asterisk_log(PBX, "Playing")
    proof("pbx, the SDP lines past the TLS, in the log of asterisk:", sdp)
    hold()
    step("act 3: phone2 offers clear RTP, asterisk refuses the call")
    phone_cmd(P2, "dial", "sip:100@%s" % ip_of(PBX))
    ok_488 = until(lambda: "488" in phone_log(P2), 20, 1)
    proof("phone2, what baresip says:",
          "\n".join(l for l in phone_log(P2).split("\n") if "488" in l))
    verdict([("both phones register over TLS on 5061", ok_tls and ok_reg),
             ("no packet on 5060 during the call", ok_clear),
             ("the call to 100 plays hello-world and ends", ok_call and ok_play),
             ("the SDP of the call says RTP/SAVP with a key", ok_savp),
             ("clear RTP is refused with 488", ok_488)])
    story("""
the network is left up. Things to try, in a terminal of pbx:
the SIP dialogs as ladders:
sngrep
the console of asterisk, its channels as they come:
asterisk -rvvv
the certificate asterisk shows to the phones:
openssl x509 -in /etc/asterisk/keys/asterisk.crt -noout -subject -dates
""")


main()
