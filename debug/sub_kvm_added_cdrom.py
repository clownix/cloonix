#!/usr/bin/env python3
"""sub_kvm_added_cdrom - added_cdrom=<file in bulk>: a second rom device in
the guest, of the size of the file, whose bytes at 20 KiB are the file's.
"""
import hashlib
import re
import os
from sublib import *

K = "tisubC"
ISO = "tisub_cd.iso"
SIZE = 4 * 1024 * 1024


def sub_kvm_added_cdrom():
    clean(K)
    path = os.path.join(BULK, ISO)
    try:
        show("dd if=/dev/urandom of=%s bs=1M count=4" % path)
        # zeros first: random bytes at 0 could look like a disk format to qemu
        data = bytes(2048) + os.urandom(SIZE - 2048)
        with open(path, "wb") as f:
            f.write(data)
        want = hashlib.md5(data[20480:20480 + 2048 * 10]).hexdigest()
        kvm(K, "added_cdrom=" + ISO)
        roms = ssh(K, "lsblk -b -d -n -o NAME,SIZE,TYPE | awk '$3==\"rom\"'")
        dev = [w.split()[0] for w in roms.splitlines()
               if w.split()[1:2] == [str(SIZE)]]
        if len(dev) != 1:
            raise Fail("no rom of %d bytes in the guest:\n%s" % (SIZE, roms))
        got = ssh(K, "dd if=/dev/%s bs=2048 skip=10 count=10 2>/dev/null | "
                  "md5sum" % dev[0]).split()[0]
        if got != want:
            raise Fail("the cdrom bytes differ from the file")
        if not re.search(r"added_cdrom=\S*%s" % ISO, dump_list()):
            raise Fail("dump commands does not show added_cdrom=")
        delete(K)
    finally:
        clean(K)
        if os.path.exists(path):
            os.unlink(path)


if __name__ == "__main__":
    run("added cdrom", sub_kvm_added_cdrom)
