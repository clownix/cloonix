#!/usr/bin/env python3
r"""testbigflows - every path that carries bytes, driven above its limit.

    ./testbigflows.py [net] [peer]          defaults: nemo mito

Two nets up, the image flow_trixie in bulk (vm_create/cnt_kvm_demo/
cnt_flow_trixie.sh: iperf3, trafgen, hping3, socat) and trixie.qcow2 (its
iperf3 and dhclient). Both nets are erased first. TESTBIGFLOWS_SECS (20)
is the length of one flood, TESTBIGFLOWS_LONG (180) that of the storm.
Not covered: phy (a real interface), webshark itself (the same doors
stream as the sniff, but its last hop is a browser). The generators: iperf3 with -P 4 for tcp, iperf3 -u -b 0 for udp at
the rate the sender can push, trafgen for 64-byte frames at the packet rate
of the machine. Every scenario prints its figures and fails on health, not
on a figure: a daemon that stopped answering, a flow that stopped, memory
that climbed, a client that could not be replaced.

  lan tcp        iperf3 -P 4, the reference for the rest
  lan udp        iperf3 -u -b 0: what the lan drops when the sender does
                 not slow down, and that it still pings after
  lan pps        trafgen 64-byte frames: sent and received packet rates
  sniff ceiling  cloonix cli <net> sniff --count, which decodes nothing, under a
                 1400-byte udp flood, a 200-byte one, the 64-byte frames and
                 a filter: the kpps and Mbit/s the chain delivers to a
                 client against what the wire carried, and what was dropped
  sniff read     one unfiltered sniff, read, under the pps and udp floods:
                 packets delivered, packets numbered by the switch and
                 missing at the client, the lan rate kept, dump commands answers
  sniff unread   a sniff whose client never reads, under the floods: the
                 lan rate kept, dump commands and ssh answer, the memory of
                 doorways and the server bounded, a new sniff after the
                 client is killed, no snf process left behind
  sniffs x4      four sniffs on four ports read at once under the floods
  scp            128 MB through the agent both ways, md5, under the floods
                 and a sniff
  c2c tcp/udp/pps  iperf3 -P 4, then -u -b 0, then trafgen across the c2c:
                 figures, then the c2c still peered, a ping, tcp again at
                 half its first rate at least, proxymous and ovs_c2c memory
                 bounded
  a2b above      rate=100 kB/s under a 50 Mbit/s udp flood: the receiver
                 gets about the rate and nothing more; delay=500 under the
                 same flood: a2b memory bounded; 0 restores the ping
  nat above      udp flood at the host through passt, then tcp still crosses
  ssh streams    128 MB by ssh stdin (into md5sum) and ssh stdout (cat) under
                 the floods and a sniff: the command path beside scp
  kvm            two kvm, one sniffed, one not: tcp both ways, udp unbounded, a
                 sniff on the sniffed port under the flood, cnt to kvm, then
                 96 MB by scp and ssh over the virtio serial under load
  pipe bounds    on a cnt and on a kvm: ssh stdout never read and ssh stdin
                 never read, each 20 s: doorways and the agent of the guest
                 hold bounded memory, a second ssh to the same guest answers,
                 the command is gone after the client is killed; 36 pipes at
                 once: the agent refuses past 30 and its KERR reaches the
                 syslog of the net as "guest <name>:"; an scp killed half way:
                 the KERR of the exec server of the guest reaches it too
  airs           mac80211_hwsim radios: cnt to cnt, kvm to kvm, cnt to kvm,
                 each on an air of its own; a ping over the air while three
                 cnt are added and deleted: no loss, no rtt past 500 ms;
                 tcp and the udp maximum; 64 MB by
                 scp into a kvm whose radio is flooded; a flooded air under a
                 sniff read and a sniff never read: suid-power and the server
                 bounded, the air still pings
  c2c both ways  8 tcp streams each way at once, udp unbounded each way at
                 once, frames each way at once, then still peered
  a2b more       10% loss each way under udp, silentms=200 each way under
                 tcp, rate=5000 each way under udp both ways, frames into
                 the shaper, then restored to half the clean rate at least
  nat more       from a kvm on dhcp: tcp and udp to the host, then the host
                 into the guest through publish=, tcp and udp
  tap            with iperf3 and sudo on the host: tcp and udp both ways
                 through the tap, frames at it
  storm          everything at once for TESTBIGFLOWS_LONG seconds (180):
                 lan floods, c2c both ways, a2b, nat, three sniffs read and
                 one unread, kvm to kvm, and scp round trips plus ssh stdin
                 in a loop; memory bounded throughout; then every path pinged
  memory         every cloonix daemon: rss before and after, bounded
  maximum        at its own point in each scenario, the maximum of every
                 device: tcp by iperf3, udp and 64-byte frames by a binary
                 search under the unbounded rate for the highest rate with
                 at most 1% lost, the sniff for the highest rate it delivers
                 whole; printed as "MAX ..." lines at the end and written to
                 TESTBIGFLOWS_MAX (/tmp/testbigflows_max_<net>.txt)

Standard library only; the helpers come from testitall.py beside this file.
"""
import getpass
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import testitall as ti
from testitall import (Fail, cli, must, ssh, wait_answer, wait_topo, erase,
                       add_cnt, address, ping, ping_stats, Sniff, show,
                       host_port_free, iperf_server, iperf_run, iperf_bg,
                       iperf_parse, mbits, big_file, md5_host, md5_guest,
                       c2c_peered, c2c_evidence, IPERF_PORT)

NET, PEER = ti.NET, ti.PEER
FLOW_ROOTFS = os.environ.get("TESTBIGFLOWS_IMAGE", "flow_trixie")
SECS = int(os.environ.get("TESTBIGFLOWS_SECS", "20"))
LONG = int(os.environ.get("TESTBIGFLOWS_LONG", "180"))
KVM_ROOTFS = os.environ.get("TESTITALL_KVM", "trixie.qcow2")
K1, K2 = "bfK1", "bfK2"
W1, W2 = "bfW1", "bfW2"
AIR_C, AIR_K = "bfair", "bfkair"
# memory a flow held back must stay under, in the host daemons and in the agent
HELD_MB = int(os.environ.get("TESTBIGFLOWS_HELD_MB", "64"))
AGENT_HELD_MB = 48
MAX_PIPES = 30
SYSLOG = ti.SYSLOG
RSS_BOUND_MB = int(os.environ.get("TESTBIGFLOWS_RSS_MB", "300"))
A, B, C, D = "bfA", "bfB", "bfC", "bfD"
P = "bfP"
LAN = "bflan"

DAEMONS = ("cloonix-main-server", "cloonix-doorways", "cloonix-proxymous",
           "cloonix-ovs-drv", "cloonix-ovs-snf", "cloonix-ovs-c2c",
           "cloonix-ovs-a2b", "cloonix-ovs-nat", "cloonix-suid-power")

T0 = time.time()
BASE = {}


# ------------------------------------------------------------------ helpers

def rss_mb(name, net=None):
    """rss in MB of every process of that binary for this net, summed."""
    r = subprocess.run(["pgrep", "-af", name], capture_output=True, text=True)
    total = 0.0
    lines = r.stdout.splitlines()
    mine = [l for l in lines if " %s" % (net or NET) in l] or lines
    for l in mine:
        pid = l.split()[0]
        try:
            for st in open("/proc/%s/status" % pid):
                if st.startswith("VmRSS:"):
                    total += int(st.split()[1]) / 1024.0
        except OSError:
            pass
    return total


def rss_table():
    return dict((d, rss_mb(d)) for d in DAEMONS)


def rss_check(before, what, bound=RSS_BOUND_MB):
    now = rss_table()
    grown = [(d, before[d], now[d]) for d in DAEMONS
             if now[d] - before[d] > bound]
    for d, b, n in ((d, before[d], now[d]) for d in DAEMONS
                    if now[d] - before[d] > 20):
        print("      %-22s %6.0f -> %6.0f MB" % (d, b, n))
    if grown:
        raise Fail("%s: memory climbed past %d MB: %s"
                   % (what, bound, ", ".join("%s %.0f->%.0f" % g
                                            for g in grown)))
    return now


def answers_in(seconds, what):
    t = time.time()
    r = cli("dump", "commands", timeout=seconds, quiet=True)
    dt = time.time() - t
    if r.returncode:
        raise Fail("%s: dump commands did not answer in %ds" % (what, seconds))
    return dt


def counters(node, net=None):
    out = ssh(node, "cat /sys/class/net/eth0/statistics/rx_packets "
                    "/sys/class/net/eth0/statistics/tx_packets "
                    "/sys/class/net/eth0/statistics/rx_bytes", net=net,
              quiet=True)
    rx, tx, rxb = (int(x) for x in out.split())
    return rx, tx, rxb


def mac_of(node, net=None):
    return ssh(node, "cat /sys/class/net/eth0/address", net=net).strip()


def trafgen_cfg(node, dst_mac, src_ip, dst_ip, size=64, net=None):
    pad = max(size - 42, 2)
    cfg = ("{ eth(da=%s, sa=%s), ipv4(sa=%s, da=%s), udp(sp=4444, dp=5555), "
           "fill(0x42, %d) }" % (dst_mac, mac_of(node, net), src_ip, dst_ip,
                                 pad))
    ssh(node, "cat > /tmp/flood.cfg <<'EOF'\n%s\nEOF" % cfg, net=net)


def trafgen_bg(node, secs, net=None, gap="0"):
    """gap: a pause between frames ("500us") for a paced flood; 0 is flat out."""
    cmd = ("timeout -s INT %d trafgen --dev eth0 --conf /tmp/flood.cfg "
           "--cpus %d --gap %s --qdisc-path >/dev/null 2>&1; true"
           % (secs, 1 if gap != "0" else 2, gap))
    show('cloonix cli %s ssh %s "%s" &' % (net or NET, node, cmd))
    return subprocess.Popen(ti.CLOONIX + ["cli", net or NET, "ssh", node, cmd],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL,
                            stdin=subprocess.DEVNULL)


def udp_bg(node, target, secs, net=None, rate="0"):
    return iperf_bg(node, target, secs, extra="-u -b %s -l 1400" % rate,
                    net=net)


def floods(src, dst_ip, secs, net=None):
    """The pps flood and the udp flood together, both in the background."""
    return [trafgen_bg(src, secs, net), udp_bg(src, dst_ip, secs, net)]


def wait_bg(procs, secs):
    end = time.time() + secs + 30
    for p in procs:
        try:
            p.wait(timeout=max(1, end - time.time()))
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()


def pps(node, secs, net=None):
    """(rx pps, tx pps, rx Mbit/s) of a node's eth0 over secs of flood."""
    rx0, tx0, b0 = counters(node, net)
    time.sleep(secs)
    rx1, tx1, b1 = counters(node, net)
    return ((rx1 - rx0) / float(secs), (tx1 - tx0) / float(secs),
            (b1 - b0) * 8 / 1e6 / secs)


# Maximum per device, by search: tcp is iperf3's, udp and frames the highest
# rate under MAX_LOSS_OK% loss; printed at the end, in TESTBIGFLOWS_MAX.
MAXF = []
MAX_LOSS_OK = 1.0
MAX_SECS = max(4, min(SECS, 6))
MAX_FILE = os.environ.get("TESTBIGFLOWS_MAX", "/tmp/testbigflows_max_%s.txt" % NET)


