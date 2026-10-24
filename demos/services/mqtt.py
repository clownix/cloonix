#!/usr/bin/env python3
r"""mqtt - one message in, several out, and a broker in the middle.

    ./mqtt.py [net]                the net defaults to nemo

Four machines and two lans::

  sensor -- lanA -- broker -- lanB -- display1
                                  \-- display2

  lanA 10.1.0.0/24   sensor .10    broker .1
  lanB 10.2.0.0/24   broker .1     display1 .11   display2 .12

Publishers and subscribers never meet: the sensor and the displays each
connect to the broker only, none told who else is there. Publish and
subscribe shows as a count: one PUBLISH crosses lanA and two cross lanB, two
machines having subscribed; the sensor sent one message.

  ACT 1  the two displays connect and subscribe: CONNECT, CONNACK,
         SUBSCRIBE, SUBACK, four packets and a topic filter.
  ACT 2  the sensor publishes once: one packet in, two out.
  ACT 3  a wildcard: display2 asked for a whole branch of the topic tree,
         so it gets messages display1 never sees.
  ACT 4  a retained message: a display connecting afterwards is told the
         last value at once, the broker keeping one message per topic.

Image: mqtt_trixie (mosquitto and mosquitto-clients)
Size:  4 containers, 2 lans

THE PACKET TO CATCH
-------------------
  MQTT is tcp 1883:

    cloonix cli nemo cnf broker eth0 sniff=on
    cloonix cli nemo sniff broker eth0 --dump tcp port 1883
    cloonix cli nemo cnf broker eth1 sniff=on
    cloonix cli nemo sniff broker eth1 --dump tcp port 1883

  The packet to catch is one PUBLISH arriving on lanA and the two leaving
  on lanB: the fan-out happens in the broker. A subscriber started after the
  sensor stopped receives on lanB a PUBLISH of a message sent before it
  existed, with nothing on lanA: the retained message. Topic and payload are
  in clear in the dump: MQTT has no transport security of its own.
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

ROOTFS_CHOICES = ("mqtt_trixie",)

HISTORY = """\
MQTT was written in 1999 by Andy Stanford-Clark of IBM and Arlen Nipper
of Arcom to carry readings from oil pipeline sensors over satellite
links that were paid by the byte: a two-byte fixed header, a broker in
the middle, topics, and a quality of service per message. IBM released
the specification royalty free in 2010, OASIS standardised version
3.1.1 in 2014 and version 5 in 2019. Mosquitto was written by Roger
Light in 2009 and has been an Eclipse Foundation project since 2014.

Today: MQTT is the transport of most connected sensors, home automation (Home
Assistant, Zigbee2MQTT), car telemetry and the device gateways of AWS, Azure
and Google clouds; it has been an ISO standard since 2016.
"""

SEN, BRK, D1, D2 = "sensor", "broker", "display1", "display2"
DISPLAYS = (D1, D2)
ALL = (SEN, BRK, D1, D2)
LAN_A, LAN_B = "lanA", "lanB"

WIRING = {
    SEN: [(0, LAN_A, "10.1.0.10/24")],
    BRK: [(0, LAN_A, "10.1.0.1/24"), (1, LAN_B, "10.2.0.1/24")],
    D1:  [(0, LAN_B, "10.2.0.11/24")],
    D2:  [(0, LAN_B, "10.2.0.12/24")],
}

GATEWAY = {SEN: "10.1.0.1", D1: "10.2.0.1", D2: "10.2.0.1"}

BROKER_A = "10.1.0.1"      # how the sensor reaches it
BROKER_B = "10.2.0.1"      # how the displays do
MQTT_PORT = 1883

TOPIC_TEMP = "house/kitchen/temperature"
TOPIC_HUM = "house/kitchen/humidity"
WILDCARD = "house/kitchen/#"

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
                 "  falls back to nemo:   ./mqtt.py <net>"
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
             "mqtt_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_mqtt_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said or "nothing"))

def tools_present(node, *tools):
    for t in tools:
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                            "command -v %s >/dev/null 2>&1" % t],
                           capture_output=True, stdin=subprocess.DEVNULL)
        if r.returncode:
            unavailable("%s has no %s: this is not the mqtt image.\n"
                     "  fetch it:\n"
                     "    cd /var/lib/cloonix/bulk\n"
                     "    wget https://clownix.net/downloads/cnt_demo_bulk/"
                     "mqtt_trixie.tar.gz\n"
                     "    (cloonix extracts it at the first use)\n"
                     "  or build it:\n"
                     "    cd vm_create/cnt_kvm_demo && ./cnt_mqtt_trixie.sh"
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
    """The broker is also the router between the two lans.

    Not because mqtt needs it -- every client talks only to the broker, and
    the broker is on both lans -- but because a demo where the machines
    cannot ping each other would mislead.
    """
    ssh(node, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    got = ssh(node, "sysctl -n net.ipv4.ip_forward", check=False).strip()
    if got != "1":
        raise RuntimeError("%s would not enable ip_forward, it reads %r"
                           % (node, got))
    trace("%s forwards ipv4", node)

# the broker

def broker_conf():
    """Anonymous, on both lans, and keeping retained messages on disk.

    allow_anonymous has no place on a real broker; here a demo about
    topics is not also a demo about passwords.
    persistence is what makes act 4 work -- the retained message
    survives in /var/lib/mosquitto, which is why the image had to claim that
    directory for root.
    """
    return """listener %d 0.0.0.0
