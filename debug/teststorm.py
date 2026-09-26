#!/usr/bin/env python3
r"""teststorm - flows on a moving target: iperf3 never stops while the
topology around it and under it is torn and rebuilt at random.

    ./teststorm.py [net] [peer]          defaults: nemo mito

Two nets up and the image flow_trixie in bulk (vm_create/cnt_kvm_demo/
cnt_flow_trixie.sh); trixie for the bystanders, trixie.qcow2 for the one kvm
(skipped without it). Both nets are erased first. TESTSTORM_SECS (240) is the
length of the storm, TESTSTORM_SEED the dice (printed, so a run repeats).

The standing flows, each an iperf3 client that reconnects every 10 s until
the storm is over and counts what got through:

  A -> B    tcp over the lan, sniffed ports
  C -> D    udp 100 Mbit/s over the same lan
  A -> B    tcp over a second lan on not sniffed ports
  A -> P    tcp across the c2c to the peer net

The dice, thrown until the time is up, one of:

  sniff read      a sniff read a few seconds on any of the flowing ports,
                  a random filter, delivered and missing counted
  sniff unread    a sniff whose client never reads, killed after a while
  sniff=off       the not sniffed port of a flow: capture off, a sniff arms it,
                  capture back on
  unplug          one end of a flow unplugged from its lan (a cnt, the c2c,
                  the peer), plugged back after a few seconds
  a2b in path     an a2b inserted under the udp flow, delay, loss or rate
                  set, then taken out and the lan restored
  bystander       a cnt added on the flow lan and a lan of its own, pinged,
                  kept for a while (up to three), deleted later
  tap, nat        added on the flow lan, deleted
  c2c rebuilt     the c2c deleted and added again, peered, cabled both sides
  kvm             once: a kvm cabled on the flow lan, booted, pinged, deleted
  dumps           topo, json, commands, pid, namespaces, ovs-vsctl show
  positions       cnf x= y= and angle= of random items

Every ten throws: dump commands within 10 s, every daemon's rss bounded, the
snf count (one process per capturable port) bounded by what the throws
added, every lan of ftopo a bridge with ports. A trace json_cli
subscriber lives through the whole storm.

After the storm: every flow reports how many of its runs got through (half
at least); every path pinged with no loss; tcp on each
path at half its pre-storm rate at least; a fresh sniff counts; no snf, no
process and no bridge left for what was deleted; ftopo holds exactly the
standing topology; rss bounded; the nets erased and nothing of theirs
left running.

Standard library only; the helpers come from testitall.py and testbigflows.py
beside this file.
"""
import os
import random
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import testitall as ti
from testitall import (Fail, cli, must, ssh, wait_answer, wait_topo,
                       ftopo, in_topo, erase, add_cnt, address, ping,
                       ping_stats, Sniff, show, iperf_server, iperf_run,
                       mbits, c2c_peered, c2c_evidence)
from testbigflows import (rss_table, rss_check, answers_in, snf_count,
                          DAEMONS, RSS_BOUND_MB)

NET, PEER = ti.NET, ti.PEER
FLOW_ROOTFS = os.environ.get("TESTBIGFLOWS_IMAGE", "flow_trixie")
CNT_ROOTFS = ti.CNT_ROOTFS
KVM_ROOTFS = ti.KVM_ROOTFS
SECS = int(os.environ.get("TESTSTORM_SECS", "240"))
SEED = int(os.environ.get("TESTSTORM_SEED", str(int(time.time()) % 100000)))
BOOT = int(os.environ.get("TESTITALL_BOOT", "240"))

A, B, C, D, P = "stA", "stB", "stC", "stD", "stP"
LAN, LANV, LANP, LAND = "stlan", "stlanv", "stlanp", "stlanD"
C2C = "%s2%s" % (NET, PEER)
A2B, TAP, NAT, KVM = "sta2b", "sttap", "stnat", "stK"
PORT2 = 5202

T0 = time.time()
DICE = random.Random(SEED)
BASE = {}
BYSTANDERS = []       # (name, lan) alive now
DELETED = set()       # every name that was created and deleted
KVM_DONE = [False]
COUNT = [0]           # bystanders created
LOG = []              # (t, what) of every throw, printed on a failure


def stamp(what):
    LOG.append((time.time() - T0, what))
    del LOG[:-40]
    print("      [%4.0fs] %s" % (time.time() - T0, what))


# ------------------------------------------------------------------ flows