def note_max(device, flow, value, unit="Mbit/s", detail=""):
    if value is None:
        return
    MAXF.append((device, flow, value, unit, detail))
    print("      MAX %-16s %-26s %9.1f %s %s" % (device, flow, value, unit, detail))


def udp_max(src, dst_ip, net=None, port=IPERF_PORT, loss_ok=MAX_LOSS_OK):
    """(Mbit/s at the receiver, loss %) of the highest udp rate the path
    carries with at most loss_ok% lost: unbounded first, then a binary
    search under what the receiver got."""
    snd, rcv, loss = iperf_run(src, dst_ip, secs=MAX_SECS,
                               extra="-u -b 0 -l 1400", net=net, port=port)
    if rcv is None:
        return None, 0.0
    if loss is None or loss <= loss_ok:
        return rcv, loss or 0.0
    lo, hi, best = 0.0, rcv, (0.0, loss)
    for _ in range(4):
        mid = (lo + hi) / 2
        if mid < 1:
            break
        snd, rcv, loss = iperf_run(src, dst_ip, secs=MAX_SECS,
                                   extra="-u -b %dM -l 1400" % int(mid),
                                   net=net, port=port)
        if rcv is not None and loss is not None and loss <= loss_ok:
            lo, best = mid, (rcv, loss)
        else:
            hi = mid
    return best


def counters_at(node, net=None):
    """counters() and the middle of the time the ssh took to read them."""
    t0 = time.time()
    c = counters(node, net)
    return c, (t0 + time.time()) / 2


def frames_run(src, dst, secs, net_src=None, net_dst=None, gap="0"):
    """(tx pps at src, rx pps at dst) of one trafgen flood, each over the
    window of its own two reads: the reads are ssh, not simultaneous."""
    bg = trafgen_bg(src, secs, net=net_src, gap=gap)
    time.sleep(1)
    (rx0, _, _), trx0 = counters_at(dst, net_dst)
    (_, tx0, _), ttx0 = counters_at(src, net_src)
    time.sleep(secs - 2)
    (rx1, _, _), trx1 = counters_at(dst, net_dst)
    (_, tx1, _), ttx1 = counters_at(src, net_src)
    wait_bg([bg], secs)
    return ((tx1 - tx0) / max(0.001, ttx1 - ttx0),
            (rx1 - rx0) / max(0.001, trx1 - trx0))


def frame_loss(tx, rx):
    """Loss % of a frame flood; the receiver's own few frames (arp) can
    make it a hair under 0, shown as 0."""
    return max(0.0, 100.0 * (tx - rx) / max(1, tx))


def pps_max(src, dst, net_src=None, net_dst=None, loss_ok=MAX_LOSS_OK):
    """(rx kpps, loss %, note) of the highest 64-byte frame rate the path
    carries with at most loss_ok% lost: flat out first, then the trafgen gap
    searched between that and a quarter of it. A paced trafgen tops out
    below what it is asked (its gap is a sleep): once it sends less than
    asked, the search stops and the note says the figure is the sender's,
    the path carrying more."""
    tx, rx = frames_run(src, dst, MAX_SECS, net_src, net_dst)
    if tx <= 0:
        return None, 0.0, ""
    loss = frame_loss(tx, rx)
    if loss <= loss_ok:
        return rx / 1000, loss, ""
    flat_rx, flat_loss = rx, loss
    lo, hi, best, short = rx / 4, rx, (0.0, loss), False
    for _ in range(4):
        mid = (lo + hi) / 2
        gap = "%dus" % max(1, int(1e6 / mid))
        tx, rx = frames_run(src, dst, MAX_SECS, net_src, net_dst, gap=gap)
        loss = frame_loss(tx, rx)
        if loss <= loss_ok:
            lo, best = mid, (rx / 1000, loss)
            if tx < 0.9 * mid:
                short = True
                break
        else:
            hi = mid
    note = ""
    if short:
        note = (" (sender-limited: the paced trafgen sends no more; flat out "
                "%.0f kpps arrive with %.1f%% lost)" % (flat_rx / 1000,
                                                       flat_loss))
    return best[0], best[1], note


def print_max():
    lines = ["MAXIMUM FLOWS on %s (loss at most %.0f%% for udp and frames)"
             % (NET, MAX_LOSS_OK)]
    for device, flow, value, unit, detail in MAXF:
        lines.append("  MAX %-16s %-26s %9.1f %s %s"
                     % (device, flow, value, unit, detail))
    print()
    print("\n".join(lines))
    try:
        with open(MAX_FILE, "w") as fd:
            fd.write("\n".join(lines) + "\n")
    except OSError as err:
        print("      (not written to %s: %s)" % (MAX_FILE, err))


def snf_count():
    r = subprocess.run(["pgrep", "-fc", "cloonix-ovs-snf"], capture_output=True,
                       text=True)
    try:
        return int(r.stdout.strip() or "0")
    except ValueError:
        return 0


def health(where):
    dt = answers_in(10, where)
    ssh(A, "echo alive", timeout=30, quiet=True)
    loss, rtt = ping_stats(A, "10.8.8.2", 10)
    if loss:
        raise Fail("%s: %d%% loss on a ping after the flood" % (where, loss))
    print("      %s: dump commands in %.2fs, ping rtt %.1f ms, 0%% loss"
          % (where, dt, rtt))


# ---------------------------------------------------------------- scenarios

def t_setup():
    for net in (NET, PEER):
        must(cli("dump", "commands", net=net), "dump commands on %s" % net)
    images = must(cli("dump", "images"), "dump images")
    if not ti.in_bulk(FLOW_ROOTFS, images):
        raise Fail("%s is not in bulk: cd vm_create/cnt_kvm_demo && "
                   "./cnt_flow_trixie.sh" % FLOW_ROOTFS)
    for net in (NET, PEER):
        erase(net)
    BASE["rss"] = rss_table()
    BASE["snf"] = snf_count()
    for name in (A, B, C, D):
        add_cnt(name, "s", rootfs=FLOW_ROOTFS)
    for name in (A, B, C, D):
        must(cli("plug", name, "eth0", LAN, ack=5), "plug")
    for i, name in enumerate((A, B, C, D)):
        wait_answer(name)
        address(name, 0, "10.8.8.%d/24" % (i + 1))
    for name in (B, D):
        iperf_server(name)
    # the eths of K1 are sniffed, those of K2 not: the two kinds of kvm port
    # carry the flows between them; K1 eth1 joins the container lan.
    must(cli("add", "kvm", K1, "ram=1024", "cpu=2", "eth=2", "wlan=1",
             KVM_ROOTFS), "add kvm")
    ti.snf_on(K1, "ss")
    must(cli("add", "kvm", K2, "ram=1024", "cpu=2", "eth=2", "wlan=1",
             KVM_ROOTFS), "add kvm")
    trafgen_cfg(A, mac_of(B), "10.8.8.1", "10.8.8.2")
    trafgen_cfg(C, mac_of(D), "10.8.8.3", "10.8.8.4")
    print("      trafgen: %s" % ssh(A, "trafgen --version 2>&1 | head -1",
                                     quiet=True).strip())


def t_lan_tcp():
    snd, rcv, _ = iperf_run(A, "10.8.8.2", secs=SECS, extra="-P 4")
    mbits("lan tcp, 4 streams", snd, rcv)
    if rcv is None or rcv < 100:
        raise Fail("four tcp streams on a lan: %s Mbit/s" % rcv)
    BASE["tcp"] = rcv
    note_max("cnt lan", "tcp 4 streams", rcv)


def t_lan_udp():
    snd, rcv, loss = iperf_run(A, "10.8.8.2", secs=SECS, extra="-u -b 0 -l 1400")
    mbits("lan udp, sender unbounded", snd, rcv, loss)
    if rcv is None or rcv < 50:
        raise Fail("udp flood on a lan: the receiver got %s Mbit/s" % rcv)
    rcv, loss = udp_max(A, "10.8.8.2")
    note_max("cnt lan", "udp 1400B", rcv, detail="loss %.1f%%" % loss)
    health("after the udp flood")


def t_lan_pps():
    bg = trafgen_bg(A, SECS)
    time.sleep(1)
    rx, _, mb = pps(B, SECS - 2)
    _, tx, _ = pps(A, 1)
    wait_bg([bg], SECS)
    print("      64-byte frames: %.0f kpps received on %s (%.1f Mbit/s)"
          % (rx / 1000, B, mb))
    if rx < 10000:
        raise Fail("trafgen flood: %.0f pps reach %s" % (rx, B))
    BASE["pps"] = rx
    kpps, loss, note = pps_max(A, B)
    note_max("cnt lan", "64B frames", kpps, "kpps",
             "loss %.1f%%%s" % (loss, note))
    health("after the pps flood")


def t_sniff_read():
    s = Sniff(B, 0, "").start()
    bg = floods(A, "10.8.8.2", SECS)
    time.sleep(1)
    rx, _, _ = pps(B, SECS - 3)
    dt = answers_in(10, "during the floods under a sniff")
    wait_bg(bg, SECS)
    n = s.stop()
    print("      sniff read: %d packets delivered, %d numbered and missing "
          "(%.0f%% of the flow reached the client), dump commands %.2fs"
          % (n, s.gaps, 100.0 * n / max(1, n + s.gaps), dt))
    print("      the lan under it: %.0f kpps (%.0f%% of the flood alone)"
          % (rx / 1000, 100.0 * rx / max(1, BASE["pps"])))
    if n < 1000:
        raise Fail("a read sniff under the floods delivered %d packets" % n)
    if rx < 0.3 * BASE["pps"]:
        raise Fail("the lan fell to %.0f pps under a read sniff" % rx)
    health("after the read sniff")


