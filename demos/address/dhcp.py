#!/usr/bin/env python3
"""dhcp - one server, two lans, and the field that tells them apart.

    ./dhcp.py [net]                the net defaults to nemo

Five machines and three lans::

  clientA -- lanA -- relay1 --                               lanC -- kea
  clientB -- lanB -- relay2 --/

  lanA   10.1.0.0/24    relay1 eth0 is 10.1.0.1
  lanB   10.2.0.0/24    relay2 eth0 is 10.2.0.1
  lanC   10.0.0.0/24    relay1 .1, relay2 .2, kea .3

The two clients start with no address, and the one server is on neither
client lan. A dhcp client shouts to 255.255.255.255 and a broadcast does
not leave its wire: the relay picks it up, writes its own client-facing
address into the field giaddr, and sends it to the server as a unicast.
giaddr is then all the server knows of where the client is: kea holds two
subnets and chooses on that field, 10.1.0.1 meaning lanA.

  ACT 1  clientA asks, the exchange caught at the client lan, the backbone
         side of the relay and the server: one conversation before and
         after relaying.
  ACT 2  clientB asks from the other lan, and gets an address of the other
         subnet from the same server, on the strength of giaddr.
  ACT 3  clientA lets its lease expire and asks again with another client,
         busybox udhcpc in place of dhcpcd: the protocol is the same
         whoever speaks it.
  ACT 4  what the server has handed out, from its lease file.

Image: dhcp_trixie (kea, dnsmasq, dhcpcd, busybox)
Size:  5 containers, 3 lans

THE PACKET TO CATCH
-------------------
  DHCP is udp 67 and 68, and the same exchange differs on the two wires it
  crosses. On the client lan, broadcast, no address yet:

    cloonix cli nemo cnf relay1 eth0 sniff=on
    cloonix cli nemo sniff relay1 eth0 --dump udp port 67 or udp port 68

  On the server lan, unicast, relayed:

    cloonix cli nemo cnf relay1 eth1 sniff=on
    cloonix cli nemo sniff relay1 eth1 --dump udp port 67

  The packet to catch is the relayed DISCOVER on lanC: giaddr 10.1.0.1,
  the whole mechanism by which one server hands out two subnets; on relay2
  it says 10.2.0.1. Of DISCOVER, OFFER, REQUEST, ACK, the third is a
  broadcast the client sends while it holds an offer already.

Important
  Discovery is pure broadcast: the demo needs lans that are broadcast
  domains end to end.
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

ROOTFS_CHOICES = ("dhcp_trixie",)

HISTORY = """\
BOOTP, RFC 951 by Bill Croft and John Gilmore in 1985, let a diskless
machine ask the network who it was. DHCP, written by Ralph Droms as RFC
1531 in 1993 and settled in RFC 2131 in 1997, kept the packet format
and added leases and a pool: the four messages seen here, DISCOVER,
OFFER, REQUEST, ACK, and the giaddr field a relay fills in. Kea is the
Internet Systems Consortium server of 2014, the successor of the ISC
dhcpd that Ted Lemon had maintained since the 1990s. dnsmasq is the
work of Simon Kelley, published in 2001 for home routers; dhcpcd is by
Roy Marples, and udhcpc belongs to busybox.

