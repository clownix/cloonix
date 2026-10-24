#!/usr/bin/env python3
"""asterisk - a PBX, two SIP phones, and two calls that carry audio.

    ./asterisk.py [net]            the net defaults to nemo

Three containers on one lan::

    phone1 ---- lan1 ---- pbx ---- lan1 ---- phone2
   10.0.0.2            10.0.0.1            10.0.0.3

pbx runs asterisk. The two phones run SIPp, a SIP traffic generator and not
a softphone: it drives a call flow from a scenario and carries RTP, so a
call is placed and measured with no sound card.

  CALL A  phone1 dials 100: asterisk answers itself with a 440 Hz tone,
          Playtones, synthesised in the switch with no sound file, and
          phone1 echoes the RTP back; six seconds later phone1 sends BYE.
          The media test: "pjsip show channelstats" then counts packets
          both ways, audio carried and not only negotiated.
  CALL B  phone1 dials 200: asterisk finds the extension in the dialplan,
          calls phone2 and bridges the two; phone2 answers, six seconds
          pass, phone1 sends BYE and asterisk relays it. The switching
          test: two SIP legs, one bridge, a release relayed.

Every configuration file of asterisk is written at run time over cloonix
cli ssh: the image is a plain alpine with asterisk and the SIP tools.

Image: asterisk_alpine
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
  A call is two conversations on one wire. The signalling:

    cloonix cli nemo cnf pbx eth0 sniff=on
    cloonix cli nemo sniff pbx eth0 --dump udp port 5060

  The packet to catch is the INVITE, then the SDP body of the 200 OK, where
  the phones give the RTP port they use; the media follows on the ports
  chosen there, which the image pins to a narrow range:

    cloonix cli nemo sniff pbx eth0 --dump udp portrange 10000-10010

  The two subscriptions together show the shape of SIP: a short exchange
  on 5060 that stops, and a long stream on another port that starts then.
"""
import os
import queue
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
    NET, ACK_FAST, CLOONIX, capturable, cli, erase_net, in_parallel,
    net_answers, show, step, story, trace, trace_ssh, wait_ready, unavailable,
    needs_subuid)

# asterisk_alpine only.
ROOTFS_CHOICES = ("asterisk_alpine",)

HISTORY = """\
SIP was published by Henning Schulzrinne, Mark Handley, Eve Schooler
and Jonathan Rosenberg as RFC 2543 in 1999 and rewritten as RFC 3261
in 2002: a text protocol built like HTTP, INVITE, 200 OK, ACK, BYE,
that sets up a call and lets RTP, RFC 1889 of 1996, carry the audio.
Asterisk was started by Mark Spencer in 1999 in Huntsville, Alabama,
because a PBX for his linux support company cost more than writing
one; it became the product of Digium and is published today by
Sangoma. SIPp, the traffic generator on the two phones, was written at
Hewlett-Packard by Richard Gayraud and Olivier Jacques in 2004.

Today: SIP carries business telephony and the voice of 4G and 5G networks
(VoLTE); Asterisk and its FreePBX distribution run the switchboards of small
companies and call centres worldwide.
"""

LAN = "lan1"
PBX, P1, P2 = "pbx", "phone1", "phone2"
IP = {PBX: "10.0.0.1", P1: "10.0.0.2", P2: "10.0.0.3"}

CALL_MS = 6000

# The configuration asterisk needs.

# No [directories] section on purpose.
ASTERISK_CONF = """[options]
verbose = 3
"""

MODULES_CONF = """[modules]
autoload=yes
"""

LOGGER_CONF = """[general]
[logfiles]
messages => notice,warning,error,verbose
"""

RTP_CONF = """[general]
rtpstart=10000
rtpend=10100
"""

