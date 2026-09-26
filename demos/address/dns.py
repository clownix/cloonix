#!/usr/bin/env python3
"""dns - two users, two resolvers, and a top server reachable only over ospf.

    ./dns.py [net]                 the net defaults to nemo

Five machines and four lans:

  user1 --lanA-- dns1 --lanC-- dnstop --lanD-- dns2 --lanB-- user2
 10.1.0.10    10.1.0.1        10.0.13.3       10.2.0.1     10.2.0.10
              10.0.13.1       10.0.23.3       10.0.23.2
                              lo 10.255.0.3

Each user resolves through the machine next to it and knows no other name
server. Neither resolver knows any answer: both forward the demo zone to the
top server, and the address they forward to is 10.255.0.3, a loopback that
exists on no lan. The only way to reach it is a route learned from ospf.

That is the point of putting bind and frr on one image. Routing is not
checked here by reading a table, it is checked by a name resolving: if the
adjacency between dns1 and dnstop is down, "user2.demo.lab" does not answer,
and the failure arrives as the thing a user would actually notice.

  ACT 1  ospf comes up on the three servers, and dns1 and dns2 learn a route
         to the top server's loopback.
  ACT 2  named starts everywhere. dnstop is authoritative for demo.lab; the
         two others are recursive resolvers that forward that zone to it.
  ACT 3  user1 asks its own resolver for a name on the far side, and gets
         user2's address. Then the same the other way.
  ACT 4  the query is caught on the wire arriving at the top server, so the
         forwarding is proved rather than assumed.

Needs cloonix on the PATH and nothing else. Standard library only, like
cloonix itself, so it runs wherever the client does.

Image: dns_trixie (bind9 and frr together)
Size:  5 containers, 4 lans

THE PACKET TO CATCH
-------------------
  Two protocols on one demo, and the capture that matters catches the
  moment one depends on the other.

    cloonix cli nemo cnf d1 eth1 sniff=on
    cloonix cli nemo sniff d1 eth1 --dump udp port 53 and host 10.0.13.3

  The packet to catch is the forwarded query on its way to dt, and the
  answer coming back. Then stop ospf on the middle and watch the same
  subscription: the query still leaves d1 and no answer ever returns.
  The name has not changed and neither has bind's configuration.

  The routing underneath it, on the same wire:

    cloonix cli nemo sniff d1 eth1 --dump ip and not udp and not tcp

  ospf is IP protocol 89, so this is how the filter grammar reaches it.
  The hellos stopping is what the dns failure is made of, and having
  both on one wire, seconds apart, is the demo.

Important
  The unbound and knot demos are the other half of the DNS story -- this one
  is forwarding and routing, those are validation and signing.
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

ROOTFS_CHOICES = ("dns_trixie",)

HISTORY = """\
Until 1983 every host on the ARPANET fetched one HOSTS.TXT file from
SRI. Paul Mockapetris designed the Domain Name System that year, RFC
882 and 883, rewritten as RFC 1034 and 1035 in 1987: a tree of names,
delegated zone by zone, and resolvers that ask, cache and forward. BIND,
the Berkeley Internet Name Domain, was written at the University of
California in 1984 by Douglas Terry, Mark Painter, David Riggle and
Songnian Zhou, maintained by Paul Vixie from 1988, and rewritten as BIND
9 in 2000 by the Internet Systems Consortium, which still publishes it.

