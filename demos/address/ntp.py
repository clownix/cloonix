#!/usr/bin/env python3
r"""ntp - a client disciplining its clock to a server with chrony.

    ./ntp.py [net]                 the net defaults to nemo

Two machines on one lan::

  ntpc --- lan1 --- ntps

  ntps runs chrony and serves its own clock as a stratum-8 source. ntpc runs
  chrony as a client pointed at ntps. The demo watches ntpc discover the
  server, mark it reachable, and follow it. chronyd runs with -x: the clock
  of a container is its host's and not its to set, so ntpc measures its
  offset to ntps and steers nothing.

  ACT 1  both start. ntpc's source list shows ntps, at first not yet
         selected; within a few polls it becomes the reference.
  ACT 2  chronyc tracking on ntpc shows it is now synchronised, naming ntps
         as its reference and reporting a small offset.
  ACT 3  the exchange is caught on the wire: NTP is a single request and
         reply, UDP on port 123.

Image: ntp_trixie (chrony)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
  NTP is UDP on port 123, one packet each way:

    cloonix cli nemo cnf ntps eth0 sniff=on
    cloonix cli nemo sniff ntps eth0 --dump udp port 123

Important
  Nothing is baked into the image: chrony is installed and the demo writes
  the server and client configurations at run time.
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
    NET, build, check_net, choose_rootfs, count_on, erase_net, place, put,
    ssh, step, story, tools_present)

HISTORY = """\
chrony was started by Richard Curnow in 1997 and is now maintained mainly by
Miroslav Lichvar at Red Hat. It implements NTP, the Network Time Protocol
whose design goes back to David Mills in the 1980s, and was written to keep
good time on machines that are not always connected and whose clocks drift.

Today: chrony is the default time daemon of Red Hat, Fedora, SUSE and Ubuntu
server; a virtual machine that pauses and resumes, or a laptop that sleeps,
is the case it was written for.
"""

ROOTFS_CHOICES = ("ntp_trixie",)

NTPS, NTPC = "ntps", "ntpc"
ALL = (NTPS, NTPC)
LAN = "lan1"

WIRING = {
    NTPS: [(0, LAN, "10.0.0.2/24")],
    NTPC: [(0, LAN, "10.0.0.10/24")],
}
GATEWAY = {}

SERVER_CONF = """\
local stratum 8
allow 10.0.0.0/8
bindaddress 0.0.0.0
driftfile /var/lib/chrony/drift
pidfile /run/chrony/chronyd.pid
"""

CLIENT_CONF = """\
server 10.0.0.2 iburst minpoll 2 maxpoll 3
driftfile /var/lib/chrony/drift
pidfile /run/chrony/chronyd.pid
makestep 1.0 -1
"""

def chrony_start(node, conf):
    put(node, "/etc/chrony/chrony.conf", conf)
    ssh(node, "mkdir -p /run/chrony")
    # -x: a container may not set the clock, which is its host's.
    # -u root: the files of the image all belong to root inside.
    ssh(node, "chronyd -x -u root -f /etc/chrony/chrony.conf")
    for _ in range(20):
        if ssh(node, "chronyc tracking >/dev/null 2>&1 && echo up",
               check=False).strip():
            return
        time.sleep(1)
    raise RuntimeError("chronyd did not start on %s:\n%s"
                       % (node, ssh(node, "tail -n 20 /var/log/syslog",
                                    check=False)))

def synced(timeout=40):
    end = time.time() + timeout
    while time.time() < end:
        r = ssh(NTPC, "chronyc -n tracking", check=False)
        if "10.0.0.2" in r and "Not synchronised" not in r:
            return True
        ssh(NTPC, "chronyc makestep >/dev/null 2>&1", check=False)
        time.sleep(2)
    return False

LAYOUT = {NTPC: (-260, 0), LAN: (0, 0), NTPS: (260, 0)}
ETHS = {(NTPC, "eth0"): 0, (NTPS, "eth0"): 180}

CARD = r"""
   ntpc --- lan1 --- ntps  (chrony, serves its own clock, stratum 8)

  ntpc points at ntps, finds it, marks it reachable, and disciplines its
  clock to it. NTP is one small request and reply on UDP 123.
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

    step("creating and cabling the two machines")
    build(rootfs)
    tools_present(NTPS, "chronyd", "chronyc")

    step("act 1: the client finds the server")
    story()
    chrony_start(NTPS, SERVER_CONF)
    chrony_start(NTPC, CLIENT_CONF)
    time.sleep(3)
    story("    ntpc's sources:")
    for line in ssh(NTPC, "chronyc -n sources", check=False).strip()\
            .split("\n"):
        story("      %s" % line)
    story()

    step("act 2: the client synchronises to the server")
    story()
    if not synced():
        sys.exit("ntpc never synchronised to ntps")
    story("    ntpc tracking:")
    for line in ssh(NTPC, "chronyc -n tracking", check=False).strip()\
            .split("\n"):
        story("      %s" % line)
    ok_sync = True
    story()

    step("act 3: catch the exchange on the wire")
    story()
    got = count_on(NTPS, 0, "udp port 123",
                   lambda: (ssh(NTPC, "chronyc burst 4/4 >/dev/null 2>&1",
                                check=False), time.sleep(5))[0], settle=5.0)
    if got is not None:
        story("    %d NTP packet(s) crossed ntps eth0 during a burst"
              % got["count"])
    story("    Each is one small request or reply on UDP 123 -- the whole")
    story("    protocol is that round trip, timed very carefully.")
    story()

    if not ok_sync:
        sys.exit("a check failed: sync=%s" % ok_sync)

    story("""
the network is left up. Things to try:

  the client's view of its server and its own clock:
    cloonix cli %s ssh ntpc "chronyc -n sources -v"
    cloonix cli %s ssh ntpc "chronyc tracking"

  watch the time packets:
    cloonix cli %s sniff ntps eth0 --dump udp port 123
""" % (NET, NET, NET))

main()
