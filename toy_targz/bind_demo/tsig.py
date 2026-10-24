#!/usr/bin/env python3
r"""tsig - TSIG: a shared key between two servers.

    ./tsig.py [net]     the net defaults to nemo

The example of the BIND 9 Administrator Reference Manual (ARM), chapter
"Security Configurations", section "TSIG": the key host1-host2. made by
tsig-keygen and copied to both hosts, the zone transfer signed with it, the
TSIG-based access control of the ARM for transfers and updates, and the
errors of a wrong key.
https://bind9.readthedocs.io/en/stable/chapter7.html#tsig

    host2 (10.1.2.3) ---+
                        +-- lan2 -- host1 (10.1.2.2)
    other (10.1.2.99) --+           primary of example.com
      host2: secondary, holds the key
      other: no key

  ACT 1  tsig-keygen host1-host2. on host1, the key copied to host2.
  ACT 2  host1 allows the transfer of example.com to the key only; host2,
         told to use the key with its primary, copies the zone.
  ACT 3  other, without the key, is refused the same transfer; host2, with
         dig -k and the key, gets it.
  ACT 4  the allow-update of the ARM: an update from the lan signed with
         the key is accepted, the same unsigned is refused.
  ACT 5  the errors of the ARM: a key of the right name with a wrong secret,
         BADSIG; a key host1 does not know, BADKEY; both answered NOTAUTH.

Image: dns_trixie (bind9)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf host1 eth0 sniff=on
    cloonix cli nemo sniff host1 eth0 --dump "tcp port 53 and host 10.1.2.3"

  The transfer of ACT 2: the question carries a TSIG record at its end,
  the name of the key and a hash of the message, and so does each answer.

Important
  The secret travels from host1 to host2 inside the demo, as the ARM says
  it must travel: outside the DNS. The two hosts run the base named.conf
  of the ARM with recursion no, the key included from its own file.
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
    EXAMPLE_COM, base_conf, dig, log_of, named_start, opening, proof,
    put_any, status_of, until, verdict)

HISTORY = """\
TSIG, RFC 2845, came in 2000 from Paul Vixie, Olafur Gudmundsson, Donald
Eastlake and Brian Wellington: a secret shared by two machines, and a hash
of each message made with it, so a primary knows the secondary that asks
for its zone, and a server the client that updates it. RFC 8945
replaced it in 2020; the mechanism is the same.

BIND, written at Berkeley in 1984, maintained by Paul Vixie from 1988, was
rewritten as BIND 9 in 2000 by the Internet Systems Consortium.
"""

ROOTFS_CHOICES = ("dns_trixie",)
TOOL = "named"

HOST1, HOST2, OTHER = "host1", "host2", "other"
ALL = (HOST1, HOST2, OTHER)
LAN = "lan2"

WIRING = {
    HOST1: [(0, LAN, "10.1.2.2/24")],
    HOST2: [(0, LAN, "10.1.2.3/24")],
    OTHER: [(0, LAN, "10.1.2.99/24")],
}
GATEWAY = {}

LAYOUT = {HOST2: (-320, -110), OTHER: (-320, 110), LAN: (0, 0),
          HOST1: (320, 0)}
ETHS = {(HOST2, "eth0"): 90, (OTHER, "eth0"): 90, (HOST1, "eth0"): 270}

KEY = "/etc/bind/host1-host2.key"
AUTH = base_conf("  recursion no;\n")

# 7.5.2 Loading a New Key, 7.5.3 the server directive of host1 for host2,
# 7.5.4 TSIG-Based Access Control: transfer and update.
HOST1_CONF = AUTH + """
include "/etc/bind/host1-host2.key";

server 10.1.2.3 {
    keys { host1-host2. ;};
};

zone "example.com" {
  type primary;
  file "example.com";
  allow-transfer { key host1-host2. ;};
  allow-update { !{ !localnets; any; }; key host1-host2. ;};
};
"""
# 7.5.3 a key for each server of the primaries of a secondary zone.
HOST2_CONF = AUTH + """
include "/etc/bind/host1-host2.key";

zone "example.com" {
  type secondary;
  file "example.com.saved";
  primaries { 10.1.2.2 key host1-host2.; };
};
"""

CARD = r"""
    host2 --+
    (key)   +---- host1 (primary, transfers to the key only)
    other --+
    (no key)

  A secret shared by two servers, and every message signed with it.
