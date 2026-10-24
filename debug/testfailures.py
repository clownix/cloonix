#!/usr/bin/env python3
r"""testfailures - what cloonix does when something goes wrong, on purpose.

    ./testfailures.py [net] [peer]          defaults: nemo mito

Two nets up, trixie and trixie.qcow2 in bulk. Both nets are erased first.
The contract every scenario checks is the same one: a fault is reported
with a reason, nothing hangs, no ghost is left (an object the topology
shows that does not work, a process of an object that no longer exists),
the net stays usable for everything it did not touch, and the same name
can be created again afterwards.

  bad input     ninety refusals, every verb: wrong names, wrong numbers
                (above the cpus, ram and swap of the host too), unknown
                objects, missing images and discs, bad keys and flags. Each
                comes back within 10 s with the exit code of who refused
                (1 the server, 2 the client), words of its fault and, from
                the server, a KERR in the syslog; dump topo must not have
                moved; a kvm with every cpu of the host is then taken. Then
                garbage on the doors port: random bytes, a huge length, a
                frame with a wrong hmac: the server still answers
  killed        kill -9 on the helper of each item in turn: the dumpcap of
                a sniff, the snf, the a2b, the nat and its passt, the c2c
                daemon and its two tap children,
                the qemu of a kvm (its KERR wanted), the init of a cnt.
                After each, within 60 s: the object is either back and
                working or gone from the topology, no process of it is
                left, the rest of the net still pings, and the same name
                can be added again
  aborted       a client killed in the middle of scp up, scp down, an ssh
                stream and a sniff: the fds of doorways and proxymous come
                back to what they were, the next scp is intact
  ports         100 connections held open for 15 s then dropped, random
                bytes, an oversized http request and a bad websocket on the
                web port: the daemons answer, the fds come back
  guests        the agent killed inside a kvm and inside a cnt, a reboot
                from inside a kvm, an interface flapped: ssh comes back
  image owner   with sudo: a copy of the image chowned to root, add cnt works
                (the tree is idmapped from whatever owner it has)
  net kill      a full topology, cloonix stop: no process, no namespace,
                no mount of the net left; the net starts again and works

On a failure the nets are left as they are. Standard library only; the
helpers come from testitall.py beside this file.
"""
import os
import random
import re
import shutil
import signal
import socket
import ssl
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import testitall as ti
from testitall import (Fail, cli, must, ssh, answers, wait_answer, wait_topo,
                       topo, erase, add_cnt, address, ping, Sniff, show,
                       host_port_free, cfg_value, c2c_peered, c2c_evidence,
                       big_file, md5_host)

NET, PEER = ti.NET, ti.PEER
CNT_ROOTFS, KVM_ROOTFS = ti.CNT_ROOTFS, ti.KVM_ROOTFS
BULK = os.environ.get("TESTFAILURES_BULK", "/var/lib/cloonix/bulk")


def meminfo_mb(key):
    for line in open("/proc/meminfo"):
        if line.startswith(key + ":"):
            return int(line.split()[1]) // 1024
    return 0


# the server bounds a kvm by its host, this host: cpus, ram and swap
HOST_CPUS = os.cpu_count() or 1
HOST_RAM = meminfo_mb("MemTotal") + meminfo_mb("SwapTotal")
T0 = time.time()


# ------------------------------------------------------------------ helpers

def host_pid(inner):
    """The host pid of a process the toy knows by its own pid: the toy has
    a pid namespace, and /proc/<pid>/status of every host process lists
    its pids in each namespace it belongs to as NSpid, outermost first: the
    host's, then the toy's, then, for the init of a container of the toy,
    the container's own. The toy's is the second one. A pid of the toy is
    never a pid of the host: killing it as such would hit some process of
    the host, or nothing."""
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        try:
            status = open("/proc/%s/status" % entry).read()
        except OSError:
            continue
        for line in status.splitlines():
            if line.startswith("NSpid:"):
                ns = line.split()[1:]
                if len(ns) > 1 and ns[1] == str(inner):
                    return int(entry)
    return None


def pids_of(name):
    """pids of every process the net files under that object name, as the
    host sees them."""
    out = must(cli("dump", "pid", quiet=True), "dump pid")
    pids = []
    for line in out.splitlines():
        w = line.split()
        if len(w) >= 2 and w[0] == name and w[1].isdigit():
            pid = int(w[1])
            if ti.TOY:
                hp = host_pid(pid)
                if hp is None:
                    raise Fail("%s: the toy's pid %d has no host pid, the "
                               "process is gone or /proc is not readable"
                               % (name, pid))
                pid = hp
            pids.append(pid)
    return pids


def pgrep(pattern):
    r = subprocess.run(["pgrep", "-f", pattern], capture_output=True, text=True)
    return [int(x) for x in r.stdout.split()]


def kill9(pid, what):
    show("kill -9 %d   (%s)" % (pid, what))
    try:
        os.kill(pid, signal.SIGKILL)
        return
    except PermissionError:
        pass
    except ProcessLookupError:
        return
    if not ti.sudo_ok():
        raise Fail("%s runs as root and there is no passwordless sudo to "
                   "kill it" % what)
    r = ti.sudo("kill", "-9", str(pid))
    if r.returncode and "No such process" not in r.stderr:
        raise Fail("sudo kill -9 %d: %s" % (pid, r.stderr.strip()))


def alive(pid):
    return os.path.exists("/proc/%d" % pid)


