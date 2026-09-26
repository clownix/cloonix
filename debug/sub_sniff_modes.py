#!/usr/bin/env python3
"""sub_sniff_modes - the three sniff outputs testitall does not read: --line
gives one hex body per packet that decodes to caplen bytes and carries the
pinged address, --dump gives the pcap header line and a hex dump per packet,
--count prints a rate line per second.
"""
import subprocess
import threading
import time
from sublib import *

A, B, L = "tisubSa", "tisubSb", "tisubSl"


class Raw:
    def __init__(self, mode, filt):
        show("cloonix cli %s sniff %s eth0 %s %s" % (NET, B, mode, filt))
        cmd = CLOONIX + ["cli", NET, "sniff", B, "eth0", mode] + filt.split()
        self.p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True)
        self.lines = []
        self.live = threading.Event()
        threading.Thread(target=self._drain, daemon=True).start()

    def _drain(self):
        for line in self.p.stdout:
            if line.startswith("# waiting"):
                self.live.set()
            self.lines.append(line.rstrip("\n"))
        self.live.set()

    def start(self):
        self.live.wait(15)
        if self.p.poll() is not None:
            raise Fail("sniff did not stay: %s" % self.p.stderr.read())
        time.sleep(1)
        return self

    def stop(self):
        time.sleep(1.5)
        self.p.terminate()
        self.p.wait(5)
        return self.lines


def sub_sniff_modes():
    clean(A, B)
    try:
        add_cnt(A, "s")
        add_cnt(B, "s")
        must(cli("plug", A, "eth0", L), "plug")
        must(cli("plug", B, "eth0", L), "plug")
        wait_answer(A)
        wait_answer(B)
        address(A, 0, "10.22.0.1/24")
        address(B, 0, "10.22.0.2/24")
        ping(A, "10.22.0.2")

        s = Raw("--line", "icmp").start()
        ping(A, "10.22.0.2", 3)
        lines = [l for l in s.stop() if l.startswith("pkt ")]
        if len(lines) < 4:
            raise Fail("--line printed %d pkt lines" % len(lines))
        for l in lines:
            f = l.split()
            caplen = int(f[3].split("/")[0])
            body = bytes.fromhex(f[4])
            if len(body) != caplen:
                raise Fail("--line: %d hex bytes for caplen %d" % (len(body),
                                                                 caplen))
            if bytes([10, 22, 0, 2]) not in body:
                raise Fail("--line: no 10.22.0.2 in an icmp packet")

        s = Raw("--dump", "icmp").start()
        ping(A, "10.22.0.2", 3)
        lines = s.stop()
        if not any(l.startswith("# pcap file header, link type") for l in lines):
            raise Fail("--dump printed no pcap header line")
        heads = [l for l in lines if l.startswith("No. ")]
        hexs = [l for l in lines if l.startswith("0000   ")]
        if len(heads) < 4 or len(hexs) < 4:
            raise Fail("--dump printed %d headings and %d hex lines"
                       % (len(heads), len(hexs)))

        s = Raw("--count", "").start()
        ping(A, "10.22.0.2", 5)
        lines = [l for l in s.stop() if "pkt/s" in l]
        if not lines:
            raise Fail("--count printed no rate line")
        if not any(int(l.split()[0]) > 0 for l in lines):
            raise Fail("--count never saw a packet: %s" % lines[:3])
        delete(A)
        delete(B)
    finally:
        clean(A, B)


if __name__ == "__main__":
    run("sniff --line --dump --count", sub_sniff_modes)
