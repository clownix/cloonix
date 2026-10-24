#!/usr/bin/env python3
r"""proxy - one request, two connections, and a health check that notices.

    ./proxy.py [net]               the net defaults to nemo

Four machines and two lans::

  client -- lanA -- haproxy -- lanB -- web1
                                   \-- web2

  lanA 10.1.0.0/24   client .10   haproxy .1
  lanB 10.2.0.0/24   haproxy .1   web1 .11   web2 .12

The client asks haproxy for a page and never speaks to web1 or web2. Its
connection ends at the proxy: haproxy reads the whole request, then opens a
second connection of its own to a backend and asks again; the backend
learns who asked from the header haproxy adds, X-Forwarded-For.

  ACT 1  one request, caught on both lans at once: one page, two
         conversations.
  ACT 2  ten requests: round robin, half on web1 and half on web2, from the
         one address of the client.
  ACT 3  web1 stopped: the health check notices within a couple of seconds
         and every request goes to web2, the client seeing pages arrive.
  ACT 4  what each backend logged: without X-Forwarded-For every request
         comes from the proxy.

Image: proxy_trixie (haproxy and nginx-light)
Size:  4 containers, 2 lans

THE PACKET TO CATCH
-------------------
  One request, two connections, two captures:

    cloonix cli nemo cnf haproxy eth0 sniff=on
    cloonix cli nemo cnf haproxy eth1 sniff=on

    cloonix cli nemo sniff haproxy eth0 --dump tcp port 80
    cloonix cli nemo sniff haproxy eth1 --dump tcp port 80

  The packet to catch is the pair of SYNs: one from the client to 10.1.0.1,
  another from 10.2.0.1 to a backend, with other ports and other sequence
  numbers: two connections, nothing forwarded, a reverse proxy.

  Round robin, one filter narrower: every other request appears, none once
  web1 is killed, while lanA keeps being answered:

    cloonix cli nemo sniff haproxy eth1 --dump host 10.2.0.11

  The health check, traffic nobody asked for, a small connection to each
  backend on a timer:

    cloonix cli nemo sniff haproxy eth1 --dump tcp port 80 and less 100

Important
  This is the reverse proxy; services/squid.py is the forward one.
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
    trace_ssh, wait_ready, unavailable,
    needs_subuid)

ROOTFS_CHOICES = ("proxy_trixie",)

HISTORY = """\
A reverse proxy ends the client's connection and opens its own, an idea
as old as HTTP/1.1, RFC 2068 of 1997 by Roy Fielding and the others of
the HTTP working group, whose Via and Host headers exist for it. HAProxy
was started by Willy Tarreau in 2000 as a few hundred lines to spread
load between servers and has been the event-driven proxy of most large
sites since. nginx, the web server behind it, was written by Igor Sysoev
from 2002 at Rambler, in Moscow, and published in October 2004.
X-Forwarded-For was invented by the Squid developers in the 1990s and
standardised late, as the Forwarded header of RFC 7239 in 2014.

Today: nearly every public web site is reached through a reverse proxy;
HAProxy fronts GitHub, Reddit and Stack Overflow, and the health check shown
here is what takes a dead server out of rotation without an operator.
"""

CLI, LB, W1, W2 = "client", "haproxy", "web1", "web2"
BACKENDS = (W1, W2)
ALL = (CLI, LB, W1, W2)
LAN_A, LAN_B = "lanA", "lanB"

WIRING = {
    CLI: [(0, LAN_A, "10.1.0.10/24")],
    LB:  [(0, LAN_A, "10.1.0.1/24"), (1, LAN_B, "10.2.0.1/24")],
    W1:  [(0, LAN_B, "10.2.0.11/24")],
    W2:  [(0, LAN_B, "10.2.0.12/24")],
}

GATEWAY = {W1: "10.2.0.1", W2: "10.2.0.1"}
FRONT = "10.1.0.1"                       # what the client asks
BACK = {W1: "10.2.0.11", W2: "10.2.0.12"}
HEALTH_MS = 1000                         # how often a backend is checked

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
                 "  falls back to nemo:   ./proxy.py <net>"
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
             "proxy_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_proxy_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said or "nothing"))

def tools_present(node, *tools):
    for t in tools:
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                            "command -v %s >/dev/null 2>&1" % t],
                           capture_output=True, stdin=subprocess.DEVNULL)
        if r.returncode:
            unavailable("%s has no %s: this is not the proxy image.\n"
                     "  fetch it:\n"
                     "    cd /var/lib/cloonix/bulk\n"
                     "    wget https://clownix.net/downloads/cnt_demo_bulk/"
                     "proxy_trixie.tar.gz\n"
                     "    (cloonix extracts it at the first use)\n"
                     "  or build it:\n"
                     "    cd vm_create/cnt_kvm_demo && ./cnt_proxy_trixie.sh"
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
    """haproxy is also the router between the two lans."""
    ssh(node, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    got = ssh(node, "sysctl -n net.ipv4.ip_forward", check=False).strip()
    if got != "1":
        raise RuntimeError("%s would not enable ip_forward, it reads %r"
                           % (node, got))
    trace("%s forwards ipv4", node)

# the two backends

def nginx_conf(node):
    """One site, saying which machine it is, and logging who asked.

    The log format is the interesting half. The default one records
    $remote_addr, which behind a proxy is ALWAYS the proxy -- every request
    in the log looks as though it came from the same machine, because as far
    as tcp is concerned it did. $http_x_forwarded_for is the header haproxy
    adds, and putting both side by side in one line is the clearest way to
    show what that header is for.
    """
    return """user root;