def pjsip_conf():
    """One transport, and each phone as an endpoint identified by its address.

    No registration anywhere. A phone that registers has to be up before the
    PBX will talk to it, which turns a demo into an ordering problem; an
    endpoint matched on its source address is reachable the moment its
    container is. That is also how a trunk to another switch is configured
    in real life, so nothing is being faked to make the demo easier.

    direct_media=no matters more than it looks. Left at yes, asterisk tells
    the two phones to send RTP to each other and steps out of the media
    path -- which is the efficient thing to do and makes the packet counters
    on the PBX read zero for a call that is working perfectly. Keeping the
    media on the switch is what makes call B measurable from the PBX.
    """
    out = ["[global]",
           "type=global",
           # ip first, so the identify sections below are consulted before
           # asterisk gives up and treats the INVITE as anonymous.
           "endpoint_identifier_order=ip,username,anonymous",
           "",
           "[transport-udp]",
           "type=transport",
           "protocol=udp",
           "bind=0.0.0.0:5060",
           ""]
    for name in (P1, P2):
        out += ["[%s]" % name,
                "type=endpoint",
                "context=demo",
                "disallow=all",
                "allow=ulaw",
                "aors=%s" % name,
                "direct_media=no",
                "",
                "[%s]" % name,
                "type=aor",
                "contact=sip:%s@%s:5060" % (name, IP[name]),
                "",
                "[%s]" % name,
                "type=identify",
                "endpoint=%s" % name,
                "match=%s" % IP[name],
                ""]
    return "\n".join(out)

EXTENSIONS_CONF = """[demo]
exten => 100,1,Answer()
exten => 100,2,Playtones(440/1000,0/1000)
exten => 100,3,Wait(20)
exten => 100,4,StopPlaytones()
exten => 100,5,Hangup()
exten => 200,1,Dial(PJSIP/phone2,30)
exten => 200,2,Hangup()
"""

# cloonix plumbing, the same shape the frr demos use.

def ssh(node, cmd, check=True, log=None):
    """Run one command in a guest.

    A command whose output has been redirected into a file says nothing on
    failure, and the bare "-> " that produced said nothing. When
    the caller names the log, it is fetched and shown instead.
    """
    trace_ssh("ssh %s: %s", node, cmd if len(cmd) < 70 else cmd[:67] + "...")
    show('cloonix cli %s ssh %s "%s"' % (NET, node, cmd))
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node, cmd],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if r.returncode:
        trace("ssh %s: returned %d", node, r.returncode)
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

def create_cnt(node, rootfs):
    """Create one container, and check that the server took it.

    Two things make this a function of its own.

    A create issued with the short ack does not report a refusal: the client
    waits a third of a second, hears nothing, and calls that success. So a
    container the server would not make looked created, and the failure
    surfaced ninety seconds later as "never answered" -- which points at
    booting, not at a request that was turned down.

    And it can legitimately be refused for a moment. erase_net waits until
    the old machines stop answering ssh, but ssh stops answering well before
    a container is finished being torn down, so a create issued straight
    afterwards can land on a name the server is still disposing of. Hence
    retries: the full ack so a refusal is seen, and a few goes so a tail end
    of the previous run is ridden out rather than treated as fatal.
    """
    last = ""
    for attempt in range(6):
        r = cli("add", "cnt", node, ETH, rootfs)
        if r.returncode == 0:
            if attempt:
                trace("%s created on attempt %d", node, attempt + 1)
            return
        last = (r.stdout + r.stderr).strip()
        time.sleep(2)
    raise RuntimeError("%s was never created. The server said: %s"
                       % (node, last or "nothing at all"))

# eth=1, not eth=1.
ETH = "eth=1"

PROBE_ETH = "eth=1"

def choose_rootfs():
    """Create a throwaway container to find out whether the image is usable:
    the client cannot list bulk, and the server refuses an image it cannot
    run, so asking is the only way to find out."""
    up, why = net_answers()
    if not up:
        sys.exit("no answer from a cloonix net called '%s'.\n"
                 "  the client said: %s\n"
                 "  A demo takes the net name as its first argument and\n"
                 "  falls back to nemo, so this is what to check first:\n"
                 "    ./asterisk.py <net>\n"
                 "  The nets this machine knows are in\n"
                 "  /usr/libexec/cloonix/cloonfs/etc/cloonix.cfg"
                 % (NET, why or "nothing at all"))
    said = ""
    for cand in ROOTFS_CHOICES:
        trace("probing the server with a throwaway cnt on %s", cand)
        r = cli("add", "cnt", "probe_rootfs", PROBE_ETH, cand, ack=240)
        trace_ssh("probe returned %d", r.returncode)
        if r.returncode == 0:
            # Nothing depends on the answer to this one.
            cli("del", "probe_rootfs", ack=ACK_FAST)
            return cand
        said = (r.stdout + r.stderr).strip()
    cli("del", "probe_rootfs", ack=ACK_FAST)
    unavailable("the server would not start a container on %s.\n"
             "  it said: %s\n"
             "  If /var/lib/cloonix/bulk/%s is not there, fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_demo_bulk/"
             "asterisk_alpine.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_asterisk_alpine.sh\n"
             "  If it IS there, the tree is the thing to look at. An image\n"
             "  is only offered when it has an init the server recognises:\n"
             "    ls -l /var/lib/cloonix/bulk/%s/sbin/init\n"
             "  and compare its permissions with an image that works:\n"
             "    ls -ld /var/lib/cloonix/bulk/alpine"
             % (" or ".join(ROOTFS_CHOICES), said or "nothing",
                ROOTFS_CHOICES[0], ROOTFS_CHOICES[0]))

