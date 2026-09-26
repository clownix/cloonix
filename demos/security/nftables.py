#!/usr/bin/env python3
r"""nftables - state, translation, and counters that say which rule matched.

    ./nftables.py [net]             the net defaults to nemo

Four containers, two lans::

    in1  10.1.0.10 ----+
                       +---- lan_in ---- fw ---- lan_out ---- srv
    in2  10.1.0.11 ----+              10.1.0.1  10.0.0.1   10.0.0.10
                                        eth0       eth1

Two machines behind a firewall and one in front of it. The inside is a
private range and the outside is not, so masquerade has something to do,
and there are two inside hosts so a port forward has to pick one.

  ACT 1  a stateful forward ruleset. Established traffic passes, new
         inbound does not, one outbound port is blocked, and the named
         counters say which rule each packet met.
  ACT 2  masquerade, with the same connection caught on both interfaces
         of one machine: a filter naming the real client finds it on eth0
         and never on eth1.
  ACT 3  a dnat port forward back in, and the conntrack entry that makes
         the return path work.
  ACT 4  that entry is deleted mid-connection and the connection dies.
         Stateful means there is state; this is where it lives.

Needs cloonix on the PATH and nothing else. Standard library only, like
cloonix itself, so it runs wherever the client does.

Image: nft_trixie
Size:  4 containers, 2 lans

THE PACKET TO CATCH
-------------------
  Every act here is the same packet seen on two interfaces of one
  machine, and the filter is identical on both. The firewall's decision
  is the difference between the two captures.

    cloonix cli nemo cnf fw eth0 sniff=on
    cloonix cli nemo cnf fw eth1 sniff=on

  Accepted and dropped, side by side:

    cloonix cli nemo sniff fw eth0 --dump tcp port 80
    cloonix cli nemo sniff fw eth1 --dump tcp port 80

  The permitted SYN appears on both. The one for a blocked port appears
  on eth0 and never on eth1, and nothing in the guest has to be trusted
  to say so.

  Masquerade is the packet to catch, because the address changes and can
  be shown to change:

    cloonix cli nemo sniff fw eth0 --dump host 10.1.0.10
    cloonix cli nemo sniff fw eth1 --dump host 10.1.0.10

  The second prints nothing: on the far side that connection is wearing
  10.0.0.1. The same conversation, and no filter naming the real client
  can find it -- which is what NAT means, put as an experiment.

    cloonix cli nemo sniff fw eth1 --dump host 10.0.0.1

  ACT 4 flushes the conntrack entry mid-connection. Watch the retransmits
  arrive on eth0 and stop appearing on eth1, then the RST come back.

Important
  iptables is deliberately NOT in the image; mixing backends is a long
  explanation with no payoff.
  The ruleset is per network namespace, so every container has its own and
  nothing leaks between them.
"""
import os
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
    NET, Tap, answers, check_net, create, erase_net, in_lst, in_parallel,
    place, put, run, ssh, step, story, wait_exists)

ROOTFS_CHOICES = ("nft_trixie",)

HISTORY = """\
Packet filtering in linux was rewritten three times by the same
community: ipfwadm, then ipchains by Rusty Russell in 1998, then the
netfilter framework and iptables, by Rusty Russell and Harald Welte, in
kernel 2.4 of 2000. nftables is the fourth: Patrick McHardy presented
it in 2008 as one virtual machine in the kernel replacing the separate
ip, ip6, arp and ethernet tables, it was merged in kernel 3.13 in
January 2014, and Pablo Neira Ayuso leads the netfilter project that
maintains it. Connection tracking, the state seen here, dates from the
iptables generation.

Today: nftables is the default packet filter of Debian, Ubuntu, Fedora and
RHEL; firewalld, docker and kubernetes rules end up in it, and the iptables
command of those systems is a translator to it.
"""

IN1, IN2, FW, SRV = "in1", "in2", "fw", "srv"
ALL = (IN1, IN2, FW, SRV)
LAN_IN, LAN_OUT = "lan_in", "lan_out"

