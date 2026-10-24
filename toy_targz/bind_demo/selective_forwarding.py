#!/usr/bin/env python3
r"""selective_forwarding - Selective Forwarding Resolver Configuration.

    ./selective_forwarding.py [net]     the net defaults to nemo

The example of the BIND 9 Administrator Reference Manual (ARM), chapter
"Configurations and Zone Files", section "Selective Forwarding Resolver
Configuration": a resolver that forwards only example.com, to the
resolvers at 192.168.250.3 and 192.168.230.27, and finds every other name
itself, from the root down.
https://bind9.readthedocs.io/en/stable/chapter3.html#selective-forwarding-resolver-configuration

    pc  192.168.4.10 --+                 +-- res1 192.168.250.3   resolvers
                       +-- corp -- r ----+-- res2 192.168.230.27  (core)
    sel 192.168.4.2 ---+        (router) |
        selective forwarder              +-- inet: root, gtld, ns1

  The machines of the forwarding resolver demo; sel replaces fwd.

  ACT 1  the lab internet, the two resolvers, then sel with the named.conf
         of the ARM.
  ACT 2  pc asks two names of example.com: sel forwards them into the core
         and sends nothing to the root.
  ACT 3  pc asks two names of example.net: sel asks the root itself, and
         the core hears nothing.

Image: dns_trixie (bind9)
Size:  8 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf sel eth0 sniff=on
    cloonix cli nemo sniff sel eth0 --dump "udp port 53 and host 198.41.0.4"

  Empty for the names of example.com, busy for those of example.net: the
  zone statement of type forward decides, name by name.

Important
  One statement is added to the named.conf of the ARM, dnssec-validation
  no, as in the resolver demo: the lab root has no key.
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
    RESOLVER_CONF, RESOLVER_FILES, dig, forwarding, internet_start,
    named_start, opening, proof, verdict)

HISTORY = """\
A forward zone sends the questions of one domain to chosen servers and
leaves the rest of the tree to normal resolution: the internal domain of
a company to its internal servers, a partner's domain over a private
link, the domain of a VPN to the resolver at its end.

In BIND it is one zone statement, of type forward. BIND, written at
Berkeley in 1984 and maintained by Paul Vixie from 1988, was rewritten as
BIND 9 in 2000 by the Internet Systems Consortium.
"""

ROOTFS_CHOICES = ("dns_trixie",)
TOOL = "named"

PC, SEL, R, RES1, RES2, ROOT, GTLD, NS1 = \
    "pc", "sel", "r", "res1", "res2", "root", "gtld", "ns1"
ALL = (SEL, PC, R, RES1, RES2, ROOT, GTLD, NS1)
CORP, CORE, INET = "corp", "core", "inet"

WIRING = {
    PC: [(0, CORP, "192.168.4.10/24")],
    SEL: [(0, CORP, "192.168.4.2/24")],
    R: [(0, CORP, "192.168.4.1/24"), (1, CORE, "192.168.250.1/24"),
        (2, INET, "198.41.0.1/24")],
    RES1: [(0, CORE, "192.168.250.3/24")],
    RES2: [(0, CORE, "192.168.230.27/24")],
    ROOT: [(0, INET, "198.41.0.4/24")],
    GTLD: [(0, INET, "192.5.6.30/24")],
    NS1: [(0, INET, "192.168.254.2/24")],
}
GATEWAY = {PC: "192.168.4.1", SEL: "192.168.4.1", RES1: "192.168.250.1",
           RES2: "192.168.230.1", ROOT: "198.41.0.1", GTLD: "192.5.6.1",
           NS1: "192.168.254.1"}

LAYOUT = {PC: (-480, -100), SEL: (-480, 100), CORP: (-260, 0), R: (0, 0),
          CORE: (0, -200), RES1: (-120, -330), RES2: (120, -330),
          INET: (260, 0), ROOT: (480, -140), GTLD: (480, 0), NS1: (480, 140)}
ETHS = {(PC, "eth0"): 90, (SEL, "eth0"): 90, (R, "eth0"): 270,
        (R, "eth1"): 0, (R, "eth2"): 90, (RES1, "eth0"): 180,
        (RES2, "eth0"): 180, (ROOT, "eth0"): 270, (GTLD, "eth0"): 270,
        (NS1, "eth0"): 270}

# 3.3.4 Selective Forwarding Resolver Configuration, dnssec-validation added.
SELECTIVE_CONF = """\
// selective forwarding named.conf file
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
  // added for the lab: its root has no DNSSEC key
  dnssec-validation no;

  // forwarding is not global but selective by zone in this configuration
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

