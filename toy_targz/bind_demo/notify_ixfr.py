#!/usr/bin/env python3
r"""notify_ixfr - NOTIFY, Incremental Zone Transfers (IXFR).

    ./notify_ixfr.py [net]     the net defaults to nemo

The examples of the BIND 9 Administrator Reference Manual (ARM), chapter
"Advanced Configurations", sections "NOTIFY" and "Incremental Zone
Transfers (IXFR)": a primary maintained by dynamic update tells its
secondaries the zone has changed, and they copy only the change.
https://bind9.readthedocs.io/en/stable/chapter6.html#notify

    ns2 (192.168.254.3) ---+
                           +-- lan254 -- ns1 (192.168.254.2)
    ns3 (192.168.254.5) ---+             primary, update-policy local
       secondaries

  The base zone example.com, its second name server inside the zone and a
  third one added: the NS records name ns2 and ns3, the secondaries, so
  the NOTIFY of ns1 reaches them.

  ACT 1  ns1 starts, then ns2 and ns3: each copies the whole zone, AXFR.
  ACT 2  one record added on ns1 by nsupdate -l: ns1 sends a NOTIFY to ns2
         and ns3, which answer it, ask the serial and copy the change; both
         serve the new record within seconds, nobody touched them.
  ACT 3  that copy was an IXFR: the journal of the update, a handful of
         records, where the AXFR carried the whole zone.

Image: dns_trixie (bind9)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf ns1 eth0 sniff=on
    cloonix cli nemo sniff ns1 eth0 --dump "udp port 53"

  The NOTIFY of ACT 2, opcode 4 in the DNS header, from 192.168.254.2 to
  each secondary, then the SOA question each one asks back.

Important
  The base zone of the ARM names ns2.example.net, outside the zone: its
  NOTIFY would wait for that name to resolve. Here the secondaries are
  ns2 and ns3 of example.com, with their addresses in the zone. As the ARM
  notes, a secondary sends NOTIFY too, for every zone it loads: ns2 and
  ns3 notify each other and each refuses it, "refused notify from
  non-primary" in their logs; notify primary-only; in their zone
  statement would keep them silent. The three run the base named.conf of
  the ARM, recursion no added.
"""
import os
import re
import sys
import time

# What every demo shares is in demos/lib/demolib.py, toylib.py and bindlib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, hold, in_parallel, ssh, step, story
from bindlib import (
    EXAMPLE_COM, base_conf, dig, log_of, named_start, opening, proof, rndc,
    until, verdict)

HISTORY = """\
In 1996 two RFCs put an end to secondaries polling their primary for
hours: NOTIFY, RFC 1996 by Paul Vixie, a message from the primary saying
the zone has changed, and IXFR, RFC 1995 by Masataka Ohta, which copies
only the differences instead of the whole zone. Together they carry a
change to every secondary of a zone in a second.

BIND sends and answers both since the 1990s; written at Berkeley in 1984
and maintained by Paul Vixie from 1988, it was rewritten as BIND 9 in 2000
by the Internet Systems Consortium.
"""

ROOTFS_CHOICES = ("dns_trixie",)
TOOL = "named"

NS1, NS2, NS3 = "ns1", "ns2", "ns3"
SECONDARIES = {NS2: "192.168.254.3", NS3: "192.168.254.5"}
ALL = (NS1, NS2, NS3)
L254 = "lan254"

WIRING = {
    NS1: [(0, L254, "192.168.254.2/24")],
    NS2: [(0, L254, "192.168.254.3/24")],
    NS3: [(0, L254, "192.168.254.5/24")],
}
GATEWAY = {}

LAYOUT = {NS2: (-320, -110), NS3: (-320, 110), L254: (0, 0), NS1: (320, 0)}
ETHS = {(NS2, "eth0"): 90, (NS3, "eth0"): 90, (NS1, "eth0"): 270}

# The base zone, its name servers ns1, ns2 and ns3, all inside the zone.
ZONE = EXAMPLE_COM.replace(
    "; the second name server is external to this zone (domain)\n"
    "           IN      NS      ns2.example.net.\n",
    "; the secondaries, inside the zone: the NOTIFY goes to them\n"
    "           IN      NS      ns2.example.com.\n"
    "           IN      NS      ns3.example.com.\n").replace(
    "ns1        IN      A       192.168.254.2\n",
    "ns1        IN      A       192.168.254.2\n"
    "ns2        IN      A       192.168.254.3\n"
    "ns3        IN      A       192.168.254.5\n")