Today: every home router, campus and cloud network hands out addresses with
it; a phone joining a wifi gets address, gateway and resolver in four packets,
and the relay shown here is what lets one server serve hundreds of lans.
"""

CA, CB, R1, R2, KEA = "clientA", "clientB", "relay1", "relay2", "kea"
RELAYS = (R1, R2)
ALL = (CA, R1, CB, R2, KEA)
LAN_A, LAN_B, LAN_C = "lanA", "lanB", "lanC"

# node -> list of (eth index, lan, address/len or None for "ask for one")
WIRING = {
    CA:  [(0, LAN_A, None)],
    R1:  [(0, LAN_A, "10.1.0.1/24"), (1, LAN_C, "10.0.0.1/24")],
    CB:  [(0, LAN_B, None)],
    R2:  [(0, LAN_B, "10.2.0.1/24"), (1, LAN_C, "10.0.0.2/24")],
    KEA: [(0, LAN_C, "10.0.0.3/24")],
}

SERVER_IP = "10.0.0.3"

# Which relay serves which client lan, and the address it will put in giaddr.
RELAY_OF = {CA: (R1, "10.1.0.1"), CB: (R2, "10.2.0.1")}

# The two pools kea holds, and the routes it needs to answer them.
SUBNETS = [
    {"id": 1, "cidr": "10.1.0.0/24", "pool": "10.1.0.100 - 10.1.0.150",
     "router": "10.1.0.1", "via": "10.0.0.1"},
    {"id": 2, "cidr": "10.2.0.0/24", "pool": "10.2.0.100 - 10.2.0.150",
     "router": "10.2.0.1", "via": "10.0.0.2"},
]

LEASE_SECONDS = 120

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
                 "  falls back to nemo:   ./dhcp.py <net>"
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
             "dhcp_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_dhcp_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said or "nothing"))

def tools_present(node, *tools):
    for t in tools:
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                            "command -v %s >/dev/null 2>&1" % t],
                           capture_output=True, stdin=subprocess.DEVNULL)
        if r.returncode:
            unavailable("%s has no %s: this is not the dhcp image.\n"
                     "  fetch it:\n"
                     "    cd /var/lib/cloonix/bulk\n"
                     "    wget https://clownix.net/downloads/cnt_demo_bulk/"
                     "dhcp_trixie.tar.gz\n"
                     "    (cloonix extracts it at the first use)\n"
                     "  or build it:\n"
                     "    cd vm_create/cnt_kvm_demo && ./cnt_dhcp_trixie.sh"
                     % (node, t))

def address(node):
    """Give the routers and the server their addresses. The clients get none.

    A client with an address configured would have nothing to ask for, so
    the interface is brought up and left bare. That is also what makes the
    first packet of act 1 possible: a machine with no address can still
    send, from 0.0.0.0 to 255.255.255.255, which is the only conversation
    ip allows before you have one.
    """
    for num, lan, addr in WIRING[node]:
        if addr:
            ssh(node, "ip addr add %s dev eth%d" % (addr, num))
        ssh(node, "ip link set dev eth%d up" % num)
    if any(a for _, _, a in WIRING[node]):
        trace("%s addressed: %s", node,
              " ".join(a for _, _, a in WIRING[node] if a))
    else:
        trace("%s is up with no address at all", node)

def enable_forwarding(node):
    """The relays route between the client lans and the backbone.

    Relaying dhcp does not need forwarding -- the relay originates its own
    packet -- but everything the client does afterwards does, and a client
    that gets an address and cannot use it is a poor demonstration.
    """
    ssh(node, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    got = ssh(node, "sysctl -n net.ipv4.ip_forward", check=False).strip()
    if got != "1":
        raise RuntimeError("%s would not enable ip_forward, it reads %r"
                           % (node, got))
    trace("%s forwards ipv4", node)

def routes_on_server():
    """kea has to be able to REACH the address it answers to.

    This is the detail that catches people building a relay for real. The
    server does not reply to the client -- it cannot, the client has no
    address yet -- it replies to giaddr, and giaddr is the relay's
    client-facing address on a lan the server is not on. Without a route
    back to 10.1.0.1 the offer is written and dropped, the client retries
    for ever, and everything looks fine at the server end.
    """
    for sub in SUBNETS:
        ssh(KEA, "ip route replace %s via %s" % (sub["cidr"], sub["via"]))
    trace("%s can reach both relay addresses, so its replies can get home",
          KEA)

def routes_on_relays():
    """Each relay needs to know where the other client lan is, so that a
    client on one can reach a client on the other once both are addressed."""
    ssh(R1, "ip route replace 10.2.0.0/24 via 10.0.0.2")
    ssh(R2, "ip route replace 10.1.0.0/24 via 10.0.0.1")

# kea and the relays

def kea_conf():
    """Two subnets, one server, and no interface on either of them.

    kea does not need to be on a subnet to serve it. It works out which one
    a request belongs to from giaddr and answers from the matching pool,
    which is exactly what this demo is for. The lifetime is deliberately
    short so a lease can be watched expiring inside a demo run.
    """
    subs = []
    for sub in SUBNETS:
        subs.append(
            '    {\n'
            '      "id": %d,\n'
            '      "subnet": "%s",\n'
            '      "pools": [ { "pool": "%s" } ],\n'
            '      "option-data": [\n'
            '        { "name": "routers", "data": "%s" },\n'
            '        { "name": "domain-name-servers", "data": "%s" }\n'
            '      ]\n'
            '    }' % (sub["id"], sub["cidr"], sub["pool"], sub["router"],
                       SERVER_IP))
    return (
        '{\n'
        '  "Dhcp4": {\n'
        '    "interfaces-config": { "interfaces": [ "eth0" ] },\n'
        '    "lease-database": { "type": "memfile", "persist": true,\n'
        '                        "name": "/var/lib/kea/leases4.csv" },\n'
        '    "valid-lifetime": %d,\n'
        '    "renew-timer": %d,\n'
        '    "rebind-timer": %d,\n'
        '    "loggers": [ { "name": "kea-dhcp4",\n'
        '                   "output_options": [ { "output":'
        ' "/var/log/kea/kea-dhcp4.log" } ],\n'
        '                   "severity": "INFO" } ],\n'
        '    "subnet4": [\n%s\n    ]\n'
        '  }\n'
        '}\n' % (LEASE_SECONDS, LEASE_SECONDS // 2, LEASE_SECONDS * 3 // 4,
                 ",\n".join(subs)))

def kea_start():
    ssh(KEA, "mkdir -p /etc/kea /var/lib/kea /var/log/kea /run/kea")
    put(KEA, "/etc/kea/kea-dhcp4.conf", kea_conf())
    out = ssh(KEA, "kea-dhcp4 -t /etc/kea/kea-dhcp4.conf 2>&1 && echo CFG_OK",
              check=False)
    if "CFG_OK" not in out:
        raise RuntimeError("kea will not accept its configuration:\n%s" % out)
    trace("the kea configuration checks out")
    ssh(KEA, "kea-dhcp4 -c /etc/kea/kea-dhcp4.conf "
             "</dev/null >/var/log/kea/out 2>&1 &")
    for _ in range(20):
        if ssh(KEA, "pgrep -x kea-dhcp4 >/dev/null && echo up",
               check=False).strip():
            trace("kea is running, serving %d subnets it is not on"
                  % len(SUBNETS))
            return
        time.sleep(0.5)
    raise RuntimeError("kea did not start:\n%s"
                       % ssh(KEA, "cat /var/log/kea/out", check=False))

def relay_start(node):
    """dnsmasq with its dns half switched off, doing nothing but relaying.

    --port=0 is what makes it a relay and not a name server: dnsmasq is a
    dns server that also does dhcp, and here only the second half is wanted.
    --dhcp-relay takes the address to put in giaddr and the server to send
    to, which is the whole configuration of a relay.
    """
    local = RELAY_OF[CA][1] if node == R1 else RELAY_OF[CB][1]
    ssh(node, "pkill -x dnsmasq 2>/dev/null; true", check=False)
    ssh(node, "dnsmasq --port=0 --dhcp-relay=%s,%s --interface=eth0 "
              "--bind-interfaces --log-dhcp --log-facility=/var/log/relay.log "
              "</dev/null >/dev/null 2>&1" % (local, SERVER_IP))
    for _ in range(20):
        if ssh(node, "pgrep -x dnsmasq >/dev/null && echo up",
               check=False).strip():
            trace("%s relays lan broadcasts to %s, writing %s into giaddr",
                  node, SERVER_IP, local)
            return
        time.sleep(0.5)
    raise RuntimeError("the relay did not start on %s:\n%s"
                       % (node, ssh(node, "cat /var/log/relay.log",
                                    check=False)))

DHCP_TYPES = {1: "DISCOVER", 2: "OFFER", 3: "REQUEST", 4: "DECLINE",
              5: "ACK", 6: "NAK", 7: "RELEASE", 8: "INFORM"}

DHCP_OPTS = {1: "subnet mask", 3: "router", 6: "dns", 12: "hostname",
             15: "domain", 28: "broadcast", 51: "lease time",
             53: "message type", 54: "server id", 55: "parameter list",
             58: "renew at", 59: "rebind at", 61: "client id",
             82: "relay agent info"}

def udp_ports(pkt):
    d = pkt["data"]
    if len(d) < 34 or d[12:14] != b"\x08\x00" or d[23] != 17:
        return None
    ihl = (d[14] & 0x0f) * 4
    base = 14 + ihl
    if len(d) < base + 8:
        return None
    return (int.from_bytes(d[base:base + 2], "big"),
            int.from_bytes(d[base + 2:base + 4], "big"),
            d[base + 8:],
            ".".join(str(b) for b in d[26:30]),
            ".".join(str(b) for b in d[30:34]))

def dhcp_decode(pkt):
    u = udp_ports(pkt)
    if not u:
        return None
    sport, dport, p, src, dst = u
    if 67 not in (sport, dport) and 68 not in (sport, dport):
        return None
    if len(p) < 240 or p[236:240] != b"\x63\x82\x53\x63":
        return None
    ip4 = lambda o: ".".join(str(b) for b in p[o:o + 4])
    msg = {"kind": "DHCP", "src": src, "dst": dst,
           "sport": sport, "dport": dport,
           "op": p[0], "xid": int.from_bytes(p[4:8], "big"),
           "flags": int.from_bytes(p[10:12], "big"),
           "ciaddr": ip4(12), "yiaddr": ip4(16), "siaddr": ip4(20),
           "giaddr": ip4(24),
           "chaddr": ":".join("%02x" % b for b in p[28:34]),
           "type": None, "opts": []}
    i = 240
    while i < len(p):
        code = p[i]
        if code == 255:
            break
        if code == 0:
            i += 1
            continue
        if i + 1 >= len(p):
            break
        ln = p[i + 1]
        val = p[i + 2:i + 2 + ln]
        i += 2 + ln
        if code == 53 and ln == 1:
            msg["type"] = DHCP_TYPES.get(val[0], str(val[0]))
        elif code in (1, 3, 6, 28, 54) and ln >= 4:
            msg["opts"].append((DHCP_OPTS.get(code, str(code)),
                                ".".join(str(b) for b in val[:4])))
        elif code in (51, 58, 59) and ln == 4:
            msg["opts"].append((DHCP_OPTS[code],
                                "%ds" % int.from_bytes(val, "big")))
        elif code == 12:
            msg["opts"].append(("hostname",
                                val.decode("ascii", "replace")))
        elif code == 55:
            msg["opts"].append(("asks for",
                                ",".join(str(b) for b in val[:8])
                                + ("..." if ln > 8 else "")))
    return msg

def describe(rec):
    m = rec["msg"]
    what = m["type"] or ("bootp request" if m["op"] == 1 else "bootp reply")
    bits = ["%-8s" % what,
            "xid=0x%08x" % m["xid"]]
    if m["giaddr"] != "0.0.0.0":
        bits.append("giaddr=%s <- the relay" % m["giaddr"])
    else:
        bits.append("giaddr=0.0.0.0")
    if m["yiaddr"] != "0.0.0.0":
        bits.append("yiaddr=%s" % m["yiaddr"])
    line = "  ".join(bits)
    if m["opts"]:
        line += "   [" + ", ".join("%s %s" % (k, v)
                                   for k, v in m["opts"][:4]) + "]"
    return line

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
            msg = dhcp_decode(pkt)
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

def taps_for(client):
    """The three places one exchange can be watched.

    The client lan, the relay's backbone side, and the server. The first and
    the last carry the SAME conversation in two completely different shapes,
    and the middle one is where it changes.
    """
    relay = RELAY_OF[client][0]
    lan = LAN_A if client == CA else LAN_B
    return [(client, 0, "%s eth0  %s" % (client, lan)),
            (relay, 1, "%s eth1  lanC" % relay),
            (KEA, 0, "kea    eth0  lanC")]

def watch(client, do_it, title, settle=2.0):
    """Sniff the three points, make something happen, print what crossed."""
    sink, lock = [], threading.Lock()
    taps = [Tap(n, e, lab, sink, lock) for n, e, lab in taps_for(client)]
    for t in taps:
        if not t.ready():
            for x in taps:
                x.stop()
            return None
    try:
        # sniff saying "waiting" means the server accepted the subscription,
        # not that packets are already arriving here.
        time.sleep(1.5)
        with lock:
            sink.clear()
        result = do_it()
        time.sleep(settle)
    finally:
        for t in taps:
            t.stop()
    show_packets(sink, lock, title)
    return {"rows": sorted(sink, key=lambda r: r["when"]), "result": result}

def show_packets(sink, lock, title, limit=40):
    with lock:
        rows = sorted(sink, key=lambda r: r["when"])
    story("    %s" % title)
    if not rows:
        story("      (no dhcp crossed the sniffed interfaces)")
        return 0
    t0 = rows[0]["when"]
    story("      %-8s  %-18s  %-19s %-19s %s"
          % ("t+ms", "seen on", "from", "to", "what it said"))
    story("      " + "-" * 124)
    for r in rows[:limit]:
        m = r["msg"]
        story("      %-8.2f  %-18s  %-19s %-19s %s"
              % ((r["when"] - t0) * 1000.0, r["where"],
                 "%s:%d" % (m["src"], m["sport"]),
                 "%s:%d" % (m["dst"], m["dport"]), describe(r)))
    if len(rows) > limit:
        story("      ... and %d more" % (len(rows) - limit))
    story("      (times are when each sniffer read the packet, not when it")
    story("       crossed the wire: rows from different interfaces can be a")
    story("       few ms out of order when the machine is busy)")
    story()
    return len(rows)

# the asking side

def dhcpcd_ask(node):
    """trixie's own client. -1 means give up after one attempt rather than
    settling into the background and retrying for ever."""
    ssh(node, "pkill -x dhcpcd 2>/dev/null; rm -f /run/dhcpcd/*.pid; true",
        check=False)
    ssh(node, "dhcpcd -4 -1 -t 15 eth0 </dev/null >/tmp/dhcpcd.log 2>&1",
        check=False)
    return client_address(node)

def udhcpc_ask(node):
    """busybox's client, which needs a script to do anything with what it
    is given. The script is three lines and writing it here keeps the image
    free of demo-specific files."""
    put(node, "/tmp/udhcpc.sh", """#!/bin/sh
