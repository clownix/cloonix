#!/usr/bin/env python3
"""sub_trace_events - the trace subscriptions the gui lives on: crun and
nspace report a container as it is created, qemu reports a kvm, json_cli
reports every message; trace probe walks the pipe to a guest and finds it
whole.
"""
import subprocess
import threading
import time
from sublib import *

C, K = "tisubTc", "tisubTk"


class Trace:
    def __init__(self, kind):
        show("cloonix cli %s trace %s" % (NET, kind))
        self.kind = kind
        self.p = subprocess.Popen(CLOONIX + ["cli", NET, "trace", kind],
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True)
        self.out = []
        self.live = threading.Event()
        threading.Thread(target=self._drain, daemon=True).start()

    def _drain(self):
        for line in self.p.stdout:
            if line.startswith("# waiting"):
                self.live.set()
            self.out.append(line.rstrip("\n"))
        self.live.set()

    def start(self):
        self.live.wait(15)
        if self.p.poll() is not None:
            raise Fail("trace %s did not stay: %s"
                       % (self.kind, self.p.stderr.read()))
        return self

    def saw(self, name, timeout=60):
        end = time.time() + timeout
        while time.time() < end:
            if any(l.startswith("%s %s" % (self.kind, name)) for l in self.out):
                return True
            time.sleep(0.5)
        return False

    def stop(self):
        self.p.terminate()
        self.p.wait(5)


def sub_trace_events():
    clean(C, K)
    try:
        traces = [Trace(k).start() for k in ("crun", "nspace", "json_cli")]
        add_cnt(C, "s")
        wait_answer(C)
        for t in traces[:2]:
            if not t.saw(C):
                raise Fail("trace %s never reported %s" % (t.kind, C))
        if not any("cnt_add" in l and C in l for l in traces[2].out):
            raise Fail("trace json_cli did not carry the cnt_add of %s" % C)
        for t in traces:
            t.stop()
        t = Trace("qemu").start()
        must(cli("add", "kvm", K, "ram=1024", "cpu=1", "eth=1", KVM_ROOTFS),
             "add kvm")
        if not t.saw(K):
            raise Fail("trace qemu never reported %s" % K)
        t.stop()
        show("cloonix cli %s trace probe %s" % (NET, C))
        r = subprocess.run(CLOONIX + ["cli", NET, "trace", "probe", C],
                           capture_output=True, text=True, timeout=60)
        if r.returncode or "whole end to end" not in r.stdout:
            raise Fail("trace probe %s: rc %d\n%s%s"
                       % (C, r.returncode, r.stdout, r.stderr))
        wait_answer(K, timeout=BOOT)
        r = subprocess.run(CLOONIX + ["cli", NET, "trace", "probe", K],
                           capture_output=True, text=True, timeout=60)
        if r.returncode or "whole end to end" not in r.stdout:
            raise Fail("trace probe %s: rc %d\n%s%s"
                       % (K, r.returncode, r.stdout, r.stderr))
        delete(C)
        delete(K)
    finally:
        clean(C, K)


if __name__ == "__main__":
    run("trace events and probe", sub_trace_events)
