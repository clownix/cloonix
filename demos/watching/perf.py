#!/usr/bin/env python3
r"""perf - what the path can carry, and what it costs to find out.

    ./perf.py [net]                 the net defaults to nemo

Four containers, one impaired cable::

  sender --- lan_a --- [a2b] --- lan_b --- receiver
 10.0.1.10                               10.0.2.10
                |                   |
              probe1              probe2
            (mtr, hop by hop)   (a second flow)

The a2b is the instrument: it can be told to delay, to lose and to limit
a rate, each direction on its own. Every number below is measured over it
rather than argued about.

  ACT 1  the path as it is: iperf3 flat out, and mtr saying what is
         between the two ends. A baseline nobody has done anything to.
  ACT 2  100 ms of delay, and nothing lost. The throughput collapses
         anyway, which is the bandwidth-delay product and the first
         surprise of the demo.
  ACT 3  1 % loss, no delay. Then 1 % loss AND delay together, which is
         far worse than the sum of the two -- and is what a real bad link
         is.
  ACT 4  a rate limit, two flows at once, and what "fair" turns out to
         mean. The sniffer counts what really crossed, against what
         iperf3 says it sent.

Needs cloonix on the PATH and nothing else. Standard library only, like
cloonix itself.

Image: perf_trixie
Size:  4 containers, 2 lans, 1 a2b

THE PACKET TO CATCH
-------------------
  This is the demo where the filter matters most, and for a reason that
  is easy to miss: a filter is applied on the server, and what does not
  match never crosses the wire to the client at all. Subscribing to a
  10 Gbit/s iperf3 run unfiltered would drown the subscription and change
  the number being measured. A narrow filter does not.

    cloonix cli nemo cnf h2 eth0 sniff=on
    cloonix cli nemo sniff h2 eth0 --dump tcp port 5201 and less 100

  "less 100" keeps the small packets and drops the full-size ones, so
  what arrives is the control conversation and the pure acks -- a few
  per second instead of hundreds of thousands. The packet to catch is
  the iperf3 control exchange on 5201 at the start and the end of a run,
  which is where the JSON summary is handed back.

  ACT 3 is where a capture explains a number. With 100 ms of delay on
  the a2b and a default window, throughput collapses:

    cloonix cli nemo sniff h2 eth0 --dump tcp port 5201 and less 100

  and the acks come back in a rhythm that shows the sender waiting. The
  bandwidth-delay product stops being a formula at that point.

  Say so in the report: any number taken while a snf is running is a
  number taken with a sniffer running, and should be labelled that way.

Important
  Pairs with demos/cloonix/a2b.py throughout.
  Report the container count, the host and the kernel with any number, or
  the number means nothing later.
"""
import os
import shlex
import re
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
    in_parallel, place, run, show, step, story, wait_exists)

ROOTFS_CHOICES = ("perf_trixie",)

HISTORY = """\
The bandwidth-delay product measured here is the 1988 finding of Van
Jacobson at Lawrence Berkeley Laboratory, whose congestion control
saved the internet from collapse and put a window between throughput
and round trip. iperf began at NLANR around 2003 as a portable heir
of ttcp of 1984; iperf3 is the 2014 rewrite of the ESnet group at the
same Berkeley laboratory, by Jon Dugan and Brian Tierney's team. mtr,
Matt's traceroute, was written by Matt Kimball in 1997 and merged
traceroute, of Van Jacobson again, with ping into one screen; Roger
Wolff maintains it.

Today: iperf3 and mtr are the first two tools of a network engineer facing a
slow path, and the bandwidth-delay product is why a single TCP flow across an
ocean needs large windows.
"""

SND, RCV, P1, P2 = "sender", "receiver", "probe1", "probe2"
ALL = (SND, RCV, P1, P2)
CABLE = "cable"
LAN_A, LAN_B = "lan_a", "lan_b"

WIRING = {
    SND: [(0, LAN_A, "10.0.1.10/24")],
    P1:  [(0, LAN_A, "10.0.1.11/24")],
    RCV: [(0, LAN_B, "10.0.2.10/24")],
    P2:  [(0, LAN_B, "10.0.2.11/24")],
}
RCV_IP = "10.0.2.10"
P2_IP = "10.0.2.11"

IPERF_PORT = 5201
IPERF_PORT2 = 5202
SECONDS = 6

