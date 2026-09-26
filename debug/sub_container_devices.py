#!/usr/bin/env python3
"""sub_container_devices - crun gives a container the base device nodes
always, and each host-dependent one on its own when the host has it. So the
base six are present, and every one of /dev/net/tun, /dev/kvm, /dev/vhost-net
and /dev/rfkill that exists on the host is present in the container too. A
host device missing from the container is the bug this guards -- it is what
the old all-or-nothing device gate did when the host lacked one of the set.
"""
import os
from sublib import *

C = "tisubDev"
BASE = ["null", "zero", "full", "tty", "random", "urandom"]
OPTIONAL = ["net/tun", "kvm", "vhost-net", "rfkill"]


def is_char(node):
    return "yes" in ssh(C, "test -c /dev/%s && echo yes || echo no" % node,
                        check=False)


def sub_container_devices():
    clean(C)
    try:
        add_cnt(C, "s")
        wait_answer(C)
        missing = [d for d in BASE if not is_char(d)]
        if missing:
            raise Fail("base device(s) missing in the container: %s"
                       % " ".join("/dev/" + d for d in missing))
        print("      base devices present: %s"
              % " ".join("/dev/" + d for d in BASE))
        for d in OPTIONAL:
            on_host = os.path.exists("/dev/" + d)
            in_cnt = is_char(d)
            print("      /dev/%-10s host %-3s container %s"
                  % (d, "yes" if on_host else "no", "yes" if in_cnt else "no"))
            if on_host and not in_cnt:
                raise Fail("/dev/%s is on the host but not in the container: "
                           "crun did not provision it (the per-device device "
                           "list is not working)" % d)
        delete(C)
    finally:
        clean(C)


if __name__ == "__main__":
    run("container devices", sub_container_devices)