def tools_present(node, *tools):
    """Fail early and clearly rather than at the first missing file."""
    for t in tools:
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                            "command -v %s >/dev/null 2>&1" % t],
                           capture_output=True, stdin=subprocess.DEVNULL)
        if r.returncode:
            sys.exit("%s has no %s: the image carries no SIP tools.\n"
                     "  build one with"
                     " vm_create/cnt_kvm_demo/cnt_asterisk_alpine.sh" % (node, t))

def address(node):
    trace("%s gets %s", node, IP[node])
    ssh(node, "ip addr add %s/24 dev eth0" % IP[node])
    ssh(node, "ip link set dev eth0 up")
    name_resolution(node)

def name_resolution(node):
    """Make every name on this lan resolve, and make everything else fail fast.

    Without this the calls carry an extra ten seconds each, and the capture
    shows exactly where they go: INVITE, 100 Trying, then 10.01 s of nothing
    before the 200 OK, and 20.03 s before the second leg of the switched
    call. Those are not asterisk thinking. They are the resolver: 5 s per
    attempt, twice, once per leg.

    Two things cause it, and both come from the image rather than from here.
    cnt_common_config writes an /etc/hosts holding only localhost, and the
    image sets a hostname of its own -- so the guest cannot resolve its own
    name, which asterisk looks up while setting a channel up. Having failed
    in /etc/hosts it asks DNS, and /etc/resolv.conf is whatever the machine
    that BUILT the image had, pointing at a nameserver no guest on a cloonix
    lan can reach. Nothing answers and nothing refuses, so it waits.

    So: the three names of this demo and the guest's own go in /etc/hosts,
    and the resolver file goes away entirely. There is no DNS on this lan
    and pretending otherwise is what costs the twenty seconds; with no
    resolv.conf a lookup that misses /etc/hosts fails at once.
    """
    hosts = "127.0.0.1 localhost"
    for name in (PBX, P1, P2):
        hosts += "\\n%s %s" % (IP[name], name)
    ssh(node, "printf '%s\\n' > /etc/hosts" % hosts)
    ssh(node, 'echo "127.0.1.1 $(hostname)" >> /etc/hosts')
    ssh(node, "rm -f /etc/resolv.conf")

def start_asterisk():
    """Write the configuration, start the switch, and wait until it answers."""
    ssh(PBX, "mkdir -p /etc/asterisk /var/log/asterisk /var/lib/asterisk "
             "/var/spool/asterisk /var/run/asterisk")
    put(PBX, "/etc/asterisk/asterisk.conf", ASTERISK_CONF)
    put(PBX, "/etc/asterisk/modules.conf", MODULES_CONF)
    put(PBX, "/etc/asterisk/logger.conf", LOGGER_CONF)
    put(PBX, "/etc/asterisk/rtp.conf", RTP_CONF)
    put(PBX, "/etc/asterisk/pjsip.conf", pjsip_conf())
    put(PBX, "/etc/asterisk/extensions.conf", EXTENSIONS_CONF)

    # asterisk stays root.
    trace("six config files written on %s, starting the switch", PBX)
    ssh(PBX, "asterisk </dev/null >/dev/null 2>&1")

    # asterisk returns as soon as it has forked, well before the modules are
    # loaded, so it is polled rather than slept at.
    for _ in range(30):
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", PBX,
                            "asterisk -rx 'core show version'"],
                           capture_output=True, text=True,
                           stdin=subprocess.DEVNULL)
        if r.returncode == 0 and "Asterisk" in r.stdout:
            trace("the console answers")
            return r.stdout.strip().split("\n")[0]
        trace_ssh("the console does not answer yet")
        time.sleep(1)
    raise RuntimeError("asterisk never came up, see /var/log/asterisk on pbx")