case $1 in
bound|renew)
    ip addr flush dev $interface
    ip addr add $ip/$mask dev $interface
    [ -n "$router" ] && ip route replace default via $router
    ;;
esac
exit 0
""")
    ssh(node, "chmod 755 /tmp/udhcpc.sh")
    ssh(node, "pkill -f udhcpc 2>/dev/null; true", check=False)
    ssh(node, "busybox udhcpc -i eth0 -n -q -t 5 -T 2 -s /tmp/udhcpc.sh "
              "</dev/null >/tmp/udhcpc.log 2>&1", check=False)
    return client_address(node)

def client_address(node):
    out = ssh(node, "ip -4 -brief addr show eth0", check=False)
    for word in out.split():
        if "/" in word and word[0].isdigit():
            return word
    return None

def drop_address(node):
    ssh(node, "pkill -x dhcpcd 2>/dev/null; pkill -f udhcpc 2>/dev/null; true",
        check=False)
    ssh(node, "ip addr flush dev eth0", check=False)

LAYOUT = {
    CA: (-570, 0),
    LAN_A: (-380, 0),
    R1: (-190, 0),
    LAN_C: (0, 0),
    KEA: (190, -75),
    R2: (190, 75),
    LAN_B: (380, 0),
    CB: (570, 0),
}

ETHS = {
    (CA, "eth0"): 90,
    (CB, "eth0"): 270,
    (KEA, "eth0"): 248,
    (R1, "eth0"): 270,
    (R1, "eth1"): 90,
    (R2, "eth0"): 68,
    (R2, "eth1"): 292,
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
    story("rootfs %s: one dhcp server, two relays, two bare clients" % rootfs)
    story("""
    clientA -- lanA -- relay1 --\\
                                 lanC -- kea
    clientB -- lanB -- relay2 --/

    lanA 10.1.0.0/24   relay1 eth0 10.1.0.1     pool .100-.150
    lanB 10.2.0.0/24   relay2 eth0 10.2.0.1     pool .100-.150
    lanC 10.0.0.0/24   relay1 .1  relay2 .2  kea .3

    The clients start with no address. The server is on neither of their
    lans, and a broadcast does not leave its own wire, so the relay is the
    only reason the server ever hears them. It writes its own address into
    giaddr, and that one field is how the server knows which pool to use.
