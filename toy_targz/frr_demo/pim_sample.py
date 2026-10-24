#!/usr/bin/env python3
r"""pim_sample - Sample configuration (PIM).

    ./pim_sample.py [net]     the net defaults to nemo

The example of the FRR User Guide, page pim, section "Sample
configuration": PIM in source-specific multicast, with its hints.
https://docs.frrouting.org/en/latest/pim.html#sample-configuration

    src ---- lanS ---- r1 ---- lan12 ---- r2 ---- lanR ---- rcv
    10.1.0.10       (first hop)        (last hop)        10.2.0.10

  Following the hints of the guide: "ip pim ssm" on the interface of the
  first hop router facing the source and on the router-to-router links;
  "ip pim ssm" and "ip igmp" together on the interface of r2 facing the
  receiver, so its IGMPv3 membership enters PIM.

  ACT 1  r1 and r2 become PIM neighbours.
  ACT 2  rcv joins (10.1.0.10, 232.1.1.1) with IGMPv3; r2 sends a PIM
         join towards the source; src pings 232.1.1.1 twenty times and
         rcv answers each echo it gets, through an (S,G) state on both
         routers.

Image: frr_trixie (zebra, staticd, pimd)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r2 eth1 sniff=on
    cloonix cli nemo sniff r2 eth1 --dump "igmp"

  The IGMPv3 membership report of rcv: a record for 232.1.1.1 of type
  ALLOW_NEW_SOURCES, listing 10.1.0.10, the one source it wants.

Important
  "ip pim ssm" is the guide's form, a hidden synonym of "ip pim" in frr
  10; ssmpingd of the guide, a troubleshooting helper bound to addresses
  the routers do not have, is left out. The unicast routes that PIM needs
  for its RPF checks are two static routes.
  rcv is a host: its pimd has igmp on eth0, without which pimd refuses a
  join, and holds the join ("ip igmp join-group"), for which its kernel sends
  the IGMPv3 report; it answers the echoes sent to
  the group (icmp_echo_ignore_broadcasts=0), so the ping of src counts
  what reaches it.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
import re

from demolib import NET, count_on, frr_setup, hold, in_parallel, ssh, step, \
    story
from frrlib import forwarding, has, opening, proof, until, verdict, vtysh

HISTORY = """\
IP multicast was the thesis of Steve Deering at Stanford, RFC 1112 in
1989. PIM, written at the IETF by Deborah Estrin, Dino Farinacci, Van
Jacobson and others, routes it independently of the unicast protocol;
source-specific multicast, RFC 4607 in 2006 by Hugh Holbrook and Brad
Cain, drops the rendezvous point when the receiver names the source. The
pimd of frr was written by Everton Marques for Quagga in 2008.

Today: IPTV of the internet providers and the market data feeds of stock
exchanges are the big users of SSM.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

SRC, R1, R2, RCV = "src", "r1", "r2", "rcv"
ALL = (R1, R2, SRC, RCV)
ROUTERS = (R1, R2)
LS, L12, LR = "lanS", "lan12", "lanR"

WIRING = {
    SRC: [(0, LS, "10.1.0.10/24")],
    R1: [(0, L12, "10.0.12.1/24"), (1, LS, "10.1.0.1/24")],
    R2: [(0, L12, "10.0.12.2/24"), (1, LR, "10.2.0.1/24")],
    RCV: [(0, LR, "10.2.0.10/24")],
}
GATEWAY = {SRC: "10.1.0.1", RCV: "10.2.0.1"}

R1_CONF = """\
hostname r1
debug igmp
debug pim
debug pim zebra
ip route 10.2.0.0/24 10.0.12.2
interface eth0
 ip pim ssm
interface eth1
 ip pim ssm"""

R2_CONF = """\
hostname r2
debug igmp
debug pim
debug pim zebra
ip route 10.1.0.0/24 10.0.12.1
interface eth0
 ip pim ssm
interface eth1
 ip pim ssm
 ip igmp"""

