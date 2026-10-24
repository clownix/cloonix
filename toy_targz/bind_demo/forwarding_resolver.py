#!/usr/bin/env python3
r"""forwarding_resolver - Forwarding Resolver Configuration.

    ./forwarding_resolver.py [net]     the net defaults to nemo

The example of the BIND 9 Administrator Reference Manual (ARM), chapter
"Configurations and Zone Files", section "Forwarding Resolver
Configuration": a forwarding resolver that sends every question it cannot
answer to the full-service resolver at 192.168.250.3, with an alternative
at 192.168.230.27, and caches what they answer.
https://bind9.readthedocs.io/en/stable/chapter3.html#forwarding-resolver-configuration

    pc  192.168.4.10 --+                 +-- res1 192.168.250.3   resolvers
                       +-- corp -- r ----+-- res2 192.168.230.27  (core)
    fwd 192.168.4.2 ---+        (router) |
        forwarding resolver              +-- inet: root, gtld, ns1

  res1 and res2 run the resolver named.conf of the ARM, the one of the
  resolver demo; fwd, in corpnets, may ask them. The lab internet is the
  one of the resolver demo.

  ACT 1  the lab internet, the two full-service resolvers, then fwd with
         the named.conf of the ARM.
  ACT 2  pc asks fwd a cold name: fwd forwards it into the core, never to
         the root itself.
  ACT 3  the same name again: fwd answers from its cache, the core hears
         nothing.
  ACT 4  res1 stopped: a new name is answered all the same, through the
         alternative, 192.168.230.27.
  ACT 5  the zones of fwd: the reverse map of 192.168.254.4 is answered
         without forwarding.

Image: dns_trixie (bind9)
Size:  8 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r eth1 sniff=on
    cloonix cli nemo sniff r eth1 --dump "udp port 53"

  The core lan carries only what fwd forwards: one question to one of
  the two resolvers, and its answer, for each name not yet in the cache.

Important
  One statement is added to the named.conf of the ARM, dnssec-validation
  no, as in the resolver demo: with the root key built in BIND, fwd would
  refuse the answers of the lab, whose root has no key.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and bindlib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, hold, in_parallel, ssh, step, story
from bindlib import (
    RESOLVER_CONF, RESOLVER_FILES, answer_of, brief, dig, flags_of,
    forwarding, internet_start, named_start, named_stop, opening, proof,
    verdict)

HISTORY = """\
Forwarding came with the first caching servers: a site sent its questions
to one big cache instead of every host walking the tree, saving the
traffic of slow and costly lines. Home routers still do it today, and so
do the forwarding resolvers of companies, in front of resolvers they
trust.

BIND has forwarded queries since the 1980s: written at Berkeley in 1984,
maintained by Paul Vixie from 1988, rewritten as BIND 9 in 2000 by the
Internet Systems Consortium.
"""

ROOTFS_CHOICES = ("dns_trixie",)
TOOL = "named"

PC, FWD, R, RES1, RES2, ROOT, GTLD, NS1 = \
    "pc", "fwd", "r", "res1", "res2", "root", "gtld", "ns1"
ALL = (FWD, PC, R, RES1, RES2, ROOT, GTLD, NS1)
CORP, CORE, INET = "corp", "core", "inet"

WIRING = {
    PC: [(0, CORP, "192.168.4.10/24")],
    FWD: [(0, CORP, "192.168.4.2/24")],
    R: [(0, CORP, "192.168.4.1/24"), (1, CORE, "192.168.250.1/24"),
        (2, INET, "198.41.0.1/24")],
    RES1: [(0, CORE, "192.168.250.3/24")],
    RES2: [(0, CORE, "192.168.230.27/24")],
    ROOT: [(0, INET, "198.41.0.4/24")],
    GTLD: [(0, INET, "192.5.6.30/24")],
    NS1: [(0, INET, "192.168.254.2/24")],
}
GATEWAY = {PC: "192.168.4.1", FWD: "192.168.4.1", RES1: "192.168.250.1",
           RES2: "192.168.230.1", ROOT: "198.41.0.1", GTLD: "192.5.6.1",
           NS1: "192.168.254.1"}

LAYOUT = {PC: (-480, -100), FWD: (-480, 100), CORP: (-260, 0), R: (0, 0),
          CORE: (0, -200), RES1: (-120, -330), RES2: (120, -330),
          INET: (260, 0), ROOT: (480, -140), GTLD: (480, 0), NS1: (480, 140)}
ETHS = {(PC, "eth0"): 90, (FWD, "eth0"): 90, (R, "eth0"): 270,
        (R, "eth1"): 0, (R, "eth2"): 90, (RES1, "eth0"): 180,
        (RES2, "eth0"): 180, (ROOT, "eth0"): 270, (GTLD, "eth0"): 270,
        (NS1, "eth0"): 270}

# 3.3.3 Forwarding Resolver Configuration, dnssec-validation added.
FORWARDING_CONF = """\
// forwarding named.conf file
// Two corporate subnets we wish to allow queries from
// defined in an acl clause
acl corpnets {
  192.168.4.0/24;
  192.168.7.0/24;
};

