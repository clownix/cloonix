#!/usr/bin/env python3
r"""stp - a loop, and the protocol whose whole job is to break it.

    ./stp.py [net]                  the net defaults to nemo

Two containers, two wires between them::

                 ======= lan_x =======
    br1                                     br2
  10.0.0.1  ===========================  10.0.0.2
                 ======= lan_y =======

Each one runs a kernel bridge with eth0 and eth1 in it, so the two lans
together are a loop. A broadcast entering either bridge leaves by the other
port, comes back round the far bridge, and is flooded again.

  ACT 1  one wire only. It works, and it is boring.
  ACT 2  the second wire, spanning tree OFF. One arp, and three seconds of
         counting what comes back. Then a port is taken down by hand,
         because nothing else is going to stop it.
  ACT 3  spanning tree ON: a root is elected, one port goes to blocking,
         read out of the kernel. The same arp, and a count of a handful.
  ACT 4  the forwarding wire cut with a ping running, and the seconds
         counted until the blocked port takes over. That number is why
         RSTP was written.

Runs on the plain trixie image: iproute2 and ping and nothing else.

ACT 2 makes a real storm for three seconds. It is ended by taking a port
down, not by a timer, so if this is killed in those three seconds:
  cloonix cli <net> ssh br2 "ip link set dev eth1 down"

Needs cloonix on the PATH and nothing else. Standard library only.

Image: trixie            (the plain one -- no demo image to build)
Size:  2 containers, 2 lans

THE PACKET TO CATCH
-------------------
  BPDUs are not IP. They are 802.3 frames -- bytes 12 and 13 are a
  LENGTH, not an ethertype -- addressed to 01:80:c2:00:00:00, carrying
  LLC 0x42 0x42 0x03 and then the config message. The filter grammar
  knows ip, ip6, arp, tcp, udp, icmp and addresses, and has no word for
  any of that, so the capture that shows the protocol is unfiltered:

    cloonix cli nemo cnf br1 eth0 sniff=on
    cloonix cli nemo sniff br1 eth0 --dump

  One every two seconds from the root, which is the hello timer.

  The packet to catch is the arp of ACT 2, and the point is the count.
  The same single request, sent twice:

    cloonix cli nemo sniff br1 eth1 --dump arp

  With no spanning tree it arrives thousands of times and does not stop.
  With spanning tree it arrives once or twice and stops. Nothing inside
  either guest has to be believed: the number comes off the wire.

  ACT 4 is worth watching in a second terminal:

    cloonix cli nemo sniff br1 eth1 --dump icmp

  Cut the other wire and this subscription goes quiet, then resumes
  about thirty seconds later when the blocked port has finished
  listening and learning. That gap is 802.1D, measured.

Important
  It runs on the plain trixie container image. There is no image to build,
  which makes this and switching/vlan the two demos that work on a fresh
  install.
  ACT 2 makes a real broadcast storm on two cloonix lans for three seconds.
  It is bounded by taking a port down and not by any timer, so if the demo
  is killed with ctrl-C during those three seconds, break it by hand:
    cloonix cli nemo ssh br2 "ip link set dev eth1 down"
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
    NET, CLOONIX, answers, capturable, check_net, erase_net, in_lst,
    parse_bytes_line, place, run, show, ssh, step, story, wait_exists)

ROOTFS_CHOICES = ("trixie", "trixie_systemd")

HISTORY = """\
Spanning tree was written by Radia Perlman at Digital Equipment
Corporation in 1985, in one afternoon by her own account, with a poem
in the paper that states the algorithm in verse. Bridges elect a root,
each keeps its cheapest path to it and blocks every other port, so a
loop of wires becomes a tree without anybody drawing one. IEEE 802.1D
adopted it in 1990; the rapid version, 802.1w, came in 2001 and is what
the kernel runs today. The bridge under study is the one of the linux
kernel, written by Lennert Buytenhek in 2000 for the 2.4 series, driven
here with the bridge command of iproute2.

