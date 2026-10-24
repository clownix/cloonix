#!/usr/bin/env python3
r"""resolver - Resolver (Caching Name Servers): Resolver Configuration.

    ./resolver.py [net]     the net defaults to nemo

The example of the BIND 9 Administrator Reference Manual (ARM), chapter
"Configurations and Zone Files", section "Resolver Configuration", with its
hint file and its private reverse map zone file: a closed resolver for the
two corporate subnets of the ARM, asking a lab internet.
https://bind9.readthedocs.io/en/stable/chapter3.html#resolver-configuration

    pc  192.168.4.10 --+                      +-- root  198.41.0.4  (a.root-servers.net)
                       +-- corp -- r -- inet -+-- gtld  192.5.6.30  (a.gtld-servers.net)
    res 192.168.4.2 ---+       (router)       +-- ns1   192.168.254.2 (ns1.example.com)
        the resolver                          +-- out   203.0.113.66  (an outsider)

  The lab internet has the hierarchy of the real one: root delegates com
  and net to gtld, which delegates example.com and example.net to ns1. The
  hint file of res names one root server, at the address of
  a.root-servers.net.

  ACT 1  the lab internet starts, then res with the named.conf of the ARM.
  ACT 2  pc asks www.example.com, the cache of res is cold: res asks the
         root, then gtld, then ns1. With res in its /etc/resolv.conf, pc
         walks the same path with dig +trace.
  ACT 3  the same question again: res answers from its cache, the root
         hears nothing. ftp.example.com, a CNAME into example.net, is
         followed to its address.
  ACT 4  what res answers alone: the reverse map of 192.168.254.4 from its
         zone file, a private address from its empty zones; nothing leaves.
  ACT 5  out, outside corpnets, is refused: a closed resolver.

Image: dns_trixie (bind9)
Size:  7 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf res eth0 sniff=on
    cloonix cli nemo sniff res eth0 --dump "udp port 53 and host 198.41.0.4"

  The question res sends the root server for a cold name, and the
  referral back: the NS of com, at a.gtld-servers.net. In the cache, the
  same name sends nothing.

Important
  One statement is added to the named.conf of the ARM, dnssec-validation
  no: the lab root has no key, and the root key built in BIND would refuse
  its answers. ns2.example.net, external to example.com in the ARM, has
  the address of ns1 here.
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
    RESOLVER_CONF, RESOLVER_FILES, answer_of, brief, dig, flags_of,
    forwarding, internet_start, log_of, named_start, opening, proof,
    status_of, verdict)

HISTORY = """\
A stub resolver asks; a resolver finds. From the root down, it follows the
referrals of the hierarchy and keeps every answer for its TTL. It knows
where to begin from the hint file: the 13 root server names, a to m,
published by IANA as named.root; a.root-servers.net, at 198.41.0.4, is
run by Verisign, as are the servers of com and net, a.gtld-servers.net at
192.5.6.30 among them.

BIND has been a caching resolver since its first version, written at
Berkeley in 1984; Paul Vixie maintained it from 1988, and the Internet
Systems Consortium rewrote it as BIND 9 in 2000.
"""

ROOTFS_CHOICES = ("dns_trixie",)
TOOL = "named"

PC, RES, R, ROOT, GTLD, NS1, OUT = "pc", "res", "r", "root", "gtld", "ns1", \
    "out"
ALL = (RES, PC, R, ROOT, GTLD, NS1, OUT)
CORP, INET = "corp", "inet"

WIRING = {
    PC: [(0, CORP, "192.168.4.10/24")],
    RES: [(0, CORP, "192.168.4.2/24")],
    R: [(0, CORP, "192.168.4.1/24"), (1, INET, "198.41.0.1/24")],
    ROOT: [(0, INET, "198.41.0.4/24")],
    GTLD: [(0, INET, "192.5.6.30/24")],
    NS1: [(0, INET, "192.168.254.2/24")],
    OUT: [(0, INET, "203.0.113.66/24")],
}
GATEWAY = {PC: "192.168.4.1", RES: "192.168.4.1", ROOT: "198.41.0.1",
           GTLD: "192.5.6.1", NS1: "192.168.254.1", OUT: "203.0.113.1"}

LAYOUT = {PC: (-480, -100), RES: (-480, 100), CORP: (-260, 0), R: (0, 0),
          INET: (260, 0), ROOT: (480, -210), GTLD: (480, -70),
          NS1: (480, 70), OUT: (480, 210)}
ETHS = {(PC, "eth0"): 90, (RES, "eth0"): 90, (R, "eth0"): 270,
        (R, "eth1"): 90, (ROOT, "eth0"): 270, (GTLD, "eth0"): 270,
        (NS1, "eth0"): 270, (OUT, "eth0"): 270}

CARD = r"""
    pc ---+                 +--- root  (a.root-servers.net)
          +-- r --- inet ---+--- gtld  (com, net)
    res --+                 +--- ns1   (example.com, example.net)
    (resolver)              +--- out   (an outsider)

  A cold name found from the root down, then answered from the cache.
