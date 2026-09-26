#!/usr/bin/env python3
"""sub_web_items - every /ctl route the canvas uses that no other test drives,
posted as the page posts it, each checked by its effect on the net:
add-tap, add-nat, add-a2b, add-c2c, del-node; cnf-a2b on a cable carrying a
ping; snf on and off, wireshark-start, the webshark start and stop, and the
409 of an snf off under a running webshark; position and poseth read back;
add-vm, reboot-vm, save-vm on one kvm; add-phy when TESTITALL_PHY names a
spare interface. Then the client paths no test takes: add cisco, the flag
--no_qemu_ga it carries, -s host:port:passwd and --cfg, each with a wrong
password refused.
"""
import importlib.util
import json
import os
import re
import shutil
import subprocess
import tempfile
import time
from sublib import *

A, B, CABLE = "tisubWiA", "tisubWiB", "tisubWiab"
LAN_A, LAN_B = "tisubWilA", "tisubWilB"
TAP, NAT = "tisubWitap", "tisubWinat"
C2C = "tisubWic2c"
K, CI = "tisubWik", "tisubWici"
PHY_IF = os.environ.get("TESTITALL_PHY", "")
PATH, SEEN = shared_file("tisub_websave_%s.qcow2" % NET)
SUB = '{"fctident":"event_topo_sub","tid":0}'


class Page:
    """The https of the net, with the origin the page has."""

    def __init__(self):
        self.w = Web()
        self.origin = "https://" + self.w.host

    def post(self, route, body, want=200):
        st, out = self.w.request("POST", "/ctl/" + route, json.dumps(body),
                                 self.origin)
        if st != want:
            raise Fail("POST /ctl/%s %s: %d, wanted %d %s"
                       % (route, json.dumps(body), st, want, out.strip()))
        return out

    def get(self, route, want=200):
        st, out = self.w.request("GET", "/ctl/" + route, None, self.origin)
        if st != want:
            raise Fail("GET /ctl/%s: %d, wanted %d %s"
                       % (route, st, want, out.strip()))
        return out


def gone(*names, net=None):
    for n in names:
        wait_topo(n, False, net=net, timeout=60)


def web_items():
    clean(TAP, NAT, CABLE, C2C)
    p = Page()
    try:
        p.post("add-tap", {"name": TAP})
        p.post("add-nat", {"name": NAT})
        p.post("add-a2b", {"name": CABLE})
        for n in (TAP, NAT, CABLE):
            wait_topo(n, True)
        p.post("add-c2c", {"name": C2C, "dest": PEER})
        end = time.time() + 60
        while time.time() < end:
            if in_topo(C2C) and in_topo(C2C, PEER) and ti.c2c_peered(C2C, NET):
                break
            time.sleep(1)
        else:
            raise Fail("the c2c %s posted by the page is not peered within "
                       "60s" % C2C)
        for n in (TAP, NAT, CABLE, C2C):
            p.post("del-node", {"name": n})
        gone(TAP, NAT, CABLE, C2C)
        gone(C2C, net=PEER)
    finally:
        clean(TAP, NAT, CABLE, C2C)


def rtt():
    loss, avg = ping_stats(A, "10.61.0.2", 6)
    if loss or avg is None:
        raise Fail("the cable does not carry a ping: %s%% loss, rtt %s"
                   % (loss, avg))
    return avg


def web_cnf_a2b():
    clean(A, B, CABLE)
    p = Page()
    try:
        add_cnt(A, "s")
        add_cnt(B, "s")
        must(cli("add", "a2b", CABLE), "add a2b")
        for item, num, lan in ((A, "eth0", LAN_A), (CABLE, "A", LAN_A),
                               (B, "eth0", LAN_B), (CABLE, "B", LAN_B)):
            must(cli("plug", item, num, lan), "plug")
        wait_answer(A)
        wait_answer(B)
        address(A, 0, "10.61.0.1/24")
        address(B, 0, "10.61.0.2/24")
        base = rtt()
        p.post("cnf-a2b", {"name": CABLE, "dir": 0, "cmd": "nosuch",
                           "val": 1}, want=400)
        for d in (0, 1):
            p.post("cnf-a2b", {"name": CABLE, "dir": d, "cmd": "delay",
                               "val": 100})
        end = time.time() + 20
        while True:
            now = rtt()
            if now - base >= 120:
                break
            if time.time() > end:
                raise Fail("cnf-a2b delay 100 each way: the rtt went from "
                           "%.1f to %.1f ms, 120 more wanted" % (base, now))
            time.sleep(1)
        print("      cnf-a2b delay: rtt %.1f -> %.1f ms" % (base, now))
        for cmd in ("delay", "loss", "rate", "silentms"):
            for d in (0, 1):
                p.post("cnf-a2b", {"name": CABLE, "dir": d, "cmd": cmd,
                                   "val": 0})
        end = time.time() + 20
        while rtt() - base >= 120:
            if time.time() > end:
                raise Fail("cnf-a2b delay 0: the rtt did not come back")
            time.sleep(1)
    finally:
        clean(A, B, CABLE)


