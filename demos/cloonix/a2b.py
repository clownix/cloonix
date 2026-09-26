#!/usr/bin/env python3
r"""a2b - a cable that can be told to be bad, and what tcp does about it.

    ./a2b.py [net]                 the net defaults to nemo

Two machines and one cable::

  hostA -- lanA -- [ a2b ] -- lanB -- hostB
 10.0.0.1                            10.0.0.2

An a2b is a cloonix object with no equivalent in the other demos: a piece of
wire that can be told to misbehave, in each direction separately::

  cnf cable A2B delay=100      100 ms one way
  cnf cable A2B loss=100       1% of packets dropped
  cnf cable A2B rate=100       100 kilo-octets per second
  cnf cable A2B silentms=200   bursty, 200 ms at a time

Both sides are one broadcast domain -- it is a cable, not a router -- so the
two machines share a subnet and nothing has to be routed.

Both ends are sniffed throughout, and that is the point. A tool at one end
reports a round trip and a loss percentage; two sniffers report which
DIRECTION the time went in, and which particular packets never arrived.

  ACT 1  a clean cable. A slow ping, and the same packet timed leaving one
         wire and arriving on the other, so the method is established
         before there is anything to find.
  ACT 2  100 ms each way. The round trip doubles, and the two halves are
         measured SEPARATELY on the same packets -- which is what one tool
         at one end cannot do.
  ACT 3  1% loss, with ping -f. Five hundred numbered packets go in, and
         the demo names the sequence numbers that never came out.
  ACT 4  a rate cap, with udp sent 30% faster than it allows. udp does not
         back off, so the excess is visible as datagrams that entered the
         cable and never left it.

The traffic is chosen deliberately. icmp for the first three: one packet
out, one back, a sequence number on each, no window and no congestion
control, so what the cable does is not mixed up with what a protocol
decided to do about it. udp for the last: a rate cap and tcp argue with
each other -- tcp finds the ceiling and sits under it, so the measurement
comes out at the cap and proves only that tcp works.

Needs cloonix on the PATH and nothing else. Standard library only, like
cloonix itself, so it runs wherever the client does.

Image: wg_trixie or proxy_trixie
Size:  2 containers, 2 lans, 1 a2b

THE PACKET TO CATCH
-------------------
  This is the demo where snf is the instrument and not an illustration.
  Nothing else can answer the question it asks.

    cloonix cli nemo cnf hostA eth0 sniff=on
    cloonix cli nemo cnf hostB eth0 sniff=on

    cloonix cli nemo sniff hostA eth0 --dump icmp
    cloonix cli nemo sniff hostB eth0 --dump icmp

  The packet to catch is the one that appears on the first and never on
  the second. A tool at one end reports a percentage; two captures at the
  two ends of the same cable say which DIRECTION the loss was in and
  exactly which packets never arrived.

  With delay set, the same pair gives the one-way time rather than the
  round trip, which ping cannot separate.

Important
  udp for the rate act and icmp for the rest, deliberately. tcp finds the
  ceiling and sits under it, so a rate measurement over tcp comes out at
  the cap and proves only that tcp works.
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

# Any image with python3 and curl in it.
ROOTFS_CHOICES = ("wg_trixie", "proxy_trixie")

HA, HB = "hostA", "hostB"
ALL = (HA, HB)
CABLE = "cable"                 # the a2b itself
LAN_A, LAN_B = "lanA", "lanB"

# One subnet on both sides: an a2b joins two lans into one wire.
WIRING = {
    HA: [(0, LAN_A, "10.0.0.1/24")],
    HB: [(0, LAN_B, "10.0.0.2/24")],
}
GATEWAY = {}

IP_B = "10.0.0.2"
UDP_PORT = 9999

CLEAN = [("delay", 0), ("loss", 0), ("rate", 0), ("silentms", 0)]

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
                 "  falls back to nemo:   ./a2b.py <net>"
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
            sys.exit("%s has no %s: this image has no python3 or curl.\n"
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

# the cable

def impair(cmd, val, both=True):
    """Tell the cable to misbehave.

    A2B and B2A are the two directions of the wire and they are set
    independently, which is genuinely useful -- a satellite link is often
    asymmetric -- and a trap for a demo: setting only one direction gives a
    round trip of half what you asked for.
    """
    for d in ((0, 1) if both else (0,)):
        way = ("A2B", "B2A")[d]
        r = cli("cnf", CABLE, way, "%s=%d" % (cmd, val))
        if r.returncode:
            raise RuntimeError("the cable refused '%s %s=%d': %s"
                               % (way, cmd, val,
                                  (r.stdout + r.stderr).strip() or "no reason"))
    trace("cable: %s = %d in both directions", cmd, val)

def clean_cable():
    for cmd, val in CLEAN:
        impair(cmd, val)

# The instruments Everything below is measured on the wire, at BOTH ends,
# because that is the only place a cable can be judged.

def decode(pkt):
    """icmp echo with its sequence number, or a udp datagram with its size."""
    d = pkt["data"]
    if len(d) < 34 or d[12:14] != b"\x08\x00":
        return None
    proto = d[23]
    ihl = (d[14] & 0x0f) * 4
    base = 14 + ihl
    src = ".".join(str(b) for b in d[26:30])
    dst = ".".join(str(b) for b in d[30:34])
    if proto == 1:
        p = d[base:]
        if len(p) < 8:
            return None
        kind = {8: "request", 0: "reply"}.get(p[0])
        if not kind:
            return None
        return {"kind": "ICMP", "what": kind, "src": src, "dst": dst,
                "seq": int.from_bytes(p[6:8], "big"),
                "bytes": int.from_bytes(d[16:18], "big")}
    if proto == 17:
        if len(d) < base + 8:
            return None
        dport = int.from_bytes(d[base + 2:base + 4], "big")
        if dport != UDP_PORT:
            return None
        return {"kind": "UDP", "what": "datagram", "src": src, "dst": dst,
                "seq": None,
                "bytes": int.from_bytes(d[base + 4:base + 6], "big") - 8}
    return None

class Wire:
    """One sniffed interface, remembering what crossed it and when."""

    def __init__(self, node, num, label):
        self.node, self.num, self.label = node, num, label
        self.live = False
        self.lock = threading.Lock()
        self.icmp = []          # (when, what, seq)
        self.udp_bytes = 0
        self.udp_count = 0
        self.udp_first = None
        self.udp_last = None
        capturable(node, num)
        self.proc = subprocess.Popen(
            CLOONIX + ["cli", NET, "sniff", node, "eth%d" % num, "--bytes"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1)
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
            m = decode(pkt)
            if not m:
                continue
            with self.lock:
                if m["kind"] == "ICMP":
                    self.icmp.append((pkt["when"], m["what"], m["seq"]))
                else:
                    self.udp_bytes += m["bytes"]
                    self.udp_count += 1
                    if self.udp_first is None:
                        self.udp_first = pkt["when"]
                    self.udp_last = pkt["when"]
        self.ready_evt.set()

    def ready(self, timeout=15):
        self.ready_evt.wait(timeout)
        if not self.live:
            err = self.proc.stderr.read() if self.proc.stderr else ""
            story("  could not sniff %s eth%d: %s"
                  % (self.node, self.num, err.strip() or "no answer"))
        return self.live

    def reset(self):
        with self.lock:
            self.icmp = []
            self.udp_bytes = 0
            self.udp_count = 0
            self.udp_first = None
            self.udp_last = None

    def seqs(self, what):
        with self.lock:
            return sorted(s for _w, k, s in self.icmp if k == what)

    def first(self, what, seq):
        with self.lock:
            for w, k, s in sorted(self.icmp):
                if k == what and s == seq:
                    return w
        return None

    def udp(self):
        """count, bytes, and the rate over the span this wire actually saw.

        Dividing by the six seconds the sender was told to run for is wrong
        in both directions: the sender takes a moment to start, and a shaped
        wire goes on emitting after the sender has stopped, because what it
        is emitting is a queue. Timing the traffic by its own first and last
        packet is the only figure that means anything.
        """
        with self.lock:
            span = None
            if self.udp_first is not None and self.udp_last is not None:
                span = self.udp_last - self.udp_first
            rate = (self.udp_bytes / span) if span and span > 0.5 else None
            return self.udp_count, self.udp_bytes, span, rate

    def stop(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()

def open_wires():
    w = [Wire(HA, 0, "hostA eth0"), Wire(HB, 0, "hostB eth0")]
    if not all(x.ready() for x in w):
        for x in w:
            x.stop()
        return None
    time.sleep(1.5)
    for x in w:
        x.reset()
    return w

# what the acts do

def ping_slow(count=5):
    out = ssh(HA, "ping -c %d -i 0.4 -W 3 %s 2>&1" % (count, IP_B),
              check=False)
    avg = None
    for line in out.split("\n"):
        if "rtt min/avg/max" in line or "round-trip" in line:
            try:
                avg = float(line.split("=")[1].split("/")[1])
            except (IndexError, ValueError):
                pass
    return avg, out

def ping_flood(count=500):
    """ping -f: as fast as the link will take them, with a sequence number
    on every one. The fastest way to put several hundred numbered packets
    on a wire and see which of them do not come out the other side."""
    out = ssh(HA, "ping -f -c %d -W 3 %s 2>&1" % (count, IP_B), check=False)
    lost = None
    for line in out.split("\n"):
        if "packet loss" in line:
            for word in line.replace(",", " ").split():
                if word.endswith("%"):
                    try:
                        lost = float(word[:-1])
                    except ValueError:
                        pass
    return lost, out

def one_way(wires, what, seq):
    """How long a single packet took to cross the cable.

    The same packet, seen leaving one wire and arriving on the other. Two
    sniffers with their own clocks are a few milliseconds apart, so this is
    only worth reading when the answer is tens of milliseconds -- which is
    exactly when it is interesting.
    """
    a = wires[0].first(what, seq)
    b = wires[1].first(what, seq)
    if a is None or b is None:
        return None
    return (b - a) * 1000.0 if what == "request" else (a - b) * 1000.0

UDP_SENDER = '''import socket, sys, time
target, port, rate, secs, size = sys.argv[1], int(sys.argv[2]), \\
    int(sys.argv[3]), float(sys.argv[4]), int(sys.argv[5])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
payload = b"u" * size
per_sec = max(rate // size, 1)
gap = 1.0 / per_sec
sent = 0
end = time.time() + secs
nxt = time.time()
while time.time() < end:
    s.sendto(payload, (target, port))
    sent += size
    nxt += gap
    d = nxt - time.time()
    if d > 0:
        time.sleep(d)
print(sent)
'''

UDP_RECEIVER = '''import socket, sys
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
s.bind(("", int(sys.argv[1])))
got = 0
s.settimeout(30)
try:
    while True:
        d, _ = s.recvfrom(4096)
        got += len(d)
except Exception:
    pass
open("/tmp/udp_got", "w").write(str(got))
'''

def udp_start_receiver():
    put(HB, "/tmp/udp_recv.py", UDP_RECEIVER)
    out = ssh(HB, "python3 -m py_compile /tmp/udp_recv.py 2>&1", check=False)
    if out.strip():
        raise RuntimeError("the udp receiver did not survive:\n%s" % out)
    ssh(HB, "pkill -f udp_recv.py 2>/dev/null; rm -f /tmp/udp_got; true",
        check=False)
    ssh(HB, "python3 /tmp/udp_recv.py %d </dev/null >/dev/null 2>&1 &"
            % UDP_PORT)
    time.sleep(1)

def udp_send(rate, secs, size=1000):
    put(HA, "/tmp/udp_send.py", UDP_SENDER)
    out = ssh(HA, "python3 -m py_compile /tmp/udp_send.py 2>&1", check=False)
    if out.strip():
        raise RuntimeError("the udp sender did not survive:\n%s" % out)
    got = ssh(HA, "python3 /tmp/udp_send.py %s %d %d %s %d 2>&1"
                  % (IP_B, UDP_PORT, rate, secs, size), check=False)
    try:
        return int(got.strip().split()[-1])
    except (ValueError, IndexError):
        return None

def udp_received():
    ssh(HB, "pkill -f udp_recv.py 2>/dev/null; true", check=False)
    time.sleep(1)
    out = ssh(HB, "cat /tmp/udp_got 2>/dev/null", check=False).strip()
    try:
        return int(out)
    except ValueError:
        return None

LAYOUT = {
    HA: (-380, 0),
    LAN_A: (-190, 0),
    CABLE: (0, 0),
    LAN_B: (190, 0),
    HB: (380, 0),
}

ETHS = {
    (CABLE, "A"): 270,
    (CABLE, "B"): 90,
    (HA, "eth0"): 90,
    (HB, "eth0"): 270,
}

def place():
    for name, (x, y) in LAYOUT.items():
        cli("cnf", name, "x=%s" % x, "y=%s" % y, ack=ACK_FAST)
    for (name, num), deg in ETHS.items():
        cli("cnf", name, num, "angle=%s" % deg, ack=ACK_FAST)

def main():
    step("looking for an image with python3 in it")
    rootfs = choose_rootfs()
    story("rootfs %s: two machines and one cable that can be told to be bad"
          % rootfs)
    story("""
    hostA -- lanA -- [ a2b ] -- lanB -- hostB
   10.0.0.1                            10.0.0.2

    An a2b is a piece of wire cloonix can be told to spoil, in each
    direction separately:

      cnf cable A2B delay=100   100 ms one way
      cnf cable A2B loss=100    1% of packets dropped
      cnf cable A2B rate=100    100 kilo-octets a second

    Both ends are sniffed throughout. A tool at one end can tell you a
    round trip and a loss percentage; two sniffers tell you which
    DIRECTION the time went in, and which particular packets never
    arrived.