RCV_CONF = """\
hostname rcv
interface eth0
 ip igmp
 ip igmp join-group 232.1.1.1 10.1.0.10"""

CONFS = {R1: R1_CONF, R2: R2_CONF}

LAYOUT = {
    SRC: (-520, 0), LS: (-390, 0), R1: (-220, 0), L12: (0, 0),
    R2: (220, 0), LR: (390, 0), RCV: (520, 0),
}
ETHS = {
    (SRC, "eth0"): 90, (R1, "eth1"): 270, (R1, "eth0"): 90,
    (R2, "eth0"): 270, (R2, "eth1"): 90, (RCV, "eth0"): 270,
}

CARD = r"""
    src ---- r1 ==== r2 ---- rcv

  rcv asks for (10.1.0.10, 232.1.1.1); PIM SSM builds the tree back to the
  source, no rendezvous point.
"""

def configure(node):
    forwarding(node)
    frr_setup(node, ["staticd", "pimd"], CONFS[node])

def main():
    opening()
    ssh(RCV, "sysctl -w net.ipv4.icmp_echo_ignore_broadcasts=0 >/dev/null")

    step("act 1: r1 and r2 become PIM neighbours")
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_nbr = until(lambda: has(R2, "show ip pim neighbor", ["10.0.12.1"]), 90)
    proof("r2, its PIM neighbours:", vtysh(R2, "show ip pim neighbor"))
    hold()

    step("act 2: rcv joins (10.1.0.10, 232.1.1.1), src sends, rcv receives")
    got = count_on(R2, 1, "igmp", lambda: (
        frr_setup(RCV, ["pimd"], RCV_CONF),
        until(lambda: has(R2, "show ip igmp sources", ["232.1.1.1"]), 30))[1],
        settle=1.0)
    ok_igmp = got["out"] if got else \
        has(R2, "show ip igmp sources", ["232.1.1.1"])
    if got is not None:
        story("    %d IGMP packet(s) on r2 eth1: the membership report."
              % got["count"])
    proof("rcv, the (S,G) its pimd joined:", vtysh(RCV, "show ip igmp join"))
    proof("r2, the source-specific membership it heard:",
          vtysh(R2, "show ip igmp sources"))
    ok_join = until(lambda: has(R1, "show ip pim join", ["232.1.1.1"]), 60)
    proof("r1, the join r2 sent towards the source:",
          vtysh(R1, "show ip pim join"))
    out = ssh(SRC, "ping -c 20 -i 0.5 -t 8 -W 2 232.1.1.1", check=False)
    got_rx = re.search(r"(\d+) received", out)
    got_rx = int(got_rx.group(1)) if got_rx else 0
    ok_rx = got_rx >= 10
    proof("src, its pings to the group and the answers of rcv:",
          "\n".join(out.strip().split("\n")[-4:]))
    ok_mroute = has(R1, "show ip mroute", ["232.1.1.1"]) and \
        has(R2, "show ip mroute", ["232.1.1.1"])
    proof("r2, its multicast routing table:", vtysh(R2, "show ip mroute"))
    story("    rcv answered %d of the 20 echoes src sent to the group."
          % got_rx)
    story()

    verdict([("r1 and r2 are PIM neighbours", ok_nbr),
             ("r2 hears the IGMPv3 report for (S,G)", ok_igmp),
             ("r1 receives the PIM join", ok_join),
             ("rcv receives the stream (10 of 20 answered at least)", ok_rx),
             ("both routers hold the (S,G) route", ok_mroute)])

    story("""
the network is left up. Things to try:

  the PIM state of the (S,G):
    cloonix cli %s ssh r1 "vtysh -c 'show ip pim state'"

  the multicast routes in the kernel:
    cloonix cli %s ssh r2 "ip mroute show"
""" % (NET, NET))

main()