allow_anonymous true
user root
persistence true
persistence_location /var/lib/mosquitto/
log_dest file /var/log/mosquitto/mosquitto.log
log_type all
""" % MQTT_PORT

def broker_start():
    ssh(BRK, "mkdir -p /etc/mosquitto /var/lib/mosquitto /var/log/mosquitto")
    put(BRK, "/etc/mosquitto/mosquitto.conf", broker_conf())
    out = ssh(BRK, "mosquitto -c /etc/mosquitto/mosquitto.conf -v -t 2>&1 "
                   "| head -n 5", check=False)
    ssh(BRK, "pkill -x mosquitto 2>/dev/null; true", check=False)
    ssh(BRK, "mosquitto -c /etc/mosquitto/mosquitto.conf -d "
             "</dev/null >/var/log/mosquitto/out 2>&1", check=False)
    # up is the port bound, not the process seen: a client in between is
    # refused
    for _ in range(40):
        if ssh(BRK, "pgrep -x mosquitto >/dev/null && "
                    "ss -ltn | grep -q ':%d ' && echo up" % MQTT_PORT,
               check=False).strip():
            trace("the broker is listening on %d, on both lans", MQTT_PORT)
            return
        time.sleep(0.5)
    raise RuntimeError("mosquitto did not start:\n%s\n%s"
                       % (out, ssh(BRK, "cat /var/log/mosquitto/out",
                                   check=False)))

# the clients

def subscribe(node, topic, tag):
    """Leave a subscriber running in the background, writing what it hears.

    -v so the topic is printed with the payload: with a wildcard the topic
    is half the information, and act 3 turns on being able to see which one
    a message arrived on.
    """
    log = "/tmp/%s.log" % tag
    ssh(node, "pkill -f mosquitto_sub 2>/dev/null; rm -f %s; true" % log,
        check=False)
    # V mqttv311 on purpose.
    ssh(node, "mosquitto_sub -V mqttv311 -h %s -p %d -t '%s' -v -i %s "
              "</dev/null >%s 2>&1 &"
              % (BROKER_B, MQTT_PORT, topic, node, log), check=False)
    for _ in range(20):
        if ssh(node, "pgrep -f mosquitto_sub >/dev/null && echo up",
               check=False).strip():
            trace("%s subscribed to %s", node, topic)
            return
        time.sleep(0.5)
    raise RuntimeError("%s never subscribed:\n%s"
                       % (node, ssh(node, "cat " + log, check=False)))

def publish(topic, payload, retain=False):
    ssh(SEN, "mosquitto_pub -V mqttv311 -h %s -p %d -t '%s' -m '%s' "
             "-i sensor %s"
             % (BROKER_A, MQTT_PORT, topic, payload,
                "-r" if retain else ""), check=False)

def heard(node, tag):
    return ssh(node, "cat /tmp/%s.log 2>/dev/null" % tag, check=False)

def stop_subscribers():
    for n in DISPLAYS:
        ssh(n, "pkill -f mosquitto_sub 2>/dev/null; true", check=False)

# The wire An mqtt packet starts with one byte: the top four bits are the
# message type and the bottom four are flags.

MQTT_TYPES = {1: "CONNECT", 2: "CONNACK", 3: "PUBLISH", 4: "PUBACK",
              5: "PUBREC", 6: "PUBREL", 7: "PUBCOMP", 8: "SUBSCRIBE",
              9: "SUBACK", 10: "UNSUBSCRIBE", 11: "UNSUBACK",
              12: "PINGREQ", 13: "PINGRESP", 14: "DISCONNECT"}

def mqtt_varlen(p, i):
    """The remaining-length field: seven bits per byte, top bit continues."""
    n, shift = 0, 0
    for _ in range(4):
        if i >= len(p):
            return None, i
        b = p[i]
        i += 1
        n |= (b & 0x7f) << shift
        if not (b & 0x80):
            return n, i
        shift += 7
    return None, i

def mqtt_string(p, i):
    if i + 2 > len(p):
        return None, i
    ln = int.from_bytes(p[i:i + 2], "big")
    i += 2
    if i + ln > len(p):
        return None, i
    return p[i:i + ln].decode("ascii", "replace"), i + ln

def skip_v5_properties(p, i):
    """Step over an mqtt 5 property block, if that is what is at i.

    Only CONNECT carries the protocol version, so every other packet type
    has to be read without knowing which version it is. Version 5 puts a
    property block -- a length, then that many bytes -- where version 3.1.1
    puts the next field straight away.

    The guess: a length of 0 followed by something that reads as a string
    is version 5 with no properties, which is what these clients send. It
    is a guess, said as one, which is why the demo
    also asks its clients for 3.1.1 and does not have to rely on it.
    """
    if i < len(p) and p[i] == 0 and i + 3 <= len(p):
        ln = int.from_bytes(p[i + 1:i + 3], "big")
        if 0 < ln <= len(p) - i - 3:
            return i + 1
    return i

def decode(pkt):
    d = pkt["data"]
    if len(d) < 34 or d[12:14] != b"\x08\x00" or d[23] != 6:
        return None
    ihl = (d[14] & 0x0f) * 4
    base = 14 + ihl
    sport = int.from_bytes(d[base:base + 2], "big")
    dport = int.from_bytes(d[base + 2:base + 4], "big")
    if MQTT_PORT not in (sport, dport):
        return None
    off = base + ((d[base + 12] >> 4) * 4)
    p = d[off:]
    src = ".".join(str(b) for b in d[26:30])
    dst = ".".join(str(b) for b in d[30:34])
    if not p:
        return None
    typ = p[0] >> 4
    flags = p[0] & 0x0f
    name = MQTT_TYPES.get(typ)
    if not name:
        return None
    ln, i = mqtt_varlen(p, 1)
    if ln is None:
        return None
    text = name
    topic = None
    if typ == 3:                                   # PUBLISH
        qos = (flags >> 1) & 3
        retain = flags & 1
        topic, i = mqtt_string(p, i)
        if topic is None:
            return None
        if qos:
            i += 2                                 # packet id, when qos > 0
        i = skip_v5_properties(p, i)
        # A v5 publish with no properties leaves a zero byte at the payload start.
        payload = p[i:].decode("ascii", "replace").lstrip("\x00")
        text = "PUBLISH   %-28s %s%s" % (
            topic, payload.strip()[:24],
            "   RETAINED" if retain else "")
    elif typ == 8:                                 # SUBSCRIBE
        i += 2                                     # packet id
        i = skip_v5_properties(p, i)
        topic, i = mqtt_string(p, i)
        text = "SUBSCRIBE %s" % (topic or "?")
    elif typ == 1:                                 # CONNECT
        proto, i = mqtt_string(p, i)
        ver = p[i] if i < len(p) else 0
        i += 4                                     # version, flags, keepalive
        if ver >= 5:
            plen, i = mqtt_varlen(p, i)
            i += plen or 0
        cid, i = mqtt_string(p, i)
        text = "CONNECT   protocol %s v%d, client id %s" % (proto, ver,
                                                            cid or "(none)")
    return {"kind": "MQTT", "src": src, "dst": dst,
            "sport": sport, "dport": dport,
            "type": name, "topic": topic, "text": text}

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
    (SEN, 0, "sensor eth0  lanA"),
    (BRK, 1, "broker eth1  lanB"),
]

def show_packets(sink, lock, title, limit=30, skip_pings=True):
    with lock:
        rows = sorted(sink, key=lambda r: r["when"])
    if skip_pings:
        # A subscriber that is doing nothing still says PINGREQ every few
        # seconds so the broker knows it is alive.
        rows = [r for r in rows
                if r["msg"]["type"] not in ("PINGREQ", "PINGRESP")]
    story("    %s" % title)
    if not rows:
        story("      (no mqtt crossed the sniffed interfaces)")
        return 0
    t0 = rows[0]["when"]
    story("      %-8s  %-18s  %-19s %-19s %s"
          % ("t+ms", "seen on", "from", "to", "what went past"))
    story("      " + "-" * 118)
    for r in rows[:limit]:
        m = r["msg"]
        story("      %-8.2f  %-18s  %-19s %-19s %s"
              % ((r["when"] - t0) * 1000.0, r["where"],
                 "%s:%d" % (m["src"], m["sport"]),
                 "%s:%d" % (m["dst"], m["dport"]), m["text"][:62]))
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
        out = do_it()
        time.sleep(settle)
    finally:
        for t in taps:
            t.stop()
    show_packets(sink, lock, title)
    rows = sorted(sink, key=lambda r: r["when"])
    return {"out": out, "rows": rows}

def count_publishes(rows, topic=None):
    """How many PUBLISH packets crossed each wire."""
    per = {}
    for r in rows:
        m = r["msg"]
        if m["type"] != "PUBLISH":
            continue
        if topic and m["topic"] != topic:
            continue
        per[r["where"]] = per.get(r["where"], 0) + 1
    return per

LAYOUT = {
    SEN: (-380, 0),
    LAN_A: (-190, 0),
    BRK: (0, 0),
    LAN_B: (190, 0),
    D1: (380, -75),
    D2: (380, 75),
}

ETHS = {
    (BRK, "eth0"): 270,
    (BRK, "eth1"): 90,
    (D1, "eth0"): 248,
    (D2, "eth0"): 292,
    (SEN, "eth0"): 90,
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
    story("rootfs %s: one broker, one sensor, two displays" % rootfs)
    story("""
    sensor -- lanA -- broker -- lanB -- display1
   10.1.0.10      10.1.0.1  \\        10.2.0.11
                  10.2.0.1   \\--- display2
                                 10.2.0.12

    Publishers and subscribers never meet. Every machine has exactly one
    connection, to the broker, and none of them is told who else is
    there. The sensor names a TOPIC, not a recipient.
