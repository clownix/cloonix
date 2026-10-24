#!/usr/bin/env python3
r"""catalog_zones - Catalog Zones.

    ./catalog_zones.py [net]     the net defaults to nemo

The example of the BIND 9 Administrator Reference Manual (ARM), chapter
"Advanced Configurations", section "Catalog Zones": the catalog-zones
statement of the ARM, its catalog.example of version 2 with its two
members, domain.example and domain2.example, and a secondary that serves
every zone the catalog lists, without a line about them in its named.conf.
https://bind9.readthedocs.io/en/stable/chapter6.html#catalog-zones

    pc   (10.53.0.10) ---+
    cat1 (10.53.0.1) ----+-- lan53
    cat2 (10.53.0.2) ----+
      cat1: primary of the catalog and of its members (default-primaries)
      cat2: secondary of the catalog, the consumer

  ACT 1  cat1 serves catalog.example and the member zones; cat2 starts with
         a named.conf that names catalog.example only.
  ACT 2  cat2 creates domain.example and domain2.example itself, as
         secondaries of 10.53.0.1, transfers them and answers for them.
  ACT 3  one PTR record added to the catalog on cat1 by nsupdate -l: cat2
         adds domain3.example, transfers it and serves it.
  ACT 4  one PTR record removed: cat2 stops serving domain2.example.

Image: dns_trixie (bind9)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf cat2 eth0 sniff=on
    cloonix cli nemo sniff cat2 eth0 --dump "tcp port 53"

  The transfers of ACT 3: the catalog first, then the member zone it now
  names, which cat2 asks for by itself.

Important
  The member zones are declared in the named.conf of cat1, the first of
  the two ways the ARM gives (the other is rndc addzone). The catalog is
  changed by nsupdate -l, which needs update-policy local in its zone
  statement, and its NOTIFY goes to cat2 by also-notify: the NS of a
  catalog, invalid., names no server. cat1 and cat2 are given recursion
  no, allow-transfer and the zones beside the base named.conf of the ARM.
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
    base_conf, dig, flags_of, log_of, named_start, opening, proof, rndc,
    until, verdict)

HISTORY = """\
A secondary used to learn every zone it serves from its named.conf, edited
on every machine for every new zone. The catalog zone, a zone whose
records list zones, came into BIND 9.11 in 2016: the list travels by zone
transfer, and the secondaries add and drop zones as it changes. It was
standardised in 2023 as RFC 9432, written by people of PowerDNS, NLnet
Labs, CZ.NIC, ISC and others.

BIND, written at Berkeley in 1984, maintained by Paul Vixie from 1988, was
rewritten as BIND 9 in 2000 by the Internet Systems Consortium.
"""

ROOTFS_CHOICES = ("dns_trixie",)
TOOL = "named"

PC, CAT1, CAT2 = "pc", "cat1", "cat2"
ALL = (CAT1, CAT2, PC)
L53 = "lan53"

WIRING = {
    PC: [(0, L53, "10.53.0.10/24")],
    CAT1: [(0, L53, "10.53.0.1/24")],
    CAT2: [(0, L53, "10.53.0.2/24")],
}
GATEWAY = {}

LAYOUT = {PC: (-320, -110), CAT2: (-320, 110), L53: (0, 0), CAT1: (320, 0)}
ETHS = {(PC, "eth0"): 90, (CAT2, "eth0"): 90, (CAT1, "eth0"): 270}

# The base named.conf, the catalog and its members, the primary side.
CAT1_CONF = base_conf("""\
  // added: authoritative only, the zones transferred to the consumer
  recursion no;
  allow-transfer { 10.53.0.2; };
""") + """
zone "catalog.example" {
  type primary;
  file "catalog.example.db";
  update-policy local;
  also-notify { 10.53.0.2; };
};
zone "domain.example" { type primary; file "domain.example.db"; };
zone "domain2.example" { type primary; file "domain2.example.db"; };
zone "domain3.example" { type primary; file "domain3.example.db"; };
"""

# The base named.conf, the catalog-zones statement of the ARM in its
# options, the catalog as a secondary zone: the consumer.
CAT2_CONF = base_conf("""\
  recursion no;
  catalog-zones {
      zone "catalog.example"
           default-primaries { 10.53.0.1; }
           in-memory no
           zone-directory "catzones"
           min-update-interval 10;
  };
