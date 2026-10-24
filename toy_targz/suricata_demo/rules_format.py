#!/usr/bin/env python3
r"""rules_format - Rules Format.

    ./rules_format.py [net]     the net defaults to nemo

The example rule of the Suricata User Guide, chapter "Rules Format", the
signature the whole chapter takes apart: action, header, options.
https://docs.suricata.io/en/suricata-7.0.10/rules/intro.html

    cl (10.0.1.10) ---- lanA ---- sen ---- lanB ---- srv (203.0.113.10)
       $HOME_NET                (router,                $EXTERNAL_NET
                                 Suricata on eth1)

  The rule of the guide, word for word: alert, http from $HOME_NET to
  $EXTERNAL_NET, an established flow to the server, the method GET and a
  URI holding "rule", sid 123. cl is in $HOME_NET (10.0.0.0/8 by default),
  srv outside it.

  ACT 1  cl asks srv for /rule.html: the alert 123 fires.
  ACT 2  each part of the rule, one at a time: a POST to /rule.html (not
         GET), a GET of /index.html (no "rule"), a GET of /rule.html from
         srv to cl (header reversed); none fires.

Image: suricata_trixie (suricata, curl, python3)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf sen eth1 sniff=on
    cloonix cli nemo sniff sen eth1 --dump "tcp port 80"

  The request "GET /rule.html": the http.method buffer holds GET, the
  http.uri buffer /rule.html, the two the options inspect.
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
The rule format comes from Snort, written by Martin Roesch in 1998: one
line, an action, a header like a firewall rule, and options in parentheses.
Suricata kept it and added sticky buffers such as http.method and http.uri,
which point the next content at one field of the parsed protocol.

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

# The rule of the guide.
RULES = """\
alert http $HOME_NET any -> $EXTERNAL_NET any (msg:"HTTP GET Request Containing Rule in URI"; flow:established,to_server; http.method; content:"GET"; http.uri; content:"rule"; fast_pattern; classtype:bad-unknown; sid:123; rev:1;)
"""
PAGES = {"rule.html": "a page whose name holds rule\n",
         "index.html": "an ordinary page\n"}

CARD = r"""
    cl ($HOME_NET) ---- sen (Suricata) ---- srv ($EXTERNAL_NET)

  The example rule of the guide, and each of its parts tested apart.
"""

def main():
    opening()
    forwarding(SEN)
    web_server(SRV, PAGES)
    web_server(CL, PAGES)

    step("act 1: GET /rule.html from $HOME_NET to $EXTERNAL_NET: alert 123")
    suricata_start(SEN, RULES, "-i eth1")
    got = count_on(SEN, 1, "tcp port 80", lambda: ssh(
        CL, "curl -s http://203.0.113.10/rule.html", check=False), settle=1.0)
    ok_get = until(lambda: count(SEN, 123) == 1, 30)
    if got is not None:
        story("    %d packet(s) of the request on eth1 of sen." % got["count"])
    proof("sen, fast.log:", fast(SEN))
    hold()

    step("act 2: change one part at a time, nothing fires")
    ssh(CL, "curl -s -X POST http://203.0.113.10/rule.html", check=False)
    ssh(CL, "curl -s http://203.0.113.10/index.html", check=False)
    ssh(SRV, "curl -s http://10.0.1.10/rule.html", check=False)
    until(lambda: count(SEN, 123) > 1, 10)
    ok_none = count(SEN, 123) == 1
    story("    POST instead of GET, /index.html instead of /rule.html, and")
    story("    srv to cl instead of cl to srv: the rule stays at one alert.")
    story()

    verdict([("GET /rule.html from cl to srv fires sid 123", ok_get),
             ("the POST, the other URI, the reversed header do not", ok_none)])

    story("""
the network is left up. Things to try:

  the http events of eve.json, method and uri per request:
    cloonix cli %s ssh sen "grep http /var/log/suricata/eve.json"

  the rule, as loaded:
    cloonix cli %s ssh sen "cat /etc/suricata/rules/demo.rules"
""" % (NET, NET))

main()
