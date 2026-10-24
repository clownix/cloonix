#!/usr/bin/env python3
r"""vrrp - two routers, one address, and a ping that survives losing one.

    ./vrrp.py [net]                the net defaults to nemo

Four machines and two lans::

  client ---- lanA ---- rt1 ---- lanB ---- server
 10.1.0.10          10.1.0.1  10.2.0.1   10.2.0.10
                  \ rt2 /
                   10.1.0.2  10.2.0.2

  lanA virtual address 10.1.0.254   <- the client's default gateway
  lanB virtual address 10.2.0.254   <- the server's default gateway

Neither virtual address belongs to a machine: the master router holds
both, and the client and the server route through them whichever router
that is. rt1 has the higher priority, so it is master while it is alive.
The protocol is one packet repeated every second, a router announcing its
priority for a virtual router id, the highest being in charge.

  ACT 1  keepalived starts on both: the advertisements appear on the wire,
         the virtual addresses on rt1 alone.
  ACT 2  a ping from client to server, and rt1 killed under it: its last
         advertisement, priority 0, then rt2 taking over, then the
         gratuitous arp telling the lan the address moved to another mac;
         the ping counts the outage.
  ACT 3  rt1 comes back and takes the address again, its priority being
         higher: the same three things in reverse.

Image: vrrp_trixie (keepalived)
Size:  4 containers, 2 lans

THE PACKET TO CATCH
-------------------
  VRRP is IP protocol 112 to a multicast group, reached by exclusion:

    cloonix cli nemo cnf client eth0 sniff=on
    cloonix cli nemo sniff client eth0 --dump "ip and not udp and not tcp and not icmp"

  One advertisement a second from the master, silence from the backup. The
  packet to catch is the last one before the master is killed, then the
  first from the other router: the gap is the failover time. The frame
  that moves the traffic is a gratuitous arp for 10.1.0.254 with a new mac,
  nothing on the client reconfigured:

    cloonix cli nemo sniff client eth0 --dump arp

  With the ping under a third subscription, what went missing during the
  switch is the result:

    cloonix cli nemo sniff client eth0 --dump icmp
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
    trace_ssh, wait_ready, unavailable)

ROOTFS_CHOICES = ("vrrp_trixie",)

HISTORY = """\
Cisco patented the idea of two routers sharing one address as HSRP in
1994, published as RFC 2281; the IETF wrote the open equivalent, VRRP,
as RFC 2338 in 1998, RFC 3768 in 2004 and version 3 for IPv6 in RFC
5798 in 2010. The master sends an advertisement every second to
224.0.0.18, protocol 112; when the backups stop hearing it, the highest
priority takes the address and announces the move with a gratuitous
ARP. Keepalived was written by Alexandre Cassen in 2000, first as a
health checker for the Linux Virtual Server, and carries the VRRP
implementation most linux routers use.

