#!/usr/bin/env python3
"""sub_kvmwifi - radios and airs of the kvm, the sub_cntwifi of the kvm.
wlan=N gives a kvm N mac80211_hwsim radios made in the guest by the
cloonix agent, wlan0.. in iw dev, named so in plug; each starts on
the default air "kvmwifi". A wlan is plugged into another air with plug
and unplugged with unplug, as an eth with a lan; an eth never goes on an
air, a wlan never on a lan of eths, no item may be named kvmwifi, and an
air holds kvm and cnt radios alike: a kvm radio goes on cntwifi. dump json
carries the air of each radio, dump commands replays it, and each air has its
capture fifo snf/<air>_0; the canvas, the /topo websocket, draws each wlan
with etype wlan and a link to its air. Two radios on one air ping each
other and the ping is captured on the air; off the air, the ping fails. The
kvm radios need nothing on the host; with mac80211_hwsim of the host, a cnt
radio put on the air of a kvm radio pings it both ways, captured there.
"""
import os
import subprocess
import threading
import time
from sublib import *

C, D, E = "tisubK", "tisubK2", "tisubKc"
AIR2, ELAN = "tisubkair2", "tisubKlan"
ADDR = {C: "10.47.0.1", D: "10.47.0.2", E: "10.47.0.3"}


def airs_of(name):
    for k in ftopo().get("kvm", []) or []:
        if k.get("name") == name:
            return k.get("vwif_air", "")
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


def wait_wlans(node, wlans, secs=60):
    """The agent makes the radios a few seconds after the guest answers."""
    end = time.time() + secs
    while True:
        dev = ssh(node, "iw dev", check=False)
        if all(w in dev for w in wlans):
            return
        if time.time() > end:
            raise Fail("no %s in %s after %d s:\n%s"
                       % (" ".join(wlans), node, secs, dev))
        time.sleep(2)


def cell(node, iface):
    out = ssh(node, "iw dev %s link" % iface, check=False).split()
    return out[out.index("IBSS") + 1] if "IBSS" in out else ""


def adhoc(node, iface):
    ssh(node, "iw dev %s set type ibss" % iface)
    ssh(node, "ip link set dev %s up" % iface)
    ssh(node, "iw dev %s ibss join tisubkcell 2412" % iface)
    ssh(node, "ip addr add %s/24 dev %s" % (ADDR[node], iface))


def wait_cell(node, iface, want="", secs=40):
    end = time.time() + secs
    while time.time() < end:
        got = cell(node, iface)
        if got and (not want or got == want):
            return got
        time.sleep(2)
    return cell(node, iface)


def icmp_on_air(air, secs, during):
    """ICMP frames of the air while during() runs: radiotap, then 802.11 data
    with LLC/SNAP, then IPv4 of protocol 1. An air is born not sniffed."""
    must(cli("cnf", air, "sniff=on"), "cnf %s sniff=on" % air)
    show("cloonix cli %s sniff %s --line" % (NET, air))
    p = subprocess.Popen(CLOONIX + ["cli", NET, "sniff", air, "--line"],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         text=True, stdin=subprocess.DEVNULL)
    lines = []
    reader = threading.Thread(target=lambda: lines.extend(p.stdout),
                              daemon=True)
    reader.start()
    try:
        time.sleep(1)
        during()
        time.sleep(secs)
    finally:
        p.terminate()
    try:
        p.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        p.kill()
    reader.join(2)
    if not any(ln.startswith("hdr ") for ln in lines):
        raise Fail("the air %s is not captured" % air)
    nb = 0
    for ln in lines:
        f = ln.split()
        if len(f) < 5 or f[0] != "pkt":
            continue
        b = bytes.fromhex(f[4])
        rt = b[2] | (b[3] << 8) if len(b) > 4 else len(b)
        w = b[rt:]
        if len(w) < 24 or ((w[0] >> 2) & 3) != 2 or (w[1] & 0x40):
            continue
        off = 24 + (2 if (w[0] >> 4) & 8 else 0)
        if w[off:off + 3] == b"\xaa\xaa\x03" and \
           w[off + 6:off + 8] == b"\x08\x00" and len(w) > off + 17 and \
           w[off + 17] == 1:
            nb += 1
    return nb


def host_module():
    """The cnt radios need mac80211_hwsim of the host; best effort here, the
    ping between a kvm radio and a cnt radio is skipped without it."""
    if os.path.isdir("/sys/module/mac80211_hwsim"):
        return True
    if not sudo_ok():
        return False
    return sudo("modprobe", "mac80211_hwsim", "radios=0").returncode == 0


