#!/usr/bin/env python3
"""sub_webshark_voip - webshark on a call: a pcap of ethernet frames of a SIP
call, served on the capture socket webshark subscribes to, <snf>_pcap_sock,
as the fan-out of a dumpcap serves it. Checked on its websocket: SIP over
UDP, the SDP of the INVITE and of its 200 setting up the RTP and RTCP
streams on the ports they give, the voice decoded as RTP with the name of
its payload type, a DTMF key as an RTP event through the dynamic type the
SDP names telephone-event, a sender report of RTCP, the labels of the tree,
and the display filters of the asterisk toy.
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

BIN = [os.environ.get("TESTITALL_WEBSHARK", ""),
       "/usr/libexec/cloonix/cloonfs/bin/jscloonix/cloonix-webshark"]
PHONE, PBX = "10.0.0.2", "10.0.0.1"
PHONE_RTP, PBX_RTP = 10002, 10004


def frame(src, dst, sport, dport, payload, t):
    """An ethernet, IPv4, UDP frame as a pcap record at time t."""
    udp = struct.pack("!HHHH", sport, dport, 8 + len(payload), 0) + payload
    ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + len(udp), 1, 0, 64, 17, 0,
                     socket.inet_aton(src), socket.inet_aton(dst))
    eth = bytes.fromhex("020000000001020000000002") + b"\x08\x00"
    f = eth + ip + udp
    return struct.pack("<IIII", int(t), int((t % 1) * 1e6), len(f), len(f)) + f


def sip(first, cseq, body=""):
    head = ("%s\r\nVia: SIP/2.0/UDP %s:5060;branch=z9hG4bK1\r\n"
            "From: <sip:6001@%s>;tag=a1\r\nTo: <sip:100@%s>\r\n"
            "Call-ID: tisubcall@%s\r\nCSeq: %s\r\n"
            "Contact: <sip:6001@%s:5060>\r\n" % (first, PHONE, PBX, PBX,
                                                PHONE, cseq, PHONE))
    if body:
        head += "Content-Type: application/sdp\r\n"
    return (head + "Content-Length: %d\r\n\r\n%s" % (len(body), body)).encode()


def sdp(addr, port):
    return ("v=0\r\no=- 1 1 IN IP4 %s\r\ns=-\r\nc=IN IP4 %s\r\nt=0 0\r\n"
            "m=audio %d RTP/AVP 0 101\r\na=rtpmap:0 PCMU/8000\r\n"
            "a=rtpmap:101 telephone-event/8000\r\n" % (addr, addr, port))


def rtp(pt, seq, ts, ssrc, payload, mark=False):
    return struct.pack("!BBHII", 0x80, (0x80 if mark else 0) | pt, seq, ts,
                       ssrc) + payload


def pcap():
    """10 frames: the numbers the checks below expect."""
    out = [struct.pack("<IHHiIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1)]
    req = "INVITE sip:100@%s SIP/2.0" % PBX
    calls = [
        (PHONE, PBX, 5060, 5060, sip(req, "1 INVITE", sdp(PHONE, PHONE_RTP))),
        (PBX, PHONE, 5060, 5060, sip("SIP/2.0 100 Trying", "1 INVITE")),
        (PBX, PHONE, 5060, 5060, sip("SIP/2.0 200 OK", "1 INVITE",
                                     sdp(PBX, PBX_RTP))),
        (PHONE, PBX, 5060, 5060, sip("ACK sip:100@%s SIP/2.0" % PBX, "1 ACK")),
        (PHONE, PBX, PHONE_RTP, PBX_RTP,
         rtp(0, 1, 160, 0x11111111, b"\xff" * 160, mark=True)),
        (PBX, PHONE, PBX_RTP, PHONE_RTP,
         rtp(0, 7, 320, 0x22222222, b"\xff" * 160)),
        (PHONE, PBX, PHONE_RTP, PBX_RTP,
         rtp(101, 2, 480, 0x11111111, struct.pack("!BBH", 1, 10, 160), mark=True)),
        (PHONE, PBX, PHONE_RTP + 1, PBX_RTP + 1,
         struct.pack("!BBHIIIIII", 0x80, 200, 6, 0x11111111, 1, 2, 480, 3, 480)
         + struct.pack("!BBHIBB", 0x81, 202, 2, 0x11111111, 1, 2) + b"ab"),
        (PHONE, PBX, 5060, 5060, sip("BYE sip:100@%s SIP/2.0" % PBX, "2 BYE")),
        (PBX, PHONE, 5060, 5060, sip("SIP/2.0 200 OK", "2 BYE")),
    ]
    for i, (src, dst, sp, dp, payload) in enumerate(calls):
        out.append(frame(src, dst, sp, dp, payload, 1000 + i * 0.02))
    return b"".join(out)


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


def sub_webshark_voip():
    binary = next((b for b in BIN if b and os.access(b, os.X_OK)), None)
    if not binary and TOY:
        # its binaries run inside the rootfs of the toy only
        print("      (toy: its webshark runs inside the toy only, not checked)")
        return
    if not binary:
        raise Fail("no cloonix-webshark in %s" % ", ".join(b for b in BIN if b))
    base = "/tmp/tisub_wsv_%d" % os.getpid()
    sock, feed = base + ".sock", base + "_pcap_sock"
    for f in (sock, feed):
        if os.path.exists(f):
            os.unlink(f)
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(feed)
    srv.listen(1)
    proc = subprocess.Popen([binary, "-f", sock, base],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    data = pcap()
    done = threading.Event()

    def serve():
        # one subscriber, held open as the fan-out holds it: a close stops
        # the capture
        srv.settimeout(20)
        try:
            conn, _a = srv.accept()
        except OSError:
            return
        with conn:
            conn.sendall(data)
            done.wait(30)
    threading.Thread(target=serve, daemon=True).start()
    try:
        end = time.time() + 15
        while not os.path.exists(sock) and time.time() < end:
            time.sleep(0.2)
        ws = Ws(sock)
        msgs = ws.read(4)
        got = dict((r["num"], r["cols"]) for r in rows(msgs))
        if len(got) != 10:
            raise Fail("%d frames listed, 10 written" % len(got))
        want = {1: ("SIP/SDP", "Request: INVITE sip:100@10.0.0.1"),
                2: ("SIP", "Status: 100 Trying (INVITE)"),
                3: ("SIP/SDP", "Status: 200 OK (INVITE)"),
                4: ("SIP", "Request: ACK sip:100@10.0.0.1"),
                5: ("RTP", "PT=ITU-T G.711 PCMU"),
                6: ("RTP", "PT=ITU-T G.711 PCMU"),
                7: ("RTP EVENT", "DTMF One 1"),
                8: ("RTCP", "Sender Report"),
                9: ("SIP", "Request: BYE sip:100@10.0.0.1"),
                10: ("SIP", "Status: 200 OK (BYE)")}
        for num, (proto, info) in want.items():
            if got[num][4] != proto or info not in got[num][6]:
                raise Fail("frame %d: %s %r, wanted %s %r"
                           % (num, got[num][4], got[num][6], proto, info))
        for num, want_labels in (
                (1, ("Method: INVITE", "Call-ID: tisubcall@10.0.0.2",
                     "Media Description, name and address (m): audio 10002 RTP/AVP 0 101",
                     "RTP stream to 10.0.0.2 port 10002 set up, RTCP port 10003")),
                (5, ("Stream setup by SDP (frame 3)", "Sequence number: 1")),
                (8, ("Sender SSRC: 0x11111111 (286331153)",
                     "Text: ab"))):
            ws.send({"cmd": "select", "frame": num})
            tree = [m for m in ws.read(2) if m.get("type") == "proto_tree"]
            if not tree:
                raise Fail("no tree for frame %d" % num)
            text = labels(tree[0]["tree"])
            for want_label in want_labels:
                if want_label not in text:
                    raise Fail("frame %d tree lacks %r" % (num, want_label))
        for expr, nums in (('sip.Method == "INVITE"', [1]),
                           ("sip.Status-Code == 200", [3, 10]),
                           ("sip.CSeq.method == \"BYE\"", [9, 10]),
                           ("rtp.p_type == 0", [5, 6]),
                           ("rtpevent.event_id == 1", [7]),
                           ("rtcp.pt == 200", [8]),
                           ("sdp", [1, 3])):
            ws.send({"cmd": "filter", "expr": expr})
            shown = [r["num"] for r in rows(ws.read(2))]
            if shown != nums:
                raise Fail("filter %s: frames %s, wanted %s"
                           % (expr, shown, nums))
            print("      filter %s: frames %s" % (expr, shown))
    finally:
        done.set()
        srv.close()
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
        for f in glob.glob(base + "*"):
            os.unlink(f)


if __name__ == "__main__":
    run("webshark voip", sub_webshark_voip)