Today: every managed switch ships with spanning tree enabled, in its rapid
form 802.1w since 2001; a second cable plugged between two office switches is
the loop it silently removes.
"""

B1, B2 = "br1", "br2"
ALL = (B1, B2)
LAN_X, LAN_Y = "lan_x", "lan_y"
ADDR = {B1: "10.0.0.1", B2: "10.0.0.2"}

NOBODY = "10.0.0.99"

# How long the loop is left unprotected.
STORM_SECONDS = 3.0

PORT_STATE = {"0": "disabled", "1": "listening", "2": "learning",
              "3": "FORWARDING", "4": "BLOCKING"}

def choose_rootfs():
    said = ""
    for cand in ROOTFS_CHOICES:
        r = run("cloonix cli %s add cnt probe_stp eth=1 %s" % (NET, cand))
        ok = (r.returncode == 0) and wait_exists("probe_stp", 20)
        run("cloonix --wait 0.3 cli %s del probe_stp" % NET)
        if ok:
            return cand
        said = (r.stdout + r.stderr).strip() or "nothing"
    sys.exit("the server would not start a container on %s.\n"
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
        r = run("cloonix cli %s add cnt %s eth=2 %s" % (NET, name, rootfs))
        last = (r.stdout + r.stderr).strip()
        if r.returncode == 0 and wait_exists(name):
            return
        if r.returncode == 0:
            last = "the add was taken and %s never appeared" % name
        time.sleep(2)
    raise RuntimeError("%s was never created. The server said: %s"
                       % (name, last or "nothing at all"))

# the bridges

def bridge_up(node):
    """A kernel bridge with eth0 in it, and the address on the bridge.

    Only eth0 to start with. The second port is added in act 2, when the
    loop is wanted, so that nothing is looping while the machines are being
    set up and a stray broadcast has nowhere to go round.
    """
    ssh(node, "ip link add name br0 type bridge")
    ssh(node, "ip link set dev eth0 master br0")
    ssh(node, "ip link set dev eth0 up")
    ssh(node, "ip addr add %s/24 dev br0" % ADDR[node])
    ssh(node, "ip link set dev br0 up")
    ssh(node, "ip link set dev eth1 down")

def stp(node, on):
    ssh(node, "ip link set dev br0 type bridge stp_state %d" % (1 if on else 0))

def second_port(node, up):
    if up:
        ssh(node, "ip link set dev eth1 master br0", check=False)
        ssh(node, "ip link set dev eth1 up")
    else:
        ssh(node, "ip link set dev eth1 down")

def port_state(node, port):
    out = ssh(node, "cat /sys/class/net/%s/brport/state 2>/dev/null" % port,
              check=False).strip()
    return PORT_STATE.get(out, out or "?")

def bridge_ids(node):
    root = ssh(node, "cat /sys/class/net/br0/bridge/root_id", check=False)
    me = ssh(node, "cat /sys/class/net/br0/bridge/bridge_id", check=False)
    return root.strip(), me.strip()

def show_ports(title):
    story("    %s" % title)
    story("      %-5s %-6s %-12s %s" % ("node", "port", "state", "bridge id"))
    for node in ALL:
        _, me = bridge_ids(node)
        for port in ("eth0", "eth1"):
            story("      %-5s %-6s %-12s %s"
                  % (node, port, port_state(node, port), me))
    root1, _ = bridge_ids(B1)
    root2, _ = bridge_ids(B2)
    story("      root as seen by %s: %s" % (B1, root1 or "?"))
    story("      root as seen by %s: %s" % (B2, root2 or "?"))
    story("      (they agree, or spanning tree has not converged yet)")
    story()

def blocking_ports():
    return [(n, p) for n in ALL for p in ("eth0", "eth1")
            if port_state(n, p) == "BLOCKING"]

def wait_converged(timeout=60):
    """Wait until exactly one port is blocking and the rest are forwarding.

    Timed rather than slept: the whole subject here is how long 802.1D
    takes, and a fixed sleep would hide the number the demo is about.
    """
    end = time.time() + timeout
    while time.time() < end:
        states = [port_state(n, p) for n in ALL for p in ("eth0", "eth1")]
        if states.count("BLOCKING") == 1 and states.count("FORWARDING") == 3:
            return time.time() - (end - timeout)
        time.sleep(1)
    return None

# the wire

def mac(data, off):
    return ":".join("%02x" % b for b in data[off:off + 6])

def bpdu(data):
    """A config BPDU, decoded, or None.

    An 802.3 frame: destination 01:80:c2:00:00:00, then a LENGTH where an
    ethertype would be, then LLC 42 42 03, then the message. Nothing above
    the link layer is involved, which is why no filter can name it.
    """
    if len(data) < 39:
        return None
    if data[0:6] != b"\x01\x80\xc2\x00\x00\x00":
        return None
    if data[14:17] != b"\x42\x42\x03":
        return None
    body = data[17:]
    if len(body) < 22 or body[0:2] != b"\x00\x00":
        return None
    kind = {0x00: "config", 0x80: "topology change"}.get(body[3], "type %d"
                                                          % body[3])
    root = ":".join("%02x" % b for b in body[5:13])
    cost = int.from_bytes(body[13:17], "big")
    sender = ":".join("%02x" % b for b in body[17:25]) \
        if len(body) >= 25 else "?"
    return {"kind": kind, "root": root, "cost": cost, "sender": sender,
            "flags": body[4]}

class Tap:
    """One sniff subscription, with an optional filter, counting frames."""

    def __init__(self, node, num, filt=""):
        self.filt = filt
        self.live = False
        self.lock = threading.Lock()
        self.count = 0
        self.first = None
        self.last = None
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
            now = time.time()
            with self.lock:
                self.count += 1
                if self.first is None:
                    self.first = now
                self.last = now
                if len(self.frames) < 60:
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
            self.first = self.last = None
            self.frames = []

    def stop(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()

def arp_once(node):
    """One arp request for an address nobody holds.

    Nobody answers, so what is on the wire is the broadcast and nothing
    else -- which is what makes the count afterwards mean something.
    """
    run('cloonix cli %s ssh %s "ping -c 1 -W 1 %s"' % (NET, node, NOBODY))

def ping(node, target, count=5, wait=2):
    return run('cloonix cli %s ssh %s "ping -c %s -W %s %s"'
               % (NET, node, count, wait, target)).returncode == 0

LAYOUT = {
    B1: (-300, 0),
    LAN_X: (0, -130),
    LAN_Y: (0, 130),
    B2: (300, 0),
}

ETHS = {
    (B1, "eth0"): 60,
    (B1, "eth1"): 120,
    (B2, "eth0"): 300,
    (B2, "eth1"): 240,
}

CARD = r"""
                 ======= lan_x =======
    br1                                     br2
  10.0.0.1  ===========================  10.0.0.2
                 ======= lan_y =======

  Two containers, each running a kernel bridge with eth0 and eth1 in it,
  and TWO cloonix lans between them. That is a loop: a broadcast entering
  either bridge leaves by the other port and comes back round.
