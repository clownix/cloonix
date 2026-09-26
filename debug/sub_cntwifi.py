#!/usr/bin/env python3
"""sub_cntwifi - radios and airs. wlan=N gives a container N mac80211_hwsim
radios, wlan0.. in iw dev, named so in plug; each starts on the
default air "cntwifi". A wlan is plugged into another air with plug and
unplugged with unplug, as an eth with a lan; an eth never goes on an air,
a wlan never on a lan of eths, and no item may be named cntwifi. dump json
carries the air of each radio, dump commands replays it; an air is born not
sniffed, cnf <air> sniff=on and off, dump commands replays it; each air has its
capture fifo snf/<air>_0 while a radio is on it; the canvas, the /topo
websocket, draws each wlan with etype wlan and a link to its air. The radios
are given back
after del. The module is the host's: loaded here with sudo when absent, the
test is skipped without it.
"""
import os
import time
from sublib import *

C, D = "tisubW", "tisubW2"
AIR2, ELAN = "tisubair2", "tisubWlan"
SYS = "/sys/devices/virtual/mac80211_hwsim"


def radios():
    try:
        return len(os.listdir(SYS))
    except OSError:
        return 0


def airs_of(name):
    for c in ftopo().get("cnt", []) or []:
        if c.get("name") == name:
            return c.get("vwif_air", "")
    raise Fail("%s is not in dump json" % name)


def fifo(air):
    return "/var/lib/cloonix/%s/snf/%s_0" % (NET, air)


def wait_path(path, present, secs=15):
    end = time.time() + secs
    while time.time() < end:
        if os.path.exists(path) == present:
            return True
        time.sleep(1)
    return False


def refused(r, words, what):
    said = (r.stdout + r.stderr).strip()
    if r.returncode == 0 or words not in said:
        raise Fail("%s: refused with \"%s\" wanted, got rc %d: %s"
                   % (what, words, r.returncode, said[-200:]))


def module_loaded():
    if os.path.isdir("/sys/module/mac80211_hwsim"):
        return True
    if not sudo_ok():
        print("      (mac80211_hwsim not loaded and no sudo: not checked)")
        return False
    # Not every kernel has the module (RHEL ships none): the wifi
    # emulation is optional, so is this check.
    r = sudo("modprobe", "mac80211_hwsim", "radios=0")
    said = (r.stderr or "") + (r.stdout or "")
    # "command not found" is a modprobe sudo could not find, not a
    # kernel without the module: that one fails below, in full.
    if (r.returncode != 0 and "not found" in said
            and "command not found" not in said):
        print("      (no mac80211_hwsim module in this kernel: not checked)")
        return False
    must(r, "modprobe")
    return True


def sub_cntwifi():
    clean(C, D)
    refused(cli("add", "cnt", "cntwifi", "eth=1", CNT_ROOTFS),
            "air of the cnt radios", "add cnt cntwifi")
    if not module_loaded():
        return
    before = radios()
    try:
        add_cnt(C, "s", ("wlan=1",))
        add_cnt(D, "s", ("wlan=2",))
        wait_answer(C)
        wait_answer(D)
        for n, wlans in ((C, ("wlan0",)), (D, ("wlan0", "wlan1"))):
            dev = ssh(n, "iw dev")
            for w in wlans:
                if w not in dev:
                    raise Fail("no %s in %s:\n%s" % (w, n, dev))
        if radios() != before + 3:
            raise Fail("%d radios on the host, %d before the add of 3"
                       % (radios(), before))
        if airs_of(C) != "cntwifi" or airs_of(D) != "cntwifi,cntwifi":
            raise Fail("default airs: %s %r, %s %r"
                       % (C, airs_of(C), D, airs_of(D)))

        # eth0 is 0, the wlans follow: C wlan0 is 1, D wlan1 is 2
        must(cli("plug", D, "wlan1", AIR2),
             "wlan1 of %s onto %s" % (D, AIR2))
        if airs_of(D) != "cntwifi," + AIR2:
            raise Fail("%s airs after the plug: %r" % (D, airs_of(D)))
        edges = ('{"name":"%s","num":1,"lan":"cntwifi"}' % C,
                 '{"name":"%s","num":2,"lan":"%s"}' % (D, AIR2))
        web, end = Web(), time.time() + 15
        # the topology reaches the gui server after a hysteresis
        while True:
            frame = web.websocket("/topo")
            if all(e in frame for e in edges) and '"wlan"' in frame:
                break
            if time.time() > end:
                raise Fail("the /topo of the canvas lacks the wlans on their "
                           "airs %s" % " ".join(edges))
            time.sleep(1)
        must(cli("plug", C, "eth0", ELAN),
             "eth0 of %s onto %s" % (C, ELAN))
        refused(cli("plug", D, "wlan0", ELAN), "a wlan goes on an air",
                "a wlan onto a lan of eths")
        refused(cli("plug", D, "eth0", AIR2), "an eth goes on a lan",
                "an eth onto an air")
        must(cli("unplug", C, "wlan0", "cntwifi"),
             "wlan0 of %s off cntwifi" % C)
        if airs_of(C) != "":
            raise Fail("%s airs after the del: %r" % (C, airs_of(C)))
        refused(cli("unplug", C, "wlan0", "cntwifi"), "is on no air",
                "a del of a wlan from an air it is not on")
        listed = dump_list()
        for line in ("plug %s wlan1 %s" % (D, AIR2),
                     "unplug %s wlan0 cntwifi" % C):
            if line not in listed:
                raise Fail("dump commands lacks \"%s\":\n%s" % (line, listed))
        if not TOY:
            for air in ("cntwifi", AIR2):
                if not wait_path(fifo(air), True):
                    raise Fail("the air %s has a radio and no %s"
                               % (air, fifo(air)))
        refused(cli("plug", C, "wlan1", AIR2), "has no wlan1",
                "a radio the container does not have")
        # an air is born not sniffed, sniff=on and off, in dump commands
        line = "cnf %s sniff=on" % AIR2
        if line in dump_list():
            raise Fail("the air %s is born sniffed" % AIR2)
        must(cli("cnf", AIR2, "sniff=on"), line)
        if line not in dump_list():
            raise Fail("dump commands lacks \"%s\"" % line)
        must(cli("cnf", AIR2, "sniff=off"), "cnf %s sniff=off" % AIR2)
        if line in dump_list():
            raise Fail("dump commands still has \"%s\" after off" % line)
        # the number past the eths, of 76 clients, still names a wlan
        raw_rpc('{"fctident": "del_lan_endp", "tid": 0, "name": "%s", '
                '"num": 2, "lan": "%s"}' % (D, AIR2))
        if airs_of(D) != "cntwifi,":
            raise Fail("%s airs after the del by number: %r" % (D, airs_of(D)))
        delete(C)
        delete(D)
        if radios() != before:
            raise Fail("%d radios on the host after the del, %d before"
                       % (radios(), before))
        if not TOY and not wait_path(fifo(AIR2), False):
            raise Fail("no radio on %s and %s is still there"
                       % (AIR2, fifo(AIR2)))
    finally:
        clean(C, D)


if __name__ == "__main__":
    run("cntwifi", sub_cntwifi)