WIRING = {
    IN1: [(0, LAN_IN, "10.1.0.10/24")],
    IN2: [(0, LAN_IN, "10.1.0.11/24")],
    FW:  [(0, LAN_IN, "10.1.0.1/24"), (1, LAN_OUT, "10.0.0.1/24")],
    SRV: [(0, LAN_OUT, "10.0.0.10/24")],
}
GATEWAY = {IN1: "10.1.0.1", IN2: "10.1.0.1", SRV: "10.0.0.1"}

INSIDE_NET = "10.1.0.0/24"
FW_OUT = "10.0.0.1"
SRV_IP = "10.0.0.10"
IN1_IP = "10.1.0.10"

PAGE_PORT = 8080        # the page srv serves, and the one in1 serves
ECHO_PORT = 9100        # the long-lived connection of act 4
BLOCKED_PORT = 9999     # a port the ruleset refuses to let out

def choose_rootfs():
    said = ""
    for cand in ROOTFS_CHOICES:
        r = run("cloonix cli %s add cnt probe_nft eth=1 %s" % (NET, cand))
        ok = (r.returncode == 0) and wait_exists("probe_nft", 20)
        run("cloonix --wait 0.3 cli %s del probe_nft" % NET)
        if ok:
            return cand
        said = (r.stdout + r.stderr).strip() or "nothing"
    sys.exit("the server would not start a container on %s.\n"
             "  it said: %s\n"
             "  fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_demo_bulk/"
             "nft_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_nft_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said))

def address(node):
    for num, _lan, addr in WIRING[node]:
        ssh(node, "ip addr add %s dev eth%d" % (addr, num))
        ssh(node, "ip link set dev eth%d up" % num)
    if node in GATEWAY:
        ssh(node, "ip route add default via %s" % GATEWAY[node])

def tools_present(node, *tools):
    for t in tools:
        r = run('cloonix cli %s ssh %s "command -v %s"' % (NET, node, t))
        if r.returncode:
            sys.exit("%s has no %s: this is not the nftables image.\n"
                     "  fetch it:\n"
                     "    cd /var/lib/cloonix/bulk\n"
                     "    wget https://clownix.net/downloads/cnt_demo_bulk/"
                     "nft_trixie.tar.gz\n"
                     "    (cloonix extracts it at the first use)\n"
                     "  or build it:\n"
                     "    cd vm_create/cnt_kvm_demo && ./cnt_nft_trixie.sh"
                     % (node, t))

# the ruleset: named counters, readable and resettable on their own.

RULESET = """
table inet fw {
  counter c_est { }
  counter c_new { }
  counter c_blocked { }
  counter c_dnat { }
  counter c_drop { }

  chain forward {
    type filter hook forward priority 0; policy drop;
    ct state established,related counter name c_est accept
    ip saddr %(inside)s tcp dport %(blocked)d counter name c_blocked drop
    ip saddr %(inside)s ct state new counter name c_new accept
    counter name c_drop drop
  }
}
""".replace("%(inside)s", INSIDE_NET).replace("%(blocked)d",
                                              str(BLOCKED_PORT))

NAT_MASQ = """
table ip nat {
  chain postrouting {
    type nat hook postrouting priority 100; policy accept;
    ip saddr %(inside)s counter masquerade
  }
}
""".replace("%(inside)s", INSIDE_NET)

NAT_DNAT = """
table ip nat {
  chain prerouting {
    type nat hook prerouting priority -100; policy accept;
    ip daddr %(fwout)s tcp dport %(port)d counter dnat to %(in1)s:%(port)d
  }
  chain postrouting {
    type nat hook postrouting priority 100; policy accept;
    ip saddr %(inside)s counter masquerade
  }
}
""".replace("%(inside)s", INSIDE_NET).replace("%(fwout)s", FW_OUT) \
   .replace("%(in1)s", IN1_IP).replace("%(port)d", str(PAGE_PORT))

def load_rules(text, name):
    put(FW, "/tmp/%s.nft" % name, text)
    out = ssh(FW, "nft -c -f /tmp/%s.nft 2>&1 || true" % name, check=False)
    if out.strip():
        raise RuntimeError("the ruleset %s did not check out:\n%s"
                           % (name, out))
    ssh(FW, "nft -f /tmp/%s.nft" % name)

def counters():
    """The named counters, as {name: packets}."""
    out = ssh(FW, "nft list counters table inet fw", check=False)
    got = {}
    current = None
    for line in out.split("\n"):
        line = line.strip()
        if line.startswith("counter "):
            current = line.split()[1]
        elif current and line.startswith("packets "):
            try:
                got[current] = int(line.split()[1])
            except (IndexError, ValueError):
                pass
            current = None
    return got

def reset_counters():
    ssh(FW, "nft reset counters table inet fw >/dev/null", check=False)

def show_counters(title):
    got = counters()
    story("    %s" % title)
    for name, what in (("c_est", "established or related, let through"),
                       ("c_new", "new, from the inside, let through"),
                       ("c_blocked", "to the blocked port, dropped"),
                       ("c_dnat", "new, port-forwarded in, let through"),
                       ("c_drop", "everything else, dropped by policy")):
        story("      %-11s %6d   %s" % (name, got.get(name, 0), what))
    story()
    return got

# something to talk to

PAGE_SERVER = '''import http.server, socketserver, sys
WHO = sys.argv[1]
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = ("served by " + WHO + ", you came from " +
                self.client_address[0] + "\\n").encode()
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

ECHO_SERVER = '''import socket, socketserver
class H(socketserver.StreamRequestHandler):
    def handle(self):
        while True:
            line = self.rfile.readline()
            if not line:
                return
            self.wfile.write(line)
            self.wfile.flush()
socketserver.TCPServer.allow_reuse_address = True
socketserver.TCPServer(("", PORT), H).serve_forever()
'''

ECHO_CLIENT = '''import socket, sys, time
s = socket.create_connection(("HOST", PORT), 5)
s.settimeout(5)
def turn(word):
    try:
        s.sendall((word + "\\n").encode())
        got = s.recv(100)
        return "got back " + got.decode().strip() if got else "connection closed"
    except Exception as err:
        return "FAILED: " + err.__class__.__name__
print("first  " + turn("one"))
time.sleep(GAP)
print("second " + turn("two"))
'''

def start_server(node, source, port, name):
    body = source.replace("PORT", str(port))
    put(node, "/tmp/%s.py" % name, body)
    out = ssh(node, "python3 -m py_compile /tmp/%s.py 2>&1" % name,
              check=False)
    if out.strip():
        raise RuntimeError("%s did not survive being written:\n%s"
                           % (name, out))
    ssh(node, "pkill -f /tmp/%s.py 2>/dev/null; true" % name, check=False)
    extra = " " + node if "argv" in source else ""
    ssh(node, "python3 /tmp/%s.py%s </dev/null >/tmp/%s.log 2>&1 &"
              % (name, extra, name))
    for _ in range(20):
        if ssh(node, "pgrep -f /tmp/%s.py >/dev/null && echo up" % name,
               check=False).strip():
            return
        time.sleep(0.5)
    raise RuntimeError("%s did not start on %s:\n%s"
                       % (name, node, ssh(node, "cat /tmp/%s.log" % name,
                                          check=False)))

def fetch(node, url, timeout=5):
    return ssh(node, "curl -sS -m %d %s 2>&1" % (timeout, url),
               check=False).strip()

def reaches(node, host, port, timeout=4):
    return run('cloonix cli %s ssh %s "nc -z -w %s %s %s"'
               % (NET, node, timeout, host, port)).returncode == 0

# the wire

def both_wires(filt, do_it, settle=2.0):
    """The same filter on eth0 and eth1 of the firewall, over one action.

    The whole demo is this pair. What the firewall did to a packet is the
    difference between two numbers that came off the wire, and nothing
    inside any guest has to be believed.
    """
    inside = Tap(FW, 0, filt)
    outside = Tap(FW, 1, filt)
    if not (inside.ready() and outside.ready()):
        inside.stop()
        outside.stop()
        return None
    time.sleep(1.0)
    inside.clear()
    outside.clear()
    out = do_it()
    time.sleep(settle)
    with inside.lock:
        a = inside.count
    with outside.lock:
        b = outside.count
    inside.stop()
    outside.stop()
    return {"out": out, "eth0": a, "eth1": b}

def report(got, filt, note=""):
    story("      filter %-26s eth0 (inside) %4d   eth1 (outside) %4d"
          % (filt, got["eth0"], got["eth1"]))
    if note:
        story("        %s" % note)

LAYOUT = {
    IN1: (-570, -90),
    IN2: (-570, 90),
    LAN_IN: (-290, 0),
    FW: (0, 0),
    LAN_OUT: (290, 0),
    SRV: (570, 0),
}

ETHS = {
    (IN1, "eth0"): 120,
    (IN2, "eth0"): 60,
    (FW, "eth0"): 270,
    (FW, "eth1"): 90,
    (SRV, "eth0"): 270,
}

CARD = r"""
    in1  10.1.0.10 ----+
                       +---- lan_in ---- fw ---- lan_out ---- srv
    in2  10.1.0.11 ----+              10.1.0.1  10.0.0.1   10.0.0.10
                                        eth0       eth1

  Two machines behind a firewall, one in front of it. The inside is a
  private range and the outside is not, so masquerade has something to do,
  and there are two inside hosts so a port forward has to pick one.
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

    step("creating the four containers")
    in_parallel(lambda n: create(n, rootfs), list(ALL), "add cnt")

    step("waiting for them to answer")
    for name in ALL:
        end = time.time() + 90
        while time.time() < end and not answers(name):
            time.sleep(2)
        if not answers(name):
            raise RuntimeError("%s is in the list and never answered ssh"
                               % name)
    tools_present(FW, "nft", "conntrack")
    tools_present(SRV, "python3")
    tools_present(IN1, "curl", "nc")

    step("cabling and addressing them")
    for node in ALL:
        for num, lan, _ in WIRING[node]:
            run("cloonix cli %s plug %s eth%d %s"
                % (NET, node, num, lan))
    in_parallel(address, list(ALL), "address")
    ssh(FW, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    # tcp_loose=0: a mid-stream packet must not create a conntrack entry (act 4).
    ssh(FW, "sysctl -w net.netfilter.nf_conntrack_tcp_loose=0 >/dev/null",
        check=False)

    step("starting a page on srv and an echo on both")
    start_server(SRV, PAGE_SERVER, PAGE_PORT, "page")
    start_server(SRV, ECHO_SERVER, ECHO_PORT, "echo")
    start_server(IN1, PAGE_SERVER, PAGE_PORT, "page")

    step("ACT 1: a stateful ruleset, and counters that say which rule met "
         "each packet")
    story()
    load_rules(RULESET, "filter")
    reset_counters()
    story("    the whole ruleset, which is short enough to read:")
    for line in RULESET.strip().split("\n"):
        story("      %s" % line)
    story()

    got = both_wires("tcp port %d" % PAGE_PORT,
                     lambda: fetch(IN1, "http://%s:%d/" % (SRV_IP,
                                                           PAGE_PORT)))
    if got is None:
        sys.exit("could not sniff the firewall")
    story("    in1 fetching a page from srv, an allowed direction:")
    story("      srv said: %s" % (got["out"] or "(nothing)"))
    report(got, "tcp port %d" % PAGE_PORT,
           "on both wires, because the firewall forwarded it")
    ok_allowed = "served by" in got["out"]
    story()

    blocked = both_wires("tcp port %d" % BLOCKED_PORT,
                         lambda: reaches(IN1, SRV_IP, BLOCKED_PORT))
    if blocked is None:
        sys.exit("could not sniff the firewall")
    story("    in1 to the one port the ruleset refuses:")
    report(blocked, "tcp port %d" % BLOCKED_PORT,
           "the SYN arrived on eth0 and was never seen on eth1")
    ok_blocked = (blocked["eth0"] > 0 and blocked["eth1"] == 0)
    story()

    story("    srv opening a NEW connection inward, which nothing allows:")
    ok_inbound = not reaches(SRV, IN1_IP, PAGE_PORT)
    story("      srv -> in1:%d   %s"
          % (PAGE_PORT, "refused" if ok_inbound else "GOT THROUGH"))
    story("      The inside reaches out and the outside does not reach in.")
    story("      That asymmetry is the whole of what a stateful firewall")
    story("      is for, and it is three lines of ruleset.")
    story()
    show_counters("the counters, which is how you find out WHICH rule:")

    step("ACT 2: masquerade, and a filter that can no longer find the "
         "client")
    story()
    load_rules(NAT_MASQ, "masq")
    story("    one rule added:  ip saddr %s counter masquerade" % INSIDE_NET)
    story()
    said = fetch(IN1, "http://%s:%d/" % (SRV_IP, PAGE_PORT))
    story("    in1 fetches the same page again, and srv says:")
    story("      %s" % (said or "(nothing)"))
    ok_masq = FW_OUT in said
    story()
    got = both_wires("host %s" % IN1_IP,
                     lambda: fetch(IN1, "http://%s:%d/" % (SRV_IP,
                                                           PAGE_PORT)))
    if got is None:
        sys.exit("could not sniff the firewall")
    report(got, "host %s" % IN1_IP,
           "the same conversation, and the far side does not carry that"
           " address at all")
    story()
    got2 = both_wires("host %s" % FW_OUT,
                      lambda: fetch(IN1, "http://%s:%d/" % (SRV_IP,
                                                            PAGE_PORT)))
    if got2 is None:
        sys.exit("could not sniff the firewall")
    report(got2, "host %s" % FW_OUT,
           "and this is what it is wearing instead")
    ok_hidden = (got["eth0"] > 0 and got["eth1"] == 0)
    story()
    story("    Two subscriptions, one wire each, one filter apiece, and no")
    story("    guest was asked anything. NAT put as an experiment: the")
    story("    address the client thinks it has cannot be found on the far")
    story("    side of the machine that changed it.")
    story()

    step("ACT 3: a port forward back in, and the state behind it")
    story()
    load_rules(NAT_DNAT, "dnat")
    story("    added:  ip daddr %s tcp dport %d dnat to %s:%d"
          % (FW_OUT, PAGE_PORT, IN1_IP, PAGE_PORT))
    # The nat table rewrites the address; the forward chain still sees a
    # NEW connection from the outside and its policy is drop. The dnat
    # is let in by what conntrack marked, not by its port.
    ssh(FW, "nft insert rule inet fw forward index 1 "
            "ct status dnat counter name c_dnat accept")
    story("    added:  ct status dnat accept, in the forward chain, before")
    story("            the drop -- the filter sees the new connection from")
    story("            the outside and would drop it whatever the nat did")
    story()
    said = fetch(SRV, "http://%s:%d/" % (FW_OUT, PAGE_PORT))
    story("    srv asks the FIREWALL for a page and gets one from in1:")
    story("      %s" % (said or "(nothing)"))
    ok_dnat = "served by in1" in said
    story()
    story("    what makes the reply find its way home, from the firewall:")
    table = ssh(FW, "conntrack -L -p tcp --dport %d 2>/dev/null" % PAGE_PORT,
                check=False).strip()
    for line in (table.split("\n") if table else ["(no entry)"])[:4]:
        story("      %s" % line.strip())
    story()
    story("    Two tuples in one entry: what the client sent, and what the")
    story("    reply must look like to be recognised as belonging to it.")
    story("    The dnat rule ran once, on the first packet. Every packet")
    story("    after it was translated by this table and never met a rule.")
    story()

    step("ACT 4: the entry is deleted mid-connection")
    story()
    body = (ECHO_CLIENT.replace("HOST", SRV_IP).replace("PORT",
                                                        str(ECHO_PORT))
            .replace("GAP", "8"))
    put(IN1, "/tmp/echo_client.py", body)
    out = ssh(IN1, "python3 -m py_compile /tmp/echo_client.py 2>&1",
              check=False)
    if out.strip():
        raise RuntimeError("the echo client did not survive being written:"
                           "\n%s" % out)

    flow = ["tcp", "port", str(ECHO_PORT)]
    inside = Tap(FW, 0, " ".join(flow))
    outside = Tap(FW, 1, " ".join(flow))
    if not (inside.ready() and outside.ready()):
        inside.stop()
        outside.stop()
        sys.exit("could not sniff the firewall")
    time.sleep(1.0)
    inside.clear()
    outside.clear()

    runner = {}

    def run_client():
        runner["out"] = ssh(IN1, "python3 /tmp/echo_client.py 2>&1",
                            check=False)
    thread = threading.Thread(target=run_client)
    thread.start()
    time.sleep(4)
    story("    deleting the conntrack entry for that connection, while it")
    story("    is open and idle:")
    killed = ssh(FW, "conntrack -D -p tcp --dport %d 2>&1 || true"
                     % ECHO_PORT, check=False).strip()
    for line in killed.split("\n")[:3]:
        story("      %s" % line.strip())
    thread.join(30)
    time.sleep(2)
    with inside.lock:
        a = inside.count
    with outside.lock:
        b = outside.count
    inside.stop()
    outside.stop()
    story()
    story("    what the client on in1 reported:")
    for line in (runner.get("out", "") or "(nothing)").strip().split("\n"):
        story("      %s" % line)
    story()
    story("      packets on eth0 (inside)  %4d" % a)
    story("      packets on eth1 (outside) %4d" % b)
    story()
    said_first = "first  got back one" in runner.get("out", "")
    story("    The ruleset did not change. No rule was added, nothing was")
    story("    blocked by name. The connection died because the thing that")
    story("    made it allowed was a row in a table, and the row is gone.")
    story("    Retransmissions keep arriving on eth0 and stop being")
    story("    forwarded to eth1 -- established no longer matches, so the")
    story("    drop policy at the end of the chain does.")
    story()
    story("    (nf_conntrack_tcp_loose was set to 0 at the start. With its")
    story("     default of 1 the kernel adopts a mid-stream packet into a")
    story("     new entry, and deleting one costs the connection nothing --")
    story("     worth knowing before trusting conntrack -D as a tool.)")
    story()
    show_counters("the counters again, at the end of the whole demo:")

    bad = []
    if not ok_allowed:
        bad.append("the allowed fetch never worked, so nothing after it"
                   " means anything")
    if not ok_blocked:
        bad.append("the blocked port was not blocked, or was never seen"
                   " arriving on eth0")
    if not ok_inbound:
        bad.append("a new inbound connection got through a policy of drop")
    if not ok_masq:
        bad.append("srv did not see the firewall's address, so masquerade"
                   " did nothing")
    if not ok_hidden:
        bad.append("the client address was still visible on the outside"
                   " wire after masquerade")
    if not ok_dnat:
        bad.append("the port forward did not reach in1")
    if not said_first:
        bad.append("the echo connection never worked even before the entry"
                   " was deleted, so act 4 shows nothing")
    if bad:
        sys.exit("\n" + "\n".join("  " + b for b in bad))

    story("""
the network is left up, with the full ruleset loaded. Things worth trying:

  the whole ruleset, as the kernel holds it:
    cloonix cli %s ssh fw "nft list ruleset"

  the counters, live, while you make traffic:
    cloonix cli %s ssh fw "nft list counters table inet fw"

  the state table, which is where the decisions actually live:
    cloonix cli %s ssh fw "conntrack -L"

  the same packet on both sides of the firewall at once, in two terminals:
    cloonix cli %s sniff fw eth0 --dump tcp port %d
    cloonix cli %s sniff fw eth1 --dump tcp port %d

  and add a rule of your own, then watch its counter move:
    cloonix cli %s ssh fw "nft add rule inet fw forward icmp type echo-request counter drop"
""" % (NET, NET, NET, NET, PAGE_PORT, NET, PAGE_PORT, NET))

if __name__ == "__main__":
    try:
        main()
    except SystemExit as stop:
        if stop.code:
            sys.stderr.write("%s\n" % stop.code)
            up = [n for n in ALL if in_lst(n)]
            if up:
                sys.stderr.write("Still running: %s\n" % ", ".join(up))
            raise SystemExit(1)
        raise
    except RuntimeError as err:
        sys.stderr.write("%s\n" % err)
        raise SystemExit(1)
