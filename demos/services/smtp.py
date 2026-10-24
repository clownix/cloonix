#!/usr/bin/env python3
"""smtp - a mail server asks dns where to deliver, then says so in words.

    ./smtp.py [net]                the net defaults to nemo

Five machines and three lans::

  userA -- lanA -- mailA -- lanC -- mailB -- lanB -- userB
                              |
                             dns

  lanA  10.1.0.0/24   userA .10   mailA .1
  lanB  10.2.0.0/24   userB .10   mailB .1
  lanC  10.0.0.0/24   mailA .1    mailB .2    dns .3

Two mail domains, a.lab and b.lab, one per site; userA sends to bob@b.lab.
Mail routing is dns: mailA asks for the MX record of b.lab, resolves the
host it names, and only then moves the mail. smtp itself is plain text:
the greeting, the sender, the recipient, the message and its acceptance are
readable on the wire with no decoder, and the demo prints them as the two
ends say them.

  ACT 1  userA hands the message to its own server, in a conversation the
         demo speaks itself, line by line.
  ACT 2  mailA finds where b.lab lives, MX then A, the lookups caught on
         lanC.
  ACT 3  mailA delivers to mailB: the same protocol between two servers.
  ACT 4  the message read out of the mailbox where it landed.

Image: smtp_trixie (postfix, bind9, s-nail)
Size:  5 containers, 3 lans

THE PACKET TO CATCH
-------------------
  Two protocols in sequence on one wire, one subscription left running:

    cloonix cli nemo cnf mailA eth1 sniff=on
    cloonix cli nemo sniff mailA eth1 --dump udp port 53 or tcp port 25

  The packet to catch is the MX query, then, to the address its answer
  gave, the SYN that opens the SMTP session: cause and effect in one
  capture. The conversation itself:

    cloonix cli nemo sniff mailA eth1 --dump tcp port 25

  EHLO, MAIL FROM, RCPT TO, DATA and the body in ASCII, headers, subject
  and text readable as typed; security/wg.py shows the same page of hex
  with a reason to be opaque.
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

ROOTFS_CHOICES = ("smtp_trixie",)

HISTORY = """\
Jon Postel wrote SMTP as RFC 821 in August 1982: a text dialogue, HELO,
MAIL FROM, RCPT TO, DATA, that still runs unchanged, the current text
being RFC 5321 by John Klensin in 2008. The MX record that tells a
sender which host takes mail for a domain was added to the DNS by Craig
Partridge in RFC 974 in 1986. Postfix was written by Wietse Venema at
the IBM T.J. Watson Research Center and released in December 1998 as a
replacement for Eric Allman's sendmail of 1983, built as a set of
small processes that trust each other as little as possible.

