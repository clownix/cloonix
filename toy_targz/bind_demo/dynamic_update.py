#!/usr/bin/env python3
r"""dynamic_update - Dynamic Update, The Journal File.

    ./dynamic_update.py [net]     the net defaults to nemo

The example of the BIND 9 Administrator Reference Manual (ARM), chapter
"Advanced Configurations", sections "Dynamic Update" and "The Journal
File": example.com opened to updates by update-policy local, nsupdate -l
on the server, the journal of the changes, and the ARM's way to edit a
dynamic zone by hand, rndc freeze, then rndc thaw.
https://bind9.readthedocs.io/en/stable/chapter6.html#dynamic-update

    pc (192.168.254.10) ---- lan254 ---- ns1 (192.168.254.2)
                                         named, example.com
                                         update-policy local

  ns1 runs the base named.conf of the ARM, recursion no added, with the
  base zone example.com, its zone statement given update-policy local:
  updates are accepted
  signed with the key local-ddns, which named makes at its start and
  nsupdate -l reads.

  ACT 1  ns1 starts: printer.example.com does not exist.
  ACT 2  nsupdate -l on ns1 adds printer: pc resolves it at once, the
         serial has gone up by one, the change is in example.com.jnl.
  ACT 3  pc sends the same kind of update from the lan, unsigned: REFUSED.
  ACT 4  by hand, the ARM's way: rndc freeze writes the journal into the
         zone file, a record is added to it, rndc thaw loads it and opens
         the updates again; rndc sync -clean folds a later update into the
         file and removes the journal.

Image: dns_trixie (bind9)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf ns1 eth0 sniff=on
    cloonix cli nemo sniff ns1 eth0 --dump "udp port 53 and host 192.168.254.10"

  The UPDATE message of ACT 3, opcode 5 in the DNS header, and the
  REFUSED that answers it.

Important
  The updates of ACT 2 travel inside ns1, to 127.0.0.1: they cross no lan.
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
    EXAMPLE_COM, base_conf, dig, log_of, named_start, opening, proof, rndc,
    status_of, until, verdict)

HISTORY = """\
Dynamic update, RFC 2136, came in 1997, edited by Paul Vixie with Susan
Thomson, Yakov Rekhter and Jim Bound: records added and removed by a DNS
message, UPDATE, instead of a zone file edited and reloaded. DHCP servers
use it to publish the names of the machines they address, and the ACME
clients of Let's Encrypt to publish the record that proves a domain is
theirs.

BIND carries it since BIND 8; BIND, written at Berkeley in 1984 and
maintained by Paul Vixie from 1988, was rewritten as BIND 9 in 2000 by the
Internet Systems Consortium.
"""

ROOTFS_CHOICES = ("dns_trixie",)
TOOL = "named"

NS1, PC = "ns1", "pc"
ALL = (NS1, PC)
L254 = "lan254"

WIRING = {
    NS1: [(0, L254, "192.168.254.2/24")],
    PC: [(0, L254, "192.168.254.10/24")],
}
GATEWAY = {}

LAYOUT = {PC: (-300, 0), L254: (0, 0), NS1: (300, 0)}
ETHS = {(PC, "eth0"): 90, (NS1, "eth0"): 270}

# 6.1 Dynamic Update: the base named.conf, example.com open to local-ddns.
CONF = base_conf("  recursion no;\n") + """
zone "example.com" {
  type primary;
  file "example.com";
  update-policy local;
};
"""

CARD = r"""
    pc ---- ns1 (example.com, update-policy local)

  Records added by a message, kept in a journal, folded into the file.
