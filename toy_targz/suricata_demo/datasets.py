#!/usr/bin/env python3
r"""datasets - Datasets.

    ./datasets.py [net]     the net defaults to nemo

The first example of the Suricata User Guide, chapter "Datasets": "sets for
tracking unique values", the datasets ua-seen and dns-sha256-seen of
suricata.yaml and their two rules, sid 123 and sid 234.
https://docs.suricata.io/en/suricata-7.0.10/rules/datasets.html

    cl (10.0.1.10) ---- lanA ---- sen ---- lanB ---- srv (203.0.113.10)
                                (router,                http 80
                                 Suricata on eth1)

  dns-sha256-seen holds the sha256 of evil.example; its rule hashes every
  DNS query with to_sha256 and alerts when the hash is in the set.
  ua-seen starts empty; its rule adds every HTTP user agent to the set.

  ACT 1  cl asks DNS for good.example, then evil.example: only the second
         fires sid 123, though the set holds no name, only hashes.
  ACT 2  cl makes HTTP requests with its user agent: sid 234 fires and the
         user agent enters ua-seen; when Suricata stops, ua-seen.lst holds
         it, base64 encoded.

Image: suricata_trixie (suricata, curl, python3)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf sen eth1 sniff=on
    cloonix cli nemo sniff sen eth1 --dump "udp port 53"

  The query for evil.example: its name, hashed by Suricata, is what the set
  compares.

Important
  The datasets of suricata.yaml are given with --set, the keys the guide
  writes in the file; their files are absolute paths. No DNS server
  answers: Suricata inspects the queries.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and suricatalib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
import hashlib
from demolib import NET, hold, ssh, step, story
from suricatalib import count, fast, forwarding, opening, proof, put_any, \
    suricata_start, suricata_stop, until, verdict, web_server

HISTORY = """\
Datasets came in Suricata 5.0, in 2019: large lists of values, strings,
hashes or addresses, loaded from files and filled by the rules themselves,
so that a sensor can match a million bad domains or remember what it has
already seen.

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

# The two rules of the guide.
RULES = """\
alert dns any any -> any any (msg:"dns list test"; dns.query; to_sha256; dataset:isset,dns-sha256-seen; sid:123; rev:1;)
alert http any any -> any any (msg: "http user-agent test"; http.user_agent; dataset:set,ua-seen; sid:234; rev:1;)
"""
D = "/etc/suricata/rules"
# The datasets of the guide's suricata.yaml.
SETS = ("datasets.ua-seen.type=string",
        "datasets.ua-seen.state=%s/ua-seen.lst" % D,
        "datasets.dns-sha256-seen.type=sha256",
        "datasets.dns-sha256-seen.state=%s/dns-sha256-seen.lst" % D)
AGENT = "cloonix-demo-agent"

# One DNS query of type A for the name given, sent to srv.
QUERY = """\
import socket, struct, sys
name = sys.argv[1]
q = struct.pack("!HHHHHH", 0x1234, 0x0100, 1, 0, 0, 0)
for label in name.split("."):
    q += bytes([len(label)]) + label.encode()
q += b"\\x00" + struct.pack("!HH", 1, 1)
socket.socket(socket.AF_INET, socket.SOCK_DGRAM).sendto(q, ("203.0.113.10", 53))
"""

LAYOUT = {CL: (-440, 0), LA: (-220, 0), SEN: (0, 0), LB: (220, 0),
          SRV: (440, 0)}
ETHS = {(CL, "eth0"): 90, (SEN, "eth0"): 270, (SEN, "eth1"): 90,
        (SRV, "eth0"): 270}

CARD = r"""
    cl ---- sen (Suricata, two datasets) ---- srv

  A set of hashes to match against, a set of strings to remember.
"""

def main():
    opening()
    forwarding(SEN)
    web_server(SRV, {"index.html": "hello\n"})
    put_any(CL, "/tmp/query.py", QUERY)
    evil = hashlib.sha256(b"evil.example").hexdigest()
    ssh(SEN, "mkdir -p %s" % D)
    put_any(SEN, "%s/dns-sha256-seen.lst" % D, evil + "\n")
    ssh(SEN, "rm -f %s/ua-seen.lst; touch %s/ua-seen.lst" % (D, D))

    step("act 1: a DNS name whose hash is in the set fires sid 123")
    suricata_start(SEN, RULES, "-i eth1", SETS)
    proof("sen, dns-sha256-seen.lst, the sha256 of evil.example:",
          ssh(SEN, "cat %s/dns-sha256-seen.lst" % D, check=False))
    ssh(CL, "python3 /tmp/query.py good.example", check=False)
    until(lambda: count(SEN, 123) > 0, 8)
    ok_good = count(SEN, 123) == 0
    ssh(CL, "python3 /tmp/query.py evil.example", check=False)
    ok_evil = until(lambda: count(SEN, 123) == 1, 30)
    proof("sen, fast.log:", fast(SEN))
    hold()

    step("act 2: the user agent enters ua-seen, kept in its state file")
    for _ in range(3):
        ssh(CL, "curl -s -A %s http://203.0.113.10/index.html" % AGENT,
            check=False)
    ok_set = until(lambda: count(SEN, 234) >= 1, 30)
    suricata_stop(SEN)
    state = ssh(SEN, "cat %s/ua-seen.lst" % D, check=False)
    proof("sen, ua-seen.lst written at the stop:", state)
    decoded = ssh(SEN, "base64 -d %s/ua-seen.lst 2>/dev/null" % D,
                  check=False)
    ok_state = AGENT in decoded
    story("    decoded: %s" % decoded.strip())
    story()

    verdict([("good.example does not fire", ok_good),
             ("evil.example, by its hash, fires sid 123", ok_evil),
             ("the user agent fires sid 234 as it enters ua-seen", ok_set),
             ("ua-seen.lst keeps the user agent", ok_state)])

    story("""
the network is left up, Suricata stopped on sen. Things to try:

  the dns events, one per query:
    cloonix cli %s ssh sen "grep dns /var/log/suricata/eve.json"

  the hash of a name, as Suricata computes it:
    cloonix cli %s ssh sen "printf evil.example | sha256sum"
""" % (NET, NET))

main()
