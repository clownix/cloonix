#!/usr/bin/env python3
r"""secondary_authoritative - Secondary Authoritative Name Server.

    ./secondary_authoritative.py [net]     the net defaults to nemo

The example of the BIND 9 Administrator Reference Manual (ARM), chapter
"Configurations and Zone Files", section "Secondary Authoritative Name
Server": the authoritative secondary named.conf, as the ARM prints it,
copying example.com from the primary of the section before.
https://bind9.readthedocs.io/en/stable/chapter3.html#secondary-authoritative-name-server

    pc (192.168.4.20) ---+                 +--- ns1 (192.168.254.2)
                         +-- lan4 -- r -- lan254   primary
    sec (192.168.4.14) --+       (router)
         secondary

  sec is 192.168.4.14, the first secondary the primary allows to transfer
  the zone. It has no zone file of its own: it copies example.com from ns1
  and keeps the copy in example.com.saved.

  ACT 1  the primary starts, then the secondary: it transfers the zone, over
         TCP port 53, and writes it to example.com.saved.
  ACT 2  pc asks both: the same answer, both with the flag aa. Nothing tells
         a primary from a secondary.
  ACT 3  the serial number: www changes on the primary, serial untouched,
         and the secondary keeps the old address; the serial raised, the
         secondary copies the new zone.
  ACT 4  the primary stopped, the secondary restarted: it loads
         example.com.saved and answers at once, as the comment of the ARM
         says.

Image: dns_trixie (bind9)
Size:  4 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf sec eth0 sniff=on
    cloonix cli nemo sniff sec eth0 --dump "tcp port 53"

  The zone transfer of ACT 1: a TCP connection to port 53 of ns1, the
  question AXFR, and the whole zone back in one message.

Important
  The ARM's NOTIFY goes to the name servers of the zone: ns2.example.net,
  that no server of this net resolves, not sec. The secondary is asked to
  check now by rndc refresh, which does what a NOTIFY would make it do.
  The saved copy is in the raw format of BIND: named-compilezone prints
  it as text, as the ARM section "Additional File Formats" shows.
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
    LOCALHOST_FORWARD, LOCALHOST_REV, PRIMARY_CONF, PRIMARY_FILES, dig,
    flags_of, forwarding, log_of, named_start, named_stop, opening, proof,
    rndc, status_of, until, verdict)

HISTORY = """\
The zone transfer is as old as the DNS: RFC 1034 and 1035, in 1987,
already define AXFR, the whole zone copied from a primary to its
secondaries over TCP, and the serial number of the SOA record that tells
a secondary its copy is out of date. The secondary polls the SOA at the
refresh interval of the zone, hours, unless a NOTIFY (RFC 1996, Paul Vixie,
1996) tells it sooner.