def ssh(node, cmd, check=True, timeout=120):
    """One shell command inside a guest.

    Its own function rather than a run() line because the command is a
    single argument with spaces and quotes in it: quoting it into a line
    and splitting it back would be a way of corrupting it. So the line is
    printed for reading and pasting, and the argument vector is built
    straight from cmd.
    """
    show('cloonix cli %s ssh %s "%s"' % (NET, node, cmd))
    try:
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node, cmd],
                           capture_output=True, text=True, timeout=timeout,
                           stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired:
        if check:
            raise RuntimeError("%s: %s -> took longer than %ds"
                               % (node, cmd, timeout))
        return ""
    if check and r.returncode:
        raise RuntimeError("%s: %s -> %s"
                           % (node, cmd,
                              (r.stderr or r.stdout).strip() or "no output"))
    return r.stdout

def choose_rootfs():
    said = ""
    for cand in ROOTFS_CHOICES:
        r = run("cloonix cli %s add cnt probe_perf eth=1 %s" % (NET, cand))
        ok = (r.returncode == 0) and wait_exists("probe_perf", 20)
        run("cloonix --wait 0.3 cli %s del probe_perf" % NET)
        if ok:
            return cand
        said = (r.stdout + r.stderr).strip() or "nothing"
    sys.exit("the server would not start a container on %s.\n"
             "  it said: %s\n"
             "  fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_demo_bulk/"
             "perf_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_perf_trixie.sh"
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

def address(node):
    for num, _lan, addr in WIRING[node]:
        ssh(node, "ip addr add %s dev eth%d" % (addr, num))
        ssh(node, "ip link set dev eth%d up" % num)
        # the cable is a wire, not a router: the other side is on-link
        ssh(node, "ip route add 10.0.0.0/16 dev eth%d" % num)

def tools_present(node, *tools):
    for t in tools:
        r = run('cloonix cli %s ssh %s "command -v %s"' % (NET, node, t))
        if r.returncode:
            sys.exit("%s has no %s: this is not the perf image.\n"
                     "  fetch it:\n"
                     "    cd /var/lib/cloonix/bulk\n"
                     "    wget https://clownix.net/downloads/cnt_demo_bulk/"
                     "perf_trixie.tar.gz\n"
                     "    (cloonix extracts it at the first use)\n"
                     "  or build it:\n"
                     "    cd vm_create/cnt_kvm_demo && ./cnt_perf_trixie.sh"
                     % (node, t))

# the cable: the a2b, a wire with loss, delay and rate on it.

BOTH_WAYS = (0, 1)

def cable_set(cmd, val):
    for d in BOTH_WAYS:
        r = run("cloonix cli %s cnf %s %s %s=%d"
            % (NET, CABLE, ("A2B", "B2A")[d], cmd, val))
        if r.returncode:
            raise RuntimeError("the cable refused %s=%d: %s"
                               % (cmd, val, (r.stdout + r.stderr).strip()))

def cable_clear():
    for cmd in ("delay", "loss", "rate"):
        cable_set(cmd, 0)

def cable_says(delay=0, loss=0, rate=0):
    bits = []
    bits.append("%d ms delay" % delay if delay else "no delay")
    bits.append("%d %% loss" % loss if loss else "no loss")
    bits.append("%d Mbit/s cap" % rate if rate else "no cap")
    return ", ".join(bits)

# iperf3 and mtr

def iperf_server(node, port):
    ssh(node, "pkill -f 'iperf3 -s -p %d' 2>/dev/null; true" % port,
        check=False)
    ssh(node, "iperf3 -s -p %d -D >/dev/null 2>&1" % port, check=False)
    for _ in range(20):
        if ssh(node, "pgrep -f 'iperf3 -s -p %d' >/dev/null && echo up"
                     % port, check=False).strip():
            return
        time.sleep(0.5)
    raise RuntimeError("iperf3 would not start on %s port %d" % (node, port))

MBITS = re.compile(r"([\d.]+)\s+([KMG]?)bits/sec")
RETR = re.compile(r"bits/sec\s+(\d+)\s+")

def iperf_run(node, target, port=IPERF_PORT, secs=SECONDS, extra=""):
    """One transfer, and the sender's own summary of it.

    -f m so the unit never changes under the parser, and the sender side
    read rather than the receiver side: retransmissions are a fact the
    sender knows and the receiver cannot.
    """
    out = ssh(node, "iperf3 -c %s -p %d -t %d -f m %s 2>&1"
                    % (target, port, secs, extra),
              check=False, timeout=secs + 60)
    mbits = None
    retr = None
    for line in out.split("\n"):
        if "sender" not in line:
            continue
        m = MBITS.search(line)
        if m:
            val = float(m.group(1))
            mbits = {"": val / 1e6, "K": val / 1e3, "M": val,
                     "G": val * 1e3}[m.group(2)]
        r = RETR.search(line)
        if r:
            retr = int(r.group(1))
    return {"mbits": mbits, "retr": retr, "raw": out}

def show_iperf(label, got):
    if got["mbits"] is None:
        story("      %-26s no result: %s"
              % (label, (got["raw"] or "").strip().split("\n")[-1][:60]))
        return
    extra = "" if got["retr"] is None else "   %d retransmit(s)" % got["retr"]
    story("      %-26s %8.1f Mbit/s%s" % (label, got["mbits"], extra))

def mtr(node, target):
    out = ssh(node, "mtr -r -c 5 -n %s 2>&1" % target, check=False,
              timeout=90)
    return [ln.rstrip() for ln in out.split("\n") if ln.strip()]

def rtt(node, target, count=5):
    out = ssh(node, "ping -c %d -W 3 %s 2>&1" % (count, target), check=False,
              timeout=60)
    m = re.search(r"=\s*([\d.]+)/([\d.]+)/([\d.]+)", out)
    lost = re.search(r"(\d+)% packet loss", out)
    return {"avg": float(m.group(2)) if m else None,
            "loss": int(lost.group(1)) if lost else None}

def show_rtt(label, got):
    story("      %-26s %s   %s"
          % (label,
             "avg %6.1f ms" % got["avg"] if got["avg"] is not None
             else "no reply    ",
             "%d %% lost" % got["loss"] if got["loss"] is not None else ""))

# the wire

def parse_bytes_line(line):
    f = line.split()
    if len(f) < 5:
        return None
    try:
        return {"nb": int(f[1]), "when": float(f[2]),
                "size": len(f) - 4}
    except ValueError:
        return None

class Tap:
    """One subscription, counting frames and bytes.

    The filter runs in the switch, so a subscription with a narrow filter
    is usable during a throughput test -- which is the property this whole
    demo depends on. An unfiltered one at 900 Mbit/s would be measuring the
    sniffer.
    """

    def __init__(self, node, num, filt=""):
        self.live = False
        self.lock = threading.Lock()
        self.count = 0
        self.bytes = 0
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
            if pkt:
                with self.lock:
                    self.count += 1
                    self.bytes += pkt["size"]
        self.ready_evt.set()

    def ready(self, timeout=15):
        self.ready_evt.wait(timeout)
        return self.live

    def clear(self):
        with self.lock:
            self.count = 0
            self.bytes = 0

    def stop(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()

LAYOUT = {
    SND: (-520, -95),
    P1: (-260, 95),
    LAN_A: (-260, -95),
    CABLE: (0, -95),
    LAN_B: (260, -95),
    P2: (260, 95),
    RCV: (520, -95),
}

ETHS = {
    (SND, "eth0"): 90,
    (P1, "eth0"): 0,
    (P2, "eth0"): 0,
    (RCV, "eth0"): 270,
    (CABLE, "A"): 270,
    (CABLE, "B"): 90,
}

CARD = r"""
  sender --- lan_a --- [a2b] --- lan_b --- receiver
 10.0.1.10                               10.0.2.10
                |                   |
              probe1              probe2
            (mtr, hop by hop)   (a second flow)

  The a2b is the instrument: a cable that can be told to delay, to lose
  and to limit a rate, each direction on its own. Every number is
  measured over it rather than argued about.
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

    step("creating the four containers and the cable")
    in_parallel(lambda n: create(n, rootfs), list(ALL), "add cnt")
    r = run("cloonix cli %s add a2b %s" % (NET, CABLE))
    if r.returncode or not wait_exists(CABLE, 20):
        sys.exit("the server would not add an a2b: %s"
                 % ((r.stdout + r.stderr).strip() or "it said nothing"))
    run("cloonix cli %s plug %s A %s" % (NET, CABLE, LAN_A))
    run("cloonix cli %s plug %s B %s" % (NET, CABLE, LAN_B))
    for node in ALL:
        for num, lan, _ in WIRING[node]:
            run("cloonix cli %s plug %s eth%d %s"
                % (NET, node, num, lan))

    step("waiting for them to answer")
    for name in ALL:
        end = time.time() + 90
        while time.time() < end and not answers(name):
            time.sleep(2)
        if not answers(name):
            raise RuntimeError("%s never answered ssh" % name)
    tools_present(SND, "iperf3", "mtr")
    in_parallel(address, list(ALL), "address")
    cable_clear()
    iperf_server(RCV, IPERF_PORT)
    iperf_server(P2, IPERF_PORT2)

    step("ACT 1: the path with nothing done to it")
    story()
    story("    the cable: %s" % cable_says())
    story()
    base_rtt = rtt(SND, RCV_IP)
    show_rtt("round trip, 5 pings", base_rtt)
    base = iperf_run(SND, RCV_IP)
    show_iperf("iperf3, %d seconds" % SECONDS, base)
    story()
    if base["mbits"] is None:
        sys.exit("the baseline transfer produced no number, so nothing after"
                 " it can be compared to anything")
    story("    what mtr says is between the two ends:")
    for line in mtr(P1, RCV_IP)[:6]:
        story("      %s" % line)
    story()
    story("    Two machines, one emulated cable, and no kernel doing any")
    story("    forwarding: the number above is roughly what a memory copy")
    story("    through openvswitch costs on this machine. It is a ceiling,")
    story("    not a prediction about any real network.")
    story()

    step("ACT 2: 100 ms of delay, and nothing lost at all")
    story()
    cable_set("delay", 100)
    story("    the cable: %s" % cable_says(delay=100))
    time.sleep(2)
    show_rtt("round trip, 5 pings", rtt(SND, RCV_IP))
    slow = iperf_run(SND, RCV_IP)
    show_iperf("iperf3, %d seconds" % SECONDS, slow)
    story()
    if base["mbits"] and slow["mbits"]:
        story("      %.1f Mbit/s became %.1f, a factor of %.0f, with not one"
              % (base["mbits"], slow["mbits"],
                 max(base["mbits"] / max(slow["mbits"], 0.001), 1)))
        story("      packet lost.")
    story()
    story("    Nothing is broken. A sender may have one window of data in")
    story("    flight and no more until an ack comes back, so throughput is")
    story("    the window divided by the round trip -- and the round trip")
    story("    just got two hundred times longer. That is the")
    story("    bandwidth-delay product, and it is why a fast link to")
    story("    somewhere far away feels slow when nothing is wrong with it.")
    story()
    story("      what the sender itself thinks, from the socket:")
    for line in ssh(SND, "ss -tin 2>/dev/null | head -6",
                    check=False).split("\n")[:6]:
        if line.strip():
            story("        %s" % line.strip()[:96])
    story()

    step("ACT 3: loss, alone and then together with delay")
    story()
    cable_clear()
    cable_set("loss", 1)
    story("    the cable: %s" % cable_says(loss=1))
    time.sleep(2)
    show_rtt("round trip, 20 pings", rtt(SND, RCV_IP, count=20))
    lossy = iperf_run(SND, RCV_IP)
    show_iperf("1 %% loss, no delay", lossy)

    cable_set("delay", 100)
    story("    the cable: %s" % cable_says(delay=100, loss=1))
    time.sleep(2)
    both = iperf_run(SND, RCV_IP)
    show_iperf("1 %% loss AND 100 ms", both)
    story()
    if lossy["mbits"] and both["mbits"] and slow["mbits"]:
        story("      alone:  delay %.1f   loss %.1f"
              % (slow["mbits"], lossy["mbits"]))
        story("      together: %.1f -- not the smaller of the two, and not"
              % both["mbits"])
        story("      their average either.")
    story()
    story("    Loss on a fast short link costs little: the retransmit is")
    story("    back in under a millisecond. On a long one, every loss costs")
    story("    a whole round trip AND makes the sender halve its window,")
    story("    which then takes many round trips to grow back. The two")
    story("    impairments multiply, and that combination is what a bad")
    story("    wifi link or a congested transit really is.")
    story()
    story("      retransmissions counted by the sender: %s"
          % (both["retr"] if both["retr"] is not None else "not reported"))
    story()

    step("ACT 4: a rate limit, two flows, and what the wire really carried")
    story()
    cable_clear()
    cable_set("rate", 50)
    story("    the cable: %s" % cable_says(rate=50))
    time.sleep(2)

    tap = Tap(SND, 0, "tcp port %d" % IPERF_PORT)
    if not tap.ready():
        story("    (could not sniff %s; the wire count is skipped)" % SND)
    else:
        time.sleep(1.0)
        tap.clear()
    one = iperf_run(SND, RCV_IP)
    time.sleep(1.0)
    counted = None
    if tap.live:
        with tap.lock:
            counted = (tap.count, tap.bytes)
        tap.stop()
    show_iperf("one flow, capped at 50", one)
    if counted:
        story("      the wire carried            %8d frames, %.1f MB"
              % (counted[0], counted[1] / 1e6))
        if one["mbits"]:
            claimed = one["mbits"] * SECONDS / 8.0
            story("      iperf3 says it sent         %8.1f MB" % claimed)
            story("      (the wire count is one direction of one filtered")
            story("       subscription, so it is the lower of the two by")
            story("       construction -- what matters is that it moved)")
    story()

    story("    now two flows at once over the same capped cable:")
    results = {}

    def flow(which):
        if which == "a":
            results["a"] = iperf_run(SND, RCV_IP, port=IPERF_PORT)
        else:
            results["b"] = iperf_run(P1, P2_IP, port=IPERF_PORT2)
    threads = [threading.Thread(target=flow, args=(w,)) for w in ("a", "b")]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    show_iperf("sender -> receiver", results.get("a", {"mbits": None,
                                                      "retr": None,
                                                      "raw": ""}))
    show_iperf("probe1 -> probe2", results.get("b", {"mbits": None,
                                                     "retr": None,
                                                     "raw": ""}))
    got_a = results.get("a", {}).get("mbits")
    got_b = results.get("b", {}).get("mbits")
    if got_a and got_b:
        story()
        story("      together %.1f, against %.1f for one flow alone."
              % (got_a + got_b, one["mbits"] or 0))
        share = min(got_a, got_b) / max(got_a, got_b)
        story("      the smaller flow got %.0f %% of the larger one."
              % (share * 100))
        story()
        story("    Two flows through one bottleneck end up sharing it, and")
        story("    nothing in the network arranged that. It falls out of")
        story("    each sender backing off when it loses a packet, which")
        story("    both of them do at about the same rate. It is fair only")
        story("    because the two are similar: one flow with a much shorter")
        story("    round trip would take most of the cable and be behaving")
        story("    perfectly correctly while it did.")
    story()

    cable_clear()
    story("    the cable is set back to %s" % cable_says())
    story()

    bad = []
    if base["mbits"] and slow["mbits"] and slow["mbits"] >= base["mbits"]:
        bad.append("adding 100 ms of delay did not slow the transfer down,"
                   " so the a2b is not in the path")
    if lossy["mbits"] is None or both["mbits"] is None:
        bad.append("one of the impaired transfers produced no number")
    if bad:
        sys.exit("\n" + "\n".join("  " + b for b in bad))

    story("""
the network is left up, with the cable clean. Things worth trying:

  set one direction only, and watch an asymmetric path behave:
    cloonix cli %s cnf cable A2B delay=200

  a subscription running THROUGH a throughput test, which is what the
  server-side filter is for -- this stays readable at full rate:
    cloonix cli %s sniff sender eth0 --dump icmp

  the same test with a different congestion control on the sender:
    cloonix cli %s ssh sender "sysctl -w net.ipv4.tcp_congestion_control=reno"

  and udp instead of tcp, which does not back off at all and shows what
  the loss really is:
    cloonix cli %s ssh sender "iperf3 -c %s -u -b 100M -t 5"
""" % (NET, NET, NET, NET, RCV_IP))

if __name__ == "__main__":
    try:
        main()
    except SystemExit as stop:
        if stop.code:
            sys.stderr.write("%s\n" % stop.code)
            up = [n for n in list(ALL) + [CABLE] if in_lst(n)]
            if up:
                sys.stderr.write("Still running: %s\n" % ", ".join(up))
            raise SystemExit(1)
        raise
    except RuntimeError as err:
        sys.stderr.write("%s\n" % err)
        raise SystemExit(1)