def sniff_live(timeout=30):
    end = time.time() + timeout
    while time.time() < end:
        s = Sniff(A, 0, "icmp")
        if s.ready(8):
            return s.start()
        time.sleep(1)
    raise Fail("snf on=1 by the page: sniff %s eth0 never went live" % A)


def sniff_refused(timeout=30):
    """on=0 by the page: the eth leaves dump commands."""
    line = "cnf %s eth0 sniff=on" % A
    end = time.time() + timeout
    while time.time() < end:
        if line not in dump_list():
            return
        time.sleep(1)
    raise Fail("snf on=0 by the page: %s is still in dump commands" % line)


def web_snf_webshark():
    clean(A, B)
    p = Page()
    sock = "sock_%s_0" % A
    try:
        add_cnt(A, "s")
        add_cnt(B, "s")
        for item in (A, B):
            must(cli("plug", item, "eth0", LAN_A), "plug")
        wait_answer(A)
        wait_answer(B)
        address(A, 0, "10.62.0.1/24")
        address(B, 0, "10.62.0.2/24")
        p.post("snf", {"name": A, "num": 0, "on": 1})
        s = sniff_live()
        ping(A, "10.62.0.2", 3)
        expect_count(s, 6, what="3 requests and 3 replies")
        p.post("wireshark-start", {"name": A, "num": 0})
        out = p.get("start?sock=%s&snf_sock=%s_0" % (sock, A))
        if "started" not in out:
            raise Fail("/ctl/start answered %r, not started" % out)
        p.post("snf", {"name": A, "num": 0, "on": 0}, want=409)
        out = p.get("stop?sock=%s" % sock)
        if "stopped" not in out:
            raise Fail("/ctl/stop answered %r, not stopped" % out)
        out = p.get("stop?sock=sock_tisub_nothing_0")
        if "none" not in out:
            raise Fail("/ctl/stop of no webshark answered %r, not none" % out)
        p.post("snf", {"name": A, "num": 0, "on": 0})
        sniff_refused()
    finally:
        clean(A, B)


def layout():
    pos, eth = {}, {}
    for m in raw_rpc(SUB):
        if m.get("fctident") == "item_pos":
            pos[m.get("name")] = (m.get("x"), m.get("y"))
        elif m.get("fctident") == "item_poseth":
            eth[(m.get("name"), m.get("num"))] = m.get("deg")
    return pos, eth


def web_position():
    clean(A)
    p = Page()
    try:
        add_cnt(A, "s")
        p.post("position", {"name": A, "x": -30, "y": 40})
        p.post("poseth", {"name": A, "num": 0, "deg": 270})
        end = time.time() + 15
        while True:
            pos, eth = layout()
            if pos.get(A) == (-30, 40) and eth.get((A, 0)) == 270:
                break
            if time.time() > end:
                raise Fail("position -30 40 / angle 270 of eth0 posted, read back "
                           "%s / %s" % (pos.get(A), eth.get((A, 0))))
            time.sleep(1)
    finally:
        clean(A)


def uptime():
    try:
        out = ssh(K, "cut -d. -f1 /proc/uptime", check=False, quiet=True,
                  timeout=20)
    except Fail:
        return None
    try:
        return int(out.strip().splitlines()[-1])
    except (ValueError, IndexError):
        return None


def size_settled(timeout=600):
    end, last, same = time.time() + timeout, -1, 0
    while time.time() < end:
        size = os.path.getsize(PATH) if os.path.exists(PATH) else -1
        same = same + 1 if (size > 0 and size == last) else 0
        if same >= 2:
            return size
        last = size
        time.sleep(5)
    raise Fail("save-vm: %s never settled in %ds" % (PATH, timeout))


def web_kvm():
    clean(K)
    if os.path.exists(PATH):
        os.unlink(PATH)
    p = Page()
    try:
        p.post("add-vm", {"name": K, "image": KVM_ROOTFS, "cpu": 1,
                          "mem": 1024, "nb_eth": 1, "eth": [{"type": "eths"}]})
        wait_topo(K, True, timeout=60)
        wait_answer(K, timeout=BOOT)
        end = time.time() + 60
        while (uptime() or 0) < 30:
            if time.time() > end:
                raise Fail("%s: no uptime of 30s to compare a reboot with" % K)
            time.sleep(5)
        before = uptime()
        p.post("reboot-vm", {"name": K})
        end = time.time() + BOOT
        while True:
            now = uptime()
            if now is not None and now < before:
                break
            if time.time() > end:
                raise Fail("reboot-vm: %s still up %ss after %ds"
                           % (K, now, BOOT))
            time.sleep(5)
        print("      reboot-vm: uptime %ds, then %ds" % (before, now))
        p.post("save-vm", {"name": K, "path": SEEN})
        print("      save-vm: %s, %d bytes" % (PATH, size_settled()))
        p.post("del-node", {"name": K})
        gone(K)
    finally:
        clean(K)
        if os.path.exists(PATH):
            os.unlink(PATH)


