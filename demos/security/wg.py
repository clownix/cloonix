#!/usr/bin/env python3
r"""wg - the same taps as every other demo, and a table that goes blank.

    ./wg.py [net]                  the net defaults to nemo

Five machines and three lans::

  hostA -- lanA -- gwA -- lanC -- gwB -- lanB -- hostB
 10.1.0.10     10.1.0.1      10.0.0.1  10.0.0.2   10.2.0.10
                                    10.2.0.1

  lanC is "the internet": the untrusted middle that both sites cross and
  neither owns. It is the only wire an eavesdropper is assumed to have.

The two gateways build a wireguard tunnel across lanC and route 10.1.0.0/24
and 10.2.0.0/24 through it. hostA then talks to hostB exactly as before and
knows nothing about any of it.

  ACT 1  the tunnel is NOT up yet. hostA fetches a page from hostB and the
         whole request is readable on all three wires, lanC included.
         This is what every demo before this one has looked like.
  ACT 2  the tunnel comes up. The handshake is caught: two messages, and
         the second one is the last thing about this connection anybody
         will ever read.
  ACT 3  the identical fetch, repeated. lanA and lanB look exactly as they
         did. lanC now carries nothing but udp 51820 with an opaque
         payload -- same size, same timing, no content.
  ACT 4  what the two ends know that the middle does not, from wg show.

This is the demo that only makes sense next to the others. A packet decoder
is worth having because most protocols are legible; the value of a tunnel is
that this one is not.

Needs cloonix on the PATH and nothing else. Standard library only, like
cloonix itself, so it runs wherever the client does.

Image: wg_trixie (wireguard-tools)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
  The same filter on the same wire, before and after, is the demo:

    cloonix cli nemo cnf gwA eth1 sniff=on
    cloonix cli nemo sniff gwA eth1 --dump tcp port 80

  ACT 1, with no tunnel, prints the request and the page. ACT 3 repeats
  the identical fetch and this subscription stays empty for ever --
  there is no tcp 80 on that wire any more.

  What replaced it:

    cloonix cli nemo sniff gwA eth1 --dump udp port 51820

  The packet to catch is the second handshake message. After it, nothing
  about this connection is ever readable again, and the dump shows why:
  a four-byte type, a receiver index, and then bytes.

  The control, on the site lan, where nothing changed:

    cloonix cli nemo cnf gwA eth0 sniff=on
    cloonix cli nemo sniff gwA eth0 --dump tcp port 80

  Same request, same page, in the clear, in both acts. The tunnel
  protects the middle and nothing else, which is a thing worth showing
  rather than saying.

Important
  No configuration is written into the image. Keys, peers and addresses are
  all done at run time -- keys especially have no business being baked in.
  strongswan, libreswan and openvpn, whose images vm_create/cnt_kvm_demo
  builds, solve the same problem other ways.
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

ROOTFS_CHOICES = ("wg_trixie",)

HISTORY = """\
WireGuard was written by Jason A. Donenfeld and published in 2016, and
merged into linux 5.6 in March 2020 after Linus Torvalds called its
few thousand lines a work of art next to the ipsec code. It negotiates
nothing: one key exchange from the Noise framework of Trevor Perrin,
Curve25519 by Daniel J. Bernstein, ChaCha20-Poly1305 for the data, and
a peer is a public key with a list of allowed addresses. The 1-RTT
handshake caught below is the whole protocol.

