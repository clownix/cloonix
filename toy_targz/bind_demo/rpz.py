#!/usr/bin/env python3
r"""rpz - DNS Firewalls and Response Policy Zones.

    ./rpz.py [net]     the net defaults to nemo

The examples of the BIND 9 Administrator Reference Manual (ARM), chapter
"Advanced Configurations", section "DNS Firewalls and Response Policy
Zones": the rule set rpz.example.com kept on a distribution master and
copied by NOTIFY and zone transfer to a subscribing resolver (Practical
Tips, Keeping Firewall Policies Updated), its names forced to NXDOMAIN, an
address range sent to a walled garden (Creating a Simple Walled Garden
Triggered by IP Address), and mozilla.rpz, the canary domain of Firefox
(Using RPZ to Disable Mozilla DoH-by-Default).
https://bind9.readthedocs.io/en/stable/chapter6.html#dns-firewalls-and-response-policy-zones

    pc  192.0.200.10 --+                    +-- dm   192.0.222.2  the rule set
                       +-- lan200 -- r -- wan +       (distribution master)
    res 192.0.200.1 ---+         (router)   +-- inet 198.41.0.4   the lab internet
        resolver, subscriber                         (root and every zone)

  inet plays the whole internet: the root and every zone the demo asks,
  malicious1.org, malicious2.org, malicious.net, use-application-dns.net
  and example.com. res resolves from it, and rewrites what its policy
  zones say.

  ACT 1  the internet, dm with an empty rule set, then res, which copies
         it: the malicious names resolve.
  ACT 2  dm gets the rules of the ARM and reloads: NOTIFY, transfer, and
         res answers NXDOMAIN for malicious1.org and malicious2.org and
         every name under them, while the internet still answers them.
  ACT 3  use-application-dns.net, the canary of Firefox, NXDOMAIN by
         mozilla.rpz: Firefox keeps the resolver of the network.
  ACT 4  an address rule on dm: www.malicious.net, whose address is in
         109.94.212.0/22, is sent to drop.garden.example.com.

Image: dns_trixie (bind9)
Size:  5 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf res eth0 sniff=on
    cloonix cli nemo sniff res eth0 --dump "host 192.0.222.2"

  ACT 2 on the wire: the NOTIFY of dm, the SOA question of res, and the
  transfer that carries the four new rules.

Important
  The recursive-server sample of the ARM has a stray semicolon after the
  zone name, zone "rpz.example.com"; type secondary; ...: named-checkconf
  refuses it as printed, it is removed here. res is also given what the
  "// ..." of the ARM leaves out: its directory, a log file, the hint of
  the lab root, and dnssec-validation no (the lab root has no key). Its
  response-policy has min-update-interval 5: by default BIND rebuilds its
  policy from a changed rule set at most once a minute, a pause the demo
  would wait through. Three of the four subscribers of the ARM are not in
  this net: the NOTIFY sent to them fails. ixfr-from-differences makes dm
  keep the differences of each reload; for a rule set this small, the
  whole zone is smaller than its differences and BIND sends it whole
  (max-ixfr-ratio): notify_ixfr shows an IXFR.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and bindlib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, hold, ssh, step, story
from bindlib import (
    ROOT_HINTS, answer_of, dig, forwarding, lab_conf, log_of, named_start,
    opening, proof, put_any, rndc, status_of, until, verdict)

HISTORY = """\
Response Policy Zones were designed by Paul Vixie and Vernon Schryver and
came into BIND 9.8 in 2011: a firewall for names, whose rules are the
records of a zone, so a security vendor can sell them as a feed and every
resolver of a network can copy them by zone transfer. Banks, companies
and internet providers use it to make the domains of phishing and malware
vanish for their users.

BIND, written at Berkeley in 1984, maintained by Paul Vixie from 1988, was
rewritten as BIND 9 in 2000 by the Internet Systems Consortium.
"""

ROOTFS_CHOICES = ("dns_trixie",)
TOOL = "named"

PC, RES, R, DM, INET = "pc", "res", "r", "dm", "inet"
ALL = (RES, PC, R, DM, INET)
L200, WAN = "lan200", "wan"

WIRING = {
    PC: [(0, L200, "192.0.200.10/24")],
    RES: [(0, L200, "192.0.200.1/24")],
    R: [(0, L200, "192.0.200.254/24"), (1, WAN, "192.0.222.254/24")],
    DM: [(0, WAN, "192.0.222.2/24")],
    INET: [(0, WAN, "198.41.0.4/24")],
}
GATEWAY = {PC: "192.0.200.254", RES: "192.0.200.254", DM: "192.0.222.254",
           INET: "198.41.0.1"}

LAYOUT = {PC: (-460, -100), RES: (-460, 100), L200: (-240, 0), R: (0, 0),
          WAN: (240, 0), DM: (460, -100), INET: (460, 100)}
ETHS = {(PC, "eth0"): 90, (RES, "eth0"): 90, (R, "eth0"): 270,
        (R, "eth1"): 90, (DM, "eth0"): 270, (INET, "eth0"): 270}

# The lab internet, every zone on one server.
INTERNET = {
    ".": """a.root-servers.net. IN A 198.41.0.4
