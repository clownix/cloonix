#!/usr/bin/env python3
"""sub_kvm_save_boot - save writes a qcow2 that boots: a marker written before
the save is read back from a kvm started on the saved file (an absolute path
is accepted as rootfs).
"""
import os
from sublib import *

K, K2 = "tisubS", "tisubS2"
PATH, SEEN = shared_file("tisub_saved_%s.qcow2" % NET)


def sub_kvm_save_boot():
    clean(K, K2)
    if os.path.exists(PATH):
        os.unlink(PATH)
    try:
        kvm(K)
        ssh(K, "echo saved > /root/tisub_marker; sync")
        must(cli("save", K, SEEN, ack=600), "save")
        if not os.path.exists(PATH):
            raise Fail("save answered ok and %s does not exist" % PATH)
        delete(K)
        kvm(K2, rootfs=SEEN)
        got = ssh(K2, "cat /root/tisub_marker").strip()
        if got != "saved":
            raise Fail("the saved qcow2 boots but the marker is %r" % got)
        delete(K2)
    finally:
        clean(K, K2)
        if os.path.exists(PATH):
            os.unlink(PATH)


if __name__ == "__main__":
    run("save then boot the saved qcow2", sub_kvm_save_boot)