""")

    step("erasing the net, then placing the items")
    erase_net()
    place()

    step("creating the four containers")
    in_parallel(lambda n: create_cnt(n, rootfs), list(ALL), "add cnt")

    step("waiting for them to answer")
    in_parallel(wait_ready, list(ALL), "wait_ready")
    tools_present(BRK, "mosquitto")
    tools_present(D1, "mosquitto_sub")
    tools_present(SEN, "mosquitto_pub")

    step("cabling them")
    for node in ALL:
        for num, lan, _ in WIRING[node]:
            cli("plug", node, "eth%d" % num, lan)

    step("addressing them")
    in_parallel(address, list(ALL), "address")
    enable_forwarding(BRK)

    step("starting the broker")
    broker_start()

    step("act 1: the two displays connect and say what they want")
    story()
    a = watch(lambda: [subscribe(D1, TOPIC_TEMP, "d1"),
                       subscribe(D2, WILDCARD, "d2")],
              "two clients arriving:", settle=3.0)
    if a is None:
        sys.exit("could not sniff the path")
    story("    Four packets each, and the interesting one is SUBSCRIBE.")
    story("    display1 asked for one topic. display2 asked for %s --" % WILDCARD)
    story("    a whole branch of the tree, which act 3 turns on. Neither")
    story("    of them named a publisher, because there is no such thing")
    story("    to name.")
    story()

    step("act 2: the sensor publishes once")
    story()
    b = watch(lambda: publish(TOPIC_TEMP, "21.5"),
              "one message, and what it became:", settle=5.0)
    if b is None:
        sys.exit("could not sniff the path")
    per = count_publishes(b["rows"], TOPIC_TEMP)
    for label in ("sensor eth0  lanA", "broker eth1  lanB"):
        story("      PUBLISH packets on %-20s %d" % (label, per.get(label, 0)))
    story()
    story("    One in and two out. The sensor sent a single message and")
    story("    has no idea it became two -- it does not know how many")
    story("    subscribers there are, or whether there are any at all. A")
    story("    publish to a topic nobody wants is not an error and looks")
    story("    exactly the same from the sensor's end.")
    story()
    for n, tag in ((D1, "d1"), (D2, "d2")):
        story("      %-9s heard: %s" % (n, heard(n, tag).strip() or "nothing"))
    story()
    ok_fanout = (TOPIC_TEMP in heard(D1, "d1")
                 and TOPIC_TEMP in heard(D2, "d2"))
    if ok_fanout and per.get("broker eth1  lanB", 0) < 2:
        story("      (both displays received it, so the fan-out happened;")
        story("       the lanB capture reported fewer packets than that,")
        story("       which is the sniffer missing them, not the broker.)")
        story()

    step("act 3: a second topic, and only one display wants it")
    story()
    c = watch(lambda: publish(TOPIC_HUM, "48"),
              "the same sensor, a different topic:", settle=5.0)
    if c is None:
        sys.exit("could not sniff the path")
    per = count_publishes(c["rows"], TOPIC_HUM)
    for label in ("sensor eth0  lanA", "broker eth1  lanB"):
        story("      PUBLISH packets on %-20s %d" % (label, per.get(label, 0)))
    story()
    for n, tag in ((D1, "d1"), (D2, "d2")):
        story("      %-9s heard: %s"
              % (n, heard(n, tag).strip().replace("\n", " ; ") or "nothing"))
    story()
    story("    One in and ONE out this time. display1 subscribed to a single")
    story("    topic and this is not it, so the broker did not send it")
    story("    anything -- the filtering happens in the broker, not in the")
    story("    client, and the packet display1 does not want never reaches")
    story("    its wire at all.")
    story()
    d1_log, d2_log = heard(D1, "d1"), heard(D2, "d2")
    ok_wildcard = (TOPIC_HUM in d2_log) and (TOPIC_HUM not in d1_log)
    if ok_wildcard and per.get("broker eth1  lanB", 0) != 1:
        story("      (display2 received it and display1 did not, which is")
        story("       the whole claim; the lanB capture reported %d packets"
              % per.get("broker eth1  lanB", 0))
        story("       rather than 1, which is the sniffer, not the broker.)")
        story()

    step("act 4: a retained message, for whoever turns up next")
    story()
    publish(TOPIC_TEMP, "22.0", retain=True)
    time.sleep(1)
    story("      the sensor published %s with the retain flag set, and then"
          % TOPIC_TEMP)
    story("      stopped. Nothing else will be published from here on.")
    story()
    stop_subscribers()
    time.sleep(1)
    d = watch(lambda: subscribe(D1, TOPIC_TEMP, "d1b"),
              "a display connecting to a silent network:", settle=3.0)
    if d is None:
        sys.exit("could not sniff the path")
    late = heard(D1, "d1b").strip()
    story("      display1 connected and immediately heard: %s"
          % (late or "nothing"))
    story()
    story("    Nobody published anything while it was connecting. The broker")
    story("    kept the last message on that topic because it was told to,")
    story("    and hands it to every new subscriber the moment they ask.")
    story("    That is why a dashboard shows a value the instant it opens")
    story("    instead of an empty box until the next reading.")
    story()
    ok_retained = "22.0" in late

    if not (ok_fanout and ok_wildcard and ok_retained):
        sys.exit("\nsomething did not work,"
                 " see the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")

    story("""
the network is left up. Things to try:

  watch the broker's side live while you publish by hand from the sensor:
    cloonix cli %s sniff broker eth1 --dump
    cloonix cli %s ssh sensor "mosquitto_pub -h %s -t %s -m 30.0"

  subscribe to everything and see how much a broker carries. The
  # filter matches every topic there is:
    cloonix cli %s ssh display1 "mosquitto_sub -h %s -t '#' -v"

  and watch what happens with no subscribers at all -- publish to a topic
  nobody asked for, and see one packet arrive at the broker and none leave:
    cloonix cli %s ssh sensor "mosquitto_pub -h %s -t nobody/wants/this -m x"
""" % (NET, NET, BROKER_A, TOPIC_TEMP, NET, BROKER_B, NET, BROKER_A))

main()
