#!/usr/bin/env python3
"""sub_kvm_added_disk - added_disk=<raw file in bulk>: seen in the guest as
a 64 MiB block device (virtio, so not the scsi root), written at 1 MiB with
dd, the bytes are in the host file, and are read back after a second boot:
an added disk has no overlay.
"""
import hashlib
import re
import os
from sublib import *

K = "tisubD"
IMG = "tisub_disk.img"
MB = 1024 * 1024


def sub_kvm_added_disk():
    clean(K)
    path = os.path.join(BULK, IMG)
    try:
        show("dd if=/dev/zero of=%s bs=1M count=64" % path)
        with open(path, "wb") as f:
            f.truncate(64 * MB)
        kvm(K, "added_disk=" + IMG)
        dev = ssh(K, "lsblk -b -d -n -o NAME,SIZE | awk '$2==%d {print $1}'"
                  % (64 * MB)).split()
        if len(dev) != 1:
            raise Fail("no block device of 64 MiB in the guest: %s"
                       % ssh(K, "lsblk -b -d -n -o NAME,SIZE,TYPE"))
        dev = "/dev/" + dev[0]
        ssh(K, "head -c 1048576 /dev/urandom > /tmp/blk; "
               "dd if=/tmp/blk of=%s bs=1M seek=1 conv=fsync 2>/dev/null"
               % dev)
        want = ssh(K, "md5sum /tmp/blk").split()[0]
        with open(path, "rb") as f:
            f.seek(MB)
            host = hashlib.md5(f.read(MB)).hexdigest()
        if host != want:
            raise Fail("the bytes written to %s are not in %s" % (dev, path))
        delete(K)
        kvm(K, "added_disk=" + IMG)
        back = ssh(K, "dd if=%s bs=1M skip=1 count=1 2>/dev/null | md5sum"
                   % dev).split()[0]
        if back != want:
            raise Fail("the added disk does not carry the bytes at the second "
                       "boot")
        if not re.search(r"added_disk=\S*%s" % IMG, dump_list()):
            raise Fail("dump commands does not show added_disk=")
        delete(K)
    finally:
        clean(K)
        if os.path.exists(path):
            os.unlink(path)


if __name__ == "__main__":
    run("added disk", sub_kvm_added_disk)
