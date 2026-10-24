#!/usr/bin/env python3
r"""tinc - a small mesh vpn where each node knows the others.

    ./tinc.py [net]                the net defaults to nemo

Two nodes on one underlay lan::

  na --- lan1 --- nb
  10.0.0.1       10.0.0.2

tinc joins them into a VPN named vpn, each with a tun interface on a private
10.9.0.0/24. Every node has a host file describing itself, its public key
in it; the nodes exchange those files and connect. A third node would mesh
with both.

  ACT 1  each node makes its key pair, the host files are exchanged, both
         start: the tun interfaces come up, na 10.9.0.1, nb 10.9.0.2.
  ACT 2  na pings nb over the VPN; on the underlay the traffic is a tinc
         flow on UDP 655, not readable ICMP.
  ACT 3  the two counts: UDP/655 present, ICMP in clear absent.

Image: tinc_trixie (tinc)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
  The data and metadata of tinc are UDP and TCP port 655, the ping inside
  not visible:

    cloonix cli nemo cnf na eth0 sniff=on
    cloonix cli nemo sniff na eth0 --dump port 655
    cloonix cli nemo sniff na eth0 --dump icmp

Important
  tinc needs a tun device, given to the container when the host has it: on
  the host, once per boot, then the demo as a user:

    sudo modprobe tun

  Keys, host files and configuration are made at run time.
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
    NET, build, check_net, choose_rootfs, count_on, erase_net, in_parallel,
    place, put, reached, run, ssh, step, story, tools_present, unavailable)

HISTORY = """\
tinc was started by Guus Sliepen and Ivo Timmermans in the late 1990s. Its
distinctive idea is the automatic full mesh: tell each node how to reach one
or two others and tinc works out direct paths between every pair, routing
around a node that goes down: a peer-to-peer VPN, where a classic OpenVPN
server is a hub with spokes.

Today: tinc links small fleets of servers and home networks where no node
can be the hub; its mesh idea is the one later tools such as ZeroTier,
Nebula and Tailscale built products on.
"""

ROOTFS_CHOICES = ("tinc_trixie",)

NA, NB = "na", "nb"
ALL = (NA, NB)
LAN = "lan1"
VPN = "vpn"
UNDER = {NA: "10.0.0.1", NB: "10.0.0.2"}
INNER = {NA: "10.9.0.1", NB: "10.9.0.2"}

WIRING = {
    NA: [(0, LAN, "10.0.0.1/24")],
    NB: [(0, LAN, "10.0.0.2/24")],
}
GATEWAY = {}

def tinc_conf(node, other):
    return ("Name = %s\nAddressFamily = ipv4\nInterface = tun0\n"
            "ConnectTo = %s\n" % (node, other))

def host_file(node):
    return "Address = %s\nSubnet = %s/32\n" % (UNDER[node], INNER[node])

def tinc_up(node):
    return ("#!/bin/sh\nip link set $INTERFACE up\n"
            "ip addr add %s/24 dev $INTERFACE\n" % INNER[node])

def prepare(node, other):
    base = "/etc/tinc/%s" % VPN
    ssh(node, "mkdir -p %s/hosts" % base)
    put(node, "%s/tinc.conf" % base, tinc_conf(node, other))
    put(node, "%s/hosts/%s" % (base, node), host_file(node))
    put(node, "%s/tinc-up" % base, tinc_up(node))
    ssh(node, "chmod +x %s/tinc-up" % base)
    # -K generates the key pair and appends the public key to the host file.
    ssh(node, "printf '\\n\\n' | tincd -n %s -K 2048 >/dev/null 2>&1" % VPN,
        check=False)

def exchange():
    for node, other in ((NA, NB), (NB, NA)):
        text = ssh(node, "cat /etc/tinc/%s/hosts/%s" % (VPN, node),
                   check=False)
        put(other, "/etc/tinc/%s/hosts/%s" % (VPN, node), text)

def start(node):
    ssh(node, "tincd -n %s" % VPN)

def tunnel_ready(timeout=45):
    end = time.time() + timeout
    while time.time() < end:
        r = run('cloonix cli %s ssh %s "ping -c1 -W1 10.9.0.2"'
                % (NET, NA), quiet=True)
        if r.returncode == 0:
            return True
        time.sleep(2)
    return False

def ensure_tun(node):
    r = ssh(node, "test -c /dev/net/tun && echo yes || echo no", check=False)
    if "yes" not in r:
        unavailable("%s has no /dev/net/tun.\n"
                 "  the container is given this node whenever the host has it.\n"
                 "  On the host, once per boot, load the module as yourself:\n"
                 "    sudo modprobe tun\n"
                 "  then start this demo again as your normal user." % node)

LAYOUT = {NA: (-260, 0), LAN: (0, 0), NB: (260, 0)}
ETHS = {(NA, "eth0"): 0, (NB, "eth0"): 180}

CARD = r"""
   na --- lan1 --- nb      underlay 10.0.0.0/24
 tun0 10.9.0.1   tun0 10.9.0.2   (VPN "vpn")

  Each node has a host file with its public key; they exchange them and
  connect. The ping over the VPN rides UDP 655 and is not readable.
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
    tools_present(NA, "tincd")
    for n in ALL:
        ensure_tun(n)

    step("act 1: keys, host files exchanged, both up")
    story()
    prepare(NA, NB)
    prepare(NB, NA)
    exchange()
    in_parallel(start, list(ALL), "tinc start")
    if not tunnel_ready(90):
        sys.exit("the VPN never came up: na could not reach 10.9.0.2\n%s"
                 % ssh(NA, "ip -br addr show tun0; tail -n 20 /var/log/syslog",
                       check=False))
    story("    tun0 on na:")
    for line in ssh(NA, "ip -br addr show tun0", check=False).strip()\
            .split("\n"):
        story("      %s" % line)
    story()

    step("act 2: ping over the VPN, watched on the underlay")
    story()
    r = ssh(NA, "ping -c 5 10.9.0.2", check=False)
    story("    na -> nb over the VPN:")
    story(r.strip())
    ok_ping = reached(r)
    story()

    step("act 3: what the underlay carried")
    story()
    p655 = count_on(NA, 0, "port 655",
                    lambda: ssh(NA, "ping -c 5 10.9.0.2", check=False),
                    settle=3.0)
    clr = count_on(NA, 0, "icmp",
                   lambda: ssh(NA, "ping -c 5 10.9.0.2", check=False),
                   settle=3.0)
    pc = p655["count"] if p655 else -1
    cc = clr["count"] if clr else -1
    story("    on the underlay while pinging over the VPN:")
    story("      port 655 packets       : %d" % pc)
    story("      cleartext ICMP packets : %d" % cc)
    story()
    ok_tinc = pc > 0
    ok_hidden = cc == 0
    story()

    if not (ok_ping and ok_tinc and ok_hidden):
        sys.exit("a check failed: ping=%s tinc=%s hidden=%s"
                 % (ok_ping, ok_tinc, ok_hidden))

    story("""
the network is left up. Things to try:

  the tunnel interface and a ping over it:
    cloonix cli %s ssh na "ip addr show tun0"
    cloonix cli %s ssh na "ping -c3 10.9.0.2"

  the tinc flow on the underlay:
    cloonix cli %s sniff na eth0 --dump port 655
""" % (NET, NET, NET))

main()