""")

    step("erasing the net, then placing the items")
    erase_net()
    place()

    step("creating the two containers")
    in_parallel(lambda n: create_cnt(n, rootfs), list(ALL), "add cnt")

    step("waiting for them to answer")
    in_parallel(wait_ready, list(ALL), "wait_ready")
    tools_present(HA, "ping", "python3")

    step("creating the cable and plugging both ends in")
    r = cli("add", "a2b", CABLE)
    if r.returncode:
        sys.exit("could not create the a2b: %s"
                 % ((r.stdout + r.stderr).strip() or "no reason given"))
    for node in ALL:
        for num, lan, _ in WIRING[node]:
            cli("plug", node, "eth%d" % num, lan)
    cli("plug", CABLE, "A", LAN_A)
    cli("plug", CABLE, "B", LAN_B)
    trace("cable: side A on %s, side B on %s", LAN_A, LAN_B)

    step("addressing them")
    in_parallel(address, list(ALL), "address")
    clean_cable()

    step("act 1: a clean cable, and where the time goes")
    story()
    wires = open_wires()
    if wires is None:
        sys.exit("could not sniff both ends")
    try:
        base_rtt, out = ping_slow(5)
        time.sleep(1.5)
        base_one = [one_way(wires, "request", s) for s in range(1, 6)]
        base_one = [x for x in base_one if x is not None]
        if base_rtt is None:
            story(out)
            sys.exit("the two machines cannot reach each other")
        story("      ping says the round trip is %.2f ms" % base_rtt)
        if base_one:
            story("      the wires say one crossing takes %.2f ms"
                  % (sum(base_one) / len(base_one)))
        story()
        story("    Two independent measurements of the same cable: ping's")
        story("    own arithmetic, and the gap between the same packet")
        story("    leaving hostA and arriving at hostB. On a clean cable")
        story("    they agree that there is nothing to see.")
        story()

        step("act 2: 100 ms each way, caught on both wires")
        story()
        impair("delay", 100)
        time.sleep(1)
        for w in wires:
            w.reset()
        ms, _ = ping_slow(5)
        time.sleep(1.5)
        crossings = [one_way(wires, "request", s) for s in range(1, 6)]
        crossings = [x for x in crossings if x is not None]
        backs = [one_way(wires, "reply", s) for s in range(1, 6)]
        backs = [x for x in backs if x is not None]
        story("      %-34s %s" % ("clean, round trip",
                                  "%.2f ms" % base_rtt))
        story("      %-34s %s" % ("delayed, round trip",
                                  "%.2f ms" % ms if ms else "(no answer)"))
        story()
        if crossings:
            story("      the echo REQUEST, hostA's wire to hostB's : "
                  "%.1f ms" % (sum(crossings) / len(crossings)))
        if backs:
            story("      the echo REPLY, hostB's wire to hostA's   : "
                  "%.1f ms" % (sum(backs) / len(backs)))
        story()
        story("    That is what two sniffers give that one tool cannot. The")
        story("    round trip is one number and it hides where the time")
        story("    went; these are the two halves, measured separately, on")
        story("    the same packets. Set only A2B and the second line")
        story("    stays at nothing while the first goes to 100.")
        story()
        ok_delay = bool(crossings) and (sum(crossings) / len(crossings)) > 50
        impair("delay", 0)

        step("act 3: 1% loss, and exactly which packets went missing")
        story()
        impair("loss", 100)
        time.sleep(0.5)
        for w in wires:
            w.reset()
        lost_pct, out = ping_flood(500)
        time.sleep(2.0)
        sent = wires[0].seqs("request")
        arrived = wires[1].seqs("request")
        missing = sorted(set(sent) - set(arrived))
        story("      ping -f sent 500 and reported %s lost"
              % ("%.1f%%" % lost_pct if lost_pct is not None else "?"))
        story()
        story("      echo requests on hostA's wire, as sent    : %d"
              % len(sent))
        story("      echo requests on hostB's wire, as arrived : %d"
              % len(arrived))
        story("      sequence numbers that never made it       : %d"
              % len(missing))
        if missing:
            shown = ", ".join(str(x) for x in missing[:12])
            story("        %s%s" % (shown, " ..." if len(missing) > 12 else ""))
        story()
        story("    A percentage is a summary. These are the packets: number")
        story("    %s went into the cable and did not come out, and the"
              % (missing[0] if missing else "N"))
        story("    sequence on hostB's wire steps straight over it.")
        story()
        story("    Note the two numbers ping gives and the wire gives are")
        story("    not the same thing. ping counts a ping as lost if either")
        story("    direction ate it; the wires count each direction on its")
        story("    own, which is why %s%% and %d are different figures about"
              % ("%.1f" % lost_pct if lost_pct is not None else "?",
                 len(missing)))
        story("    the same cable.")
        story()
        ok_loss = len(missing) > 0
        impair("loss", 0)

        step("act 4: udp sent 30% faster than the cap allows")
        story()
        CAP_KB = 100
        cap_bytes = CAP_KB * 1000
        over = int(cap_bytes * 1.3)
        impair("rate", CAP_KB)
        time.sleep(0.5)
        for w in wires:
            w.reset()
        udp_start_receiver()
        udp_send(over, 6.0)
        time.sleep(1.5)
        got_bytes = udp_received()
        a_count, a_bytes, a_span, a_rate = wires[0].udp()
        b_count, b_bytes, b_span, b_rate = wires[1].udp()
        impair("rate", 0)

        def rate_of(r):
            return ("%d B/s" % r) if r else "(too short to time)"

        story("      the cap asked for                : %d 000 B/s"
              % CAP_KB)
        story("      hostA was told to send at        : %d B/s" % over)
        story("      on hostA's wire  : %4d datagrams, %7d B over %.1f s"
              "  = %s" % (a_count, a_bytes, a_span or 0, rate_of(a_rate)))
        story("      on hostB's wire  : %4d datagrams, %7d B over %.1f s"
              "  = %s" % (b_count, b_bytes, b_span or 0, rate_of(b_rate)))
        if got_bytes is not None:
            story("      the receiver counted %d B" % got_bytes)
        story()
        if a_rate and b_rate:
            story("    %d datagrams went in and %d came out: the cable threw"
                  % (a_count, b_count))
            story("    %d of them away, and udp neither noticed nor slowed"
                  % (a_count - b_count))
            story("    down. That is the whole difference from tcp.")
            story()
            story("    The rate that got through was %d B/s against a cap of"
                  % b_rate)
            story("    %d. %s" % (cap_bytes,
                                  "Within a few per cent, which is a shaper"
                                  " doing its job."
                                  if abs(b_rate - cap_bytes) < cap_bytes * 0.1
                                  else "Above it, which is the shaper"
                                       " allowing a burst rather than"
                                       " metering every packet."))
        story()
        story("    This is why udp and not tcp. A rate cap and tcp argue")
        story("    with each other: tcp finds the ceiling and sits under")
        story("    it, so the measurement comes out at whatever the cap")
        story("    was and proves only that tcp works. udp does not back")
        story("    off, so the excess is visible as packets that entered")
        story("    the cable and never left it.")
        story()
        ok_rate = bool(b_count) and b_count < a_count
    finally:
        for w in wires:
            w.stop()
        clean_cable()

    if not (ok_delay and ok_loss and ok_rate):
        sys.exit("\nsomething did not show what it should,"
                 " see the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")

    story("""
the cable is left in place, and clean. Things worth trying:

  a satellite link, and the same ping caught at both ends -- half a second
  up and down, and every crossing visible on its own:
    cloonix cli %s cnf %s A2B delay=250
    cloonix cli %s cnf %s B2A delay=250
    cloonix cli %s ssh %s "ping -c 5 %s"

  spoil ONE direction only, which is what an asymmetric link really is,
  and watch the two halves of the round trip stop matching:
    cloonix cli %s cnf %s A2B delay=200

  make it bursty rather than slow, which breaks things a rate cap does
  not:
    cloonix cli %s cnf %s A2B silentms=300

  and run another demo across it. Every one of them has two sides that
  could have this cable in the middle, and a protocol that has never been
  asked what it does about a bad one.
""" % (NET, CABLE, NET, CABLE, NET, HA, IP_B, NET, CABLE, NET, CABLE))

main()