PRIMARY = base_conf("  recursion no;\n") + """
zone "example.com" {
  type primary;
  file "example.com";
  // a dynamic zone: its journal gives the differences IXFR sends
  update-policy local;
  allow-transfer { 192.168.254.3; 192.168.254.5; };
};
"""
SECONDARY = base_conf("  recursion no;\n") + """
zone "example.com" {
  type secondary;
  file "example.com.saved";
  primaries { 192.168.254.2; };
};
"""

CARD = r"""
    ns2 --+
          +---- ns1 (primary, dynamic)
    ns3 --+
    (secondaries)

  A change announced by NOTIFY, carried by IXFR, in a second.
"""

def newhost(node):
    return dig(node, "@127.0.0.1 newhost.example.com +short").strip()

def lines(node, word):
    return "\n".join(l for l in log_of(node).split("\n") if word in l)

def records(text):
    """The records counted in the last "Transfer completed" line."""
    found = re.findall(r"Transfer completed: \d+ messages, (\d+) records",
                       text)
    return int(found[-1]) if found else 0

def main():
    opening()

    step("act 1: ns1 starts, ns2 and ns3 copy the whole zone")
    named_start(NS1, PRIMARY, [("/var/example.com", ZONE)])
    in_parallel(lambda n: named_start(n, SECONDARY), list(SECONDARIES),
                "named_start")
    ok_axfr = all(until(lambda n=n: "Transfer completed" in log_of(n),
                        30, 0.5) for n in SECONDARIES)
    axfr = dict((n, records(log_of(n))) for n in SECONDARIES)
    proof("ns1, the zone sent whole to each secondary:", lines(NS1, "AXFR"))
    hold()

    step("act 2: one record added on ns1, NOTIFY, and the secondaries follow")
    start = time.time()
    got = count_on(NS1, 0, "udp port 53", lambda: (
        ssh(NS1, "printf 'update add newhost.example.com 300 IN A "
                 "192.168.254.20\\nsend\\n' | nsupdate -l"),
        all(until(lambda n=n: newhost(n) == "192.168.254.20", 20, 0.3)
            for n in SECONDARIES))[1], settle=1.0)
    took = time.time() - start
    ok_follow = bool(got) and got["out"]
    ok_wire = bool(got) and got["count"] >= 4
    ok_sent = "sending notifies (serial 2003080801)" in log_of(NS1)
    ok_heard = all("notify from 192.168.254.2" in log_of(n)
                   for n in SECONDARIES)
    log1 = log_of(NS1)
    after = log1[log1.rfind("\n", 0, log1.find(
        "sending notifies (serial 2003080801)")) + 1:]
    proof("ns1, after the update:", "\n".join(
        l for l in after.split("\n") if "notif" in l))
    for n in sorted(SECONDARIES):
        proof("%s:" % n, "%s\nnewhost.example.com %s"
              % (lines(n, "notify from 192.168.254.2"), newhost(n)))
    proof("the change on both secondaries, after:", "%.1f s" % took)
    hold()

    step("act 3: the copy was an IXFR, the change only")
    ixfr = dict((n, records(log_of(n))) for n in SECONDARIES)
    ok_ixfr = "IXFR" in lines(NS1, "transfer of 'example.com/IN'")
    ok_small = all(0 < ixfr[n] < axfr[n] for n in SECONDARIES)
    proof("ns1, the transfers it sent:",
          lines(NS1, "transfer of 'example.com/IN'"))
    proof("records carried, the AXFR of act 1 and the IXFR of act 2:",
          "\n".join("%s  AXFR %2d   IXFR %2d" % (n, axfr[n], ixfr[n])
                    for n in sorted(SECONDARIES)))
    proof("ns2, the serial of its copy:", "\n".join(
        l for l in rndc(NS2, "zonestatus example.com").split("\n")
        if l.startswith("serial")))

    verdict([("ns2 and ns3 copy the zone at their start", ok_axfr),
             ("ns1 sends NOTIFY for the serial 2003080801", ok_sent),
             ("ns2 and ns3 receive the NOTIFY of ns1", ok_heard),
             ("the NOTIFY and what follows cross eth0 of ns1", ok_wire),
             ("both secondaries serve the new record", ok_follow),
             ("ns1 sends the change as an IXFR", ok_ixfr),
             ("the IXFR carries fewer records than the AXFR", ok_small)])

    story("""
the network is left up. Things to try:

one more record, and its arrival on ns3:
cloonix cli %s ssh ns1 "printf 'update add other.example.com 300 IN A 192.168.254.21\\nsend\\n' | nsupdate -l"
cloonix cli %s ssh ns3 "dig @127.0.0.1 other.example.com +short"

the transfers in the log of the primary:
cloonix cli %s ssh ns1 "grep XFR /var/log/named/example.log"
""" % (NET, NET, NET))

main()