def web_phy():
    if not PHY_IF or PHY_IF == "eth0":
        print("      (no TESTITALL_PHY: add-phy not posted)")
        return
    obj = PHY_IF + "_1"
    clean(obj)
    p = Page()
    try:
        p.post("add-phy", {"name": PHY_IF, "type": "macvlan"})
        p.post("add-phy", {"name": PHY_IF, "type": "nosuch"}, want=400)
        end = time.time() + 30
        while time.time() < end and not in_topo(obj):
            time.sleep(1)
        if not in_topo(obj):
            print("      (macvlan on %s refused by the server, the route "
                  "answered 200)" % PHY_IF)
            return
        p.post("del-node", {"name": obj})
        gone(obj)
    finally:
        clean(obj)


def cli_cisco():
    clean(CI)
    try:
        must(cli("add", "cisco", CI, "ram=1024", "cpu=1", "eth=1",
                 KVM_ROOTFS, ack=180), "add cisco")
        line = [l for l in dump_list().splitlines()
                if re.search(r"add kvm %s " % CI, l)]
        if not line or "--no_qemu_ga" not in line[0]:
            raise Fail("add cisco: dump commands has %r, --no_qemu_ga wanted"
                       % (line[0] if line else "no line"))
        delete(CI)
    finally:
        clean(CI)


def client_path():
    env = os.environ.get("CLOONIX_CLIENT")
    path = env or shutil.which("cloonix")
    if not path:
        raise Fail("no cloonix client found")
    return os.path.realpath(path)


def net_entry():
    cfg = os.path.join(os.path.dirname(client_path()), "cloonix_config.py")
    spec = importlib.util.spec_from_file_location("tisub_cfg", cfg)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    for name, block in re.findall(r"CLOONIX_NET\s*:\s*(\S+)\s*\{(.*?)\}",
                                  mod.CONFIG, re.S):
        if name == NET:
            return dict(l.split(None, 1) for l in
                        (x.split("#", 1)[0].strip()
                         for x in block.splitlines()) if len(l.split()) == 2)
    raise Fail("%s is not in %s" % (NET, cfg))


def client(*args):
    show("cloonix " + " ".join(args))
    return subprocess.run(CLOONIX + list(args), capture_output=True,
                          text=True, stdin=subprocess.DEVNULL, timeout=60)


def cli_options():
    e = net_entry()
    ip, port, pw = e["cloonix_ip"], e["cloonix_port"], e["cloonix_passwd"]
    r = client("-s", "%s:%s:%s" % (ip, port, pw), "cli", "dump", "commands")
    if r.returncode:
        raise Fail("-s %s:%s:<passwd> cli dump commands: rc %d %s"
                   % (ip, port, r.returncode, (r.stdout + r.stderr).strip()))
    r = client("--wait", "5", "-s", "%s:%s:tisub_wrong" % (ip, port), "cli",
               "dump", "commands")
    if r.returncode == 0:
        raise Fail("-s with a wrong password answered as a good one")
    # the file of --cfg wins over the config beside the client: a wrong
    # password in it must fail, the right one pass
    fd, cfg = tempfile.mkstemp(prefix="tisub_cfg_", suffix=".cfg")
    os.close(fd)
    try:
        for word, ok in (("tisub_wrong", False), (pw, True)):
            with open(cfg, "w") as f:
                f.write("CLOONIX_NET: %s {\n  cloonix_ip %s\n  cloonix_port "
                        "%s\n  cloonix_passwd %s\n}\n" % (NET, ip, port, word))
            r = client("--wait", "5", "--cfg", cfg, "cli", NET, "dump", "commands")
            if (r.returncode == 0) != ok:
                raise Fail("--cfg with the %s password: rc %d %s"
                           % ("right" if ok else "wrong", r.returncode,
                              (r.stdout + r.stderr).strip()[-300:]))
    finally:
        os.unlink(cfg)
    r = client("--cfg", cfg, "cli", NET, "dump", "commands")
    if r.returncode == 0 or "no such config file" not in r.stdout + r.stderr:
        raise Fail("--cfg of a missing file: rc %d %s"
                   % (r.returncode, (r.stdout + r.stderr).strip()))

if __name__ == "__main__":
    ti.run_checks((("web add-tap add-nat add-a2b add-c2c del-node", web_items),
                   ("web cnf-a2b", web_cnf_a2b),
                   ("web snf, wireshark-start, webshark start stop",
                    web_snf_webshark),
                   ("web position poseth", web_position),
                   ("web add-vm reboot-vm save-vm", web_kvm),
                   ("web add-phy", web_phy),
                   ("add cisco, --no_qemu_ga", cli_cisco),
                   ("client -s and --cfg", cli_options)),
                  "sub_web_items on %s, peer %s" % (NET, PEER), "SUB OK")