Today: WireGuard is in the linux, windows, macos, ios and android of
commercial VPN providers, in Tailscale and Netbird meshes and in kubernetes
network plugins; its four thousand lines are auditable where IPsec stacks
count hundreds of thousands.
"""

HA, HB, GA, GB = "hostA", "hostB", "gwA", "gwB"
GATES = (GA, GB)
ALL = (HA, GA, GB, HB)
LAN_A, LAN_B, LAN_C = "lanA", "lanB", "lanC"

WIRING = {
    HA: [(0, LAN_A, "10.1.0.10/24")],
    GA: [(0, LAN_A, "10.1.0.1/24"), (1, LAN_C, "10.0.0.1/24")],
    GB: [(0, LAN_B, "10.2.0.1/24"), (1, LAN_C, "10.0.0.2/24")],
    HB: [(0, LAN_B, "10.2.0.10/24")],
}

GATEWAY = {HA: "10.1.0.1", HB: "10.2.0.1"}

WG_PORT = 51820
WG = {
    GA: {"addr": "10.9.0.1/24", "peer_ip": "10.0.0.2", "allowed": "10.2.0.0/24",
         "peer": GB},
    GB: {"addr": "10.9.0.2/24", "peer_ip": "10.0.0.1", "allowed": "10.1.0.0/24",
         "peer": GA},
}

PAGE_PORT = 8080

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
                 "  falls back to nemo:   ./wg.py <net>"
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
             "  fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_demo_bulk/"
             "wg_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_wg_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said or "nothing"))

def tools_present(node, *tools):
    for t in tools:
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                            "command -v %s >/dev/null 2>&1" % t],
                           capture_output=True, stdin=subprocess.DEVNULL)
        if r.returncode:
            sys.exit("%s has no %s: this is not the wireguard image.\n"
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

def enable_forwarding(node):
    ssh(node, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    got = ssh(node, "sysctl -n net.ipv4.ip_forward", check=False).strip()
    if got != "1":
        raise RuntimeError("%s would not enable ip_forward, it reads %r"
                           % (node, got))
    trace("%s forwards ipv4", node)

def plain_routes():
    """Route between the sites the ordinary way, with no tunnel.

    Act 1 needs the traffic to cross lanC in the clear, which means it has
    to cross lanC at all. These routes are replaced in act 2 by ones
    pointing into the tunnel, and that swap is the only thing that changes
    on either host.
    """
    ssh(GA, "ip route replace 10.2.0.0/24 via 10.0.0.2")
    ssh(GB, "ip route replace 10.1.0.0/24 via 10.0.0.1")
    trace("the two sites can reach each other across lanC, in the clear")

# something to look for

PAGE_SERVER = '''import http.server, socketserver
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b"SECRET-STRING-THAT-SHOULD-NOT-BE-READABLE\\n"
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def log_message(self, *a):
        pass
socketserver.TCPServer.allow_reuse_address = True
socketserver.TCPServer(("", PORT), H).serve_forever()
'''

def page_start():
    """A one-file web server on hostB, serving a string worth looking for.

    The body is deliberately a distinctive run of characters. Act 1 finds it
    on lanC and act 3 does not, and "the string is there / the string is
    gone" is a stronger demonstration than any amount of describing.
    """
    put(HB, "/tmp/page.py", PAGE_SERVER.replace("PORT", str(PAGE_PORT)))
    out = ssh(HB, "python3 -m py_compile /tmp/page.py 2>&1", check=False)
    if out.strip():
        raise RuntimeError("the page server did not survive being written:\n%s"
                           % out)
    ssh(HB, "pkill -f /tmp/page.py 2>/dev/null; true", check=False)
    ssh(HB, "python3 /tmp/page.py </dev/null >/tmp/page.log 2>&1 &")
    for _ in range(20):
        if ssh(HB, "pgrep -f /tmp/page.py >/dev/null && echo up",
               check=False).strip():
            trace("%s is serving a page on port %d", HB, PAGE_PORT)
            return
        time.sleep(0.5)
    raise RuntimeError("the page server did not start:\n%s"
                       % ssh(HB, "cat /tmp/page.log", check=False))

def fetch():
    return ssh(HA, "curl -sS -m 5 http://10.2.0.10:%d/" % PAGE_PORT,
               check=False)

# the tunnel

def wg_keys():
    """A key pair per gateway, made on the gateway that will use it.

    The private key never leaves the machine that generated it, which is
    not a detail: it is the whole security model, and a demo that generated
    both pairs in one place and copied them around would be teaching the
    opposite of what wireguard is for.
    """
    pub = {}
    for node in GATES:
        ssh(node, "mkdir -p /etc/wireguard; umask 077; "
                  "wg genkey > /etc/wireguard/private")
        pub[node] = ssh(node, "wg pubkey < /etc/wireguard/private",
                        check=False).strip()
        if len(pub[node]) < 40:
            raise RuntimeError("%s produced no usable public key: %r"
                               % (node, pub[node]))
        trace("%s public key %s...", node, pub[node][:16])
    return pub

def wg_up(node, pub):
    """Bring the interface up with ip and wg, not wg-quick.

    wg-quick is a shell script that also writes routes, sets up a firewall
    mark and expects a config file; doing it by hand is four commands and
    every one of them is legible.
    """
    cfg = WG[node]
    peer = cfg["peer"]
    ssh(node, "ip link add wg0 type wireguard")
    ssh(node, "ip addr add %s dev wg0" % cfg["addr"])
    ssh(node, "wg set wg0 listen-port %d private-key /etc/wireguard/private "
              "peer %s allowed-ips %s endpoint %s:%d persistent-keepalive 25"
              % (WG_PORT, pub[peer], cfg["allowed"], cfg["peer_ip"], WG_PORT))
    ssh(node, "ip link set wg0 up")
    trace("%s: wg0 up, %s, peer at %s:%d",
          node, cfg["addr"], cfg["peer_ip"], WG_PORT)

def tunnel_routes():
    """Point the far site down the tunnel instead of across lanC.

    This is the only change either gateway makes. Nothing on hostA or hostB
    is touched, and neither of them ever learns that anything happened.
    """
    ssh(GA, "ip route replace 10.2.0.0/24 dev wg0")
    ssh(GB, "ip route replace 10.1.0.0/24 dev wg0")
    trace("both sites now route the other through wg0")

def wg_show(node):
    return ssh(node, "wg show", check=False)

# The wire A wireguard packet is udp, and its first byte says which of four
# things it is.

WG_TYPES = {1: "handshake initiation", 2: "handshake response",
            3: "cookie reply", 4: "transport data"}

SECRET = "SECRET-STRING-THAT-SHOULD-NOT-BE-READABLE"

def ipv4_parts(pkt):
    d = pkt["data"]
    if len(d) < 34 or d[12:14] != b"\x08\x00":
        return None
    ihl = (d[14] & 0x0f) * 4
    return (d[23], ".".join(str(b) for b in d[26:30]),
            ".".join(str(b) for b in d[30:34]), 14 + ihl)

def decode(pkt):
    got = ipv4_parts(pkt)
    if not got:
        return None
    proto, src, dst, base = got
    d = pkt["data"]

    if proto == 17:
        sport = int.from_bytes(d[base:base + 2], "big")
        dport = int.from_bytes(d[base + 2:base + 4], "big")
        p = d[base + 8:]
        if WG_PORT in (sport, dport) and p:
            kind = WG_TYPES.get(p[0], "type %d" % p[0])
            extra = ""
            if p[0] == 4 and len(p) >= 16:
                # The only two things in a data packet that are not encrypted:
                # which session it belongs to, and a counter that stops
                # anything being replayed.
                idx = int.from_bytes(p[4:8], "little")
                ctr = int.from_bytes(p[8:16], "little")
                extra = "  receiver=0x%08x counter=%d" % (idx, ctr)
            return {"kind": "WG", "src": src, "dst": dst,
                    "sport": sport, "dport": dport,
                    "readable": False,
                    "text": "%-21s %d bytes%s" % (kind, len(p), extra)}
        return None

    if proto == 6:
        sport = int.from_bytes(d[base:base + 2], "big")
        dport = int.from_bytes(d[base + 2:base + 4], "big")
        off = base + ((d[base + 12] >> 4) * 4)
        payload = d[off:]
        if not payload:
            return None
        text = payload.decode("ascii", "replace")
        lines = [ln for ln in text.replace("\r\n", "\n").split("\n") if ln]
        if not lines:
            return None
        return {"kind": "HTTP", "src": src, "dst": dst,
                "sport": sport, "dport": dport,
                "readable": True,
                "text": " | ".join(lines[:2])[:70]}

    if proto == 1:
        p = d[base:]
        if len(p) < 8:
            return None
        what = {8: "echo request", 0: "echo reply"}.get(p[0])
        if not what:
            return None
        return {"kind": "ICMP", "src": src, "dst": dst,
                "sport": 0, "dport": 0, "readable": True,
                "text": "%s seq=%d" % (what, int.from_bytes(p[6:8], "big"))}
    return None

class Tap:
    def __init__(self, node, num, label, sink, lock):
        self.node, self.num, self.label = node, num, label
        self.sink, self.lock = sink, lock
        self.live = False
        self.seen = 0
        self.secret_seen = 0
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
            # Look for the secret in the RAW bytes, before any decoding.
            if SECRET.encode() in pkt["data"]:
                with self.lock:
                    self.secret_seen += 1
            msg = decode(pkt)
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

PATH_TAPS = [
    (HA, 0, "hostA eth0  lanA"),
    (GA, 1, "gwA   eth1  lanC"),
    (HB, 0, "hostB eth0  lanB"),
]

def show_packets(sink, lock, title, limit=30):
    with lock:
        rows = sorted(sink, key=lambda r: r["when"])
    story("    %s" % title)
    if not rows:
        story("      (nothing crossed the sniffed interfaces)")
        return 0
    t0 = rows[0]["when"]
    story("      %-8s  %-18s  %-6s %-19s %-19s %s"
          % ("t+ms", "seen on", "what", "from", "to", "what a sniffer gets"))
    story("      " + "-" * 124)
    for r in rows[:limit]:
        m = r["msg"]
        story("      %-8.2f  %-18s  %-6s %-19s %-19s %s"
              % ((r["when"] - t0) * 1000.0, r["where"], m["kind"],
                 "%s:%d" % (m["src"], m["sport"]) if m["sport"] else m["src"],
                 "%s:%d" % (m["dst"], m["dport"]) if m["dport"] else m["dst"],
                 m["text"]))
    if len(rows) > limit:
        story("      ... and %d more" % (len(rows) - limit))
    story("      (times are when each sniffer read the packet, not when it")
    story("       crossed the wire: rows from different interfaces can be a")
    story("       few ms out of order when the machine is busy)")
    story()
    return len(rows)

def watch(do_it, title, settle=2.5):
    sink, lock = [], threading.Lock()
    taps = [Tap(n, e, lab, sink, lock) for n, e, lab in PATH_TAPS]
    for t in taps:
        if not t.ready():
            for x in taps:
                x.stop()
            return None
    try:
        time.sleep(1.5)
        with lock:
            sink.clear()
            for t in taps:
                t.secret_seen = 0
        out = do_it()
        time.sleep(settle)
    finally:
        for t in taps:
            t.stop()
    show_packets(sink, lock, title)
    found = {t.label: t.secret_seen for t in taps}
    return {"out": out, "found": found,
            "rows": sorted(sink, key=lambda r: r["when"])}

def report_secret(found):
    story("      the string the page contains, looked for in the RAW bytes")
    story("      of every packet, before anything was decoded:")
    for label, n in found.items():
        story("        %-20s %s" % (label,
              ("found in %d packet(s)" % n) if n else "not there at all"))
    story()

LAYOUT = {
    HA: (-570, 0),
    LAN_A: (-380, 0),
    GA: (-190, 0),
    LAN_C: (0, 0),
    GB: (190, 0),
    LAN_B: (380, 0),
    HB: (570, 0),
}

ETHS = {
    (GA, "eth0"): 270,
    (GA, "eth1"): 90,
    (GB, "eth0"): 90,
    (GB, "eth1"): 270,
    (HA, "eth0"): 90,
    (HB, "eth0"): 270,
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
    story("rootfs %s: two sites, one untrusted middle" % rootfs)
    story("""
    hostA -- lanA -- gwA -- lanC -- gwB -- lanB -- hostB
   10.1.0.10     10.1.0.1     |    10.0.0.2    10.2.0.10
                  10.0.0.1  "the internet"   10.2.0.1

    lanC is the untrusted middle: the only wire an eavesdropper is
    assumed to have. hostB serves a page containing a distinctive
    string, and the question in every act is whether that string can
    be read on lanC.