class Flow:
    """One iperf3 client run again and again inside a guest until the end,
    each run 10 s; the guest counts the runs that reported a receiver."""

    def __init__(self, label, node, target, extra="", net=None, port=5201):
        self.label, self.node, self.net = label, node, net or NET
        self.target, self.extra, self.port = target, extra, port
        self.runs = self.ok = None
        self.last_ok = False
        self.proc = None

    def start(self, secs):
        script = ("end=$(( $(date +%%s) + %d )); n=0; ok=0; last=0; "
                  "while [ $(date +%%s) -lt $end ]; do n=$((n+1)); "
                  "if timeout -s KILL 40 iperf3 -c %s -p %d -t 10 -f m %s 2>&1 "
                  "| grep -q receiver; "
                  "then ok=$((ok+1)); last=1; else last=0; fi; sleep 1; done; "
                  "echo STORM $n $ok $last"
                  % (secs, self.target, self.port, self.extra))
        show('cloonix cli %s ssh %s "%s" &' % (self.net, self.node, script))
        self.proc = subprocess.Popen(ti.CLOONIX + ["cli", self.net, "ssh",
                                                   self.node, script],
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True,
                                     stdin=subprocess.DEVNULL)
        return self

    def collect(self, timeout):
        try:
            out, _ = self.proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            raise Fail("flow %s: its ssh never came back" % self.label)
        for line in out.splitlines():
            w = line.split()
            if len(w) == 4 and w[0] == "STORM":
                self.runs, self.ok = int(w[1]), int(w[2])
                self.last_ok = w[3] == "1"
        if self.runs is None:
            raise Fail("flow %s reported nothing:\n%s" % (self.label,
                                                          out[-800:]))

    def kill(self):
        if self.proc and self.proc.poll() is None:
            self.proc.kill()
        ssh(self.node, "pkill -f 'iperf3 -c'; true", net=self.net,
            check=False, quiet=True)


def flows_start(secs):
    return [Flow("A->B tcp lan", A, "10.8.8.2", "-P 2").start(secs),
            Flow("C->D udp lan", C, "10.8.8.4", "-u -b 100M -l 1400").start(secs),
            Flow("A->B tcp vhost lan", A, "10.7.7.2", "", port=PORT2).start(secs),
            Flow("A->P tcp c2c", A, "10.8.8.9", "").start(secs)]


# ------------------------------------------------------------------ pieces

def itf(item, num):
    """The words that name an interface: eth<i> of a guest, A or B of the
    a2b, none for the one interface of the c2c, tap or nat."""
    if item == A2B:
        return ["AB"[num]]
    return [] if item in (C2C, TAP, NAT) else ["eth%d" % num]


def lan_add(item, num, lan, net=None):
    must(cli("plug", item, *(itf(item, num) + [lan]), net=net),
         " ".join(["plug", item] + itf(item, num) + [lan]))


def lan_del(item, num, lan, net=None):
    must(cli("unplug", item, *(itf(item, num) + [lan]), net=net),
         " ".join(["unplug", item] + itf(item, num) + [lan]))


def sat_del(name, net=None, timeout=60):
    must(cli("del", name, net=net), "del %s" % name)
    wait_topo(name, False, net=net, timeout=timeout)
    DELETED.add(name)


def c2c_build():
    must(cli("add", "c2c", C2C, PEER), "add c2c")
    end = time.time() + 90
    while time.time() < end:
        if in_topo(C2C) and in_topo(C2C, PEER) and c2c_peered(C2C, NET):
            break
        time.sleep(1)
    else:
        raise Fail("the c2c is not peered within 90s\n" + c2c_evidence(C2C))
    lan_add(C2C, 0, LAN)
    lan_add(C2C, 0, LANP, net=PEER)
    end = time.time() + 30
    while not ping(A, "10.8.8.9", 1, check=False):
        if time.time() > end:
            raise Fail("nothing crosses the c2c\n" + c2c_evidence(C2C))
        time.sleep(1)


def flow_ports():
    """(node, num, net) of every port a flow crosses."""
    return [(A, 0, NET), (B, 0, NET), (C, 0, NET), (D, 0, NET),
            (A, 1, NET), (B, 1, NET), (P, 0, PEER)]


# ------------------------------------------------------------------ throws