Today: every connection on the internet starts with a DNS query. BIND serves a
large share of the authoritative zones and several root servers, and the zone
file format it introduced is the one every other server reads.
"""

PROBE_ETH = "eth=1"

U1, U2 = "user1", "user2"
D1, D2, DT = "dns1", "dns2", "dnstop"
SERVERS = (D1, D2, DT)
ALL = (U1, D1, DT, D2, U2)

LAN_A, LAN_B, LAN_C, LAN_D = "lanA", "lanB", "lanC", "lanD"

# node -> list of (eth index, lan, address/len)
WIRING = {
    U1: [(0, LAN_A, "10.1.0.10/24")],
    D1: [(0, LAN_A, "10.1.0.1/24"), (1, LAN_C, "10.0.13.1/24")],
    DT: [(0, LAN_C, "10.0.13.3/24"), (1, LAN_D, "10.0.23.3/24")],
    D2: [(0, LAN_B, "10.2.0.1/24"), (1, LAN_D, "10.0.23.2/24")],
    U2: [(0, LAN_B, "10.2.0.10/24")],
}

TOP_LOOPBACK = "10.255.0.3"

# Each user's only name server is the machine on its own lan.
RESOLVER = {U1: "10.1.0.1", U2: "10.2.0.1"}
# And its only route out, so a name that resolves can then be reached.
GATEWAY = {U1: "10.1.0.1", U2: "10.2.0.1"}

ZONE = "demo.lab"

RECORDS = [
    ("user1",  "10.1.0.10"),
    ("user2",  "10.2.0.10"),
    ("dns1",   "10.1.0.1"),
    ("dns2",   "10.2.0.1"),
    ("dnstop", TOP_LOOPBACK),
]

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

# cloonix plumbing

def choose_rootfs():
    up, why = net_answers()
    if not up:
        sys.exit("no answer from a cloonix net called '%s'.\n"
                 "  the client said: %s\n"
                 "  A demo takes the net name as its first argument and\n"
                 "  falls back to nemo, so this is what to check first:\n"
                 "    ./dns.py <net>" % (NET, why or "nothing at all"))
    said = ""
    for cand in ROOTFS_CHOICES:
        trace("probing the server with a throwaway cnt on %s", cand)
        r = cli("add", "cnt", "probe_rootfs", PROBE_ETH, cand, ack=240)
        trace_ssh("probe returned %d", r.returncode)
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
             "dns_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_dns_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said or "nothing"))

def tools_present(node, *tools):
    for t in tools:
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                            "command -v %s >/dev/null 2>&1" % t],
                           capture_output=True, stdin=subprocess.DEVNULL)
        if r.returncode:
            sys.exit("%s has no %s: this is not the dns image.\n"
                     "  fetch it:\n"
                     "    cd /var/lib/cloonix/bulk\n"
                     "    wget https://clownix.net/downloads/cnt_demo_bulk/"
                     "dns_trixie.tar.gz\n"
                     "    (cloonix extracts it at the first use)\n"
                     "  or build it:\n"
                     "    cd vm_create/cnt_kvm_demo && ./cnt_dns_trixie.sh"
                     % (node, t))

def address(node):
    for num, lan, addr in WIRING[node]:
        ssh(node, "ip addr add %s dev eth%d" % (addr, num))
        ssh(node, "ip link set dev eth%d up" % num)
    trace("%s addressed: %s", node,
          " ".join(a for _, _, a in WIRING[node]))

def name_resolution(node):
    """/etc/hosts for the machines, and the resolver for everything else.

    The hosts file carries only the machines themselves, so a demo command
    can say "dnstop" without depending on the very thing under test. Names
    in demo.lab are deliberately NOT in it: those must come from bind or the
    demo proves nothing.
    """
    hosts = "127.0.0.1 localhost"
    for name, ip in (("user1", "10.1.0.10"), ("user2", "10.2.0.10"),
                     ("dns1", "10.1.0.1"), ("dns2", "10.2.0.1"),
                     ("dnstop", TOP_LOOPBACK)):
        hosts += "\\n%s %s-host" % (ip, name)
    ssh(node, "printf '%s\\n' > /etc/hosts" % hosts)
    ssh(node, 'echo "127.0.1.1 $(hostname)" >> /etc/hosts')
    if node in RESOLVER:
        ssh(node, "printf 'nameserver %s\\n' > /etc/resolv.conf"
                  % RESOLVER[node])
        ssh(node, "ip route add default via %s" % GATEWAY[node])
        trace("%s resolves through %s", node, RESOLVER[node])
    else:
        ssh(node, "rm -f /etc/resolv.conf")

FRRINIT = "/usr/lib/frr/frrinit.sh"
VTYSH_CONF = "service integrated-vtysh-config"

DAEMON_OPTS = ('zebra_options="-A 127.0.0.1 -u root -g root"\\n'
               'ospfd_options="-A 127.0.0.1 -u root -g root"\\n'
               'mgmtd_options="-A 127.0.0.1 -u root -g root"\\n'
               'staticd_options="-A 127.0.0.1 -u root -g root"\\n')

VTY_GROUP_FIX = ("grep -q '^frrvty:.*root' /etc/group || "
                 "sed -i '/^frrvty:/ s/$/,root/' /etc/group; true")

def ospf_conf(node):
    """One area, every link in it, and the user lans passive.

    Passive matters: a user machine runs no ospf, and sending hellos at it
    would be noise on the very lan act 4 listens to. The network statement
    stays, so the lan is still advertised and the far user is reachable by
    address as well as by name.
    """
    rid = {D1: "1.1.1.1", D2: "2.2.2.2", DT: "3.3.3.3"}[node]
    out = ["hostname %s" % node,
           "!",
           "interface lo",
           "!",
           "router ospf",
           " ospf router-id %s" % rid]
    if node == D1:
        out += [" network 10.1.0.0/24 area 0",
                " network 10.0.13.0/24 area 0",
                " passive-interface eth0"]
    elif node == D2:
        out += [" network 10.2.0.0/24 area 0",
                " network 10.0.23.0/24 area 0",
                " passive-interface eth0"]
    else:
        out += [" network 10.0.13.0/24 area 0",
                " network 10.0.23.0/24 area 0",
                " network %s/32 area 0" % TOP_LOOPBACK]
    out += ["!", "line vty", "!"]
    return "\n".join(out)

def frr_start(node):
    ssh(node, "mkdir -p /etc/frr /var/log/frr /var/lib/frr /var/tmp/frr")
    ssh(node, "printf '%s\\n' > /etc/frr/vtysh.conf" % VTYSH_CONF)
    ssh(node, "printf '%s' > /etc/frr/daemons"
              % "zebra=yes\\nospfd=yes\\nvtysh_enable=yes\\n")
    ssh(node, "printf '%s' >> /etc/frr/daemons" % DAEMON_OPTS)
    ssh(node, "printf '%s\\n' > /etc/frr/frr.conf"
              % ospf_conf(node).replace("\n", "\\n"))
    ssh(node, VTY_GROUP_FIX)
    ssh(node, "chmod 640 /etc/frr/frr.conf /etc/frr/daemons /etc/frr/vtysh.conf")
    ssh(node, "rm -rf /run/frr; mkdir -p /run/frr; chmod 755 /run/frr")
    ssh(node, "%s start </dev/null >/dev/null 2>&1" % FRRINIT)
    ssh(node, "for i in $(seq 1 20); do pgrep -x ospfd >/dev/null && exit 0; "
              "sleep 1; done; echo 'ospfd did not start'; exit 1")
    trace("%s: ospfd running", node)

def enable_forwarding(node):
    """A router with routes still drops what is not addressed to it.

    ospf fills the table, and the table is only consulted for packets the
    machine is willing to forward. net.ipv4.ip_forward is 0 by default and
    neither frr nor the image turns it on, so dns1 knew perfectly well how to
    reach 10.2.0.0/24 and dropped the ping anyway.

    Nothing before this act noticed, because resolution never needed it: the
    forwarded query is dns1's OWN traffic, sourced by dns1 and addressed to
    dnstop, and a machine always sends its own. Only the ping crosses a
    router, which is why the demo passed everything else and failed there.

    It is a per-namespace setting, so each container has its own and setting
    it here changes nothing on the machine running cloonix.
    """
    ssh(node, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    got = ssh(node, "sysctl -n net.ipv4.ip_forward", check=False).strip()
    if got != "1":
        raise RuntimeError("%s would not enable ip_forward, it reads %r"
                           % (node, got))
    trace("%s forwards ipv4", node)

def loopback_on_top():
    """The address the whole demo forwards to, on dnstop and nowhere else."""
    ssh(DT, "ip addr add %s/32 dev lo" % TOP_LOOPBACK)
    trace("%s carries %s on its loopback", DT, TOP_LOOPBACK)

def wait_for_route(node, dest, timeout=60):
    """Wait until ospf has given this machine a route to dest.

    Polled rather than slept at: an adjacency plus an LSA flood is a second
    on an idle machine and rather more on a loaded one, and a fixed sleep
    would be either too short on a bad day or wasted on a good one.
    """
    end = time.time() + timeout
    while time.time() < end:
        out = ssh(node, "ip route get %s 2>/dev/null" % dest, check=False)
        if out.strip() and "via" in out:
            trace("%s reaches %s: %s", node, dest, out.split("\n")[0].strip())
            return True
        time.sleep(1)
    story("  FAIL %s never learned a route to %s" % (node, dest))
    story(ssh(node, "vtysh -c 'show ip ospf neighbor'", check=False))
    story(ssh(node, "ip route", check=False))
    return False

# ACT 2 -- bind

# named runs as root and says so.
NAMED = "/usr/sbin/named -u root -c /etc/bind/named.conf"

def zone_file():
    """The demo zone, as the top server holds it."""
    out = ["$TTL 60",
           "@ IN SOA dnstop." + ZONE + ". admin." + ZONE + ". (",
           "        1 60 60 600 60 )",
           "@ IN NS dnstop." + ZONE + "."]
    for name, ip in RECORDS:
        out.append("%s IN A %s" % (name, ip))
    return "\n".join(out) + "\n"

def named_conf_top():
    """Authoritative, and recursion off.

    A server that is authoritative for a zone and recursive for everything
    else hides the mistake this demo is meant to show: if the forwarders
    were misconfigured, a recursive top server would still answer and
    nobody would notice. Off, it answers for demo.lab and refuses the rest,
    so the arrangement has to be right.
    """
    return "\n".join([
        "options {",
        "    directory \"/var/cache/bind\";",
        "    listen-on { any; };",
        "    listen-on-v6 { none; };",
        "    recursion no;",
        "    dnssec-validation no;",
        "    querylog yes;",
        "};",
        "logging {",
        "    channel q { file \"/var/cache/bind/query.log\"; "
        "severity info; print-time yes; };",
        "    category queries { q; };",
        "};",
        "zone \"" + ZONE + "\" {",
        "    type master;",
        "    file \"/var/cache/bind/" + ZONE + ".zone\";",
        "};",
        "",
    ])

def named_conf_forwarder():
    """Recursive for the lan it serves, and forwards the demo zone upward.

    "forward only" is deliberate: without it bind would fall back to asking
    the root servers when the forwarder did not answer, and on a lab network
    with no internet that turns a routing failure into a thirty second
    timeout instead of an immediate SERVFAIL.
    """
    return "\n".join([
        "options {",
        "    directory \"/var/cache/bind\";",
        "    listen-on { any; };",
        "    listen-on-v6 { none; };",
        "    recursion yes;",
        "    allow-query { any; };",
        "    allow-recursion { any; };",
        "    dnssec-validation no;",
        "    querylog yes;",
        "};",
        "logging {",
        "    channel q { file \"/var/cache/bind/query.log\"; "
        "severity info; print-time yes; };",
        "    category queries { q; };",
        "};",
        "zone \"" + ZONE + "\" {",
        "    type forward;",
        "    forward only;",
        "    forwarders { " + TOP_LOOPBACK + "; };",
        "};",
        "",
    ])

def named_start(node):
    ssh(node, "mkdir -p /etc/bind /var/cache/bind")
    if node == DT:
        put(node, "/etc/bind/named.conf", named_conf_top())
        put(node, "/var/cache/bind/" + ZONE + ".zone", zone_file())
        out = ssh(node, "named-checkzone %s /var/cache/bind/%s.zone 2>&1 && echo CFG_OK"
                        % (ZONE, ZONE), check=False)
        if "CFG_OK" not in out:
            raise RuntimeError("the zone file is not valid:\n%s" % out)
        trace("%s: zone %s checks out", node, ZONE)
    else:
        put(node, "/etc/bind/named.conf", named_conf_forwarder())
    out = ssh(node, "named-checkconf /etc/bind/named.conf", check=False)
    if out.strip():
        raise RuntimeError("named.conf on %s is not valid:\n%s" % (node, out))
    ssh(node, "%s </dev/null >/var/log/named.out 2>&1 &" % NAMED)
    # up is the udp port 53 bound, not the process seen: named loads its
    # zones before it listens, and a query in between is refused
    for _ in range(40):
        if ssh(node, "pgrep -x named >/dev/null && "
                    "ss -lun | grep -q ':53 ' && echo up",
               check=False).strip():
            trace("%s: named running", node)
            return
        time.sleep(0.5)
    # A daemonized named reports to syslog and nothing to named.out: run it
    # once more in the foreground, where the reason goes to stderr.
    raise RuntimeError("named did not start on %s:\n%s"
                       % (node, ssh(node, "timeout 5 %s -g 2>&1 | tail -20"
                                    % NAMED, check=False)))

DNS_TYPES = {1: "A", 2: "NS", 5: "CNAME", 6: "SOA", 12: "PTR", 15: "MX",
             16: "TXT", 28: "AAAA", 33: "SRV", 41: "OPT", 255: "ANY"}

DNS_RCODES = {0: "NOERROR", 1: "FORMERR", 2: "SERVFAIL", 3: "NXDOMAIN",
              4: "NOTIMP", 5: "REFUSED"}

def dns_name(data, off):
    """A name out of a dns message, following compression pointers.

    Returns (name, offset just past the name AS IT APPEARED). A pointer ends
    the name where it stands, so the caller must carry on from the two bytes
    of the pointer and not from wherever it led.
    """
    labels = []
    jumped = False
    end = off
    guard = 0
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

def dns_decode(payload):
    """A dns message into something printable, or None if it is not one."""
    if len(payload) < 12:
        return None
    ident = int.from_bytes(payload[0:2], "big")
    flags = int.from_bytes(payload[2:4], "big")
    qd = int.from_bytes(payload[4:6], "big")
    an = int.from_bytes(payload[6:8], "big")
    ns = int.from_bytes(payload[8:10], "big")
    ar = int.from_bytes(payload[10:12], "big")
    msg = {"id": ident,
           "qr": (flags >> 15) & 1,
           "opcode": (flags >> 11) & 0xf,
           "aa": (flags >> 10) & 1,
           "tc": (flags >> 9) & 1,
           "rd": (flags >> 8) & 1,
           "ra": (flags >> 7) & 1,
           "rcode": flags & 0xf,
           "counts": (qd, an, ns, ar),
           "question": None,
           "answers": []}
    off = 12
    for _ in range(qd):
        name, off = dns_name(payload, off)
        if off + 4 > len(payload):
            return msg
        qtype = int.from_bytes(payload[off:off + 2], "big")
        off += 4
        if msg["question"] is None:
            msg["question"] = (name, DNS_TYPES.get(qtype, str(qtype)))
    for _ in range(an):
        name, off = dns_name(payload, off)
        if off + 10 > len(payload):
            break
        rtype = int.from_bytes(payload[off:off + 2], "big")
        ttl = int.from_bytes(payload[off + 4:off + 8], "big")
        rdlen = int.from_bytes(payload[off + 8:off + 10], "big")
        off += 10
        rdata = payload[off:off + rdlen]
        off += rdlen
        if rtype == 1 and rdlen == 4:
            shown = ".".join(str(b) for b in rdata)
        elif rtype in (2, 5, 12):
            shown, _ = dns_name(payload, off - rdlen)
        else:
            shown = rdata.hex()
        msg["answers"].append((name, DNS_TYPES.get(rtype, str(rtype)),
                               ttl, shown))
    return msg

def dns_summary(msg):
    """One line describing a decoded message, in the order a reader cares."""
    if msg["qr"] == 0:
        flags = "rd" if msg["rd"] else "--"
        q = msg["question"]
        return "QUERY    id=0x%04x %s   %s %s" % (
            msg["id"], flags, q[0] if q else "?", q[1] if q else "?")
    bits = []
    bits.append("aa" if msg["aa"] else "--")
    bits.append("ra" if msg["ra"] else "--")
    q = msg["question"]
    head = "RESPONSE id=0x%04x %s %s  %s %s" % (
        msg["id"], " ".join(bits), DNS_RCODES.get(msg["rcode"],
                                                  str(msg["rcode"])),
        q[0] if q else "?", q[1] if q else "?")
    if msg["answers"]:
        head += "  ->  " + ", ".join("%s %s %s (ttl %d)" % (n, t, v, ttl)
                                     for n, t, ttl, v in msg["answers"])
    else:
        head += "  ->  no answer section"
    return head

# The taps

class Tap:
    """One sniffed interface, draining into a list shared by all the taps.

    Every tap writes into the same list with the same clock, so what comes
    out at the end is one merged, time-ordered account of a packet's journey
    rather than four separate ones to be lined up by hand.
    """

    def __init__(self, node, num, label, sink, lock):
        self.node = node
        self.num = num
        self.label = label
        self.sink = sink
        self.lock = lock
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
            u = udp_ports(pkt)
            if not u:
                continue
            sport, dport, payload = u
            if 53 not in (sport, dport):
                continue
            msg = dns_decode(payload)
            if not msg:
                continue
            d = pkt["data"]
            with self.lock:
                self.seen += 1
                self.sink.append({
                    "when": pkt["when"],
                    "where": self.label,
                    "src": "%s:%d" % (".".join(str(b) for b in d[26:30]),
                                      sport),
                    "dst": "%s:%d" % (".".join(str(b) for b in d[30:34]),
                                      dport),
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
    (U1, 0, "user1  eth0  lanA"),
    (D1, 0, "dns1   eth0  lanA"),
    (D1, 1, "dns1   eth1  lanC"),
    (DT, 0, "dnstop eth0  lanC"),
]

def open_taps(spec):
    sink = []
    lock = threading.Lock()
    taps = [Tap(n, e, lab, sink, lock) for n, e, lab in spec]
    for t in taps:
        if not t.ready():
            for x in taps:
                x.stop()
            return None, None
    trace("sniffing %d interfaces", len(taps))
    return taps, sink

def show_ladder(sink, lock, title):
    """Print every dns message the taps caught, in the order they happened."""
    with lock:
        rows = sorted(sink, key=lambda r: r["when"])
    story("    %s" % title)
    if not rows:
        story("      (nothing crossed any of the sniffed interfaces)")
        return 0
    t0 = rows[0]["when"]
    story("      %-6s  %-18s  %-22s %-22s %s"
          % ("t+ms", "seen on", "from", "to", "dns message"))
    story("      " + "-" * 108)
    for r in rows:
        story("      %-6.1f  %-18s  %-22s %-22s %s"
              % ((r["when"] - t0) * 1000.0, r["where"], r["src"], r["dst"],
                 dns_summary(r["msg"])))
    story("      (times are when each sniffer read the packet, not when it")
    story("       crossed the wire: rows from different interfaces can be a")
    story("       few ms out of order when the machine is busy)")
    story()
    return len(rows)

def resolution_on_the_wire(name, wanted, flush):
    """Ask for a name from user1 and print the journey of the question.

    flush=True empties dns1's cache first, so the query has to travel the
    whole way. flush=False leaves it, and the difference between the two is
    the most instructive thing in this demo: the same command, the same
    answer, and a completely different set of packets.
    """
    sink = []
    lock = threading.Lock()
    taps = [Tap(n, e, lab, sink, lock) for n, e, lab in PATH_TAPS]
    for t in taps:
        if not t.ready():
            for x in taps:
                x.stop()
            return None
    try:
        if flush:
            trace("dns1 cache will be emptied, so the question has to travel")
        else:
            trace("dns1 cache left alone, so it may already know")
        # Warm every tap before measuring anything.
        warm_name = "warmup." + ZONE
        threading.Thread(target=lambda: dig_a(U1, warm_name, timeout=2),
                         daemon=True).start()
        end = time.time() + 8
        while time.time() < end:
            if all(t.seen for t in taps):
                break
            time.sleep(0.2)
        cold_taps = [t.label for t in taps if not t.seen]
        if cold_taps:
            trace("these taps never warmed: %s", ", ".join(cold_taps))
        else:
            trace("all %d taps are delivering", len(taps))
        time.sleep(0.5)
        with lock:
            sink.clear()

        if flush:
            # After the warm-up, not before: the warm-up put things in the
            # cache too.
            ssh(D1, "rndc flush 2>/dev/null || true", check=False)
            time.sleep(0.3)
        got = dig_a(U1, name)
        time.sleep(1.0)          # let the last response reach every tap
    finally:
        for t in taps:
            t.stop()
    with lock:
        rows = sorted(sink, key=lambda r: r["when"])
    ok = (got == wanted)
    story("  %s user1 asked for %-18s and got %s"
          % ("OK  " if ok else "FAIL", name, got or "(nothing)"))
    show_ladder(sink, lock, "what crossed the wires:")
    return {"ok": ok, "rows": rows}

def dig_a(node, name, server=None, timeout=5):
    """One A record, as the short answer, using the node's own resolver."""
    at = ("@" + server + " ") if server else ""
    cmd = "dig +time=%d +tries=1 +short %s%s A" % (timeout, at, name)
    return ssh(node, cmd, check=False).strip()