Today: every mail still crosses the internet as the SMTP dialogue of 1982, now
wrapped in TLS and checked by SPF, DKIM and DMARC; Postfix is the default mail
server of most linux distributions and relays a large share of it.
"""

UA, UB, MA, MB, DNS = "userA", "userB", "mailA", "mailB", "dns"
SERVERS = (MA, MB)
ALL = (UA, MA, DNS, MB, UB)
LAN_A, LAN_B, LAN_C = "lanA", "lanB", "lanC"

WIRING = {
    UA:  [(0, LAN_A, "10.1.0.10/24")],
    MA:  [(0, LAN_A, "10.1.0.1/24"), (1, LAN_C, "10.0.0.1/24")],
    DNS: [(0, LAN_C, "10.0.0.3/24")],
    MB:  [(0, LAN_B, "10.2.0.1/24"), (1, LAN_C, "10.0.0.2/24")],
    UB:  [(0, LAN_B, "10.2.0.10/24")],
}

GATEWAY = {UA: "10.1.0.1", UB: "10.2.0.1"}
DNS_IP = "10.0.0.3"
ZONE = "lab"

# domain -> (mail host name, its address, the machine running it)
DOMAINS = {
    "a.lab": ("mailA.lab", "10.0.0.1", MA),
    "b.lab": ("mailB.lab", "10.0.0.2", MB),
}

# Every name the zone knows about.
RECORDS = [
    ("mailA", "10.0.0.1"),
    ("mailB", "10.0.0.2"),
    ("dns",   DNS_IP),
    ("userA", "10.1.0.10"),
    ("userB", "10.2.0.10"),
]

SENDER, RCPT = "alice@a.lab", "bob@b.lab"

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
                 "  falls back to nemo:   ./smtp.py <net>"
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
             "smtp_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_smtp_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said or "nothing"))

def tools_present(node, *tools):
    for t in tools:
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                            "command -v %s >/dev/null 2>&1" % t],
                           capture_output=True, stdin=subprocess.DEVNULL)
        if r.returncode:
            unavailable("%s has no %s: this is not the smtp image.\n"
                     "  fetch it:\n"
                     "    cd /var/lib/cloonix/bulk\n"
                     "    wget https://clownix.net/downloads/cnt_demo_bulk/"
                     "smtp_trixie.tar.gz\n"
                     "    (cloonix extracts it at the first use)\n"
                     "  or build it:\n"
                     "    cd vm_create/cnt_kvm_demo && ./cnt_smtp_trixie.sh"
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
    """The two mail servers are also the routers between the sites."""
    ssh(node, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    got = ssh(node, "sysctl -n net.ipv4.ip_forward", check=False).strip()
    if got != "1":
        raise RuntimeError("%s would not enable ip_forward, it reads %r"
                           % (node, got))
    trace("%s forwards ipv4", node)

def routes():
    ssh(MA, "ip route replace 10.2.0.0/24 via 10.0.0.2")
    ssh(MB, "ip route replace 10.1.0.0/24 via 10.0.0.1")
    trace("each site knows how to reach the other")

def resolver(node):
    """One name server for everybody, and it is not on a mail machine.

    Act 2 shows that mail routing is a dns question, and that is
    only visible if the question has to leave the machine asking it.
    """
    ssh(node, "printf 'nameserver %s\\n' > /etc/resolv.conf" % DNS_IP)

# the name server: MX records and nothing else interesting

def zone_file():
    out = ["$TTL 60",
           "@ IN SOA dns." + ZONE + ". admin." + ZONE + ". ( 1 60 60 600 60 )",
           "@ IN NS dns." + ZONE + "."]
    for name, ip in RECORDS:
        out.append("%s IN A %s" % (name, ip))
    for domain, (host, _ip, _node) in DOMAINS.items():
        label = domain.split(".")[0]
        out.append("%s IN MX 10 %s." % (label, host))
        out.append("%s IN A %s" % (label, "10.0.0.1"
                                   if label == "a" else "10.0.0.2"))
    return "\n".join(out) + "\n"

def named_conf():
    return "\n".join([
        "options {",
        "    directory \"/var/cache/bind\";",
        "    listen-on { any; };",
        "    listen-on-v6 { none; };",
        "    recursion yes;",
        "    allow-query { any; };",
        "    dnssec-validation no;",
        "    querylog yes;",
        "};",
        "zone \"" + ZONE + "\" {",
        "    type master;",
        "    file \"/var/cache/bind/" + ZONE + ".zone\";",
        "};",
        "",
    ])

def dns_start():
    ssh(DNS, "mkdir -p /etc/bind /var/cache/bind")
    put(DNS, "/etc/bind/named.conf", named_conf())
    put(DNS, "/var/cache/bind/" + ZONE + ".zone", zone_file())
    out = ssh(DNS, "named-checkzone %s /var/cache/bind/%s.zone 2>&1 && echo CFG_OK"
                   % (ZONE, ZONE), check=False)
    if "CFG_OK" not in out:
        raise RuntimeError("the zone file is not valid:\n%s" % out)
    out = ssh(DNS, "named-checkconf /etc/bind/named.conf", check=False)
    if out.strip():
        raise RuntimeError("named.conf is not valid:\n%s" % out)
    ssh(DNS, "named -u root -c /etc/bind/named.conf "
             "</dev/null >/var/log/named.out 2>&1 &")
    # up is the udp port 53 bound, not the process seen: named loads its
    # zones before it listens, and a query in between is refused
    for _ in range(40):
        if ssh(DNS, "pgrep -x named >/dev/null && "
                    "ss -lun | grep -q ':53 ' && echo up",
               check=False).strip():
            trace("the name server is up, holding MX records for %s",
                  " and ".join(DOMAINS))
            return
        time.sleep(0.5)
    # A daemonized named reports to syslog and nothing to named.out: run it
    # once more in the foreground, where the reason goes to stderr.
    raise RuntimeError("named did not start:\n%s"
                       % ssh(DNS, "timeout 5 named -u root -g "
                                  "-c /etc/bind/named.conf 2>&1 | tail -20",
                             check=False))

# postfix

def postfix_conf(node):
    """main.cf for one site.

    mynetworks is wide open and smtpd_relay_restrictions permits the local
    network. On a real server that would be a disaster -- it is what an open
    relay is -- and here it is exactly what is wanted: the demo needs mailA
    to accept a message from userA and pass it on, which is relaying, and
    every restriction that stops that in the real world would stop this too.
    """
    host, _ip, _n = DOMAINS[domain_of(node)]
    return "\n".join([
        "myhostname = " + host,
        "mydomain = " + domain_of(node),
        "myorigin = " + domain_of(node),
        "mydestination = " + domain_of(node) + ", " + host + ", localhost",
        "mynetworks = 10.0.0.0/8, 127.0.0.0/8",
        "inet_interfaces = all",
        "inet_protocols = ipv4",
        "mailbox_size_limit = 0",
        "alias_maps = hash:/etc/aliases",
        "alias_database = hash:/etc/aliases",
        "local_recipient_maps = proxy:unix:passwd.byname $alias_maps",
        "maillog_file = /var/log/postfix.log",
        "smtpd_relay_restrictions = permit_mynetworks, reject",
        "smtpd_recipient_restrictions = permit_mynetworks, reject",
        "queue_run_delay = 10s",
        "minimal_backoff_time = 10s",
        "maximal_backoff_time = 30s",
        # The dns server is the only one that knows about these domains.
        "smtp_host_lookup = dns",
        "",
    ])

def domain_of(node):
    for dom, (_h, _ip, n) in DOMAINS.items():
        if n == node:
            return dom
    raise RuntimeError("%s runs no mail domain" % node)

def postfix_start(node):
    ssh(node, "mkdir -p /etc/postfix /var/spool/postfix /var/mail")
    put(node, "/etc/postfix/main.cf", postfix_conf(node))
    # Unchroot smtp in master.cf: chrooted it cannot read /etc/resolv.conf.
    ssh(node, "sed -i 's/^\\\\(smtp .*inet.*\\\\)y\\\\(.*\\\\)/\\\\1n\\\\2/' "
              "/etc/postfix/master.cf", check=False)
    ssh(node, "sed -i 's/ y / n /g' /etc/postfix/master.cf", check=False)
    # The aliases file and its database, which nothing else will make.
    put(node, "/etc/aliases",
        "postmaster: root\nroot: root\nMAILER-DAEMON: postmaster\n")
    ssh(node, "newaliases 2>&1", check=False)
    # useradd (passwd) is in every image; adduser may be absent.
    for who in ("alice", "bob"):
        out = ssh(node, "id %s >/dev/null 2>&1 || useradd -m -s /bin/sh %s "
                        "2>&1; id %s 2>&1" % (who, who, who), check=False)
        if ("uid=" not in out):
            raise RuntimeError("%s: the user %s could not be created: %s"
                               % (node, who, out.strip() or "no output"))
    ssh(node, "postfix set-permissions >/dev/null 2>&1; true", check=False)
    # Silence the "backwards-compatible default settings" warning, without
    # pinning a version.
    lvl = ssh(node, "postconf -h -d compatibility_level 2>/dev/null",
              check=False).strip()
    if lvl:
        ssh(node, "postconf -e compatibility_level=%s >/dev/null 2>&1; true"
                  % lvl, check=False)
        trace_ssh("%s: compatibility_level set to %s, its own default",
                  node, lvl)
    if not ssh(node, "test -f /etc/aliases.db && echo yes",
               check=False).strip():
        raise RuntimeError("newaliases did not build /etc/aliases.db on %s, "
                           "so every recipient will be a lookup failure:\n%s"
                           % (node, ssh(node, "newaliases 2>&1", check=False)))
    out = ssh(node, "postfix check 2>&1", check=False)
    if out.strip():
        trace("%s: postfix check said: %s", node,
              out.strip().replace("\n", " | ")[:150])
    else:
        trace("%s: postfix check is happy", node)
    ssh(node, "postfix start </dev/null >/var/log/postfix.out 2>&1",
        check=False)
    # up is the port bound, not the process seen: a client in between is
    # refused
    for _ in range(40):
        if ssh(node, "pgrep -x master >/dev/null && "
                     "ss -ltn | grep -q ':25 ' && echo up",
               check=False).strip():
            trace("%s: postfix running for %s", node, domain_of(node))
            return
        time.sleep(0.5)
    raise RuntimeError("postfix did not start on %s:\n%s\n%s"
                       % (node, ssh(node, "cat /var/log/postfix.out",
                                    check=False),
                          ssh(node, "tail -n 20 /var/log/postfix.log",
                              check=False)))

# The wire: smtp is text, dns is not

DNS_TYPES = {1: "A", 2: "NS", 5: "CNAME", 6: "SOA", 15: "MX", 28: "AAAA",
             41: "OPT", 255: "ANY"}

def ipv4(pkt):
    d = pkt["data"]
    if len(d) < 34 or d[12:14] != b"\x08\x00":
        return None
    ihl = (d[14] & 0x0f) * 4
    return (d[23], ".".join(str(b) for b in d[26:30]),
            ".".join(str(b) for b in d[30:34]), 14 + ihl)

def dns_name(data, off):
    labels, jumped, end, guard = [], False, off, 0
    while True:
        guard += 1
        if guard > 128 or off >= len(data):
            break
        ln = data[off]
        if ln == 0:
            off += 1
            if not jumped:
                end = off
            break
        if (ln & 0xc0) == 0xc0:
            if off + 1 >= len(data):
                break
            ptr = ((ln & 0x3f) << 8) | data[off + 1]
            if not jumped:
                end = off + 2
            jumped = True
            off = ptr
            continue
        off += 1
        labels.append(data[off:off + ln].decode("ascii", "replace"))
        off += ln
        if not jumped:
            end = off
    return ".".join(labels) if labels else ".", end

def decode_dns(pkt):
    got = ipv4(pkt)
    if not got or got[0] != 17:
        return None
    proto, src, dst, base = got
    d = pkt["data"]
    sport = int.from_bytes(d[base:base + 2], "big")
    dport = int.from_bytes(d[base + 2:base + 4], "big")
    if 53 not in (sport, dport):
        return None
    p = d[base + 8:]
    if len(p) < 12:
        return None
    qr = (p[2] >> 7) & 1
    qd = int.from_bytes(p[4:6], "big")
    an = int.from_bytes(p[6:8], "big")
    off = 12
    qname, qtype = "?", "?"
    for _ in range(qd):
        qname, off = dns_name(p, off)
        if off + 4 > len(p):
            break
        qtype = DNS_TYPES.get(int.from_bytes(p[off:off + 2], "big"), "?")
        off += 4
    answers = []
    for _ in range(an):
        name, off = dns_name(p, off)
        if off + 10 > len(p):
            break
        rtype = int.from_bytes(p[off:off + 2], "big")
        rdlen = int.from_bytes(p[off + 8:off + 10], "big")
        off += 10
        rd = p[off:off + rdlen]
        if rtype == 1 and rdlen == 4:
            answers.append("A " + ".".join(str(b) for b in rd))
        elif rtype == 15 and rdlen > 2:
            pref = int.from_bytes(rd[:2], "big")
            host, _ = dns_name(p, off + 2)
            answers.append("MX %d %s" % (pref, host))
        else:
            answers.append(DNS_TYPES.get(rtype, str(rtype)))
        off += rdlen
    return {"kind": "DNS", "src": src, "dst": dst,
            "text": ("%-8s %s %s%s"
                     % ("query" if not qr else "answer", qname, qtype,
                        ("   ->  " + ", ".join(answers)) if answers else ""))}

def decode_smtp(pkt):
    got = ipv4(pkt)
    if not got or got[0] != 6:
        return None
    proto, src, dst, base = got
    d = pkt["data"]
    sport = int.from_bytes(d[base:base + 2], "big")
    dport = int.from_bytes(d[base + 2:base + 4], "big")
    if 25 not in (sport, dport):
        return None
    off = base + ((d[base + 12] >> 4) * 4)
    payload = d[off:]
    if not payload:
        return None            # a bare ack carries nothing to print
    try:
        text = payload.decode("ascii", "replace")
    except Exception:                                   # noqa: BLE001
        return None
    lines = [ln for ln in text.replace("\r\n", "\n").split("\n") if ln]
    if not lines:
        return None
    return {"kind": "SMTP", "src": src, "dst": dst,
            "text": " | ".join(lines[:3])
                    + (" ..." if len(lines) > 3 else "")}

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
            msg = decode_dns(pkt) or decode_smtp(pkt)
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

# Two taps, not three.
PATH_TAPS = [
    (UA, 0, "userA eth0  lanA"),
    (MA, 1, "mailA eth1  lanC"),
]

def show_packets(sink, lock, title, kinds=None, limit=45):
    with lock:
        rows = sorted(sink, key=lambda r: r["when"])
    if kinds:
        rows = [r for r in rows if r["msg"]["kind"] in kinds]
    story("    %s" % title)
    if not rows:
        story("      (nothing of that kind crossed the sniffed interfaces)")
        return 0
    t0 = rows[0]["when"]
    story("      %-8s  %-18s  %-6s %-13s %-13s %s"
          % ("t+ms", "seen on", "what", "from", "to", "it said"))
    story("      " + "-" * 118)
    for r in rows[:limit]:
        m = r["msg"]
        story("      %-8.1f  %-18s  %-6s %-13s %-13s %s"
              % ((r["when"] - t0) * 1000.0, r["where"], m["kind"],
                 m["src"], m["dst"], m["text"][:70]))
    if len(rows) > limit:
        story("      ... and %d more" % (len(rows) - limit))
    story("      (times are when each sniffer read the packet, not when it")
    story("       crossed the wire: rows from different interfaces can be a")
    story("       few ms out of order when the machine is busy)")
    story()
    return len(rows)

# the sending side, spoken by hand

SENDER_SCRIPT = '''import socket, sys, time
s = socket.create_connection((sys.argv[1], 25), 20)
f = s.makefile("rwb")
def expect(code):
    while True:
        line = f.readline().decode("ascii", "replace").rstrip()
        print("S: " + line)
        if len(line) > 3 and line[3] == "-":
            continue
        if not line.startswith(code):
            print("UNEXPECTED, wanted " + code)
            sys.exit(1)
        return
def say(what):
    print("C: " + what)
    f.write((what + "\\r\\n").encode()); f.flush()
expect("220")
say("EHLO userA.a.lab"); expect("250")
say("MAIL FROM:<" + sys.argv[2] + ">"); expect("250")
say("RCPT TO:<" + sys.argv[3] + ">"); expect("250")
say("DATA"); expect("354")
for line in ["Subject: a message that had to be routed",
             "From: " + sys.argv[2],
             "To: " + sys.argv[3],
             "",
             "This crossed two lans and needed an MX lookup to find its way.",
             "."]:
    print("C: " + line)
    f.write((line + "\\r\\n").encode())
f.flush()
expect("250")
say("QUIT"); expect("221")
s.close()
'''

def send_mail():
    """Speak smtp by hand rather than run a mail client.

    A client would work, but it would also decide for itself when to talk,
    what to say and whether to use a submission port. Doing it here means
    every line in the table below was put there on purpose, which is the
    difference between a demonstration and a recording.
    """
    put(UA, "/tmp/send.py", SENDER_SCRIPT)
    out = ssh(UA, "python3 -m py_compile /tmp/send.py 2>&1", check=False)
    if out.strip():
        raise RuntimeError("the sender script did not survive being written "
                           "into %s:\n%s" % (UA, out))
    return ssh(UA, "python3 /tmp/send.py 10.1.0.1 %s %s 2>&1"
                   % (SENDER, RCPT), check=False)

def watch(do_it, title, kinds=None, settle=3.0, warm=True):
    sink, lock = [], threading.Lock()
    taps = [Tap(n, e, lab, sink, lock) for n, e, lab in PATH_TAPS]
    for t in taps:
        if not t.ready():
            for x in taps:
                x.stop()
            return None
    try:
        if warm:
            time.sleep(1.5)
        with lock:
            sink.clear()
        out = do_it()
        time.sleep(settle)
    finally:
        for t in taps:
            t.stop()
    show_packets(sink, lock, title, kinds=kinds)
    return {"out": out, "rows": sorted(sink, key=lambda r: r["when"])}

LAYOUT = {
    UA: (-570, 0),
    LAN_A: (-380, 0),
    MA: (-190, 0),
    LAN_C: (0, 0),
    DNS: (190, -75),
    MB: (190, 75),
    LAN_B: (380, 0),
    UB: (570, 0),
}

ETHS = {
    (DNS, "eth0"): 248,
    (MA, "eth0"): 270,
    (MA, "eth1"): 90,
    (MB, "eth0"): 68,
    (MB, "eth1"): 292,
    (UA, "eth0"): 90,
    (UB, "eth0"): 270,
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
    story("rootfs %s: two mail sites, one name server, one message" % rootfs)
    story("""
    userA -- lanA -- mailA -- lanC -- mailB -- lanB -- userB
   10.1.0.10      10.1.0.1     |    10.0.0.2     10.2.0.10
                  10.0.0.1    dns   10.2.0.1
                            10.0.0.3

    a.lab is served by mailA, b.lab by mailB, and the only thing that
    knows which is which is an MX record on the name server.

    alice@a.lab is about to send to bob@b.lab. mailA has never heard of
    b.lab and cannot deliver anything until it has asked.
