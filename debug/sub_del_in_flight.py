#!/usr/bin/env python3
"""sub_del_in_flight - a delete thrown at an item right behind a plug of its
own: a cnt, a tap and a kvm deleted while the plug of their eth may still be
in flight. Both commands must be answered, ok or ko, never with the client's
NO ANSWER; the delete succeeds; neither the item nor the lan stays. Where the
delete lands in the plug is a matter of milliseconds, so each kind is tried
with the delay swept over the plug's round trip:

    DEL_TRIES_CNT, DEL_TRIES_TAP, DEL_TRIES_KVM   (default 10, 10, 3)
"""
import os
import re
import subprocess
import time
from sublib import *

L = "tisubDl"


def env_i(name, default):
    try:
        return int(os.environ.get(name, default))
    except ValueError:
        return default


TRIES = {"cnt": env_i("DEL_TRIES_CNT", 10), "tap": env_i("DEL_TRIES_TAP", 10),
         "kvm": env_i("DEL_TRIES_KVM", 3)}
NAMES = ["tisubD%s%d" % (k[0], i) for k in TRIES for i in range(TRIES[k])]


def lan_left():
    return re.search(r"(?<![A-Za-z0-9_])%s(?![A-Za-z0-9_])" % L, dump_list())


def race(name, itf, delay):
    """The plug in the background, the delete delay seconds behind it."""
    show("cloonix cli %s plug %s %s &" % (NET, " ".join([name] + itf), L))
    plug = subprocess.Popen(CLOONIX + ["cli", NET, "plug", name] + itf + [L],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, stdin=subprocess.DEVNULL)
    time.sleep(delay)
    must(cli("del", name), "del %s %.0f ms behind its plug" % (name, delay * 1000))
    try:
        out, err = plug.communicate(timeout=60)
    except subprocess.TimeoutExpired:
        plug.kill()
        raise Fail("the plug of %s never came back" % name)
    if "NO ANSWER" in err:
        raise Fail("the plug of %s was never answered:\n%s" % (name, err.strip()))
    if plug.returncode not in (0, 1):
        raise Fail("the plug of %s exited %d: %s"
                   % (name, plug.returncode, (err + out).strip()))
    wait_topo(name, False, timeout=60)
    end = time.time() + 30
    while lan_left():
        if time.time() > end:
            raise Fail("lan %s stayed after %s was deleted" % (L, name))
        time.sleep(1)
    return plug.returncode == 0


def sub_del_in_flight():
    clean(*NAMES)
    try:
        for kind in ("cnt", "tap", "kvm"):
            ok = 0
            for i in range(TRIES[kind]):
                name = "tisubD%s%d" % (kind[0], i)
                if kind == "cnt":
                    add_cnt(name, "v")
                elif kind == "tap":
                    must(cli("add", "tap", name), "add tap %s" % name)
                    wait_topo(name, True)
                else:
                    kvm(name, eth="v")
                delay = 0.01 * i if kind != "kvm" else 0.03 * i
                ok += race(name, [] if kind == "tap" else ["eth0"], delay)
            print("      %s: %d plug(s) done before the delete, %d refused by "
                  "it, all answered" % (kind, ok, TRIES[kind] - ok))
    finally:
        clean(*NAMES)


if __name__ == "__main__":
    run("delete behind a plug in flight", sub_del_in_flight)
