#!/usr/bin/env python3
r"""load_balancing - Load Balancing.

    ./load_balancing.py [net]     the net defaults to nemo

The example of the BIND 9 Administrator Reference Manual (ARM), chapter
"Configurations and Zone Files", section "Load Balancing": three HTTP
servers, 10.0.0.1, 10.0.0.2 and 10.0.0.3, behind one name, www, with three
A records of TTL 600.
https://bind9.readthedocs.io/en/stable/chapter3.html#load-balancing

    web1 10.0.0.1 ---+
    web2 10.0.0.2 ---+-- web -- r -- dns -- ns1 192.168.254.2
    web3 10.0.0.3 ---+      (router)        the three A records of www
    pc   10.0.0.10 --+

  ns1 is the primary of the ARM, its example.com zone holding the three
  records of the section for www.

  ACT 1  ns1 starts; pc sees the three addresses of www, in an order
         drawn at random for each answer.
  ACT 2  thirty questions: each of the three addresses comes first, about
         one time in three.
  ACT 3  pc connects to the first address of each answer, as most clients
         do: the three machines receive it in turn.

Image: dns_trixie (bind9)
Size:  6 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf ns1 eth0 sniff=on
    cloonix cli nemo sniff ns1 eth0 --dump "udp port 53"

  Answers in a row to the same question: the same three records, in an
  order drawn again for each one.

Important
  A ping stands for the connection of a client: the ARM's three HTTP
  servers serve nothing here, the point is which one a client picks.
"""
import collections
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
    EXAMPLE_COM, PRIMARY_CONF, PRIMARY_FILES, answer_of, dig, forwarding,
    icmp_in_echos, named_start, opening, proof, verdict)

HISTORY = """\
Several addresses behind one name is one of the oldest ways to share the
load of a service between machines, older than the load balancers of the
web: the name server gives every address, in an order that changes, and
each client takes the first. It costs nothing, and knows nothing of the
health of the servers it names.

BIND, which gives the records of a set in a changing order, was written at
Berkeley in 1984, maintained by Paul Vixie from 1988 and rewritten as BIND
9 in 2000 by the Internet Systems Consortium.
"""

ROOTFS_CHOICES = ("dns_trixie",)
TOOL = "named"

NS1, R, PC, WEB1, WEB2, WEB3 = "ns1", "r", "pc", "web1", "web2", "web3"
WEBS = {WEB1: "10.0.0.1", WEB2: "10.0.0.2", WEB3: "10.0.0.3"}
ALL = (NS1, R, PC, WEB1, WEB2, WEB3)
DNS, WEB = "dns", "web"

WIRING = {
    NS1: [(0, DNS, "192.168.254.2/24")],
    R: [(0, DNS, "192.168.254.1/24"), (1, WEB, "10.0.0.254/24")],
    PC: [(0, WEB, "10.0.0.10/24")],
    WEB1: [(0, WEB, "10.0.0.1/24")],
    WEB2: [(0, WEB, "10.0.0.2/24")],
    WEB3: [(0, WEB, "10.0.0.3/24")],
}
GATEWAY = {NS1: "192.168.254.1", PC: "10.0.0.254"}

LAYOUT = {WEB1: (-480, -210), WEB2: (-480, -70), WEB3: (-480, 70),
          PC: (-480, 210), WEB: (-260, 0), R: (0, 0), DNS: (240, 0),
          NS1: (460, 0)}
ETHS = {(WEB1, "eth0"): 90, (WEB2, "eth0"): 90, (WEB3, "eth0"): 90,
        (PC, "eth0"): 90, (R, "eth0"): 90, (R, "eth1"): 270,
        (NS1, "eth0"): 270}

# 3.4 Load Balancing: the records of the table, in the base zone file.
RECORDS = """\
www        600     IN      A       10.0.0.1
           600     IN      A       10.0.0.2
           600     IN      A       10.0.0.3
"""
ZONE = EXAMPLE_COM.replace("www        IN      A       192.168.254.7\n",
                           RECORDS)
FILES = [(p, ZONE if p == "/var/example.com" else t) for p, t in PRIMARY_FILES]

CARD = r"""
    web1 --+
    web2 --+-- r ---- ns1 (www: 10.0.0.1, .2, .3)
    web3 --+
    pc   --+

  One name, three addresses, a different one first every time.
"""

# Thirty questions, the first address of each answer.
FIRSTS = ("for i in $(seq 1 30); do "
          "dig @192.168.254.2 www.example.com +short | head -n 1; done")
# Twenty-four clients, each to the first address it is given.
CLIENTS = ("for i in $(seq 1 24); do "
           "a=$(dig @192.168.254.2 www.example.com +short | head -n 1); "
           "ping -c 1 -W 1 $a >/dev/null; done")

def main():
    opening()
    forwarding(R)

    step("act 1: ns1 serves www with the three records of the ARM")
    named_start(NS1, PRIMARY_CONF, FILES)
    orders = []
    for _ in range(5):
        out = answer_of(dig(PC, "@192.168.254.2 www.example.com"))
        orders.append(" ".join(l.split()[-1] for l in out.split("\n") if l))
    ok_three = all(sorted(o.split()) == sorted(WEBS.values()) for o in orders)
    proof("pc, five answers in a row, the addresses in their order:",
          "\n".join(orders))
    hold()

    step("act 2: thirty questions, which address comes first")
    got = count_on(NS1, 0, "udp port 53", lambda: ssh(PC, FIRSTS), settle=1.0)
    firsts = collections.Counter((got["out"] if got else
                                  ssh(PC, FIRSTS)).split())
    ok_rotate = all(firsts[a] > 0 for a in WEBS.values())
    ok_seen = bool(got) and got["count"] >= 60
    proof("pc, how many times each address came first:", "\n".join(
        "%-9s %2d" % (a, firsts[a]) for a in sorted(WEBS.values())))
    hold()

    step("act 3: each client connects to the first address it is given")
    before = dict((w, icmp_in_echos(w)) for w in WEBS)
    ssh(PC, CLIENTS)
    reached = dict((w, icmp_in_echos(w) - before[w]) for w in WEBS)
    ok_spread = all(n > 0 for n in reached.values())
    proof("the connections each machine received, of 24:", "\n".join(
        "%-5s %-9s %2d" % (w, WEBS[w], reached[w]) for w in sorted(WEBS)))

    verdict([("www has the three addresses of the ARM", ok_three),
             ("each address comes first in turn", ok_rotate),
             ("the thirty questions and answers cross eth0 of ns1",
              ok_seen),
             ("the three machines are reached by clients", ok_spread)])

    story("""
the network is left up. Things to try:

the order of the records, question after question:
cloonix cli %s ssh pc "dig @192.168.254.2 www.example.com +short"

the TTL of 600 seconds counted down in a cache is the time a client
keeps the address it chose.
""" % NET)

main()
