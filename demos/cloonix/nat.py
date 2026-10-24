#!/usr/bin/env python3
r"""nat - the one way out of an isolated network.

    ./nat.py [net]                  the net defaults to nemo

Three containers, two lans, one nat::

    inside1 ---- lanA ---- gw ---- lanB ---- [nat] ---- the real internet
   10.1.0.10            10.1.0.1        10.117.0.10
                                        10.117.0.1 = passt

A cloonix network is isolated and the nat is the way out. One nat serves
one guest, so gw is the only machine passt addresses, and inside1 gets out
only by being routed through gw.

  ACT 1  add the nat, cable it, and watch the guest get an address from a
         dhcp server that is not in the topology. The demo carries its own
         dhcp client, so the offer can be printed field by field.
  ACT 2  reach outside: a name resolved through the host resolver, a fetch,
         and a ping -- the three things passt carries. Skipped with a word
         if this machine has no internet.
  ACT 3  the loopback mapping. The demo starts a server on 127.0.0.1 of the
         HOST and fetches it from inside the guest at 10.117.0.1. That is
         what makes a local debian mirror usable from a container.
  ACT 4  the second guest, routed and masqueraded through the first: what a
         single-guest nat does and does not give a network behind it.
  ACT 5  a published port, which is docker's -p. A server in the guest is
         knocked on from the HOST -- the one direction a nat refuses until
         it is asked -- then cnf publishes it, the same knock is answered,
         and publish=none takes it away again.


Image: nft_trixie
Size:  3 containers, 2 lans, 1 nat

THE PACKET TO CATCH
-------------------
  The translation, seen on both sides of it:

    cloonix cli nemo cnf gw eth1 sniff=on
    cloonix cli nemo sniff gw eth1 --dump udp port 67

  The dhcp exchange with no dhcp server anywhere in the topology is the
  first surprise. The second is on the outward interface: every address
  the guest ever writes is 10.117.0.x, and nothing outside ever sees it.

  ACT 5 is the other direction. With nothing
  published, a connection from the host to the guest never appears on
  this wire at all -- there is nothing to drop, because nothing was ever
  translated. With publish=8080:80 the same connection arrives:

    cloonix cli nemo sniff gw eth1 --dump tcp port 80

  Note the port. It leaves the host as 8080 and arrives as 80, and this
  subscription proves which of the two is on the wire.

Important
  nft_trixie, not the plain image: python3 for the dhcp client the demo
  writes into the guest, no image carrying one; curl for the fetches;
  nftables for the masquerade of ACT 4.
  ACT 3 needs no internet: its server runs on 127.0.0.1 of this machine.
  For several guests, several nats, or one guest routing the others, as
  here.
"""
import os
import shlex
import socket
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    NET, CLOONIX, answers, capturable, check_net, erase_net, parse_bytes_line,
    place, put, reached, run, show, ssh, step, story, wait_exists, unavailable,
    guarded)

# nft_trixie: python3 (the demo's own dhcp client), curl and nftables.
ROOTFS_CHOICES = ("nft_trixie",)

IN1, GW = "inside1", "gw"
ALL = (IN1, GW)
NAT = "nat1"
LAN_A, LAN_B = "lanA", "lanB"

IN1_ADDR = "10.1.0.10"
GW_INSIDE = "10.1.0.1"

PASST_GW = "10.117.0.1"
PASST_GUEST = "10.117.0.10"

OUTSIDE_NAME = "deb.debian.org"
OUTSIDE_ADDR = "9.9.9.9"
PING_NAME = "google.com"

def pause(what):
    """Stop until a key comes, where somebody can press one.

    A terminal is one place, the Demo panel of the canvas another: there
    every key typed in the log reaches this stdin as a line, and the panel
    says so with CLOONIX_DEMO_KEY=1. Run from a script, or by
    debug/demo_test.py, nobody is there and the demo goes on.
    """
    print()
    print("    %s" % what)
    if sys.stdin.isatty() or os.environ.get("CLOONIX_DEMO_KEY") == "1":
        print("    press Enter here to go on")
        sys.stdin.readline()
    print()

