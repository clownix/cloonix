#!/usr/bin/env python3
r"""dnssec - DNSSEC: signing, secure delegation, validation.

    ./dnssec.py [net]     the net defaults to nemo

The examples of the BIND 9 Administrator Reference Manual (ARM), chapter
"DNSSEC", sections "Fully Automated (Key and Signing Policy)", "Secure
Delegation" and "DNSSEC Validation": the zone dnssec.example of the ARM
signed by dnssec-policy default, its DS put in the parent zone example.,
and a resolver that validates from a trust anchor for example.
https://bind9.readthedocs.io/en/stable/chapter5.html

    pc     (10.0.0.10) ---+
    res    (10.0.0.2) ----+-- lan0 --+-- parent (10.0.0.3)  the root, and
       validating resolver           |                      example. signed
                                     +-- child  (10.0.0.4)  dnssec.example
                                                            signed, and
                                                            insecure.example

  ACT 1  child signs dnssec.example by itself: a key made, DNSKEY and RRSIG
         records in the zone; parent signs example. the same way.
  ACT 2  the DS of the key of dnssec.example, made by dnssec-dsfromkey, is
         put in the zone example.: the delegation is secure.
  ACT 3  res, its trust anchor the DS of example., validates: the answer
         for www.dnssec.example carries the flag ad; www.insecure.example,
         delegated without DS, answers without it, accepted all the same.
  ACT 4  the parent publishes a DS that matches no key: res answers
         SERVFAIL, the data still there with the checks disabled (+cd);
         the right DS back, the flag ad is back.

Image: dns_trixie (bind9)
Size:  4 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf res eth0 sniff=on
    cloonix cli nemo sniff res eth0 --dump "udp port 53 and host 10.0.0.3"

  The questions of res for the DNSKEY of example. and the DS of
  dnssec.example: the links of the chain of trust, fetched one by one.

Important
  The parent plays the root too: the resolver needs a root to start from,
  the trust anchor is for example. (the ARM's parent zone), not for the
  root, and res is given dnssec-validation yes, which uses no other
  anchor. The servers run the base named.conf of the ARM with recursion
  no; res logs to /var/log/named/example.log as they do.
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
    answer_of, base_conf, brief, dig, flags_of, log_of, named_start,
    opening, proof, put_any, rndc, status_of, until, verdict)

HISTORY = """\
The DNS answered anyone who asked, and anyone could answer in its place:
in 2008 Dan Kaminsky showed how to poison the cache of a resolver in
seconds. DNSSEC, RFC 4033 to 4035 in 2005, signs the records, and a
resolver checks the signatures from a key it trusts down to the answer,
each zone vouching for the key of its children with a DS record. The
root zone was signed on 15 July 2010.

dnssec-policy, BIND signing and rolling its keys by itself, came with
BIND 9.16 in 2020. BIND, written at Berkeley in 1984 and maintained by
Paul Vixie from 1988, was rewritten as BIND 9 in 2000 by the Internet
Systems Consortium.
"""

ROOTFS_CHOICES = ("dns_trixie",)
TOOL = "named"

PC, RES, PARENT, CHILD = "pc", "res", "parent", "child"
ALL = (PARENT, CHILD, RES, PC)
L0 = "lan0"

WIRING = {
    PC: [(0, L0, "10.0.0.10/24")],
    RES: [(0, L0, "10.0.0.2/24")],
    PARENT: [(0, L0, "10.0.0.3/24")],
    CHILD: [(0, L0, "10.0.0.4/24")],
}
GATEWAY = {}

LAYOUT = {PC: (-320, -110), RES: (-320, 110), L0: (0, 0),
          PARENT: (320, -110), CHILD: (320, 110)}
ETHS = {(PC, "eth0"): 90, (RES, "eth0"): 90, (PARENT, "eth0"): 270,
        (CHILD, "eth0"): 270}

AUTH = base_conf("  recursion no;\n")

# 5.1.2 Fully Automated (Key and Signing Policy): the zone of the ARM.
CHILD_CONF = AUTH + """
zone "dnssec.example" {
    type primary;
    file "dnssec.example.db";
    dnssec-policy default;
};
zone "insecure.example" { type primary; file "insecure.example.db"; };
"""
PARENT_CONF = AUTH + """
zone "." { type primary; file "root.db"; };
zone "example" {
    type primary;
    file "example.db";
    dnssec-policy default;
};
"""

HEAD = ("$TTL 300\n@ IN SOA %s hostmaster.example. %d 1800 900 604800 60\n"
        "@ IN NS %s\n")
ROOT = HEAD % ("a.root-servers.net.", 1, "a.root-servers.net.") + """\
a.root-servers.net. IN A 10.0.0.3
example. IN NS ns.example.
ns.example. IN A 10.0.0.3
"""
def example(serial, ds=""):
    return HEAD % ("ns.example.", serial, "ns.example.") + """\
