#!/usr/bin/env python3
r"""openvpn - a point-to-point tunnel with one shared key.

    ./openvpn.py [net]             the net defaults to nemo

Two machines on one underlay lan::

  ovs --- lan1 --- ovc
  10.0.0.1        10.0.0.2

  OpenVPN joins them with a tun device at each end, carrying a private
  10.8.0.0/24 inside a single UDP flow on the underlay. This is the simplest
  OpenVPN there is: no certificates, one pre-shared static key, two peers.

  ACT 1  the key is generated on ovs and copied to ovc, both start OpenVPN,
         and the tun interfaces come up: ovs is 10.8.0.1, ovc is 10.8.0.2.
  ACT 2  ovs pings ovc over the tunnel. It works, and on the underlay the
         traffic is a UDP flow on port 1194, not readable ICMP.
  ACT 3  the two counts are shown: UDP/1194 present on the wire, cleartext
         ICMP absent -- the tunnel is doing its job.

Image: openvpn_trixie (openvpn, easy-rsa)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
  The tunnel is UDP on port 1194; the ping inside is not visible in clear:

    cloonix cli nemo cnf ovs eth0 sniff=on
    cloonix cli nemo sniff ovs eth0 --dump udp port 1194
    cloonix cli nemo sniff ovs eth0 --dump icmp

Important
  OpenVPN needs a tun device. The container is given /dev/net/tun whenever the
  host has it, so on the host, once per boot, load the module as yourself:

    sudo modprobe tun

  then run this demo as your normal user -- no need to run the net as root.
  Nothing else is baked in: the key and both invocations are made at run time.
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
    reached, run, ssh, step, story, tools_present)

HISTORY = """\
OpenVPN was written by James Yonan and first released in 2001. It runs
entirely in user space on top of a tun or tap device and uses the OpenSSL
library for its cryptography, which made it easy to port and easy to run
through NAT and firewalls -- it just looks like a UDP flow. For two decades it
was the default do-it-yourself VPN before WireGuard arrived; the wg demo in
this same area is the deliberate modern comparison. Here it is shown in its
oldest and simplest mode: two peers and a single shared key.
"""

ROOTFS_CHOICES = ("openvpn_trixie",)

OVS, OVC = "ovs", "ovc"
ALL = (OVS, OVC)
LAN = "lan1"
COMMON_OPTS = ("--dev tun0 --secret /etc/openvpn/static.key "
               "--cipher AES-256-CBC --auth SHA256 --proto udp --port 1194 "
               "--daemon --log /var/log/openvpn/ovpn.log")

WIRING = {
    OVS: [(0, LAN, "10.0.0.1/24")],
    OVC: [(0, LAN, "10.0.0.2/24")],
}
GATEWAY = {}

def ensure_tun(node):
    r = ssh(node, "test -c /dev/net/tun && echo yes || echo no", check=False)
    if "yes" not in r:
        sys.exit("%s has no /dev/net/tun.\n"
                 "  the container is given this node whenever the host has it.\n"
                 "  On the host, once per boot, load the module as yourself:\n"
                 "    sudo modprobe tun\n"
                 "  then start this demo again as your normal user." % node)

def start_server():
    ssh(OVS, "openvpn --ifconfig 10.8.0.1 10.8.0.2 " + COMMON_OPTS)

def start_client():
    ssh(OVC, "openvpn --remote 10.0.0.1 --ifconfig 10.8.0.2 10.8.0.1 "
             + COMMON_OPTS)

def tunnel_ready(timeout=40):
    end = time.time() + timeout
    while time.time() < end:
        r = run('cloonix cli %s ssh %s "ping -c1 -W1 10.8.0.2"'
                % (NET, OVS), quiet=True)
        if r.returncode == 0:
            return True
        time.sleep(2)
    return False

LAYOUT = {OVS: (-260, 0), LAN: (0, 0), OVC: (260, 0)}
ETHS = {(OVS, "eth0"): 0, (OVC, "eth0"): 180}

CARD = r"""
   ovs --- lan1 --- ovc     underlay 10.0.0.0/24
 tun0 10.8.0.1    tun0 10.8.0.2

  One shared key, two peers. The private 10.8.0.0/24 rides inside a single
  UDP/1194 flow; the ping over it is not readable on the underlay.
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
    tools_present(OVS, "openvpn")
    for n in ALL:
        ensure_tun(n)

    step("act 1: one key, two peers, tun up on each")
    story()
    ssh(OVS, "openvpn --genkey secret /etc/openvpn/static.key")
    key = ssh(OVS, "cat /etc/openvpn/static.key", check=False)
    put(OVC, "/etc/openvpn/static.key", key)
    ssh(OVC, "chmod 600 /etc/openvpn/static.key")
    start_server()
    start_client()
    if not tunnel_ready():
        sys.exit("the tunnel never came up: ovs could not reach 10.8.0.2")
    story("    tun0 on ovs:")
    for line in ssh(OVS, "ip -br addr show tun0", check=False).strip()\
            .split("\n"):
        story("      %s" % line)
    story()

    step("act 2: ping over the tunnel, watched on the underlay")
    story()
    r = ssh(OVS, "ping -c 5 10.8.0.2", check=False)
    story("    ovs -> ovc over the tunnel:")
    story(r.strip())
    ok_ping = reached(r)
    story()

    step("act 3: what the underlay actually carried")
    story()
    udp = count_on(OVS, 0, "udp port 1194",
                   lambda: ssh(OVS, "ping -c 5 10.8.0.2", check=False),
                   settle=3.0)
    clr = count_on(OVS, 0, "icmp",
                   lambda: ssh(OVS, "ping -c 5 10.8.0.2", check=False),
                   settle=3.0)
    uc = udp["count"] if udp else -1
    cc = clr["count"] if clr else -1
    story("    on the underlay while pinging over the tunnel:")
    story("      UDP/1194 packets       : %d" % uc)
    story("      cleartext ICMP packets : %d" % cc)
    story()
    story("    The ping crosses, but the underlay sees only the UDP flow;")
    story("    the ICMP is inside it and not readable.")
    ok_udp = uc > 0
    ok_hidden = cc == 0
    story()

    if not (ok_ping and ok_udp and ok_hidden):
        sys.exit("a check failed: ping=%s udp=%s hidden=%s"
                 % (ok_ping, ok_udp, ok_hidden))

    story("""
the network is left up. Things worth trying:

  the tunnel interface and a ping over it:
    cloonix cli %s ssh ovs "ip addr show tun0"
    cloonix cli %s ssh ovs "ping -c3 10.8.0.2"

  see the UDP flow on the underlay:
    cloonix cli %s sniff ovs eth0 --dump udp port 1194
""" % (NET, NET, NET))

main()