def rx(cmd):
    """One command on the asterisk console."""
    return ssh(PBX, "asterisk -rx '%s'" % cmd, check=False).strip()

def place_call(extension, note):
    """Run SIPp as the caller and report what the switch saw mid-call.

    The sampler is started first, in the background on the PBX, so the
    channel and RTP counters are read while the call is up. Reading them
    afterwards would show nothing at all: a released call has no channel.
    """
    story("\n--- calling %s: %s" % (extension, note))
    ssh(PBX, "(sleep 3; asterisk -rx 'core show channels' > /tmp/mid.txt; "
             "asterisk -rx 'pjsip show channelstats' >> /tmp/mid.txt) "
             "</dev/null >/dev/null 2>&1 &")

    # -m 1 one call, -d call duration, -rtp_echo two-way media, -timeout give up.
    cmd = ("sipp -sn uac -m 1 -r 1 -d %d -rtp_echo -timeout 40 "
           "-timeout_error -i %s -p 5060 -s %s %s:5060 "
           "</dev/null >/tmp/uac.log 2>&1"
           % (CALL_MS, IP[P1], extension, IP[PBX]))
    trace("running sipp on %s", P1)
    t0 = time.time()
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", P1, cmd],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    took = time.time() - t0

    mid = ssh(PBX, "cat /tmp/mid.txt 2>/dev/null", check=False).strip()
    if mid:
        story(mid)
    if r.returncode:
        story("SIPp reported a failure, its log follows:")
        story(ssh(P1, "tail -n 25 /tmp/uac.log", check=False))
        return False
    story("call to %s completed, %.1f s from INVITE to BYE" % (extension, took))
    return True

