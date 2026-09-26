#!/usr/bin/env python3
r"""snf - the same lan, looked at four ways.

    ./snf.py [net]              the net defaults to nemo

Four machines on one lan::

  alice -- lan1 -- bob        noisy1 -- lan1 -- noisy2
 10.0.0.10      10.0.0.20    10.0.0.31        10.0.0.32

noisy1 floods the lan with several hundred broadcast datagrams a second on
ports 10000 to 10100 -- the shape of rtp, and the shape of most of what a
real network carries. Meanwhile alice sends bob one small beacon a second on
port 53 and pings him once a second.

Three packets a second that matter, inside two thousand that do not.

The demo then subscribes four times to the SAME interface over the SAME
traffic, changing only the filter, and counts what arrives::

  (no filter)                        everything, and the beacon is lost in it
  not udp portrange 10000-10100      the noise removed, nothing else changed
  udp port 53                        the beacons alone, and readable
  icmp                               the pings alone, with their sequence

The filter runs in the switch. What it rejects is never encoded, never sent
and never decoded -- so the numbers below are not a display convenience,
they are work that did not happen.

Needs cloonix on the PATH and nothing else. Standard library only, like
cloonix itself, so it runs wherever the client does.

Image: any trixie image
Size:  4 containers, 1 lan

THE PACKET TO CATCH
-------------------
  The whole demo is the packet to catch, counted four ways over the same
  traffic:

    cloonix cli nemo cnf alice eth0 sniff=on
    cloonix cli nemo sniff alice eth0 --dump not udp portrange 10000-10100

  Nothing about the lan changes between the acts. n1 sends at the same rate
  throughout and alice's interface carries all of it -- the difference is
  that the switch throws it away instead of sending it on.

Important
  The noise is broadcast on purpose. An ovs lan learns where machines are
  and stops flooding unicast to everybody, so a flood between n1 and n2
  would never reach alice's interface and there would be nothing to filter.
"""
import os
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
    NET, ACK_FAST, CLOONIX, capturable, cli, create_cnt, erase_net,
    in_parallel, net_answers, parse_bytes_line, show, step, story, trace,
    trace_ssh, wait_ready)

# Any image with python3 in it.
ROOTFS_CHOICES = ("wg_trixie", "proxy_trixie", "smtp_trixie", "mqtt_trixie")

ALICE, BOB, N1, N2 = "alice", "bob", "noisy1", "noisy2"
ALL = (ALICE, BOB, N1, N2)
LAN = "lan1"

WIRING = {
    ALICE: [(0, LAN, "10.0.0.10/24")],
    BOB:   [(0, LAN, "10.0.0.20/24")],
    N1:    [(0, LAN, "10.0.0.31/24")],
    N2:    [(0, LAN, "10.0.0.32/24")],
}
GATEWAY = {}

BCAST = "10.0.0.255"
NOISE_LO, NOISE_HI = 10000, 10100
BEACON_PORT = 53
NOISE_RATE = 500          # datagrams a second, broadcast
WINDOW = 4.0              # how long each act watches

def ssh(node, cmd, check=True, log=None):
    trace_ssh("ssh %s: %s", node, cmd if len(cmd) < 70 else cmd[:67] + "...")
    show('cloonix cli %s ssh %s "%s"' % (NET, node, cmd))
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node, cmd],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if r.returncode:
        trace_ssh("ssh %s: returned %d", node, r.returncode)
    if check and r.returncode:
        said = (r.stderr or r.stdout).strip()
        if not said and log:
            said = ssh(node, "tail -n 20 %s 2>/dev/null" % log,
                       check=False).strip()
        raise RuntimeError("%s: %s\n  -> %s" %
                           (node, cmd, said or "no output anywhere"))
    return r.stdout

