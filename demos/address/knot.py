#!/usr/bin/env python3
r"""knot - an authoritative server and the zone transfer that feeds a second.

    ./knot.py [net]                the net defaults to nemo

Three machines on one lan::

  kp --- lan1 --- ks
          |
         cl

  kp is the primary authoritative server for shop.test. ks is a secondary:
  it holds no zone file of its own, only the address of the primary. cl is a
  client that can ask either one. When kp is told to serve the zone, it
  notifies ks, ks pulls the whole zone across in a transfer, and from then on
  both answer the same data.

  ACT 1  kp is authoritative for shop.test. cl asks kp for www.shop.test and
         gets the address.
  ACT 2  ks starts, is notified, and transfers the zone from kp. cl now asks
         ks and gets the same answer -- served by a machine that was never
         given the zone by hand.
  ACT 3  the transfer itself is caught on the wire: a zone transfer is DNS
         over TCP, not the UDP a lookup uses.

Image: knot_trixie (knot, kdig)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
  A lookup is UDP 53; a zone transfer (AXFR) is TCP 53, between the two
  servers:

    cloonix cli nemo cnf kp eth0 sniff=on
    cloonix cli nemo sniff kp eth0 --dump tcp port 53

Important
  Nothing is baked into the image: Knot is installed and the demo writes both
  configurations and the zone file at run time.
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
    put, ssh, step, story, tools_present)

HISTORY = """\
Knot DNS is made by CZ.NIC, the Czech domain registry -- the same house as
the BIRD routing daemon -- with a first release around 2011. It is an
authoritative-only server: its job is to hold zones and answer for them, not
to resolve on a client's behalf, which is Unbound's job in the address area
here. It was written to serve very large and very busy zones, and CZ.NIC runs
it for the .cz top-level domain. This demo shows the everyday mechanism that
keeps two authoritative servers holding the same data: notify and transfer.

Today: Knot DNS serves the .cz zone and several other top-level domains and
runs on some root server instances; notify and transfer, shown here, are how
any zone with more than one server stays identical on all of them.
"""

ROOTFS_CHOICES = ("knot_trixie",)

KP, KS, CL = "kp", "ks", "cl"
ALL = (KP, KS, CL)
LAN = "lan1"

WIRING = {
    KP: [(0, LAN, "10.0.0.2/24")],
    KS: [(0, LAN, "10.0.0.3/24")],
    CL: [(0, LAN, "10.0.0.10/24")],
}
GATEWAY = {}

ZONE = """\
$TTL 3600
@       SOA ns.shop.test. admin.shop.test. 1 3600 900 604800 3600
@       NS  ns.shop.test.
ns      A   10.0.0.2
www     A   10.0.0.80
shop    A   10.0.0.81
"""

PRIMARY_CONF = """\
server:
    listen: 0.0.0.0@53
    rundir: "/run/knot"
log:
  - target: syslog
    any: info
remote:
  - id: secondary
    address: 10.0.0.3@53
acl:
  - id: allow_transfer
    address: 10.0.0.3
    action: transfer
zone:
  - domain: shop.test.
    file: "/var/lib/knot/shop.test.zone"
    notify: [secondary]
    acl: [allow_transfer]
"""

SECONDARY_CONF = """\
server:
    listen: 0.0.0.0@53
    rundir: "/run/knot"
log:
  - target: syslog
    any: info
remote:
  - id: primary
    address: 10.0.0.2@53
acl:
  - id: allow_notify
    address: 10.0.0.2
    action: notify
zone:
  - domain: shop.test.
    master: primary
    acl: [allow_notify]
"""

def knot_start(node, conf, zone=None):
    if zone is not None:
        put(node, "/var/lib/knot/shop.test.zone", zone)
    put(node, "/etc/knot/knot.conf", conf)
    ssh(node, "mkdir -p /run/knot")
    ssh(node, "knotd -d -c /etc/knot/knot.conf")
    for _ in range(20):
        if ssh(node, "ss -lun 2>/dev/null | grep -q :53 && echo up",
               check=False).strip():
            return
        time.sleep(1)
    raise RuntimeError("knotd did not start on %s:\n%s"
                       % (node, ssh(node, "tail -n 20 /var/log/syslog",
                                    check=False)))

def kdig(server, name, extra=""):
    return ssh(CL, "kdig @%s %s %s" % (server, name, extra), check=False)

def transferred(timeout=30):
    end = time.time() + timeout
    while time.time() < end:
        r = kdig("10.0.0.3", "www.shop.test", "+short")
        if "10.0.0.80" in r:
            return True
        time.sleep(2)
    return False

LAYOUT = {KP: (-280, 0), LAN: (0, 0), KS: (280, 0), CL: (0, 200)}
ETHS = {(KP, "eth0"): 0, (KS, "eth0"): 180, (CL, "eth0"): 90}

CARD = r"""
   kp --- lan1 --- ks     kp: primary for shop.test
           |              ks: secondary, transfers the zone from kp
          cl              cl: asks either one

  kp is given the zone; ks pulls it across by transfer and answers the same.
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
    tools_present(KP, "knotd")
    tools_present(CL, "kdig")

    step("act 1: kp is authoritative for shop.test")
    story()
    knot_start(KP, PRIMARY_CONF, zone=ZONE)
    r = kdig("10.0.0.2", "www.shop.test", "+short")
    story("    cl asked kp for www.shop.test:")
    story("      %s" % (r.strip() or "(nothing)"))
    ok_primary = "10.0.0.80" in r
    hold()

    step("act 2: ks starts, is notified, and transfers the zone")
    story()
    knot_start(KS, SECONDARY_CONF)
    if not transferred():
        sys.exit("ks never obtained the zone from kp")
    r = kdig("10.0.0.3", "www.shop.test", "+short")
    story("    cl asked ks (never given the zone by hand):")
    story("      %s" % (r.strip() or "(nothing)"))
    ok_secondary = "10.0.0.80" in r
    story()

    step("act 3: catch a transfer on the wire (TCP, not UDP)")
    story()
    got = count_on(KP, 0, "tcp port 53",
                   lambda: (ssh(KS, "knotc zone-retransfer shop.test",
                                check=False), time.sleep(4))[0], settle=4.0)
    if got is not None:
        story("    %d TCP/53 packet(s) crossed kp eth0 for the transfer"
              % got["count"])
    story("    A lookup is one small UDP exchange; moving a whole zone is")
    story("    TCP, which is why a transfer looks different on the wire.")
    story()

    if not (ok_primary and ok_secondary):
        sys.exit("a check failed: primary=%s secondary=%s"
                 % (ok_primary, ok_secondary))

    story("""
the network is left up. Things worth trying:

  ask each server and compare:
    cloonix cli %s ssh cl "kdig @10.0.0.2 shop.shop.test"
    cloonix cli %s ssh cl "kdig @10.0.0.3 shop.shop.test"

  the zone status on the secondary:
    cloonix cli %s ssh ks "knotc zone-status"
""" % (NET, NET, NET))

main()
