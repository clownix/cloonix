#!/usr/bin/env python3
"""sub_kvm_fullvirt - --fullvirt: the root disk on the ide (sata on q35),
the guest boots and answers, its root device has transport sata, a ping on a
lan still crosses, dump commands echoes the flag.
"""
from sublib import *

K, C, L = "tisubF", "tisubFc", "tisubFlan"


def sub_kvm_fullvirt():
    clean(K, C)
    try:
        add_cnt(C, "s")
        kvm(K, "--fullvirt")
        tran = ssh(K, "lsblk -d -n -o NAME,TRAN")
        if "sata" not in tran:
            raise Fail("no sata disk in the guest:\n%s" % tran)
        if "--fullvirt" not in dump_list():
            raise Fail("dump commands does not show --fullvirt")
        must(cli("plug", K, "eth0", L), "plug")
        must(cli("plug", C, "eth0", L), "plug")
        wait_answer(C)
        address(K, 0, "10.21.0.1/24")
        address(C, 0, "10.21.0.2/24")
        ping(K, "10.21.0.2")
        delete(K)
        delete(C)
    finally:
        clean(K, C)


if __name__ == "__main__":
    run("fullvirt kvm", sub_kvm_fullvirt)