BIND has served zones this way since the 1980s; it was written at Berkeley
in 1984, maintained by Paul Vixie from 1988, and rewritten as BIND 9 in
2000 by the Internet Systems Consortium.
"""

ROOTFS_CHOICES = ("dns_trixie",)
TOOL = "named"

PC, SEC, R, NS1 = "pc", "sec", "r", "ns1"
ALL = (NS1, SEC, PC, R)
L4, L254 = "lan4", "lan254"

WIRING = {
    PC: [(0, L4, "192.168.4.20/24")],
    SEC: [(0, L4, "192.168.4.14/24")],
    R: [(0, L254, "192.168.254.1/24"), (1, L4, "192.168.4.1/24")],
    NS1: [(0, L254, "192.168.254.2/24")],
}
GATEWAY = {PC: "192.168.4.1", SEC: "192.168.4.1", NS1: "192.168.254.1"}

LAYOUT = {PC: (-440, -110), SEC: (-440, 110), L4: (-220, 0), R: (0, 0),
          L254: (220, 0), NS1: (440, 0)}
ETHS = {(PC, "eth0"): 90, (SEC, "eth0"): 90, (R, "eth0"): 90,
        (R, "eth1"): 270, (NS1, "eth0"): 270}

# 3.2.2 Secondary Authoritative Name Server
SECONDARY_CONF = """\
// authoritative secondary named.conf file
// options clause defining the server-wide properties
options {
  // all relative paths use this directory as a base
  directory "/var";
  // version statement for security to avoid hacking known weaknesses
  // if the real version number is revealed
  version "not currently available";
  // This is the default - allows user queries from any IP
  allow-query { any; };
  // normal server operations may place items in the cache
  // this prevents any user query from accessing these items
  // only authoritative zone data will be returned
  allow-query-cache { none; };
  // Do not provide recursive service to user queries
  recursion no;
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
// Provide forward mapping zone for localhost
// (optional)
zone "localhost" {
  type primary;
  file "master/localhost-forward.db";
  notify no;
};
// Provide reverse mapping zone for the loopback
// address 127.0.0.1
zone "0.0.127.in-addr.arpa" {
  type primary;
  file "localhost.rev";
  notify no;
};
// We are the secondary server for example.com
zone "example.com" {
  // this is a secondary server for the zone
  type secondary;
  // the file statement here allows the secondary to save
  // each zone transfer so that in the event of a program restart
  // the zone can be loaded immediately and the server can start
  // to respond to queries without waiting for a zone transfer
  file "example.com.saved";
  // IP address of example.com primary server
  primaries { 192.168.254.2; };
};
"""
SECONDARY_FILES = [("/var/master/localhost-forward.db", LOCALHOST_FORWARD),
                   ("/var/localhost.rev", LOCALHOST_REV)]

CARD = r"""
    pc ---+
          +-- r ---- ns1 (primary)
    sec --+
    (secondary)

  A zone copied, a serial number obeyed, a copy that outlives its source.
"""

def www(server):
    return dig(PC, "@%s www.example.com +short" % server).strip()

def transferred(serial):
    return "Transfer completed" in log_of(SEC) and serial in rndc(
        SEC, "zonestatus example.com")

def main():
    opening()
    forwarding(R)

    step("act 1: the primary starts, then the secondary copies the zone")
    named_start(NS1, PRIMARY_CONF, PRIMARY_FILES)
    got = count_on(SEC, 0, "tcp port 53", lambda: (
        named_start(SEC, SECONDARY_CONF, SECONDARY_FILES),
        until(lambda: transferred("2003080800"), 30, 0.5))[1], settle=1.0)
    ok_xfr = got["out"] if got else transferred("2003080800")
    ok_tcp = bool(got) and got["count"] > 0
    proof("sec, the transfer in /var/log/named/example.log:", "\n".join(
        l for l in log_of(SEC).split("\n") if "transfer" in l.lower()))
    saved = ssh(SEC, "named-compilezone -f raw -F text -o - example.com "
                     "/var/example.com.saved 2>/dev/null", check=False)
    ok_saved = "www.example.com." in saved and "192.168.254.7" in saved
    proof("sec, example.com.saved printed as text by named-compilezone:",
          saved)
    hold()

    step("act 2: pc asks the primary and the secondary")
    a1 = dig(PC, "@192.168.254.2 www.example.com")
    a2 = dig(PC, "@192.168.4.14 www.example.com")
    ok_same = www("192.168.254.2") == www("192.168.4.14") == "192.168.254.7" \
        and "aa" in flags_of(a1) and "aa" in flags_of(a2)
    proof("pc, the two answers:", "\n".join(
        l for l in (a1 + a2).split("\n") if "flags:" in l or
        l.startswith("www.example.com.")))
    hold()

    step("act 3: www changes on the primary, first with the serial unchanged")
    ssh(NS1, "sed -i 's/192.168.254.7/192.168.254.8/' /var/example.com")
    rndc(NS1, "reload example.com")
    until(lambda: www("192.168.254.2") == "192.168.254.8", 20, 0.5)
    rndc(SEC, "refresh example.com")
    ok_kept = not until(lambda: www("192.168.4.14") == "192.168.254.8", 8, 1) \
        and www("192.168.4.14") == "192.168.254.7"
    proof("the same serial, two answers:",
          "primary   www %s\nsecondary www %s"
          % (www("192.168.254.2"), www("192.168.4.14")))
    step("act 3: the serial raised to 2003080801, the secondary follows")
    ssh(NS1, "sed -i 's/2003080800/2003080801/' /var/example.com")
    rndc(NS1, "reload example.com")
    rndc(SEC, "refresh example.com")
    ok_new = until(lambda: www("192.168.4.14") == "192.168.254.8", 30, 1)
    proof("a higher serial, the new zone copied:",
          "primary   www %s\nsecondary www %s\n%s"
          % (www("192.168.254.2"), www("192.168.4.14"), "\n".join(
              l for l in log_of(SEC).split("\n") if "2003080801" in l)))
    hold()

    step("act 4: the primary stopped, the secondary restarted")
    named_stop(NS1)
    named_stop(SEC)
    named_start(SEC, SECONDARY_CONF)
    down = dig(PC, "@192.168.254.2 www.example.com +tries=1 +time=2")
    after = dig(PC, "@192.168.4.14 www.example.com")
    ok_alone = status_of(down) == "" and \
        www("192.168.4.14") == "192.168.254.8" and "aa" in flags_of(after)
    proof("pc, the primary is gone:", down.strip().split("\n")[-1])
    proof("pc, the secondary answers from example.com.saved:", "\n".join(
        l for l in after.split("\n") if "flags:" in l or
        l.startswith("www.example.com.")))

    verdict([("sec transfers example.com from ns1", ok_xfr),
             ("the transfer is TCP, port 53", ok_tcp),
             ("example.com.saved holds the zone", ok_saved),
             ("primary and secondary answer the same, both aa", ok_same),
             ("serial unchanged: the secondary keeps the old address",
              ok_kept),
             ("serial raised: the secondary copies the new zone", ok_new),
             ("primary down: the secondary answers from its saved copy",
              ok_alone)])

    story("""
the network is left up, the primary stopped. Things to try:

the primary back, then a change seen by the secondary:
cloonix cli %s ssh ns1 "named -c /etc/bind/named.conf"
cloonix cli %s ssh sec "rndc refresh example.com"

the state of the copy:
cloonix cli %s ssh sec "rndc zonestatus example.com"
""" % (NET, NET, NET))

main()