""") + """
zone "catalog.example" {
  type secondary;
  file "catalog.example.db";
  primaries { 10.53.0.1; };
};
"""

# 6.8.3 Catalog Zone Format, $TTL added.
CATALOG = """\
$TTL 3600
catalog.example.    IN SOA . . 2016022901 900 600 86400 1
catalog.example.    IN NS invalid.
version.catalog.example.    IN TXT "2"
5960775ba382e7a4e09263fc06e7c00569b6a05c.zones.catalog.example. IN PTR domain.example.
uniquelabel.zones.catalog.example. IN PTR domain2.example.
"""

def member(name, last):
    return """\
$TTL 3600
@    IN SOA ns.%s. hostmaster.%s. 1 900 600 86400 60
@    IN NS  ns.%s.
ns   IN A   10.53.0.1
www  IN A   10.53.0.%d
""" % (name, name, name, last)

FILES = [("/var/catalog.example.db", CATALOG),
         ("/var/domain.example.db", member("domain.example", 101)),
         ("/var/domain2.example.db", member("domain2.example", 102)),
         ("/var/domain3.example.db", member("domain3.example", 103))]

CARD = r"""
    pc ----+
           +---- cat1 (the catalog, its members)
    cat2 --+
    (consumer: serves what the catalog lists)

  Zones added and removed by the records of a zone.
"""

def www(zone):
    out = dig(PC, "@10.53.0.2 www.%s" % zone)
    addr = [l.split()[-1] for l in out.split("\n")
            if l.startswith("www.%s." % zone)]
    return (addr[0] if addr else "") if "aa" in flags_of(out) else ""

def serves(zone):
    return "type: secondary" in rndc(CAT2, "zonestatus %s" % zone)

def main():
    opening()

    step("act 1: cat1 serves the catalog, cat2 names nothing but it")
    named_start(CAT1, CAT1_CONF, FILES)
    ssh(CAT2, "mkdir -p /var/catzones")
    named_start(CAT2, CAT2_CONF)
    zones = ssh(CAT2, "grep -c '^zone' /etc/bind/named.conf", check=False)
    proof("cat2, the zone statements of its named.conf:", zones.strip())
    hold()

    step("act 2: cat2 creates the two members and answers for them")
    ok_members = until(lambda: www("domain.example") == "10.53.0.101" and
                       www("domain2.example") == "10.53.0.102", 40, 1)
    proof("pc, asking cat2:", "www.domain.example  %s\nwww.domain2.example %s"
          % (www("domain.example"), www("domain2.example")))
    proof("cat2, what the catalog made it do:", "\n".join(
        l for l in log_of(CAT2).split("\n") if "catz" in l))
    proof("cat2, the copies in /var/catzones:",
          ssh(CAT2, "ls /var/catzones", check=False))
    hold()

    step("act 3: domain3.example added to the catalog")
    got = count_on(CAT2, 0, "tcp port 53", lambda: (
        ssh(CAT1, "printf 'update add label3.zones.catalog.example 3600 IN "
                  "PTR domain3.example.\\nsend\\n' | nsupdate -l"),
        until(lambda: www("domain3.example") == "10.53.0.103", 60, 1))[1],
        settle=1.0)
    ok_added = bool(got) and got["out"]
    ok_xfr = bool(got) and got["count"] > 0
    proof("pc, www.domain3.example from cat2:", www("domain3.example"))
    proof("cat2:", "\n".join(l for l in log_of(CAT2).split("\n")
                             if "domain3" in l))
    hold()

    step("act 4: domain2.example removed from the catalog")
    ssh(CAT1, "printf 'update delete uniquelabel.zones.catalog.example "
              "PTR\\nsend\\n' | nsupdate -l")
    ok_removed = until(lambda: not serves("domain2.example"), 60, 1) and \
        serves("domain.example")
    proof("cat2, rndc zonestatus domain2.example:",
          rndc(CAT2, "zonestatus domain2.example"))

    verdict([("cat2 serves domain.example and domain2.example", ok_members),
             ("a PTR added: cat2 serves domain3.example", ok_added),
             ("cat2 transfers it over TCP by itself", ok_xfr),
             ("a PTR removed: cat2 drops domain2.example", ok_removed)])

    story("""
the network is left up. Things to try:

a member as the catalog of cat2 names it:
cloonix cli %s ssh pc "dig @10.53.0.2 label3.zones.catalog.example PTR +short"

domain2.example back in the catalog under a new label:
cloonix cli %s ssh cat1 "printf 'update add again.zones.catalog.example 3600 IN PTR domain2.example.\\nsend\\n' | nsupdate -l"
""" % (NET, NET))

main()
