#!/usr/bin/env python3
r"""quickstart_alerting - Quickstart guide: Alerting.

    ./quickstart_alerting.py [net]     the net defaults to nemo

The test of the Suricata User Guide, chapter "Quickstart guide", section
"Alerting": the signature 2100498 of the ET Open ruleset and the curl of
http://testmynids.org/uid/index.html that triggers it.
https://docs.suricata.io/en/suricata-7.0.10/quickstart.html#alerting

    cl (10.0.1.10) ---- lanA ---- sen ---- lanB ---- srv (203.0.113.10)
                                (router,                testmynids.org,
                                 Suricata on eth1)      offline

  sen routes between the two lans and runs Suricata on eth1 with the one
  signature of the guide, 2100498 "GPL ATTACK_RESPONSE id check returned
  root". srv plays testmynids.org: its page /uid/index.html is the output
  of the id command of root.

  ACT 1  Suricata starts on sen with the signature.
  ACT 2  cl runs the curl of the guide: fast.log shows the line the guide
         shows, [1:2100498:7], and eve.json holds the same alert.

Image: suricata_trixie (suricata, curl, python3)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf sen eth1 sniff=on
    cloonix cli nemo sniff sen eth1 --dump "tcp port 80"

  The answer of srv: "uid=0(root) gid=0(root) groups=0(root)" in its body,
  the bytes the content "uid=0|28|root|29|" of the signature names.

Important
  testmynids.org is played by srv, so the demo needs no internet; its
  address is given to curl by --resolve.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and suricatalib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, hold, ssh, step, story
from suricatalib import count, fast, forwarding, opening, proof, \
    suricata_start, until, verdict, web_server

HISTORY = """\
The signature 2100498 is one of the oldest of the ET Open ruleset, a GPL
rule of the Snort community from the early 2000s: it spots the answer of a
compromised host to an id command run by an attacker. testmynids.org serves
that answer on purpose, so that anyone can check a sensor in one command.

Suricata is made by the Open Information Security Foundation, OISF,
founded in 2009 with funding of the US Department of Homeland Security; its
first release came in 2010, written by Victor Julien and others,
multi-threaded from the start, its rule language broadly the one of Snort.
"""

ROOTFS_CHOICES = ("suricata_trixie",)
TOOL = "suricata"

CL, SEN, SRV = "cl", "sen", "srv"
ALL = (SEN, CL, SRV)
LA, LB = "lanA", "lanB"

WIRING = {
    CL: [(0, LA, "10.0.1.10/24")],
    SEN: [(0, LA, "10.0.1.1/24"), (1, LB, "203.0.113.1/24")],
    SRV: [(0, LB, "203.0.113.10/24")],
}
GATEWAY = {CL: "10.0.1.1", SRV: "203.0.113.1"}

LAYOUT = {CL: (-440, 0), LA: (-220, 0), SEN: (0, 0), LB: (220, 0),
          SRV: (440, 0)}
ETHS = {(CL, "eth0"): 90, (SEN, "eth0"): 270, (SEN, "eth1"): 90,
        (SRV, "eth0"): 270}

# The signature of the guide.
RULES = """\
alert ip any any -> any any (msg:"GPL ATTACK_RESPONSE id check returned root"; content:"uid=0|28|root|29|"; classtype:bad-unknown; sid:2100498; rev:7; metadata:created_at 2010_09_23, updated_at 2010_09_23;)
"""
PAGE = "uid=0(root) gid=0(root) groups=0(root)\n"
CURL = ("curl -s --resolve testmynids.org:80:203.0.113.10 "
        "http://testmynids.org/uid/index.html")

CARD = r"""
    cl ---- sen (Suricata) ---- srv (testmynids.org)

  One signature, one curl: the test the guide gives for a new sensor.
"""

def main():
    opening()
    forwarding(SEN)
    web_server(SRV, {"uid/index.html": PAGE})

    step("act 1: Suricata starts on sen with the signature 2100498")
    suricata_start(SEN, RULES, "-i eth1")
    story("    Suricata watches eth1 of sen, the side of srv.")
    hold()

    step("act 2: the curl of the guide, the alert of the guide")
    got = count_on(SEN, 1, "tcp port 80", lambda: ssh(CL, CURL, check=False),
                   settle=1.0)
    out = got["out"] if got else ssh(CL, CURL, check=False)
    proof("cl, the page it fetched:", out)
    ok_page = "uid=0(root)" in out
    ok_fast = until(lambda: "[1:2100498:7] GPL ATTACK_RESPONSE id check "
                    "returned root" in fast(SEN), 30)
    proof("sen, fast.log:", fast(SEN))
    ok_eve = count(SEN, 2100498) >= 1
    proof("sen, the alert in eve.json:", ssh(
        SEN, "grep event_type.:.alert /var/log/suricata/eve.json | tail -1",
        check=False))

    verdict([("cl fetches the page of testmynids.org", ok_page),
             ("fast.log shows [1:2100498:7]", ok_fast),
             ("eve.json holds the alert", ok_eve)])

    story("""
the network is left up. Things to try:

  the alerts as they come:
    cloonix cli %s ssh sen "tail -f /var/log/suricata/fast.log"

  the curl again:
    cloonix cli %s ssh cl "%s"
""" % (NET, NET, CURL))

main()
