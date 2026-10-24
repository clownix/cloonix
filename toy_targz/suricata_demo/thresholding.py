#!/usr/bin/env python3
r"""thresholding - Thresholding Keywords.

    ./thresholding.py [net]     the net defaults to nemo

The examples of the Suricata User Guide, chapter "Thresholding Keywords":
type "threshold", the spambot signature 2002087 of ET Open, and type
"limit", the Internet Explorer 6 signature 2010706.
https://docs.suricata.io/en/suricata-7.0.10/rules/thresholding.html

    cl (203.0.113.20, 10.0.1.10) ---- lanA ---- sen ---- lanB ---- srv (10.0.2.10)
                                              (router,          smtp 25, http 80
                                               Suricata on eth1)

  The two rules of the guide, word for word. 2002087 counts the "mail
  from:" sent to port 25 of $HOME_NET from outside it, by source, and alerts
  on the tenth within 60 seconds. 2010706 alerts on a request of Internet
  Explorer 6 from $HOME_NET, at most once per source in 180 seconds.

  ACT 1  type threshold: cl, from 203.0.113.20, sends 9 "MAIL FROM:" to srv:
         no alert; the tenth: one alert.
  ACT 2  type limit: cl, from 10.0.1.10, makes 5 requests as MSIE 6.0: one
         alert, not five.

Image: suricata_trixie (suricata, curl, python3)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf sen eth1 sniff=on
    cloonix cli nemo sniff sen eth1 --dump "tcp port 25"

  Ten short SMTP sessions, each carrying "MAIL FROM:": the signature matches
  each one, the threshold lets the tenth through as an alert.

Important
  A short session is inspected once its data fills a chunk of the stream, or
  when its flow ends, 60 s after its close: a chunk of 16 bytes makes each
  MAIL FROM inspected at once.
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
from suricatalib import count, fast, forwarding, listening, opening, proof, \
    put_any, suricata_start, until, verdict, web_server

HISTORY = """\
Thresholds came with the large rulesets of the 2000s: a rule that is right
once can be noise a thousand times. Snort had threshold and limit, Suricata
kept them in the rule itself and in threshold.config.

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
    CL: [(0, LA, "203.0.113.20/24")],
    SEN: [(0, LA, "203.0.113.1/24"), (1, LB, "10.0.2.1/24")],
    SRV: [(0, LB, "10.0.2.10/24")],
}
GATEWAY = {CL: "203.0.113.1", SRV: "10.0.2.1"}

# The two rules of the guide.
# Each MAIL FROM inspected at once, not at the end of its flow.
SETS = ("stream.reassembly.toserver-chunk-size=16",
        "stream.reassembly.randomize-chunk-size=no")

RULES = """\
alert tcp !$HOME_NET any -> $HOME_NET 25 (msg:"ET POLICY Inbound Frequent Emails - Possible Spambot Inbound"; flow:established; content:"mail from|3a|"; nocase; threshold: type threshold, track by_src, count 10, seconds 60; reference:url,doc.emergingthreats.net/2002087; classtype:misc-activity; sid:2002087; rev:10;)
alert http $HOME_NET any -> any $HTTP_PORTS (msg:"ET USER_AGENTS Internet Explorer 6 in use - Significant Security Risk"; flow:to_server,established; content:"|0d 0a|User-Agent|3a| Mozilla/4.0 (compatible|3b| MSIE 6.0|3b|"; threshold: type limit, track by_src, seconds 180, count 1; reference:url,doc.emergingthreats.net/2010706; classtype:policy-violation; sid:2010706; rev:7;)
"""

# A mail server that reads one line and closes.
SMTP = """\
import socket
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("0.0.0.0", 25))
s.listen(16)
while True:
    c, _ = s.accept()
    c.sendall(b"220 srv ESMTP\\r\\n")
    c.recv(1024)
    c.sendall(b"250 OK\\r\\n")
    c.close()
"""

# One session: the banner, one MAIL FROM, the answer.
MAIL = """\
import socket, sys
for i in range(int(sys.argv[1])):
    c = socket.create_connection(("10.0.2.10", 25), timeout=5)
    c.recv(1024)
    c.sendall(b"MAIL FROM:<bot@203.0.113.20>\\r\\n")
    c.recv(1024)
    c.close()
"""
MSIE = "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)"

LAYOUT = {CL: (-440, 0), LA: (-220, 0), SEN: (0, 0), LB: (220, 0),
          SRV: (440, 0)}
ETHS = {(CL, "eth0"): 90, (SEN, "eth0"): 270, (SEN, "eth1"): 90,
        (SRV, "eth0"): 270}

CARD = r"""
    cl ---- sen (Suricata) ---- srv (smtp, http)

  A rule that alerts on the tenth match, and one that alerts once.
"""

def main():
    opening()
    forwarding(SEN)
    ssh(CL, "ip addr add 10.0.1.10/24 dev eth0")
    ssh(SEN, "ip addr add 10.0.1.1/24 dev eth0")
    put_any(SRV, "/tmp/smtp.py", SMTP)
    ssh(SRV, "python3 /tmp/smtp.py </dev/null >/tmp/smtp.log 2>&1 &")
    listening(SRV, 25, "/tmp/smtp.log")
    web_server(SRV, {"index.html": "hello\n"})
    put_any(CL, "/tmp/mail.py", MAIL)

    step("act 1: type threshold, nine MAIL FROM, then the tenth")
    suricata_start(SEN, RULES, "-i eth1", SETS)
    got = count_on(SEN, 1, "tcp port 25", lambda: ssh(
        CL, "python3 /tmp/mail.py 9", check=False), settle=2.0)
    if got is not None:
        story("    %d SMTP packet(s) on eth1 of sen." % got["count"])
    until(lambda: count(SEN, 2002087) > 0, 8)
    ok_nine = count(SEN, 2002087) == 0
    story("    after nine: %d alert." % count(SEN, 2002087))
    ssh(CL, "python3 /tmp/mail.py 1", check=False)
    ok_ten = until(lambda: count(SEN, 2002087) == 1, 30)
    story("    after the tenth: %d alert." % count(SEN, 2002087))
    story()
    proof("sen, fast.log:", fast(SEN))
    hold()

    step("act 2: type limit, five requests of MSIE 6.0, one alert")
    for _ in range(5):
        ssh(CL, "curl -s --interface 10.0.1.10 -A '%s' "
                "http://10.0.2.10/index.html" % MSIE, check=False)
    until(lambda: count(SEN, 2010706) > 1, 10)
    ok_limit = count(SEN, 2010706) == 1
    story("    five requests, %d alert." % count(SEN, 2010706))
    story()
    proof("sen, fast.log:", fast(SEN))

    verdict([("nine MAIL FROM: no alert", ok_nine),
             ("the tenth: one alert", ok_ten),
             ("five MSIE 6.0 requests: one alert", ok_limit)])

    story("""
the network is left up. Things to try:

  ten more MAIL FROM, a second alert:
    cloonix cli %s ssh cl "python3 /tmp/mail.py 10"

  the alerts of the threshold rule:
    cloonix cli %s ssh sen "grep 2002087 /var/log/suricata/fast.log"
""" % (NET, NET))

main()