def c2c_role(victims, what):
    """The process of a c2c that plays a role, told by the process tree, not
    by the pids: the second tap child and the daemon fork at the same time,
    either may get the higher pid. The daemon leads its session (daemon()
    calls setsid), the second tap child is the child of the first."""
    stat = {}
    for p in victims:
        try:
            f = open("/proc/%d/stat" % p).read().rsplit(")", 1)[1].split()
            stat[p] = (int(f[1]), int(f[3]))
        except (OSError, IndexError, ValueError):
            stat[p] = (0, 0)
    roles = {"daemon": [p for p in victims if stat[p][1] == p],
             "second tap child": [p for p in victims if stat[p][0] in victims]}
    roles["first tap child"] = [p for p in victims if p not in
                                roles["daemon"] + roles["second tap child"]]
    if len(roles[what]) != 1:
        raise Fail("the c2c %s is not one process of %s (pid: ppid, session):"
                   " %s" % (what, victims, stat))
    return roles[what][0]


def fds_of(binary):
    """open fds of every process of that binary for this net, summed."""
    total = 0
    for line in subprocess.run(["pgrep", "-af", binary], capture_output=True,
                               text=True).stdout.splitlines():
        if " %s" % NET not in line:
            continue
        pid = line.split()[0]
        try:
            total += len(os.listdir("/proc/%s/fd" % pid))
        except OSError:
            try:
                total += len(ti.sudo("ls", "/proc/%s/fd" % pid).stdout.split())
            except Exception:
                pass
    return total


def fds_table():
    return dict((b, fds_of(b)) for b in ("cloonix-doorways", "cloonix-proxymous",
                                         "cloonix-main-server"))


def fds_back(before, what, slack=4, timeout=30):
    end = time.time() + timeout
    while time.time() < end:
        now = fds_table()
        if all(now[b] <= before[b] + slack for b in before):
            return now
        time.sleep(1)
    now = fds_table()
    raise Fail("%s: fds did not come back: %s" % (what, ", ".join(
        "%s %d->%d" % (b, before[b], now[b]) for b in before
        if now[b] > before[b] + slack)))


def server_answers(what):
    r = cli("dump", "commands", timeout=10, quiet=True)
    if r.returncode:
        raise Fail("%s: the server does not answer dump commands" % what)


def settle(name, probe, timeout=60):
    """After a kill: the object is back and probe() holds, or it is gone.
    Returns "back" or "gone"."""
    end = time.time() + timeout
    state = "?"
    while time.time() < end:
        if not ti.in_topo(name):
            state = "gone"
            time.sleep(2)
            if not ti.in_topo(name):
                return "gone"
        else:
            try:
                if probe():
                    return "back"
            except Fail:
                pass
            state = "still in the topology, not working"
        time.sleep(1)
    raise Fail("%s: %s %ds after its process was killed"
               % (name, state, timeout))


def cmdline(pid):
    try:
        return open("/proc/%d/cmdline" % pid).read().replace("\0", " ")
    except OSError:
        return ""


def carries(cl, word):
    """The word as an argument of its own or as a path element: the passt
    names its net and nat only inside --socket /var/lib/cloonix/NET/../NAT."""
    return re.search(r"[ /]%s(?=[ /]|$)" % re.escape(word), cl) is not None


def no_process_left(name, what):
    time.sleep(2)
    left = [p for p in pgrep(name) if carries(cmdline(p), name)
            and carries(cmdline(p), NET)
            and "testfailures" not in cmdline(p)]
    if left:
        raise Fail("%s: process(es) still carry the name %s: %s"
                   % (what, name, " ".join(str(p) for p in left)))


def sniff_retry(node, num, filt, timeout=40):
    """A sniff that may be refused while its capture is being restarted."""
    end = time.time() + timeout
    last = ""
    while time.time() < end:
        s = Sniff(node, num, filt)
        if s.ready(10):
            return s.start()
        last = s.err
        time.sleep(2)
    raise Fail("no sniff on %s %d within %ds: %s" % (node, num, timeout, last))


# ---------------------------------------------------------------- scenarios

def t_setup():
    for net in (NET, PEER):
        must(cli("dump", "commands", net=net), "dump commands on %s" % net)
        erase(net)
    images = must(cli("dump", "images"), "dump images")
    for name in (CNT_ROOTFS, KVM_ROOTFS):
        if not ti.in_bulk(name, images):
            raise Fail("%s is not in bulk" % name)
    add_cnt("tfA", "s")
    add_cnt("tfB", "s")
    must(cli("plug", "tfA", "eth0", "tflan", ack=5), "plug")
    must(cli("plug", "tfB", "eth0", "tflan", ack=5), "plug")
    wait_answer("tfA")
    wait_answer("tfB")
    address("tfA", 0, "10.11.11.1/24")
    address("tfB", 0, "10.11.11.2/24")
    ping("tfA", "10.11.11.2", 3)


class W:
    """How a refusal comes back: the exit code (1 the server said no, 2 the
    client did, None any but 0 and 3), words of its fault, and a KERR of the
    net in the syslog, wanted when the server refused."""
    def __init__(self, rc, words="", kerr=None):
        self.rc = (rc,) if isinstance(rc, int) else rc
        self.words = words
        self.kerr = (self.rc == (1,)) if kerr is None else kerr