class PacketStream:
    """The packets of one interface, as they arrive, from cloonix cli sniff.

    sniff blocks for ever by design, so it is run as a child and drained by
    a thread into a queue. A thread rather than select() because the child's
    stdout is line buffered on this side: select would say "ready" for bytes
    already sitting in the buffer and say nothing for a line that is
    complete but unread.
    """

    def __init__(self, node, num, filt=None):
        self.node = node
        self.num = num
        self.filt = filt
        capturable(node, num)
        cmd = CLOONIX + ["cli", NET, "sniff", node, "eth%d" % num, "--bytes"]
        if filt:
            cmd += filt.split()
        self.proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1)
        self.q = queue.Queue()
        self.tally = {}
        self.sip = []
        self.thread = threading.Thread(target=self._drain, daemon=True)
        self.thread.start()
        trace("subscribed to %s %d, pid %d%s", node, num, self.proc.pid,
              (", filtering on: " + filt) if filt else "")

    def _drain(self):
        for line in self.proc.stdout:
            self.q.put(line.rstrip("\n"))
        self.q.put(None)

    def ready(self, timeout=15):
        """Wait for the subscription to be live.

        This matters more than it looks. Subscribing and then immediately
        making the traffic happen is a race the scenario loses about as often
        as it wins, and it loses silently: the packets are not there
        and the test reports that the INVITE never came.
        """
        end = time.time() + timeout
        while time.time() < end:
            try:
                line = self.q.get(timeout=0.5)
            except queue.Empty:
                continue
            if line is None:
                break
            if line.startswith("# waiting"):
                trace("the subscription is live")
                return True
            if line.startswith("#"):
                continue
        err = self.proc.stderr.read() if self.proc.stderr else ""
        story("could not subscribe to %s %d: %s"
              % (self.node, self.num, err.strip() or "no answer"))
        return False

    def wait_for(self, want, what, timeout=25):
        """Read packets until one satisfies want(), or give up.

        Returns the packet dict, or None. Packets that do not match are
        dropped rather than kept: a scenario wants the one it is waiting for,
        and holding the rest only grows.
        """
        end = time.time() + timeout
        seen = 0
        while time.time() < end:
            try:
                line = self.q.get(timeout=0.5)
            except queue.Empty:
                continue
            if line is None:
                break
            if not line.startswith("pkt "):
                continue
            seen += 1
            pkt = parse_bytes_line(line)
            if pkt is None:
                trace("a line did not parse: %s", line[:60])
                continue
            kind = describe(pkt)
            self.tally[kind] = self.tally.get(kind, 0) + 1
            if kind.endswith("(SIP)"):
                line_txt = sip_line(pkt)
                self.sip.append("%.6f  %s" % (pkt["when"],
                                              line_txt if line_txt
                                              else "<did not decode as text>"))
            if pkt and want(pkt):
                story("  OK   %s, after %d packets on the wire" % (what, seen))
                return pkt
        story("  FAIL %s: nothing matched in %d s (%d packets seen)"
              % (what, timeout, seen))
        if self.tally:
            story("       the wire carried: %s"
                  % ", ".join("%s x%d" % (k, v)
                              for k, v in sorted(self.tally.items(),
                                                 key=lambda kv: -kv[1])))
        if self.sip:
            story("       the SIP that did go past:")
            for one in self.sip[:20]:
                story("         %s" % one)
        return None

    def stop(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()

def parse_bytes_line(line):
    """One --bytes line into something a test can look at.

        pkt <n> <sec>.<usec> <caplen>/<len> <byte> <byte> ...
    """
    f = line.split()
    if len(f) < 5:
        return None
    try:
        return {"nb": int(f[1]),
                "when": float(f[2]),
                "caplen": int(f[3].split("/")[0]),
                "data": bytes(int(x, 16) for x in f[4:])}
    except ValueError:
        return None

def udp_payload(pkt):
    """The UDP payload of an IPv4 packet, or None if it is not one.

    Written out rather than pulled from a library on purpose: the demos have
    no dependency beyond the standard library, same as the client itself.
    """
    d = pkt["data"]
    if len(d) < 34 or d[12:14] != b"\x08\x00":     # not IPv4
        return None
    ihl = (d[14] & 0x0f) * 4
    if d[23] != 17:                                 # not UDP
        return None
    start = 14 + ihl + 8
    return d[start:] if len(d) > start else None

def describe(pkt):
    """A short name for what a packet is, for the tally in a failure."""
    d = pkt["data"]
    if len(d) < 14:
        return "runt"
    et = int.from_bytes(d[12:14], "big")
    if et == 0x0806:
        return "ARP"
    if et == 0x86dd:
        return "IPv6"
    if et != 0x0800:
        return "ethertype 0x%04x" % et
    if len(d) < 34:
        return "IPv4 runt"
    proto = d[23]
    if proto == 1:
        return "ICMP"
    if proto not in (6, 17):
        return "IP proto %d" % proto
    ihl = (d[14] & 0x0f) * 4
    sport = int.from_bytes(d[14+ihl:14+ihl+2], "big")
    dport = int.from_bytes(d[14+ihl+2:14+ihl+4], "big")
    name = "TCP" if proto == 6 else "UDP"
    if 5060 in (sport, dport):
        return name + " 5060 (SIP)"
    return "%s %d->%d" % (name, sport, dport)

def sip_line(pkt):
    """The first line of a SIP message, or None. Ports checked as well as
    content: many things are text and only one of them is on 5060."""
    d = pkt["data"]
    payload = udp_payload(pkt)
    if not payload:
        return None
    ihl = (d[14] & 0x0f) * 4
    sport = int.from_bytes(d[14+ihl:14+ihl+2], "big")
    dport = int.from_bytes(d[14+ihl+2:14+ihl+4], "big")
    if 5060 not in (sport, dport):
        return None
    first = payload.split(b"\r\n", 1)[0]
    try:
        return first.decode("ascii")
    except UnicodeDecodeError:
        return None

def watched_call():
    """Drive a call and prove each step from the packets, not from asterisk.

      1. subscribe to the pbx interface and wait until it is live
      2. place a call
      3. wait to see the INVITE arrive at the pbx          -> success
      4. ask asterisk to end the call
      5. wait to see the BYE go out                         -> success

    Step 4 is the pbx hanging up, not the phone: it is the more interesting
    direction to check, because the BYE then has to be generated by the thing
    under test rather than forwarded by it.
    """
    story("\n--- watching the wire while a call is made and released")
    # A filter, so the server drops what this act does not want.
    stream = PacketStream(PBX, 0, "udp port 5060")
    if not stream.ready():
        return False
    try:
        # A long call, so it ends when this scenario says so and not when SIPp
        # gets bored.
        ssh(P1, "sipp -sn uac -m 1 -r 1 -d 60000 -rtp_echo -i %s -p 5060 "
                "-s 100 %s:5060 </dev/null >/tmp/uac_watch.log 2>&1 &"
                % (IP[P1], IP[PBX]))

        for _ in range(10):
            if ssh(P1, "pgrep -x sipp >/dev/null && echo up",
                   check=False).strip():
                trace("sipp is running on %s", P1)
                break
            time.sleep(0.5)
        else:
            story("  FAIL sipp never started on %s, its log says:" % P1)
            story(ssh(P1, "cat /tmp/uac_watch.log", check=False).rstrip()
                  or "       (the log is empty)")
            return False

        invite = stream.wait_for(
            lambda p: (sip_line(p) or "").startswith("INVITE"),
            "INVITE seen on the wire")
        if not invite:
            return False
        story("       %s" % sip_line(invite))

        # Let the call settle before pulling it down, so the BYE is a release
        # of something established rather than a race with the answer.
        time.sleep(2)

        story("  ---> asking the pbx to hang up")
        rx("channel request hangup all")

        bye = stream.wait_for(
            lambda p: (sip_line(p) or "").startswith("BYE"),
            "BYE seen on the wire")
        if not bye:
            return False
        story("       %s" % sip_line(bye))
        story("  call lasted %.2f s from INVITE to BYE, measured on the wire"
              % (bye["when"] - invite["when"]))
        return True
    finally:
        stream.stop()
        ssh(P1, "pkill sipp", check=False)

LAYOUT = {
    PBX: (-190, 0),
    LAN: (0, 0),
    P1: (190, -75),
    P2: (190, 75),
}

ETHS = {
    (PBX, "eth0"): 90,
    (P1, "eth0"): 248,
    (P2, "eth0"): 292,
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
    story("rootfs %s: one asterisk and two SIPp phones" % rootfs)

    step("erasing the net, then placing the items")
    erase_net()
    place()

    step("creating the three containers")
    in_parallel(lambda n: create_cnt(n, rootfs), [PBX, P1, P2], "add cnt")

    step("waiting for them to answer")
    in_parallel(wait_ready, [PBX, P1, P2], "wait_ready")

    step("cabling them to the lan")
    in_parallel(lambda n: cli("plug", n, "eth0", LAN),
                [PBX, P1, P2], "plug")

    tools_present(PBX, "asterisk")
    tools_present(P1, "sipp")

    step("addressing them")
    in_parallel(address, [PBX, P1, P2], "address")

    step("starting asterisk")
    version = start_asterisk()
    story(version)
    story(rx("pjsip show endpoints"))

    ssh(P2, "sipp -sn uas -rtp_echo -i %s -p 5060 "
            "</dev/null >/tmp/uas.log 2>&1 &" % IP[P2])
    for _ in range(10):
        if ssh(P2, "pgrep -x sipp >/dev/null && echo up", check=False).strip():
            break
        time.sleep(1)
    else:
        story(ssh(P2, "cat /tmp/uas.log", check=False))
        sys.exit("sipp would not start on phone2,"
                 " its log is in the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")

    ok_a = place_call("100", "asterisk answers and plays a tone")
    ok_b = place_call("200", "asterisk switches the call through to phone2")

    ssh(P2, "pkill sipp", check=False)

    cli("cnf", PBX, "eth0", "sniff=on", ack=ACK_FAST)
    ok_c = watched_call()

    story("\n--- what the switch recorded")
    cdr = ssh(PBX, "cat /var/log/asterisk/cdr-csv/Master.csv 2>/dev/null",
              check=False).strip()
    story(cdr if cdr else "no CDR file, cdr_csv did not load")

    if not (ok_a and ok_b and ok_c):
        sys.exit("\none of the calls failed, see /tmp/uac.log on phone1")

    story("\nthe network is left up. To watch a call as it happens, open a")
    story("terminal on pbx from the canvas and run:   sngrep")
    story("then place another one from phone1:")
    story("  cloonix cli %s ssh %s \"sipp -sn uac -m 1 -r 1 -d %d -rtp_echo"
          " -i %s -p 5060 -s 100 %s:5060\""
          % (NET, P1, CALL_MS, IP[P1], IP[PBX]))

main()