def choose_rootfs():
    said = ""
    for cand in ROOTFS_CHOICES:
        r = run("cloonix cli %s add cnt probe_nat eth=1 %s" % (NET, cand))
        ok = (r.returncode == 0) and wait_exists("probe_nat", 20)
        run("cloonix --wait 0.3 cli %s del probe_nat" % NET)
        if ok:
            return cand
        said = (r.stdout + r.stderr).strip() or "nothing"
    unavailable("the server would not start a container on %s.\n"
             "  it said: %s\n"
             "  fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_demo_bulk/"
             "nft_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_nft_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said))

def create(name, rootfs, eths):
    last = ""
    for _ in range(6):
        r = run("cloonix cli %s add cnt %s %s %s"
            % (NET, name, "eth=%d" % eths, rootfs))
        last = (r.stdout + r.stderr).strip()
        if r.returncode == 0 and wait_exists(name):
            return
        if r.returncode == 0:
            last = "the add was taken and %s never appeared" % name
        time.sleep(2)
    raise RuntimeError("%s was never created. The server said: %s"
                       % (name, last or "nothing at all"))

DHCP_CLIENT = '''import socket, struct, sys, random, time

IFACE = "eth1"
BCAST = b"\\xff\\xff\\xff\\xff\\xff\\xff"

def mac_bytes(name):
    f = open("/sys/class/net/" + name + "/address")
    txt = f.read().strip()
    f.close()
    return bytes(int(x, 16) for x in txt.split(":"))

def csum(data):
    if len(data) & 1:
        data = data + b"\\x00"
    total = 0
    for i in range(0, len(data), 2):
        total = total + ((data[i] << 8) | data[i + 1])
    while total >> 16:
        total = (total & 0xffff) + (total >> 16)
    return (~total) & 0xffff

def dhcp_body(mac, xid, kind, opts):
    b = struct.pack("!BBBBIHHIIII", 1, 1, 6, 0, xid, 0, 0x8000, 0, 0, 0, 0)
    b = b + mac + b"\\x00" * 10 + b"\\x00" * 192
    b = b + struct.pack("!I", 0x63825363)
    b = b + bytes([53, 1, kind])
    for code, val in opts:
        b = b + bytes([code, len(val)]) + val
    b = b + bytes([55, 5, 1, 3, 6, 15, 28]) + bytes([255])
    while len(b) < 300:
        b = b + b"\\x00"
    return b

def frame(mac, body):
    udp = struct.pack("!HHHH", 68, 67, 8 + len(body), 0) + body
    total = 20 + len(udp)
    ip = struct.pack("!BBHHHBBH", 0x45, 0, total, 0, 0, 64, 17, 0)
    ip = ip + b"\\x00\\x00\\x00\\x00" + b"\\xff\\xff\\xff\\xff"
    ip = ip[:10] + struct.pack("!H", csum(ip)) + ip[12:]
    return BCAST + mac + b"\\x08\\x00" + ip + udp

def options_of(pkt):
    out = {}
    i = pkt.index(b"\\x63\\x82\\x53\\x63") + 4
    while i < len(pkt):
        code = pkt[i]
        if code == 255:
            break
        if code == 0:
            i = i + 1
            continue
        size = pkt[i + 1]
        out[code] = pkt[i + 2:i + 2 + size]
        i = i + 2 + size
    return out

def dotted(raw):
    return ".".join(str(b) for b in raw)

def wait_for(sock, xid, want, deadline):
    while time.time() < deadline:
        sock.settimeout(max(0.2, deadline - time.time()))
        try:
            pkt = sock.recv(2048)
        except Exception:
            continue
        if len(pkt) < 280 or pkt[12:14] != b"\\x08\\x00":
            continue
        if pkt[23] != 17:
            continue
        base = 14 + (pkt[14] & 0x0f) * 4
        if struct.unpack("!H", pkt[base + 2:base + 4])[0] != 68:
            continue
        body = pkt[base + 8:]
        if struct.unpack("!I", body[4:8])[0] != xid:
            continue
        opts = options_of(body)
        if 53 not in opts or opts[53][0] != want:
            continue
        return body, opts
    return None, None

mac = mac_bytes(IFACE)
sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, 3)
sock.bind((IFACE, 3))
xid = random.randrange(1, 1 << 30)

sock.send(frame(mac, dhcp_body(mac, xid, 1, [])))
body, opts = wait_for(sock, xid, 2, time.time() + 10)
if not opts:
    print("NOOFFER")
    sys.exit(1)
offered = dotted(body[16:20])
server = dotted(opts.get(54, b"\\x00\\x00\\x00\\x00"))
print("OFFER address " + offered)
print("OFFER server " + server)
print("OFFER mask " + dotted(opts.get(1, b"")))
print("OFFER router " + dotted(opts.get(3, b"")))
print("OFFER dns " + dotted(opts.get(6, b"")))
if 51 in opts:
    print("OFFER lease " + str(struct.unpack("!I", opts[51])[0]))

ask = [(50, body[16:20])]
if 54 in opts:
    ask.append((54, opts[54]))
sock.send(frame(mac, dhcp_body(mac, xid, 3, ask)))
body, opts = wait_for(sock, xid, 5, time.time() + 10)
if not opts:
    print("NOACK")
    sys.exit(1)
print("ACK address " + dotted(body[16:20]))
print("ACK router " + dotted(opts.get(3, b"")))
print("ACK dns " + dotted(opts.get(6, b"")))
'''

def run_dhcp(node):
    put(node, "/tmp/dhcp.py", DHCP_CLIENT)
    out = ssh(node, "python3 -m py_compile /tmp/dhcp.py 2>&1", check=False)
    if out.strip():
        raise RuntimeError("the dhcp client did not survive being written:"
                           "\n%s" % out)
    return ssh(node, "python3 /tmp/dhcp.py 2>&1", check=False)

def lease_of(said):
    got = {}
    for line in said.split("\n"):
        f = line.split()
        if len(f) == 3 and f[0] in ("OFFER", "ACK"):
            got.setdefault(f[0].lower() + "_" + f[1], f[2])
    return got

# The wire

def dhcp_kind(data):
    """DISCOVER / OFFER / REQUEST / ACK, from a captured frame, or None."""
    if len(data) < 280 or data[12:14] != b"\x08\x00" or data[23] != 17:
        return None
    base = 14 + (data[14] & 0x0f) * 4
    ports = (int.from_bytes(data[base:base + 2], "big"),
             int.from_bytes(data[base + 2:base + 4], "big"))
    if 67 not in ports and 68 not in ports:
        return None
    body = data[base + 8:]
    magic = b"\x63\x82\x53\x63"
    if magic not in body:
        return None
    i = body.index(magic) + 4
    while i + 1 < len(body):
        code = body[i]
        if code == 255:
            break
        if code == 0:
            i += 1
            continue
        size = body[i + 1]
        if code == 53 and size == 1:
            return {1: "DISCOVER", 2: "OFFER", 3: "REQUEST",
                    5: "ACK", 6: "NAK"}.get(body[i + 2], "type %d"
                                            % body[i + 2])
        i += 2 + size
    return None

class Tap:
    def __init__(self, node, num, filt=""):
        self.live = False
        self.lock = threading.Lock()
        self.frames = []
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
                    if len(self.frames) < 40:
                        self.frames.append(pkt["data"])
        self.ready_evt.set()

    def ready(self, timeout=15):
        self.ready_evt.wait(timeout)
        return self.live

    def stop(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()

# ACT 3 -- something on the loopback of the host, to be fetched from inside

MARK = "THIS-CAME-FROM-THE-HOST-LOOPBACK"
GUEST_MARK = "THIS-CAME-FROM-INSIDE-THE-GUEST"

GUEST_PORT = 80

GUEST_SERVER = '''import http.server, socketserver
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b"THIS-CAME-FROM-INSIDE-THE-GUEST\\n"
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

def free_host_port():
    """A port nothing on this host is using, asked of the kernel."""
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port

def knock(port, timeout=4):
    """Open the published port from the HOST and say what came back."""
    try:
        with socket.create_connection(("127.0.0.1", port), timeout) as sk:
            sk.sendall(b"GET / HTTP/1.0\r\nHost: h\r\n\r\n")
            sk.settimeout(timeout)
            got = b""
            while len(got) < 4096:
                more = sk.recv(1024)
                if not more:
                    break
                got += more
        return got.decode("utf-8", "replace")
    except OSError as err:
        return "no connection: %s" % err.__class__.__name__

class Quiet(BaseHTTPRequestHandler):
    def do_GET(self):
        body = (MARK + "\n").encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *a):
        pass

def host_loopback_server():
    """A server on 127.0.0.1 of the machine running this demo.

    Bound to the loopback and nowhere else on purpose: if the guest can
    reach it, the only path is the mapping passt makes, because there is
    no other route to 127.0.0.1 of this machine from anywhere.
    """
    srv = HTTPServer(("127.0.0.1", 0), Quiet)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv, srv.server_address[1]

LAYOUT = {
    IN1: (-500, 0),
    LAN_A: (-250, 0),
    GW: (0, 0),
    LAN_B: (250, 0),
    NAT: (500, 0),
}

ETHS = {
    (IN1, "eth0"): 90,
    (GW, "eth0"): 270,
    (GW, "eth1"): 90,
}

CARD = r"""
    inside1 ---- lanA ---- gw ---- lanB ---- [nat] ---- the real internet
   10.1.0.10            10.1.0.1        10.117.0.10
                                        10.117.0.1 = passt

  A cloonix network is isolated; the nat is the way out. One nat serves
  one guest, so the guest on lanB is the only one addressed by passt, and
  inside1 reaches the outside only by routing through gw.
"""

def main():
    step("checking the server")
    check_net()

    step("looking for the image")
    rootfs = choose_rootfs()
    erase_net()
    place()
    story(CARD)
    story("  image: %s" % rootfs)

    step("creating the two containers")
    create(GW, rootfs, 2)
    create(IN1, rootfs, 1)
    run("cloonix cli %s plug %s eth0 %s" % (NET, GW, LAN_A))
    run("cloonix cli %s plug %s eth0 %s" % (NET, IN1, LAN_A))
    for name in ALL:
        end = time.time() + 90
        while time.time() < end and not answers(name):
            time.sleep(2)
        if not answers(name):
            raise RuntimeError("%s never answered ssh" % name)
    ssh(GW, "ip addr add %s/24 dev eth0" % GW_INSIDE)
    ssh(GW, "ip link set dev eth0 up")
    ssh(GW, "ip link set dev eth1 up")
    ssh(IN1, "ip addr add %s/24 dev eth0" % IN1_ADDR)
    ssh(IN1, "ip link set dev eth0 up")
    ssh(IN1, "ip route add default via %s" % GW_INSIDE)

    step("ACT 1: the nat, and an address from a server that is not here")
    story()
    r = run("cloonix cli %s add nat %s" % (NET, NAT))
    if r.returncode or not wait_exists(NAT, 20):
        sys.exit("the server would not add a nat: %s"
                 % ((r.stdout + r.stderr).strip() or "it said nothing"))
    run("cloonix cli %s plug %s %s" % (NET, NAT, LAN_B))
    run("cloonix cli %s plug %s eth1 %s" % (NET, GW, LAN_B))
    time.sleep(3)
    story("    the nat is on %s, and so is eth1 of %s." % (LAN_B, GW))
    story("    Nothing in this topology serves dhcp. There is no dhcp")
    story("    server container, and nothing was configured to answer.")
    story()

    tap = Tap(GW, 1, "udp port 67 or udp port 68")
    if not tap.ready():
        story("    (could not sniff %s eth1; the exchange will not be shown)"
              % GW)
    else:
        time.sleep(1.0)
    said = run_dhcp(GW)
    time.sleep(1.5)
    frames = []
    if tap.live:
        with tap.lock:
            frames = list(tap.frames)
        tap.stop()

    story("    the exchange, caught on the wire:")
    kinds = [dhcp_kind(f) for f in frames]
    kinds = [k for k in kinds if k]
    if kinds:
        for k in kinds[:8]:
            story("      %s" % k)
    else:
        story("      (nothing caught)")
    story()
    story("    what the client got:")
    for line in said.strip().split("\n"):
        story("      %s" % line)
    story()
    lease = lease_of(said)
    got_addr = lease.get("ack_address") or lease.get("offer_address", "")
    got_router = lease.get("ack_router") or lease.get("offer_router", "")
    got_dns = lease.get("ack_dns") or lease.get("offer_dns", "")
    ok_dhcp = bool(got_addr)
    if ok_dhcp:
        ssh(GW, "ip addr add %s/24 dev eth1" % got_addr, check=False)
        if got_router:
            ssh(GW, "ip route add default via %s" % got_router, check=False)
        if got_dns:
            ssh(GW, "printf 'nameserver %s\\n' > /etc/resolv.conf"
                    % got_dns, check=False)
        story("    %s is now %s, gateway %s, resolver %s."
              % (GW, got_addr, got_router or "?", got_dns or "?"))
        if got_addr != PASST_GUEST:
            story("    (the documented address is %s; this run got %s)"
                  % (PASST_GUEST, got_addr))
    else:
        story("    No offer came back. passt answers dhcp itself, so no")
        story("    offer means the nat is not talking to this lan --")
        story("    check that %s is cabled to %s." % (NAT, LAN_B))
    story()
    story("    passt is a process on the HOST, unprivileged, holding the")
    story("    real sockets. It answered that request itself. There is no")
    story("    dhcp server anywhere in the topology and there never was.")
    story()

    step("ACT 2: outside, if this machine has an outside")
    story()
    ping_out = ssh(GW, "ping -c 5 -W 3 %s >/dev/null 2>&1 && echo yes"
                       % OUTSIDE_ADDR, check=False).strip()
    ping_name = ssh(GW, "ping -c 5 -W 3 %s 2>&1 | tail -2" % PING_NAME,
                    check=False).strip()
    resolved = ssh(GW, "getent hosts %s 2>/dev/null | head -1"
                       % OUTSIDE_NAME, check=False).strip()
    fetched = ssh(GW, "curl -sS -m 8 -o /dev/null -w %%{http_code} "
                      "http://%s/ 2>&1" % OUTSIDE_NAME, check=False).strip()
    story("      icmp to %-16s %s" % (OUTSIDE_ADDR,
                                      "reaches" if ping_out else "no reply"))
    story("      ping %-19s %s" % (PING_NAME, ping_name.replace("\n", "\n" + " " * 31)
                                   or "nothing"))
    story("      name %-19s %s" % (OUTSIDE_NAME, resolved or "not resolved"))
    story("      http %-19s %s" % (OUTSIDE_NAME, fetched or "nothing"))
    story()
    if ping_out or resolved or reached(ping_name):
        story("    Outbound tcp, udp and icmp, v4 and v6. There is no")
        story("    inbound: the nat is egress only, and nothing outside can")
        story("    open a connection to anything in here.")
    else:
        story("    This machine has no route to the internet, so this act")
        story("    proves nothing about cloonix. The next one does not need")
        story("    one and is the more interesting half anyway.")
    story()

    step("ACT 3: the loopback of the host, from inside the guest")
    story()
    srv, port = host_loopback_server()
    story("    this demo just started a web server on 127.0.0.1:%d of the"
          % port)
    story("    machine it is running on. Bound to the loopback and nowhere")
    story("    else, so nothing on any network can reach it.")
    story()
    said = ssh(GW, "curl -sS -m 8 http://%s:%d/ 2>&1" % (PASST_GW, port),
               check=False).strip()
    story("    the guest asked %s:%d and got:" % (PASST_GW, port))
    story("      %s" % (said or "(nothing)"))
    ok_loop = MARK in said
    story()
    if ok_loop:
        story("    The gateway address of the nat is mapped onto the")
        story("    loopback of the host. That single mapping is why a debian")
        story("    mirror on 127.0.0.1 can be the apt source of every")
        story("    container, and it is how the image builders in")
        story("    vm_create work with no packages from the internet.")
    else:
        story("    That did not come back, so the mapping did not work on")
        story("    this run. It is what vm_create/*/doitall.sh depends on,")
        story("    so find out why before building images.")
    srv.shutdown()
    story()

    step("ACT 4: the second guest, behind the first")
    story()
    before = ssh(IN1, "ping -c 1 -W 3 %s >/dev/null 2>&1 && echo yes"
                      % PASST_GW, check=False).strip()
    story("      inside1 -> %s, with gw forwarding nothing: %s"
          % (PASST_GW, "reaches" if before else "no reply"))
    ssh(GW, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    routed = ssh(IN1, "ping -c 1 -W 3 %s >/dev/null 2>&1 && echo yes"
                      % PASST_GW, check=False).strip()
    story("      with ip_forward on but no translation:            %s"
          % ("reaches" if routed else "no reply"))
    story()
    story("    passt knows one address, the one it handed out. A packet")
    story("    from 10.1.0.10 arrives wearing a source it has never seen")
    story("    and is not going anywhere.")
    story()
    ssh(GW, "printf 'table ip nat {\\n chain post {\\n  type nat hook "
            "postrouting priority 100;\\n  ip saddr 10.1.0.0/24 masquerade\\n"
            " }\\n}\\n' > /tmp/nat.nft", check=False)
    ssh(GW, "nft -f /tmp/nat.nft", check=False)
    after = ssh(IN1, "ping -c 5 -W 3 %s >/dev/null 2>&1 && echo yes"
                     % PASST_GW, check=False).strip()
    story("      with masquerade on gw as well:                    %s"
          % ("reaches" if after else "no reply"))
    said = ssh(IN1, "curl -sS -m 8 http://%s:%d/ 2>&1" % (PASST_GW, port),
               check=False).strip()
    story()
    story("    and inside1, two hops from the host, reaching its loopback:")
    story("      %s" % (said or "(nothing, and the server is stopped now)"))
    story()
    story("    So one nat serves one guest, and a network behind that guest")
    story("    needs the guest to translate for it -- which is a second")
    story("    nat, in software this demo installed, doing the same job as")
    story("    the one cloonix provides. For several guests out at once,")
    story("    make several nats instead.")
    story()

    step("ACT 5: a published port, which is docker's -p")
    story()
    host_port = free_host_port()
    story("    starting a server on port %d INSIDE the guest, and asking"
          % GUEST_PORT)
    story("    the host for it on port %d." % host_port)
    story()
    put(GW, "/tmp/guest_page.py",
        GUEST_SERVER.replace("PORT", str(GUEST_PORT)))
    out = ssh(GW, "python3 -m py_compile /tmp/guest_page.py 2>&1",
              check=False)
    if out.strip():
        raise RuntimeError("the guest page server did not survive being "
                           "written:\n%s" % out)
    ssh(GW, "pkill -f /tmp/guest_page.py; true", check=False)
    ssh(GW, "python3 /tmp/guest_page.py </dev/null >/tmp/guest_page.log 2>&1 &")
    time.sleep(2)

    said = knock(host_port)
    story("      with nothing published:  %s" % said.strip().split("\n")[0])
    ok_closed = GUEST_MARK not in said
    story()
    story("    now publishing that port on the nat that is already up:")
    r = run("cloonix cli %s cnf %s publish=%d:%d"
            % (NET, NAT, host_port, GUEST_PORT))
    ok_published = (r.returncode == 0)
    if not ok_published:
        story("      the server would not take it: %s"
              % ((r.stdout + r.stderr).strip() or "it said nothing"))
    else:
        time.sleep(6)
        said = knock(host_port)
        story()
        story("      with %d:%d published: %s"
              % (host_port, GUEST_PORT, said.strip().split("\n")[-1]))
        ok_published = GUEST_MARK in said
        if ok_published:
            pause("open http://127.0.0.1:%d/ in a browser now, a new tab "
                  "of this one does: the page comes from the guest"
                  % host_port)
    story()
    story("    Nothing inside the guest changed between the two knocks. The")
    story("    same server was listening on port %d for both of them. What"
          % GUEST_PORT)
    story("    changed is that passt was given -t %d:%d instead of -t none,"
          % (host_port, GUEST_PORT))
    story("    so it binds that port on the host and carries what arrives")
    story("    to the guest.")
    story()
    story("    and taking it away again, which is the same command:")
    run("cloonix cli %s cnf %s publish=none" % (NET, NAT))
    time.sleep(6)
    said = knock(host_port)
    story()
    story("      with nothing published again: %s"
          % said.strip().split("\n")[0])
    ok_closed_again = GUEST_MARK not in said
    story()
    story("    Each cnf replaces the whole list, so a nat publishes what the")
    story("    last one said and nothing else. It costs a passt restart --")
    story("    every connection through the nat is cut, and the guest keeps")
    story("    its address and notices nothing else. Where that matters,")
    story("    publish at creation instead:")
    story("      cloonix cli %s add nat %s publish=8080:80" % (NET, NAT))
    story()

    bad = []
    if not ok_dhcp:
        bad.append("no dhcp offer came from passt, so the nat is not"
                   " reaching %s" % LAN_B)
    if not ok_loop:
        bad.append("the loopback mapping did not answer, and the image"
                   " builders depend on it")
    if not after:
        bad.append("inside1 could not reach the nat even with forwarding"
                   " and masquerade on gw")
    if not ok_closed:
        bad.append("the host reached the guest with nothing published, so"
                   " act 5 shows nothing: a nat is egress only until asked")
    if not ok_published:
        bad.append("the published port did not carry the connection in")
    if not ok_closed_again:
        bad.append("publish=none left the port open, so what a nat"
                   " publishes cannot be taken back")
    if bad:
        sys.exit("\n" + "\n".join("  " + b for b in bad))

    story("""
the network is left up. Things to try:

  the dhcp exchange again, with nothing but a subscription:
    cloonix cli %s sniff gw eth1 --dump udp port 67

  what leaves the guest, and the single address everything wears:
    cloonix cli %s sniff gw eth1 --dump

  serve something on your own loopback and fetch it from inside:
    python3 -m http.server 8000 --bind 127.0.0.1
    cloonix cli %s ssh gw "curl http://%s:8000/"

  publish a port on the nat that is up, the way docker does, and reach it
  from the host -- then take it back:
    cloonix cli %s cnf nat1 publish=8080:80 publish=5353:53/udp
    cloonix cli %s cnf nat1 publish=none
""" % (NET, NET, NET, PASST_GW, NET, NET))

if __name__ == "__main__":
    guarded(main, list(ALL) + [NAT])