Today: the default gateway of most data-centre racks and many office lans is a
VRRP address; keepalived is also the usual front of linux load balancers,
holding the service address of an haproxy or nginx pair.
"""

CLI, RT1, RT2, SRV = "client", "rt1", "rt2", "server"
ROUTERS = (RT1, RT2)
ALL = (CLI, RT1, RT2, SRV)
LAN_A, LAN_B = "lanA", "lanB"

WIRING = {
    CLI: [(0, LAN_A, "10.1.0.10/24")],
    RT1: [(0, LAN_A, "10.1.0.1/24"), (1, LAN_B, "10.2.0.1/24")],
    RT2: [(0, LAN_A, "10.1.0.2/24"), (1, LAN_B, "10.2.0.2/24")],
    SRV: [(0, LAN_B, "10.2.0.10/24")],
}

VIP_A, VIP_B = "10.1.0.254", "10.2.0.254"
VRID_A, VRID_B = 10, 20
GATEWAY = {CLI: VIP_A, SRV: VIP_B}
PRIORITY = {RT1: 200, RT2: 100}

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

    printf reads its first argument as a FORMAT: three things in it are
    not literal:

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
                 "  falls back to nemo:   ./vrrp.py <net>"
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
    unavailable("the server would not start a container on %s.\n"
             "  it said: %s\n"
             "  fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_demo_bulk/"
             "vrrp_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_vrrp_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said or "nothing"))

def tools_present(node, *tools):
    for t in tools:
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                            "command -v %s >/dev/null 2>&1" % t],
                           capture_output=True, stdin=subprocess.DEVNULL)
        if r.returncode:
            unavailable("%s has no %s: this is not the vrrp image.\n"
                     "  fetch it:\n"
                     "    cd /var/lib/cloonix/bulk\n"
                     "    wget https://clownix.net/downloads/cnt_demo_bulk/"
                     "vrrp_trixie.tar.gz\n"
                     "    (cloonix extracts it at the first use)\n"
                     "  or build it:\n"
                     "    cd vm_create/cnt_kvm_demo && ./cnt_vrrp_trixie.sh"
                     % (node, t))

def address(node):
    for num, lan, addr in WIRING[node]:
        ssh(node, "ip addr add %s dev eth%d" % (addr, num))
        ssh(node, "ip link set dev eth%d up" % num)
    if node in GATEWAY:
        # The default route points at an address no machine owns.
        ssh(node, "ip route add default via %s" % GATEWAY[node])
        trace("%s addressed, default via %s (a virtual address)",
              node, GATEWAY[node])
    else:
        trace("%s addressed: %s", node,
              " ".join(a for _, _, a in WIRING[node]))

def enable_forwarding(node):
    """ospf is not here to fill a table, but a router still has to be willing
    to forward: net.ipv4.ip_forward is 0 by default and the routers would
    otherwise hold the virtual address and drop everything through it."""
    ssh(node, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    got = ssh(node, "sysctl -n net.ipv4.ip_forward", check=False).strip()
    if got != "1":
        raise RuntimeError("%s would not enable ip_forward, it reads %r"
                           % (node, got))
    trace("%s forwards ipv4", node)

# keepalived

def keepalived_conf(node):
    """Two instances in one sync group, one per lan.

    A sync group matters here. Without it the two instances fail over
    independently, and a moment where rt2 holds the lanA address while rt1
    still holds the lanB one is a black hole: the client's packets go to rt2,
    which forwards them out of lanB, and the replies come back to rt1, which
    has no idea what they are. Grouped, the two move together or not at all.

    The priorities are the whole election. rt1 says 200 and rt2 says 100, so
    rt1 is master for as long as it keeps saying so.
    """
    prio = PRIORITY[node]
    out = ["global_defs {",
           "    router_id " + node,
           "    enable_script_security",
           "    script_user root",
           "}",
           "vrrp_sync_group VG {",
           "    group {",
           "        VI_A",
           "        VI_B",
           "    }",
           "}"]
    for tag, iface, vrid, vip in (("VI_A", "eth0", VRID_A, VIP_A),
                                  ("VI_B", "eth1", VRID_B, VIP_B)):
        out += [
            "vrrp_instance " + tag + " {",
            # MASTER only decides who speaks first at start up.
            "    state " + ("MASTER" if node == RT1 else "BACKUP"),
            "    interface " + iface,
            "    virtual_router_id " + str(vrid),
            "    priority " + str(prio),
            "    advert_int 1",
            "    authentication {",
            "        auth_type PASS",
            "        auth_pass cloonix",
            "    }",
            "    virtual_ipaddress {",
            "        " + vip + "/24 dev " + iface,
            "    }",
            "}"]
    return "\n".join(out) + "\n"

def keepalived_start(node):
    ssh(node, "mkdir -p /etc/keepalived /run/keepalived /var/log/keepalived")
    put(node, "/etc/keepalived/keepalived.conf", keepalived_conf(node))
    ssh(node, "keepalived -f /etc/keepalived/keepalived.conf -D "
              "</dev/null >/var/log/keepalived/out 2>&1")
    for _ in range(20):
        if ssh(node, "pgrep -x keepalived >/dev/null && echo up",
               check=False).strip():
            trace("%s: keepalived running, priority %d", node, PRIORITY[node])
            return
        time.sleep(0.5)
    raise RuntimeError("keepalived did not start on %s:\n%s"
                       % (node, ssh(node, "cat /var/log/keepalived/out",
                                    check=False)))

def who_holds(vip):
    """Which router carries the virtual address right now, by asking both."""
    holders = []
    for node in ROUTERS:
        out = ssh(node, "ip -4 addr show | grep -c %s || true" % vip,
                  check=False).strip()
        if out and out != "0":
            holders.append(node)
    return holders

def wait_for_holder(vip, want, timeout=25):
    """Wait until exactly one router holds the address, and say which."""
    end = time.time() + timeout
    while time.time() < end:
        h = who_holds(vip)
        if h == [want]:
            trace("%s holds %s", want, vip)
            return True
        time.sleep(0.5)
    story("  FAIL %s is held by %s, expected %s"
          % (vip, who_holds(vip) or "nobody", want))
    return False

# The wire: vrrp is ip protocol 112, 8 bytes (version<<4|type, vrid, prio, count, auth, adv) + addresses.

def ipv4_payload(pkt, proto):
    """The payload of an IPv4 packet of this protocol, with src and dst."""
    d = pkt["data"]
    if len(d) < 34 or d[12:14] != b"\x08\x00" or d[23] != proto:
        return None
    ihl = (d[14] & 0x0f) * 4
    src = ".".join(str(b) for b in d[26:30])
    dst = ".".join(str(b) for b in d[30:34])
    return src, dst, d[14 + ihl:]

def decode_vrrp(pkt):
    got = ipv4_payload(pkt, 112)
    if not got:
        return None
    src, dst, p = got
    if len(p) < 8:
        return None
    ver, typ = p[0] >> 4, p[0] & 0x0f
    if typ != 1:
        return None
    n = p[3]
    addrs = []
    for i in range(n):
        off = 8 + 4 * i
        if off + 4 <= len(p):
            addrs.append(".".join(str(b) for b in p[off:off + 4]))
    return {"kind": "VRRP", "src": src, "dst": dst, "ver": ver,
            "vrid": p[1], "prio": p[2], "interval": p[5], "addrs": addrs,
            "mac": ":".join("%02x" % b for b in pkt["data"][6:12])}

def decode_arp(pkt):
    d = pkt["data"]
    if len(d) < 42 or d[12:14] != b"\x08\x06":
        return None
    op = int.from_bytes(d[20:22], "big")
    sha = ":".join("%02x" % b for b in d[22:28])
    spa = ".".join(str(b) for b in d[28:32])
    tpa = ".".join(str(b) for b in d[38:42])
    return {"kind": "ARP", "op": op, "sha": sha, "spa": spa, "tpa": tpa,
            "mac": ":".join("%02x" % b for b in d[6:12])}

def decode_icmp(pkt):
    got = ipv4_payload(pkt, 1)
    if not got:
        return None
    src, dst, p = got
    if len(p) < 8:
        return None
    kind = {8: "echo request", 0: "echo reply"}.get(p[0])
    if not kind:
        return None
    return {"kind": "ICMP", "src": src, "dst": dst, "what": kind,
            "seq": int.from_bytes(p[6:8], "big")}

def describe(rec):
    m = rec["msg"]
    if m["kind"] == "VRRP":
        if m["prio"] == 0:
            note = "  <- RESIGNING, giving the address up"
        elif m["prio"] == 255:
            note = "  <- owner"
        else:
            note = ""
        return ("VRRPv%d  vrid=%-3d priority=%-4d every %ds   claims %s%s"
                % (m["ver"], m["vrid"], m["prio"], m["interval"],
                   ", ".join(m["addrs"]) or "nothing", note))
    if m["kind"] == "ARP":
        what = "request" if m["op"] == 1 else "reply"
        if m["spa"] == m["tpa"]:
            return ("ARP %-7s GRATUITOUS: %s is at %s   <- the address has "
                    "moved" % (what, m["spa"], m["sha"]))
        return "ARP %-7s who has %s, tell %s" % (what, m["tpa"], m["spa"])
    return "ICMP    %s seq=%d" % (m["what"], m["seq"])

class Tap:
    """One sniffed interface, draining into a list shared by all the taps."""

    def __init__(self, node, num, label, sink, lock):
        self.node, self.num, self.label = node, num, label
        self.sink, self.lock = sink, lock
        self.live = False
        self.seen = 0
        capturable(node, num)
        self.proc = subprocess.Popen(
            CLOONIX + ["cli", NET, "sniff", node, "eth%d" % num, "--bytes"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1)
        self.ready_evt = threading.Event()
        self.thread = threading.Thread(target=self._drain, daemon=True)
        self.thread.start()

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
            msg = decode_vrrp(pkt) or decode_arp(pkt) or decode_icmp(pkt)
            if not msg:
                continue
            with self.lock:
                self.seen += 1
                self.sink.append({"when": pkt["when"], "where": self.label,
                                  "msg": msg})
        self.ready_evt.set()

    def ready(self, timeout=15):
        self.ready_evt.wait(timeout)
        if not self.live:
            err = self.proc.stderr.read() if self.proc.stderr else ""
            story("  could not sniff %s eth%d: %s"
                  % (self.node, self.num, err.strip() or "no answer"))
        return self.live

    def stop(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()

# Both lans, from the point of view of the machines that are not routers.
PATH_TAPS = [
    (CLI, 0, "client eth0  lanA"),
    (SRV, 0, "server eth0  lanB"),
]

def open_taps():
    sink, lock = [], threading.Lock()
    taps = [Tap(n, e, lab, sink, lock) for n, e, lab in PATH_TAPS]
    for t in taps:
        if not t.ready():
            for x in taps:
                x.stop()
            return None, None, None
    # Wait until every tap has really delivered something before measuring.
    end = time.time() + 10
    while time.time() < end and not all(t.seen for t in taps):
        time.sleep(0.2)
    cold = [t.label for t in taps if not t.seen]
    if cold:
        trace("these taps never warmed: %s", ", ".join(cold))
    else:
        trace("both taps are delivering")
    return taps, sink, lock

def show_packets(sink, lock, title, kinds=None, limit=60):
    """Print what the taps caught, in the order it happened.

    kinds selects which protocols to show, and it is not decoration. A ping
    at five packets a second on two lans is six hundred rows a minute, and
    with everything shown the three packets that ARE the failover -- the
    resignation, the takeover, the gratuitous arp -- fell off the end of the
    table under "and 244 more". The ping is measured separately instead.

    Identical consecutive rows are collapsed. keepalived sends its gratuitous
    arp five times in case one is lost: printed once, not
    five times.
    """
    with lock:
        rows = sorted(sink, key=lambda r: r["when"])
    if kinds:
        rows = [r for r in rows if r["msg"]["kind"] in kinds]
    story("    %s" % title)
    if not rows:
        story("      (nothing of that kind crossed the sniffed interfaces)")
        return 0
    t0 = rows[0]["when"]
    story("      %-7s  %-18s  %s" % ("t+ms", "seen on", "what went past"))
    story("      " + "-" * 96)
    # Repeats are gathered within a second, not just when they are adjacent.
    texts = [describe(r) for r in rows]
    used = [False] * len(rows)
    shown = 0
    left = 0
    for i, r in enumerate(rows):
        if used[i]:
            continue
        if shown >= limit:
            left = sum(1 for u in used if not u)
            break
        n = 1
        used[i] = True
        for j in range(i + 1, len(rows)):
            if rows[j]["when"] - r["when"] > 1.0:
                break
            if (not used[j] and rows[j]["where"] == r["where"]
                    and texts[j] == texts[i]):
                used[j] = True
                n += 1
        story("      %-7.0f  %-18s  %s%s"
              % ((r["when"] - t0) * 1000.0, r["where"], texts[i],
                 ("   x%d" % n) if n > 1 else ""))
        shown += 1
    if left:
        story("      ... and %d more" % left)
    story("      (times are when each sniffer read the packet, not when it")
    story("       crossed the wire: rows from different interfaces can be a")
    story("       few ms out of order when the machine is busy)")
    story()
    return len(rows)

def ping_start(node, target, count, interval=0.2):
    """A ping running in the background, its output kept for counting."""
    ssh(node, "rm -f /tmp/ping.log; ping -i %s -c %d %s "
              "</dev/null >/tmp/ping.log 2>&1 &" % (interval, count, target))

def ping_gap(sink, lock, label):
    """The longest silence between consecutive echo requests on one lan.

    This is the outage, measured on the wire rather than inferred from a
    count. Losing nothing at 200 ms intervals only says the failover was
    quicker than 200 ms; the gap says how much quicker.
    """
    with lock:
        seqs = sorted((r["when"] for r in sink
                       if r["msg"]["kind"] == "ICMP"
                       and r["msg"]["what"] == "echo request"
                       and r["where"] == label))
    if len(seqs) < 2:
        return None, 0
    gaps = [(seqs[i + 1] - seqs[i]) for i in range(len(seqs) - 1)]
    return max(gaps), len(seqs)

def ping_result(node):
    out = ssh(node, "cat /tmp/ping.log", check=False)
    sent = recv = 0
    for line in out.split("\n"):
        if "packets transmitted" in line:
            f = line.replace(",", " ").split()
            try:
                sent, recv = int(f[0]), int(f[3])
            except (ValueError, IndexError):
                pass
    return sent, recv, out

LAYOUT = {
    CLI: (-380, 0),
    LAN_A: (-190, 0),
    RT1: (0, -75),
    RT2: (0, 75),
    LAN_B: (190, 0),
    SRV: (380, 0),
}

ETHS = {
    (CLI, "eth0"): 90,
    (RT1, "eth0"): 248,
    (RT1, "eth1"): 112,
    (RT2, "eth0"): 292,
    (RT2, "eth1"): 68,
    (SRV, "eth0"): 270,
}

def place():
    for name, (x, y) in LAYOUT.items():
        cli("cnf", name, "x=%s" % x, "y=%s" % y, ack=ACK_FAST)
    for (name, num), deg in ETHS.items():
        cli("cnf", name, num, "angle=%s" % deg, ack=ACK_FAST)

def main():
    story(HISTORY)
    step("looking for the image")
    rootfs = choose_rootfs()
    story("rootfs %s: two routers sharing two virtual addresses" % rootfs)
    story("""
    client ---- lanA ---- rt1 ---- lanB ---- server
   10.1.0.10          10.1.0.1  10.2.0.1   10.2.0.10
                    \\  rt2  /
                      10.1.0.2  10.2.0.2

    lanA virtual address %s   the client's default gateway
    lanB virtual address %s   the server's default gateway

    rt1 priority %d, rt2 priority %d, so rt1 is master while it lives.
    Neither virtual address belongs to a machine, and neither the client
    nor the server is ever told which router is carrying it.