org. IN NS a.root-servers.net.
net. IN NS a.root-servers.net.
com. IN NS a.root-servers.net.
""",
    "org": """malicious1.org. IN NS a.root-servers.net.
malicious2.org. IN NS a.root-servers.net.
""",
    "net": """a.root-servers.net. IN A 198.41.0.4
malicious.net. IN NS a.root-servers.net.
use-application-dns.net. IN NS a.root-servers.net.
""",
    "com": "example.com. IN NS a.root-servers.net.\n",
    "malicious1.org": "@ IN A 203.0.113.11\nwww IN A 203.0.113.11\n",
    "malicious2.org": "@ IN A 203.0.113.12\nsub IN A 203.0.113.13\n",
    "malicious.net": "www IN A 109.94.212.22\n",
    "use-application-dns.net": "@ IN A 198.51.100.53\n",
    "example.com": "drop.garden IN A 192.168.7.89\nwww IN A 192.168.254.7\n",
}

def zone_path(name):
    return "/var/cache/bind/%s.zone" % ("root" if name == "." else name)

def zone_file(name, body):
    return ("$TTL 300\n@ IN SOA a.root-servers.net. hostmaster.example.com. "
            "1 1800 900 604800 60\n@ IN NS a.root-servers.net.\n" + body)

INET_CONF = lab_conf([(name, zone_path(name)) for name in INTERNET])
INET_FILES = [(zone_path(name), zone_file(name, body))
              for name, body in INTERNET.items()]

# 6.9.8 Practical Tips: the distribution master; 6.9.6: ixfr-from-differences.
DM_CONF = """\
options {
          directory "/var/cache/bind";
          recursion no;
          // ...
          ixfr-from-differences yes;
          // ...
};

zone "rpz.example.com" {
     type primary;
     file "primary/rpz.example.com";
     notify explicit;
     also-notify { 192.0.200.1;
                   192.0.201.1;
                   192.0.202.1;
                   192.0.203.1; };
     allow-transfer { 192.0.200.1;
                      192.0.201.1;
                      192.0.202.1;
                      192.0.203.1; };
     allow-query { localhost; };
};
"""

RPZ_HEAD = """\
$TTL 300
@ IN SOA localhost. hostmaster.example.com. %d 3600 600 86400 60
@ IN NS  localhost.
"""
# 6.9.6 Keeping Firewall Policies Updated
RULES_NAMES = """\
$ORIGIN rpz.example.com.
malicious1.org          CNAME .
*.malicious1.org        CNAME .
malicious2.org          CNAME .
*.malicious2.org        CNAME .
"""
# 6.9.9 Creating a Simple Walled Garden Triggered by IP Address
RULES_GARDEN = """\
$ORIGIN rpz.example.com.
22.0.212.94.109.rpz-ip          CNAME   drop.garden.example.com.
*.212.94.109.in-addr.arpa       CNAME   .
*.213.94.109.in-addr.arpa       CNAME   .
*.214.94.109.in-addr.arpa       CNAME   .
*.215.94.109.in-addr.arpa       CNAME   .
"""

# 6.9.8 the subscribing resolver, its stray semicolon removed, its "// ..."
# given what the lab needs; 6.9.11 mozilla.rpz beside rpz.example.com.
RES_CONF = """\
options {
     // ...
     directory "/var/cache/bind";
     dnssec-validation no;
     response-policy {
          zone "rpz.example.com";
          zone "mozilla.rpz";
     } break-dnssec yes min-update-interval 5;
     // ...
};

logging {
  channel example_log {
    file "/var/log/named/example.log" versions 3 size 250k;
    severity info;
  };
  category default { example_log; };
};

zone "." {
     type hint;
     file "named.root";
};

zone "rpz.example.com" {
     type secondary;
     primaries { 192.0.222.2; };
     file "secondary/rpz.example.com";
     allow-query { localhost; };
     allow-transfer { none; };
};

zone mozilla.rpz {
    type primary;
    file "mozilla.rpz.db";
    allow-query { localhost; };
};
"""
# 6.9.11 the policy zone file mozilla.rpz.db
MOZILLA_RPZ = """\
$TTL  604800
$ORIGIN       mozilla.rpz.
@     IN      SOA     localhost. root.localhost. 1 604800 86400 2419200 604800
@     IN      NS      localhost.
use-application-dns.net CNAME .
"""

CARD = r"""
    pc ---+                 +--- dm   (the rule set rpz.example.com)
          +-- r --- wan ----+
    res --+                 +--- inet (the lab internet)
    (resolver, subscriber)

  Names made to vanish, an address range sent to a walled garden.