def expect_a(node, name, wanted):
    got = dig_a(node, name)
    if got == wanted:
        story("  OK   %-6s asked for %-20s and got %s" % (node, name, got))
        return True
    story("  FAIL %-6s asked for %-20s wanted %s, got %r"
          % (node, name, wanted, got))
    story(ssh(node, "dig +time=3 +tries=1 %s A" % name, check=False))
    return False

def udp_ports(pkt):
    """(sport, dport, payload) for an IPv4 UDP packet, else None."""
    d = pkt["data"]
    if len(d) < 34 or d[12:14] != b"\x08\x00" or d[23] != 17:
        return None
    ihl = (d[14] & 0x0f) * 4
    base = 14 + ihl
    if len(d) < base + 8:
        return None
    sport = int.from_bytes(d[base:base + 2], "big")
    dport = int.from_bytes(d[base + 2:base + 4], "big")
    return sport, dport, d[base + 8:]

def print_topology():
    story("""
    user1 ---- lanA ---- dns1 ---- lanC ---- dnstop ---- lanD ---- dns2 ---- lanB ---- user2
  10.1.0.10          10.1.0.1              10.0.13.3            10.0.23.2          10.2.0.10
                     10.0.13.1             10.0.23.3            10.2.0.1
                                        lo 10.255.0.3

  users     : one name server each, the machine on their own lan, and no other
  dns1/dns2 : recursive for their lan, forward %s to %s
  dnstop    : authoritative for %s, recursion refused
  ospf      : on the three servers only, area 0, user lans passive

  %s is on no lan. It lives on dnstop's loopback and is announced by ospf,
  so a name resolving is a route working.
""" % (ZONE, TOP_LOOPBACK, ZONE, TOP_LOOPBACK))