""" % (VIP_A, VIP_B, PRIORITY[RT1], PRIORITY[RT2]))

    step("erasing the net, then placing the items")
    erase_net()
    place()

    step("creating the four containers")
    in_parallel(lambda n: create_cnt(n, rootfs), list(ALL), "add cnt")

    step("waiting for them to answer")
    in_parallel(wait_ready, list(ALL), "wait_ready")
    tools_present(RT1, "keepalived")

    step("cabling them")
    for node in ALL:
        for num, lan, _ in WIRING[node]:
            cli("plug", node, "eth%d" % num, lan)

    step("addressing them")
    in_parallel(address, list(ALL), "address")
    in_parallel(enable_forwarding, list(ROUTERS), "enable_forwarding")

    step("starting keepalived on both routers")
    in_parallel(keepalived_start, list(ROUTERS), "keepalived_start")

    step("act 1: who holds the addresses, and what the lans hear")
    story()
    if not (wait_for_holder(VIP_A, RT1) and wait_for_holder(VIP_B, RT1)):
        sys.exit("rt1 never became master, see /var/log/keepalived on it")
    story("  the addresses, as the routers themselves see them:")
    for n in ROUTERS:
        for line in ssh(n, "ip -4 -brief addr show",
                        check=False).rstrip().split("\n"):
            story("      %-6s %s" % (n, line))
    story()
    taps, sink, lock = open_taps()
    if taps is None:
        sys.exit("could not sniff the lans")
    try:
        time.sleep(3.5)
        show_packets(sink, lock, "three seconds of a quiet, working network:",
             kinds=("VRRP",))
        story("    One advertisement per lan per second, all from rt1, all")
        story("    saying priority %d. That is the entire protocol while"
              % PRIORITY[RT1])
        story("    nothing is wrong: rt2 hears them, sees a bigger number")
        story("    than its own, and stays quiet.")
        story()

        step("act 2: kill rt1 while a ping is in flight")
        story()
        with lock:
            sink.clear()
        ping_start(CLI, "10.2.0.10", 60)     # 12 seconds at 0.2s intervals
        time.sleep(3)
        trace("killing keepalived on rt1 now")
        ssh(RT1, "pkill -x keepalived", check=False)
        time.sleep(6)
        show_packets(sink, lock,
             "what the two lans heard around the failover "
             "(vrrp and arp only, the ping is counted below):",
             kinds=("VRRP", "ARP"))
        story("    The order is the lesson:")
        story("      * rt1 sends one last advertisement with priority 0.")
        story("        That is not a timeout, it is a resignation: keepalived")
        story("        says so on the way out, and the backup does not have")
        story("        to wait three seconds to work it out.")
        story("      * rt2 starts advertising, priority %d, same vrid."
              % PRIORITY[RT2])
        story("      * rt2 sends a GRATUITOUS arp for each virtual address.")
        story("        This is the half that moves the traffic: the")
        story("        client had %s cached against rt1's mac, and this is"
              % VIP_A)
        story("        what corrects it. Without it the ping would keep going")
        story("        to a machine that is no longer listening.")
        story()
        if not (wait_for_holder(VIP_A, RT2) and wait_for_holder(VIP_B, RT2)):
            sys.exit("rt2 never took over")

        time.sleep(7)
        sent, recv, _ = ping_result(CLI)
        lost = sent - recv
        story("  the ping across the failover: %d sent, %d received, %d lost"
              % (sent, recv, lost))
        gap, n = ping_gap(sink, lock, "client eth0  lanA")
        if gap is not None:
            story("  on the wire: %d echo requests on lanA, and the longest"
                  % n)
            story("  silence between two of them was %.0f ms." % (gap * 1000))
        story()
        if lost == 0 and gap is not None and gap < 0.5:
            story("    The ping did not notice, and the reason matters more")
            story("    than the number does.")
            story()
            story("    Killing keepalived took the virtual address off rt1.")
            story("    It did NOT stop rt1: the machine is still up, its")
            story("    interfaces are still up, and it still forwards. The")
            story("    client went on sending to the mac it had cached, that")
            story("    mac still belonged to a running router, and the")
            story("    packets went through as before.")
            story()
            story("    So this act shows the protocol changing hands cleanly")
            story("    and proves nothing about outage. To measure a real")
            story("    one the router has to go -- take the link")
            story("    down instead, which is the first suggestion at the")
            story("    end, and the backup then waits for three missed")
            story("    advertisements with nothing forwarding in between.")
        elif lost:
            story("    %d packets went missing, about %.1f s of outage."
                  % (lost, lost * 0.2))
        story()

        step("act 3: rt1 comes back and takes the address again")
        story()
        with lock:
            sink.clear()
        keepalived_start(RT1)
        time.sleep(5)
        show_packets(sink, lock, "what the lans heard when the master returned:",
             kinds=("VRRP", "ARP"))
        story("    rt1 advertises priority %d, rt2 hears a number bigger than"
              % PRIORITY[RT1])
        story("    its own and stands down without being asked, and another")
        story("    gratuitous arp moves the address back. That is preemption,")
        story("    and it is a choice: nopreempt in the config would leave")
        story("    rt2 in charge until it failed in its turn.")
        story()
        ok_back = wait_for_holder(VIP_A, RT1) and wait_for_holder(VIP_B, RT1)
    finally:
        for t in taps:
            t.stop()

    ok_ping = recv > 0 and lost < sent
    if not (ok_back and ok_ping):
        sys.exit("\nsomething did not work,"
                 " see the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")

    story("""
the network is left up. Things to try:

  watch the advertisements go past, live and decoded:
    cloonix cli %s sniff client eth0 --dump

  see which router holds the address at any moment:
    cloonix cli %s ssh rt1 "ip -4 -brief addr show"
    cloonix cli %s ssh rt2 "ip -4 -brief addr show"

  cause a REAL outage, which killing the daemon does not: take rt1 off the
  lan entirely. There is no resignation then, so the backup waits for three
  missed advertisements, and nothing forwards in the meantime. Run a ping
  from the client first and watch it stop:
    cloonix cli %s ssh client "ping -i 0.2 10.2.0.10"
    cloonix cli %s ssh rt1 "ip link set eth0 down; ip link set eth1 down"

  and watch what a split brain looks like: stop rt2 hearing rt1 while both
  are alive, and both will hold the same address at once
    cloonix cli %s ssh rt2 "ip link set eth0 down"
""" % (NET, NET, NET, NET, NET, NET))

main()