def throw_sniff_read():
    node, num, net = DICE.choice(flow_ports())
    filt = DICE.choice(("", "tcp", "udp", "icmp", "port 5201", "arp"))
    secs = DICE.randint(2, 6)
    stamp("sniff read %s %d '%s' for %ds" % (node, num, filt, secs))
    s = Sniff(node, num, filt, net=net).start()
    time.sleep(secs)
    n = s.stop()
    stamp("sniff read %s %d: %d delivered, %d missing" % (node, num, n, s.gaps))


def throw_sniff_unread():
    node, num, net = DICE.choice(flow_ports())
    secs = DICE.randint(3, 15)
    stamp("sniff unread %s %d for %ds" % (node, num, secs))
    show("cloonix cli %s sniff %s eth%d --bytes   (stdout never read)"
         % (net, node, num))
    p = subprocess.Popen(ti.CLOONIX + ["cli", net, "sniff", node, "eth%d" % num,
                                       "--bytes"],
                         stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    time.sleep(secs)
    p.kill()
    p.wait()


def throw_cnf_eth():
    node = DICE.choice((A, B))
    stamp("cnf %s eth1 sniff=off: capture off, a sniff arms it again" % node)
    must(cli("cnf", node, "eth1", "sniff=off"), "cnf <n> eth1 sniff=off")
    time.sleep(1)
    s = Sniff(node, 1, "").start()
    time.sleep(2)
    s.stop()


def throw_unplug():
    item, num, lan, net = DICE.choice(((B, 0, LAN, NET), (D, 0, LAN, NET),
                                       (B, 1, LANV, NET), (C2C, 0, LAN, NET),
                                       (P, 0, LANP, PEER)))
    secs = DICE.randint(1, 4)
    stamp("unplug %s %d from %s for %ds" % (item, num, lan, secs))
    lan_del(item, num, lan, net=net)
    time.sleep(secs)
    lan_add(item, num, lan, net=net)


def throw_a2b():
    knob = DICE.choice(("A2B delay=50", "B2A delay=50",
                        "A2B loss=1000", "A2B rate=2000",
                        "B2A rate=2000"))
    secs = DICE.randint(4, 10)
    stamp("a2b under the udp flow: %s for %ds" % (knob, secs))
    must(cli("add", "a2b", A2B), "add a2b")
    wait_topo(A2B, True)
    lan_del(D, 0, LAN)
    lan_add(A2B, 0, LAN)
    lan_add(A2B, 1, LAND)
    lan_add(D, 0, LAND)
    must(cli("cnf", A2B, knob), "cnf <a2b>")
    time.sleep(secs)
    must(cli("cnf", A2B, knob.rsplit("=", 1)[0] + "=0"), "cnf 0")
    sat_del(A2B)
    lan_del(D, 0, LAND)
    lan_add(D, 0, LAN)


def throw_bystander():
    COUNT[0] += 1
    n = COUNT[0]
    name, lan = "stX%d" % n, "stbl%d" % n
    stamp("bystander %s on %s and %s" % (name, LAN, lan))
    add_cnt(name, "ss", rootfs=CNT_ROOTFS)
    lan_add(name, 0, LAN)
    lan_add(name, 1, lan)
    wait_answer(name, timeout=180)
    address(name, 0, "10.8.8.%d/24" % (100 + n % 100))
    ping(name, "10.8.8.2", 3)
    BYSTANDERS.append((name, lan))
    if len(BYSTANDERS) > 3:
        throw_bystander_del()


def throw_bystander_del():
    if not BYSTANDERS:
        return throw_bystander()
    name, lan = BYSTANDERS.pop(DICE.randrange(len(BYSTANDERS)))
    stamp("bystander %s deleted" % name)
    sat_del(name)


def throw_tap_nat():
    kind = DICE.choice(("tap", "nat"))
    name = TAP if kind == "tap" else NAT
    stamp("%s %s added on %s, deleted" % (kind, name, LAN))
    must(cli("add", kind, name), "add %s" % kind)
    wait_topo(name, True)
    lan_add(name, 0, LAN)
    time.sleep(DICE.randint(1, 3))
    sat_del(name)


def throw_c2c_rebuild():
    stamp("c2c %s deleted and built again" % C2C)
    sat_del(C2C)
    wait_topo(C2C, False, net=PEER)
    c2c_build()


def throw_kvm():
    if KVM_DONE[0] or not ti.in_bulk(KVM_ROOTFS, BASE["images"]):
        return throw_bystander()
    if time.time() - BASE["t_storm"] > SECS - BOOT:
        return throw_bystander()
    KVM_DONE[0] = True
    stamp("kvm %s booted on %s in the storm" % (KVM, LAN))
    must(cli("add", "kvm", KVM, "ram=1024", "cpu=1", "eth=1", KVM_ROOTFS),
         "add kvm")
    lan_add(KVM, 0, LAN)
    wait_answer(KVM, timeout=BOOT)
    address(KVM, 0, "10.8.8.200/24")
    ping(KVM, "10.8.8.2", 3)
    stamp("kvm %s answered and pinged, deleted" % KVM)
    sat_del(KVM, timeout=90)


def throw_dumps():
    what = DICE.choice((("topo",), ("json",), ("commands",), ("pid",),
                        ("namespaces",), ("ovs-vsctl", "show"),
                        ("topo",), ("commands",)))
    net = DICE.choice((NET, NET, PEER))
    t = time.time()
    r = cli("dump", *what, net=net, timeout=10)
    if r.returncode:
        raise Fail("dump %s on %s in the storm: %s"
                   % (" ".join(what), net, (r.stderr + r.stdout).strip()))
    stamp("dump %s on %s in %.2fs" % (" ".join(what), net, time.time() - t))


def throw_positions():
    item = DICE.choice((A, B, C, D) + tuple(n for n, _ in BYSTANDERS))
    stamp("cnf %s x= y=, cnf %s eth0 angle=" % (item, item))
    must(cli("cnf", item, "x=%d" % DICE.randint(-300, 300),
             "y=%d" % DICE.randint(-300, 300)), "cnf x= y=")
    must(cli("cnf", item, "eth0", "angle=%d" % DICE.randint(0, 359)),
         "cnf angle=")


THROWS = ((throw_sniff_read, 6), (throw_sniff_unread, 2), (throw_cnf_eth, 1),
          (throw_unplug, 5), (throw_a2b, 2), (throw_bystander, 4),
          (throw_bystander_del, 2), (throw_tap_nat, 2), (throw_c2c_rebuild, 1),
          (throw_kvm, 1), (throw_dumps, 3), (throw_positions, 2))


# ------------------------------------------------------------------ checks

def bridges(net=None):
    out = must(cli("dump", "ovs-vsctl", "list-br", net=net, quiet=True),
               "list-br")
    return set(l.strip() for l in out.splitlines() if l.strip())


def lans_in_ftopo(net=None):
    out = set()
    for e in ftopo(net).get("endp", []) or []:
        for lan in e.get("lans", []) or []:
            out.add(lan)
    return out


def check_lans(where, net=None):
    lans = lans_in_ftopo(net)
    br = bridges(net)
    missing = ["_b_" + l for l in lans if "_b_" + l not in br]
    if missing:
        raise Fail("%s: lans of ftopo with no bridge: %s" % (where, missing))
    for lan in lans:
        out = must(cli("dump", "ovs-vsctl", "list-ports", "_b_" + lan, net=net,
                       quiet=True), "list-ports")
        if not out.strip():
            raise Fail("%s: _b_%s has no port and ftopo cables it" % (where, lan))


def check_health(where):
    dt = answers_in(10, where)
    now = rss_table()
    grown = [(d, BASE["rss"][d], now[d]) for d in DAEMONS
             if now[d] - BASE["rss"][d] > RSS_BOUND_MB]
    if grown:
        raise Fail("%s: memory climbed past %d MB: %s"
                   % (where, RSS_BOUND_MB, ", ".join("%s %.0f->%.0f" % g
                                                     for g in grown)))
    snf = snf_count()
    # the throws add capturable ports of their own: two per bystander, two
    # for the a2b, one for a tap, a nat or the kvm, one while the c2c restarts
    if snf > BASE["snf"] + 2 * len(BYSTANDERS) + 5:
        raise Fail("%s: %d cloonix-ovs-snf for %d standing ports and %d "
                   "bystanders" % (where, snf, BASE["snf"], len(BYSTANDERS)))
    check_lans(where)
    stamp("%s: dump commands %.2fs, %d snf, rss ok" % (where, dt, snf))


def check_left_behind():
    # a name deleted and built again in the storm (the c2c) is alive now:
    # its processes are not leftovers
    gone = [n for n in DELETED if not in_topo(n) and not in_topo(n, PEER)]
    r = subprocess.run(["pgrep", "-af", "cloonix"], capture_output=True,
                       text=True)
    left = []
    for line in r.stdout.splitlines():
        if " %s" % NET not in line and " %s" % PEER not in line:
            continue
        for name in gone:
            if (" %s " % name) in line + " " or ("_%s_" % name) in line:
                left.append(line.strip())
    if left:
        raise Fail("processes of deleted objects:\n  " + "\n  ".join(left))
    stale = [b for b in bridges() if b.startswith("_b_st") and
             b[3:] not in (LAN, LANV)]
    if stale:
        raise Fail("bridges left after the storm: %s" % stale)


# ------------------------------------------------------------------ steps

def t_setup():
    print("      seed %d, %ds of storm" % (SEED, SECS))
    for net in (NET, PEER):
        must(cli("dump", "commands", net=net), "dump commands on %s" % net)
    BASE["images"] = must(cli("dump", "images"), "dump images").split()
    if not ti.in_bulk(FLOW_ROOTFS, BASE["images"]):
        raise Fail("%s is not in bulk: cd vm_create/cnt_kvm_demo && "
                   "./cnt_flow_trixie.sh" % FLOW_ROOTFS)
    if not ti.in_bulk(KVM_ROOTFS, BASE["images"]):
        print("      (no %s: the kvm throw is skipped)" % KVM_ROOTFS)
    for net in (NET, PEER):
        erase(net)
    for name in (A, B, C, D):
        add_cnt(name, "sv", rootfs=FLOW_ROOTFS)
    add_cnt(P, "s", net=PEER, rootfs=FLOW_ROOTFS)
    for name in (A, B, C, D):
        lan_add(name, 0, LAN)
    for name in (A, B):
        lan_add(name, 1, LANV)
    lan_add(P, 0, LANP, net=PEER)
    for i, name in enumerate((A, B, C, D)):
        wait_answer(name)
        address(name, 0, "10.8.8.%d/24" % (i + 1))
    for i, name in enumerate((A, B)):
        address(name, 1, "10.7.7.%d/24" % (i + 1))
        must(cli("cnf", name, "eth1", "sniff=on"), "cnf <n> eth1 sniff=on")
    wait_answer(P, PEER)
    address(P, 0, "10.8.8.9/24", net=PEER)
    for name in (B, D):
        iperf_server(name)
    ssh(B, "iperf3 -s -p %d -D >/dev/null 2>&1" % PORT2)
    iperf_server(P, net=PEER)
    c2c_build()
    ping(A, "10.7.7.2", 3)
    ping(C, "10.8.8.4", 3)
    # every sniffed port carries one cloonix-ovs-snf of its own from the day
    # it is up: the standing topology is the reference, not the empty net
    time.sleep(3)
    BASE["rss"] = rss_table()
    BASE["snf"] = snf_count()
    print("      standing topology: %d cloonix-ovs-snf, one per capturable port"
          % BASE["snf"])
    BASE["tcp"] = {}
    for label, node, target, port in (("A->B lan", A, "10.8.8.2", 5201),
                                      ("A->B vhost lan", A, "10.7.7.2", PORT2),
                                      ("A->P c2c", A, "10.8.8.9", 5201)):
        snd, rcv, _ = iperf_run(node, target, secs=4, port=port)
        mbits("before the storm, %s" % label, snd, rcv)
        if rcv is None or rcv < 5:
            raise Fail("tcp %s before the storm: %s Mbit/s" % (label, rcv))
        BASE["tcp"][label] = rcv


def t_storm():
    BASE["t_storm"] = time.time()
    show("cloonix cli %s trace json_cli" % NET)
    trace = subprocess.Popen(ti.CLOONIX + ["cli", NET, "trace", "json_cli"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                             text=True)
    seen = [0]

    def drain():
        for _ in trace.stdout:
            seen[0] += 1
    threading.Thread(target=drain, daemon=True).start()
    flows = flows_start(SECS + 20)
    time.sleep(12)
    end = BASE["t_storm"] + SECS
    throws = 0
    try:
        while time.time() < end:
            fn = DICE.choices([f for f, _ in THROWS],
                              [w for _, w in THROWS])[0]
            fn()
            throws += 1
            if throws % 10 == 0:
                check_health("after %d throws" % throws)
            time.sleep(DICE.choice((0, 0, 0.5, 1, 2)))
    except Exception:
        for f in flows:
            f.kill()
        trace.kill()
        raise
    stamp("%d throws, the dice are put down; the flows finish" % throws)
    for f in flows:
        f.collect(60)
    if trace.poll() is not None:
        raise Fail("the trace json_cli subscriber died in the storm")
    trace.kill()
    if seen[0] < throws:
        raise Fail("trace json_cli reported %d lines for %d throws"
                   % (seen[0], throws))
    print("      trace json_cli: %d lines, alive to the end" % seen[0])
    for f in flows:
        print("      flow %-22s %3d runs, %3d got through, last %s"
              % (f.label, f.runs, f.ok, "ok" if f.last_ok else "FAILED"))
    for f in flows:
        if f.runs < 3 or f.ok * 2 < f.runs:
            raise Fail("flow %s: %d of %d runs got through" % (f.label, f.ok,
                                                                f.runs))


def t_after():
    for name, _ in list(BYSTANDERS):
        sat_del(name)
    del BYSTANDERS[:]
    for name in (A2B, TAP, NAT, KVM):
        if in_topo(name):
            sat_del(name, timeout=90)
    time.sleep(5)
    if not c2c_peered(C2C, NET):
        raise Fail("the c2c is not peered after the storm\n" + c2c_evidence(C2C))
    for node, target, net in ((A, "10.8.8.2", NET), (C, "10.8.8.4", NET),
                              (A, "10.7.7.2", NET), (A, "10.8.8.9", NET),
                              (P, "10.8.8.1", PEER)):
        loss, rtt = ping_stats(node, target, 10, net=net)
        if loss:
            raise Fail("after the storm: %s -> %s %d%% loss" % (node, target,
                                                                 loss))
        print("      after the storm: %s -> %s rtt %.1f ms" % (node, target,
                                                                rtt))
    for label, node, target, port in (("A->B lan", A, "10.8.8.2", 5201),
                                      ("A->B vhost lan", A, "10.7.7.2", PORT2),
                                      ("A->P c2c", A, "10.8.8.9", 5201)):
        snd, rcv, _ = iperf_run(node, target, secs=4, port=port)
        mbits("after the storm, %s" % label, snd, rcv)
        if rcv is None or rcv < 0.5 * BASE["tcp"][label]:
            raise Fail("tcp %s after the storm: %s Mbit/s, was %.1f"
                       % (label, rcv, BASE["tcp"][label]))
    s = Sniff(A, 0, "icmp").start()
    ping(A, "10.8.8.2", 3)
    if s.stop() < 6:
        raise Fail("a fresh sniff after the storm counts nothing")
    s = Sniff(A, 1, "icmp").start()
    ping(A, "10.7.7.2", 3)
    if s.stop() < 6:
        raise Fail("a fresh sniff on the not sniffed port after the storm counts "
                   "nothing")
    time.sleep(3)
    snf = snf_count()
    if snf > BASE["snf"] + 3:
        raise Fail("%d cloonix-ovs-snf before the storm, %d after"
                   % (BASE["snf"], snf))
    check_lans("after the storm")
    check_lans("after the storm", net=PEER)
    if lans_in_ftopo() != {LAN, LANV}:
        raise Fail("lans of ftopo after the storm: %s" % sorted(lans_in_ftopo()))
    check_left_behind()
    rss_check(BASE["rss"], "the storm")


def t_end():
    sat_del(C2C)
    wait_topo(C2C, False, net=PEER)
    for net in (NET, PEER):
        erase(net)
    DELETED.update((A, B, C, D, P))
    time.sleep(5)
    check_left_behind()


STEPS = (("setup", t_setup), ("storm", t_storm), ("after", t_after),
         ("end", t_end))


def main():
    print("teststorm: flows on a moving target, %s with peer %s" % (NET, PEER))
    for name, fn in STEPS:
        t = time.time()
        print("[%6.1fs] %s" % (t - T0, name))
        try:
            fn()
        except Fail as err:
            print()
            print("FAILED: %s" % name)
            print("  %s" % str(err).replace("\n", "\n  "))
            print("  seed %d; the last throws:" % SEED)
            for t, what in LOG[-15:]:
                print("    [%4.0fs] %s" % (t, what))
            print("  the last commands given:")
            for line in ti.LAST:
                print("    %s" % line)
            print("  the nets are left as they are: cloonix cli %s dump topo"
                  % NET)
            sys.exit(1)
        print("      ok in %.1fs" % (time.time() - t))
    print()
    print("THE FLOWS HELD ON A MOVING TARGET in %.0fs, seed %d"
          % (time.time() - T0, SEED))


if __name__ == "__main__":
    main()