"""

def via_res(name):
    return dig(PC, "@192.0.200.1 %s" % name)

def addr(name):
    return dig(PC, "@192.0.200.1 %s +short" % name).strip().split("\n")[-1]

def rules(serial, body):
    put_any(DM, "/var/cache/bind/primary/rpz.example.com",
            RPZ_HEAD % serial + body)
    rndc(DM, "reload rpz.example.com")

def main():
    opening()
    forwarding(R)
    ssh(R, "ip addr add 198.41.0.1/24 dev eth1")

    step("act 1: the lab internet, dm with no rule, res subscribed")
    named_start(INET, INET_CONF, INET_FILES)
    named_start(DM, DM_CONF, [("/var/cache/bind/primary/rpz.example.com",
                               RPZ_HEAD % 1)])
    ssh(RES, "mkdir -p /var/cache/bind/secondary")
    named_start(RES, RES_CONF, [("/var/cache/bind/named.root", ROOT_HINTS),
                                ("/var/cache/bind/mozilla.rpz.db",
                                 MOZILLA_RPZ)])
    before = dict((n, addr(n)) for n in ("www.malicious1.org",
                                         "sub.malicious2.org",
                                         "www.malicious.net"))
    ok_before = before == {"www.malicious1.org": "203.0.113.11",
                           "sub.malicious2.org": "203.0.113.13",
                           "www.malicious.net": "109.94.212.22"}
    proof("pc, through res, no rule yet:", "\n".join(
        "%-20s %s" % (n, a) for n, a in sorted(before.items())))
    hold()

    step("act 2: the rules of the ARM on dm, copied to res by NOTIFY")
    got = count_on(RES, 0, "host 192.0.222.2", lambda: (
        rules(2, RULES_NAMES),
        until(lambda: status_of(via_res("www.malicious1.org")) ==
              "NXDOMAIN", 30, 0.5))[1], settle=1.0)
    ok_copied = bool(got) and got["out"]
    ok_wire = bool(got) and got["count"] >= 3
    gone = dict((n, status_of(via_res(n))) for n in (
        "malicious1.org", "www.malicious1.org", "malicious2.org",
        "sub.malicious2.org"))
    ok_gone = all(s == "NXDOMAIN" for s in gone.values())
    still = addr("drop.garden.example.com") == "192.168.7.89"
    truth = dig(PC, "@198.41.0.4 www.malicious1.org +short").strip()
    ok_truth = truth == "203.0.113.11"
    proof("pc, through res, after the rules:", "\n".join(
        "%-30s %s" % (n, s) for n, s in sorted(gone.items())))
    proof("pc, the internet itself still answers www.malicious1.org:", truth)
    proof("res, the copy of the rules:", "\n".join(
        l for l in log_of(RES).split("\n")
        if "rpz.example.com" in l and "serial 2" in l))
    rewrites = [l for l in log_of(RES).split("\n")
                if "rpz QNAME NXDOMAIN rewrite" in l]
    ok_log = bool(rewrites)
    proof("res, its last rewrite in its log:", rewrites[-1] if rewrites
          else "")
    hold()

    step("act 3: the canary domain of Firefox")
    canary = via_res("use-application-dns.net")
    real = dig(PC, "@198.41.0.4 use-application-dns.net +short").strip()
    ok_canary = status_of(canary) == "NXDOMAIN" and real == "198.51.100.53"
    proof("pc, use-application-dns.net, through res, then from the internet:",
          "%s\n%s" % (status_of(canary), real))
    hold()

    step("act 4: an address range sent to the walled garden")
    rules(3, RULES_NAMES + RULES_GARDEN)
    ok_garden = until(lambda: addr("www.malicious.net") == "192.168.7.89",
                      30, 0.5)
    garden = answer_of(via_res("www.malicious.net"))
    ok_cname = "CNAME\tdrop.garden.example.com." in garden
    proof("pc, www.malicious.net through res:", garden)

    verdict([("before the rules, the malicious names resolve", ok_before),
             ("dm reloads, res applies the rules by itself", ok_copied),
             ("NOTIFY, SOA and transfer cross eth0 of res", ok_wire),
             ("the domains and every name under them: NXDOMAIN", ok_gone),
             ("a name without a rule still resolves", still),
             ("the internet still answers, res rewrites", ok_truth),
             ("res logs its rewrites", ok_log),
             ("the canary of Firefox is NXDOMAIN", ok_canary),
             ("109.94.212.0/22 sent to drop.garden.example.com",
              ok_garden and ok_cname)])

    story("""
the network is left up. Things to try:

the rewrites as they happen:
cloonix cli %s ssh res "tail -f /var/log/named/example.log"

a name of the internet, and a forbidden one, from pc:
cloonix cli %s ssh pc "dig @192.0.200.1 www.example.com +short"
cloonix cli %s ssh pc "dig @192.0.200.1 phishing.malicious1.org"
""" % (NET, NET, NET))

main()
