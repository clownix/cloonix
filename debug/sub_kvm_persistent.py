#!/usr/bin/env python3
"""sub_kvm_persistent - what --persistent does to a disk, and what its absence
does.

  kvm  --persistent: a marker written survives del and a second add on the
       same qcow2 (the vm runs on the file itself, no derived file);
       without it the same file gives a fresh overlay: the marker written
       there is gone at the next boot, and the marker the persistent run
       left is still visible (it is in the file).
  cnt  --persistent: the same with a copy of the image directory; and two
       refusals: a second persistent cnt on the image in use, a mix of
       persistent and not on one image.
Both run on copies in bulk, the shared images are not written.
"""
import os
import shutil
import subprocess
from sublib import *

PQ = "tisub_persist.qcow2"
PD = "tisub_persist_cnt"
K, C, C2 = "tisubP", "tisubPc", "tisubPc2"
MARK = "/root/tisub_marker"


def copy_image(src, dst):
    show("cp -a %s %s" % (src, dst))
    r = subprocess.run(["cp", "-a", "--reflink=auto", src, dst],
                       capture_output=True, text=True)
    if r.returncode:
        raise Fail("cp %s: %s" % (src, r.stderr.strip()))


def marker(node):
    return ssh(node, "cat %s 2>/dev/null; true" % MARK).strip()


def sub_kvm_persistent():
    clean(K, C, C2)
    q = os.path.join(BULK, PQ)
    d = os.path.join(BULK, PD)
    for p in (q, d):
        if os.path.exists(p):
            shutil.rmtree(p) if os.path.isdir(p) else os.unlink(p)
    copy_image(os.path.join(BULK, KVM_ROOTFS), q)
    cnt_image_copy(d)
    try:
        kvm(K, "--persistent", rootfs=PQ)
        if "--persistent" not in dump_list():
            raise Fail("dump commands does not show --persistent on %s" % K)
        ssh(K, "echo first > %s; sync" % MARK)
        delete(K)
        kvm(K, "--persistent", rootfs=PQ)
        if marker(K) != "first":
            raise Fail("the marker did not survive a persistent del/add")
        delete(K)
        kvm(K, rootfs=PQ)
        if marker(K) != "first":
            raise Fail("the file was written persistent, a fresh overlay "
                       "on it does not show the marker")
        ssh(K, "echo overlay > %s; sync" % MARK)
        delete(K)
        kvm(K, rootfs=PQ)
        if marker(K) != "first":
            raise Fail("a non persistent write survived a del/add: %s"
                       % marker(K))
        delete(K)

        add_cnt(C, "s", ("--persistent",), rootfs=PD)
        wait_answer(C)
        r = cli("add", "cnt", C2, "eth=1", PD, "--persistent", ack=40)
        if r.returncode == 0:
            raise Fail("a second persistent cnt on the image in use was "
                       "accepted")
        if "is in use by a --persistent container" not in r.stdout + r.stderr:
            raise Fail("the refusal does not say why: %s"
                       % (r.stdout + r.stderr).strip())
        r = cli("add", "cnt", C2, "eth=1", PD, ack=40)
        if r.returncode == 0:
            raise Fail("a non persistent cnt on an image in persistent use "
                       "was accepted")
        if "is in use by a --persistent container" not in r.stdout + r.stderr:
            raise Fail("the refusal does not say why: %s"
                       % (r.stdout + r.stderr).strip())
        ssh(C, "echo first > %s" % MARK)
        delete(C)
        add_cnt(C, "s", ("--persistent",), rootfs=PD)
        wait_answer(C)
        if marker(C) != "first":
            raise Fail("the cnt marker did not survive a persistent del/add")
        delete(C)
        add_cnt(C, "s", rootfs=PD)
        wait_answer(C)
        ssh(C, "echo overlay > %s" % MARK)
        delete(C)
        add_cnt(C, "s", rootfs=PD)
        wait_answer(C)
        if marker(C) != "first":
            raise Fail("a non persistent cnt write survived: %s" % marker(C))
        delete(C)
    finally:
        clean(K, C, C2)
        if os.path.exists(q):
            os.unlink(q)
        if os.path.isdir(d):
            shutil.rmtree(d, ignore_errors=True)


if __name__ == "__main__":
    run("kvm and cnt persistent", sub_kvm_persistent)
