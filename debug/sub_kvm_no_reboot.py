#!/usr/bin/env python3
"""sub_kvm_no_reboot - --no_reboot: a reboot from inside ends qemu, the server
sees the pid gone and the kvm leaves the topology with no qemu left for it.
"""
import subprocess
import time
from sublib import *

K = "tisubR"


def qemu_pids():
    r = subprocess.run(["pgrep", "-f", "qemu.*%s" % K], capture_output=True,
                       text=True)
    return r.stdout.split()


def sub_kvm_no_reboot():
    clean(K)
    try:
        kvm(K, "--no_reboot")
        if "--no_reboot" not in dump_list():
            raise Fail("dump commands does not show --no_reboot")
        ssh(K, "reboot", check=False, timeout=60)
        wait_topo(K, False, timeout=90)
        end = time.time() + 20
        while time.time() < end and qemu_pids():
            time.sleep(1)
        if qemu_pids():
            raise Fail("a qemu for %s is still running: %s" % (K, qemu_pids()))
        kvm(K)
        delete(K)
    finally:
        clean(K)


if __name__ == "__main__":
    run("no_reboot kvm", sub_kvm_no_reboot)
