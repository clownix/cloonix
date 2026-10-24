#!/usr/bin/env python3
r"""vlan - one wire, two networks, and a filter that stops working.

    ./vlan.py [net]                 the net defaults to nemo

Five machines on one lan::

    a10                                                    b10
  10.0.10.1 --+                                    +-- 10.0.10.2
              |                                    |
              +------------ lan_trunk -------------+
              |                 |                  |
    a20       |               rtr                  |       b20
  10.0.20.1 --+          10.0.10.254               +-- 10.0.20.2
                         10.0.20.254

eth0 carries no address on any of them: the addresses are on the vlan
interfaces eth0.10 and eth0.20, and the cloonix lan carries the tagged
frames untouched. That is a trunk.

  ACT 1  a10 reaches b10, a20 reaches b20, over the same wire.
  ACT 2  a20 is given 10.0.10.9 as well, same wire, same subnet as a10,
         and still unreachable: the tag is the network, not the subnet.
  ACT 3  the tag read out of the raw frame: 0x8100, priority, vlan id.
  ACT 4  a router on a stick: rtr forwards vlan 10 to vlan 20 through one
         interface, the same ping caught arriving tagged 10 and leaving
         tagged 20.

Image: trixie            (the plain one: iproute2 and ping)
Size:  5 containers, 1 lan

THE PACKET TO CATCH
-------------------
  The trunk, a10 pinging b10:

    cloonix cli nemo cnf a10 eth0 sniff=on
    cloonix cli nemo sniff a10 eth0 --dump icmp

  Nothing arrives while the ping succeeds: the filter reads the ethertype
  at offset 12, 0x8100 on a trunk, and the grammar has no vlan word to step
  over it, so ip, icmp, tcp, udp, host and port miss every tagged frame, as
  in tcpdump. The capture of this protocol is the unfiltered one, the tag
  in bytes 12 and 13:

    cloonix cli nemo sniff a10 eth0 --dump

  ACT 4 is the packet to wait for: one ping from a10 to b20 caught twice on
  the same wire, tagged 10 then 20, the macs rewritten and the IP addresses
  untouched, routing at its smallest scale.

Important
  A cloonix lan is a trunk because nothing was done to it. Tagging the ovs
  port makes it an access port, the guests then needing no vlan interface.
"""
import os
import shlex
import subprocess
import sys
import threading
import time

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    NET, CLOONIX, answers, capturable, check_net, erase_net, in_parallel,
    parse_bytes_line, place, run, show, ssh, step, story, wait_exists,
    unavailable, guarded)

ROOTFS_CHOICES = ("trixie", "trixie_systemd")

HISTORY = """\
IEEE 802.1Q, ratified in 1998, puts four bytes inside the ethernet
header: a tag protocol id 0x8100 where the ethertype used to be, and a
12-bit vlan id, so one wire carries up to 4094 separate networks. The
first linux implementation was written by Ben Greear in 1999 and went
into kernel 2.4 with its vconfig tool; the eth0.10 naming used here dates
from it, and the ip link command of iproute2 has replaced vconfig since.

Today: every enterprise switch port is in a vlan; the same tag separates voice
from data on a desk, tenants in a hosting rack and wifi networks on one access
point cable.
"""

A10, A20, B10, B20, RTR = "a10", "a20", "b10", "b20", "rtr"
ALL = (A10, A20, B10, B20, RTR)
LAN = "lan_trunk"

VID = {A10: 10, A20: 20, B10: 10, B20: 20}
ADDR = {A10: "10.0.10.1", A20: "10.0.20.1",
        B10: "10.0.10.2", B20: "10.0.20.2"}
RTR_ADDR = {10: "10.0.10.254", 20: "10.0.20.254"}

# The second address of act 2: on vlan 20, in vlan 10's subnet.
SAME_SUBNET = "10.0.10.9"