def put(node, path, text):
    """Write a file into a guest with printf.

    printf reads its first argument as a FORMAT, and that is the whole
    difficulty. Three things in it are not literal:

      %   a conversion. Refused outright below.
      \'   ends the shell argument. Refused too.
      \\   an escape. printf turns \\r into a carriage return, \\t into a tab,
          \\n into a newline -- so any backslash in the text has to be
          doubled before it is sent, or the file that arrives is not the
          file that was meant.

    That last one is not hypothetical. A python script containing
    f.write("\\r\\n") arrived with a real carriage return in the middle of a
    string literal, and the guest reported an unterminated string on a line
    that looked perfectly fine in the source here.

    So: backslashes are doubled FIRST, and only then are real newlines
    turned into the escape that printf will turn back.
    """
    if "'" in text or "%" in text:
        raise RuntimeError("%s cannot be written with printf as it stands"
                           % path)
    trace_ssh("writing %s on %s, %d bytes", path, node, len(text))
    escaped = text.replace("\\", "\\\\").replace("\n", "\\n")
    ssh(node, "printf '%s' > %s" % (escaped, path))

def choose_rootfs():
    up, why = net_answers()
    if not up:
        sys.exit("no answer from a cloonix net called '%s'.\n"
                 "  the client said: %s\n"
                 "  A demo takes the net name as its first argument and\n"
                 "  falls back to nemo:   ./snf.py <net>"
                 % (NET, why or "nothing at all"))
    said = ""
    for cand in ROOTFS_CHOICES:
        trace("probing the server with a throwaway cnt on %s", cand)
        r = cli("add", "cnt", "probe_rootfs", "eth=1", cand, ack=240)
        if r.returncode == 0:
            cli("del", "probe_rootfs", ack=ACK_FAST)
            return cand
        said = (r.stdout + r.stderr).strip()
    cli("del", "probe_rootfs", ack=ACK_FAST)
    sys.exit("the server would not start a container on %s.\n"
             "  it said: %s\n"
             "  either will do; build one:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_wg_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said or "nothing"))

def tools_present(node, *tools):
    for t in tools:
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                            "command -v %s >/dev/null 2>&1" % t],
                           capture_output=True, stdin=subprocess.DEVNULL)
        if r.returncode:
            sys.exit("%s has no %s: this image has no python3.\n"
                     "  fetch it:\n"
                     "    cd /var/lib/cloonix/bulk\n"
                     "    wget https://clownix.net/downloads/cnt_demo_bulk/"
                     "wg_trixie.tar.gz\n"
                     "    (cloonix extracts it at the first use)\n"
                     "  or build it:\n"
                     "    cd vm_create/cnt_kvm_demo && ./cnt_wg_trixie.sh"
                     % (node, t))

def address(node):
    for num, lan, addr in WIRING[node]:
        ssh(node, "ip addr add %s dev eth%d" % (addr, num))
        ssh(node, "ip link set dev eth%d up" % num)
    if node in GATEWAY:
        ssh(node, "ip route add default via %s" % GATEWAY[node])
    trace("%s addressed: %s", node,
          " ".join(a for _, _, a in WIRING[node]))

# the traffic The noise is BROADCAST on purpose.

NOISE = '''import socket, sys, time
target, lo, hi, rate = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), \\
    int(sys.argv[4])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
payload = b"r" * 172
gap = 1.0 / rate
port = lo
nxt = time.time()
while True:
    s.sendto(payload, (target, port))
    port += 1
    if port > hi:
        port = lo
    nxt += gap
    d = nxt - time.time()
    if d > 0:
        time.sleep(d)
'''

BEACON = '''import socket, sys, time
target, port = sys.argv[1], int(sys.argv[2])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
n = 0
while True:
    n += 1
    s.sendto(("BEACON " + str(n) + " from alice").encode(), (target, port))
    time.sleep(1.0)
'''