BAD = [
    # names
    ("add", "cnt", "x" * 70, "eth=1", CNT_ROOTFS, W(1, "bad name")),
    ("add", "cnt", "a/b", "eth=1", CNT_ROOTFS, W(1, "bad name")),
    ("add", "cnt", "a b", "eth=1", CNT_ROOTFS, W(1, "bad name")),
    ("add", "cnt", "", "eth=1", CNT_ROOTFS, W(1, "bad name")),
    ("add", "cnt", "tfA", "eth=1", CNT_ROOTFS, W(1, "tfA is a container")),
    ("add", "cnt", "tflan", "eth=1", CNT_ROOTFS, W(1, "tflan is a lan")),
    ("add", "nat", "tfA", W(1, "tfA is a container")),
    ("add", "a2b", "tfA", W(1, "tfA is a container")),
    ("add", "tap", "x" * 20, W(1, "15 chars at most")),   # ifname limit
    ("add", "tap", "tfA", W(1, "tfA is a container")),
    # numbers and words
    # eth= with nothing after it is a guest with no interface, allowed
    ("add", "cnt", "tfbad", "eth=xyz", CNT_ROOTFS, W(2, "eth=xyz: a whole number")),
    ("add", "cnt", "tfbad", "eth=%d" % 40, CNT_ROOTFS, W(1)),
    # ram=0 and cpu=0 are taken as defaults by the server, on purpose
    ("add", "kvm", "tfbad", "ram=99999999", "cpu=1", "eth=1", KVM_ROOTFS,
     W(1, "the ram and swap of this host")),
    ("add", "kvm", "tfbad", "ram=%d" % (HOST_RAM + 1024), "cpu=1", "eth=1",
     KVM_ROOTFS, W(1, "the ram and swap of this host")),
    ("add", "kvm", "tfbad", "ram=abc", "cpu=1", "eth=1", KVM_ROOTFS,
     W(2, "megabytes")),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=999", "eth=1", KVM_ROOTFS,
     W(1, "those of this host")),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=%d" % (HOST_CPUS + 1), "eth=1",
     KVM_ROOTFS, W(1, "those of this host")),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=-1", "eth=1", KVM_ROOTFS,
     W(2, "cpu=-1: a whole number")),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=abc", "eth=1", KVM_ROOTFS,
     W(2, "cpu=abc: a whole number")),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=2.5", "eth=1", KVM_ROOTFS,
     W(2, "cpu=2.5: a whole number")),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=1", "eth=1", "wlan=x", KVM_ROOTFS,
     W(2, "wlan=x: a whole number")),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=1", "eth=1", "wlan=99",
     KVM_ROOTFS, W(1, "radios at most")),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=1", "eth=1", "--no_such_flag",
     KVM_ROOTFS, W(2, "does not take --no_such_flag")),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=1", "eth=1",
     "mac_addr=eth3:02:00:00:00:00:01", KVM_ROOTFS,
     W(2, "names an interface this machine has not got")),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=1", "eth=1",
     "mac_addr=eth0:zz:00:00:00:00:01", KVM_ROOTFS, W(2, "wants hex bytes")),
    ("add", "cnt", "tfbad", "eth=1", "wlan=x", CNT_ROOTFS,
     W(2, "wlan=x: a whole number")),
    ("add", "cnt", "tfbad", "eth=1", "wlan=99", CNT_ROOTFS,
     W(1, "radios asked")),
    ("add", "cnt", "tfbad", "eth=1", "--no_such_flag", CNT_ROOTFS,
     W(2, "does not take --no_such_flag")),
    ("add", "cnt", "tfbad", "eth=1", "vmount=/no/such/dir:/mnt", CNT_ROOTFS,
     W(1, "not a directory the server")),
    ("add", "nat", "tfnat2", "publish=8080", W(2, "hostport:guestport")),
    ("add", "nat", "tfnat2", "publish=0:80", W(2, "0 is not a port")),
    ("add", "nat", "tfnat2", "publish=8080:80/sctp", W(2, "tcp or udp")),
    # images
    ("add", "cnt", "tfbad", "eth=1", "no_such_image", W(1, "is not in|not found")),
    ("add", "cnt", "tfbad", "eth=1", "../../etc", W(1)),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=1", "eth=1", "no_such.qcow2",
     W(1, "is not in|not found")),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=1", "eth=1",
     "install_cdrom=no_such.iso", KVM_ROOTFS, W(1, "not found")),
    ("add", "kvm", "tfbad", "ram=1024", "cpu=1", "eth=1",
     "added_disk=no_such.qcow2", KVM_ROOTFS, W(1, "not found")),
    ("add", "win", "tfbad", "ram=1024", "cpu=1", "eth=1", "no_such.qcow2",
     W(1, "is not in|not found")),
    ("add", "freebsd", "tfbad", "ram=1024", "cpu=1", "eth=1", "no_such.qcow2",
     W(1)),
    ("add", "cisco", "tfbad", "ram=1024", "cpu=1", "eth=1", "no_such.qcow2",
     W(1, "is not in|not found")),
    ("add", "phy", "tfnosuchif0", W(1, "no host interface")),
    # cabling
    ("plug", "nosuch", "tflan", W(1, "nothing has that name")),
    ("plug", "tfA", "eth9", "tflan", W(1, "eth does not exist")),
    ("plug", "tfA", -1, "tflan", W(2, "is not an interface")),
    ("plug", "tfA", "eth0", "x" * 70, W(1, "bad name")),
    ("plug", "tfA", "eth0", "eth1", W(2, "word of an interface")),
    ("plug", "tfA", "eth0", "B", W(2, "word of an interface")),
    ("unplug", "tfA", "eth1", "tflan", W(1, "eth does not exist")),
    ("unplug", "nosuch", "tflan", W(1)),
    ("unplug", "tfA", "eth0", "tfnolan", W(1)),            # not its lan
    ("plug", "tflan", "tfA", W(1, "tfA is a container")),  # a cnt as a lan
    ("plug", "tfA", "wlan0", "cntwifi", W(1, "has no wlan0")),
    ("del", "nosuch", W(1, "NOT FOUND")),
    ("del", "tflan", W(1, "goes away with the unplug")),
    ("del", "cntwifi", W(1, "NOT FOUND|goes away")),
    # cnf
    ("cnf", "nosuch", "A2B", "delay=1", W(1, "no a2b of that name")),
    ("cnf", "tfA", "eth9", "sniff=on", W(1)),
    ("cnf", "nosuch", "sniff=on", W(1)),
    ("cnf", "tfA", "cpu=-1", "ram=0", W(2, "cpu=-1: a whole number")),
    ("cnf", "tfA", "cpu=abc", W(2, "cpu=abc: a whole number")),
    ("cnf", "nosuch", "publish=1:1", W(1, "no nat of that name")),
    ("cnf", "nosuch", "rstp=on", W(1, "no lan of that name")),
    ("cnf", "tflan", "rstp=maybe", W(2, "rstp=on or rstp=off")),
    ("cnf", "tfA", "x=1", W(2, "together")),
    ("cnf", "tfA", "x=a", "y=1", W(2, "whole numbers")),
    ("cnf", "tfA", "eth0", "angle=abc", W(2, "whole numbers")),
    ("cnf", "tfA", "note=" + "n" * 101, W(2, "100 bytes at most")),
    ("cnf", "tfA", "eth0", "cpu=10", W(2, "sniff= and angle= only")),
    ("cnf", "tfA", "sniff=maybe", W(2, "sniff=on or sniff=off")),
    ("cnf", "tfA", "cpu=10", "rstp=on", W(2, "not a setting cloonix knows")),
    ("cnf", "tfA", "bogus=1", W(2, "not a setting cloonix knows")),
    ("cnf", "tfA", "ram=abc", W(2, "megabytes")),
    ("cnf", "nosuch", "A2B", "delay=abc", W(2, "one of delay=")),
    ("cnf", "nosuch", "publish=0:80", W(2, "0 is not a port")),
    ("cnf", "tflan", "mac_mangle=zz", W(1, "mac_mangle=zz")),
    # c2c
    ("add", "c2c", "tfc2c", "no_such_net", W(2, "unknown c2c destination")),
    ("add", "c2c", "tfA", PEER, W(1, "tfA is a container")),
    # the rest (x= y= and angle= take any name: a layout may be filed
    # before its object exists)
    ("save", "nosuch", "/tmp/x.qcow2", W(1, "MACHINE NOT FOUND")),
    ("save", "tfA", "/tmp/x.qcow2", W(1, "save writes the disk of a kvm")),
    ("scp", "/etc/hostname", "nosuch:/tmp/", W(None)),
    ("scp", "nosuch:/etc/hostname", "/tmp/", W(None)),
    ("scp", "/no/such/file", "tfA:/tmp/", W(2, "No such file")),
    ("ssh", "nosuch", "true", W(None)),
    ("dump", "nosuchkind", W(2, "cannot map that")),
    ("dump", "ip", "nosuch", "tfA", W(1, "no nat of that name")),
    ("trace", "nosuchkind", W(2, "no trace called")),
    ("sniff", "tfA", "eth0", "--count", "--bytes", W(2, "pick one of")),
    ("save", "tfA", W(2, "cannot map that")),
    ("scp", "/etc/hostname", "/tmp/x", W(2, "one of the two paths")),
    ("scp", "-r", "/no/such/dir", "tfA:/tmp/", W(2, "is not a directory")),
]


