#!/usr/bin/env python3
r"""hello_world - Hello World.

    ./hello_world.py [net]     the net defaults to nemo

The first call of the Asterisk documentation, "Getting Started", page
"Hello World": one SIP phone, 6001, registered with a password, dials
extension 100 of context from-internal, which answers, waits one second,
plays the sound file hello-world and hangs up. pjsip.conf and
extensions.conf are the ones of the page; the phone is baresip, a real
softphone, its microphone a 440 Hz sine.
https://docs.asterisk.org/Getting-Started/Hello-World/

    phone (10.0.0.2) ---- lan1 ---- pbx (10.0.0.1)
    baresip, 6001                   asterisk

  ACT 1  asterisk starts with the two files of the page; the phone
         registers: REGISTER, the challenge 401 Unauthorized, REGISTER again
         with the digest of the password, 200 OK.
  ACT 2  the phone dials 100: asterisk answers, plays hello-world into the
         call (the phone receives its RTP) and hangs up itself.

Image: asterisk_trixie (asterisk, baresip)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf pbx eth0 sniff=on
    cloonix cli nemo sniff pbx eth0 --dump "udp port 5060"
  The 401 of the first REGISTER: asterisk asks for the password without
  ever seeing it, a nonce in WWW-Authenticate that the phone must hash
  with the password into the Authorization of its second REGISTER.
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
    phone_cmd, phone_log, phone_start, proof, registered, rx, SECRET,
    UDP_TRANSPORT, until, verdict)

HISTORY = """\
SIP, the Session Initiation Protocol, was published by Mark Handley, Henning
Schulzrinne, Eve Schooler and Jonathan Rosenberg as RFC 2543 in 1999 and
rewritten as RFC 3261 in 2002: a text protocol built like HTTP, REGISTER to
say where a phone is, INVITE to call it, that leaves the voice to RTP.
Asterisk was started by Mark Spencer in 1999 in Huntsville, Alabama, because
a PBX for his small linux company cost more than writing one; Digium, the
company he founded, grew around it, and Sangoma publishes it today.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PBX, PHONE = "pbx", "phone"
ALL = (PBX, PHONE)
WIRING = {PBX: [(0, "lan1", "10.0.0.1/24")],
          PHONE: [(0, "lan1", "10.0.0.2/24")]}
GATEWAY = {}
LAYOUT = {PHONE: (-220, 0), "lan1": (0, 0), PBX: (220, 0)}
ETHS = {(PHONE, "eth0"): 90, (PBX, "eth0"): 270}
CARD = r"""
    phone (baresip, 6001) ---- pbx (asterisk)
  A phone registers with a password, dials 100, hears hello-world.
"""

PJSIP_CONF = UDP_TRANSPORT + endpoint("6001", "from-internal", SECRET)
EXTENSIONS_CONF = """\
[from-internal]
exten = 100,1,Answer()
same = n,Wait(1)
same = n,Playback(hello-world)
same = n,Hangup()
"""


def main():
    opening()
    hosts()
    step("act 1: asterisk starts, the phone registers with its password")
    version = asterisk_start(PBX, {"pjsip.conf": PJSIP_CONF,
                                   "extensions.conf": EXTENSIONS_CONF})
    got = count_on(PBX, 0, "udp port 5060",
                   lambda: (phone_start(PHONE, "6001", ip_of(PBX)),
                            until(lambda: registered(PBX, "6001"), 20, 1)))
    ok_wire = bool(got) and got["count"] >= 4
    contacts = rx(PBX, "pjsip show contacts")
    ok_reg = registered(PBX, "6001")
    proof("pbx, %s, the registrar:" % version, contacts)
    hold()
    step("act 2: the phone dials 100, asterisk plays hello-world")
    phone_cmd(PHONE, "dial", "sip:100@%s" % ip_of(PBX))
    ok_play = until(lambda: "hello-world" in asterisk_log(PBX, "Playing"), 20, 1)
    ok_end = until(lambda: "terminated" in phone_log(PHONE), 30, 1)
    log = phone_log(PHONE)
    ok_rtp = "incoming rtp" in log
    proof("pbx, the dialplan running:", asterisk_log(PBX, "Executing")
          + "\n" + asterisk_log(PBX, "Playing"))
    proof("phone, what baresip says of the call:",
          "\n".join(l for l in log.split("\n")
                    if "established" in l or "terminated" in l or "rtp" in l))
    verdict([("REGISTER, 401, REGISTER, 200 on eth0 of pbx", ok_wire),
             ("6001 is in the registrar of pbx", ok_reg),
             ("extension 100 plays hello-world", ok_play),
             ("the RTP of asterisk reaches the phone", ok_rtp),
             ("asterisk hangs up, the call is over", ok_end)])
    story("""
the network is left up. Things to try, in a terminal of pbx:
the SIP dialogs as ladders:
sngrep
the console of asterisk, its channels as they come:
asterisk -rvvv
""")


main()