def start_script(node, name, text, args):
    """Put a script on a guest, check it survived the trip, and run it."""
    put(node, "/tmp/%s" % name, text)
    out = ssh(node, "python3 -m py_compile /tmp/%s 2>&1" % name, check=False)
    if out.strip():
        raise RuntimeError("%s did not survive being written to %s:\n%s"
                           % (name, node, out))
    ssh(node, "pkill -f /tmp/%s 2>/dev/null; true" % name, check=False)
    ssh(node, "python3 /tmp/%s %s </dev/null >/tmp/%s.log 2>&1 &"
              % (name, args, name), check=False)
    for _ in range(20):
        if ssh(node, "pgrep -f /tmp/%s >/dev/null && echo up" % name,
               check=False).strip():
            return
        time.sleep(0.5)
    raise RuntimeError("%s did not start on %s:\n%s"
                       % (name, node, ssh(node, "cat /tmp/%s.log" % name,
                                          check=False)))

def traffic_start():
    start_script(N1, "noise.py", NOISE,
                 "%s %d %d %d" % (BCAST, NOISE_LO, NOISE_HI, NOISE_RATE))
    trace("%s is flooding %s on ports %d-%d, about %d a second",
          N1, BCAST, NOISE_LO, NOISE_HI, NOISE_RATE)
    start_script(ALICE, "beacon.py", BEACON, "10.0.0.20 %d" % BEACON_PORT)
    trace("%s sends one beacon a second to %s on port %d",
          ALICE, BOB, BEACON_PORT)
    ssh(ALICE, "pkill -f 'ping -i' 2>/dev/null; true", check=False)
    ssh(ALICE, "ping -i 1 10.0.0.20 </dev/null >/tmp/ping.log 2>&1 &")
    trace("%s pings %s once a second", ALICE, BOB)

def traffic_stop():
    ssh(N1, "pkill -f noise.py 2>/dev/null; true", check=False)
    ssh(ALICE, "pkill -f beacon.py 2>/dev/null; pkill -f 'ping -i' "
               "2>/dev/null; true", check=False)

# what arrives

def classify(pkt):
    """What a packet is, in the terms this demo cares about."""
    d = pkt["data"]
    if len(d) < 14:
        return "runt", None
    et = (d[12] << 8) | d[13]
    if et == 0x0806:
        return "ARP", None
    if et != 0x0800 or len(d) < 34:
        return "other", None
    ihl = (d[14] & 0x0f) * 4
    proto = d[23]
    if proto == 1:
        p = d[14 + ihl:]
        if len(p) < 8:
            return "ICMP", None
        if p[0] in (0, 8):
            return ("ping %s" % ("request" if p[0] == 8 else "reply"),
                    "seq %d" % int.from_bytes(p[6:8], "big"))
        if p[0] == 3:
            what = {0: "network", 1: "host", 3: "port",
                    4: "fragmentation needed"}.get(p[1], "code %d" % p[1])
            return "ICMP %s unreachable" % what, None
        if p[0] == 11:
            return "ICMP time exceeded", None
        return "ICMP type %d" % p[0], None
    if proto != 17:
        return "IP proto %d" % proto, None
    base = 14 + ihl
    if len(d) < base + 8:
        return "UDP", None
    dport = int.from_bytes(d[base + 2:base + 4], "big")
    payload = d[base + 8:]
    if dport == BEACON_PORT:
        try:
            return "BEACON", payload.decode("ascii", "replace").strip()
        except Exception:                               # noqa: BLE001
            return "BEACON", None
    if NOISE_LO <= dport <= NOISE_HI:
        return "noise udp %d" % dport, None
    return "UDP %d" % dport, None

