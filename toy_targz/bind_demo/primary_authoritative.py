#!/usr/bin/env python3
r"""primary_authoritative - Primary Authoritative Name Server.

    ./primary_authoritative.py [net]     the net defaults to nemo

The example of the BIND 9 Administrator Reference Manual (ARM), chapter
"Configurations and Zone Files", section "Primary Authoritative Name
Server": the authoritative primary named.conf and the example.com base zone
file, as the ARM prints them (PRIMARY_CONF and EXAMPLE_COM in bindlib.py,
the secondary demo uses them too).
https://bind9.readthedocs.io/en/stable/chapter3.html#primary-authoritative-name-server

    pc (192.168.4.20) ---- lan4 ---- r ---- lan254 ---- ns1 (192.168.254.2)
                                  (router)              named, primary
                                                        for example.com

  ns1 holds example.com and answers for it with authority, to anyone
  (allow-query any). It answers nothing else: no recursion, no cache. Two
  addresses only may copy the whole zone, the secondaries of the ARM,
  192.168.4.14 and 192.168.5.53.

  ACT 1  named starts on ns1 with the named.conf of the ARM: its log file
         says example.com is loaded with the serial of the zone file.
  ACT 2  pc asks ns1 for www.example.com and the mail servers of the zone:
         the answers carry the flag aa, Authoritative Answer.
  ACT 3  what this primary refuses: a name it does not hold (no
         recursion), its version (hidden by the version statement), the
         zone to an address the ARM does not list. pc takes 192.168.4.14
         beside its own address: from there, the zone transfer works.

Image: dns_trixie (bind9)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf ns1 eth0 sniff=on
    cloonix cli nemo sniff ns1 eth0 --dump "udp port 53"

  The answer of ns1 for www.example.com: in its header, the bit aa says
  the server holds the zone, which no resolver answer ever carries.

Important
  The ARM writes its files relative to directory "/var": the zone file is
  /var/example.com, the log /var/log/named/example.log. named is started
  without -u: it stays uid 0 of the container.
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
    PRIMARY_CONF, PRIMARY_FILES, answer_of, dig, flags_of, forwarding, log_of,
    named_start, opening, proof, status_of, verdict)

HISTORY = """\
Until 1983 every host of the ARPANET fetched one file, HOSTS.TXT, from the
SRI. Paul Mockapetris designed the Domain Name System that year, RFC 882
and 883, rewritten as RFC 1034 and 1035 in 1987: a tree of names cut into
zones, each zone held by authoritative servers, a primary that reads it
from a file and secondaries that copy it.

BIND, the Berkeley Internet Name Domain, was written at the University of
California, Berkeley, in 1984 by Douglas Terry, Mark Painter, David Riggle
and Songnian Zhou, maintained by Paul Vixie from 1988, and rewritten as
BIND 9 in 2000 by the Internet Systems Consortium, which publishes it.
"""

ROOTFS_CHOICES = ("dns_trixie",)
TOOL = "named"

PC, R, NS1 = "pc", "r", "ns1"
ALL = (NS1, PC, R)
L4, L254 = "lan4", "lan254"

WIRING = {
    PC: [(0, L4, "192.168.4.20/24")],
    R: [(0, L254, "192.168.254.1/24"), (1, L4, "192.168.4.1/24")],
    NS1: [(0, L254, "192.168.254.2/24")],
}
GATEWAY = {PC: "192.168.4.1", NS1: "192.168.254.1"}

LAYOUT = {PC: (-440, 0), L4: (-220, 0), R: (0, 0), L254: (220, 0),
          NS1: (440, 0)}
ETHS = {(PC, "eth0"): 90, (R, "eth0"): 90, (R, "eth1"): 270,
        (NS1, "eth0"): 270}

CARD = r"""
    pc ---- r ---- ns1 (named, primary for example.com)

  The zone answered with authority, and everything else refused.
"""

def main():
    opening()
    forwarding(R)

    step("act 1: named starts on ns1 with the primary named.conf of the ARM")
    named_start(NS1, PRIMARY_CONF, PRIMARY_FILES)
    loaded = [l for l in log_of(NS1).split("\n") if "loaded serial" in l]
    ok_loaded = any("zone example.com/IN: loaded serial 2003080800" in l
                    for l in loaded)
    proof("ns1, the zones in /var/log/named/example.log:", "\n".join(loaded))
    hold()

    step("act 2: pc asks ns1, the answers carry the flag aa")
    got = count_on(NS1, 0, "udp port 53",
                   lambda: dig(PC, "@192.168.254.2 www.example.com"),
                   settle=1.0)
    out = got["out"] if got else dig(PC, "@192.168.254.2 www.example.com")
    ok_www = "192.168.254.7" in answer_of(out) and "aa" in flags_of(out)
    ok_seen = bool(got) and got["count"] >= 2
    proof("pc, dig @192.168.254.2 www.example.com:", out)
    mx = dig(PC, "@192.168.254.2 example.com MX +noall +answer")
    ok_mx = "10 mail.example.com." in mx and "20 mail.example.net." in mx \
        and "1814400" in mx
    proof("pc, the mail servers, the first with its TTL of 3 weeks:", mx)
    hold()

    step("act 3: no recursion, no version, no zone for a stranger")
    rec = dig(PC, "@192.168.254.2 www.isc.org")
    ok_norec = status_of(rec) == "REFUSED" and "ra" not in flags_of(rec)
    proof("pc, a name ns1 does not hold:",
          "\n".join(l for l in rec.split("\n") if "status:" in l or
                    "flags:" in l))
    ver = dig(PC, "@192.168.254.2 version.bind chaos txt +short")
    ok_version = "not currently available" in ver
    proof("pc, the version of ns1:", ver)
    ssh(PC, "ip addr add 192.168.4.14/24 dev eth0")
    good = dig(PC, "-b 192.168.4.14 @192.168.254.2 example.com AXFR")
    ok_axfr = "XFR size" in good and "www.example.com." in good
    proof("pc as 192.168.4.14, a secondary of the ARM, the whole zone:", good)
    bad = dig(PC, "-b 192.168.4.20 @192.168.254.2 example.com AXFR")
    ok_denied = "Transfer failed" in bad
    proof("pc as 192.168.4.20, the same transfer:", bad)
    denied = "\n".join(l for l in log_of(NS1).split("\n") if "denied" in l)
    ok_logged = "example.com/AXFR/IN' denied" in denied
    proof("ns1, the refusal in its log:", denied)

    verdict([("example.com loaded with the serial 2003080800", ok_loaded),
             ("www.example.com is 192.168.254.7, flag aa", ok_www),
             ("the query and its answer cross eth0 of ns1", ok_seen),
             ("the two MX of the zone, 3 weeks for the first", ok_mx),
             ("a name outside the zone is REFUSED, no recursion", ok_norec),
             ("the version is hidden", ok_version),
             ("192.168.4.14 transfers the zone", ok_axfr),
             ("192.168.4.20 is refused the transfer", ok_denied),
             ("ns1 logs the transfer it denied", ok_logged)])

    story("""
the network is left up. Things to try:

the whole zone, from the address the ARM allows:
cloonix cli %s ssh pc "dig -b 192.168.4.14 @192.168.254.2 example.com AXFR"

the log of ns1 as it comes:
cloonix cli %s ssh ns1 "tail -f /var/log/named/example.log"
""" % (NET, NET))

main()
