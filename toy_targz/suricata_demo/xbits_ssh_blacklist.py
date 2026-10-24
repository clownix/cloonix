#!/usr/bin/env python3
r"""xbits_ssh_blacklist - xbits: Creating a SSH blacklist.

    ./xbits_ssh_blacklist.py [net]     the net defaults to nemo

The example of the Suricata User Guide, chapter "Xbits", section
"Examples", "Creating a SSH blacklist": two rules drop the SSH clients of
bots and put their address on the list badssh for an hour; a third drops
anything from an address on the list.
https://docs.suricata.io/en/suricata-7.0.10/rules/xbits.html#creating-a-ssh-blacklist

    cl (10.0.0.1) ---- lanA ---- sen (inline IPS) ---- lanB ---- srv (10.0.0.2)
    cl2 (10.0.0.3) ---+                                          $MYSERVER, ssh 22

  Suricata runs inline on sen, in the AF_PACKET IPS mode of the guide, with
  the three rules word for word; $MYSERVER is srv. srv runs a small SSH
  listener that writes down the client banners it receives.

  ACT 1  cl connects with an OpenSSH client banner: srv receives it.
  ACT 2  cl connects as "PUTTY": dropped, sid 4000000007, and cl is put on
         the list badssh.
  ACT 3  cl connects again as OpenSSH: dropped all the same, sid
         4000000006, the address is on the list; cl2, not on it, still gets
         through.

Image: suricata_trixie (suricata, curl, python3)
Size:  4 containers, 2 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf sen eth1 sniff=on
    cloonix cli nemo sniff sen eth1 --dump "tcp port 22"

  The banner "SSH-2.0-PUTTY" of cl leaves cl on lanA and never appears on
  lanB: Suricata dropped it between the two.

Important
  $MYSERVER, which the guide leaves to the reader, is set to srv with
  --set vars.address-groups.MYSERVER.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and suricatalib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, hold, ssh, step, story
from suricatalib import count, fast, listening, opening, proof, put_any, \
    suricata_start, until, verdict

HISTORY = """\
Flowbits let one rule set a mark on a flow for another rule; xbits, in
Suricata 3.0 in 2016, by Victor Julien, carry the mark on a host or a pair
of hosts, with an expiry, so a decision taken on one connection applies to
the next ones.

Suricata is made by the Open Information Security Foundation, OISF,
founded in 2009 with funding of the US Department of Homeland Security; its
first release came in 2010, written by Victor Julien and others,
multi-threaded from the start, its rule language broadly the one of Snort.
"""

ROOTFS_CHOICES = ("suricata_trixie",)
TOOL = "suricata"

CL, SEN, SRV, CL2 = "cl", "sen", "srv", "cl2"
ALL = (SEN, CL, SRV, CL2)
LA, LB = "lanA", "lanB"

WIRING = {
    CL: [(0, LA, "10.0.0.1/24")],
    SEN: [(0, LA, None), (1, LB, None)],
    SRV: [(0, LB, "10.0.0.2/24")],
    CL2: [(0, LA, "10.0.0.3/24")],
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

LAYOUT = {CL: (-440, -100), CL2: (-440, 100), LA: (-220, 0), SEN: (0, 0),
          LB: (220, 0), SRV: (440, 0)}
ETHS = {(CL, "eth0"): 135, (CL2, "eth0"): 45, (SEN, "eth0"): 270,
        (SEN, "eth1"): 90, (SRV, "eth0"): 270}

# The three rules of the guide.
RULES = """\
drop ssh any any -> $MYSERVER 22 (msg:"DROP libssh incoming"; flow:to_server,established; ssh.softwareversion:"libssh"; xbits:set, badssh, track ip_src, expire 3600; sid:4000000005;)
drop ssh any any -> $MYSERVER 22 (msg:"DROP PUTTY incoming"; flow:to_server,established; ssh.softwareversion:"PUTTY"; xbits:set, badssh, track ip_src, expire 3600; sid:4000000007;)
drop ssh any any -> $MYSERVER 22 (msg:"DROP BLACKLISTED"; xbits:isset, badssh, track ip_src; sid:4000000006;)
"""
SETS = IPS + ("vars.address-groups.MYSERVER=10.0.0.2",)

# An SSH listener: its banner, then the client banner written down.
SERVER = """\
import socket, threading
def serve(c, addr):
    c.settimeout(10)
    c.sendall(b"SSH-2.0-OpenSSH_9.2\\r\\n")
    try:
        got = c.recv(256).decode(errors="replace").strip()
    except OSError:
        got = ""
    if got:
        with open("/tmp/banners.log", "a") as f:
            f.write(addr[0] + " " + got + "\\n")
    c.close()
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("0.0.0.0", 22))
s.listen(16)
while True:
    c, addr = s.accept()
    threading.Thread(target=serve, args=(c, addr), daemon=True).start()