class Watch:
    """One subscription, with one filter, counting what gets through."""

    def __init__(self, node, num, filt):
        self.filt = filt
        self.live = False
        self.lock = threading.Lock()
        self.kinds = {}
        self.total = 0
        self.samples = []
        self.refused = None
        capturable(node, num)
        cmd = CLOONIX + ["cli", NET, "sniff", node, "eth%d" % num, "--bytes"]
        if filt:
            cmd += filt.split()
        self.proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
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
            kind, extra = classify(pkt)
            with self.lock:
                self.total += 1
                # The noise is counted as one kind, not a hundred: a table
                # with one row per destination port would be as unreadable as
                # the traffic it is describing.
                key = "noise udp %d-%d" % (NOISE_LO, NOISE_HI) \
                    if kind.startswith("noise") else kind
                self.kinds[key] = self.kinds.get(key, 0) + 1
                if extra and len(self.samples) < 6:
                    self.samples.append("%s: %s" % (key, extra))
        self.ready_evt.set()

    def ready(self, timeout=15):
        self.ready_evt.wait(timeout)
        if not self.live:
            err = self.proc.stderr.read() if self.proc.stderr else ""
            self.refused = err.strip() or "no answer"
        return self.live

    def reset(self):
        with self.lock:
            self.kinds = {}
            self.total = 0
            self.samples = []

    def result(self):
        with self.lock:
            return self.total, dict(self.kinds), list(self.samples)

    def stop(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()

def run_filter(filt, seconds=WINDOW):
    """Watch alice's interface for a while through one filter."""
    w = Watch(ALICE, 0, filt)
    if not w.ready():
        story("      the server refused this filter: %s" % w.refused)
        w.stop()
        return None
    try:
        time.sleep(1.5)          # let the subscription really start
        w.reset()
        time.sleep(seconds)
        total, kinds, samples = w.result()
    finally:
        w.stop()
    return {"filter": filt, "total": total, "kinds": kinds,
            "samples": samples, "rate": total / seconds}

def show_count(r, title):
    story("    %s" % title)
    story("      filter: %s" % (r["filter"] if r["filter"]
                                else "(none, everything is relayed)"))
    story("      %d packets in %.0f s, %.0f a second" %
          (r["total"], WINDOW, r["rate"]))
    if r["kinds"]:
        for k, v in sorted(r["kinds"].items(), key=lambda kv: -kv[1]):
            story("        %-28s %6d" % (k, v))
    else:
        story("        (nothing at all)")
    if r["samples"]:
        story("      what they said:")
        for s in r["samples"]:
            story("        %s" % s)
    story()

LAYOUT = {
    ALICE: (-190, 0),
    LAN: (0, 0),
    BOB: (190, -150),
    N1: (190, 0),
    N2: (190, 150),
}

ETHS = {
    (ALICE, "eth0"): 90,
    (BOB, "eth0"): 232,
    (N1, "eth0"): 270,
    (N2, "eth0"): 308,
}

def place():
    for name, (x, y) in LAYOUT.items():
        cli("cnf", name, "x=%s" % x, "y=%s" % y, ack=ACK_FAST)
    for (name, num), deg in ETHS.items():
        cli("cnf", name, num, "angle=%s" % deg, ack=ACK_FAST)

def main():
    step("looking for an image with python3 in it")
    rootfs = choose_rootfs()
    story("rootfs %s: a noisy lan with three useful packets a second in it"
          % rootfs)
    story("""
    alice -- lan1 -- bob        noisy1 -- lan1 -- noisy2
   10.0.0.10      10.0.0.20    10.0.0.31        10.0.0.32

    noisy1 broadcasts about %d datagrams a second on ports %d-%d,
    which is the shape of rtp and the shape of most of what a real
    network carries.

    Meanwhile alice sends bob one beacon a second on port %d, and
    pings him once a second. Three packets a second that matter,
    inside several hundred that do not.

    The demo then watches the SAME interface four times over the SAME
    traffic, changing only the filter.
""" % (NOISE_RATE, NOISE_LO, NOISE_HI, BEACON_PORT))

    step("erasing the net, then placing the items")
    erase_net()
    place()

    step("creating the four containers")
    in_parallel(lambda n: create_cnt(n, rootfs), list(ALL), "add cnt")

    step("waiting for them to answer")
    in_parallel(wait_ready, list(ALL), "wait_ready")
    tools_present(ALICE, "python3", "ping")

    step("cabling them")
    for node in ALL:
        for num, lan, _ in WIRING[node]:
            cli("plug", node, "eth%d" % num, lan)

    step("addressing them")
    in_parallel(address, list(ALL), "address")

    step("starting the noise and the traffic that matters")
    traffic_start()
    time.sleep(2)

    results = []
    try:
        step("act 1: no filter, which is what there was before")
        story()
        r = run_filter("")
        if r is None:
            sys.exit("could not sniff the interface")
        results.append(r)
        show_count(r, "everything on the wire:")
        story("    Somewhere in that is one beacon a second. Finding it")
        story("    means decoding every packet at this end to discover")
        story("    that almost none of them is the one wanted -- and each")
        story("    of them was base64'd and sent across the doorways")
        story("    connection to get here.")
        story()

        step("act 2: the same traffic, with the noise refused")
        story()
        r = run_filter("not udp portrange %d-%d" % (NOISE_LO, NOISE_HI))
        results.append(r)
        show_count(r, "everything except the flood:")
        story("    Nothing about the lan changed. noisy1 is still sending")
        story("    at the same rate and alice's interface still carries it")
        story("    all -- the difference is that the switch now throws it")
        story("    away instead of sending it here.")
        story()
        story("    Note this is a NEGATIVE filter and it still works the")
        story("    allow way round: 'not udp portrange' is a description of")
        story("    what to keep, not a list of what to block. Everything")
        story("    that does not match is still dropped.")
        story()

        step("act 3: only the beacons, and now they can be read")
        story()
        r = run_filter("udp port %d" % BEACON_PORT)
        results.append(r)
        show_count(r, "the beacons alone:")
        story("    This is the filter a scenario would use. It waits for")
        story("    one thing, it says so, and the four packets a second it")
        story("    reads are four it wanted.")
        story()

        step("act 4: only the pings, with their sequence numbers")
        story()
        r = run_filter("icmp")
        results.append(r)
        show_count(r, "the pings alone:")
        story("    A different question, a different filter, the same")
        story("    interface and the same second. Nothing was restarted.")
        story()
        story("    The unreachables in that list are worth a look. Nothing")
        story("    on bob listens on port 53, so every beacon is answered")
        story("    by bob's kernel saying so -- and \"icmp\" catches that")
        story("    reply as readily as it catches the pings, because it is")
        story("    the same protocol. A filter says what to keep, not what")
        story("    you meant.")
        story()
    finally:
        traffic_stop()

    step("what the four filters cost")
    story()
    base = results[0]["total"] if results and results[0]["total"] else 1
    story("      %-34s %8s %9s %9s" % ("filter", "packets", "a second",
                                       "of the whole"))
    story("      " + "-" * 64)
    for r in results:
        story("      %-34s %8d %9.0f %8.1f%%"
              % (r["filter"] if r["filter"] else "(none)",
                 r["total"], r["rate"], 100.0 * r["total"] / base))
    story()
    story("    The last column is the part worth taking away. Everything")
    story("    not counted was rejected inside the switch: not encoded,")
    story("    not sent, not decoded. On a busy lan that is most of the")
    story("    work a subscriber was previously doing to find out that a")
    story("    packet was not the one it wanted.")
    story()

    ok = (len(results) == 4 and results[0]["total"] > 0
          and results[2]["total"] > 0
          and results[2]["total"] < results[0]["total"]
          and results[3]["total"] > 0)
    if not ok:
        sys.exit("\nthe filters did not behave as they should,"
                 " see the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")

    story("""
the network is left up, and quiet: the noise and the beacons were stopped.
Things worth trying:

  start the noise again and watch it live, unfiltered, to see why anyone
  would want a filter:
    cloonix cli %s ssh %s "python3 /tmp/noise.py %s %d %d %d &"
    cloonix cli %s sniff %s eth0 --dump

  then the same thing with a filter, in another terminal:
    cloonix cli %s sniff %s eth0 --dump udp port %d

  narrow it down by machine rather than by protocol:
    cloonix cli %s sniff %s eth0 --dump host 10.0.0.20 and not icmp

  and ask for something the filter does not understand, to see it refused
  rather than quietly matching nothing:
    cloonix cli %s sniff %s eth0 "sip.method == INVITE"
""" % (NET, N1, BCAST, NOISE_LO, NOISE_HI, NOISE_RATE,
       NET, ALICE, NET, ALICE, BEACON_PORT, NET, ALICE, NET, ALICE))

main()