""")

    step("erasing the net, then placing the items")
    erase_net()
    place()

    step("creating the five containers")
    in_parallel(lambda n: create_cnt(n, rootfs), list(ALL), "add cnt")

    step("waiting for them to answer")
    in_parallel(wait_ready, list(ALL), "wait_ready")
    tools_present(KEA, "kea-dhcp4")
    tools_present(R1, "dnsmasq")
    tools_present(CA, "dhcpcd", "busybox")

    step("cabling them")
    for node in ALL:
        for num, lan, _ in WIRING[node]:
            cli("plug", node, "eth%d" % num, lan)

    step("addressing them, except the clients")
    in_parallel(address, list(ALL), "address")
    in_parallel(enable_forwarding, list(RELAYS), "enable_forwarding")
    routes_on_server()
    routes_on_relays()

    step("starting the server and the two relays")
    kea_start()
    in_parallel(relay_start, list(RELAYS), "relay_start")

    step("act 1: clientA asks, watched at three points on its way")
    story()
    a = watch(CA, lambda: dhcpcd_ask(CA),
              "one address being handed out, seen three times:")
    if a is None:
        sys.exit("could not sniff the path")
    got_a = a["result"]
    story("  clientA now has %s" % (got_a or "nothing"))
    story()
    story("    Read it downwards and the relay is the whole story:")
    story("      * the DISCOVER leaves clientA from 0.0.0.0 to")
    story("        255.255.255.255. It has no address, so those are the only")
    story("        two it can use. giaddr is empty.")
    story("      * the same packet arrives at kea from the relay, as an")
    story("        ordinary unicast, with giaddr filled in. That is the")
    story("        relay's client-facing address, and it is now the only")
    story("        clue kea has about where this client is.")
    story("      * the OFFER goes back to giaddr, not to the client -- the")
    story("        client still has no address to send to. The relay is what")
    story("        turns it back into a broadcast on lanA.")
    story("      * then REQUEST and ACK do the same trip, because the client")
    story("        has to say yes to one offer out of however many arrived.")
    story()

    step("act 2: clientB asks, and one field sends it to the other pool")
    story()
    b = watch(CB, lambda: dhcpcd_ask(CB),
              "the same server, the same config, a different answer:")
    if b is None:
        sys.exit("could not sniff the path")
    got_b = b["result"]
    story("  clientB now has %s" % (got_b or "nothing"))
    story()
    story("    Nothing about kea changed between these two acts. It has one")
    story("    configuration with two subnets in it, and the only thing that")
    story("    differed was giaddr: %s in act 1, %s here. Move a client to"
          % (RELAY_OF[CA][1], RELAY_OF[CB][1]))
    story("    the other lan and it gets the other network without anybody")
    story("    reconfiguring anything.")
    story()

    step("act 3: the same exchange, spoken by a different client")
    story()
    drop_address(CA)
    c = watch(CA, lambda: udhcpc_ask(CA),
              "busybox udhcpc instead of dhcpcd:")
    got_c = c["result"] if c else None
    story("  clientA has %s again" % (got_c or "nothing"))
    story()
    story("    Different program, different options asked for, and the same")
    story("    four packets in the same order. The parameter list is where")
    story("    they differ most: dhcpcd asks for a long list of things it")
    story("    may never use, udhcpc asks for very little.")
    story()
    if got_c and got_a and got_c != got_a:
        story("    And notice the address CHANGED, from %s to %s."
              % (got_a.split("/")[0], got_c.split("/")[0]))
        story()
        story("    Same machine, same mac, same lan, and a different lease.")
        story("    A dhcp server does not identify a client by its mac if it")
        story("    is offered anything better, and option 61 is that better")
        story("    thing. dhcpcd sends a client identifier derived from a")
        story("    stable machine id, udhcpc sends the older form -- a type")
        story("    byte and the mac. Two different identifiers, so as far as")
        story("    kea is concerned two different clients, and it will not")
        story("    hand the second one a lease that belongs to the first.")
        story("    The lease file in the next act shows both, side by side.")
    story()

    step("act 4: what the server thinks it has given out")
    story()
    story("  kea's lease file:")
    out = ssh(KEA, "cat /var/lib/kea/leases4.csv 2>/dev/null", check=False)
    for line in (out.rstrip().split("\n") if out.strip() else ["(empty)"]):
        story("      " + line)
    story()
    story("    Three leases for two machines: the client_id column is why.")
    story("    Two of them are clientA, one asked for by dhcpcd and one by")
    story("    udhcpc, and the hwaddr column shows the same mac on both.")
    story()
    story("  and the clients, from their own point of view:")
    for n in (CA, CB):
        story("      %-8s %s" % (n, ssh(n, "ip -4 -brief addr show eth0",
                                        check=False).strip()))
    story()

    ok = bool(got_a and got_b and got_c)
    if not ok:
        sys.exit("\nsomething did not work,"
                 " see the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")

    story("""
the network is left up. Things to try, in rough order of how much
they teach:

  watch a renewal, which is two packets rather than four -- the client
  already knows who to ask, so it goes straight to REQUEST:
    cloonix cli %s sniff kea eth0 --dump
    cloonix cli %s ssh clientA "dhcpcd -4 -n eth0"

  break the return path and watch it fail in a way that looks like the
  client's fault. kea answers giaddr, so without this route the offer is
  written and dropped, and clientA retries for ever while the server log
  says it answered:
    cloonix cli %s ssh kea "ip route del 10.1.0.0/24"
    cloonix cli %s ssh clientA "dhcpcd -4 -1 -t 10 eth0"

  move a client to the other lan and watch it get a different network from
  the same server, with nothing reconfigured anywhere:
    cloonix cli %s unplug clientA eth0 lanA
    cloonix cli %s plug clientA eth0 lanB

  and read the server's own account of every decision:
    cloonix cli %s ssh kea "cat /var/log/kea/kea-dhcp4.log"
""" % (NET, NET, NET, NET, NET, NET, NET))

main()
