#!/usr/bin/env python3
"""sub_host_clean - the host is as it was found: the cloonix processes, the
mounts under the work zone, the netns and the interfaces of the host are
listed with the net empty, a topology of every kind is built and removed with
del all, and the four lists are the same again. The fixed tests never checked
this; soak.py does it at its end.
"""
import subprocess
import time
from sublib import *

NAMES = ("tisubHa", "tisubHb", "tisubHk", "tisubHt", "tisubHn", "tisubHz")


def sh(*cmd):
    return subprocess.run(cmd, capture_output=True, text=True).stdout


def state():
    procs = set()
    for line in sh("ps", "-eo", "pid,args").splitlines():
        if "cloonix" in line and NET in line and "sub_host_clean" not in line:
            procs.add(" ".join(line.split()[1:])[:100])
    mounts = set()
    for line in open("/proc/self/mounts"):
        p = line.split()
        if len(p) > 1 and "/cloonix/" in p[1] and ("/%s/" % NET) in p[1]:
            mounts.add(p[1])
    netns = set(l.split()[0] for l in sh("ip", "netns", "list").splitlines()
                if l.strip())
    links = set(l.split(":")[1].strip().split("@")[0]
                for l in sh("ip", "-o", "link").splitlines())
    return procs, mounts, netns, links


def diff(before, after):
    out = []
    for label, b, a in zip(("process", "mount", "netns", "link"), before,
                           after):
        for x in sorted(a - b):
            out.append("%s left: %s" % (label, x))
    return out


def sub_host_clean():
    erase(NET)
    time.sleep(3)
    before = state()
    a, b, k, t, n, z = NAMES
    add_cnt(a, "ss")
    add_cnt(b, "s")
    must(cli("add", "kvm", k, "ram=1024", "cpu=1", "eth=1", KVM_ROOTFS),
         "add kvm")
    must(cli("add", "tap", t), "add tap")
    must(cli("add", "nat", n), "add nat")
    must(cli("add", "a2b", z), "add a2b")
    for x in (t, n, z):
        wait_topo(x, True)
    must(cli("plug", a, "eth0", "tisubH1"), "plug")
    must(cli("plug", b, "eth0", "tisubH1"), "plug")
    must(cli("plug", a, "eth1", "tisubH2"), "plug")
    must(cli("plug", z, "A", "tisubH2"), "plug")
    must(cli("plug", z, "B", "tisubH3"), "plug")
    must(cli("plug", k, "eth0", "tisubH3"), "plug")
    must(cli("plug", t, "tisubH3"), "plug")
    must(cli("plug", n, "tisubH4"), "plug")
    wait_answer(a)
    s = Sniff(a, 0, "").start()
    wait_answer(k, timeout=BOOT)
    s.stop()
    erase(NET)
    end = time.time() + 60
    left = diff(before, state())
    while time.time() < end and left:
        time.sleep(3)
        left = diff(before, state())
    if left:
        raise Fail("after del all the host is not as found:\n  "
                   + "\n  ".join(left))


if __name__ == "__main__":
    run("host clean after del all", sub_host_clean)