// zone file for the root servers
// discretionary zone (see root server discussion above)
zone "." {
  type hint;
  file "named.root";
};

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
// zone file forwarded example.com
zone "example.com" {
  type forward;
  // this defines the addresses of the resolvers to
  // which queries for this zone will be forwarded
  forwarders {
    192.168.250.3;
    192.168.230.27;
  };
  // indicates all queries for this zone will be forwarded
  forward only;
};
"""

CARD = r"""
                     res1   res2
                        \   /
    pc ---+            core
          +-- corp ---- r ---- inet: root, gtld, ns1
    sel --+
    (forwards example.com, resolves the rest)

  One domain forwarded, the rest found from the root.
"""

def ask(name):
    return dig(PC, "@192.168.4.2 %s +short" % name).strip()

def main():
    opening()
    forwarding(R)
    ssh(R, "ip addr add 192.168.230.1/24 dev eth1")
    for addr in ("192.5.6.1/24", "192.168.254.1/24"):
        ssh(R, "ip addr add %s dev eth2" % addr)

    step("act 1: the lab internet, res1 and res2, then sel")
    internet_start(ROOT, GTLD, NS1)
    in_parallel(lambda n: named_start(n, RESOLVER_CONF, RESOLVER_FILES),
                [RES1, RES2], "named_start")
    named_start(SEL, SELECTIVE_CONF, RESOLVER_FILES)
    hold()

    step("act 2: example.com, forwarded into the core")
    got = count_on(R, 1, "udp port 53", lambda: ask("www.example.com"),
                   settle=1.0)
    ok_com = bool(got) and got["out"] == "192.168.254.7"
    ok_core = bool(got) and got["count"] >= 2
    proof("pc, www.example.com:", got["out"] if got else "")
    got = count_on(SEL, 0, "udp port 53 and host 198.41.0.4",
                   lambda: ask("mail.example.com"), settle=1.0)
    ok_noroot = bool(got) and got["count"] == 0 and \
        got["out"] == "192.168.254.4"
    proof("pc, mail.example.com, and the root never asked:",
          got["out"] if got else "")
    hold()

    step("act 3: example.net, resolved by sel from the root down")
    got = count_on(SEL, 0, "udp port 53 and host 198.41.0.4",
                   lambda: ask("ftp.example.net"), settle=1.0)
    ok_net = bool(got) and got["out"] == "192.168.254.21"
    ok_root = bool(got) and got["count"] >= 2
    proof("pc, ftp.example.net:", got["out"] if got else "")
    got = count_on(R, 1, "udp port 53", lambda: ask("mail.example.net"),
                   settle=1.0)
    ok_nocore = bool(got) and got["count"] == 0 and \
        got["out"] == "192.168.254.25"
    proof("pc, mail.example.net, and the core never used:",
          got["out"] if got else "")

    verdict([("www.example.com answered", ok_com),
             ("example.com crosses the core, to the resolvers", ok_core),
             ("example.com sends nothing to the root", ok_noroot),
             ("ftp.example.net answered", ok_net),
             ("example.net makes sel ask the root", ok_root),
             ("example.net sends nothing into the core", ok_nocore)])

    story("""
the network is left up. Things to try:

the cache of sel emptied:
cloonix cli %s ssh sel "rndc flush"

then a name of each kind, from pc:
cloonix cli %s ssh pc "dig @192.168.4.2 joe.example.com"
cloonix cli %s ssh pc "dig @192.168.4.2 ns2.example.net"
""" % (NET, NET, NET))

main()