worker_processes 1;
error_log /var/log/nginx/error.log;
pid /run/nginx.pid;
events { worker_connections 64; }
http {
    access_log /var/log/nginx/access.log;
    log_format who "from=$remote_addr xff=$http_x_forwarded_for $request";
    access_log /var/log/nginx/who.log who;
    server {
        listen 80 default_server;
        location / {
            add_header X-Served-By NODE always;
            return 200 "you were served by NODE\\n";
        }
        location /health {
            return 200 "ok\\n";
        }
    }
}
""".replace("NODE", node)

def nginx_start(node):
    ssh(node, "mkdir -p /etc/nginx /var/log/nginx /var/www/html")
    put(node, "/etc/nginx/nginx.conf", nginx_conf(node))
    # Told to stay root.
    out = ssh(node, "nginx -t 2>&1 && echo CFG_OK", check=False)
    if "CFG_OK" not in out:
        raise RuntimeError("nginx will not accept its config on %s:\n%s"
                           % (node, out))
    ssh(node, "nginx </dev/null >/dev/null 2>&1")
    for _ in range(20):
        if ssh(node, "pgrep -x nginx >/dev/null && echo up",
               check=False).strip():
            trace("%s: nginx serving on port 80", node)
            return
        time.sleep(0.5)
    raise RuntimeError("nginx did not start on %s:\n%s"
                       % (node, ssh(node, "cat /var/log/nginx/error.log",
                                    check=False)))

def stop_backend(node):
    ssh(node, "nginx -s stop 2>/dev/null || pkill -x nginx; true", check=False)
    trace("%s: nginx stopped", node)

def start_backend(node):
    ssh(node, "nginx </dev/null >/dev/null 2>&1", check=False)
    trace("%s: nginx started again", node)

# the proxy

def haproxy_conf():
    """One front end, two servers, and a check that runs every second.

    forwardfor is one line and it is the whole of act 4: it makes haproxy
    add an X-Forwarded-For header carrying the address of whoever really
    asked. Without it a backend cannot tell one user from another, because
    every connection it sees comes from the proxy.

    The check interval is deliberately short. A real deployment would use
    something slower and would want two or three failures before believing
    it; a second here means act 3 happens while somebody is still watching.
    """
    return """global
    user root
    group root
    maxconn 256
    log stdout format raw daemon

defaults
    mode http
    log global
    timeout connect 3s
    timeout client 10s
    timeout server 10s
    option httplog
    option forwardfor

frontend in
    bind *:80
    default_backend pool

backend pool
    balance roundrobin
    option httpchk GET /health
    server web1 %s:80 check inter %dms fall 2 rise 2
    server web2 %s:80 check inter %dms fall 2 rise 2