"""

def main():
    opening()
    forwarding(R)
    # the gateways of the servers, on the inet side of r
    for addr in ("192.5.6.1/24", "192.168.254.1/24", "203.0.113.1/24"):
        ssh(R, "ip addr add %s dev eth1" % addr)

    step("act 1: the lab internet starts, then the resolver of the ARM")
    internet_start(ROOT, GTLD, NS1)
    named_start(RES, RESOLVER_CONF, RESOLVER_FILES)
    proof("res, the zones of its named.conf loaded:", "\n".join(
        l for l in log_of(RES).split("\n") if "loaded serial" in l))
    hold()

    step("act 2: a cold cache, the question goes down from the root")
    got = count_on(RES, 0, "udp port 53 and host 198.41.0.4",
                   lambda: dig(PC, "@192.168.4.2 www.example.com"),
                   settle=1.0)
    cold = got["out"] if got else dig(PC, "@192.168.4.2 www.example.com")
    ok_cold = "192.168.254.7" in answer_of(cold) and \
        "ra" in flags_of(cold) and "aa" not in flags_of(cold)
    ok_root = bool(got) and got["count"] >= 2
    proof("pc, the answer of res (ra: recursion available, no aa):",
          brief(cold))
    # the stub resolver of pc, which dig +trace asks for the server names
    ssh(PC, "echo nameserver 192.168.4.2 > /etc/resolv.conf")
    trace = dig(PC, "+trace www.example.com")
    hops = [l for l in trace.split("\n") if l.startswith(";; Received")]
    ok_trace = len(hops) >= 3 and "198.41.0.4#53" in hops[-3] and \
        "192.5.6.30#53" in hops[-2] and "192.168.254.2#53" in hops[-1]
    proof("pc, dig +trace, the referrals from the root down:",
          "\n".join(hops))
    hold()

    step("act 3: the same name from the cache, then a CNAME followed")
    got = count_on(RES, 0, "udp port 53 and host 198.41.0.4",
                   lambda: dig(PC, "@192.168.4.2 www.example.com"),
                   settle=1.0)
    warm = got["out"] if got else ""
    ttl = answer_of(warm).split()
    ok_cache = bool(got) and got["count"] == 0 and len(ttl) > 1 and \
        ttl[1].isdigit() and int(ttl[1]) < 172800
    proof("pc, the same answer, its TTL counting down in the cache:",
          answer_of(warm))
    cname = dig(PC, "@192.168.4.2 ftp.example.com")
    ok_cname = "CNAME\tftp.example.net." in answer_of(cname) and \
        "192.168.254.21" in answer_of(cname)
    proof("pc, ftp.example.com, an alias into example.net:",
          answer_of(cname))
    hold()

    step("act 4: the zones of res answer alone, nothing leaves")
    got = count_on(RES, 0, "udp port 53 and not host 192.168.4.10",
                   lambda: (dig(PC, "@192.168.4.2 -x 192.168.254.4"),
                            dig(PC, "@192.168.4.2 -x 10.1.2.3")),
                   settle=1.0)
    ptr, empty = got["out"] if got else ("", "")
    ok_ptr = "mail.example.com." in answer_of(ptr) and "aa" in flags_of(ptr)
    ok_empty = status_of(empty) == "NXDOMAIN"
    ok_local = bool(got) and got["count"] == 0
    proof("pc, 192.168.254.4 from the file 192.168.254.rev:", brief(ptr))
    proof("pc, 10.1.2.3 from an empty zone:", brief(empty))
    hold()

    step("act 5: out is not in corpnets, it is refused")
    refused = dig(OUT, "@192.168.4.2 www.example.com")
    ok_closed = status_of(refused) == "REFUSED"
    proof("out, the same question:", brief(refused))

    verdict([("www.example.com found, ra and no aa", ok_cold),
             ("a cold name makes res ask the root server", ok_root),
             ("dig +trace: root, then gtld, then ns1", ok_trace),
             ("the same name again: from the cache, nothing to the root",
              ok_cache),
             ("ftp.example.com followed into example.net", ok_cname),
             ("192.168.254.4 answered from 192.168.254.rev", ok_ptr),
             ("10.1.2.3 answered NXDOMAIN by an empty zone", ok_empty),
             ("those two sent nothing out of res", ok_local),
             ("out, outside corpnets, is REFUSED", ok_closed)])

    story("""
the network is left up. Things to try:

the whole walk from the root, from pc:
cloonix cli %s ssh pc "dig +trace mail.example.net"

the cache of res, written to /var/named_dump.db:
cloonix cli %s ssh res "rndc dumpdb -cache"
cloonix cli %s ssh res "cat /var/named_dump.db"
""" % (NET, NET, NET))

main()