// options clause defining the server-wide properties
options {
  // all relative paths use this directory as a base
  directory "/var";
  // version statement for security to avoid hacking known weaknesses
  // if the real version number is revealed
  version "not currently available";
  // this is the default
  recursion yes;
  // recursive queries only allowed from these ips
  // and references the acl clause
  allow-query { corpnets; };
  // this ensures that any reverse map for private IPs
  // not defined in a zone file will *not* be passed to the public network
  // it is the default value
  empty-zones-enable yes;
  // this defines the addresses of the resolvers to which queries will be forwarded
  forwarders {
    192.168.250.3;
    192.168.230.27;
  };
  // indicates all queries will be forwarded other than for defined zones
  forward only;
  // added for the lab: its root has no DNSSEC key
  dnssec-validation no;
};

// logging clause
// log to /var/log/named/example.log all events from info UP in severity (no debug)
// uses 3 files in rotation swaps files when size reaches 250K
// failure messages that occur before logging is established are
// in syslog (/var/log/messages)
//
logging {
  channel example_log {
    // uses a relative path name and the directory statement to
    // expand to /var/log/named/example.log
    file "log/named/example.log" versions 3 size 250k;
    // only log info and up messages - all others discarded
    severity info;
  };
  category default {
    example_log;
  };
};

// hints zone file is not required

// zone file for the localhost forward map
// discretionary zone depending on hosts file (see discussion)
zone "localhost" {
  type primary;
  file "masters/localhost-forward.db";
  notify no;
};

// zone file for the loopback address
// necessary zone
zone "0.0.127.in-addr.arpa" {
  type primary;
  file "localhost.rev";
  notify no;
};

// zone file for local IP reverse map
// discretionary file depending on requirements
zone "254.168.192.in-addr.arpa" {
  type primary;
  file "192.168.254.rev";
  notify no;
};
"""
# The files of RESOLVER_FILES but the hint file.
FORWARDING_FILES = [f for f in RESOLVER_FILES if f[0] != "/var/named.root"]

CARD = r"""
                     res1   res2
                        \   /
    pc ---+            core
          +-- corp ---- r ---- inet: root, gtld, ns1
    fwd --+
    (forwarding resolver)

  Every question forwarded to a resolver, every answer kept.
"""

def ask(name):
    return dig(PC, "@192.168.4.2 %s" % name)

def main():
    opening()
    forwarding(R)
    ssh(R, "ip addr add 192.168.230.1/24 dev eth1")
    for addr in ("192.5.6.1/24", "192.168.254.1/24"):
        ssh(R, "ip addr add %s dev eth2" % addr)

    step("act 1: the lab internet, res1 and res2, then the forwarder")
    internet_start(ROOT, GTLD, NS1)
    in_parallel(lambda n: named_start(n, RESOLVER_CONF, RESOLVER_FILES),
                [RES1, RES2], "named_start")
    named_start(FWD, FORWARDING_CONF, FORWARDING_FILES)
    hold()

    step("act 2: a cold name, forwarded into the core")
    got = count_on(R, 1, "udp port 53", lambda: ask("www.example.com"),
                   settle=1.0)
    out = got["out"] if got else ask("www.example.com")
    ok_answer = "192.168.254.7" in answer_of(out) and "ra" in flags_of(out)
    ok_core = bool(got) and got["count"] >= 2
    proof("pc, the answer of fwd:", brief(out))
    got = count_on(FWD, 0, "udp port 53 and host 198.41.0.4",
                   lambda: ask("mail.example.com"), settle=1.0)
    ok_noroot = bool(got) and got["count"] == 0 and \
        "192.168.254.4" in answer_of(got["out"])
    proof("pc, a second cold name, nothing between fwd and the root:",
          answer_of(got["out"]) if got else "")
    hold()

    step("act 3: the same name again, from the cache of fwd")
    got = count_on(R, 1, "udp port 53", lambda: ask("www.example.com"),
                   settle=1.0)
    ok_cache = bool(got) and got["count"] == 0 and \
        "192.168.254.7" in answer_of(got["out"])
    proof("pc, the cached answer, its TTL counting down:",
          answer_of(got["out"]) if got else "")
    hold()

    step("act 4: res1 stopped, the alternative 192.168.230.27 answers")
    named_stop(RES1)
    got = count_on(R, 1, "udp port 53 and host 192.168.230.27",
                   lambda: ask("joe.example.com"), settle=1.0)
    ok_alt = bool(got) and got["count"] >= 2 and \
        "192.168.254.6" in answer_of(got["out"])
    proof("pc, a new name with res1 down:",
          answer_of(got["out"]) if got else "")
    hold()

    step("act 5: the zones of fwd answer without forwarding")
    got = count_on(R, 1, "udp port 53",
                   lambda: ask("-x 192.168.254.4"), settle=1.0)
    ok_local = bool(got) and got["count"] == 0 and \
        "mail.example.com." in answer_of(got["out"]) and \
        "aa" in flags_of(got["out"])
    proof("pc, 192.168.254.4, from the file 192.168.254.rev of fwd:",
          brief(got["out"]) if got else "")

    verdict([("pc gets www.example.com from fwd", ok_answer),
             ("the cold name crosses the core lan", ok_core),
             ("fwd never asks the root itself", ok_noroot),
             ("the same name again: nothing forwarded", ok_cache),
             ("res1 down: the alternative answers", ok_alt),
             ("the reverse map of fwd answers alone", ok_local)])

    story("""
the network is left up, res1 stopped. Things to try:

res1 back:
cloonix cli %s ssh res1 "named -c /etc/bind/named.conf"

the cache of fwd emptied, then a name forwarded again:
cloonix cli %s ssh fwd "rndc flush"
cloonix cli %s ssh pc "dig @192.168.4.2 www.example.com"
""" % (NET, NET, NET))

main()
