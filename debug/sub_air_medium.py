#!/usr/bin/env python3
"""sub_air_medium - the medium of the airs survives a netlink overflow.
Two cnt radios of flow_trixie on one air, one ad hoc cell; iperf3 udp -b 0
floods it until cloonix-suid-power-air reports frames the kernel dropped
for it, not read in time. Then: no "medium socket closed" in the syslog,
the ping of the cell still captured on the air. A medium lost leaves the radios to the kernel:
the ping goes, the capture of the air sees none of it. The module is the
host's: loaded here with sudo when absent, skipped without it.

    ./sub_air_medium.py [net] [peer]          defaults: nemo mito
"""
import os
import re
import subprocess
import time
from sublib import *
from sub_airchurn import module_loaded
from sub_kvmwifi import icmp_on_air

A, B = "tiamW1", "tiamW2"
AIR = "tiamair"
ADDR = {A: "10.47.8.1", B: "10.47.8.2"}
FLOW = os.environ.get("TESTBIGFLOWS_IMAGE", "flow_trixie")
FLOODS, FLOOD_SECS = 5, 10
# the drops said on the beat of the medium (6 s); on older builds the
# ENOBUFS of the socket
OVERFLOW = (r"suid-power-air.*(dropped, not read in time by the medium|"
            r"ENOBUFS, the kernel dropped|EPOLLERR \(No buffer)")
LOST = r"medium socket closed|process of the medium ended"


def syslog_find(mark, pattern):
    rx = re.compile(pattern)
    for raw in ti.syslog_after(mark).splitlines():
        line = raw.decode("utf-8", "replace")
        if rx.search(line):
            return line[line.find("KERR"):].strip()
    return None


def cell(node):
    ssh(node, "iw dev wlan0 set type ibss")
    ssh(node, "ip link set dev wlan0 up")
    ssh(node, "iw dev wlan0 ibss join tiamcell 2412")
    ssh(node, "ip addr add %s/24 dev wlan0" % ADDR[node])


def pings(src, dst, secs):
    end = time.time() + secs
    while " 0% packet loss" not in ssh(src, "ping -c 3 -i 0.3 -W 2 %s"
                                       % ADDR[dst], check=False):
        if time.time() > end:
            raise Fail("%s does not ping %s over %s in %d s"
                       % (src, dst, AIR, secs))
        time.sleep(2)


def host(cmd):
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    return (r.stdout + r.stderr).strip()


def evidence(mark):
    """What the medium, the kernel and the two radios say once the air is
    silent: one run gives every lead."""
    out = []
    kerr = [raw.decode("utf-8", "replace").strip() for raw in
            ti.syslog_after(mark).splitlines()
            if b"KERR" in raw and b"suid-power" in raw]
    out.append("syslog of suid-power since the start (%d lines, last 30):"
               % len(kerr))
    out += ["  " + l[l.find("KERR"):] for l in kerr[-30:]]
    pids = host("pgrep -f cloonix-suid-power-air").split()
    out.append("the medium process: %s" % (" ".join(pids) or "NONE"))
    for pid in pids:
        out.append("  " + host("ps -o pid,stat,pcpu,wchan:24,etime -p %s "
                               "| tail -n 1" % pid))
    # the medium socket is made in cloonix_<net>: its Rmem (queue left) and
    # Drops are seen there only
    out.append("netlink of cloonix_%s, generic (Eth 16):" % NET)
    out += ["  " + l for l in host("sudo -n ip netns exec cloonix_%s cat "
                                   "/proc/net/netlink" % NET).splitlines()
            if l.split()[1:2] == ["16"] or not l.startswith("0")]
    out.append("dmesg, last lines of hwsim:")
    out += ["  " + l for l in host("sudo -n dmesg | grep -i hwsim | "
                                   "tail -n 5").splitlines()]
    for n in (A, B):
        for cmd in ("iw dev wlan0 link", "iw dev wlan0 station dump | "
                    "grep -E 'Station|packets|inactive'", "ip neigh",
                    "ip -s link show wlan0"):
            out.append("%s: %s" % (n, cmd))
            out += ["  " + l for l in ssh(n, cmd, check=False,
                                          quiet=True).strip().splitlines()]
    try:
        nb = icmp_on_air(AIR, 4, lambda: ssh(A, "ping -c 5 -W 1 %s"
                                             % ADDR[B], check=False,
                                             quiet=True))
    except Fail as err:
        nb = str(err)
    out.append("ICMP frames on the air during a ping of 5: %s" % nb)
    time.sleep(60)
    late = " 0% packet loss" in ssh(A, "ping -c 3 -W 2 %s" % ADDR[B],
                                    check=False, quiet=True)
    out.append("a ping 60 s later: %s" % ("goes" if late else "fails"))
    return "\n".join(out)


def sub_air_medium():
    if not module_loaded():
        return
    if not in_bulk(FLOW, images()):
        raise Fail("%s is not in bulk: vm_create/cnt_kvm_demo/"
                   "cnt_flow_trixie.sh builds it" % FLOW)
    clean(A, B)
    mark = ti.syslog_mark()
    ti.syslog_after(mark)
    try:
        for n in (A, B):
            must(cli("add", "cnt", n, "eth=1", FLOW, "wlan=1"),
                 "add cnt %s with a radio" % n)
        for n in (A, B):
            wait_answer(n)
            must(cli("plug", n, "wlan0", AIR), "plug %s wlan0 %s" % (n, AIR))
            cell(n)
        try:
            pings(A, B, 40)
        except Fail as err:
            raise Fail("%s, before any flood\n%s" % (err, evidence(mark)))
        ti.iperf_server(B)
        overflow = None
        for i in range(FLOODS):
            # the flood of testbigflows; killed if its end never comes, the
            # verdict is the syslog, the ping and the capture after it
            ssh(A, "timeout -s KILL %d iperf3 -c %s -p %d -t %d -u -b 0 -l "
                   "1400 2>&1 | tail -n 4" % (FLOOD_SECS + 20, ADDR[B],
                                             ti.IPERF_PORT, FLOOD_SECS),
                check=False, timeout=FLOOD_SECS + 60)
            end = time.time() + 10
            overflow = syslog_find(mark, OVERFLOW)
            while not overflow and time.time() < end:
                time.sleep(1)
                overflow = syslog_find(mark, OVERFLOW)
            if overflow:
                break
        if not overflow:
            raise Fail("no netlink overflow of the medium in %d floods of "
                       "%d s: the fix is not verified" % (FLOODS, FLOOD_SECS))
        print("      overflow provoked at flood %d: %s" % (i + 1, overflow))
        lost = syslog_find(mark, LOST)
        if lost:
            raise Fail("the medium of the airs was lost on the overflow\n  %s"
                       % lost)
        try:
            pings(A, B, 20)
        except Fail as err:
            raise Fail("%s\n%s" % (err, evidence(mark)))
        nb = icmp_on_air(AIR, 4, lambda: ssh(A, "ping -c 5 %s" % ADDR[B],
                                             check=False))
        print("      after the overflow, %d ICMP frames on the air %s"
              % (nb, AIR))
        if nb < 8:
            raise Fail("%d ICMP frames captured on %s, 10 sent: the radios "
                       "no longer go through cloonix" % (nb, AIR))
        lost = syslog_find(mark, LOST)
        if lost:
            raise Fail("the medium of the airs was lost\n  %s" % lost)
    finally:
        clean(A, B)


if __name__ == "__main__":
    run("air_medium", sub_air_medium)