""")

    step("erasing the net, then placing the items")
    erase_net()
    place()

    step("creating the five containers")
    in_parallel(lambda n: create_cnt(n, rootfs), list(ALL), "add cnt")

    step("waiting for them to answer")
    in_parallel(wait_ready, list(ALL), "wait_ready")
    tools_present(MA, "postfix")
    tools_present(DNS, "named", "named-checkzone")
    tools_present(UA, "python3")

    step("cabling them")
    for node in ALL:
        for num, lan, _ in WIRING[node]:
            cli("plug", node, "eth%d" % num, lan)

    step("addressing them")
    in_parallel(address, list(ALL), "address")
    in_parallel(enable_forwarding, list(SERVERS), "enable_forwarding")
    routes()
    in_parallel(resolver, list(ALL), "resolver")

    step("starting the name server")
    dns_start()
    out = ssh(MA, "dig +short MX b.lab @%s" % DNS_IP, check=False).strip()
    story("  what an MX lookup for b.lab answers, asked by hand:")
    story("      %s" % (out or "(nothing -- the zone is wrong)"))
    if "mailB" not in out:
        sys.exit("the name server is not answering MX for b.lab")
    story()

    step("starting postfix on both sites")
    in_parallel(postfix_start, list(SERVERS), "postfix_start")

    step("acts 1 to 3: one message, from a person to a mailbox")
    story()
    got = watch(send_mail,
                "everything that crossed the wires while it was sent:",
                settle=6.0)
    if got is None:
        sys.exit("could not sniff the path")

    story("  the conversation userA had with its own server:")
    for line in (got["out"] or "").rstrip().split("\n")[:24]:
        story("      " + line)
    story()
    story("    Four things are in that table. The first is a surprise:")
    story()
    story("      0. before saying a word, mailA asks dns who is CONNECTING")
    story("         to it -- a reverse lookup of 10.1.0.10. Every mail")
    story("         server does this, and the answer ends up in the Received")
    story("         header of the message. There is no reverse zone here so")
    story("         it fails, and postfix carries on and writes unknown,")
    story("         which is exactly what it does on the real internet when")
    story("         a host has no PTR record.")
    story()
    story("      1. userA talks to mailA in plain words. EHLO, MAIL FROM,")
    story("         RCPT TO, DATA, and a full stop on a line of its own to")
    story("         end the message. Nothing is encoded; smtp has looked")
    story("         like this since 1982 and you could type it by hand.")
    story()
    story("      2. mailA then asks the name server where b.lab lives. That")
    story("         is TWO questions: MX gives it a host NAME with a")
    story("         preference number, and the name still has to be resolved")
    story("         to an address before anything can be connected to.")
    story()
    story("      3. mailA opens its own smtp conversation with mailB and")
    story("         says the same things again -- but now as a server")
    story("         talking to a server, which is the same protocol doing")
    story("         a different job.")
    story()

    step("act 4: where it ended up")
    story()
    delivered = None
    for _ in range(20):
        out = ssh(MB, "cat /var/mail/bob 2>/dev/null", check=False)
        if "had to be routed" in out:
            delivered = out
            break
        time.sleep(1)
    if delivered:
        story("  /var/mail/bob on mailB:")
        for line in delivered.rstrip().split("\n")[:20]:
            story("      " + line)
    else:
        story("  the message never arrived. What the queue says:")
        story(ssh(MB, "mailq 2>&1 | tail -n 10", check=False))
        story(ssh(MA, "mailq 2>&1 | tail -n 10", check=False))
        # Both sides, because the one that refused is the one that knows.
        for n in (MA, MB):
            story("  the last of %s's own log:" % n)
            story(ssh(n, "tail -n 12 /var/log/postfix.log 2>&1", check=False))
    story()

    if not delivered:
        sys.exit("\nthe message did not arrive,"
                 " see the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")

    story("""
the network is left up. Things to try:

  say it yourself. smtp is the one protocol you can speak with your
  fingers, and doing it once shows more than ten explanations:
    cloonix cli %s ssh userA "nc 10.1.0.1 25"
      EHLO me
      MAIL FROM:<alice@a.lab>
      RCPT TO:<bob@b.lab>
      DATA
      hello
      .
      QUIT

  break the routing and watch mail queue rather than fail. Delete the MX
  record and mailA has nowhere to send:
    cloonix cli %s ssh dns "sed -i /MX/d /var/cache/bind/lab.zone; rndc reload"
    cloonix cli %s ssh mailA "mailq"

  watch a delivery live, decoded, at the point where two servers meet:
    cloonix cli %s sniff mailA eth1 --dump

  and read what postfix thought it was doing at every step. Not
  /var/log/mail.log: there is no syslog daemon here handling the mail
  facility, so postfix was told to write its own log instead:
    cloonix cli %s ssh mailA "cat /var/log/postfix.log"
    cloonix cli %s ssh mailB "cat /var/log/postfix.log"
""" % (NET, NET, NET, NET, NET, NET))

main()
