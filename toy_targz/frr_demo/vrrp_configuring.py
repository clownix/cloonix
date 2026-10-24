#!/usr/bin/env python3
r"""vrrp_configuring - Configuring VRRP (needs: sudo modprobe macvlan).

    ./vrrp_configuring.py [net]     the net defaults to nemo

The example of the FRR User Guide, page vrrp, section "Configuring VRRP":
its "System Configuration" (macvlan devices made with iproute2) and its
"Interface Configuration" (virtual router 5, version 3).
https://docs.frrouting.org/en/latest/vrrp.html#vrrp-configuration

    r1 (10.0.2.15, priority 200) ----+
                                     +---- lan0 ---- h (10.0.2.100)
    r2 (10.0.2.14, priority 100) ----+

    virtual router 5:  10.0.2.16  and  2001:db8::370:7334

  !!! KERNEL MODULE NEEDED ON THE HOST: macvlan !!!
  The backed-up addresses live on macvlan devices with the virtual MAC,
  and a container cannot load a module. Before playing this demo, on the
  host, once per boot:

sudo modprobe macvlan

  or do not run this demo. It stops with a big warning if the module is
  missing.

  ACT 1  r1, priority 200, is Master for IPv4 and IPv6, r2 is Backup; h
         reaches both virtual addresses.
  ACT 2  the cable of r1 is cut: r2 becomes Master, h still reaches the
         virtual addresses, through the same virtual MAC.
  ACT 3  the cable comes back: r1 preempts and is Master again.

Image: frr_trixie (zebra, vrrpd)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf h eth0 sniff=on
    cloonix cli nemo sniff h eth0 --dump "ip proto 112"

  A VRRPv3 advertisement, IP protocol 112 to 224.0.0.18, source MAC
  00:00:5e:00:01:05: the virtual MAC of router 5, priority 200 until the
  cut, 100 after.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, frr_setup, hold, in_parallel, reached, \
    ssh, step, story
from frrlib import opening, ping, pings, proof, until, verdict, vtysh

HISTORY = """\
VRRP comes from the HSRP of Cisco, written by Tony Li and others in 1994;
the IETF standard, RFC 2338 in 1998 by Steven Knight and colleagues, made
it open; VRRPv3, RFC 5798 by Stephen Nadas in 2010, added IPv6. The vrrpd
of frr was written by Quentin Young of Cumulus Networks in 2019, on linux
macvlan devices and protodown.

Today: every network where hosts have one default gateway keeps two
routers behind it with VRRP or one of its cousins.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

R1, R2, H = "r1", "r2", "h"
ALL = (R1, R2, H)
ROUTERS = (R1, R2)
L0 = "lan0"

WIRING = {
    R1: [(0, L0, "10.0.2.15/24")],
    R2: [(0, L0, "10.0.2.14/24")],
    H: [(0, L0, "10.0.2.100/24")],
}
GATEWAY = {}
VIP4, VIP6 = "10.0.2.16", "2001:db8::370:7334"

# The guide's iproute2 commands of the System Configuration, addrgenmode
# set after the add: given with it, a kernel that checks the ipv6 part
# before the device exists answers "Address family not supported".
MACVLANS = (
    "ip link add vrrp4-2-1 link eth0 type macvlan mode bridge",
    "ip link set dev vrrp4-2-1 addrgenmode random",
    "ip link set dev vrrp4-2-1 address 00:00:5e:00:01:05",
    "ip addr add 10.0.2.16/24 dev vrrp4-2-1",
    "ip link set dev vrrp4-2-1 up",
    "ip link add vrrp6-2-1 link eth0 type macvlan mode bridge",
    "ip link set dev vrrp6-2-1 addrgenmode random",
    "ip link set dev vrrp6-2-1 address 00:00:5e:00:02:05",
    "ip addr add 2001:db8::370:7334/64 dev vrrp6-2-1",
    "ip link set dev vrrp6-2-1 up")