def kerr_since(mark, what, net=NET, timeout=5):
    """A KERR of the net in its syslog since mark: a path that leaves the
    normal one says so."""
    stamp = ("KERR: %s " % net).encode()
    end = time.time() + timeout
    while stamp not in ti.syslog_after(mark):
        if time.time() > end:
            raise Fail("%s: no KERR of %s in the syslog since" % (what, net))
        time.sleep(0.3)


def t_bad_input():
    before = topo()
    for line in BAD:
        cmd, want = [str(x) for x in line[:-1]], line[-1]
        said = " ".join(cmd)
        busy_end = time.time() + 30
        while True:
            mark = ti.syslog_mark() if want.kerr else 0
            t = time.time()
            r = cli(*cmd, timeout=10, ack=40)
            dt = time.time() - t
            out = (r.stderr + r.stdout).strip()
            # a qemu dying at its start keeps the name tfbad while it is
            # deleted: the next tfbad line waits, refused for its own fault
            if not ("tfbad" in cmd and "is a kvm" in out
                    and time.time() < busy_end):
                break
            time.sleep(1)
        if r.returncode == 0:
            raise Fail("accepted, should have been refused: %s\n%s"
                       % (said, r.stdout))
        if r.returncode == 3:
            raise Fail("the client broke instead of refusing: %s\n%s"
                       % (said, out))
        if not out:
            raise Fail("refused with no reason: %s" % said)
        if want.rc and r.returncode not in want.rc:
            raise Fail("refused with exit %d, %s wanted (1 the server said "
                       "no, 2 the client did): %s\n%s"
                       % (r.returncode, " or ".join(str(c) for c in want.rc),
                          said, out))
        if want.words and not re.search(want.words, out):
            raise Fail("refused, but not for its fault (%r wanted): %s\n%s"
                       % (want.words, said, out))
        if dt > 45:
            raise Fail("took %.0fs to refuse: %s" % (dt, said))
        if want.kerr:
            kerr_since(mark, said)
            # one KERR site lets 10 lines through in 10 s: stay under that
            time.sleep(max(0.0, 1.1 - (time.time() - t)))
    time.sleep(2)
    after = topo()
    if after != before:
        raise Fail("dump topo moved under refused requests:\nbefore:\n%s\n"
                   "after:\n%s" % (before, after))
    server_answers("after the refusals")
    # the bound itself is taken: a kvm with every cpu of the host
    must(cli("add", "kvm", "tfmax", "ram=512", "cpu=%d" % HOST_CPUS, KVM_ROOTFS,
             ack=60), "add kvm with the %d cpus of the host" % HOST_CPUS)
    must(cli("del", "tfmax", ack=60), "del tfmax")
    wait_topo("tfmax", False, timeout=60)
    # garbage on the doors port
    ip = cfg_value(NET, "cloonix_ip") or "127.0.0.1"
    port = int(cfg_value(NET, "cloonix_port") or 45211)
    random.seed(7)
    shapes = [os.urandom(64), os.urandom(4096), b"\xff" * 16,
              b"\x00\x00\x00\x00", b"\x7f\xff\xff\xff" + b"A" * 100,
              b"GET / HTTP/1.1\r\n\r\n", b'{"fctident":"dump","tid":0}\n',
              b"\x00" * 65536]
    for i, blob in enumerate(shapes):
        show("python socket -> %s:%d: %d bytes of garbage" % (ip, port, len(blob)))
        try:
            s = socket.create_connection((ip, port), timeout=5)
            s.sendall(blob)
            s.settimeout(3)
            try:
                s.recv(4096)
            except OSError:
                pass
            s.close()
        except OSError as err:
            raise Fail("the doors port refused a connection: %s" % err)
    time.sleep(2)
    server_answers("after garbage on the doors port")