def show_server_state():
    story("  ospf adjacencies as dns1 sees them:")
    for line in ssh(D1, "vtysh -c 'show ip ospf neighbor'",
                    check=False).rstrip().split("\n"):
        story("      " + line)
    story()
    story("  the routes ospf gave dns1:")
    for line in ssh(D1, "vtysh -c 'show ip route ospf'",
                    check=False).rstrip().split("\n"):
        story("      " + line)
    story()
    story("  and how dns1 reaches the top server's loopback:")
    for line in ssh(D1, "ip route get %s" % TOP_LOOPBACK,
                    check=False).rstrip().split("\n"):
        story("      " + line)
    story()

def show_query_log():
    story("  what the top server logged, which is only what it was asked:")
    out = ssh(DT, "cat /var/cache/bind/query.log 2>/dev/null | tail -n 12",
              check=False).rstrip()
    for line in (out.split("\n") if out else ["(nothing)"]):
        story("      " + line)
    story()

LAYOUT = {
    U1: (-760, 0),
    LAN_A: (-570, 0),
    D1: (-380, 0),
    LAN_C: (-190, 0),
    DT: (0, 0),
    LAN_D: (190, 0),
    D2: (380, 0),
    LAN_B: (570, 0),
    U2: (760, 0),
}

ETHS = {
    (D1, "eth0"): 270,
    (D1, "eth1"): 90,
    (D2, "eth0"): 90,
    (D2, "eth1"): 270,
    (DT, "eth0"): 270,
    (DT, "eth1"): 90,
    (U1, "eth0"): 90,
    (U2, "eth0"): 270,
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
    story("rootfs %s: two users, two resolvers, one top server" % rootfs)
    print_topology()

    step("erasing the net, then placing the items")
    erase_net()
    place()

    step("creating the five containers")
    in_parallel(lambda n: create_cnt(n, rootfs), list(ALL), "add cnt")

    step("waiting for them to answer")
    in_parallel(wait_ready, list(ALL), "wait_ready")

    tools_present(DT, "named", "named-checkzone")
    tools_present(U1, "dig")

    step("cabling them")
    for node in ALL:
        for num, lan, _ in WIRING[node]:
            cli("plug", node, "eth%d" % num, lan)

    step("addressing them")
    in_parallel(address, list(ALL), "address")
    loopback_on_top()
    in_parallel(name_resolution, list(ALL), "name_resolution")

    step("letting the three servers forward")
    in_parallel(enable_forwarding, list(SERVERS), "enable_forwarding")

    step("starting ospf on the three servers")
    in_parallel(frr_start, list(SERVERS), "frr_start")

    step("waiting for the route to the top server's loopback")
    if not all(wait_for_route(n, TOP_LOOPBACK) for n in (D1, D2)):
        sys.exit("\nospf never converged, the rest cannot work")
    story()
    show_server_state()

    step("starting named everywhere")
    in_parallel(named_start, list(SERVERS), "named_start")

    step("act 1: a cold cache, so the question travels the whole way")
    story()
    cold = resolution_on_the_wire("user2." + ZONE, "10.2.0.10", flush=True)
    if cold is None:
        sys.exit("could not sniff the path")
    story("    Read that table downwards and the whole arrangement is in it:")
    story("      * user1 asks its own resolver, with rd set: please recurse")
    story("      * the SAME packet appears on dns1 eth0 -- one wire, two")
    story("        vantage points, the frame unchanged")
    story("      * dns1 does not answer. It makes a NEW query of its own, with")
    story("        a new id, out of eth1 towards %s" % TOP_LOOPBACK)
    story("      * dnstop answers it with aa set: authoritative, this is the")
    story("        server that holds the zone")
    story("      * dns1 then answers user1, ra set, aa clear: recursion was")
    story("        available, but the answer is not its own")
    story()

    step("act 2: the same question again, this time with a warm cache")
    story()
    warm = resolution_on_the_wire("user2." + ZONE, "10.2.0.10", flush=False)
    if warm is None:
        sys.exit("could not sniff the path")
    story("    Same command, same answer, and lanC is silent: %d messages"
          % len(warm["rows"]))
    story("    against %d a moment ago. The top server was never asked,"
          % len(cold["rows"]))
    story("    because dns1 already knew and the record has not expired.")
    story("    That is caching, and it is why the ttl in the answer matters:")
    story("    this zone uses 60 seconds, so a minute from now it travels")
    story("    the whole way again.")
    story()

    step("act 3: the other user, the other resolver, the same top server")
    story()
    ok_b = expect_a(U2, "user1." + ZONE, "10.1.0.10")
    ok_c = expect_a(U1, "dnstop." + ZONE, TOP_LOOPBACK)
    story()
    show_query_log()

    step("act 4: a name is enough to reach the far side")
    out = ssh(U1, "ping -c 5 -W 2 user2." + ZONE, check=False)
    ok_d = " 0% packet loss" in out
    for line in out.rstrip().split("\n"):
        story("      " + line)
    story()
    story("  %s user1 reached user2 by name, across two ospf hops"
          % ("OK  " if ok_d else "FAIL"))
    if not ok_d:
        story("\n  how far a packet from user1 gets, hop by hop:")
        for label, addr in (("dns1  lanA side", "10.1.0.1"),
                            ("dns1  lanC side", "10.0.13.1"),
                            ("dnstop lanC side", "10.0.13.3"),
                            ("dnstop loopback", TOP_LOOPBACK),
                            ("dns2  lanD side", "10.0.23.2"),
                            ("user2", "10.2.0.10")):
            r = ssh(U1, "ping -c 1 -W 1 %s >/dev/null 2>&1 && echo yes || echo no"
                        % addr, check=False).strip()
            story("      %-18s %-12s %s" % (label, addr, r))
        story("\n  and whether each router is willing to forward at all:")
        for n in SERVERS:
            v = ssh(n, "sysctl -n net.ipv4.ip_forward", check=False).strip()
            story("      %-8s net.ipv4.ip_forward = %s" % (n, v))

    if not (cold["ok"] and warm["ok"] and ok_b and ok_c and ok_d):
        sys.exit("\nsomething did not work,"
                 " see the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")

    story("""
the network is left up. Things worth trying, in rough order of how much
they teach:

  see the whole journey again, live and decoded, at any interface:
    cloonix cli %s sniff dns1 eth1 --dump

  ask the top server directly, and watch it refuse to recurse for anything
  it is not authoritative for:
    cloonix cli %s ssh %s "dig @%s www.example.com A"

  break the middle of the network and watch resolution fail, because it is
  one network and not two things that happen to work:
    cloonix cli %s ssh %s "ip link set eth1 down"
    cloonix cli %s ssh %s "dig +time=2 user2.%s A"

  then bring it back and watch ospf reconverge on its own:
    cloonix cli %s ssh %s "ip link set eth1 up"
""" % (NET, NET, U1, TOP_LOOPBACK, NET, D1, NET, U1, ZONE, NET, D1))

main()
