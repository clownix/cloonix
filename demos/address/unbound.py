#!/usr/bin/env python3
r"""unbound - a resolver that answers, caches, and can be caught doing it.

    ./unbound.py [net]             the net defaults to nemo

Three machines on one lan::

  cl --- lan1 --- ns
          |
         web  10.0.0.80

  ns runs Unbound. It is authoritative for a small made-up zone, shop.test,
  and cl is a plain client with ns as its only resolver. web is the machine
  the name points at, so a lookup can be followed by a real connection.

  ACT 1  cl asks ns for www.shop.test. Unbound answers 10.0.0.80, and cl
         then reaches web at exactly that address -- name to address to
         connection, the whole point of a resolver.
  ACT 2  cl asks a second time. Unbound serves it from cache, and the query
         on the wire is caught to show the client really did ask the server.
  ACT 3  cl asks for a name the server does not hold. Unbound, with no
         recursion configured here, refuses rather than inventing an answer.

Image: unbound_trixie (unbound, dig, drill)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
  DNS is UDP on port 53. The query and its answer are one small exchange:

    cloonix cli nemo cnf ns eth0 sniff=on
    cloonix cli nemo sniff ns eth0 --dump udp port 53

Important
  Nothing is baked into the image: Unbound is installed and the demo writes
  its configuration, with the zone in it, at run time.
"""
import os
import sys
import time

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    NET, build, check_net, choose_rootfs, count_on, erase_net, hold, place,
    put, reached, ssh, step, story, tools_present)

HISTORY = """\
Unbound was written by NLnet Labs in the Netherlands, first released in 2008,
with Wouter Wijngaards as its main author. It is a validating, recursive,
caching resolver: the piece that turns a name into an address for a client,
checks DNSSEC signatures as it goes, and remembers what it learnt. It was
built as a clean, security-minded alternative to using BIND for the resolver
job, and it is widely run for exactly that. Here it is pinned to one small
local zone so the exchange is easy to watch.

Today: Unbound is the default resolver of FreeBSD and OpenBSD, sits in
pfSense, OPNsense and Pi-hole setups, and answers for large public resolvers;
validating DNSSEC at the resolver is its usual role.
"""

ROOTFS_CHOICES = ("unbound_trixie",)

CL, NS, WEB = "cl", "ns", "web"
ALL = (CL, NS, WEB)
LAN = "lan1"

WIRING = {
    CL:  [(0, LAN, "10.0.0.10/24")],
    NS:  [(0, LAN, "10.0.0.53/24")],
    WEB: [(0, LAN, "10.0.0.80/24")],
}
GATEWAY = {}

UNBOUND_CONF = """\
server:
    username: ""
    chroot: ""
    directory: "/etc/unbound"
    pidfile: "/run/unbound.pid"
    do-daemonize: yes
    interface: 0.0.0.0
    access-control: 0.0.0.0/0 allow
    verbosity: 1
    local-zone: "shop.test." static
    local-data: "www.shop.test. IN A 10.0.0.80"
    local-data-ptr: "10.0.0.80 www.shop.test."
"""

def resolver_start():
    put(NS, "/etc/unbound/unbound.conf", UNBOUND_CONF)
    ssh(NS, "unbound -c /etc/unbound/unbound.conf")
    for _ in range(20):
        if ssh(NS, "ss -lun 2>/dev/null | grep -q :53 && echo up",
               check=False).strip():
            return
        time.sleep(1)
    raise RuntimeError("unbound did not start:\n%s"
                       % ssh(NS, "tail -n 20 /var/log/syslog", check=False))

def dig(name, extra=""):
    return ssh(CL, "dig @10.0.0.53 %s %s" % (name, extra), check=False)

def ping(dst, count=5):
    return ssh(CL, "ping -c %d %s" % (count, dst), check=False)

LAYOUT = {CL: (-300, 0), LAN: (-40, 0), NS: (240, 0), WEB: (-40, 200)}
ETHS = {(CL, "eth0"): 0, (NS, "eth0"): 180, (WEB, "eth0"): 90}

CARD = r"""
   cl --- lan1 --- ns  (Unbound, authoritative for shop.test)
           |
          web  10.0.0.80  (www.shop.test)

  ns resolves www.shop.test to 10.0.0.80; cl then reaches web there. Name to
  address to connection, and the query caught on the wire.
"""

def main():
    story(HISTORY)
    step("checking the server")
    check_net()
    step("looking for the image")
    rootfs = choose_rootfs()
    erase_net()
    place()
    story(CARD)
    story("  image: %s" % rootfs)

    step("creating and cabling the three machines")
    build(rootfs)
    tools_present(NS, "unbound")
    tools_present(CL, "dig", "ping")
    ssh(WEB, "ip link set dev eth0 up", check=False)

    step("act 1: name to address to connection")
    story()
    resolver_start()
    r = dig("www.shop.test", "+short")
    ip = r.strip().split("\n")[-1].strip() if r.strip() else ""
    story("    cl asked ns for www.shop.test, got:")
    story("      %s" % (ip or "(nothing)"))
    ok_ip = ip == "10.0.0.80"
    story()
    p = ping(ip or "10.0.0.80", count=3)
    story("    cl -> that address:")
    story(p.strip())
    ok_ping = reached(p)
    story()

    step("act 2: ask again, caught on the wire")
    story()
    got = count_on(NS, 0, "udp port 53",
                   lambda: dig("www.shop.test", "+short"), settle=3.0)
    if got is not None:
        story("    %d DNS packet(s) crossed ns eth0 for the repeat query"
              % got["count"])
    story("    Unbound served it from cache; the client still had to ask.")
    hold()

    step("act 3: a name the server does not hold")
    story()
    r = dig("nowhere.shop.test")
    status = ""
    for line in r.split("\n"):
        if "status:" in line:
            status = line.strip()
    story("    cl asked for nowhere.shop.test:")
    story("      %s" % (status or "(no status line)"))
    story("    No recursion is configured, so Unbound refuses rather than")
    story("    inventing an answer or going to the wider internet.")
    story()

    if not (ok_ip and ok_ping):
        sys.exit("a check failed: ip=%s ping=%s" % (ok_ip, ok_ping))

    story("""
the network is left up. Things worth trying:

  ask the resolver by hand, and see the cache at work:
    cloonix cli %s ssh cl "dig @10.0.0.53 www.shop.test"

  watch the exchange:
    cloonix cli %s sniff ns eth0 --dump udp port 53
""" % (NET, NET))

main()