ns IN A 10.0.0.3
dnssec IN NS ns.dnssec.example.
ns.dnssec IN A 10.0.0.4
insecure IN NS ns.insecure.example.
ns.insecure IN A 10.0.0.4
""" + ds
DNSSEC_EXAMPLE = HEAD % ("ns.dnssec.example.", 1, "ns.dnssec.example.") + \
    "ns IN A 10.0.0.4\nwww IN A 10.0.0.80\n"
INSECURE_EXAMPLE = HEAD % ("ns.insecure.example.", 1,
                           "ns.insecure.example.") + \
    "ns IN A 10.0.0.4\nwww IN A 10.0.0.81\n"

HINTS = """\
.                        3600000      NS    A.ROOT-SERVERS.NET.
A.ROOT-SERVERS.NET.      3600000      A     10.0.0.3
"""

# 5.3 DNSSEC Validation: a trust anchor of the trust-anchors statement.
RES_CONF = """\
options {
  directory "/var/cache/bind";
  dnssec-validation yes;
};
logging {
  channel example_log {
    file "/var/log/named/example.log" versions 3 size 250k;
    severity info;
  };
  category default { example_log; };
};
zone "." { type hint; file "named.root"; };
trust-anchors {
  example. static-ds %s;
};
"""

CARD = r"""
    pc ---+          +--- parent (the root, example. signed)
          +-- lan0 --+
    res --+          +--- child  (dnssec.example signed)
    (validating resolver)

  A zone signed, its key vouched for by its parent, an answer checked.