def t_sniff_unread():
    before = rss_table()
    snf0 = snf_count()
    show("cloonix cli %s sniff %s eth0 --bytes   (stdout never read)" % (NET, B))
    stuck = subprocess.Popen(ti.CLOONIX + ["cli", NET, "sniff", B, "eth0",
                                           "--bytes"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    try:
        time.sleep(2)
        bg = floods(A, "10.8.8.2", SECS + 4)
        time.sleep(1)
        rx, _, _ = pps(B, SECS)
        dt = answers_in(10, "under a stalled sniff client")
        ssh(A, "echo alive", timeout=30, quiet=True)
        rss_check(before, "during the stalled sniff")
        wait_bg(bg, SECS + 4)
        print("      the lan under it: %.0f kpps (%.0f%% of the flood alone), "
              "dump commands %.2fs" % (rx / 1000, 100.0 * rx / max(1, BASE["pps"]),
                                   dt))
        if rx < 0.3 * BASE["pps"]:
            raise Fail("the lan fell to %.0f pps under a stalled sniff "
                       "client" % rx)
    finally:
        stuck.kill()
        stuck.wait()
    time.sleep(3)
    rss_check(before, "after the stalled sniff client was killed")
    s = Sniff(B, 0, "icmp").start()
    ping(A, "10.8.8.2", 3)
    n = s.stop()
    if n < 6:
        raise Fail("a fresh sniff after the stalled one counted %d icmp" % n)
    time.sleep(2)
    snf1 = snf_count()
    if snf1 > snf0:
        raise Fail("%d cloonix-ovs-snf before the stalled sniff, %d after "
                   "its client was killed and a new sniff stopped"
                   % (snf0, snf1))
    health("after the stalled sniff")


def t_sniffs_x4():
    sniffs = [Sniff(n, 0, "").start() for n in (A, B, C, D)]
    bg = floods(A, "10.8.8.2", SECS) + floods(C, "10.8.8.4", SECS)
    time.sleep(1)
    rx_b, _, _ = pps(B, SECS - 3)
    dt = answers_in(10, "under four sniffs and two floods")
    wait_bg(bg, SECS)
    tot = 0
    for s in sniffs:
        n = s.stop()
        tot += n
        print("      sniff %s: %d delivered, %d missing" % (s.node, n, s.gaps))
    print("      %s under it: %.0f kpps, dump commands %.2fs" % (B, rx_b / 1000, dt))
    if tot < 4000:
        raise Fail("four sniffs under two floods delivered %d packets" % tot)
    health("after four sniffs")


def t_scp():
    tmp = tempfile.mkdtemp(prefix="testbigflows_")
    try:
        src = os.path.join(tmp, "big.bin")
        big_file(src, 128)
        want = md5_host(src)
        s = Sniff(B, 0, "").start()
        bg = floods(A, "10.8.8.2", 60)
        try:
            t = time.time()
            must(cli("scp", src, B + ":/tmp/big.bin"), "scp up under the floods")
            up = time.time() - t
            if md5_guest(B, "/tmp/big.bin") != want:
                raise Fail("128 MB up under the floods: the md5 differs")
            back = os.path.join(tmp, "back.bin")
            t = time.time()
            must(cli("scp", B + ":/tmp/big.bin", back), "scp down under the floods")
            down = time.time() - t
            if md5_host(back) != want:
                raise Fail("128 MB down under the floods: the md5 differs")
            print("      128 MB through the agent under the floods and a "
                  "sniff: up %.1f MB/s, down %.1f MB/s" % (128 / up, 128 / down))
            note_max("agent cnt", "scp up", 128 / up, "MB/s", "under the floods")
            note_max("agent cnt", "scp down", 128 / down, "MB/s", "under the floods")
        finally:
            for p in bg:
                p.kill()
            ssh(A, "pkill trafgen; pkill -f 'iperf3 -c'; true", check=False,
                quiet=True)
            s.stop()
        ssh(B, "rm -f /tmp/big.bin", quiet=True)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    health("after the scp")


def t_c2c():
    name = "%s2%s" % (NET, PEER)
    before = rss_table()
    add_cnt(P, "s", net=PEER, rootfs=FLOW_ROOTFS)
    must(cli("add", "c2c", name, PEER), "add c2c")
    end = time.time() + 60
    while time.time() < end:
        if (ti.in_topo(name) and ti.in_topo(name, PEER)
                and c2c_peered(name, NET)):
            break
        time.sleep(1)
    else:
        raise Fail("the c2c is not peered within 60s\n" + c2c_evidence(name))
    must(cli("plug", name, LAN), "plug c2c")
    must(cli("plug", name, "bflanp", net=PEER), "plug c2c peer")
    must(cli("plug", P, "eth0", "bflanp", net=PEER), "plug peer")
    wait_answer(P, PEER)
    address(P, 0, "10.8.8.9/24", net=PEER)
    end = time.time() + 30
    while not ping(A, "10.8.8.9", 1, check=False):
        if time.time() > end:
            raise Fail("nothing crosses the c2c\n" + c2c_evidence(name))
        time.sleep(1)
    iperf_server(P, net=PEER)
    snd, rcv, _ = iperf_run(A, "10.8.8.9", secs=SECS, extra="-P 4")
    mbits("c2c tcp, 4 streams", snd, rcv)
    if rcv is None or rcv < 5:
        raise Fail("tcp across the c2c: %s Mbit/s" % rcv)
    first = rcv
    note_max("c2c", "tcp 4 streams", rcv)
    snd, rcv, loss = iperf_run(A, "10.8.8.9", secs=SECS, extra="-u -b 0 -l 1400")
    mbits("c2c udp, sender unbounded", snd, rcv, loss)
    rcv, loss = udp_max(A, "10.8.8.9")
    note_max("c2c", "udp 1400B", rcv, detail="loss %.1f%%" % loss)
    trafgen_cfg(A, mac_of(P, PEER), "10.8.8.1", "10.8.8.9")
    bg = trafgen_bg(A, SECS)
    time.sleep(1)
    rx, _, mb = pps(P, SECS - 2, PEER)
    wait_bg([bg], SECS)
    print("      64-byte frames across the c2c: %.0f kpps reach %s (%.1f Mbit/s)"
          % (rx / 1000, P, mb))
    kpps, loss, note = pps_max(A, P, net_dst=PEER)
    note_max("c2c", "64B frames", kpps, "kpps",
             "loss %.1f%%%s" % (loss, note))
    trafgen_cfg(A, mac_of(B), "10.8.8.1", "10.8.8.2")
    time.sleep(3)
    if not c2c_peered(name, NET):
        raise Fail("the c2c lost its peering under the floods\n"
                   + c2c_evidence(name))
    loss, rtt = ping_stats(A, "10.8.8.9", 10)
    if loss:
        raise Fail("%d%% loss across the c2c after the floods" % loss)
    snd, rcv, _ = iperf_run(A, "10.8.8.9", secs=SECS, extra="-P 4")
    mbits("c2c tcp again", snd, rcv)
    if rcv is None or rcv < 0.5 * first:
        raise Fail("tcp across the c2c after the floods: %s Mbit/s, was %.1f"
                   % (rcv, first))
    rss_check(before, "the c2c after the floods")
    must(cli("del", name), "del c2c")
    wait_topo(name, False)
    wait_topo(name, False, net=PEER)
    must(cli("del", P, net=PEER), "del peer")


def t_a2b():
    before = rss_table()
    must(cli("add", "a2b", "bfa2b"), "add a2b")
    wait_topo("bfa2b", True)
    must(cli("unplug", D, "eth0", LAN), "unplug")
    must(cli("plug", "bfa2b", "A", LAN, ack=5), "plug")
    must(cli("plug", "bfa2b", "B", "bflanD", ack=5), "plug")
    must(cli("plug", D, "eth0", "bflanD", ack=5), "plug")
    time.sleep(1)
    ping(C, "10.8.8.4", 3)
    snd, rcv, _ = iperf_run(C, "10.8.8.4", secs=MAX_SECS, extra="-P 4")
    note_max("a2b clean", "tcp 4 streams", rcv)
    rcv, loss = udp_max(C, "10.8.8.4")
    note_max("a2b clean", "udp 1400B", rcv, detail="loss %.1f%%" % loss)
    kpps, loss, note = pps_max(C, D)
    note_max("a2b clean", "64B frames", kpps, "kpps",
             "loss %.1f%%%s" % (loss, note))
    must(cli("cnf", "bfa2b", "A2B rate=100"), "cnf rate")
    time.sleep(1)
    snd, rcv, loss = iperf_run(C, "10.8.8.4", secs=SECS, extra="-u -b 50M -l 1400")
    mbits("a2b rate 100 kB/s under 50 Mbit/s udp", snd, rcv, loss)
    if rcv is None or rcv > 2.0:
        raise Fail("rate=100 kB/s let %s Mbit/s through" % rcv)
    must(cli("cnf", "bfa2b", "A2B rate=0"), "cnf rate 0")
    must(cli("cnf", "bfa2b", "A2B delay=500"), "cnf delay")
    time.sleep(1)
    snd, rcv, loss = iperf_run(C, "10.8.8.4", secs=SECS, extra="-u -b 50M -l 1400")
    mbits("a2b delay 500 ms under 50 Mbit/s udp", snd, rcv, loss)
    rss_check(before, "the a2b holding a 500 ms queue under a flood")
    must(cli("cnf", "bfa2b", "A2B delay=0"), "cnf delay 0")
    time.sleep(2)
    loss, rtt = ping_stats(C, "10.8.8.4", 10)
    if loss or rtt is None or rtt > 50:
        raise Fail("a2b restored: loss %s%%, rtt %s ms" % (loss, rtt))
    print("      a2b restored: rtt %.1f ms" % rtt)
    must(cli("del", "bfa2b"), "del a2b")
    wait_topo("bfa2b", False)
    must(cli("unplug", D, "eth0", "bflanD"), "unplug")
    must(cli("plug", D, "eth0", LAN, ack=5), "plug")


def t_nat():
    before = rss_table()
    port = host_port_free()
    show("python3 -m http.server %d --bind 127.0.0.1" % port)
    srv = subprocess.Popen([sys.executable, "-m", "http.server", str(port),
                            "--bind", "127.0.0.1"], cwd="/tmp",
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        must(cli("add", "nat", "bfnat"), "add nat")
        wait_topo("bfnat", True)
        must(cli("plug", "bfnat", "bflanN"), "plug nat")
        must(cli("unplug", D, "eth0", LAN), "unplug")
        must(cli("plug", D, "eth0", "bflanN"), "plug")
        ssh(D, "ip addr flush dev eth0")
        address(D, 0, "10.117.0.10/24")
        ssh(D, "ip route replace default via 10.117.0.1")
        time.sleep(1)
        probe = ("exec 3<>/dev/tcp/10.117.0.1/%d; printf 'GET / HTTP/1.0\\r\\n"
                 "\\r\\n' >&3; head -c 8 <&3" % port)
        if not ssh(D, probe, check=False).startswith("HTTP/1.0"):
            raise Fail("the host server is not reached through the nat "
                       "before the flood")
        host_iperf = shutil.which("iperf3")
        if host_iperf:
            hport = host_port_free()
            show("iperf3 -s -1 -p %d -B 127.0.0.1" % hport)
            isrv = subprocess.Popen(["iperf3", "-s", "-1", "-p", str(hport),
                                     "-B", "127.0.0.1"],
                                    stdout=subprocess.DEVNULL,
                                    stderr=subprocess.DEVNULL)
            try:
                snd, rcv, loss = iperf_run(D, "10.117.0.1", secs=SECS,
                                           extra="-u -b 0 -l 1400", port=hport)
                mbits("udp flood at the host through the nat", snd, rcv, loss)
            finally:
                isrv.kill()
        else:
            gw = ssh(D, "ping -c 1 -W 2 10.117.0.1 >/dev/null; ip neigh show "
                        "10.117.0.1 | awk '{print $5}'").strip()
            if not re.match(r"^[0-9a-f:]{17}$", gw):
                raise Fail("no mac for 10.117.0.1 in the guest: %r" % gw)
            trafgen_cfg(D, gw, "10.117.0.10", "10.117.0.1")
            bg = trafgen_bg(D, SECS)
            time.sleep(1)
            _, tx, _ = pps(D, SECS - 2)
            wait_bg([bg], SECS)
            print("      64-byte frames at the nat: %.0f kpps sent (no iperf3 "
                  "on the host, the far side is not measured)" % (tx / 1000))
        time.sleep(2)
        if not ssh(D, probe, check=False).startswith("HTTP/1.0"):
            raise Fail("the host server is not reached through the nat "
                       "after the flood")
        print("      tcp through the nat after the flood: ok")
        rss_check(before, "the nat after the flood")
        must(cli("del", "bfnat"), "del nat")
        wait_topo("bfnat", False)
        must(cli("unplug", D, "eth0", "bflanN"), "unplug")
        ssh(D, "ip addr flush dev eth0")
        address(D, 0, "10.8.8.4/24")
        must(cli("plug", D, "eth0", LAN, ack=5), "plug")
        iperf_server(D)
    finally:
        srv.terminate()


def t_memory():
    print("      rss before -> after the whole run, daemons that moved:")
    rss_check(BASE["rss"], "the run")
    for net in (NET, PEER):
        erase(net)
    time.sleep(3)
    snf1 = snf_count()
    if snf1 > BASE["snf"]:
        raise Fail("%d cloonix-ovs-snf before the run, %d after del all"
                   % (BASE["snf"], snf1))


# ----------------------------------------------------- the added scenarios

def ssh_stream_down(node, rpath, local, net=None):
    """cloonix cli <net> ssh <node> cat <rpath> > local: the command output."""
    show("cloonix cli %s ssh %s 'cat %s' > %s" % (net or NET, node, rpath,
                                                 local))
    with open(local, "wb") as out:
        r = subprocess.run(ti.CLOONIX + ["cli", net or NET, "ssh", node,
                                         "cat %s" % rpath],
                           stdout=out, stderr=subprocess.PIPE,
                           stdin=subprocess.DEVNULL, timeout=600)
    if r.returncode:
        raise Fail("ssh cat %s on %s: rc %d %s" % (rpath, node, r.returncode,
                                                  r.stderr.decode()[-300:]))


def ssh_stream_up(node, local, net=None):
    """cat local | cloonix cli <net> ssh <node> md5sum: the command input."""
    show("cat %s | cloonix cli %s ssh %s md5sum" % (local, net or NET, node))
    with open(local, "rb") as src:
        r = subprocess.run(ti.CLOONIX + ["cli", net or NET, "ssh", node,
                                         "md5sum"],
                           stdin=src, capture_output=True, text=True,
                           timeout=600)
    if r.returncode or not r.stdout.split():
        raise Fail("stdin to md5sum on %s: rc %d %s" % (node, r.returncode,
                                                       r.stderr[-300:]))
    return r.stdout.split()[0]


def t_ssh_stream():
    tmp = tempfile.mkdtemp(prefix="testbigflows_")
    try:
        src = os.path.join(tmp, "big.bin")
        big_file(src, 128)
        want = md5_host(src)
        s = Sniff(B, 0, "").start()
        bg = floods(A, "10.8.8.2", 120)
        try:
            t = time.time()
            got = ssh_stream_up(B, src)
            up = time.time() - t
            if got != want:
                raise Fail("128 MB into md5sum through ssh stdin: %s, wanted %s"
                           % (got, want))
            must(cli("scp", src, B + ":/tmp/big.bin"), "scp up")
            back = os.path.join(tmp, "back.bin")
            t = time.time()
            ssh_stream_down(B, "/tmp/big.bin", back)
            down = time.time() - t
            if md5_host(back) != want:
                raise Fail("128 MB out of cat through ssh stdout: md5 differs")
            print("      128 MB by ssh stdin %.1f MB/s, by ssh stdout %.1f MB/s,"
                  " under the floods and a sniff" % (128 / up, 128 / down))
            note_max("agent cnt", "ssh stdin", 128 / up, "MB/s", "under the floods")
            note_max("agent cnt", "ssh stdout", 128 / down, "MB/s", "under the floods")
        finally:
            for p in bg:
                p.kill()
            ssh(A, "pkill trafgen; pkill -f 'iperf3 -c'; true", check=False,
                quiet=True)
            s.stop()
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    health("after the ssh streams")


def t_kvm():
    wait_answer(K1, timeout=240)
    wait_answer(K2, timeout=240)
    must(cli("plug", K1, "eth0", "bflanK", ack=5), "plug")
    must(cli("plug", K2, "eth0", "bflanK", ack=5), "plug")
    must(cli("plug", K1, "eth1", LAN, ack=5), "plug")
    time.sleep(2)
    address(K1, 0, "10.9.9.1/24")
    address(K2, 0, "10.9.9.2/24")
    address(K1, 1, "10.8.8.7/24")
    before = rss_table()
    iperf_server(K2)
    iperf_server(K1)
    snd, rcv, _ = iperf_run(K1, "10.9.9.2", secs=SECS, extra="-P 4")
    mbits("kvm sniffed to kvm not sniffed, tcp 4 streams", snd, rcv)
    if rcv is None or rcv < 50:
        raise Fail("tcp between two kvm: %s Mbit/s" % rcv)
    note_max("kvm sniffed > not sniffed", "tcp 4 streams", rcv)
    snd, rcv, _ = iperf_run(K1, "10.9.9.2", secs=SECS, extra="-P 4 -R")
    mbits("the other way", snd, rcv)
    note_max("kvm not sniffed > sniffed", "tcp 4 streams", rcv)
    snd, rcv, loss = iperf_run(K1, "10.9.9.2", secs=SECS, extra="-u -b 0 -l 1400")
    mbits("kvm to kvm udp, sender unbounded", snd, rcv, loss)
    rcv, loss = udp_max(K1, "10.9.9.2")
    note_max("kvm sniffed > not sniffed", "udp 1400B", rcv, detail="loss %.1f%%" % loss)
    sn = Sniff(K1, 0, "").start()
    snd, rcv, loss = iperf_run(K1, "10.9.9.2", secs=SECS, extra="-u -b 0 -l 1400")
    n = sn.stop()
    mbits("kvm udp under a sniff on its sniffed eth", snd, rcv, loss)
    print("      the sniff on the kvm port: %d delivered, %d missing" % (n, sn.gaps))
    if n < 1000:
        raise Fail("a sniff on a sniffed kvm eth under a udp flood delivered %d" % n)
    snd, rcv, _ = iperf_run(A, "10.8.8.7", secs=SECS, extra="-P 4")
    mbits("cnt to kvm across the container lan", snd, rcv)
    if rcv is None or rcv < 50:
        raise Fail("tcp from a cnt to a kvm: %s Mbit/s" % rcv)
    note_max("cnt > kvm sniffed", "tcp 4 streams", rcv)
    rcv, loss = udp_max(A, "10.8.8.7")
    note_max("cnt > kvm sniffed", "udp 1400B", rcv, detail="loss %.1f%%" % loss)
    tmp = tempfile.mkdtemp(prefix="testbigflows_")
    try:
        src = os.path.join(tmp, "big.bin")
        big_file(src, 96)
        want = md5_host(src)
        bg = [iperf_bg(A, "10.8.8.7", 200, extra="-P 4"),
              iperf_bg(K1, "10.9.9.2", 200, extra="-u -b 0 -l 1400")]
        try:
            t = time.time()
            must(cli("scp", src, K1 + ":/tmp/big.bin"), "scp up to a kvm")
            up = time.time() - t
            if md5_guest(K1, "/tmp/big.bin") != want:
                raise Fail("96 MB into a kvm under load: md5 differs")
            back = os.path.join(tmp, "back.bin")
            t = time.time()
            must(cli("scp", K1 + ":/tmp/big.bin", back), "scp down from a kvm")
            down = time.time() - t
            if md5_host(back) != want:
                raise Fail("96 MB out of a kvm under load: md5 differs")
            os.unlink(back)
            t = time.time()
            ssh_stream_down(K1, "/tmp/big.bin", back)
            sdown = time.time() - t
            if md5_host(back) != want:
                raise Fail("96 MB by ssh stdout from a kvm under load: md5 differs")
            print("      96 MB over the virtio serial under load: scp up %.1f, "
                  "down %.1f, ssh stdout %.1f MB/s"
                  % (96 / up, 96 / down, 96 / sdown))
            note_max("agent kvm", "scp up", 96 / up, "MB/s", "under load")
            note_max("agent kvm", "scp down", 96 / down, "MB/s", "under load")
            note_max("agent kvm", "ssh stdout", 96 / sdown, "MB/s", "under load")
        finally:
            for p in bg:
                p.kill()
            for node in (A, K1):
                ssh(node, "pkill -f 'iperf3 -c'; true", check=False, quiet=True)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    time.sleep(2)
    loss, rtt = ping_stats(K1, "10.9.9.2", 10)
    if loss:
        raise Fail("%d%% loss between the kvm after the floods" % loss)
    rss_check(before, "the kvm flows")


# ------------------------------------------------------------- pipe bounds

def syslog_wait(mark, pattern, secs=20):
    """The first line after mark that matches, None after secs."""
    rx = re.compile(pattern)
    end = time.time() + secs
    while True:
        for raw in ti.syslog_after(mark).splitlines():
            line = raw.decode("utf-8", "replace")
            if rx.search(line):
                return line.strip()
        if time.time() > end:
            return None
        time.sleep(1)


def agent_kb(node):
    """VmRSS in kB of the cloonix-agent of the guest."""
    out = ssh(node, "for p in /proc/[0-9]*; do [ \"$(cat $p/comm 2>/dev/null)\""
                    " = cloonix-agent ] && grep VmRSS $p/status; done; true",
              quiet=True, check=False)
    m = re.search(r"VmRSS:\s+(\d+)", out)
    if not m:
        raise Fail("no cloonix-agent in the /proc of %s:\n%s" % (node, out))
    return int(m.group(1))


def running(node, start):
    """How many processes of the guest have a command line starting so."""
    out = ssh(node, "for p in /proc/[0-9]*; do tr '\\0' ' ' < $p/cmdline "
                    "2>/dev/null; echo; done | grep -c '^%s'; true" % start,
              quiet=True, check=False)
    try:
        return int(out.split()[-1])
    except (ValueError, IndexError):
        raise Fail("counting '%s' in %s gave: %s" % (start, node, out))


def held_check(node, before, a0, what):
    """Host daemons and the agent of node within their bound, and a second
    pipe to node answers."""
    rss_check(before, what, bound=HELD_MB)
    a1 = agent_kb(node)
    print("      %s: the agent of %s %d -> %d kB" % (what, node, a0, a1))
    if a1 - a0 > AGENT_HELD_MB * 1024:
        raise Fail("%s: the agent of %s grew from %d to %d kB"
                   % (what, node, a0, a1))
    t = time.time()
    ssh(node, "echo alive", timeout=30, quiet=True)
    print("      %s: a second ssh to %s answered in %.1fs"
          % (what, node, time.time() - t))


def stdout_unread(node):
    before, a0 = rss_table(), agent_kb(node)
    show("cloonix cli %s ssh %s \"cat /dev/zero\"   (stdout never read)"
         % (NET, node))
    stuck = subprocess.Popen(ti.CLOONIX + ["cli", NET, "ssh", node,
                                           "cat /dev/zero"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                             stdin=subprocess.DEVNULL)
    try:
        time.sleep(20)
        if stuck.poll() is not None:
            raise Fail("ssh cat /dev/zero on %s ended by itself, rc %s"
                       % (node, stuck.returncode))
        held_check(node, before, a0, "stdout never read")
    finally:
        stuck.kill()
        stuck.wait()
    end = time.time() + 20
    while running(node, "cat /dev/zero") and time.time() < end:
        time.sleep(1)
    if running(node, "cat /dev/zero"):
        raise Fail("cat /dev/zero still runs in %s 20 s after its client "
                   "was killed" % node)


def stdin_unread(node):
    before, a0 = rss_table(), agent_kb(node)
    show("zeros | cloonix cli %s ssh %s \"sleep 25\"   (stdin never read)"
         % (NET, node))
    p = subprocess.Popen(ti.CLOONIX + ["cli", NET, "ssh", node, "sleep 25"],
                         stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL)
    sent = [0]

    def feed():
        chunk = b"\0" * 65536
        try:
            while p.poll() is None:
                p.stdin.write(chunk)
                sent[0] += len(chunk)
        except (OSError, ValueError):
            pass

    th = threading.Thread(target=feed, daemon=True)
    th.start()
    try:
        time.sleep(15)
        held_check(node, before, a0, "stdin never read")
        print("      %d MB taken from the feeder in 15 s" % (sent[0] >> 20))
        try:
            p.wait(timeout=60)
        except subprocess.TimeoutExpired:
            raise Fail("ssh sleep 25 on %s with its stdin fed did not end in "
                       "75 s" % node)
    finally:
        if p.poll() is None:
            p.kill()
            p.wait()
        th.join(5)


def pipe_limit(node):
    n = MAX_PIPES + 6
    mark = ti.syslog_mark()
    show("%d x cloonix cli %s ssh %s \"sleep 20\" &" % (n, NET, node))
    procs = [subprocess.Popen(ti.CLOONIX + ["cli", NET, "ssh", node,
                                            "sleep 20"],
                              stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL,
                              stdin=subprocess.DEVNULL) for _ in range(n)]
    try:
        for q in procs:
            q.wait(timeout=90)
    except subprocess.TimeoutExpired:
        raise Fail("%d pipes at once to %s: not all ended in 90 s" % (n, node))
    finally:
        for q in procs:
            if q.poll() is None:
                q.kill()
    ok = sum(1 for q in procs if q.returncode == 0)
    print("      %d pipes at once to %s: %d went through, %d refused"
          % (n, node, ok, n - ok))
    if ok == n or ok > MAX_PIPES:
        raise Fail("%d pipes at once to %s: %d went through, the agent holds "
                   "%d at most" % (n, node, ok, MAX_PIPES))
    if ok < MAX_PIPES - 5:
        raise Fail("%d pipes at once to %s: only %d went through"
                   % (n, node, ok))
    line = syslog_wait(mark, r"KERR: %s guest %s: .*open already, the most"
                             % (NET, node))
    if not line:
        raise Fail("the refusal of the agent of %s is not in %s as a \"guest "
                   "%s:\" KERR line" % (node, SYSLOG, node))
    print("      in the syslog of the net: %s" % line[line.index("KERR"):])


def cxp_kerr(node):
    rpath = "/var/tmp/bf_big.bin"
    ssh(node, "dd if=/dev/zero of=%s bs=1M count=160 2>/dev/null" % rpath)
    tmp = tempfile.mkdtemp(prefix="testbigflows_")
    local = os.path.join(tmp, "part.bin")
    mark = ti.syslog_mark()
    show("cloonix cli %s scp %s:%s %s   (killed half way)"
         % (NET, node, rpath, local))
    p = subprocess.Popen(ti.CLOONIX + ["cli", NET, "scp",
                                       "%s:%s" % (node, rpath), local],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         stdin=subprocess.DEVNULL)
    try:
        end = time.time() + 60
        while time.time() < end:
            if os.path.exists(local) and os.path.getsize(local) > (8 << 20):
                break
            if p.poll() is not None:
                raise Fail("scp of 160 MB from %s ended before 8 MB arrived, "
                           "rc %s" % (node, p.returncode))
            time.sleep(0.1)
        else:
            raise Fail("scp from %s: 8 MB did not arrive in 60 s" % node)
        p.kill()
        p.wait()
        got = os.path.getsize(local)
        if got >= (160 << 20):
            raise Fail("scp from %s finished before it was killed" % node)
        line = syslog_wait(mark, r"KERR: %s guest %s: cloonix-cxp-srv: WARNING "
                                 r"get %s" % (NET, node, rpath))
        if not line:
            raise Fail("the WARNING of the exec server of %s about the broken "
                       "get is not in %s" % (node, SYSLOG))
        print("      killed at %d MB; in the syslog of the net: %s"
              % (got >> 20, line[line.index("KERR"):]))
    finally:
        if p.poll() is None:
            p.kill()
        shutil.rmtree(tmp, ignore_errors=True)
        ssh(node, "rm -f %s" % rpath, check=False, quiet=True)


def t_pipe_bounds():
    for node in (B, K2):
        stdout_unread(node)
        stdin_unread(node)
        pipe_limit(node)
        cxp_kerr(node)
    health("after the pipe bounds")


# -------------------------------------------------------------------- airs

def hwsim_host():
    if os.path.isdir("/sys/module/mac80211_hwsim"):
        return
    r = subprocess.run(["sudo", "-n", "modprobe", "mac80211_hwsim",
                        "radios=0"], capture_output=True, text=True)
    if r.returncode:
        raise Fail("the cnt radios need mac80211_hwsim on the host: "
                   "sudo modprobe mac80211_hwsim radios=0")


def wait_wlan0(node, secs=90):
    end = time.time() + secs
    while "wlan0" not in ssh(node, "iw dev", check=False, quiet=True):
        if time.time() > end:
            raise Fail("no wlan0 in %s after %d s" % (node, secs))
        time.sleep(2)


def ibss(node, addr):
    ssh(node, "iw dev wlan0 set type ibss")
    ssh(node, "ip link set dev wlan0 up")
    ssh(node, "iw dev wlan0 ibss join bfcell 2412")
    ssh(node, "ip addr add %s/24 dev wlan0" % addr)


def wait_cell(node, secs=40):
    end = time.time() + secs
    while "IBSS" not in ssh(node, "iw dev wlan0 link", check=False, quiet=True):
        if time.time() > end:
            raise Fail("wlan0 of %s joined no cell in %d s" % (node, secs))
        time.sleep(2)


def air_ping(src, dst, secs=40):
    """IBSS peers find each other in a few beacons: pinged until it goes."""
    end = time.time() + secs
    while not ping(src, dst, 3, check=False):
        if time.time() > end:
            raise Fail("%s does not ping %s over the air in %d s"
                       % (src, dst, secs))
        time.sleep(2)


def medium_held(mark, what):
    """A lost medium leaves the cnt radios to the kernel, bypassing cloonix:
    the flows still go, so the syslog is what tells."""
    line = syslog_wait(mark, r"medium socket closed|process of the medium "
                       r"ended", secs=0)
    if line:
        raise Fail("%s: the medium of the airs was lost\n  %s"
                   % (what, line[line.index("KERR"):]))


def air_max(src, dst, label):
    snd, rcv, _ = iperf_run(src, dst, secs=SECS, extra="-P 4")
    mbits("%s, tcp 4 streams" % label, snd, rcv)
    if rcv is None or rcv < 1:
        raise Fail("tcp over the %s: %s Mbit/s" % (label, rcv))
    note_max(label, "tcp 4 streams", rcv)
    rcv, loss = udp_max(src, dst)
    note_max(label, "udp 1400B", rcv, detail="loss %.1f%%" % loss)


def air_churn(src, dst, label):
    """Containers added and deleted on the net while src pings dst over an
    air: the medium must keep relaying, no ping lost, none late."""
    cmd = "ping -c 150 -i 0.2 -W 1 %s" % dst
    show('cloonix cli %s ssh %s "%s" &' % (NET, src, cmd))
    p = subprocess.Popen(ti.CLOONIX + ["cli", NET, "ssh", src, cmd],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         text=True, stdin=subprocess.DEVNULL)
    names = ["bfX%d" % i for i in range(3)]
    try:
        time.sleep(2)
        for name in names:
            must(cli("add", "cnt", name, "eth=1", FLOW_ROOTFS), "add cnt")
        for name in names:
            wait_answer(name)
        for name in names:
            must(cli("del", name), "del %s" % name)
        out, _ = p.communicate(timeout=60)
    finally:
        if p.poll() is None:
            p.kill()
    m = re.search(r"([\d.]+)% packet loss", out)
    r = re.search(r"= [\d.]+/[\d.]+/([\d.]+)/", out)
    if not m or not r:
        raise Fail("ping over the air under container churn gave:\n%s"
                   % out[-600:])
    loss, worst = float(m.group(1)), float(r.group(1))
    print("      %s under container adds and dels: %g%% loss, worst rtt "
          "%.0f ms" % (label, loss, worst))
    if loss or worst > 500:
        raise Fail("the air stalls while containers come and go (%s): %g%% "
                   "loss, worst rtt %.0f ms" % (label, loss, worst))


def t_airs():
    hwsim_host()
    mark = ti.syslog_mark()
    before = rss_table()
    for name in (W1, W2):
        must(cli("add", "cnt", name, "eth=1", FLOW_ROOTFS, "wlan=1"),
             "add cnt with a radio")
    for name in (W1, W2):
        wait_answer(name)
    for name in (W1, W2, K1, K2):
        wait_wlan0(name)
    for name, air in ((W1, AIR_C), (W2, AIR_C), (K1, AIR_K), (K2, AIR_K)):
        must(cli("plug", name, "wlan0", air), "plug %s wlan0 %s" % (name, air))
    for name, addr in ((W1, "10.47.9.1"), (W2, "10.47.9.2"),
                       (K1, "10.47.9.3"), (K2, "10.47.9.4")):
        ibss(name, addr)
    for name in (W1, W2, K1, K2):
        wait_cell(name)
    air_ping(W1, "10.47.9.2")
    air_ping(K1, "10.47.9.4")
    air_churn(W1, "10.47.9.2", "air cnt>cnt")
    air_churn(K1, "10.47.9.4", "air kvm>kvm")
    iperf_server(W2)
    iperf_server(K2)
    air_max(W1, "10.47.9.2", "air cnt>cnt")
    medium_held(mark, "air cnt>cnt at its max")
    air_max(K1, "10.47.9.4", "air kvm>kvm")
    medium_held(mark, "air kvm>kvm at its max")

    # the virtio serial of K2 carries its radio and an scp at once
    tmp = tempfile.mkdtemp(prefix="testbigflows_")
    try:
        src = os.path.join(tmp, "big.bin")
        big_file(src, 64)
        want = md5_host(src)
        bg = [udp_bg(K1, "10.47.9.4", 120)]
        try:
            t = time.time()
            must(cli("scp", src, K2 + ":/tmp/air.bin"),
                 "scp into a kvm whose radio is flooded")
            dt = time.time() - t
            if md5_guest(K2, "/tmp/air.bin") != want:
                raise Fail("64 MB into %s under a flood of its radio: the md5 "
                           "differs" % K2)
            print("      64 MB into %s under a flood of its radio: %.1f MB/s"
                  % (K2, 64 / dt))
        finally:
            for p in bg:
                p.kill()
            ssh(K1, "pkill -f 'iperf3 -c'; true", check=False, quiet=True)
            ssh(K2, "rm -f /tmp/air.bin", check=False, quiet=True)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    # cnt to kvm: K2 onto the air of the cnt radios
    medium_held(mark, "a kvm radio flooded")
    must(cli("plug", K2, "wlan0", AIR_C), "plug %s wlan0 %s" % (K2, AIR_C))
    wait_cell(K2)
    air_ping(W1, "10.47.9.4", secs=90)
    iperf_server(K2)
    air_max(W1, "10.47.9.4", "air cnt>kvm")
    medium_held(mark, "air cnt>kvm at its max")

    # a flooded air under a sniff read, then under one never read
    must(cli("cnf", AIR_C, "sniff=on"), "cnf %s sniff=on" % AIR_C)
    bg = [udp_bg(W1, "10.47.9.2", SECS + 4)]
    time.sleep(1)
    rate, mb, dropped = count_sniff(AIR_C, SECS, iface=None)
    wait_bg(bg, SECS + 4)
    print("      the air %s read under a udp flood: %.0f pkt/s, %.1f Mbit/s, "
          "%d dropped" % (AIR_C, rate, mb, dropped))
    if rate < 100:
        raise Fail("a sniff of a flooded air saw %.0f pkt/s" % rate)
    show("cloonix cli %s sniff %s --bytes   (stdout never read)" % (NET, AIR_C))
    stuck = subprocess.Popen(ti.CLOONIX + ["cli", NET, "sniff", AIR_C,
                                           "--bytes"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                             stdin=subprocess.DEVNULL)
    try:
        bg = [udp_bg(W1, "10.47.9.2", SECS + 4)]
        time.sleep(SECS)
        answers_in(10, "a flooded air under a sniff never read")
        rss_check(before, "a flooded air under a sniff never read",
                  bound=HELD_MB)
        wait_bg(bg, SECS + 4)
    finally:
        stuck.kill()
        stuck.wait()
    must(cli("cnf", AIR_C, "sniff=off"), "cnf %s sniff=off" % AIR_C)
    time.sleep(2)
    loss, rtt = ping_stats(W1, "10.47.9.2", 10)
    if loss:
        raise Fail("%d%% loss on the air after the floods" % loss)
    medium_held(mark, "the airs flooded and sniffed")
    for name in (W1, W2):
        must(cli("del", name), "del %s" % name)
    rss_check(before, "the airs", bound=HELD_MB)



def t_c2c_both_ways():
    name = "%s2%s" % (NET, PEER)
    before = rss_table()
    add_cnt(P, "s", net=PEER, rootfs=FLOW_ROOTFS)
    must(cli("add", "c2c", name, PEER), "add c2c")
    end = time.time() + 60
    while time.time() < end:
        if (ti.in_topo(name) and ti.in_topo(name, PEER)
                and c2c_peered(name, NET)):
            break
        time.sleep(1)
    else:
        raise Fail("the c2c is not peered within 60s\n" + c2c_evidence(name))
    must(cli("plug", name, LAN), "plug c2c")
    must(cli("plug", name, "bflanp", net=PEER), "plug c2c peer")
    must(cli("plug", P, "eth0", "bflanp", net=PEER), "plug peer")
    wait_answer(P, PEER)
    address(P, 0, "10.8.8.9/24", net=PEER)
    end = time.time() + 30
    while not ping(A, "10.8.8.9", 1, check=False):
        if time.time() > end:
            raise Fail("nothing crosses the c2c\n" + c2c_evidence(name))
        time.sleep(1)
    iperf_server(P, net=PEER)
    iperf_server(A)
    # tcp 8 streams each way at once, 1400-byte frames each way, 64-byte
    # frames each way, then the c2c is asked whether it is still there.
    bg = [iperf_bg(P, "10.8.8.1", SECS, extra="-P 8", net=PEER)]
    snd, rcv, _ = iperf_run(A, "10.8.8.9", secs=SECS, extra="-P 8")
    wait_bg(bg, SECS)
    mbits("c2c tcp 8 streams, with 8 the other way at once", snd, rcv)
    # No iperf3 (an unbounded flood kills its control connection): 1400-byte
    # frames both ways, counted on each receiving eth.
    mac_p, mac_a = mac_of(P, PEER), mac_of(A)
    trafgen_cfg(A, mac_p, "10.8.8.1", "10.8.8.9", size=1400)
    trafgen_cfg(P, mac_a, "10.8.8.9", "10.8.8.1", size=1400, net=PEER)
    bg = [trafgen_bg(A, SECS), trafgen_bg(P, SECS, net=PEER)]
    time.sleep(1)
    rxa0, txa0, ba0 = counters(A)
    rxp0, txp0, bp0 = counters(P, PEER)
    time.sleep(SECS - 3)
    rxa1, txa1, ba1 = counters(A)
    rxp1, txp1, bp1 = counters(P, PEER)
    wait_bg(bg, SECS)
    dt = float(SECS - 3)
    print("      1400-byte frames both ways: %s sends %.0f Mbit/s, %.0f reach "
          "%s; %s sends %.0f Mbit/s, %.0f reach %s"
          % (A, (txa1 - txa0) * 1400 * 8 / 1e6 / dt,
             (bp1 - bp0) * 8 / 1e6 / dt, P,
             P, (txp1 - txp0) * 1400 * 8 / 1e6 / dt,
             (ba1 - ba0) * 8 / 1e6 / dt, A))
    if bp1 - bp0 < 1400 * 100 or ba1 - ba0 < 1400 * 100:
        raise Fail("the c2c passes almost nothing under 1400-byte frames both "
                   "ways\n" + c2c_evidence(name))
    trafgen_cfg(A, mac_p, "10.8.8.1", "10.8.8.9")
    trafgen_cfg(P, mac_a, "10.8.8.9", "10.8.8.1", net=PEER)
    bg = [trafgen_bg(A, SECS), trafgen_bg(P, SECS, net=PEER)]
    time.sleep(1)
    rx, _, mb = pps(P, SECS - 3, PEER)
    dt = answers_in(10, "under the c2c pps floods both ways")
    wait_bg(bg, SECS)
    print("      64-byte frames both ways: %.0f kpps reach %s, dump commands %.2fs"
          % (rx / 1000, P, dt))
    trafgen_cfg(A, mac_of(B), "10.8.8.1", "10.8.8.2")
    time.sleep(3)
    if not c2c_peered(name, NET):
        raise Fail("the c2c lost its peering under the two-way floods\n"
                   + c2c_evidence(name))
    loss, rtt = ping_stats(A, "10.8.8.9", 20)
    if loss:
        raise Fail("%d%% loss across the c2c after the two-way floods" % loss)
    snd, rcv, _ = iperf_run(A, "10.8.8.9", secs=SECS, extra="-P 4")
    mbits("c2c tcp after it all", snd, rcv)
    if rcv is None or rcv < 5:
        raise Fail("tcp across the c2c after the floods: %s Mbit/s" % rcv)
    rss_check(before, "the c2c after the two-way floods")
    # left up for the all-at-once scenario


def t_a2b_more():
    before = rss_table()
    must(cli("add", "a2b", "bfa2b"), "add a2b")
    wait_topo("bfa2b", True)
    must(cli("unplug", D, "eth0", LAN), "unplug")
    must(cli("plug", "bfa2b", "A", LAN, ack=5), "plug")
    must(cli("plug", "bfa2b", "B", "bflanD", ack=5), "plug")
    must(cli("plug", D, "eth0", "bflanD", ack=5), "plug")
    time.sleep(1)
    ping(C, "10.8.8.4", 3)
    iperf_server(C)
    snd, rcv, _ = iperf_run(C, "10.8.8.4", secs=SECS, extra="-P 4")
    mbits("a2b clean, tcp 4 streams", snd, rcv)
    clean = rcv or 0
    # First the frame rate a clean a2b passes, flat out: its own ceiling.
    trafgen_cfg(C, mac_of(D), "10.8.8.3", "10.8.8.4")
    bg = trafgen_bg(C, SECS)
    time.sleep(1)
    rx, _, _ = pps(D, SECS - 3)
    _, tx, _ = pps(C, 1)
    wait_bg([bg], SECS)
    print("      a2b clean, 64-byte frames flat out: %.0f kpps offered, %.0f "
          "kpps come out" % (tx / 1000, rx / 1000))
    # A lossy cable kills iperf3's control connection: loss read on frames
    # counted at both ends, paced under the a2b ceiling, and on a ping.
    for cnf in ("A2B loss=1000", "B2A loss=1000"):
        must(cli("cnf", "bfa2b", cnf), "cnf a2b")
    sent, got = frames_run(C, D, SECS, gap="500us")
    lost = frame_loss(sent, got)
    print("      a2b loss=1000 each way, frames paced at 2 kpps: %.0f pps sent, "
          "%.0f pps out the other end (%.1f%% lost)" % (sent, got, lost))
    if not 5 <= lost <= 20:
        raise Fail("loss=1000 (10%%) on a paced flood: %.1f%% of the frames "
                   "lost" % lost)
    loss, rtt = ping_stats(C, "10.8.8.4", 300)
    print("      a2b loss=1000 each way, ping: %d%% lost" % loss)
    if not 8 <= loss <= 35:
        raise Fail("loss=1000 each way: a ping reports %d%% lost" % loss)
    for cnf in ("A2B loss=0", "B2A loss=0",
                "A2B silentms=200", "B2A silentms=200"):
        must(cli("cnf", "bfa2b", cnf), "cnf a2b")
    time.sleep(1)
    bg = trafgen_bg(C, SECS)
    time.sleep(1)
    rx, _, _ = pps(D, SECS - 3)
    loss, rtt = ping_stats(C, "10.8.8.4", 200)
    wait_bg([bg], SECS)
    print("      a2b silentms=200 each way under frames: %.0f kpps come out, "
          "ping %d%% lost, rtt avg %s ms" % (rx / 1000, loss, rtt))
    for cnf in ("A2B silentms=0", "B2A silentms=0",
                "A2B rate=5000", "B2A rate=5000"):
        must(cli("cnf", "bfa2b", cnf), "cnf a2b")
    # No iperf3, as for the c2c (40 Mbit/s limiter: 97% loss, client hangs):
    # 1400-byte frames both ways, counted on each receiving eth.
    mac_c, mac_d = mac_of(C), mac_of(D)
    trafgen_cfg(C, mac_d, "10.8.8.3", "10.8.8.4", size=1400)
    trafgen_cfg(D, mac_c, "10.8.8.4", "10.8.8.3", size=1400)
    bg = [trafgen_bg(C, SECS), trafgen_bg(D, SECS)]
    time.sleep(1)
    _, _, bc0 = counters(C)
    _, _, bd0 = counters(D)
    time.sleep(SECS - 3)
    _, _, bc1 = counters(C)
    _, _, bd1 = counters(D)
    wait_bg(bg, SECS)
    dt = float(SECS - 3)
    out_d = (bd1 - bd0) * 8 / 1e6 / dt
    out_c = (bc1 - bc0) * 8 / 1e6 / dt
    print("      a2b rate 5000 kB/s each way, 1400-byte frames flat out both "
          "ways: %.1f Mbit/s reach %s, %.1f reach %s" % (out_d, D, out_c, C))
    if out_d > 60 or out_c > 60:
        raise Fail("rate=5000 kB/s (40 Mbit/s) let %.0f / %.0f Mbit/s through"
                   % (out_d, out_c))
    if out_d < 10 or out_c < 10:
        raise Fail("rate=5000 kB/s (40 Mbit/s) passes only %.0f / %.0f Mbit/s"
                   % (out_d, out_c))
    trafgen_cfg(C, mac_d, "10.8.8.3", "10.8.8.4")
    bg = trafgen_bg(C, SECS)
    time.sleep(1)
    rx, _, _ = pps(D, SECS - 3)
    wait_bg([bg], SECS)
    print("      64-byte frames into a rate-limited a2b: %.0f kpps come out"
          % (rx / 1000))
    for cnf in ("A2B rate=0", "B2A rate=0"):
        must(cli("cnf", "bfa2b", cnf), "cnf a2b")
    time.sleep(2)
    snd, rcv, _ = iperf_run(C, "10.8.8.4", secs=SECS, extra="-P 4")
    mbits("a2b restored, tcp 4 streams", snd, rcv)
    if rcv is None or rcv < 0.5 * clean:
        raise Fail("a2b restored gives %s Mbit/s, clean gave %.0f" % (rcv, clean))
    rss_check(before, "the a2b after it all")
    # left in place for the all-at-once scenario


def t_nat_more():
    before = rss_table()
    port = host_port_free()
    show("python3 -m http.server %d --bind 127.0.0.1" % port)
    srv = subprocess.Popen([sys.executable, "-m", "http.server", str(port),
                            "--bind", "127.0.0.1"], cwd="/tmp",
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        must(cli("add", "nat", "bfnat"), "add nat")
        wait_topo("bfnat", True)
        must(cli("plug", "bfnat", "bflanN"), "plug nat")
        must(cli("plug", K2, "eth1", "bflanN"), "plug")
        ssh(K2, "timeout 30 dhclient -1 eth1")
        got = ssh(K2, "ip -4 -o addr show dev eth1")
        if "10.117.0.10/" not in got:
            raise Fail("dhclient on the kvm did not get 10.117.0.10: %s" % got)
        probe = ("exec 3<>/dev/tcp/10.117.0.1/%d; printf 'GET / HTTP/1.0\\r\\n"
                 "\\r\\n' >&3; head -c 8 <&3" % port)
        if not ssh(K2, probe, check=False).startswith("HTTP/1.0"):
            raise Fail("the host server is not reached from the kvm through "
                       "the nat")
        if shutil.which("iperf3"):
            hport = host_port_free()
            show("iperf3 -s -p %d -B 127.0.0.1 -D" % hport)
            isrv = subprocess.Popen(["iperf3", "-s", "-p", str(hport), "-B",
                                     "127.0.0.1"], stdout=subprocess.DEVNULL,
                                    stderr=subprocess.DEVNULL)
            try:
                time.sleep(1)
                snd, rcv, _ = iperf_run(K2, "10.117.0.1", secs=SECS,
                                        extra="-P 4", port=hport)
                mbits("kvm to the host through the nat, tcp 4 streams", snd, rcv)
                if rcv is None or rcv < 5:
                    raise Fail("tcp through the nat: %s Mbit/s" % rcv)
                note_max("nat kvm > host", "tcp 4 streams", rcv)
                rcv, loss = udp_max(K2, "10.117.0.1", port=hport)
                note_max("nat kvm > host", "udp 1400B", rcv,
                         detail="loss %.1f%%" % loss)
                snd, rcv, loss = iperf_run(K2, "10.117.0.1", secs=SECS,
                                           extra="-u -b 0 -l 1400", port=hport)
                mbits("kvm to the host through the nat, udp unbounded", snd,
                      rcv, loss)
            finally:
                isrv.kill()
            # the other way: the host into a published port of the guest
            pport = host_port_free()
            # iperf3 udp keeps its control connection on tcp: both are needed
            must(cli("cnf", "bfnat", "publish=%d:5201/tcp" % pport,
                     "publish=%d:5201/udp" % pport), "cnf publish")
            iperf_server(K2)
            end = time.time() + 15
            while time.time() < end and not ti.host_connect(pport):
                time.sleep(1)
            for extra, label in (("-P 4", "host into the guest by publish=, "
                                          "tcp 4 streams"),
                                 ("-u -b 0 -l 1400", "host into the guest by "
                                                     "publish=, udp unbounded")):
                show("iperf3 -c 127.0.0.1 -p %d -t %d -f m %s" % (pport, SECS,
                                                                  extra))
                # the single-threaded server resets a control connection made while it
                # tears the last test down: three tries 2 s apart
                for attempt in range(3):
                    r = subprocess.run(["iperf3", "-c", "127.0.0.1", "-p",
                                        str(pport), "-t", str(SECS), "-f",
                                        "m"] + extra.split(),
                                       capture_output=True, text=True,
                                       timeout=SECS + 60)
                    snd, rcv, loss = iperf_parse(r.stdout)
                    if snd is not None or rcv is not None:
                        break
                    if "control message" not in r.stdout + r.stderr:
                        break
                    time.sleep(2)
                if snd is None and rcv is None:
                    srv = ssh(K2, "tail -20 /tmp/iperf3_s.log 2>/dev/null; "
                                  "true", check=False)
                    raise Fail("iperf3 from the host into the published port:"
                               "\n%s\n  the server in the guest said:\n%s"
                               % ((r.stdout + r.stderr).strip()[-400:],
                                  srv.strip()[-600:]))
                mbits(label, snd, rcv, loss)
            must(cli("cnf", "bfnat", "publish=none"), "cnf none")
        else:
            print("      (no iperf3 on the host: only the frame flood at the nat)")
        time.sleep(2)
        if not ssh(K2, probe, check=False).startswith("HTTP/1.0"):
            raise Fail("the host server is not reached through the nat after "
                       "the floods")
        print("      tcp through the nat after the floods: ok")
        rss_check(before, "the nat after the floods")
    finally:
        srv.terminate()
    # left in place for the all-at-once scenario


def tap_side():
    """The argv prefix that runs a command where the tap is: the host, or,
    for a toy, the network namespace of its container init."""
    if not ti.TOY:
        return []
    user = os.environ.get("USER") or getpass.getuser()
    r = subprocess.run(["pgrep", "-f", "cloonix-init-%s-%s" % (user, NET)],
                       capture_output=True, text=True)
    pids = r.stdout.split()
    if not pids:
        raise Fail("toy %s: no cloonix-init-%s-%s process to enter"
                   % (NET, user, NET))
    return ["sudo", "-n", ti.sudo_path("nsenter"), "-t", pids[0], "-n"]


def tap_run(side, *args, root=False):
    """One command on the tap side; root through sudo on the host."""
    argv = list(args)
    if side:
        argv = side + [ti.sudo_path(argv[0])] + argv[1:]
    elif root:
        argv = ["sudo", "-n", ti.sudo_path(argv[0])] + argv[1:]
    show(" ".join(argv))
    return subprocess.run(argv, capture_output=True, text=True)


def tap_rx_packets(side):
    """rx packets of bftap from /proc/net/dev, which follows the netns of
    the reader where /sys does not."""
    for line in tap_run(side, "cat", "/proc/net/dev").stdout.split("\n"):
        if line.strip().startswith("bftap:"):
            return int(line.split(":", 1)[1].split()[1])
    raise Fail("bftap is not in /proc/net/dev on the tap side")


def t_tap():
    if not (shutil.which("iperf3") and ti.sudo_ok()):
        print("      (needs iperf3 on the host and passwordless sudo: skipped)")
        return
    side = tap_side()
    before = rss_table()
    tapnet = ti.TAP_NET
    must(cli("add", "tap", "bftap"), "add tap")
    wait_topo("bftap", True)
    must(cli("plug", "bftap", LAN), "plug tap")
    must(tap_run(side, "ip", "addr", "add", tapnet + ".1/24", "dev", "bftap",
                 root=True), "address on the tap")
    must(tap_run(side, "ip", "link", "set", "bftap", "up", root=True),
         "tap up")
    ssh(C, "ip addr add %s.3/24 dev eth0" % tapnet)
    time.sleep(1)
    ping(C, tapnet + ".1", 3)
    hport = host_port_free()
    argv = ["iperf3", "-s", "-p", str(hport), "-B", tapnet + ".1"]
    if side:
        argv = side + [ti.sudo_path("iperf3")] + argv[1:]
    show(" ".join(argv))
    isrv = subprocess.Popen(argv, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    try:
        time.sleep(1)
        snd, rcv, _ = iperf_run(C, tapnet + ".1", secs=SECS, extra="-P 4",
                                port=hport)
        mbits("cnt to the host through the tap, tcp 4 streams", snd, rcv)
        if rcv is None or rcv < 50:
            raise Fail("tcp through the tap: %s Mbit/s" % rcv)
        note_max("tap cnt > host", "tcp 4 streams", rcv)
        snd, rcv, loss = iperf_run(C, tapnet + ".1", secs=SECS,
                                   extra="-u -b 0 -l 1400", port=hport)
        mbits("cnt to the host through the tap, udp unbounded", snd, rcv, loss)
        rcv, loss = udp_max(C, tapnet + ".1", port=hport)
        note_max("tap cnt > host", "udp 1400B", rcv, detail="loss %.1f%%" % loss)
        snd, rcv, _ = iperf_run(C, tapnet + ".1", secs=SECS, extra="-P 4 -R",
                                port=hport)
        mbits("host to the cnt through the tap, tcp 4 streams", snd, rcv)
        note_max("tap host > cnt", "tcp 4 streams", rcv)
        link = tap_run(side, "ip", "-o", "link", "show", "bftap").stdout
        hm = link.split("link/ether")[1].split()[0] if "link/ether" in link \
             else ""
        if not hm:
            raise Fail("no mac for bftap on the tap side: %s" % link.strip())
        trafgen_cfg(C, hm, tapnet + ".3", tapnet + ".1")
        bg = trafgen_bg(C, SECS)
        time.sleep(1)
        rx0 = tap_rx_packets(side)
        time.sleep(SECS - 3)
        rx1 = tap_rx_packets(side)
        wait_bg([bg], SECS)
        print("      64-byte frames at the tap: %.0f kpps reach the host"
              % ((rx1 - rx0) / 1000.0 / (SECS - 3)))
        note_max("tap cnt > host", "64B frames", (rx1 - rx0) / 1000.0 / (SECS - 3),
                 "kpps", "flat out, what reaches the host")
        trafgen_cfg(C, mac_of(D), "10.8.8.3", "10.8.8.4")
    finally:
        # SIGTERM: through sudo it reaches the iperf3 server, SIGKILL would not.
        isrv.terminate()
        try:
            isrv.wait(timeout=5)
        except subprocess.TimeoutExpired:
            isrv.kill()
            isrv.wait()
    ssh(C, "ip addr del %s.3/24 dev eth0" % tapnet, check=False)
    must(cli("del", "bftap"), "del tap")
    wait_topo("bftap", False)
    rss_check(before, "the tap after the floods")


def t_all_at_once():
    """Everything at once for LONG seconds, then every path asked whether it
    still works: the day a user does everything on one machine."""
    name = "%s2%s" % (NET, PEER)
    before = rss_table()
    snf0 = snf_count()
    tmp = tempfile.mkdtemp(prefix="testbigflows_")
    procs = []
    stuck = None
    sniffs = []
    try:
        src = os.path.join(tmp, "big.bin")
        big_file(src, 64)
        want = md5_host(src)
        sniffs = [Sniff(B, 0, "").start(), Sniff(K1, 0, "").start(),
                  Sniff(D, 0, "udp").start()]
        show("cloonix cli %s sniff %s eth0 --bytes   (stdout never read)" % (NET, A))
        stuck = subprocess.Popen(ti.CLOONIX + ["cli", NET, "sniff", A, "eth0",
                                               "--bytes"],
                                 stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        procs += floods(A, "10.8.8.2", LONG)
        procs += [iperf_bg(A, "10.8.8.9", LONG, extra="-P 4"),
                  iperf_bg(P, "10.8.8.1", LONG, extra="-u -b 0 -l 1400", net=PEER),
                  iperf_bg(C, "10.8.8.4", LONG, extra="-u -b 0 -l 1400"),
                  iperf_bg(K1, "10.9.9.2", LONG, extra="-P 4")]
        if shutil.which("iperf3"):
            hport = host_port_free()
            isrv = subprocess.Popen(["iperf3", "-s", "-p", str(hport), "-B",
                                     "127.0.0.1"], stdout=subprocess.DEVNULL,
                                    stderr=subprocess.DEVNULL)
            procs.append(isrv)
            time.sleep(1)
            procs.append(iperf_bg(K2, "10.117.0.1", LONG,
                                  extra="-u -b 0 -l 1400", port=hport))
        end = time.time() + LONG
        rounds = 0
        worst = 0.0
        while time.time() < end - 20:
            t = time.time()
            must(cli("scp", src, B + ":/tmp/all.bin"), "scp up in the storm")
            back = os.path.join(tmp, "back.bin")
            must(cli("scp", B + ":/tmp/all.bin", back), "scp down in the storm")
            if md5_host(back) != want:
                raise Fail("64 MB round trip in the storm: md5 differs")
            if ssh_stream_up(K1, src) != want:
                raise Fail("64 MB by ssh stdin to a kvm in the storm: md5 differs")
            dt = answers_in(10, "in the storm")
            worst = max(worst, dt)
            rounds += 1
            print("      [%4.0fs] round %d: scp up+down and ssh stdin %.0fs, "
                  "dump commands %.2fs" % (time.time() - T0, rounds,
                                       time.time() - t, dt))
            during = rss_table()
            grown = [(d, before[d], during[d]) for d in DAEMONS
                     if during[d] - before[d] > RSS_BOUND_MB]
            if grown:
                raise Fail("in the storm: memory climbed past %d MB: %s"
                           % (RSS_BOUND_MB, ", ".join("%s %.0f->%.0f" % g
                                                      for g in grown)))
        print("      %d rounds in %ds, worst dump commands %.2fs" % (rounds, LONG,
                                                                  worst))
    finally:
        for p in procs:
            p.kill()
        if stuck:
            stuck.kill()
        for node in (A, C, K1, K2):
            ssh(node, "pkill trafgen; pkill -f 'iperf3 -c'; true", check=False,
                quiet=True)
        ssh(P, "pkill -f 'iperf3 -c'; true", net=PEER, check=False, quiet=True)
        for s in sniffs:
            n = s.stop()
            print("      sniff %s: %d delivered, %d missing" % (s.node, n, s.gaps))
        shutil.rmtree(tmp, ignore_errors=True)
    time.sleep(5)
    # after the storm, every path
    if not c2c_peered(name, NET):
        raise Fail("the c2c lost its peering in the storm\n" + c2c_evidence(name))
    for node, target, net in ((A, "10.8.8.2", NET), (A, "10.8.8.9", NET),
                              (C, "10.8.8.4", NET), (K1, "10.9.9.2", NET),
                              (K2, "10.117.0.1", NET), (P, "10.8.8.1", PEER)):
        loss, rtt = ping_stats(node, target, 10, net=net)
        if loss:
            raise Fail("after the storm: %s -> %s %d%% loss" % (node, target,
                                                                 loss))
        print("      after the storm: %s -> %s rtt %.1f ms" % (node, target, rtt))
    s = Sniff(A, 0, "icmp").start()
    ping(A, "10.8.8.2", 3)
    if s.stop() < 6:
        raise Fail("a fresh sniff after the storm counts nothing")
    snf1 = snf_count()
    if snf1 > snf0 + 3:
        raise Fail("%d cloonix-ovs-snf before the storm, %d after"
                   % (snf0, snf1))
    rss_check(before, "the storm")
    must(cli("del", name), "del c2c")
    wait_topo(name, False)
    must(cli("del", P, net=PEER), "del peer")
    for item in ("bfa2b", "bfnat"):
        must(cli("del", item), "del")
        wait_topo(item, False)


def count_sniff(node, secs, filt="", net=None, iface="eth0"):
    """cloonix cli sniff --count for secs: (pkt/s, Mbit/s, dropped) it saw;
    iface None for an air."""
    where = node + (" " + iface if iface else "")
    show("cloonix cli %s sniff %s --count %s" % (net or NET, where, filt))
    cmd = ti.CLOONIX + ["cli", net or NET, "sniff"] + where.split() + ["--count"]
    cmd += filt.split()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    time.sleep(secs)
    proc.send_signal(2)
    try:
        out, err = proc.communicate(timeout=15)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
    for line in out.splitlines():
        if line.startswith("total "):
            w = line.split()
            # total N packets M MB in Ts: P pkt/s R Mbit/s, D dropped ...
            return float(w[7]), float(w[9]), int(w[11])
    # The reason is on stderr: "refused by the cloonix server: ..." names the
    # server's status_ko, the syslog of the switch has the rest.
    raise Fail("sniff --count on %s printed no total (exit %s):\n%s\n%s"
               % (node, proc.returncode, out[-400:], err.strip()[-400:]))


def t_sniff_ceiling():
    """What the capture chain delivers to a consumer that decodes nothing:
    the ceiling of the server-to-client stream, in packets and in bits."""
    for extra, label in (("-u -b 0 -l 1400", "1400-byte udp flood"),
                         ("-u -b 0 -l 200", "200-byte udp flood")):
        bg = [iperf_bg(A, "10.8.8.2", SECS + 4, extra=extra)]
        time.sleep(1)
        pps_, mb, dropped = count_sniff(B, SECS)
        rx, _, wire = pps(B, 1)
        wait_bg(bg, SECS + 4)
        print("      %-22s wire %.0f kpps %.0f Mbit/s; to the client %.0f kpps "
              "%.0f Mbit/s, %d dropped for it" % (label, rx / 1000, wire,
                                                 pps_ / 1000, mb, dropped))
        if label.startswith("1400"):
            wire1400, best = wire, (None if dropped else (mb, pps_ / 1000))
        if pps_ < 1000:
            raise Fail("the count sniff got %.0f pkt/s under a %s" % (pps_, label))
    # The maximum of the sniff: the highest 1400-byte udp rate whose sniff
    # drops at most MAX_LOSS_OK% of what it delivers.
    lo, hi = 0.0, wire1400
    for _ in range(0 if best else 5):
        mid = (lo + hi) / 2
        bg = [iperf_bg(A, "10.8.8.2", MAX_SECS + 4, extra="-u -b %dM -l 1400"
                       % int(mid))]
        time.sleep(1)
        pps_, mb, dropped = count_sniff(B, MAX_SECS)
        wait_bg(bg, MAX_SECS + 4)
        if dropped <= MAX_LOSS_OK / 100.0 * pps_ * MAX_SECS:
            lo, best = mid, (mb, pps_ / 1000)
        else:
            hi = mid
    if best:
        note_max("snf", "sniffed 1400B udp", best[0],
                 detail="%.0f kpps to the client" % best[1])
    bg = [trafgen_bg(A, SECS + 4)]
    time.sleep(1)
    pps_, mb, dropped = count_sniff(B, SECS)
    rx, _, wire = pps(B, 1)
    wait_bg(bg, SECS + 4)
    print("      %-22s wire %.0f kpps; to the client %.0f kpps, %d dropped for it"
          % ("64-byte frames", rx / 1000, pps_ / 1000, dropped))
    bg = floods(A, "10.8.8.2", SECS + 4)
    time.sleep(1)
    pps_, mb, dropped = count_sniff(B, SECS, filt="icmp")
    wait_bg(bg, SECS + 4)
    print("      %-22s to the client %.0f pkt/s (the filter is applied in the "
          "switch), %d dropped" % ("filter icmp under both", pps_, dropped))
    health("after the ceiling")


SCENARIOS = (
    ("setup", t_setup),
    ("lan tcp", t_lan_tcp),
    ("lan udp", t_lan_udp),
    ("lan pps", t_lan_pps),
    ("sniff ceiling, a consumer that decodes nothing", t_sniff_ceiling),
    ("sniff read under the floods", t_sniff_read),
    ("sniff unread under the floods", t_sniff_unread),
    ("four sniffs under the floods", t_sniffs_x4),
    ("scp under the floods", t_scp),
    ("ssh stdin and stdout under the floods", t_ssh_stream),
    ("c2c tcp, udp, pps", t_c2c),
    ("a2b above its limit", t_a2b),
    ("nat above its limit", t_nat),
    ("kvm flows, sniff on a kvm port, scp and ssh to a kvm", t_kvm),
    ("pipe bounds: unread stdout and stdin, the pipe limit, the KERR of "
     "the guests", t_pipe_bounds),
    ("airs: cnt, kvm, mixed, flooded, sniffed", t_airs),
    ("c2c both ways at once", t_c2c_both_ways),
    ("a2b loss, silentms, rate both ways, frames", t_a2b_more),
    ("nat tcp and udp both ways, from a kvm", t_nat_more),
    ("tap to and from the host", t_tap),
    ("everything at once, %ds" % LONG, t_all_at_once),
    ("memory, del all", t_memory),
)


# TESTBIGFLOWS_FROM=<scenario prefix>: setup and kvm flows, then from that
# scenario on; the all-at-once uses what c2c, a2b and nat leave up.
PRELUDE = ("setup", "kvm flows, sniff on a kvm port, scp and ssh to a kvm")


def chosen():
    start = os.environ.get("TESTBIGFLOWS_FROM")
    if not start:
        return SCENARIOS
    idx = [i for i, (name, _) in enumerate(SCENARIOS)
           if name.startswith(start)]
    if not idx:
        sys.exit("TESTBIGFLOWS_FROM=%s: no scenario starts so, they are:\n  %s"
                 % (start, "\n  ".join(n for n, _ in SCENARIOS)))
    return [(n, f) for i, (n, f) in enumerate(SCENARIOS)
            if i >= idx[0] or n in PRELUDE]


def main():
    print("testbigflows: every flow path above its limit on %s, peer %s, "
          "%ds per flood" % (NET, PEER, SECS))
    for name, fn in chosen():
        t = time.time()
        ti.phase("bigflows %s" % name)
        print("[%6.1fs] %s" % (t - T0, name))
        try:
            fn()
        except Fail as err:
            print()
            print("FAILED: %s" % name)
            print("  %s" % str(err).replace("\n", "\n  "))
            print("  the last commands given:")
            for line in ti.LAST:
                print("    %s" % line)
            print("  the nets are left as they are: cloonix cli %s dump topo"
                  % NET)
            if MAXF:
                print_max()
            sys.exit(1)
        print("      ok in %.1fs" % (time.time() - t))
    print()
    print("EVERY FLOW PATH HELD in %.0fs" % (time.time() - T0))
    print_max()


if __name__ == "__main__":
    main()
