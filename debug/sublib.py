"""sublib - what every sub_*.py shares: the testitall helpers, a kvm that
boots, a cleanup of the names a sub uses, and the https of the web port.

    ./sub_<name>.py [net] [peer]          defaults: nemo mito

Not a test: it has no main.
"""
import base64
import json
import os
import socket
import ssl
import subprocess
import time

import testitall as ti
from testitall import (cli, must, ssh, Fail, show, wait_answer, wait_topo, in_bulk,
                       add_cnt, snf_on, address, ping, ping_stats, Sniff, expect_count, in_topo,
                       topo, ftopo, erase, cfg_value, sudo, sudo_ok,
                       host_port_free, host_connect, NET, PEER, CNT_ROOTFS,
                       KVM_ROOTFS, CLOONIX, TOY, shared_file, net_restart)

BULK = os.environ.get("TESTITALL_BULK", "/var/lib/cloonix/bulk")
BOOT = int(os.environ.get("TESTITALL_BOOT", "240"))


def run(name, fn):
    ti.run_checks([(name, fn)], "%s on %s, peer %s" % (name, NET, PEER),
                  "SUB OK")


def clean(*names):
    """Delete the names a sub uses, so it runs after a failed run too."""
    for n in names:
        if in_topo(n):
            cli("del", n, quiet=True)
    for n in names:
        wait_topo(n, False)


def images():
    return must(cli("dump", "images", quiet=True), "dump images").split()



def cnt_image_copy(dst, image=None):
    """A writable copy of a cnt image of bulk under a new name: cp -a of
    the directory when bulk holds one, an extraction of <image>.tar.gz
    as this user otherwise. Either is a directory --persistent takes."""
    image = image or CNT_ROOTFS
    src = os.path.join(BULK, image)
    tgz = src + ".tar.gz"
    if os.path.isdir(src):
        show("cp -a %s %s" % (src, dst))
        r = subprocess.run(["cp", "-a", "--reflink=auto", src, dst],
                           capture_output=True, text=True)
        if r.returncode:
            raise Fail("cp %s: %s" % (src, r.stderr.strip()))
    elif os.path.isfile(tgz):
        show("mkdir %s && tar --no-same-owner -xzf %s -C %s" % (dst, tgz, dst))
        os.mkdir(dst)
        r = subprocess.run(["tar", "--no-same-owner", "-xzf", tgz, "-C", dst],
                           capture_output=True, text=True)
        if r.returncode:
            raise Fail("tar %s: %s" % (tgz, r.stderr.strip()))
    else:
        raise Fail("%s is in bulk neither as a directory nor as a tar.gz"
                   % image)


def kvm(name, *args, eth="s", ram=1024, cpu=1, rootfs=None):
    """eth: one letter an eth, s sniffed, v not."""
    must(cli("add", "kvm", name, "ram=%d" % ram, "cpu=%d" % cpu,
             "eth=%d" % len(eth), *args, rootfs or KVM_ROOTFS),
         "add kvm %s" % name)
    wait_answer(name, timeout=BOOT)
    snf_on(name, eth)


def delete(name):
    must(cli("del", name), "del %s" % name)
    wait_topo(name, False, timeout=60)


def dump_list():
    return must(cli("dump", "commands", quiet=True), "dump commands")


def raw_rpc(msg, wait=3):
    """Every json the server sends back to one verbatim message."""
    show("cloonix --raw --wait %d cli %s rpc '%s'" % (wait, NET, msg))
    r = subprocess.run(CLOONIX + ["--raw", "--wait", str(wait), "cli",
                                  NET, "rpc", msg],
                       capture_output=True, text=True, stdin=subprocess.DEVNULL)
    out, dec, pos = [], json.JSONDecoder(), 0
    text = r.stdout
    while pos < len(text):
        while pos < len(text) and text[pos].isspace():
            pos += 1
        if pos >= len(text):
            break
        try:
            obj, pos = dec.raw_decode(text, pos)
        except ValueError:
            break
        out.append(obj)
    return out


class Web:
    """The https port of the net, as the browser uses it."""

    def __init__(self):
        self.port = cfg_value(NET, "cloonix_web")
        self.passwd = cfg_value(NET, "cloonix_passwd")
        if not self.port or not self.passwd:
            raise Fail("no cloonix_web or cloonix_passwd for %s in cloonix.cfg"
                       % NET)
        self.host = "127.0.0.1:%s" % self.port
        self.auth = base64.b64encode(("%s:%s" % (NET, self.passwd))
                                     .encode()).decode()

    def _sock(self):
        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        s = socket.create_connection(("127.0.0.1", int(self.port)), timeout=15)
        return ctx.wrap_socket(s)

    def request(self, method, path, body=None, origin=None, auth=True):
        """(status, body) of one request, connection closed after."""
        show("curl -k -X %s %s https://%s%s" % (method, "-d '%s'" % body
                                               if body else "", self.host, path))
        hdr = ["%s %s HTTP/1.1" % (method, path), "Host: " + self.host,
               "Connection: close"]
        if auth:
            hdr.append("Authorization: Basic " + self.auth)
        if origin:
            hdr.append("Origin: " + origin)
        data = (body or "").encode()
        if body is not None:
            hdr.append("Content-Length: %d" % len(data))
        s = self._sock()
        try:
            s.sendall(("\r\n".join(hdr) + "\r\n\r\n").encode() + data)
            got = b""
            while True:
                try:
                    chunk = s.recv(65536)
                except socket.timeout:
                    break
                if not chunk:
                    break
                got += chunk
        finally:
            s.close()
        head, _, rest = got.partition(b"\r\n\r\n")
        try:
            status = int(head.split()[1])
        except (IndexError, ValueError):
            raise Fail("%s %s: no http status in %r" % (method, path, got[:80]))
        return status, rest.decode(errors="replace")

    def websocket(self, path):
        """The first text frame of a websocket at path."""
        show("websocket https://%s%s" % (self.host, path))
        key = base64.b64encode(os.urandom(16)).decode()
        hdr = ["GET %s HTTP/1.1" % path, "Host: " + self.host,
               "Upgrade: websocket", "Connection: Upgrade",
               "Sec-WebSocket-Key: " + key, "Sec-WebSocket-Version: 13",
               "Authorization: Basic " + self.auth]
        s = self._sock()
        try:
            s.sendall(("\r\n".join(hdr) + "\r\n\r\n").encode())
            got = b""
            while b"\r\n\r\n" not in got:
                chunk = s.recv(4096)
                if not chunk:
                    raise Fail("websocket %s: closed during the handshake"
                               % path)
                got += chunk
            head, _, rest = got.partition(b"\r\n\r\n")
            if b" 101 " not in head.split(b"\r\n")[0]:
                raise Fail("websocket %s: %s" % (path, head.decode(errors="replace")))
            while len(rest) < 2:
                rest += s.recv(4096)
            op = rest[0] & 0x0f
            ln = rest[1] & 0x7f
            off = 2
            if ln == 126:
                while len(rest) < 4:
                    rest += s.recv(4096)
                ln = int.from_bytes(rest[2:4], "big")
                off = 4
            elif ln == 127:
                while len(rest) < 10:
                    rest += s.recv(4096)
                ln = int.from_bytes(rest[2:10], "big")
                off = 10
            while len(rest) < off + ln:
                chunk = s.recv(65536)
                if not chunk:
                    break
                rest += chunk
            if op != 1:
                raise Fail("websocket %s: first frame is opcode %d, not text"
                           % (path, op))
            return rest[off:off + ln].decode(errors="replace")
        finally:
            s.close()