""")

    step("erasing the net, then placing the items")
    erase_net()
    place()

    step("creating the four containers")
    in_parallel(lambda n: create_cnt(n, rootfs), list(ALL), "add cnt")

    step("waiting for them to answer")
    in_parallel(wait_ready, list(ALL), "wait_ready")
    tools_present(GA, "wg")
    tools_present(HB, "python3")
    tools_present(HA, "curl")

    step("cabling them")
    for node in ALL:
        for num, lan, _ in WIRING[node]:
            cli("plug", node, "eth%d" % num, lan)

    step("addressing them")
    in_parallel(address, list(ALL), "address")
    in_parallel(enable_forwarding, list(GATES), "enable_forwarding")
    plain_routes()

    step("starting the page server")
    page_start()

    step("act 1: no tunnel yet, and lanC can read everything")
    story()
    a = watch(fetch, "one fetch, with nothing protecting it:")
    if a is None:
        sys.exit("could not sniff the path")
    story("  hostA got: %s" % (a["out"] or "(nothing)").strip())
    report_secret(a["found"])
    story("    This is what every demo before this one looked like. The")
    story("    request and the reply are as readable in the middle as they")
    story("    are at either end, because nothing has been done about it.")
    story()
    ok_plain = SECRET in (a["out"] or "")

    step("act 2: the tunnel comes up, and the handshake is caught")
    story()
    pub = wg_keys()
    b = watch(lambda: [wg_up(n, pub) for n in GATES] and tunnel_routes(),
              "the entire negotiation, from the outside:", settle=4.0)
    if b is None:
        sys.exit("could not sniff the path")
    story("    Two messages, and neither of them says anything. There is no")
    story("    version, no list of ciphers, no certificate, no name. Even")
    story("    the sender's identity inside the first message is encrypted")
    story("    to the recipient, so an eavesdropper cannot tell WHO is")
    story("    connecting, only that somebody did.")
    story()

    step("act 3: the identical fetch, through the tunnel")
    story()
    c = watch(fetch, "the same request, the same reply, the same three wires:")
    if c is None:
        sys.exit("could not sniff the path")
    story("  hostA got: %s" % (c["out"] or "(nothing)").strip())
    report_secret(c["found"])
    story("    Nothing changed on hostA or hostB. Neither of them was")
    story("    reconfigured, neither knows a tunnel exists, and the page")
    story("    arrived exactly as before.")
    story()
    story("    lanA and lanB are unchanged too -- a tunnel between the")
    story("    gateways protects the middle and nothing else, which is worth")
    story("    being clear about: this is not end to end encryption, and")
    story("    anybody on either lan still reads everything.")
    story()
    story("    lanC is the difference. Same packet count, same sizes, same")
    story("    timing -- and a decoder that can report the type and the")
    story("    length and then has nothing left to say.")
    story()
    ok_tunnel = SECRET in (c["out"] or "")
    ok_hidden = c["found"].get("gwA   eth1  lanC", 1) == 0
    ok_seen_before = a["found"].get("gwA   eth1  lanC", 0) > 0

    step("act 4: what the two ends know that the middle does not")
    story()
    for n in GATES:
        story("  %s:" % n)
        for line in wg_show(n).rstrip().split("\n"):
            story("      " + line)
        story()
    story("    latest handshake and the byte counters are local knowledge.")
    story("    Neither gateway told the other anything about them and")
    story("    nothing on lanC carries them.")
    story()

    if not (ok_plain and ok_tunnel):
        sys.exit("\nthe page did not arrive in one of the acts,"
                 " see the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")
    if not ok_seen_before:
        sys.exit("\nthe string was never found on lanC even in act 1, when"
                 " nothing\nwas protecting it. That means the lanC tap saw"
                 " nothing at all, so\nact 3 finding nothing says nothing."
                 " Check the tap before believing\nany of this.")
    if not ok_hidden:
        sys.exit("\nthe string was still readable on lanC with the tunnel up")

    story("""
the network is left up. Things worth trying:

  watch the tunnel carry traffic, live, and see how little it says:
    cloonix cli %s sniff gwA eth1 --dump

  compare the two ends of the same packet at the same time -- readable on
  one side of a gateway and not on the other:
    cloonix cli %s sniff hostA eth0 --dump
    cloonix cli %s sniff gwA eth1 --dump

  take the tunnel down and watch the same fetch become readable again:
    cloonix cli %s ssh gwA "ip link del wg0"
    cloonix cli %s ssh gwA "ip route replace 10.2.0.0/24 via 10.0.0.2"

  and change one byte of a peer's key, to see a handshake that never
  completes -- wireguard does not negotiate, it either knows you or it
  does not:
    cloonix cli %s ssh gwA "wg show"
""" % (NET, NET, NET, NET, NET, NET))

main()