def frr_conf(node):
    c = ["hostname %s" % node, "interface eth0", " vrrp 5 version 3"]
    if node == R1:
        c.append(" vrrp 5 priority 200")
    c += [" vrrp 5 advertisement-interval 1500",
          " vrrp 5 ip 10.0.2.16",
          " vrrp 5 ipv6 2001:0db8::0370:7334"]
    return "\n".join(c)

LAYOUT = {R1: (-300, -150), R2: (-300, 150), L0: (-60, 0), H: (220, 0)}
ETHS = {(R1, "eth0"): 135, (R2, "eth0"): 45, (H, "eth0"): 270}

CARD = r"""
    r1 (200) --+
               +-- lan0 -- h
    r2 (100) --+

  One virtual router, 10.0.2.16 and 2001:db8::370:7334, two real ones.
"""

def configure(node):
    for line in MACVLANS:
        ssh(node, line)
    frr_setup(node, ["vrrpd"], frr_conf(node))

def state(node, word):
    """Status (v4) and Status (v6) of virtual router 5 are both word."""
    rows = [l.replace("|", " ").split()
            for l in vtysh(node, "show vrrp 5").split("\n")]
    return all(any(r[:2] == ["Status", fam] and r[-1] == word for r in rows)
               for fam in ("(v4)", "(v6)"))

def reach():
    return pings(H, VIP4) and pings(H, VIP6)

def main():
    opening("macvlan")
    ssh(H, "ip addr add 2001:db8::100/64 dev eth0")

    step("act 1: r1 is Master, r2 is Backup, h reaches the virtual addresses")
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_roles = until(lambda: state(R1, "Master") and state(R2, "Backup"), 60)
    proof("r1, virtual router 5:", vtysh(R1, "show vrrp 5"))
    proof("r2, virtual router 5:", vtysh(R2, "show vrrp 5 summary"))
    ok_reach = until(reach, 30)
    proof("h -> %s:" % VIP4, ping(H, VIP4))
    proof("h -> %s:" % VIP6, ping(H, VIP6, count=3))
    hold()

    step("act 2: the cable of r1 is cut, r2 becomes Master")
    got = count_on(H, 0, "ip proto 112", lambda: (
        ssh(R1, "ip link set eth0 down"),
        until(lambda: state(R2, "Master"), 30, period=1))[1], settle=1.0)
    ok_takeover = got["out"] if got else state(R2, "Master")
    if got is not None:
        story("    %d VRRP advertisement(s) seen by h meanwhile."
              % got["count"])
    proof("r2, virtual router 5:", vtysh(R2, "show vrrp 5 summary"))
    ok_still = until(reach, 30)
    r = ping(H, VIP4)
    proof("h -> %s, answered by r2:" % VIP4, r)
    ok_still = ok_still and reached(r)
    hold()

    step("act 3: the cable comes back, r1 preempts")
    ssh(R1, "ip link set eth0 up")
    ssh(R1, "ip addr add 10.0.2.15/24 dev eth0", check=False)
    ok_back = until(lambda: state(R1, "Master") and state(R2, "Backup"), 60)
    proof("r1, virtual router 5:", vtysh(R1, "show vrrp 5 summary"))

    verdict([("r1 Master, r2 Backup, for IPv4 and IPv6", ok_roles),
             ("h reaches 10.0.2.16 and 2001:db8::370:7334", ok_reach),
             ("r2 takes over when r1 is cut", ok_takeover),
             ("h still reaches the virtual addresses", ok_still),
             ("r1 preempts when it comes back", ok_back)])

    story("""
the network is left up. Things to try:

  the macvlan of the Backup, held in protodown:
    cloonix cli %s ssh r2 "ip -d link show vrrp4-2-1"

  the advertisements:
    cloonix cli %s sniff h eth0 --dump "ip proto 112"
""" % (NET, NET))

main()