def t_killed():
    # the sniff: its dumpcap, then its snf
    s = Sniff("tfA", 0, "icmp").start()
    dumpcaps = [p for p in pgrep("cloonix-dumpcap") if "tfA_0" in cmdline(p)]
    s.stop()
    if dumpcaps:
        kill9(dumpcaps[0], "the dumpcap of the sniff on tfA")
        time.sleep(3)
        s2 = sniff_retry("tfA", 0, "icmp")
        ping("tfA", "10.11.11.2", 3)
        if s2.stop() < 6:
            raise Fail("the sniff after the killed dumpcap counts nothing")
    snfs = [p for p in pgrep("cloonix-ovs-snf") if " %s" % NET in cmdline(p)]
    if not dumpcaps:
        print("      (no cloonix-dumpcap with tfA_0 in its command line, "
              "not killed)")
    if snfs:
        kill9(snfs[0], "the snf of tfA")
        time.sleep(3)
        s3 = sniff_retry("tfA", 0, "icmp")
        ping("tfA", "10.11.11.2", 3)
        if s3.stop() < 6:
            raise Fail("the sniff after the killed snf counts nothing")
    ping("tfA", "10.11.11.2", 3)

    # the a2b
    must(cli("add", "a2b", "tfa2b"), "add a2b")
    wait_topo("tfa2b", True)
    must(cli("unplug", "tfB", "eth0", "tflan"), "unplug")
    must(cli("plug", "tfa2b", "A", "tflan", ack=5), "plug")
    must(cli("plug", "tfa2b", "B", "tflanB", ack=5), "plug")
    must(cli("plug", "tfB", "eth0", "tflanB", ack=5), "plug")
    time.sleep(1)
    ping("tfA", "10.11.11.2", 3)
    for p in pids_of("tfa2b") or pgrep("cloonix-ovs-a2b .* %s .* tfa2b" % NET):
        kill9(p, "the a2b tfa2b")
    state = settle("tfa2b", lambda: ping("tfA", "10.11.11.2", 2, check=False))
    print("      a2b killed: %s" % state)
    if state == "gone":
        no_process_left("tfa2b", "a2b killed")
    server_answers("after the a2b kill")
    if state == "back":
        must(cli("del", "tfa2b"), "del a2b")
        wait_topo("tfa2b", False)
    must(cli("add", "a2b", "tfa2b"), "add a2b again")
    wait_topo("tfa2b", True)
    must(cli("del", "tfa2b"), "del a2b")
    wait_topo("tfa2b", False)
    must(cli("unplug", "tfB", "eth0", "tflanB"), "unplug")
    must(cli("plug", "tfB", "eth0", "tflan", ack=5), "plug")
    time.sleep(1)
    ping("tfA", "10.11.11.2", 3)

    # the nat and its passt
    port = host_port_free()
    srv = subprocess.Popen([sys.executable, "-m", "http.server", str(port),
                            "--bind", "127.0.0.1"], cwd="/tmp",
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    probe = ("timeout 5 bash -c \"exec 3<>/dev/tcp/10.117.0.1/%d; "
             "printf 'GET / HTTP/1.0\\r\\n\\r\\n' >&3; head -c 8 <&3\"" % port)
    try:
        add_cnt("tfN", "s")
        must(cli("add", "nat", "tfnat"), "add nat")
        wait_topo("tfnat", True)
        must(cli("plug", "tfnat", "tflanN"), "plug")
        must(cli("plug", "tfN", "eth0", "tflanN"), "plug")
        wait_answer("tfN")
        address("tfN", 0, "10.117.0.10/24")
        ssh("tfN", "ip route replace default via 10.117.0.1")
        time.sleep(1)
        if not ssh("tfN", probe, check=False).startswith("HTTP/1.0"):
            raise Fail("the nat does not work before its kill")

        def nat_ok():
            return ssh("tfN", probe, check=False, quiet=True,
                       timeout=30).startswith("HTTP/1.0")
        for what, pat in (("passt", "cloonix-passt"), ("nat", "cloonix-ovs-nat")):
            victims = [p for p in pgrep(pat) if carries(cmdline(p), NET)
                       and carries(cmdline(p), "tfnat")]
            if not victims:
                victims = [p for p in pids_of("tfnat")]
            if not victims:
                print("      (no %s process found for tfnat, not killed)" % what)
                continue
            kill9(victims[0], "the %s of tfnat" % what)
            state = settle("tfnat", nat_ok, timeout=120)
            print("      %s killed: nat is %s" % (what, state))
            if state == "gone":
                no_process_left("tfnat", "%s killed" % what)
            server_answers("after the %s kill" % what)
            if state == "gone":
                must(cli("add", "nat", "tfnat"), "add nat again")
                wait_topo("tfnat", True)
                must(cli("plug", "tfnat", "tflanN"), "plug")
                time.sleep(2)
                if not nat_ok():
                    raise Fail("the nat added again after the %s kill does "
                               "not work" % what)
        must(cli("del", "tfnat"), "del nat")
        wait_topo("tfnat", False)
        must(cli("del", "tfN"), "del")
        wait_topo("tfN", False)
    finally:
        srv.terminate()

    # the c2c
    name = "%s2%s" % (NET, PEER)
    add_cnt("tfP", "s", net=PEER)
    must(cli("add", "c2c", name, PEER), "add c2c")
    end = time.time() + 60
    while time.time() < end and not (ti.in_topo(name) and ti.in_topo(name, PEER)
                                     and c2c_peered(name, NET)):
        time.sleep(1)
    if not c2c_peered(name, NET):
        raise Fail("the c2c is not peered\n" + c2c_evidence(name))
    must(cli("plug", name, "tflan"), "plug c2c")
    must(cli("plug", name, "tflanp", net=PEER), "plug c2c peer")
    must(cli("plug", "tfP", "eth0", "tflanp", net=PEER), "plug peer")
    wait_answer("tfP", PEER)
    address("tfP", 0, "10.11.11.9/24", net=PEER)
    end = time.time() + 30
    while not ping("tfA", "10.11.11.9", 1, check=False) and time.time() < end:
        time.sleep(1)
    ping("tfA", "10.11.11.9", 3)
    # a c2c is three processes, the daemon and two tap children, each killed
    for net, what in ((NET, "daemon"), (PEER, "first tap child"),
                      (NET, "second tap child")):
        victims = [p for p in pgrep("cloonix-ovs-c2c %s " % net)
                   if name in cmdline(p)]
        if len(victims) != 3:
            raise Fail("%d cloonix-ovs-c2c process(es) for %s on %s, 3 wanted"
                       % (len(victims), name, net))
        mark = ti.syslog_mark()
        kill9(c2c_role(victims, what), "the c2c %s on %s" % (what, net))
        # a side lost is seen by the other after 20 s of silence, then the
        # master starts again and peers
        state = settle(name, lambda: c2c_peered(name, NET) and
                       ping("tfA", "10.11.11.9", 2, check=False), timeout=150)
        print("      c2c %s killed on %s: c2c is %s" % (what, net, state))
        if what != "daemon":
            kerr_since(mark, "the c2c %s on %s killed" % (what, net), net=net)
        server_answers("after the c2c kill on %s" % net)
        if state == "gone":
            wait_topo(name, False, net=PEER, timeout=60)
            must(cli("add", "c2c", name, PEER), "add c2c again")
            end = time.time() + 60
            while time.time() < end and not c2c_peered(name, NET):
                time.sleep(1)
            must(cli("plug", name, "tflan"), "plug c2c")
            must(cli("plug", name, "tflanp", net=PEER), "plug peer")
            end = time.time() + 30
            while (not ping("tfA", "10.11.11.9", 1, check=False)
                   and time.time() < end):
                time.sleep(1)
            ping("tfA", "10.11.11.9", 3)
    must(cli("del", name), "del c2c")
    wait_topo(name, False)
    wait_topo(name, False, net=PEER)
    must(cli("del", "tfP", net=PEER), "del peer")

    # the qemu of a kvm
    must(cli("add", "kvm", "tfK", "ram=1024", "cpu=1", "eth=1", KVM_ROOTFS),
         "add kvm")
    must(cli("plug", "tfK", "eth0", "tflan", ack=5), "plug")
    wait_answer("tfK", timeout=180)
    address("tfK", 0, "10.11.11.5/24")
    ping("tfA", "10.11.11.5", 3)
    qemus = [p for p in pids_of("tfK")]
    if not qemus:
        qemus = [p for p in pgrep("cloonix-qemu-system") if " tfK" in
                 cmdline(p)]
    if not qemus:
        raise Fail("no qemu process found for tfK")
    mark = ti.syslog_mark()
    kill9(qemus[0], "the qemu of tfK")
    state = settle("tfK", lambda: answers("tfK", timeout=3), timeout=90)
    kerr_since(mark, "the qemu of tfK killed")
    print("      qemu killed: kvm is %s" % state)
    if state == "gone":
        no_process_left("tfK", "qemu killed")
    server_answers("after the qemu kill")
    ping("tfA", "10.11.11.2", 3)
    if state == "back":
        must(cli("del", "tfK"), "del kvm")
        wait_topo("tfK", False)
    must(cli("add", "kvm", "tfK", "ram=1024", "cpu=1", "eth=1", KVM_ROOTFS),
         "add kvm again")
    wait_topo("tfK", True)
    must(cli("del", "tfK"), "del kvm")
    wait_topo("tfK", False, timeout=60)

    # the init of a cnt
    inits = [p for p in pids_of("tfB")]
    if not inits:
        inits = [p for p in pgrep("cloonixsbininit") if " tfB" in
                 cmdline(p)]
    if inits:
        kill9(inits[0], "the init of tfB")
        state = settle("tfB", lambda: answers("tfB", timeout=3), timeout=90)
        print("      cnt init killed: cnt is %s" % state)
        if state == "gone":
            no_process_left("tfB", "cnt init killed")
        server_answers("after the cnt init kill")
        ssh("tfA", "echo alive", quiet=True)
        if state == "back":
            must(cli("del", "tfB"), "del cnt")
            wait_topo("tfB", False)
    else:
        must(cli("del", "tfB"), "del cnt")
        wait_topo("tfB", False)
        print("      (no pid filed for tfB, its init was not killed)")
    add_cnt("tfB", "s")
    must(cli("plug", "tfB", "eth0", "tflan", ack=5), "plug")
    wait_answer("tfB")
    address("tfB", 0, "10.11.11.2/24")
    ping("tfA", "10.11.11.2", 3)


def t_aborted():
    tmp = tempfile.mkdtemp(prefix="testfailures_")
    try:
        src = os.path.join(tmp, "big.bin")
        big_file(src, 200)
        want = md5_host(src)
        before = fds_table()
        must(cli("scp", src, "tfA:/tmp/full.bin"), "scp up")
        for label, cmd in (
                ("scp up", ["cli", NET, "scp", src, "tfA:/tmp/cut.bin"]),
                ("scp down", ["cli", NET, "scp", "tfA:/tmp/full.bin",
                              os.path.join(tmp, "cut.bin")]),
                ("ssh stream", ["cli", NET, "ssh", "tfA",
                                "cat /dev/zero | head -c 800M"]),
                ("ssh sleep", ["cli", NET, "ssh", "tfA", "sleep 1000"]),
                ("sniff", ["cli", NET, "sniff", "tfA", "eth0", "--bytes"])):
            show("cloonix %s   (killed after 1.5s)" % " ".join(cmd))
            p = subprocess.Popen(ti.CLOONIX + cmd, stdout=subprocess.DEVNULL,
                                 stderr=subprocess.DEVNULL,
                                 stdin=subprocess.DEVNULL)
            time.sleep(1.5)
            p.kill()
            p.wait()
            fds_back(before, "after an aborted %s" % label)
            server_answers("after an aborted %s" % label)
        time.sleep(3)
        # The exact aborted commands ("sleep 1000", "head -c 800M"), not the bare
        # words: guests run their own sleeps.
        left = ssh("tfA",
                   "pgrep -af 'sleep 1000|head -c 800M' | grep -v pgrep; true",
                   check=False).strip()
        if left:
            raise Fail("the aborted ssh left its command running in the "
                       "guest:\n%s" % left)
        back = os.path.join(tmp, "back.bin")
        must(cli("scp", "tfA:/tmp/full.bin", back), "scp down after the aborts")
        if md5_host(back) != want:
            raise Fail("the scp after the aborts is not intact")
        ssh("tfA", "rm -f /tmp/full.bin /tmp/cut.bin", quiet=True)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def t_ports():
    ip = cfg_value(NET, "cloonix_ip") or "127.0.0.1"
    port = int(cfg_value(NET, "cloonix_port") or 45211)
    web = cfg_value(NET, "cloonix_web")
    before = fds_table()
    show("100 connections to %s:%d held open for 15s" % (ip, port))
    socks = []
    for i in range(100):
        try:
            socks.append(socket.create_connection((ip, port), timeout=5))
        except OSError as err:
            raise Fail("connection %d to the doors port refused: %s" % (i, err))
    time.sleep(15)
    server_answers("with 100 connections held open")
    for s in socks:
        s.close()
    fds_back(before, "after 100 held connections", timeout=60)
    if web:
        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        reqs = [b"GET / HTTP/1.1\r\nHost: x\r\n" + b"X-A: " + b"a" * 100000
                + b"\r\n\r\n",
                b"GET /ws HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\n"
                b"Connection: Upgrade\r\nSec-WebSocket-Key: !!!!\r\n\r\n"
                + os.urandom(2000),
                os.urandom(3000), b"\r\n" * 5000]
        for blob in reqs:
            show("tls -> web port %s: %d bytes" % (web, len(blob)))
            try:
                raw = socket.create_connection((ip, int(web)), timeout=5)
                s = ctx.wrap_socket(raw)
                s.sendall(blob)
                s.settimeout(3)
                try:
                    s.recv(4096)
                except OSError:
                    pass
                s.close()
            except OSError:
                pass
        show("100 tls connections to the web port held 10s")
        socks = []
        for i in range(100):
            try:
                socks.append(socket.create_connection((ip, int(web)), timeout=5))
            except OSError as err:
                raise Fail("connection %d to the web port refused: %s" % (i, err))
        time.sleep(10)
        for s in socks:
            s.close()
        code = subprocess.run(["curl", "-sk", "-o", "/dev/null", "-w",
                               "%{http_code}", "-u", NET + ":wrong",
                               "https://%s:%s/" % (ip, web)],
                              capture_output=True, text=True).stdout.strip() \
            if shutil.which("curl") else "401"
        if code != "401":
            raise Fail("the web port after garbage answers %s to a wrong "
                       "password, not 401" % code)
        fds_back(before, "after the web port garbage", timeout=60)
    server_answers("after the ports")


def t_guests():
    must(cli("add", "kvm", "tfK", "ram=1024", "cpu=1", "eth=1", KVM_ROOTFS),
         "add kvm")
    must(cli("plug", "tfK", "eth0", "tflan", ack=5), "plug")
    wait_answer("tfK", timeout=180)
    # the agent killed inside the kvm
    ssh("tfK", "pkill -9 cloonix-agent; pkill -9 cloonix-cxp-srv", check=False)
    t = time.time()
    ti.wait_agent_back("tfK", timeout=180)
    print("      agent killed in the kvm: back after %.0fs" % (time.time() - t))
    # the agent killed inside the cnt
    ssh("tfA", "pkill -9 cloonix-agent; pkill -9 cloonix-cxp-srv", check=False)
    t = time.time()
    ti.wait_agent_back("tfA", timeout=120)
    print("      agent killed in the cnt: back after %.0fs" % (time.time() - t))
    # an interface flapped
    ssh("tfK", "ip addr add 10.11.11.5/24 dev eth0; ip link set eth0 up")
    ping("tfA", "10.11.11.5", 3)
    ssh("tfK", "ip link set eth0 down; sleep 2; ip link set eth0 up")
    time.sleep(2)
    ping("tfA", "10.11.11.5", 3)
    # a reboot from inside
    ssh("tfK", "reboot", check=False, timeout=30)
    t = time.time()
    time.sleep(5)
    wait_answer("tfK", timeout=240)
    print("      reboot from inside the kvm: back after %.0fs" % (time.time() - t))
    ssh("tfK", "ip addr add 10.11.11.5/24 dev eth0; ip link set eth0 up",
        check=False)
    ping("tfA", "10.11.11.5", 3)
    must(cli("del", "tfK"), "del kvm")
    wait_topo("tfK", False, timeout=60)


def t_image_owner():
    if not ti.sudo_ok():
        print("      (no passwordless sudo: the owner of the image is not changed)")
        return
    src = os.path.join(BULK, CNT_ROOTFS)
    if not os.path.isdir(src):
        # the extracted tree of the tar.gz image
        add_cnt("tfO", "s")
        wait_answer("tfO", timeout=240)
        must(cli("del", "tfO"), "del")
        wait_topo("tfO", False)
        src = src + "_targz"
    if not os.path.isdir(src):
        print("      (%s is not a directory here: not changed)" % src)
        return
    # A copy owned by root, what another user's net would find. The image
    # itself is never chowned: a chown clears the setuid bits of its files.
    path = os.path.join(BULK, "tforoot")
    ti.sudo("rm", "-rf", path)
    must(ti.sudo("cp", "-a", src, path), "copy the image")
    try:
        must(ti.sudo("chown", "-R", "root:root", path), "chown the copy to root")
        ti.add_cnt("tfO", "s", rootfs="tforoot")
        wait_answer("tfO")
        must(cli("del", "tfO"), "del")
        wait_topo("tfO", False)
        st = os.stat(path)
        print("      %s chowned to root: add cnt worked (the tree is "
              "idmapped from its owner, uid %d)" % (path, st.st_uid))
    finally:
        ti.sudo("rm", "-rf", path)


def t_net_kill():
    must(cli("add", "kvm", "tfK", "ram=1024", "cpu=1", "eth=1", KVM_ROOTFS),
         "add kvm")
    must(cli("plug", "tfK", "eth0", "tflan", ack=5), "plug")
    must(cli("add", "nat", "tfnat"), "add nat")
    must(cli("add", "a2b", "tfa2b"), "add a2b")
    must(cli("add", "tap", "tftap"), "add tap")
    s = Sniff("tfA", 0, "").start()
    wait_answer("tfK", timeout=180)
    show("cloonix stop %s" % NET)
    subprocess.run(ti.CLOONIX + ["stop", NET], capture_output=True,
                   stdin=subprocess.DEVNULL)
    s.stop()
    end = time.time() + 90
    while time.time() < end:
        # in a toy, proxymous and the container init go with the server
        left = [p for p in pgrep("cloonix-") if " %s" % NET in
                cmdline(p)
                and "testfailures" not in cmdline(p)]
        if not left:
            break
        time.sleep(2)
    if left:
        raise Fail("90s after cloonix stop, processes of the net remain:\n"
                   + "\n".join("  %d %s" % (p, open("/proc/%d/cmdline" % p)
                                            .read().replace("\0", " ")[:120])
                               for p in left))
    ns = [l for l in subprocess.run(["ip", "netns", "list"], capture_output=True,
                                    text=True).stdout.split()
          if l.startswith("cloonix_%s" % NET)]
    if ns:
        raise Fail("network namespaces of the net remain: %s" % " ".join(ns))
    mounts = [l for l in open("/proc/self/mountinfo")
              if "/var/lib/cloonix/%s/" % NET in l]
    if mounts:
        raise Fail("%d mount(s) of the net remain:\n%s"
                   % (len(mounts), "\n".join(l.strip()[:160] for l in mounts[:5])))
    must(ti.net_restart(NET), "net start")
    end = time.time() + 30
    while time.time() < end and cli("dump", "commands", quiet=True).returncode:
        time.sleep(1)
    server_answers("after the restart")
    add_cnt("tfR", "s")
    wait_answer("tfR")
    must(cli("del", "tfR"), "del")
    wait_topo("tfR", False)
    erase(PEER)


SCENARIOS = (
    ("setup", t_setup),
    ("bad input", t_bad_input),
    ("killed helpers", t_killed),
    ("aborted clients", t_aborted),
    ("garbage and floods on the ports", t_ports),
    ("guests that misbehave", t_guests),
    ("image owner", t_image_owner),
    ("net kill and restart", t_net_kill),
)


def main():
    print("testfailures: things going wrong on %s, peer %s" % (NET, PEER))
    for name, fn in SCENARIOS:
        t = time.time()
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
            sys.exit(1)
        print("      ok in %.1fs" % (time.time() - t))
    print()
    print("EVERY FAULT WAS HANDLED in %.0fs" % (time.time() - T0))


if __name__ == "__main__":
    main()
