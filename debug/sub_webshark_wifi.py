#!/usr/bin/env python3
"""sub_webshark_wifi - webshark on the capture of an air, with no radio: a
pcap of radiotap 802.11 frames, the kinds an air carries, written into the
fifo webshark reads. Checked on its websocket: the link type 127 sent to the
page (it shows the button Data only), the decoding of each frame (beacon and
SSID, probe, authentication, association, the four EAPOL messages, an
encrypted data frame, ARP and ICMP carried over LLC/SNAP, ACK, deauth,
null), the labels of the tree, and the filters: eapol, ip, and the one of the
button Data only, which keeps the data frames with a payload, clear or
encrypted, and nothing else.
"""
import base64
import glob
import json
import os
import socket
import struct
import subprocess
import threading
import time
from sublib import *

# the filter of the button Data only, in webshark.html
DATA_FILTER = ("(wlan.fc.type_subtype == 0x20 || wlan.fc.type_subtype == 0x28)"
               " && !eapol")
BIN = [os.environ.get("TESTITALL_WEBSHARK", ""),
       "/usr/libexec/cloonix/cloonfs/bin/jscloonix/cloonix-webshark"]


def pcap():
    """15 frames: the numbers the checks below expect."""
    def rt():
        return (struct.pack('<BBHI', 0, 0, 14, (1 << 1) | (1 << 2) | (1 << 3))
                + struct.pack('<BBHH', 0, 2, 2412, 0x00a0))
    ap, sta, bc = (bytes.fromhex('020000000000'), bytes.fromhex('020000000100'),
                   b'\xff' * 6)

    def hdr(fc0, flags, a1, a2, a3, qos=None):
        h = struct.pack('<BBH', fc0, flags, 0) + a1 + a2 + a3 + \
            struct.pack('<H', 0x100)
        return h + (struct.pack('<H', qos) if qos is not None else b'')

    def ie(i, d):
        return bytes([i, len(d)]) + d
    rsn = (struct.pack('<H', 1) + b'\x00\x0f\xac\x04' + struct.pack('<H', 1) +
           b'\x00\x0f\xac\x04' + struct.pack('<H', 1) + b'\x00\x0f\xac\x02' +
           b'\x00\x00')
    ies = (ie(0, b'tisubair') + ie(1, bytes([0x82, 0x84, 0x8b, 0x96])) +
           ie(3, b'\x01') + ie(48, rsn))

    def eapol(info, datalen):
        body = (struct.pack('>BHHQ', 2, info, 16, 1) + b'\x11' * 32 +
                b'\x00' * 32 + b'\x22' * 16 + struct.pack('>H', datalen) +
                b'\x33' * datalen)
        return struct.pack('>BBH', 2, 3, len(body)) + body

    def snap(et):
        return b'\xaa\xaa\x03\x00\x00\x00' + struct.pack('>H', et)
    arp = (struct.pack('>HHBBH', 1, 0x0800, 6, 4, 1) + sta +
           bytes([192, 168, 5, 10]) + b'\x00' * 6 + bytes([192, 168, 5, 1]))
    ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, 28, 1, 0, 64, 1, 0,
                     bytes([192, 168, 5, 10]), bytes([192, 168, 5, 1]))
    frames = [
        hdr(0x80, 0, bc, ap, ap) + struct.pack('<QHH', 1, 100, 0x0411) + ies,
        hdr(0x40, 0, bc, sta, bc) + ie(0, b'') + ie(1, b'\x02\x04'),
        hdr(0xb0, 0, ap, sta, ap) + struct.pack('<HHH', 0, 1, 0),
        hdr(0x00, 0, ap, sta, ap) + struct.pack('<HH', 0x0411, 10) +
        ie(0, b'tisubair'),
        hdr(0x10, 0, sta, ap, ap) + struct.pack('<HHH', 0x0411, 0, 0xc001),
        hdr(0x88, 2, sta, ap, ap, 7) + snap(0x888e) + eapol(0x008a, 0),
        hdr(0x88, 1, ap, sta, ap, 7) + snap(0x888e) + eapol(0x010a, 22),
        hdr(0x88, 2, sta, ap, ap, 7) + snap(0x888e) + eapol(0x13ca, 56),
        hdr(0x88, 1, ap, sta, ap, 7) + snap(0x888e) + eapol(0x030a, 0),
        hdr(0x88, 0x41, ap, sta, bc, 0) + b'\x01\x00\x00\x20\x00\x00\x00\x00' +
        b'\x99' * 40,
        hdr(0x08, 1, ap, sta, bc) + snap(0x0806) + arp,
        hdr(0x88, 1, ap, sta, ap, 0) + snap(0x0800) + ip +
        struct.pack('>BBHHH', 8, 0, 0, 1, 1),
        struct.pack('<BBH', 0xd4, 0, 0) + ap,
        hdr(0xc0, 0, sta, ap, ap) + struct.pack('<H', 3),
        hdr(0x48, 1, ap, sta, ap)]
    out = struct.pack('<IHHiIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 127)
    for i, f in enumerate(frames):
        p = rt() + f
        out += struct.pack('<IIII', 1000 + i, 0, len(p), len(p)) + p
    return out


class Ws:
    """A websocket client of webshark, on its unix socket."""

    def __init__(self, path):
        self.s = socket.socket(socket.AF_UNIX)
        self.s.settimeout(5)
        self.s.connect(path)
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall(("GET /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\n"
                        "Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\n"
                        "Sec-WebSocket-Version: 13\r\n\r\n" % key).encode())
        buf = b''
        while b'\r\n\r\n' not in buf:
            buf += self.s.recv(4096)
        self.buf = buf.split(b'\r\n\r\n', 1)[1]

    def send(self, obj):
        d = json.dumps(obj).encode()
        m = os.urandom(4)
        h = bytes([0x81]) + (bytes([0x80 | len(d)]) if len(d) < 126 else
                             bytes([0x80 | 126]) + struct.pack('>H', len(d)))
        self.s.sendall(h + m + bytes(b ^ m[i % 4] for i, b in enumerate(d)))

    def read(self, secs):
        out, end = [], time.time() + secs
        self.s.settimeout(0.3)
        while time.time() < end:
            try:
                c = self.s.recv(65536)
                if not c:
                    break
                self.buf += c
            except socket.timeout:
                pass
            while len(self.buf) >= 2:
                n, o = self.buf[1] & 0x7f, 2
                if n == 126:
                    n, o = struct.unpack('>H', self.buf[2:4])[0], 4
                elif n == 127:
                    n, o = struct.unpack('>Q', self.buf[2:10])[0], 10
                if len(self.buf) < o + n:
                    break
                out.append(json.loads(self.buf[o:o + n].decode()))
                self.buf = self.buf[o + n:]
        return out


def rows(msgs):
    return [p for m in msgs if m.get("type") == "packet_list"
            for p in m.get("packets", [])]


def labels(tree):
    out = []
    for n in tree:
        out.append(n.get("label", ""))
        out += labels(n.get("children", []))
    return out


def sub_webshark_wifi():
    binary = next((b for b in BIN if b and os.access(b, os.X_OK)), None)
    if not binary and TOY:
        # its binaries run inside the rootfs of the toy only
        print("      (toy: its webshark runs inside the toy only, not checked)")
        return
    if not binary:
        raise Fail("no cloonix-webshark in %s" % ", ".join(b for b in BIN if b))
    base = "/tmp/tisub_ws_%d" % os.getpid()
    sock, fifo = base + ".sock", base + "_pcap_fifo"
    for f in (sock, fifo):
        if os.path.exists(f):
            os.unlink(f)
    os.mkfifo(fifo)
    proc = subprocess.Popen([binary, "-f", sock, base],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    data = pcap()
    done = threading.Event()

    def write():
        # held open as dumpcap holds it: an end of file stops the capture
        with open(fifo, "wb") as f:
            f.write(data)
            f.flush()
            done.wait(30)
    threading.Thread(target=write, daemon=True).start()
    try:
        end = time.time() + 15
        while not os.path.exists(sock) and time.time() < end:
            time.sleep(0.2)
        ws = Ws(sock)
        msgs = ws.read(4)
        link = [m for m in msgs if m.get("type") == "link"]
        if not link or link[0].get("linktype") != 127:
            raise Fail("no link type 127 for the page: %r" % link)
        got = dict((r["num"], r["cols"]) for r in rows(msgs))
        if len(got) != 15:
            raise Fail("%d frames listed, 15 written" % len(got))
        want = {1: ("802.11", 'Beacon frame'), 2: ("802.11", "Probe Request"),
                3: ("802.11", "Authentication"),
                4: ("802.11", "Association Request"),
                5: ("802.11", "Association Response"),
                6: ("EAPOL", "Message 1 of 4"), 7: ("EAPOL", "Message 2 of 4"),
                8: ("EAPOL", "Message 3 of 4"), 9: ("EAPOL", "Message 4 of 4"),
                10: ("802.11", "(encrypted)"), 11: ("ARP", "Who has"),
                12: ("ICMP", "Echo (ping) request"),
                13: ("802.11", "Acknowledgement"),
                14: ("802.11", "Deauthentication"),
                15: ("802.11", "Null function")}
        for num, (proto, info) in want.items():
            if got[num][4] != proto or info not in got[num][6]:
                raise Fail("frame %d: %s %r, wanted %s %r"
                           % (num, got[num][4], got[num][6], proto, info))
        if 'SSID="tisubair"' not in got[1][6]:
            raise Fail("the beacon names no SSID: %r" % got[1][6])
        ws.send({"cmd": "select", "frame": 1})
        tree = [m for m in ws.read(2) if m.get("type") == "proto_tree"]
        if not tree:
            raise Fail("no tree for frame 1")
        text = labels(tree[0]["tree"])
        for want_label in ("Tag: SSID parameter set: tisubair",
                           "Type/Subtype: Beacon frame (0x08)",
                           "Auth Key Management (AKM) type: PSK (2)"):
            if want_label not in text:
                raise Fail("frame 1 tree lacks %r" % want_label)
        for expr, nums in ((DATA_FILTER, [10, 11, 12]), ("eapol", [6, 7, 8, 9]),
                           ("ip", [12])):
            ws.send({"cmd": "filter", "expr": expr})
            shown = [r["num"] for r in rows(ws.read(2))]
            if shown != nums:
                raise Fail("filter %s: frames %s, wanted %s"
                           % (expr, shown, nums))
            print("      filter %s: frames %s" % (expr, shown))
    finally:
        done.set()
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
        for f in glob.glob(base + "*"):
            os.unlink(f)


if __name__ == "__main__":
    run("webshark wifi", sub_webshark_wifi)
