#!/usr/bin/env python3
r"""afpacket_ips - Setting up IPS/inline for Linux: AF_PACKET IPS mode.

    ./afpacket_ips.py [net]     the net defaults to nemo

The configuration of the Suricata User Guide, chapter "Setting up
IPS/inline for Linux", section "AF_PACKET IPS mode": Suricata between eth0
and eth1, copying each packet from one to the other, and dropping those a
drop rule matches.
https://docs.suricata.io/en/suricata-7.0.10/setting-up-ipsinline-for-linux.html#af-packet-ips-mode

    cl (10.0.0.1) ---- lanA ---- sen ---- lanB ---- srv (10.0.0.2)
                                eth0  eth1
                          (no address, no bridge:
                           Suricata copies the packets)

  sen has no address and no bridge: only Suricata carries the frames from
  one cable to the other, with the af-packet section of the guide, word
  for word, and stream.inline set as the guide asks.

  ACT 1  without Suricata, cl cannot reach srv; with it and an alert rule,
         the ping crosses and is seen.
  ACT 2  the rule becomes a drop rule: the ping no longer crosses, eve.json
         marks it blocked, while the web page of srv still comes through.

Image: suricata_trixie (suricata, curl, python3)
Size:  3 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf sen eth1 sniff=on
    cloonix cli nemo sniff sen eth1 --dump "icmp"

  In ACT 1 every echo reaches eth1, copied by Suricata from eth0; in ACT 2
  none does: the drop happens between the two cables.

Important
  The guide warns that offloads break this mode: cloonix makes the eths of a
  container with no checksum or segmentation offload, each frame finished
  and within the mtu, as on a cable.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and suricatalib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, hold, reached, ssh, step, story
from suricatalib import count, opening, ping, pings, proof, \
    suricata_start, suricata_stop, until, verdict, web_server

HISTORY = """\
Suricata could sit in the path since its first versions, through
netfilter queues; the AF_PACKET IPS mode, by Eric Leblond in Suricata 1.4
in 2013, does it with no firewall at all: two interfaces, and the packets
copied from one to the other by Suricata itself.

Suricata is made by the Open Information Security Foundation, OISF,
founded in 2009 with funding of the US Department of Homeland Security; its
first release came in 2010, written by Victor Julien and others,
multi-threaded from the start, its rule language broadly the one of Snort.
"""

ROOTFS_CHOICES = ("suricata_trixie",)
TOOL = "suricata"

CL, SEN, SRV = "cl", "sen", "srv"
ALL = (SEN, CL, SRV)
LA, LB = "lanA", "lanB"

WIRING = {
    CL: [(0, LA, "10.0.0.1/24")],
    SEN: [(0, LA, None), (1, LB, None)],
    SRV: [(0, LB, "10.0.0.2/24")],
}
GATEWAY = {}

# The af-packet section of the guide, eth0 and eth1, as --set keys.
def af_packet(n, iface, other, cid):
    return ["af-packet.%d.%s=%s" % (n, k, v) for k, v in (
        ("interface", iface), ("threads", 1), ("defrag", "no"),
        ("cluster-type", "cluster_flow"), ("cluster-id", cid),
        ("copy-mode", "ips"), ("copy-iface", other),
        ("buffer-size", 64535), ("use-mmap", "yes"))]
IPS = tuple(af_packet(0, "eth0", "eth1", 98) + af_packet(1, "eth1", "eth0", 97)
            + ["stream.inline=yes"])

LAYOUT = {CL: (-440, 0), LA: (-220, 0), SEN: (0, 0), LB: (220, 0),
          SRV: (440, 0)}
ETHS = {(CL, "eth0"): 90, (SEN, "eth0"): 270, (SEN, "eth1"): 90,
        (SRV, "eth0"): 270}

ALERT = """\
alert icmp any any -> any any (msg:"ICMP seen inline"; sid:1000001; rev:1;)
"""
DROP = """\
drop icmp any any -> any any (msg:"ICMP dropped inline"; sid:1000002; rev:1;)
"""

CARD = r"""
    cl ---- [eth0  sen  eth1] ---- srv
             Suricata copies, or drops

  An IPS with no firewall: Suricata is the only path between two cables.
"""

def main():
    opening()
    web_server(SRV, {"index.html": "the page of srv\n"})

    step("act 1: no path without Suricata; with it, the ping crosses")
    suricata_stop(SEN)
    ok_cut = until(lambda: not pings(CL, "10.0.0.2"), 30)
    story("    without Suricata: cl does not reach srv.")
    suricata_start(SEN, ALERT, "--af-packet", IPS)
    got = count_on(SEN, 1, "icmp", lambda: until(
        lambda: pings(CL, "10.0.0.2"), 30), settle=1.0)
    ok_cross = got["out"] if got else pings(CL, "10.0.0.2")
    proof("cl -> srv, through Suricata:", ping(CL, "10.0.0.2", count=3))
    ok_seen = until(lambda: count(SEN, 1000001) >= 1, 20)
    hold()

    step("act 2: a drop rule, the ping stops, the web page passes")
    suricata_start(SEN, DROP, "--af-packet", IPS)
    r = ping(CL, "10.0.0.2", count=3)
    proof("cl -> srv, the echo dropped:", r)
    ok_drop = not reached(r)
    ok_blocked = until(lambda: count(SEN, 1000002) >= 1, 20) and \
        "blocked" in ssh(SEN, "grep 1000002 /var/log/suricata/eve.json "
                              "| tail -1", check=False)
    page = ssh(CL, "curl -s -m 10 http://10.0.0.2/index.html", check=False)
    proof("cl, the page of srv:", page)
    ok_http = "the page of srv" in page

    verdict([("no path through sen without Suricata", ok_cut),
             ("Suricata carries the ping between the cables", ok_cross),
             ("the alert rule sees it", ok_seen),
             ("the drop rule stops the ping", ok_drop),
             ("eve.json marks it blocked", ok_blocked),
             ("the web traffic still crosses", ok_http)])

    story("""
the network is left up. Things to try:

  the drops in eve.json:
    cloonix cli %s ssh sen "grep blocked /var/log/suricata/eve.json"

  the counters of the two interfaces:
    cloonix cli %s ssh sen "grep -i capture /var/log/suricata/stats.log | tail"
""" % (NET, NET))

main()
