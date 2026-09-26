#!/usr/bin/env python3
"""sub_kvm_resources - ram= and cpu= are what the guest sees: nproc and
MemTotal, and dump commands echoes both.
"""
from sublib import *

K = "tisubRes"


def sub_kvm_resources():
    clean(K)
    try:
        kvm(K, ram=2048, cpu=3)
        n = int(ssh(K, "nproc").strip())
        if n != 3:
            raise Fail("cpu=3 and nproc says %d" % n)
        kb = int(ssh(K, "awk '/MemTotal/ {print $2}' /proc/meminfo").strip())
        if not 1700000 < kb < 2100000:
            raise Fail("ram=2048 and MemTotal is %d kB" % kb)
        if "add kvm %s ram=2048 cpu=3" % K not in dump_list():
            raise Fail("dump commands does not echo ram=2048 cpu=3")
        delete(K)
    finally:
        clean(K)


if __name__ == "__main__":
    run("kvm ram and cpu", sub_kvm_resources)