"""

# A client: reads the banner of the server, sends its own.
CLIENT = """\
import socket, sys, time
c = socket.create_connection(("10.0.0.2", 22), timeout=5)
c.recv(256)
c.sendall(("SSH-2.0-" + sys.argv[1] + "\\r\\n").encode())
time.sleep(2)
c.close()
"""

CARD = r"""
    cl, cl2 ---- [sen, inline] ---- srv ($MYSERVER, ssh)

  A bot client is dropped and listed; the list drops what follows from it.
"""

def banners():
    return ssh(SRV, "cat /tmp/banners.log 2>/dev/null", check=False)

def connect(node, version):
    ssh(node, "python3 /tmp/client.py %s" % version, check=False)

def main():
    opening()
    put_any(SRV, "/tmp/server.py", SERVER)
    ssh(SRV, "python3 /tmp/server.py </dev/null >/tmp/server.out 2>&1 &")
    listening(SRV, 22, "/tmp/server.out")
    for node in (CL, CL2):
        put_any(node, "/tmp/client.py", CLIENT)

    step("act 1: an OpenSSH client banner reaches srv")
    suricata_start(SEN, RULES, "--af-packet", SETS)
    connect(CL, "OpenSSH_9.6")
    ok_open = until(lambda: "10.0.0.1 SSH-2.0-OpenSSH_9.6" in banners(), 20)
    proof("srv, the banners it received:", banners())
    hold()

    step("act 2: a PUTTY banner is dropped, cl goes on the list badssh")
    got = count_on(SEN, 1, "tcp port 22", lambda: connect(CL, "PUTTY"),
                   settle=1.0)
    if got is not None:
        story("    %d packet(s) of port 22 reached eth1 of sen." % got["count"])
    ok_putty = until(lambda: count(SEN, 4000000007) >= 1, 20) and \
        "PUTTY" not in banners()
    proof("sen, fast.log:", fast(SEN))
    hold()

    step("act 3: cl as OpenSSH, dropped for being listed; cl2 passes")
    connect(CL, "OpenSSH_9.7")
    ok_listed = until(lambda: count(SEN, 4000000006) >= 1, 20) and \
        "OpenSSH_9.7" not in banners()
    connect(CL2, "OpenSSH_9.7")
    ok_other = until(lambda: "10.0.0.3 SSH-2.0-OpenSSH_9.7" in banners(), 20)
    proof("srv, the banners it received:", banners())
    proof("sen, fast.log:", fast(SEN))

    verdict([("an OpenSSH banner from cl reaches srv", ok_open),
             ("PUTTY is dropped, sid 4000000007", ok_putty),
             ("cl, listed, is dropped as OpenSSH, sid 4000000006", ok_listed),
             ("cl2, not listed, still reaches srv", ok_other)])

    story("""
the network is left up. Things to try:

  the hosts and their xbits, dumped from the host table:
    cloonix cli %s ssh sen "grep -i xbit /var/log/suricata/eve.json | tail"

  cl as libssh, the other bot rule:
    cloonix cli %s ssh cl "python3 /tmp/client.py libssh_0.10"
""" % (NET, NET))

main()