"""

def emergency():
    """Break the loop, whatever else has gone wrong."""
    for node in ALL:
        run('cloonix cli %s ssh %s "ip link set dev eth1 down"'
            % (NET, node))

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

    step("ACT 1: two bridges, ONE wire between them")
    for name in ALL:
        create(name, rootfs)
    run("cloonix cli %s plug %s eth0 %s" % (NET, B1, LAN_X))
    run("cloonix cli %s plug %s eth0 %s" % (NET, B2, LAN_X))
    run("cloonix cli %s plug %s eth1 %s" % (NET, B1, LAN_Y))
    run("cloonix cli %s plug %s eth1 %s" % (NET, B2, LAN_Y))
    for name in ALL:
        end = time.time() + 90
        while time.time() < end and not answers(name):
            time.sleep(2)
        if not answers(name):
            raise RuntimeError("%s never answered ssh" % name)
    for name in ALL:
        bridge_up(name)
        stp(name, False)
    story("  eth1 is cabled to %s on both, and DOWN on both. One path." % LAN_Y)
    ok_one_wire = ping(B1, ADDR[B2], count=5)
    story("  br1 -> br2 over one wire: %s"
          % ("reaches" if ok_one_wire else "NO REPLY"))
    story()

    step("ACT 2: the second wire, with no spanning tree")
    story()
    tap = Tap(B1, 1, "arp")
    if not tap.ready():
        emergency()
        sys.exit("could not subscribe to %s" % LAN_Y)
    time.sleep(1.0)
    tap.clear()
    story("      bringing eth1 up on both bridges. There is now a loop and")
    story("      nothing to stop one, for %.0f seconds by the clock."
          % STORM_SECONDS)
    for name in ALL:
        second_port(name, True)
    arp_once(B1)
    time.sleep(STORM_SECONDS)
    # Down before anything else, including before the counting is printed: a
    # port taken down is instant, and nothing else here is.
    emergency()
    with tap.lock:
        seen, first, last = tap.count, tap.first, tap.last
    tap.stop()
    span = (last - first) if (first and last and last > first) else 0.0
    story()
    story("        one arp request was sent.")
    story("        arp frames counted on %s: %d" % (LAN_Y, seen))
    if span:
        story("        over %.1f s, so about %.0f frames a second"
              % (span, seen / span))
    story()
    story("      Nobody holds %s, so nothing answered. Every one of those")
    story("      frames is the SAME request, going round.")
    story("      A loop has no hop count: ethernet has no ttl, and a frame")
    story("      that is flooded once is flooded for ever.")
    story()
    storm = seen

    step("ACT 3: spanning tree on")
    story()
    for name in ALL:
        stp(name, True)
        second_port(name, True)
    story("      waiting for the two bridges to elect a root and put one")
    story("      port into blocking. 802.1D takes its time on purpose.")
    took = wait_converged(70)
    if took is None:
        story("      it did not settle into one blocking port in 70 s.")
    else:
        story("      settled after about %.0f s." % took)
    story()
    show_ports("what the kernel says about each port:")
    blocked = blocking_ports()
    if blocked:
        story("    %s eth%s is blocking: it is cabled, it is up, it hears"
              % (blocked[0][0], blocked[0][1][-1]))
        story("    BPDUs, and it forwards nothing. That is the whole trick.")
    story()
    tap = Tap(B1, 1, "arp")
    if not tap.ready():
        emergency()
        sys.exit("could not subscribe to %s" % LAN_Y)
    time.sleep(1.0)
    tap.clear()
    arp_once(B1)
    time.sleep(STORM_SECONDS)
    with tap.lock:
        quiet = tap.count
    tap.stop()
    story("      the SAME single arp, with the loop broken: %d frames"
          % quiet)
    story("      against %d a moment ago." % storm)
    story()
    ok_ping = ping(B1, ADDR[B2], count=5)
    story("      and br1 still reaches br2: %s"
          % ("yes" if ok_ping else "NO REPLY"))
    story("      A blocked port does not mean a broken network. One path")
    story("      is exactly what was wanted.")
    story()

    step("ACT 4: the BPDUs, and what a failure costs")
    story()
    tap = Tap(B1, 0)
    if not tap.ready():
        emergency()
        sys.exit("could not subscribe to %s" % LAN_X)
    time.sleep(5.0)
    with tap.lock:
        frames = list(tap.frames)
    tap.stop()
    shown = 0
    for data in frames:
        got = bpdu(data)
        if not got:
            continue
        story("        BPDU %-16s from %s  root %s  cost %d"
              % (got["kind"], mac(data, 6), got["root"], got["cost"]))
        shown += 1
        if shown >= 4:
            break
    if not shown:
        story("        (no BPDU in five seconds of unfiltered capture)")
    else:
        story()
        story("      One every two seconds, from the root. They are 802.3")
        story("      frames -- a length where an ethertype would be -- so no")
        story("      filter in the grammar can name them. Capture unfiltered")
        story("      or see nothing.")
    story()

    story("      now cutting the forwarding wire, with a ping running.")
    forwarding_on_b1 = [p for p in ("eth0", "eth1")
                        if port_state(B1, p) == "FORWARDING"]
    if not forwarding_on_b1:
        story("      no forwarding port on %s to cut -- skipping." % B1)
        outage = None
    else:
        cut = forwarding_on_b1[0]
        ssh(B1, "ip link set dev %s down" % cut)
        started = time.time()
        outage = None
        while time.time() - started < 90:
            if ping(B1, ADDR[B2], count=1, wait=1):
                outage = time.time() - started
                break
            time.sleep(1)
        ssh(B1, "ip link set dev %s up" % cut)
    story()
    if outage is None:
        story("      it had not come back after 90 s.")
    else:
        story("        %s was taken down. The far side came back after"
              % forwarding_on_b1[0])
        story("        %.0f seconds." % outage)
        story()
        story("      Listening then learning, fifteen seconds each, before a")
        story("      blocked port is allowed to forward. That is 802.1D as")
        story("      Perlman specified it in 1985, and it is the number RSTP")
        story("      was written to remove -- RSTP does the same job in about")
        story("      a second by asking the neighbour instead of waiting.")
        story("      On Linux, RSTP is mstpd in userspace, not this bridge.")
    story()

    bad = []
    if not ok_one_wire:
        bad.append("the two bridges never reached each other on one wire")
    if storm < 20:
        bad.append("the unprotected loop produced only %d frames, which is"
                   " not a storm -- check that both lans really join the two"
                   " bridges" % storm)
    if quiet >= storm:
        bad.append("spanning tree changed nothing: %d frames before, %d"
                   " after" % (storm, quiet))
    if bad:
        sys.exit("\n" + "\n".join("  " + b for b in bad))

    story("""
the network is left up, with spanning tree on. Things worth trying:

  the BPDUs, live -- unfiltered, because nothing names them:
    cloonix cli %s sniff br1 eth0 --dump

  which port is blocking, from the kernel rather than from a protocol:
    cloonix cli %s ssh br1 "bridge link"

  make the other bridge the root by giving it a better priority, and watch
  the blocked port move:
    cloonix cli %s ssh br2 "ip link set dev br0 type bridge priority 4096"

  and turn spanning tree off again with both wires up, to see the storm
  come back -- then break it the only way there is:
    cloonix cli %s ssh br1 "ip link set dev br0 type bridge stp_state 0"
    cloonix cli %s ssh br2 "ip link set dev eth1 down"
""" % (NET, NET, NET, NET, NET))

if __name__ == "__main__":
    try:
        main()
    except SystemExit as stop:
        if stop.code:
            emergency()
            sys.stderr.write("%s\n" % stop.code)
            up = [n for n in ALL if in_lst(n)]
            if up:
                sys.stderr.write("Still running: %s\n" % ", ".join(up))
            raise SystemExit(1)
        raise
    except BaseException:
        # Including ctrl-C: the loop must not be left running.
        emergency()
        raise