"""

def ds_of(node, zone):
    """The DS of the key dnssec-policy made for zone, in the key directory
    /var, as dnssec-dsfromkey prints it."""
    return ssh(node, "dnssec-dsfromkey -2 /var/K%s.+013+*.key" % zone,
               check=False).strip()

def via_res(args):
    return dig(PC, "@10.0.0.2 %s" % args)

def main():
    opening()

    step("act 1: dnssec.example and example. signed by dnssec-policy default")
    named_start(CHILD, CHILD_CONF, [("/var/dnssec.example.db", DNSSEC_EXAMPLE),
                                    ("/var/insecure.example.db",
                                     INSECURE_EXAMPLE)])
    named_start(PARENT, PARENT_CONF, [("/var/root.db", ROOT),
                                      ("/var/example.db", example(1))])
    ok_keys = until(lambda: ds_of(CHILD, "dnssec.example") and
                    ds_of(PARENT, "example"), 30, 0.5)
    keys = dig(PC, "@10.0.0.4 dnssec.example DNSKEY +dnssec +noall +answer")
    ok_signed = "DNSKEY\t257 3 13" in keys and "RRSIG\tDNSKEY 13" in keys
    proof("child, the key files dnssec-policy made in /var:",
          ssh(CHILD, "ls /var/Kdnssec.example.*", check=False))
    proof("pc, the DNSKEY of dnssec.example and its signature:", keys)
    proof("child, rndc dnssec -status dnssec.example:",
          rndc(CHILD, "dnssec -status dnssec.example"))
    hold()

    step("act 2: the DS of dnssec.example into the zone example.")
    ds = ds_of(CHILD, "dnssec.example")
    proof("child, dnssec-dsfromkey -2:", ds)
    put_any(PARENT, "/var/example.db", example(2, ds + "\n"))
    rndc(PARENT, "reload example")
    tag = ds.split()[3] if len(ds.split()) > 3 else "?"
    ok_ds = until(lambda: tag in dig(
        PC, "@10.0.0.3 dnssec.example DS +short"), 30, 0.5)
    signed_ds = dig(PC, "@10.0.0.3 dnssec.example DS +dnssec +noall +answer")
    ok_ds = ok_ds and "RRSIG\tDS 13" in signed_ds
    proof("pc, the DS in example., signed by example.:", signed_ds)
    hold()

    step("act 3: res validates from its trust anchor for example.")
    anchor = ds_of(PARENT, "example").split()
    ta = " ".join(anchor[3:6]) + ' "%s"' % anchor[6] if len(anchor) > 6 \
        else ""
    named_start(RES, RES_CONF % ta, [("/var/cache/bind/named.root", HINTS)])
    proof("res, its trust anchor, the DS of the key of example.:",
          "example. static-ds %s;" % ta)
    got = count_on(RES, 0, "udp port 53 and host 10.0.0.3",
                   lambda: via_res("www.dnssec.example"), settle=1.0)
    good = got["out"] if got else via_res("www.dnssec.example")
    ok_ad = "10.0.0.80" in answer_of(good) and "ad" in flags_of(good)
    ok_chain = bool(got) and got["count"] >= 4
    proof("pc, www.dnssec.example through res, the flag ad:", brief(good))
    insecure = via_res("www.insecure.example")
    ok_insecure = "10.0.0.81" in answer_of(insecure) and \
        "ad" not in flags_of(insecure)
    proof("pc, www.insecure.example, no DS, no ad, accepted:", brief(insecure))
    hold()

    step("act 4: a DS that matches no key, then the right one back")
    digest = ds.split()[-1]
    wrong = ds[:-len(digest)] + ("0" if digest[0] != "0" else "1") + \
        digest[1:]
    put_any(PARENT, "/var/example.db", example(3, wrong + "\n"))
    rndc(PARENT, "reload example")
    until(lambda: wrong.split()[-1] in dig(
        PC, "@10.0.0.3 dnssec.example DS +short").replace(" ", "").upper(),
        30, 0.5)
    rndc(RES, "flush")
    bad = via_res("www.dnssec.example")
    ok_fail = status_of(bad) == "SERVFAIL"
    unchecked = via_res("www.dnssec.example +cd")
    ok_cd = "10.0.0.80" in answer_of(unchecked)
    proof("pc, with the wrong DS:", brief(bad))
    proof("pc, the same with +cd, checking disabled:", brief(unchecked))
    proof("res, why it refused:", "\n".join(
        l for l in log_of(RES).split("\n")
        if "dnssec.example" in l and ("no valid" in l or
                                      "broken trust" in l))[-600:])
    put_any(PARENT, "/var/example.db", example(4, ds + "\n"))
    rndc(PARENT, "reload example")
    until(lambda: digest in dig(PC, "@10.0.0.3 dnssec.example DS +short")
          .replace(" ", "").upper(), 30, 0.5)
    rndc(RES, "flush")
    again = via_res("www.dnssec.example")
    ok_back = "ad" in flags_of(again) and "10.0.0.80" in answer_of(again)
    proof("pc, the right DS back:", brief(again))

    verdict([("dnssec-policy makes a key and signs dnssec.example",
              ok_keys and ok_signed),
             ("the DS of dnssec.example is in example., signed", ok_ds),
             ("res validates www.dnssec.example: flag ad", ok_ad),
             ("res fetches the chain of trust from the parent", ok_chain),
             ("an unsigned zone, delegated without DS, still answers",
              ok_insecure),
             ("a DS that matches no key: SERVFAIL", ok_fail),
             ("the data is there with checking disabled", ok_cd),
             ("the right DS back: the flag ad again", ok_back)])

    story("""
the network is left up. Things to try:

the signatures of an answer:
cloonix cli %s ssh pc "dig @10.0.0.2 www.dnssec.example +dnssec"

the keys and their state, as dnssec-policy keeps them:
cloonix cli %s ssh child "rndc dnssec -status dnssec.example"
""" % (NET, NET))

main()