"""

UPDATE = ("printf 'server 10.1.2.2\\nupdate add %s.example.com 300 IN A "
          "10.1.2.%d\\nsend\\n' | nsupdate %s 2>&1")

def main():
    opening()

    step("act 1: tsig-keygen on host1, the key copied to host2")
    ssh(HOST1, "mkdir -p /etc/bind")
    ssh(HOST1, "tsig-keygen host1-host2. > %s" % KEY)
    key = ssh(HOST1, "cat %s" % KEY)
    ok_key = 'key "host1-host2."' in key and "hmac-sha256" in key
    proof("host1, the key, a directive for named.conf:", key)
    put_any(HOST2, KEY, key)
    hold()

    step("act 2: the transfer allowed to the key, host2 copies the zone")
    named_start(HOST1, HOST1_CONF, [("/var/example.com", EXAMPLE_COM)])
    got = count_on(HOST1, 0, "tcp port 53 and host 10.1.2.3", lambda: (
        named_start(HOST2, HOST2_CONF),
        until(lambda: "Transfer completed" in log_of(HOST2), 30, 0.5))[1],
        settle=1.0)
    ok_signed = bool(got) and got["out"]
    ok_wire = bool(got) and got["count"] > 0
    proof("host2, the transfer:", "\n".join(
        l for l in log_of(HOST2).split("\n") if "Transfer" in l))
    proof("host1, who asked:", "\n".join(
        l for l in log_of(HOST1).split("\n") if "AXFR started" in l))
    hold()

    step("act 3: no key, no zone; with the key, the zone")
    refused = dig(OTHER, "@10.1.2.2 example.com AXFR")
    ok_refused = "Transfer failed" in refused
    proof("other, without the key:", refused)
    keyed = dig(HOST2, "-k %s @10.1.2.2 example.com AXFR" % KEY)
    ok_keyed = "XFR size" in keyed and "TSIG" in keyed
    proof("host2, dig -k with the key, the end of the zone and its TSIG:",
          "\n".join(keyed.strip().split("\n")[-6:]))
    hold()

    step("act 4: updates, from the lan and signed only")
    unsigned = ssh(HOST2, UPDATE % ("plain", 51, ""), check=False)
    signed = ssh(HOST2, UPDATE % ("signed", 50, "-k " + KEY), check=False)
    ok_upd = "REFUSED" in unsigned and dig(
        OTHER, "@10.1.2.2 signed.example.com +short").strip() == "10.1.2.50" \
        and status_of(dig(OTHER, "@10.1.2.2 plain.example.com")) == "NXDOMAIN"
    proof("host2, unsigned, then signed with the key:",
          "%s\n%s" % (unsigned.strip() or "(nothing)",
                      signed.strip() or "(no error: accepted)"))
    hold()

    step("act 5: a wrong secret, an unknown key")
    ssh(OTHER, "tsig-keygen host1-host2. > /tmp/wrong.key")
    ssh(OTHER, "tsig-keygen stranger. > /tmp/stranger.key")
    badsig = dig(OTHER, "-k /tmp/wrong.key @10.1.2.2 example.com SOA")
    badkey = dig(OTHER, "-k /tmp/stranger.key @10.1.2.2 example.com SOA")
    ok_badsig = status_of(badsig) == "NOTAUTH" and "BADSIG" in badsig
    ok_badkey = status_of(badkey) == "NOTAUTH" and "BADKEY" in badkey
    proof("other, the name host1-host2. with another secret:", "\n".join(
        l for l in badsig.split("\n") if "status" in l or "TSIG" in l))
    proof("other, the key stranger.:", "\n".join(
        l for l in badkey.split("\n") if "status" in l or "TSIG" in l))

    verdict([("tsig-keygen makes the key host1-host2.", ok_key),
             ("host2 transfers the zone with the key", ok_signed),
             ("the transfer crosses eth0 of host1 over TCP", ok_wire),
             ("other, without the key, is refused", ok_refused),
             ("dig -k with the key gets the zone, signed", ok_keyed),
             ("an update signed from the lan is taken, unsigned refused",
              ok_upd),
             ("a wrong secret: NOTAUTH, BADSIG", ok_badsig),
             ("an unknown key: NOTAUTH, BADKEY", ok_badkey)])

    story("""
the network is left up. Things to try:

the TSIG record at the end of a signed answer:
cloonix cli %s ssh host2 "dig -k %s @10.1.2.2 example.com SOA"

a new key, for host1 and a third server:
cloonix cli %s ssh host1 "tsig-keygen host1-host3."
""" % (NET, KEY, NET))

main()