""" % (BACK[W1], HEALTH_MS, BACK[W2], HEALTH_MS)

def haproxy_start():
    ssh(LB, "mkdir -p /etc/haproxy /run/haproxy /var/lib/haproxy")
    put(LB, "/etc/haproxy/haproxy.cfg", haproxy_conf())
    # Ask whether it SUCCEEDED, not whether the output contains a word.
    out = ssh(LB, "haproxy -c -f /etc/haproxy/haproxy.cfg 2>&1 && echo CFG_OK",
              check=False)
    if "CFG_OK" not in out:
        raise RuntimeError("haproxy will not accept its config:\n%s" % out)
    warned = [ln for ln in out.split("\n") if "WARNING" in ln]
    if warned:
        trace("the configuration is usable, with %d warning(s): %s",
              len(warned), warned[0].strip()[:90])
    else:
        trace("the haproxy configuration is valid, nothing to report")
    ssh(LB, "haproxy -db -f /etc/haproxy/haproxy.cfg "
            "</dev/null >/var/log/haproxy.out 2>&1 &",
        check=False)
    # up is the port bound, not the process seen: a client in between is
    # refused
    for _ in range(40):
        if ssh(LB, "pgrep -x haproxy >/dev/null && "
                   "ss -ltn | grep -q ':80 ' && echo up",
               check=False).strip():
            trace("haproxy listening on %s, checking both backends every %dms",
                  FRONT, HEALTH_MS)
            return
        time.sleep(0.5)
    raise RuntimeError("haproxy did not start:\n%s"
                       % ssh(LB, "cat /var/log/haproxy.out", check=False))

# the wire: http is text, like smtp, and needs no decoder

def conn_key(src, sport, dst, dport):
    """One name for a connection, whichever way a packet is going.

    Both ends sorted, so a request and its reply share a key and a whole
    conversation can be picked out of the table as a unit.
    """
    return tuple(sorted([(src, sport), (dst, dport)]))

def real_request_keys(rows):
    """The connections that carried a REAL request, not a health check.

    Without this, act 1 is unreadable. haproxy checks both backends once a
    second, each check is a whole tcp connection -- SYN, request, reply, FIN
    -- and by the time a person has typed the curl command forty of those
    have gone past. The one request to look at ends up somewhere in
    the middle, under "and 15 more".

    A check is recognisable by what it asks for, and everything else on the
    same connection goes with it: the handshake that opened it and the FIN
    that closed it are as uninteresting as the request itself.
    """
    keys = set()
    for r in rows:
        req = r["msg"].get("request")
        if req and "/health" not in req:
            keys.add(r["msg"]["key"])
    return keys

def decode_http(pkt):
    d = pkt["data"]
    if len(d) < 34 or d[12:14] != b"\x08\x00" or d[23] != 6:
        return None
    ihl = (d[14] & 0x0f) * 4
    base = 14 + ihl
    if len(d) < base + 20:
        return None
    sport = int.from_bytes(d[base:base + 2], "big")
    dport = int.from_bytes(d[base + 2:base + 4], "big")
    if 80 not in (sport, dport):
        return None
    flags = d[base + 13]
    off = base + ((d[base + 12] >> 4) * 4)
    payload = d[off:]
    src = ".".join(str(b) for b in d[26:30])
    dst = ".".join(str(b) for b in d[30:34])
    if not payload:
        if flags & 0x02:
            what = "SYN, opening a connection" + (" (ack)" if flags & 0x10
                                                  else "")
        elif flags & 0x01:
            what = "FIN, closing it"
        else:
            return None
        return {"kind": "TCP", "src": src, "dst": dst,
                "sport": sport, "dport": dport,
                "key": conn_key(src, sport, dst, dport), "text": what}
    text = payload.decode("ascii", "replace")
    lines = [ln for ln in text.replace("\r\n", "\n").split("\n") if ln]
    if not lines:
        return None
    keep = [lines[0]]
    for ln in lines[1:]:
        low = ln.lower()
        if low.startswith(("host:", "x-forwarded-for:", "x-served-by:",
                           "user-agent:", "connection:")):
            keep.append(ln)
    body = [ln for ln in lines if ln.startswith("you were served")]
    keep += body
    return {"kind": "HTTP", "src": src, "dst": dst,
            "sport": sport, "dport": dport,
            "key": conn_key(src, sport, dst, dport),
            "request": lines[0] if lines[0].startswith(("GET", "POST", "HEAD"))
                       else None,
            "text": " | ".join(keep[:4])}

class Tap:
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
            msg = decode_http(pkt)
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

# One per wire: the client side of the proxy, and the backend side.
PATH_TAPS = [
    (CLI, 0, "client eth0  lanA"),
    (LB, 1, "haproxy eth1 lanB"),
]

def show_packets(sink, lock, title, kinds=None, limit=40, only_keys=None):
    with lock:
        rows = sorted(sink, key=lambda r: r["when"])
    if kinds:
        rows = [r for r in rows if r["msg"]["kind"] in kinds]
    if only_keys is not None:
        rows = [r for r in rows if r["msg"]["key"] in only_keys]
    story("    %s" % title)
    if not rows:
        story("      (nothing crossed the sniffed interfaces)")
        return 0
    t0 = rows[0]["when"]
    story("      %-8s  %-18s  %-21s %-21s %s"
          % ("t+ms", "seen on", "from", "to", "what went past"))
    story("      " + "-" * 122)
    for r in rows[:limit]:
        m = r["msg"]
        story("      %-8.2f  %-18s  %-21s %-21s %s"
              % ((r["when"] - t0) * 1000.0, r["where"],
                 "%s:%d" % (m["src"], m["sport"]),
                 "%s:%d" % (m["dst"], m["dport"]), m["text"][:64]))
    if len(rows) > limit:
        story("      ... and %d more" % (len(rows) - limit))
    story("      (times are when each sniffer read the packet, not when it")
    story("       crossed the wire: rows from different interfaces can be a")
    story("       few ms out of order when the machine is busy)")
    story()
    return len(rows)

def watch(do_it, title, kinds=None, settle=2.5, pick_request=False):
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
        out = do_it()
        time.sleep(settle)
    finally:
        for t in taps:
            t.stop()
    rows = sorted(sink, key=lambda r: r["when"])
    keys = real_request_keys(rows) if pick_request else None
    show_packets(sink, lock, title, kinds=kinds, only_keys=keys)
    if pick_request:
        checks = len([r for r in rows
                      if (r["msg"].get("request") or "").find("/health") >= 0])
        if checks:
            story("      %d health checks went past on lanB in the same"
                  % checks)
            story("      window and are left out of the table. They are one")
            story("      whole connection each, once a second per backend,")
            story("      and they are what act 3 depends on.")
            story()
    return {"out": out, "rows": rows}

def fetch(times=1, show_headers=False):
    """Ask the proxy for the page, and report which backend answered."""
    opt = "-sS -m 5" + (" -D -" if show_headers else "")
    cmd = ("for i in $(seq 1 %d); do curl %s http://%s/ ; done"
           % (times, opt, FRONT))
    return ssh(CLI, cmd, check=False)

def who_answered(out):
    """Count the backends named in a run of replies."""
    counts = {}
    for name in BACKENDS:
        counts[name] = out.count("served by " + name)
    return counts

def backend_state():
    """What haproxy thinks of each server, from its own log."""
    return ssh(LB, "tail -n 6 /var/log/haproxy.out 2>/dev/null", check=False)

LAYOUT = {
    CLI: (-380, 0),
    LAN_A: (-190, 0),
    LB: (0, 0),
    LAN_B: (190, 0),
    W1: (380, -75),
    W2: (380, 75),
}

ETHS = {
    (CLI, "eth0"): 90,
    (LB, "eth0"): 270,
    (LB, "eth1"): 90,
    (W1, "eth0"): 248,
    (W2, "eth0"): 292,
}

def place():
    for name, (x, y) in LAYOUT.items():
        cli("cnf", name, "x=%s" % x, "y=%s" % y, ack=ACK_FAST)
    for (name, num), deg in ETHS.items():
        cli("cnf", name, num, "angle=%s" % deg, ack=ACK_FAST)

def main():
    needs_subuid()
    story(HISTORY)
    step("looking for the image")
    rootfs = choose_rootfs()
    story("rootfs %s: one proxy, two backends, one client" % rootfs)
    story("""
    client -- lanA -- haproxy -- lanB -- web1
   10.1.0.10       10.1.0.1  \\        10.2.0.11
                   10.2.0.1   \\--- web2
                                  10.2.0.12

    The client asks %s for a page. It never speaks to web1 or web2 and
    does not know they exist. Its connection ENDS at the proxy, which
    opens a second one of its own to ask on the client's behalf.
