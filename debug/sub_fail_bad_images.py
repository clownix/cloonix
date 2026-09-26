#!/usr/bin/env python3
"""sub_fail_bad_images - images that exist and are wrong: a qcow2 of zero
bytes, a qcow2 that is a text file, a rootfs that is a file instead of a
directory, an added_disk that does not exist, an added disc whose name is 290
characters long. Each refused with a reason,
nothing left in the topology, and the same name usable right after.
"""
import os
from sublib import *

K = "tisubBad"
Q0, QT, RF = "tisub_zero.qcow2", "tisub_text.qcow2", "tisub_notadir"


def refused(what, *cmd, says=None):
    r = cli(*cmd, ack=60)
    out = (r.stdout + r.stderr).strip()
    if r.returncode == 0:
        raise Fail("%s: accepted, client said %r, dump commands:\n%s"
                   % (what, out, dump_list()))
    if not out:
        raise Fail("%s: refused with no reason" % what)
    if says and says not in out:
        raise Fail("%s: the reason did not survive, wanted %r, got %r"
                   % (what, says, out))
    wait_topo(K, False, timeout=60)


def sub_fail_bad_images():
    clean(K)
    paths = [os.path.join(BULK, f) for f in (Q0, QT, RF)]
    try:
        open(paths[0], "wb").close()
        open(paths[1], "w").write("this is not a qcow2\n")
        open(paths[2], "w").write("this is not a rootfs\n")
        refused("zero byte qcow2", "add", "kvm", K, "ram=1024", "cpu=1",
                "eth=1", Q0)
        refused("text qcow2", "add", "kvm", K, "ram=1024", "cpu=1", "eth=1",
                QT)
        refused("file as rootfs", "add", "cnt", K, "eth=1", RF)
        refused("missing added_disk", "add", "kvm", K, "ram=1024", "cpu=1",
                "eth=1", "added_disk=tisub_no_such.img", KVM_ROOTFS)
        # a name as long as the rpc carries: the refusal must fit its buffer
        for opt in ("added_disk", "added_cdrom", "install_cdrom"):
            refused("%s of 290 characters" % opt, "add", "kvm", K, "ram=1024",
                    "cpu=1", "eth=1", "%s=%s.img" % (opt, "x" * 286),
                    KVM_ROOTFS, says="not found")
        # the same on the rootfs itself, the buffer the refusal is built in
        refused("kvm image of 289 characters", "add", "kvm", K, "ram=1024",
                "cpu=1", "eth=1", "%s.qcow2" % ("x" * 283), says="at most")
        refused("cnt image of 290 characters", "add", "cnt", K, "eth=1",
                "x" * 290, says="at most")
        # an absolute path is not a name and keeps its length: the reason
        # must still come back, the path elided
        longpath = os.path.join(BULK, "%s.qcow2"
                                % ("x" * max(64, 290 - len(BULK) - 8)))
        refused("kvm rootfs path of 290 characters", "add", "kvm", K,
                "ram=1024", "cpu=1", "eth=1", longpath, says="not found")
        must(cli("dump", "commands", quiet=True), "the server after long names")
        add_cnt(K, "s")
        wait_answer(K)
        delete(K)
    finally:
        clean(K)
        for p in paths:
            if os.path.exists(p):
                os.unlink(p)


if __name__ == "__main__":
    run("bad images", sub_fail_bad_images)