def sub_kvmwifi():
    clean(C, D, E)
    refused(cli("add", "kvm", "kvmwifi", "ram=1024", "cpu=1", "eth=1",
                KVM_ROOTFS), "air of the kvm radios", "add kvm kvmwifi")
    refused(cli("add", "kvm", C, "ram=1024", "cpu=1", "eth=1", "wlan=5",
                KVM_ROOTFS), "radios at most", "add kvm wlan=5")
    refused(cli("add", "kvm", C, "ram=1024", "cpu=1", "eth=1", "wlan=1",
                "--no_qemu_ga", KVM_ROOTFS), "cloonix agent of a linux guest",
            "add kvm wlan= --no_qemu_ga")
    try:
        kvm(C, "wlan=1")
        kvm(D, "wlan=2")
        wait_wlans(C, ("wlan0",))
        wait_wlans(D, ("wlan0", "wlan1"))
        if airs_of(C) != "kvmwifi" or airs_of(D) != "kvmwifi,kvmwifi":
            raise Fail("default airs: %s %r, %s %r"
                       % (C, airs_of(C), D, airs_of(D)))

        # eth0 is 0, the wlans follow: C wlan0 is 1, D wlan1 is 2
        must(cli("plug", D, "wlan1", AIR2),
             "wlan1 of %s onto %s" % (D, AIR2))
        if airs_of(D) != "kvmwifi," + AIR2:
            raise Fail("%s airs after the plug: %r" % (D, airs_of(D)))
        edges = ('{"name":"%s","num":1,"lan":"kvmwifi"}' % C,
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
        must(cli("plug", D, "wlan0", "cntwifi"),
             "wlan0 of %s onto cntwifi" % D)
        if airs_of(D) != "cntwifi," + AIR2:
            raise Fail("%s airs on cntwifi: %r" % (D, airs_of(D)))
        must(cli("plug", D, "wlan0", "kvmwifi"),
             "wlan0 of %s onto kvmwifi" % D)
        must(cli("unplug", C, "wlan0", "kvmwifi"),
             "wlan0 of %s off kvmwifi" % C)
        if airs_of(C) != "":
            raise Fail("%s airs after the del: %r" % (C, airs_of(C)))
        refused(cli("unplug", C, "wlan0", "kvmwifi"), "is on no air",
                "a del of a wlan from an air it is not on")
        listed = dump_list()
        for line in ("plug %s wlan1 %s" % (D, AIR2),
                     "unplug %s wlan0 kvmwifi" % C,
                     "wlan=2"):
            if line not in listed:
                raise Fail("dump commands lacks \"%s\":\n%s" % (line, listed))
        if not TOY:
            for air in ("kvmwifi", AIR2):
                if not wait_path(fifo(air), True):
                    raise Fail("the air %s has a radio and no %s"
                               % (air, fifo(air)))

        # the medium: C wlan0 and D wlan1 on AIR2, one ad hoc cell, a ping
        must(cli("plug", C, "wlan0", AIR2),
             "wlan0 of %s onto %s" % (C, AIR2))
        adhoc(C, "wlan0")
        first = wait_cell(C, "wlan0")
        if not first:
            raise Fail("%s made no ad hoc cell" % C)
        adhoc(D, "wlan1")
        if wait_cell(D, "wlan1", first) != first:
            raise Fail("%s wlan1 is in the cell %r, not %s of %s: the frames "
                       "of %s do not reach it on %s"
                       % (D, cell(D, "wlan1"), first, C, C, AIR2))
        ok = False
        end = time.time() + 30
        while time.time() < end and not ok:
            ok = " 0% packet loss" in ssh(C, "ping -c 3 -W 2 %s" % ADDR[D],
                                          check=False)
        if not ok:
            raise Fail("%s does not ping %s on the air %s" % (C, D, AIR2))
        nb = icmp_on_air(AIR2, 4, lambda: ssh(C, "ping -c 5 %s" % ADDR[D],
                                               check=False))
        print("      %d ICMP frames on the air %s" % (nb, AIR2))
        if nb < 8:
            raise Fail("%d ICMP frames captured on %s, 10 sent" % (nb, AIR2))
        must(cli("unplug", D, "wlan1", AIR2),
             "wlan1 of %s off %s" % (D, AIR2))
        if " 0% packet loss" in ssh(C, "ping -c 3 -W 1 %s" % ADDR[D],
                                    check=False):
            raise Fail("%s still pings %s with wlan1 on no air" % (C, D))

        # a cnt radio on the air of C wlan0: the kvm and the cnt ping
        if host_module():
            add_cnt(E, "s", ("wlan=1",))
            wait_answer(E)
            must(cli("plug", E, "wlan0", AIR2),
                 "wlan0 of %s onto %s" % (E, AIR2))
            adhoc(E, "wlan0")
            if wait_cell(E, "wlan0", first) != first:
                raise Fail("%s wlan0 is in the cell %r, not %s of %s: the "
                           "frames of the kvm do not reach the cnt on %s"
                           % (E, cell(E, "wlan0"), first, C, AIR2))
            for a, b in ((C, E), (E, C)):
                ok = False
                end = time.time() + 30
                while time.time() < end and not ok:
                    ok = " 0% packet loss" in ssh(a, "ping -c 3 -W 2 %s"
                                                  % ADDR[b], check=False)
                if not ok:
                    raise Fail("%s does not ping %s on the air %s"
                               % (a, b, AIR2))
            nb = icmp_on_air(AIR2, 4, lambda: ssh(E, "ping -c 5 %s" % ADDR[C],
                                                   check=False))
            print("      %d ICMP frames of a cnt and a kvm on the air %s"
                  % (nb, AIR2))
            if nb < 8:
                raise Fail("%d ICMP frames captured on %s, 10 sent"
                           % (nb, AIR2))
            delete(E)
        else:
            print("      (no mac80211_hwsim on the host: the ping between a "
                  "kvm and a cnt not checked)")
        delete(C)
        delete(D)
    finally:
        clean(C, D, E)


if __name__ == "__main__":
    run("kvmwifi", sub_kvmwifi)