""" % FRONT)

    step("erasing the net, then placing the items")
    erase_net()
    place()

    step("creating the four containers")
    in_parallel(lambda n: create_cnt(n, rootfs), list(ALL), "add cnt")

    step("waiting for them to answer")
    in_parallel(wait_ready, list(ALL), "wait_ready")
    tools_present(LB, "haproxy")
    tools_present(W1, "nginx")
    tools_present(CLI, "curl")

    step("cabling them")
    for node in ALL:
        for num, lan, _ in WIRING[node]:
            cli("plug", node, "eth%d" % num, lan)

    step("addressing them")
    in_parallel(address, list(ALL), "address")
    enable_forwarding(LB)

    step("starting the two backends")
    in_parallel(nginx_start, list(BACKENDS), "nginx_start")

    step("starting the proxy")
    haproxy_start()
    time.sleep(2.5)          # let the first health checks run

    step("act 1: one request, and the two connections it is")
    story()
    got = watch(lambda: fetch(1, show_headers=True),
                "the two connections that carried one page:",
                pick_request=True)
    if got is None:
        sys.exit("could not sniff the path")
    story("  what curl received:")
    for line in (got["out"] or "").rstrip().split("\n")[:14]:
        story("      " + line)
    story()
    story("    Two conversations, not one:")
    story()
    story("      * on lanA, 10.1.0.10 -> 10.1.0.1:80. The client's whole")
    story("        request, and it stops there.")
    story("      * on lanB, 10.2.0.1 -> a backend:80. A DIFFERENT connection,")
    story("        from a different address, opened by haproxy because it")
    story("        decided to. The client has no part in it.")
    story("      * the request on lanB carries a header the one on lanA did")
    story("        not: X-Forwarded-For, with the client's address in it.")
    story("        That is the only reason the backend has any idea who")
    story("        asked, and act 4 shows what happens in its log.")
    story()

    step("act 2: ten requests, and the round robin becomes visible")
    story()
    out = fetch(10)
    counts = who_answered(out)
    for name in BACKENDS:
        story("      %-6s answered %d of 10" % (name, counts[name]))
    story()
    story("    One address asked ten times and two machines answered. The")
    story("    client changed nothing between requests -- balance roundrobin")
    story("    in the proxy is the whole of it.")
    story()
    ok_balanced = all(counts[n] > 0 for n in BACKENDS)
    if not ok_balanced:
        story("    (expected both to answer; see the haproxy log below)")
        story(backend_state())

    step("act 3: web1 is stopped, and the check notices")
    story()
    stop_backend(W1)
    story("    waiting for the health check to fail twice, at %dms each:"
          % HEALTH_MS)
    down_at = None
    t0 = time.time()
    for _ in range(30):
        out = fetch(2)
        c = who_answered(out)
        if c[W1] == 0 and c[W2] > 0:
            down_at = time.time() - t0
            break
        time.sleep(0.5)
    if down_at is None:
        story("    haproxy never took web1 out. Its log:")
        story(backend_state())
    else:
        story("      web1 was out of the pool after about %.1f s" % down_at)
        story()
        out = fetch(6)
        c = who_answered(out)
        story("      six more requests: web1 %d, web2 %d"
              % (c[W1], c[W2]))
        story()
        story("    The client saw nothing. Every request it made was")
        story("    answered, by the machine that was still standing, and")
        story("    nothing about the client changed. That is the difference")
        story("    between a proxy and a dns round robin: a name that")
        story("    resolves to two addresses keeps sending half the traffic")
        story("    at a machine that has stopped answering.")
        story()
    ok_failover = down_at is not None

    start_backend(W1)

    step("act 4: what each backend logged")
    story()
    for name in BACKENDS:
        story("  %s, one line per request -- who tcp says asked, and who "
              "did:" % name)
        log = ssh(name, "grep -v /health /var/log/nginx/who.log 2>/dev/null "
                        "| tail -n 6", check=False)
        for line in (log.rstrip().split("\n") if log.strip()
                     else ["(no real requests reached this one)"]):
            story("      " + line)
        checks = ssh(name, "grep -c /health /var/log/nginx/who.log "
                           "2>/dev/null || true", check=False).strip()
        if checks and checks != "0":
            story("      ... and %s health checks, left out. Note their"
                  " xff= is empty:" % checks)
            story("      haproxy adds the header to what it FORWARDS, and"
                  " nobody forwarded those.")
        story()
    story("    from= is the same on every line: as far as tcp is concerned")
    story("    every request came from the proxy, because it did. xff= is")
    story("    the header haproxy added, and it is the only place the real")
    story("    client survives. Turn option forwardfor off and a backend")
    story("    log tells nothing about users.")
    story()

    if not (ok_balanced and ok_failover):
        sys.exit("\nsomething did not work,"
                 " see the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")

    story("""
the network is left up. Things to try:

  watch the two connections live, one on each lan, at the same time:
    cloonix cli %s sniff client eth0 --dump
    cloonix cli %s sniff haproxy eth1 --dump

  stop BOTH backends and see what a proxy with nothing behind it says. It
  is a 503, and it comes from haproxy itself -- the client gets an answer
  from a machine that never had the page:
    cloonix cli %s ssh web1 "nginx -s stop"
    cloonix cli %s ssh web2 "nginx -s stop"
    cloonix cli %s ssh client "curl -i http://%s/"

  turn the forwarded header off and watch the backend logs lose the client:
    cloonix cli %s ssh haproxy "sed -i /forwardfor/d /etc/haproxy/haproxy.cfg"
    cloonix cli %s ssh haproxy "pkill haproxy; haproxy -D -f /etc/haproxy/haproxy.cfg"
""" % (NET, NET, NET, NET, NET, FRONT, NET, NET))

main()
