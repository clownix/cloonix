#!/usr/bin/env python3
r"""ami - Asterisk Manager Interface (AMI).

    ./ami.py [net]     the net defaults to nemo

The Asterisk Manager Interface, as its pages in the documentation describe
it: a TCP port, 5038, where a program logs in with a user of manager.conf,
sends actions and receives events, lines of "Key: Value" ended by an empty
line. Here an operator plays the program with nc: it logs in, asks
Originate to ring 6001 and connect it to 6002, and reads the events of the
call it set up.
https://docs.asterisk.org/

    phone1 (10.0.0.2) ---+
    phone2 (10.0.0.3) ---+---- lan1 ---- pbx (10.0.0.1)
    operator (10.0.0.4) -+               AMI on 5038

  ACT 1  manager.conf with a user demo; the phones register, both answer
         by themselves.
  ACT 2  the operator logs in and sends Originate: OriginateResponse
         Success, then BridgeEnter twice, the two phones in one bridge.

Image: asterisk_trixie (asterisk, baresip, netcat)
Size:  4 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf pbx eth0 sniff=on
    cloonix cli nemo sniff pbx eth0 --dump "tcp port 5038"
  The Originate action, plain text in TCP: AMI has no encryption of its
  own, it is kept to the machines that drive the switch.
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
    asterisk_start, endpoint, hosts, ip_of, opening, phone_start, proof,
    registered, rx, SECRET, ssh, UDP_TRANSPORT, until, verdict)

HISTORY = """\
The manager interface came with the first versions of Asterisk, written by
Mark Spencer: a socket for the programs around the switch, switchboard
panels, call centre software, click to call from a web page. FreePBX and
the Flash Operator Panel were built on it. ARI, the REST interface of
Asterisk 12 in 2013, came beside it for applications that control the
media; AMI stays the way to watch and to drive calls.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PBX, P1, P2, OP = "pbx", "phone1", "phone2", "operator"
ALL = (PBX, P1, P2, OP)
WIRING = {PBX: [(0, "lan1", "10.0.0.1/24")], P1: [(0, "lan1", "10.0.0.2/24")],
          P2: [(0, "lan1", "10.0.0.3/24")], OP: [(0, "lan1", "10.0.0.4/24")]}
GATEWAY = {}
LAYOUT = {P1: (-220, -110), P2: (-220, 0), OP: (-220, 110), "lan1": (0, 0),
          PBX: (220, 0)}
ETHS = {(P1, "eth0"): 135, (P2, "eth0"): 90, (OP, "eth0"): 45,
        (PBX, "eth0"): 270}
CARD = r"""
    phone1, phone2 (baresip) ---+---- pbx (asterisk, AMI 5038)
    operator (nc) --------------+
  A call set up by a program, its events read back.
"""

MANAGER_CONF = """\
[general]
enabled = yes
port = 5038
bindaddr = 0.0.0.0

[demo]
secret = %s
deny = 0.0.0.0/0.0.0.0
permit = 10.0.0.0/255.255.255.0
read = all
write = all
""" % SECRET
PJSIP_CONF = UDP_TRANSPORT + endpoint("6001", "demo", SECRET) + \
    endpoint("6002", "demo", SECRET)
EXTENSIONS_CONF = """\
[demo]
exten => 6002,1,Dial(PJSIP/6002,20)
"""
SESSION = ("Action: Login\\r\\nUsername: demo\\r\\nSecret: %s\\r\\n\\r\\n"
           "Action: Originate\\r\\nChannel: PJSIP/6001\\r\\nContext: demo\\r\\n"
           "Exten: 6002\\r\\nPriority: 1\\r\\nCallerID: AMI <100>\\r\\n"
           "Async: true\\r\\nActionID: click1\\r\\n\\r\\n" % SECRET)


def main():
    opening()
    hosts()
    step("act 1: the manager user, two phones that answer by themselves")
    asterisk_start(PBX, {"manager.conf": MANAGER_CONF, "pjsip.conf": PJSIP_CONF,
                         "extensions.conf": EXTENSIONS_CONF})
    phone_start(P1, "6001", ip_of(PBX), answer=True, tone=500)
    phone_start(P2, "6002", ip_of(PBX), answer=True, tone=800)
    ok_reg = until(lambda: registered(PBX, "6001") and registered(PBX, "6002"), 20, 1)
    proof("pbx, manager show users:", rx(PBX, "manager show users"))
    hold()
    step("act 2: the operator logs in, Originate, the events of the call")
    out = ssh(OP, "(printf '%s'; sleep 8; printf 'Action: Logoff\\r\\n\\r\\n') "
                  "| nc -q 2 %s 5038" % (SESSION, ip_of(PBX)), check=False)
    out = out.replace("\r", "")
    ok_login = "Authentication accepted" in out
    ok_orig = "Event: OriginateResponse" in out and "Response: Success" in out
    ok_bridge = out.count("Event: BridgeEnter") >= 2
    keep = [l for l in out.split("\n") if l.startswith(
        ("Event:", "Message:", "Response:", "Channel:", "ActionID:"))]
    proof("operator, what AMI said, the keys that matter:", "\n".join(keep[:40]))
    verdict([("both phones registered", ok_reg),
             ("the login of user demo is accepted", ok_login),
             ("Originate answers OriginateResponse Success", ok_orig),
             ("the two phones enter one bridge", ok_bridge)])
    story("""
the network is left up. Things to try, in a terminal of operator:
the events of the switch as they come:
(printf 'Action: Login\\r\\nUsername: demo\\r\\nSecret: cloonix\\r\\n\\r\\n'; sleep 600) | nc 10.0.0.1 5038
""")


main()