def choose_rootfs():
    said = ""
    for cand in ROOTFS_CHOICES:
        r = run("cloonix cli %s add cnt probe_vlan eth=1 %s" % (NET, cand))
        ok = (r.returncode == 0) and wait_exists("probe_vlan", 20)
        run("cloonix --wait 0.3 cli %s del probe_vlan" % NET)
        if ok:
            return cand
        said = (r.stdout + r.stderr).strip() or "nothing"
    unavailable("the server would not start a container on %s.\n"
             "  it said: %s\n"
             "  fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_bulk/"
             "trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm && ./cnt_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said))

def create(name, rootfs):
    last = ""
    for _ in range(6):
        r = run("cloonix cli %s add cnt %s eth=1 %s" % (NET, name, rootfs))
        last = (r.stdout + r.stderr).strip()
        if r.returncode == 0 and wait_exists(name):
            return
        if r.returncode == 0:
            last = "the add was taken and %s never appeared" % name
        time.sleep(2)
    raise RuntimeError("%s was never created. The server said: %s"
                       % (name, last or "nothing at all"))

# ACT 1 -- vlan interfaces instead of addresses

def address(node):
    """eth0 up and bare, the address on a vlan interface above it.

    The bare eth0 is not an oversight. A trunk port has no address of its
    own: everything it carries belongs to one vlan or another, and a frame
    with no tag would belong to none of them.
    """
    ssh(node, "ip link set dev eth0 up")
    if node == RTR:
        for vid, addr in RTR_ADDR.items():
            ssh(node, "ip link add link eth0 name eth0.%d type vlan id %d"
                      % (vid, vid))
            ssh(node, "ip addr add %s/24 dev eth0.%d" % (addr, vid))
            ssh(node, "ip link set dev eth0.%d up" % vid)
        return
    vid = VID[node]
    ssh(node, "ip link add link eth0 name eth0.%d type vlan id %d"
              % (vid, vid))
    ssh(node, "ip addr add %s/24 dev eth0.%d" % (ADDR[node], vid))
    ssh(node, "ip link set dev eth0.%d up" % vid)

def ping(node, target, count=5, wait=2):
    return run('cloonix cli %s ssh %s "ping -c %s -W %s %s"'
               % (NET, node, count, wait, target)).returncode == 0

def reach(pairs, title):
    story("    %s" % title)
    got = {}
    for src, dst_addr, note in pairs:
        ok = ping(src, dst_addr)
        got[(src, dst_addr)] = ok
        story("      %-5s -> %-12s %-9s %s"
              % (src, dst_addr, "reaches" if ok else "NO REPLY", note))
    story()
    return got

# ACT 3 -- the tag read out of the frame: dst(6) src(6) then ethertype or 0x8100 + TCI.

def frame_tag(data):
    """(vid, pcp, inner ethertype) if this frame is tagged, else None."""
    if len(data) < 18:
        return None
    if (data[12] << 8 | data[13]) != 0x8100:
        return None
    tci = data[14] << 8 | data[15]
    return (tci & 0x0fff, tci >> 13, data[16] << 8 | data[17])

def mac(data, off):
    return ":".join("%02x" % b for b in data[off:off + 6])

def ipv4_of(data):
    """src, dst of an ipv4 packet, tagged or not, or None."""
    base = 14
    if frame_tag(data):
        base = 18
    if len(data) < base + 20:
        return None
    if base == 14 and (data[12] << 8 | data[13]) != 0x0800:
        return None
    if base == 18 and (data[16] << 8 | data[17]) != 0x0800:
        return None
    return (".".join(str(b) for b in data[base + 12:base + 16]),
            ".".join(str(b) for b in data[base + 16:base + 20]))

class Tap:
    """One sniff subscription, with an optional filter, counting frames."""

    def __init__(self, node, num, filt=""):
        self.filt = filt
        self.live = False
        self.lock = threading.Lock()
        self.count = 0
        self.frames = []
        capturable(node, num)
        line = ("cloonix cli %s sniff %s eth%d --bytes %s"
                % (NET, node, num, filt)).rstrip()
        show(line)
        self.proc = subprocess.Popen(CLOONIX + shlex.split(line)[1:],
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, text=True,
                                     bufsize=1)
        self.ready_evt = threading.Event()
        threading.Thread(target=self._drain, daemon=True).start()

    def _drain(self):
        for line in self.proc.stdout:
            line = line.rstrip("\n")
            if line.startswith("# waiting"):
                self.live = True
                self.ready_evt.set()
                continue
            if not line.startswith("pkt "):
                continue
            pkt = parse_bytes_line(line)
            if not pkt:
                continue
            with self.lock:
                self.count += 1
                if len(self.frames) < 40:
                    self.frames.append(pkt["data"])
        self.ready_evt.set()

    def ready(self, timeout=15):
        self.ready_evt.wait(timeout)
        if not self.live:
            err = self.proc.stderr.read() if self.proc.stderr else ""
            story("      could not subscribe with filter %r: %s"
                  % (self.filt, err.strip() or "no answer"))
        return self.live

    def clear(self):
        with self.lock:
            self.count = 0
            self.frames = []

    def stop(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()

def with_taps(taps, do_it, settle=1.5):
    for t in taps:
        if not t.ready():
            for x in taps:
                x.stop()
            return None
    time.sleep(1.0)
    for t in taps:
        t.clear()
    out = do_it()
    time.sleep(settle)
    for t in taps:
        t.stop()
    return out

def show_tags(frames, limit=6):
    shown = 0
    for data in frames:
        tag = frame_tag(data)
        if not tag:
            continue
        vid, pcp, inner = tag
        ip = ipv4_of(data)
        story("        vlan %-3d pcp %d  ethertype 0x%04x  %s -> %s   %s"
              % (vid, pcp, inner, mac(data, 6), mac(data, 0),
                 ("%s > %s" % ip) if ip else ""))
        shown += 1
        if shown >= limit:
            break
    if not shown:
        story("        (no tagged frame in what was captured)")
    return shown

LAYOUT = {
    A10: (-380, -165),
    A20: (-380, 55),
    LAN: (0, -55),
    B10: (380, -165),
    B20: (380, 55),
    RTR: (0, 165),
}

ETHS = {
    (A10, "eth0"): 110,
    (A20, "eth0"): 70,
    (B10, "eth0"): 250,
    (B20, "eth0"): 290,
    (RTR, "eth0"): 0,
}

CARD = r"""
    a10                                                    b10
  10.0.10.1 --+                                    +-- 10.0.10.2
              |                                    |
              +------------ lan_trunk -------------+
              |                 |                  |
    a20       |               rtr                  |       b20
  10.0.20.1 --+          10.0.10.254               +-- 10.0.20.2
                         10.0.20.254

  ONE cloonix lan. Every machine has eth0 on it with no address at all;
  the addresses live on eth0.10 and eth0.20, which are vlan interfaces.
  a10 and b10 are on vlan 10, a20 and b20 on vlan 20, rtr on both.
"""

def main():
    story(HISTORY)
    step("checking the server")
    check_net()

    step("looking for an image")
    rootfs = choose_rootfs()
    erase_net()
    place()
    story(CARD)
    story("  image: %s" % rootfs)

    step("ACT 1: five containers on one lan, and vlan interfaces on top")
    in_parallel(lambda n: create(n, rootfs), list(ALL), "add cnt")
    for name in ALL:
        run("cloonix cli %s plug %s eth0 %s" % (NET, name, LAN))
    for name in ALL:
        end = time.time() + 90
        while time.time() < end and not answers(name):
            time.sleep(2)
        if not answers(name):
            raise RuntimeError("%s is in the list and never answered ssh"
                               % name)
    in_parallel(address, list(ALL), "address")
    story("  eth0 is up and bare on all five. A trunk port has no address")
    story("  of its own: every frame on it belongs to a vlan.")
    story()
    first = reach([(A10, ADDR[B10], "vlan 10, both ends"),
                   (A20, ADDR[B20], "vlan 20, both ends"),
                   (A10, ADDR[B20], "vlan 10 to vlan 20")],
                  "the same wire, asked three times:")
    ok_same_vlan = (first[(A10, ADDR[B10])] and first[(A20, ADDR[B20])])
    ok_split = not first[(A10, ADDR[B20])]

    step("ACT 2: the same subnet, and still no answer")
    story()
    ssh(A20, "ip addr add %s/24 dev eth0.20" % SAME_SUBNET)
    story("    a20 now also holds %s, on vlan 20." % SAME_SUBNET)
    story("    Same wire as a10, same subnet as a10, different tag.")
    story()
    second = reach([(A10, SAME_SUBNET, "same subnet, other vlan")],
                   "a10 asking for an address that is on the wire:")
    ok_subnet = not second[(A10, SAME_SUBNET)]
    got = ssh(A10, "ip neigh show %s" % SAME_SUBNET, check=False).strip()
    story("    a10's arp table for it: %s" % (got or "(empty)"))
    story("    The arp request went out tagged 10 and nothing tagged 10")
    story("    holds that address. A subnet is a convention between hosts;")
    story("    the tag is what the wire enforces.")
    story()
    # a20 gives the address back: its connected route to 10.0.10.0/24
    # would otherwise shadow the route through rtr of ACT 4
    ssh(A20, "ip addr del %s/24 dev eth0.20" % SAME_SUBNET)

    step("ACT 3: the tag on the wire, and a filter that misses it")
    story()
    plain = Tap(A10, 0)
    filtered = Tap(A10, 0, "icmp")
    out = with_taps([plain, filtered],
                    lambda: ping(A10, ADDR[B10], count=5))
    if out is None:
        sys.exit("could not subscribe to the trunk")
    story("      one ping, a10 to b10, and two subscriptions to the SAME")
    story("      interface over it:")
    story()
    story("        no filter        %4d frames" % plain.count)
    story("        \"icmp\"           %4d frames" % filtered.count)
    story()
    story("      what the unfiltered one caught:")
    tagged = show_tags(plain.frames)
    story()
    if filtered.count == 0 and plain.count:
        story("      The filter reads the ethertype at offset 12 of the")
        story("      frame. On a trunk that field is 0x8100, not 0x0800, and")
        story("      the grammar has no vlan keyword to step over the tag")
        story("      with -- so ip, icmp, tcp, udp, host and port all miss")
        story("      every frame. tcpdump does the same thing for the same")
        story("      reason. On a trunk, capture unfiltered.")
    elif filtered.count:
        story("      This server's filter DID match tagged frames (%d of"
              % filtered.count)
        story("      them), so it steps over the tag. That is")
        story("      not what tcpdump does with the same expression.")
    story()

    step("ACT 4: a router on a stick")
    story()
    ssh(RTR, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    for node in (A10, B10):
        ssh(node, "ip route add 10.0.20.0/24 via %s" % RTR_ADDR[10])
    for node in (A20, B20):
        ssh(node, "ip route add 10.0.10.0/24 via %s" % RTR_ADDR[20])
    story("    rtr forwards, and each machine is given a route to the other")
    story("    vlan through it. rtr has ONE interface on the wire.")
    story()
    both = Tap(RTR, 0)
    out = with_taps([both], lambda: ping(A10, ADDR[B20], count=5))
    if out is None:
        sys.exit("could not subscribe to the trunk")
    ok_routed = bool(out)
    story("      a10 -> b20, across the vlans: %s"
          % ("reaches" if ok_routed else "NO REPLY"))
    story()
    story("      the same ping caught on rtr's single interface, arriving")
    story("      on one vlan and leaving on the other:")
    seen = {}
    with both.lock:
        frames = list(both.frames)
    for data in frames:
        tag = frame_tag(data)
        ip = ipv4_of(data)
        if not tag or not ip:
            continue
        key = (tag[0], ip[0], ip[1])
        seen[key] = seen.get(key, 0) + 1
    if seen:
        story("        %-6s %-14s %-14s %s"
              % ("vlan", "from", "to", "frames"))
        for (vid, src, dst), nb in sorted(seen.items()):
            story("        %-6d %-14s %-14s %d" % (vid, src, dst, nb))
    else:
        story("        (nothing tagged and ipv4 was captured)")
    story()
    story("    The addresses never change. The tag does, and so do the mac")
    story("    addresses, which is what routing is: a new frame around the")
    story("    same packet, once per hop.")
    story()

    bad = []
    if not ok_same_vlan:
        bad.append("machines on the same vlan could not reach each other")
    if not ok_split:
        bad.append("vlan 10 reached vlan 20 with no router, which means the"
                   " tags did not separate anything")
    if not ok_subnet:
        bad.append("the same subnet on another vlan answered, so act 2"
                   " proves nothing")
    if not tagged:
        bad.append("no tagged frame was captured, so act 3 proves nothing")
    if bad:
        sys.exit("\n" + "\n".join("  " + b for b in bad))

    story("""
the network is left up. Things to try:

  the wire, unfiltered, while a ping runs -- every frame tagged:
    cloonix cli %s sniff a10 eth0 --dump

  the same wire with a filter, which finds nothing at all:
    cloonix cli %s sniff a10 eth0 --dump icmp

  put a third vlan on the same wire with no reboot and no cabling:
    cloonix cli %s ssh a10 "ip link add link eth0 name eth0.30 type vlan id 30"

  and take a tag away, to watch a machine fall off a network it is still
  physically cabled to:
    cloonix cli %s ssh b10 "ip link set dev eth0.10 down"
""" % (NET, NET, NET, NET))

if __name__ == "__main__":
    guarded(main, ALL)