"""

def addr(name):
    return dig(PC, "@192.168.254.2 %s.example.com +short" % name).strip()

def serial():
    out = dig(PC, "@192.168.254.2 example.com SOA +short").split()
    return out[2] if len(out) > 2 else ""

def main():
    opening()

    step("act 1: ns1 starts, printer.example.com does not exist")
    named_start(NS1, CONF, [("/var/example.com", EXAMPLE_COM)])
    before = dig(PC, "@192.168.254.2 printer.example.com")
    ok_absent = status_of(before) == "NXDOMAIN"
    s0 = serial()
    proof("pc, printer.example.com and the serial of the zone:",
          "status %s, serial %s" % (status_of(before), s0))
    hold()

    step("act 2: nsupdate -l on ns1 adds printer")
    ssh(NS1, "printf 'update add printer.example.com 300 IN A "
             "192.168.254.10\\nsend\\n' | nsupdate -l")
    ok_added = until(lambda: addr("printer") == "192.168.254.10", 10, 0.5)
    s1 = serial()
    ok_serial = s0.isdigit() and s1 == str(int(s0) + 1)
    proof("pc, printer.example.com and the serial:",
          "%s, serial %s" % (addr("printer"), s1))
    jnl = ssh(NS1, "named-journalprint /var/example.com.jnl", check=False)
    ok_jnl = "printer.example.com." in jnl
    proof("ns1, the journal example.com.jnl, by named-journalprint:", jnl)
    hold()

    step("act 3: the same from pc, unsigned, is refused")
    got = count_on(NS1, 0, "udp port 53 and host 192.168.254.10", lambda: ssh(
        PC, "printf 'server 192.168.254.2\\nupdate add rogue.example.com 300 "
            "IN A 10.6.6.6\\nsend\\n' | nsupdate 2>&1", check=False),
        settle=1.0)
    said = got["out"] if got else ""
    ok_refused = "REFUSED" in said and status_of(
        dig(PC, "@192.168.254.2 rogue.example.com")) == "NXDOMAIN"
    ok_wire = bool(got) and got["count"] >= 2
    proof("pc, nsupdate to 192.168.254.2:", said)
    denied = "\n".join(l for l in log_of(NS1).split("\n") if "denied" in l)
    proof("ns1, in its log:", denied)
    hold()

    step("act 4: freeze, a record by hand, thaw; then sync -clean")
    rndc(NS1, "freeze example.com")
    frozen = ssh(NS1, "grep printer /var/example.com", check=False)
    ok_frozen = "printer" in frozen
    proof("ns1, rndc freeze has written printer into the zone file:", frozen)
    s2 = str(int(s1) + 1) if s1.isdigit() else ""
    ssh(NS1, "sed -i 's/%s/%s/' /var/example.com" % (s1, s2))
    ssh(NS1, "echo 'scanner.example.com. 300 IN A 192.168.254.11' "
             ">> /var/example.com")
    rndc(NS1, "thaw example.com")
    ok_thaw = until(lambda: addr("scanner") == "192.168.254.11", 10, 0.5) \
        and addr("printer") == "192.168.254.10" and serial() == s2
    proof("pc, after rndc thaw:", "printer %s\nscanner %s\nserial %s"
          % (addr("printer"), addr("scanner"), serial()))
    ssh(NS1, "printf 'update add copier.example.com 300 IN A "
             "192.168.254.12\\nsend\\n' | nsupdate -l")
    until(lambda: addr("copier") == "192.168.254.12", 10, 0.5)
    rndc(NS1, "sync -clean example.com")
    synced = ssh(NS1, "grep copier /var/example.com; "
                      "ls /var/example.com.jnl 2>&1", check=False)
    ok_sync = "copier" in synced and "No such file" in synced
    proof("ns1, after rndc sync -clean: copier in the file, no journal:",
          synced)

    verdict([("printer.example.com absent at first", ok_absent),
             ("nsupdate -l adds it, pc resolves it", ok_added),
             ("the update raised the serial by one", ok_serial),
             ("the journal holds the change", ok_jnl),
             ("an unsigned update from pc is REFUSED", ok_refused),
             ("the refused UPDATE crosses eth0 of ns1", ok_wire),
             ("rndc freeze writes the journal into the file", ok_frozen),
             ("rndc thaw loads the record added by hand", ok_thaw),
             ("rndc sync -clean: file current, journal gone", ok_sync)])

    story("""
the network is left up. Things to try:

one more record, then its trace in the journal:
cloonix cli %s ssh ns1 "printf 'update add fax.example.com 300 IN A 192.168.254.13\\nsend\\n' | nsupdate -l"
cloonix cli %s ssh ns1 "named-journalprint /var/example.com.jnl"
""" % (NET, NET))

main()
