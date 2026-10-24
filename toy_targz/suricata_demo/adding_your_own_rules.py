#!/usr/bin/env python3
r"""adding_your_own_rules - Adding Your Own Rules.

    ./adding_your_own_rules.py [net]     the net defaults to nemo

The guide of the Suricata User Guide, chapter "Rule Management", section
"Adding Your Own Rules": a local.rules file, added to the rule-files of
suricata.yaml, checked, then triggered; its example rule "Do not read
gossip during work".
https://docs.suricata.io/en/suricata-7.0.10/rule-management/adding-your-own-rules.html

    cl (10.0.1.10) ---- lanA ---- sen ---- lanB ---- srv (203.0.113.10)
                                (router,                a gossip page
                                 Suricata on eth1)

  ACT 1  local.rules is written with the rule of the guide and added to
         suricata.yaml; "suricata -T" checks that it loads.
  ACT 2  cl reads a page about Scarlett: the alert [1:1:1] is in the two
         places the guide names, fast.log and eve.json.

Image: suricata_trixie (suricata, curl, python3)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf sen eth1 sniff=on
    cloonix cli nemo sniff sen eth1 --dump "tcp port 80"

  The answer of srv holding "Scarlett": content with nocase matches it in
  any case.

Important
  The rule-files of suricata.yaml are set on the command line with --set,
  the same keys the guide edits in the file.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and suricatalib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, hold, ssh, step, story
from suricatalib import count, events, fast, forwarding, opening, proof, \
    put_any, suricata_start, until, verdict, web_server

HISTORY = """\
Writing one's own rules is how a sensor learns what matters to its
network: the public rulesets know the internet, not the house. The rule of
the guide, about reading gossip at work, has been its example since 2011.

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

# The rule of the guide, in local.rules.
LOCAL = """\
alert http any any -> any any (msg:"Do not read gossip during work"; content:"Scarlett"; nocase; classtype:policy-violation; sid:1; rev:1;)
"""
SETS = ("default-rule-path=/etc/suricata/rules",
        "rule-files.0=local.rules")
PAGE = "<html><body>What SCARLETT did last night</body></html>\n"

CARD = r"""
    cl ---- sen (Suricata, local.rules) ---- srv

  A rule of one's own, from the file to the alert.
"""

def main():
    opening()
    forwarding(SEN)
    web_server(SRV, {"gossip.html": PAGE})

    step("act 1: local.rules, added to suricata.yaml, checked with -T")
    ssh(SEN, "mkdir -p /etc/suricata/rules")
    put_any(SEN, "/etc/suricata/rules/local.rules", LOCAL)
    test = ssh(SEN, "suricata -T -c /etc/suricata/suricata.yaml -l /tmp %s "
                    "2>&1 | tail -3"
               % " ".join("--set %s" % s for s in SETS), check=False)
    proof("sen, suricata -T:", test)
    ok_test = "successfully loaded" in test
    suricata_start(SEN, None, "-i eth1", SETS, rule_opt="")
    hold()

    step("act 2: cl reads the gossip page, the alert in fast.log and eve.json")
    page = ssh(CL, "curl -s http://203.0.113.10/gossip.html", check=False)
    proof("cl, the page it read:", page)
    ok_page = "SCARLETT" in page
    ok_fast = until(lambda: "[1:1:1] Do not read gossip during work"
                    in fast(SEN), 30)
    proof("sen, fast.log:", fast(SEN))
    ok_eve = count(SEN, 1) >= 1
    # http events: suricata parsed the exchange, whatever the rule did
    proof("sen, eve.json:", "%d http event(s), %d alert(s) of rule 1"
          % (events(SEN, "http"), count(SEN, 1)))

    verdict([("suricata -T loads local.rules", ok_test),
             ("cl reads the gossip page", ok_page),
             ("fast.log shows [1:1:1] Do not read gossip during work", ok_fast),
             ("eve.json holds the alert", ok_eve)])

    story("""
the network is left up. Things to try:

  the rules suricata loaded, counted in its log:
    cloonix cli %s ssh sen "grep -i rules /var/log/suricata/suricata.log"

  the page again:
    cloonix cli %s ssh cl "curl -s http://203.0.113.10/gossip.html"
""" % (NET, NET))

main()
