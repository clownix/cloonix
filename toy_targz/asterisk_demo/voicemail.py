#!/usr/bin/env python3
r"""voicemail - Voicemail.

    ./voicemail.py [net]     the net defaults to nemo

The voicemail of Asterisk, app_voicemail, as its pages in the documentation
describe it: a call to 6001 not answered within 8 s ends in its mailbox
6001@default (VoiceMail), the phone is told a message waits (MWI, a NOTIFY
with Messages-Waiting: yes), and the mailbox is opened by its password
typed as DTMF (VoiceMailMain on *98). The caller is SIPp, which plays a
recording as its voice and the DTMF keys as RFC 4733 events.
https://docs.asterisk.org/

    phone (10.0.0.2) ---- lan1 ---- pbx (10.0.0.1) ---- lan1 ---- caller
    baresip, 6001                   asterisk                     (10.0.0.3)
    rings, never answers            mailbox 6001, password 1234   SIPp

  ACT 1  the caller dials 6001: the phone rings 8 s, asterisk takes the
         call to the mailbox, the caller speaks, hangs up: msg0000.wav.
  ACT 2  asterisk tells the phone: NOTIFY, Messages-Waiting: yes.
  ACT 3  the caller dials *98, types 1234# as DTMF, and hears "you have
         one new message".

Image: asterisk_trixie (asterisk, baresip, sipp)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf phone eth0 sniff=on
    cloonix cli nemo sniff phone eth0 --dump "udp port 5060"
  The NOTIFY of asterisk to the phone after the message, its body
  application/simple-message-summary: Messages-Waiting: yes, Voice-Message:
  1/0, the lamp of a desk phone.
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
    asterisk_log, asterisk_start, endpoint, hosts, ip_of, opening,
    phone_start, play, proof, put_any, put_dtmf, registered, rx, SECRET,
    sipp, SIPP_PCAP, sipp_uac_xml, ssh, trunk, UDP_TRANSPORT, until,
    verdict)

HISTORY = """\
Gordon Matthews, an engineer of Texas, patented the Voice Message Exchange in
1979 and sold the first voicemail system, VMX, to 3M in 1981: a computer
recording the calls nobody answered, behind the PBX. The message waiting
lamp followed, and SIP has its own, RFC 3842 of 2004 by Rohan Mahy: a NOTIFY
carrying Messages-Waiting. app_voicemail, "Comedian Mail", has been in
Asterisk since its first versions.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PBX, PHONE, CALLER = "pbx", "phone", "caller"
ALL = (PBX, PHONE, CALLER)
WIRING = {PBX: [(0, "lan1", "10.0.0.1/24")],
          PHONE: [(0, "lan1", "10.0.0.2/24")],
          CALLER: [(0, "lan1", "10.0.0.3/24")]}
GATEWAY = {}
LAYOUT = {PHONE: (-220, -80), CALLER: (-220, 80), "lan1": (0, 0), PBX: (220, 0)}
ETHS = {(PHONE, "eth0"): 120, (CALLER, "eth0"): 60, (PBX, "eth0"): 270}
CARD = r"""
    phone (6001, never answers) ---+---- pbx (asterisk, mailbox 6001)
    caller (SIPp) -----------------+
  A message left, a lamp lit, a password typed as DTMF.
"""

INBOX = "/var/spool/asterisk/voicemail/default/6001/INBOX"
# mailboxes on the endpoint: asterisk NOTIFYs it unasked; on the aor it
# would only answer a SUBSCRIBE of the phone
PJSIP_CONF = UDP_TRANSPORT + endpoint(
    "6001", "demo", SECRET, extra=("mailboxes = 6001@default",)) + \
    trunk("sipp", "10.0.0.3", "demo", codecs=("alaw",))
VOICEMAIL_CONF = """\
[general]
format = wav

[default]
6001 => 1234,Phone One
"""
EXTENSIONS_CONF = """\
[demo]
exten => 6001,1,Dial(PJSIP/6001,8)
 same => n,VoiceMail(6001@default,u)
 same => n,Hangup()
exten => *98,1,VoiceMailMain(6001@default)
 same => n,Hangup()
"""


def main():
    opening()
    hosts()
    asterisk_start(PBX, {"pjsip.conf": PJSIP_CONF, "voicemail.conf": VOICEMAIL_CONF,
                         "extensions.conf": EXTENSIONS_CONF},
                   modules=("app_voicemail", "res_pjsip_mwi",
                            "res_pjsip_mwi_body_generator"))
    rx(PBX, "pjsip set logger on")
    phone_start(PHONE, "6001", ip_of(PBX))
    until(lambda: registered(PBX, "6001"), 20, 1)
    step("act 1: the caller dials 6001, nobody answers, a message is left")
    put_any(CALLER, "/tmp/leave.xml", sipp_uac_xml(
        "6001", 6000, '  <pause milliseconds="9000"/>\n' + play(SIPP_PCAP, 7000)))
    rc = sipp(CALLER, "-sf /tmp/leave.xml %s:5060 -i %s -p 5060 -mp 6000 -m 1 "
              "-timeout 60 -timeout_error" % (ip_of(PBX), ip_of(CALLER)))
    files = ssh(PBX, "ls -l %s" % INBOX, check=False)
    ok_msg = rc == 0 and "msg0000.wav" in files
    proof("pbx, the mailbox of 6001:", files)
    hold()
    step("act 2: asterisk tells the phone a message waits")
    ok_mwi = until(lambda: asterisk_log(PBX, "Messages-Waiting: yes"), 10, 1)
    proof("pbx, the NOTIFY it sent, read by its pjsip logger:",
          asterisk_log(PBX, "NOTIFY sip:") + "\n"
          + asterisk_log(PBX, "Messages-Waiting") + "\n"
          + asterisk_log(PBX, "Voice-Message"))
    hold()
    step("act 3: *98, the password 1234# as DTMF, \"you have 1 new message\"")
    put_dtmf(CALLER, "/tmp/pass.pcap", "1234#")
    put_any(CALLER, "/tmp/read.xml", sipp_uac_xml(
        "*98", 6000, '  <pause milliseconds="3000"/>\n'
        + play("/tmp/pass.pcap", 9000)))
    rc = sipp(CALLER, "-sf /tmp/read.xml %s:5060 -i %s -p 5060 -mp 6000 -m 1 "
              "-timeout 60 -timeout_error" % (ip_of(PBX), ip_of(CALLER)))
    played = asterisk_log(PBX, "Playing")
    ok_login = rc == 0 and "vm-youhave" in played
    proof("pbx, the prompts of VoiceMailMain:",
          "\n".join(l for l in played.split("\n") if "'vm-" in l)[-1200:])
    verdict([("the unanswered call leaves msg0000.wav", ok_msg),
             ("NOTIFY Messages-Waiting: yes to the phone", bool(ok_mwi)),
             ("1234# typed as DTMF opens the mailbox", ok_login)])
    story("""
the network is left up. Things to try, in a terminal of pbx:
the SIP dialogs as ladders:
sngrep
the console of asterisk, its channels as they come:
asterisk -rvvv
the mailbox, its message and what asterisk knows of it:
ls -l /var/spool/asterisk/voicemail/default/6001/INBOX
""")


main()
