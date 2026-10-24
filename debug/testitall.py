#!/usr/bin/env python3
r"""testitall - every item of cloonix, one short check each, stop at the first
that does not hold.

    ./testitall.py [net] [peer]          defaults: nemo mito

Run by top_testitall.sh between the start of the two nets and the doc checks;
runs alone too, on two nets already up. Both nets are erased first.

The checks, in the order they run. A sniff is a "cloonix cli sniff" with a
filter, counted while the traffic runs, so every item is checked twice: the
guest says it worked, and the switch shows the packets.

  net        both nets answer cloonix list
  kvm boot   two kvm are created first, not cabled, and boot while the
             container checks run
  cnt+lan    two cnt on one lan, a ping; sniff icmp counts it, sniff arp
             counts the resolution, a filter that matches nothing counts 0
  eth kinds  a sniff on a not sniffed eth arms it and works; sniff=off
             takes it out of dump commands, sniff=on puts it back
  unplug     unplug: the ping fails, the lan is gone from dump topo;
             plug again: the ping is back; a plug onto its own lan is done
             already, onto another lan it moves; an unplug finds the lan
  del    the container is gone from the topology and from ssh
  scp        a file there and back, a directory with scp -r
  vmount     vmount= shows a host file in the guest, startup_env= reaches
             the environment of pid 1
  cnf caps   cpu=25 ram=100 is read back from the cgroup, 0 lifts it
  rstp       cnf <lan> rstp=on|off is read back with dump ovs-vsctl
  dump       config topo json drawing commands pid images namespaces
             ovs-vsctl all answer, json is json, commands holds the adds
  position   cnf x= y= and cnf eth0 angle= are acknowledged
  tap        the host end exists; with sudo, 172.26.198.1/24 on it
             (TESTITALL_TAP_NET changes the /24, the only address the host
             carries) and a ping from the guest, sniffed on the guest
  nat cnt    static 10.117.0.10, a server on the loopback of the host
             reached at 10.117.0.1, sniffed on the guest
  a2b        a clean cable, delay=100 raises the rtt, loss=5000 halves the
             ping, the two sniffs show fewer packets past the cable than
             before it, 0 restores
  c2c        a c2c to the peer net, tcp and udp both peered, a lan on each
             side, a ping across, arp and icmp sniffed on the far side,
             the c2c gone from both nets after del
  flow lan   iperf3 between two perf_trixie containers on one lan
  flow sniff the same run under an unfiltered sniff that is read (packets
             counted, throughput kept), then under one whose client never
             reads its stdout: the lan keeps its rate, the server still
             answers, and a fresh sniff works after the stalled one is killed
  flow scp   48 MB through the agent, md5 both ways, alone and then while
             an iperf3 and a sniff of it run on the same doors connection
  flow a2b   rate=1000 kB/s on A2B: iperf3 reads 8 Mbit/s that way and
             stays fast the other way; 0 restores
  flow c2c   iperf3 tcp and udp 20 Mbit/s to a perf_trixie on the peer net
  flow nat   with iperf3 on the host: cnt to 10.117.0.1 through the nat,
             host to the cnt through publish=
  kvm        the two kvm answer; kvm-kvm ping and kvm-cnt ping, sniff on a
             kvm interface, a not sniffed one armed by the sniff, scp to a kvm
  nat kvm    dhclient gets 10.117.0.10 (dhcp sniffed), publish= reaches a
             server in the kvm from the host, publish=none stops it, the
             internet when the host has it
  flow kvm   32 MB by scp over the virtio serial, md5 both ways
  save       save writes a qcow2
  web        https refuses a wrong password, accepts the right one
  del all    both nets empty

Not covered here: phy (takes a real interface of the host: set
TESTITALL_PHY=<iface> to add and delete a macvlan on it), the windows,
freebsd and cisco guests (minutes to boot, their own demos), files and
webshark (a screen and a browser). mac_mangle of a c2c is sub_c2c_mac_mangle.

The flows need the image perf_trixie (vm_create/cnt_kvm_demo/
cnt_perf_trixie.sh: iperf3, socat, curl, python3); TESTITALL_PERF names
another. On a failure the nets are left as they are: the state is the evidence.
Standard library only.
"""
import json
import glob
import os
import shutil
import socket
import subprocess
import syslog
import sys
import tempfile
import threading
import time

CLOONIX = ([sys.executable, os.environ["CLOONIX_CLIENT"]]
           if os.environ.get("CLOONIX_CLIENT") else ["cloonix"])
# TESTITALL_TOY: directory holding one toy per net, restarted with <dir>/<net>/<net>cmd.
# A toy has its own netns: a tap of the net is not a host interface.
TOY = os.environ.get("TESTITALL_TOY", "")
BULK = os.environ.get("TESTITALL_BULK", "/var/lib/cloonix/bulk")
NET = sys.argv[1] if len(sys.argv) > 1 else "nemo"
PEER = sys.argv[2] if len(sys.argv) > 2 else "mito"
CNT_ROOTFS = os.environ.get("TESTITALL_CNT", "trixie")
PERF_ROOTFS = os.environ.get("TESTITALL_PERF", "perf_trixie")
KVM_ROOTFS = os.environ.get("TESTITALL_KVM", "trixie.qcow2")
PHY = os.environ.get("TESTITALL_PHY", "")
# The one address the host itself carries, on the tap: every other address
# of this file lives inside a cloonix namespace and reaches no host route.
TAP_NET = os.environ.get("TESTITALL_TAP_NET", "172.26.198")

sys.stdout.reconfigure(line_buffering=True)

T0 = time.time()
LAST = []          # the last commands given, printed on a failure


class Fail(Exception):
    pass


def show(line):
    LAST.append(line)
    del LAST[:-12]
    print(line)


def cli(*args, net=None, ack=None, quiet=False, timeout=None):
    net = net or NET
    a = [str(x) for x in args]
    cmd = list(CLOONIX)
    wait = ack if ack is not None else timeout
    if wait is not None:
        cmd += ["--wait", str(wait)]
    cmd += ["cli", net] + a
    if not quiet:
        show("cloonix cli %s %s"
             % (net, " ".join('"%s"' % x if " " in x else x for x in a)))
    return subprocess.run(cmd, capture_output=True, text=True,
                          stdin=subprocess.DEVNULL)


def must(r, what):
    if r.returncode:
        raise Fail("%s: %s" % (what, (r.stderr + r.stdout).strip()
                               or "no output"))
    return r.stdout


def ssh(node, cmd, net=None, check=True, quiet=False, timeout=300):
    net = net or NET
    if not quiet:
        show('cloonix cli %s ssh %s "%s"' % (net, node, cmd))
    try:
        r = subprocess.run(CLOONIX + ["cli", net, "ssh", node, cmd],
                           capture_output=True, text=True, timeout=timeout,
                           stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired as err:
        # A Fail, not a traceback: the run reports it with its context and
        # what was gathered so far.
        got = (err.stdout or b"")[-400:]
        raise Fail("%s: %s\n  -> no answer in %ds%s%s"
                   % (node, cmd, timeout, "\n  " if got else "",
                      got.decode("utf-8", "replace") if got else ""))
    if check and r.returncode:
        raise Fail("%s: %s\n  -> %s" % (node, cmd,
                                        (r.stderr + r.stdout).strip()))
    return r.stdout


def answers(node, net=None, timeout=3):
    r = subprocess.run(CLOONIX + ["--wait", str(timeout), "cli", net or NET,
                                  "ssh", node, "echo yes"],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    return "yes" in r.stdout


def ping_ok(node, net=None):
    """The agent's liveness as the server holds it: ftopo's ping_ok of the
    cnt or kvm, what makes it blue on the canvas."""
    for kind in ("cnt", "kvm"):
        for obj in ftopo(net).get(kind, []) or []:
            if obj.get("name") == node:
                return bool(obj.get("ping_ok"))
    return False


def wait_agent_back(node, net=None, timeout=120):
    """After the agent of a guest died: back means the server's ping to it
    is answered again AND an ssh goes through. The exec server alone can
    answer an ssh while the agent is still dead, and a kvm's agent only
    comes back when the server's watchdog relaunches it, 30 s or more later;
    a command sent between the two lands on that relaunch and is cut."""
    end = time.time() + timeout
    while time.time() < end:
        if ping_ok(node, net) and answers(node, net):
            return
        time.sleep(1)
    raise Fail("the agent of %s did not come back in %ds" % (node, timeout))


def wait_answer(node, net=None, timeout=90):
    end = time.time() + timeout
    while time.time() < end:
        if answers(node, net):
            return
        time.sleep(1)
    raise Fail("%s never answered in %ds" % (node, timeout))


def topo(net=None):
    return must(cli("dump", "topo", net=net, quiet=True), "dump topo")


def ftopo(net=None):
    return json.loads(must(cli("dump", "json", net=net, quiet=True),
                           "dump json"))


# Object arrays of ftopo, each [{"name":..}]. Presence is asked here, never
# grepped in "dump topo" text, where a stale lan endpoint still names the object.
_TOPO_KINDS = ("cnt", "kvm", "c2c", "tap", "nat", "a2b", "phy")


def in_topo(name, net=None):
    t = ftopo(net)
    for kind in _TOPO_KINDS:
        for obj in t.get(kind, []) or []:
            if obj.get("name") == name:
                return True
    return False


def wait_topo(name, present, net=None, timeout=30):
    end = time.time() + timeout
    while time.time() < end:
        if in_topo(name, net) == present:
            return
        time.sleep(1)
    raise Fail("%s is %s the topology of %s after %ds"
               % (name, "still in" if present is False else "not in",
                  net or NET, timeout))


def erase(net):
    if "(no objects)" in topo(net):
        return
    cli("del", "all", net=net)
    end = time.time() + 60
    while time.time() < end:
        if "(no objects)" in topo(net):
            return
        time.sleep(1)
    raise Fail("del all left objects on %s" % net)


def snf_on(name, eth, net=None):
    """eth=<n> gives eths not sniffable: sniff=on for each s of eth."""
    for i, ch in enumerate(eth):
        if ch == "s":
            must(cli("cnf", name, "eth%d" % i, "sniff=on", net=net),
                 "cnf %s eth%d sniff=on" % (name, i))


def add_cnt(name, eth="s", extra=(), net=None, rootfs=None):
    """eth: one letter an eth, s sniffed, v not."""
    last = ""
    for _ in range(6):
        r = cli("add", "cnt", name, "eth=%d" % len(eth), rootfs or CNT_ROOTFS,
                *extra, net=net)
        if r.returncode == 0:
            snf_on(name, eth, net)
            return
        last = (r.stdout + r.stderr).strip()
        time.sleep(2)
    raise Fail("add cnt %s refused: %s" % (name, last))


def address(node, num, addr, net=None):
    ssh(node, "ip addr add %s dev eth%d" % (addr, num), net=net)
    ssh(node, "ip link set dev eth%d up" % num, net=net)


def ping(node, target, count=3, net=None, check=True):
    out = ssh(node, "ping -c %d -i 0.3 -W 2 %s" % (count, target), net=net,
              check=False)
    ok = "0% packet loss" in out and " 0 received" not in out
    if check and not ok:
        raise Fail("%s does not ping %s:\n%s" % (node, target, out))
    return ok


def ping_stats(node, target, count, net=None):
    """(loss percent, rtt avg ms or None)."""
    out = ssh(node, "ping -c %d -i 0.05 -W 2 %s" % (count, target), net=net,
              check=False)
    loss, avg = None, None
    for line in out.splitlines():
        if "packet loss" in line:
            loss = int(round(float(line.split("%")[0].split()[-1])))
        if "rtt min/avg/max" in line:
            avg = float(line.split("=")[1].split("/")[1])
    if loss is None:
        raise Fail("ping %s -> %s printed no statistics:\n%s"
                   % (node, target, out))
    return loss, avg


class Sniff:
    """One cloonix cli sniff, counting the packets that match its filter."""

    def __init__(self, node, num, filt, net=None):
        self.net = net or NET
        self.node, self.num, self.filt = node, num, filt
        self.live = False
        self.err = ""
        self.packets = []
        self.count = 0
        self.gaps = 0          # packets the switch numbered and the client never got
        self.last_nb = None
        self.evt = threading.Event()
        show("cloonix cli %s sniff %s eth%d --bytes %s"
             % (self.net, node, num, filt))
        cmd = CLOONIX + ["cli", self.net, "sniff", node, "eth%d" % num, "--bytes"]
        cmd += filt.split()
        self.proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, text=True,
                                     bufsize=1)
        threading.Thread(target=self._drain, daemon=True).start()

    def _drain(self):
        for line in self.proc.stdout:
            if line.startswith("# waiting"):
                self.live = True
                self.evt.set()
            elif line.startswith("pkt "):
                self.count += 1
                f = line.split()
                try:
                    nb = int(f[1])
                    if self.last_nb is not None and nb > self.last_nb + 1:
                        self.gaps += nb - self.last_nb - 1
                    self.last_nb = nb
                except (ValueError, IndexError):
                    pass
                # the bytes of the first ones only: a flow is counted, not kept
                if len(self.packets) < 200:
                    try:
                        self.packets.append(bytes(int(x, 16) for x in f[4:]))
                    except ValueError:
                        pass
        self.evt.set()

    def ready(self, timeout=15):
        self.evt.wait(timeout)
        if not self.live:
            self.err = (self.proc.stderr.read() or "").strip() or "no answer"
        return self.live

    def start(self):
        if not self.ready():
            raise Fail("sniff %s eth%d '%s' on %s: %s"
                       % (self.node, self.num, self.filt, self.net, self.err))
        time.sleep(1)
        del self.packets[:]
        self.count = 0
        self.gaps = 0
        self.last_nb = None
        return self

    def stop(self):
        time.sleep(1.5)
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
        return self.count

    def refused(self, timeout=8):
        """True when the server said no; a live subscription is not that."""
        self.evt.wait(timeout)
        if self.live:
            self.stop()
            return False
        self.err = (self.proc.stderr.read() or "").strip()
        return "refused" in self.err


def expect_count(sniff, low, high=None, what=""):
    n = sniff.stop()
    if n < low or (high is not None and n > high):
        raise Fail("sniff %s eth%d '%s' counted %d packets, wanted %s%s"
                   % (sniff.node, sniff.num, sniff.filt, n,
                      "%d..%d" % (low, high) if high is not None
                      else ">= %d" % low, " (%s)" % what if what else ""))
    return n


# The syslog of the net: a toy keeps its own, TESTITALL_SYSLOG names it.
SYSLOG = os.environ.get("TESTITALL_SYSLOG", "/var/log/syslog")


def syslog_mark():
    try:
        return os.path.getsize(SYSLOG)
    except OSError as err:
        raise Fail("the syslog of the net %s cannot be read: %s (set "
                   "TESTITALL_SYSLOG)" % (SYSLOG, err))


def syslog_after(mark):
    """The syslog from mark on, all of it when it was rotated since, through
    sudo -n when this user cannot read it."""
    if os.path.getsize(SYSLOG) < mark:
        mark = 0
    try:
        with open(SYSLOG, "rb") as f:
            f.seek(mark)
            return f.read()
    except PermissionError:
        r = subprocess.run(["sudo", "-n", "tail", "-c", "+%d" % (mark + 1),
                            SYSLOG], capture_output=True)
        if r.returncode:
            raise Fail("the syslog %s cannot be read, not even with sudo -n: "
                       "%s" % (SYSLOG, r.stderr.decode().strip()))
        return r.stdout


def sudo_ok():
    """Passwordless sudo here; a host without sudo has none, not an error."""
    return bool(shutil.which("sudo")) and subprocess.run(
        ["sudo", "-n", "true"], capture_output=True).returncode == 0


def sudo_path(cmd):
    """The command by its full path: sudo then needs nothing of its own
    secure_path, which has no sbin on some distros (tumbleweed)."""
    if "/" in cmd:
        return cmd
    dirs = os.environ.get("PATH", "").split(":")
    for d in dirs + ["/usr/sbin", "/sbin", "/usr/local/sbin"]:
        full = os.path.join(d, cmd)
        if d and os.access(full, os.X_OK) and not os.path.isdir(full):
            return full
    return cmd


def sudo(*args):
    show("sudo " + " ".join(args))
    return subprocess.run(["sudo", "-n", sudo_path(args[0])] + list(args[1:]),
                          capture_output=True, text=True)


def host_port_free():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def host_connect(port, timeout=3):
    try:
        s = socket.create_connection(("127.0.0.1", port), timeout=timeout)
        s.close()
        return True
    except OSError:
        return False


def shared_file(name):
    """(host path, the same file as the server names it) for a file the
    test and the server both touch: a save target, a vmount source. On a
    bundle the two are one path in /tmp; a toy sees nothing of the host but
    its bulk, as /root/bulk_host, so the file goes there."""
    if TOY:
        return os.path.join(BULK, name), "/root/bulk_host/" + name
    return "/tmp/" + name, "/tmp/" + name


def net_restart(net):
    """The net started again after a cloonix stop: cloonix start for a
    bundle, the toy's own cmd for a toy, which clears what the stop left.
    Returns the CompletedProcess of the start."""
    if TOY:
        cmd = os.path.join(TOY, net, net + "cmd")
        show("%s start" % cmd)
        return subprocess.run([cmd, "start"], capture_output=True, text=True)
    show("cloonix start %s" % net)
    return subprocess.run(CLOONIX + ["start", net], capture_output=True,
                          text=True)


def cfg_value(net, key):
    for path in (os.environ.get("TESTITALL_CFG", ""),
                 "/usr/libexec/cloonix/cloonfs/etc/cloonix.cfg",
                 "/cloonix/cloonfs/etc/cloonix.cfg"):
        if not path or not os.path.exists(path):
            continue
        inside = False
        for line in open(path):
            w = line.split()
            if w[:1] == ["CLOONIX_NET:"]:
                inside = w[1:2] == [net]
            elif inside and w[:1] == [key]:
                return w[1]
    return None


def in_bulk(name, images):
    """True when dump images lists the image under any form add takes:
    the directory, the delivered tar.gz, or the tree extracted from it."""
    words = images.split() if isinstance(images, str) else images
    if name.endswith(".qcow2"):
        return name in words
    return (name in words or name + ".tar.gz" in words
            or name + "_targz" in words)


# ---------------------------------------------------------------- the checks

def t_net():
    for net in (NET, PEER):
        must(cli("dump", "commands", net=net), "dump commands on %s" % net)
    show("cloonix list")
    r = subprocess.run(CLOONIX + ["list"], capture_output=True, text=True)
    must(r, "cloonix list")
    for net in (NET, PEER):
        erase(net)
    images = must(cli("dump", "images"), "dump images")
    for name in (CNT_ROOTFS, KVM_ROOTFS, PERF_ROOTFS):
        if not in_bulk(name, images):
            raise Fail("%s is not in bulk (dump images); build it first: "
                       "vm_create/cnt_kvm for trixie, "
                       "vm_create/cnt_kvm_demo/cnt_perf_trixie.sh for "
                       "perf_trixie" % name)


def t_kvm_boot():
    for name in ("tikvm1", "tikvm2"):
        must(cli("add", "kvm", name, "ram=1024", "cpu=2", "eth=2",
                 KVM_ROOTFS), "add kvm %s" % name)
        snf_on(name, "sv")


def t_cnt_lan():
    add_cnt("ticnt1", "sv")
    add_cnt("ticnt2", "s")
    must(cli("plug", "ticnt1", "eth0", "tilan1", ack=5), "plug")
    must(cli("plug", "ticnt2", "eth0", "tilan1", ack=5), "plug")
    wait_answer("ticnt1")
    wait_answer("ticnt2")
    address("ticnt1", 0, "10.1.1.1/24")
    address("ticnt2", 0, "10.1.1.2/24")
    s_arp = Sniff("ticnt1", 0, "arp").start()
    s_icmp = Sniff("ticnt1", 0, "icmp").start()
    s_none = Sniff("ticnt1", 0, "udp port 9999").start()
    ping("ticnt1", "10.1.1.2", 4)
    expect_count(s_arp, 2, what="request and reply")
    expect_count(s_icmp, 8, what="4 requests and 4 replies")
    expect_count(s_none, 0, 0, what="a filter matching nothing")
    lines = topo()
    if "tilan1" not in lines or "ticnt1 eth0 (eth-s)" not in lines:
        raise Fail("dump topo does not show tilan1 with ticnt1 eth0:\n%s" % lines)


def t_eth_kinds():
    line = "cnf ticnt1 eth1 sniff=on"
    s = Sniff("ticnt1", 1, "")
    if not s.ready():
        raise Fail("a sniff on a not sniffed eth did not arm it: %s" % s.err)
    s.stop()
    if line not in must(cli("dump", "commands"), "dump commands"):
        raise Fail("dump commands lacks \"%s\" after the sniff" % line)
    must(cli("cnf", "ticnt1", "eth1", "sniff=off"), "cnf ticnt1 eth1 sniff=off")
    if line in must(cli("dump", "commands"), "dump commands"):
        raise Fail("dump commands still has \"%s\" after sniff=off" % line)
    must(cli("cnf", "ticnt1", "eth1", "sniff=on"), line)
    s = Sniff("ticnt1", 1, "")
    if not s.ready():
        raise Fail("sniff after %s: %s" % (line, s.err))
    s.stop()


def t_del_lan():
    must(cli("unplug", "ticnt2", "eth0", "tilan1"), "unplug")
    time.sleep(1)
    if ping("ticnt1", "10.1.1.2", 2, check=False):
        raise Fail("ticnt2 unplugged from tilan1 still answers the ping")
    if "ticnt2 eth0" in topo():
        raise Fail("dump topo still lists ticnt2 eth0 on a lan")
    must(cli("plug", "ticnt2", "eth0", "tilan1"), "plug")
    time.sleep(1)
    ping("ticnt1", "10.1.1.2", 3)
    # onto the lan it is on: done already
    r = cli("plug", "ticnt2", "eth0", "tilan1")
    if r.returncode or "already" not in r.stderr:
        raise Fail("a plug onto the lan it is on: rc %d %s"
                   % (r.returncode, (r.stderr + r.stdout).strip()))
    # onto another lan: moved, the ping gone
    must(cli("plug", "ticnt2", "eth0", "tilanM"), "plug elsewhere")
    time.sleep(1)
    if ping("ticnt1", "10.1.1.2", 2, check=False):
        raise Fail("ticnt2 moved to tilanM still answers on tilan1")
    must(cli("plug", "ticnt2", "eth0", "tilan1"), "plug back")
    time.sleep(1)
    ping("ticnt1", "10.1.1.2", 3)
    # the lan found by the unplug
    must(cli("unplug", "ticnt1", "eth0"), "unplug")
    must(cli("unplug", "ticnt2", "eth0", "tilan1"), "unplug")
    time.sleep(1)
    if "tilan1" in topo():
        raise Fail("tilan1 is still there with no cable")


def t_del_sat():
    must(cli("del", "ticnt2"), "del")
    wait_topo("ticnt2", False)
    if answers("ticnt2", timeout=2):
        raise Fail("ticnt2 deleted and still answering ssh")


def t_scp():
    tmp = tempfile.mkdtemp(prefix="testitall_")
    try:
        src = os.path.join(tmp, "put.txt")
        open(src, "w").write("testitall %d\n" % os.getpid())
        must(cli("scp", src, "ticnt1:/tmp/"), "scp up")
        back = os.path.join(tmp, "back.txt")
        must(cli("scp", "ticnt1:/tmp/put.txt", back), "scp down")
        if open(src).read() != open(back).read():
            raise Fail("the file did not survive the scp round trip")
        d = os.path.join(tmp, "tidir")
        os.mkdir(d)
        open(os.path.join(d, "a"), "w").write("a\n")
        open(os.path.join(d, "b"), "w").write("bb\n")
        must(cli("scp", "-r", d, "ticnt1:/tmp/"), "scp -r up")
        out = ssh("ticnt1", "cat /tmp/tidir/a /tmp/tidir/b")
        if out != "a\nbb\n":
            raise Fail("scp -r: the directory arrived as %r" % out)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def t_vmount():
    # The source is a path the server sees. A toy sees nothing of the host
    # but its bulk, as /root/bulk_host, so the directory is made there.
    host = tempfile.mkdtemp(prefix="testitall_mnt_", dir=BULK if TOY else None)
    seen = shared_file(os.path.basename(host))[1]
    try:
        open(os.path.join(host, "marker"), "w").write("from the host\n")
        add_cnt("ticnt3", "s", ("startup_env=TI_VAR=hello",
                                "vmount=%s:/shared" % seen))
        wait_answer("ticnt3")
        if ssh("ticnt3", "cat /shared/marker") != "from the host\n":
            raise Fail("vmount=: /shared/marker is not the host file")
        ssh("ticnt3", "echo back > /shared/written")
        if open(os.path.join(host, "written")).read() != "back\n":
            raise Fail("vmount=: a file written in the guest is not on the host")
        env = ssh("ticnt3", "tr '\\0' '\\n' < /proc/1/environ")
        if "TI_VAR=hello" not in env.split():
            raise Fail("startup_env=: TI_VAR is not in the environment of pid 1")
    finally:
        shutil.rmtree(host, ignore_errors=True)


def t_cnf_cnt():
    if TOY:
        # a container of the toy has no cgroup of its own: the cap is
        # refused, and that refusal is what is checked
        r = cli("cnf", "ticnt3", "cpu=25", "ram=100")
        if r.returncode == 0 or "no cgroup" not in r.stdout + r.stderr:
            raise Fail("toy: cnf cpu= ram= should be refused, a container "
                       "of a toy has no cgroup, got rc %d: %s"
                       % (r.returncode, (r.stdout + r.stderr).strip()[-200:]))
        print("      (toy: the cap is refused, no cgroup in a toy, as it "
              "must)")
        must(cli("del", "ticnt3"), "del")
        wait_topo("ticnt3", False)
        return
    must(cli("cnf", "ticnt3", "cpu=25", "ram=100"), "cnf cap")
    got = ssh("ticnt3", "echo cpu.max=$(cat /sys/fs/cgroup/cpu.max) "
                        "memory.max=$(cat /sys/fs/cgroup/memory.max)")
    if "cpu.max=25000 100000" not in got or "memory.max=104857600" not in got:
        raise Fail("the cap is not in the cgroup: %s" % got.strip())
    # a cap not named keeps its value
    must(cli("cnf", "ticnt3", "cpu=0"), "cnf lift cpu")
    got = ssh("ticnt3", "echo cpu.max=$(cat /sys/fs/cgroup/cpu.max) "
                        "memory.max=$(cat /sys/fs/cgroup/memory.max)")
    if "cpu.max=max 100000" not in got or "memory.max=104857600" not in got:
        raise Fail("cpu=0 alone: cpu lifted and ram kept wanted: %s"
                   % got.strip())
    must(cli("cnf", "ticnt3", "ram=0"), "cnf lift ram")
    got = ssh("ticnt3", "echo cpu.max=$(cat /sys/fs/cgroup/cpu.max) "
                        "memory.max=$(cat /sys/fs/cgroup/memory.max)")
    if "cpu.max=max 100000" not in got or "memory.max=max" not in got:
        raise Fail("the cap was not lifted: %s" % got.strip())
    must(cli("del", "ticnt3"), "del")
    wait_topo("ticnt3", False)


def rstp_state(lan):
    end = time.time() + 10
    while time.time() < end:
        r = cli("dump", "ovs-vsctl", "get", "Bridge", "_b_" + lan,
                "rstp_enable", quiet=True)
        if r.returncode == 0 and r.stdout.strip() in ("true", "false"):
            return r.stdout.strip()
        time.sleep(1)
    raise Fail("ovs-vsctl get Bridge _b_%s rstp_enable: %s"
               % (lan, (r.stderr + r.stdout).strip()))


def t_rstp():
    must(cli("plug", "ticnt1", "eth0", "tilan1"), "plug")
    must(cli("cnf", "tilan1", "rstp=on"), "cnf tilan1 rstp=on")
    end = time.time() + 10
    while rstp_state("tilan1") != "true" and time.time() < end:
        time.sleep(1)
    if rstp_state("tilan1") != "true":
        raise Fail("rstp on: the bridge says rstp_enable=false")
    must(cli("cnf", "tilan1", "rstp=off"), "cnf tilan1 rstp=off")
    end = time.time() + 10
    while rstp_state("tilan1") != "false" and time.time() < end:
        time.sleep(1)
    if rstp_state("tilan1") != "false":
        raise Fail("rstp off: the bridge says rstp_enable=true")


def t_dump():
    for what in ("config", "topo", "drawing", "commands", "pid", "images",
                 "namespaces"):
        must(cli("dump", what), "dump %s" % what)
    t = ftopo()
    if t.get("network") != NET:
        raise Fail("dump json: network is %r" % t.get("network"))
    lst = must(cli("dump", "commands"), "dump commands")
    if "add cnt ticnt1" not in lst:
        raise Fail("dump commands does not hold 'add cnt ticnt1':\n%s" % lst)
    out = must(cli("dump", "ovs-vsctl", "list-ports", "_b_tilan1"),
               "dump ovs-vsctl list-ports")
    if not out.strip():
        raise Fail("ovs-vsctl list-ports _b_tilan1 printed nothing")
    pid = must(cli("dump", "pid"), "dump pid")
    if "ticnt1" not in pid:
        raise Fail("dump pid does not name ticnt1")


def t_position():
    must(cli("cnf", "ticnt1", "x=-100", "y=50", ack=2), "cnf x= y=")
    must(cli("cnf", "ticnt1", "eth0", "angle=90", ack=2), "cnf angle=")


def t_tap():
    must(cli("add", "tap", "titap1"), "add tap")
    wait_topo("titap1", True)
    must(cli("plug", "titap1", "tilan1"), "plug tap")
    if TOY:
        print("      (toy: the tap is in the toy's network namespace, "
              "not addressed from the host)")
        must(cli("del", "titap1"), "del tap")
        wait_topo("titap1", False)
        return
    show("ip link show titap1")
    r = subprocess.run(["ip", "link", "show", "titap1"], capture_output=True,
                       text=True)
    if r.returncode:
        raise Fail("titap1 is not an interface of the host")
    if sudo_ok():
        must(sudo("ip", "addr", "add", TAP_NET + ".1/24", "dev", "titap1"),
             "address on the tap")
        must(sudo("ip", "link", "set", "titap1", "up"), "tap up")
        ssh("ticnt1", "ip addr flush dev eth0")
        address("ticnt1", 0, TAP_NET + ".2/24")
        s = Sniff("ticnt1", 0, "icmp and host " + TAP_NET + ".1").start()
        ping("ticnt1", TAP_NET + ".1", 3)
        expect_count(s, 6, what="3 requests and 3 replies through the tap")
    else:
        print("      (no passwordless sudo: the tap is not addressed)")
    must(cli("del", "titap1"), "del tap")
    wait_topo("titap1", False)
    r = subprocess.run(["ip", "link", "show", "titap1"], capture_output=True)
    if r.returncode == 0:
        raise Fail("titap1 is still an interface of the host after del")
    if PHY:
        must(cli("add", "phy", PHY, "--macvlan"), "add phy macvlan")
        wait_topo(PHY + "_1", True)
        must(cli("del", PHY + "_1"), "del phy")
        wait_topo(PHY + "_1", False)


def t_nat_cnt():
    port = host_port_free()
    show("python3 -m http.server %d --bind 127.0.0.1" % port)
    srv = subprocess.Popen([sys.executable, "-m", "http.server", str(port),
                            "--bind", "127.0.0.1"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           cwd="/tmp")
    try:
        must(cli("add", "nat", "tinat1"), "add nat")
        wait_topo("tinat1", True)
        must(cli("plug", "tinat1", "tinatlan"), "plug nat")
        must(cli("unplug", "ticnt1", "eth0", "tilan1"), "unplug")
        must(cli("plug", "ticnt1", "eth0", "tinatlan"), "plug")
        ssh("ticnt1", "ip addr flush dev eth0")
        address("ticnt1", 0, "10.117.0.10/24")
        ssh("ticnt1", "ip route replace default via 10.117.0.1")
        s = Sniff("ticnt1", 0, "tcp port %d" % port).start()
        got = ssh("ticnt1", "exec 3<>/dev/tcp/10.117.0.1/%d; "
                            "printf 'GET / HTTP/1.0\\r\\n\\r\\n' >&3; "
                            "head -c 12 <&3" % port, check=False)
        if not got.startswith("HTTP/1.0"):
            raise Fail("the host server on 127.0.0.1:%d is not reached at "
                       "10.117.0.1 through the nat: %r" % (port, got))
        expect_count(s, 3, what="the tcp exchange through the nat")
        must(cli("del", "tinat1"), "del nat")
        wait_topo("tinat1", False)
        must(cli("unplug", "ticnt1", "eth0", "tinatlan"), "unplug")
    finally:
        srv.terminate()


def t_a2b():
    add_cnt("ticntB", "s")
    must(cli("add", "a2b", "tia2b"), "add a2b")
    wait_topo("tia2b", True)
    for item, num, lan in (("ticnt1", "eth0", "tilanA"), ("tia2b", "A", "tilanA"),
                           ("tia2b", "B", "tilanB"), ("ticntB", "eth0", "tilanB")):
        must(cli("plug", item, num, lan, ack=5), "plug")
    wait_answer("ticntB")
    ssh("ticnt1", "ip addr flush dev eth0")
    address("ticnt1", 0, "10.2.2.1/24")
    address("ticntB", 0, "10.2.2.2/24")
    time.sleep(1)
    loss, base = ping_stats("ticnt1", "10.2.2.2", 20)
    if loss != 0 or base is None or base > 50:
        raise Fail("clean a2b: loss %s%%, rtt %s ms" % (loss, base))
    must(cli("cnf", "tia2b", "A2B delay=100"),
         "cnf delay")
    time.sleep(1)
    loss, rtt = ping_stats("ticnt1", "10.2.2.2", 10)
    if rtt is None or rtt < 80:
        raise Fail("delay=100 one way: rtt is %s ms" % rtt)
    must(cli("cnf", "tia2b", "A2B delay=0"),
         "cnf delay 0")
    must(cli("cnf", "tia2b", "A2B loss=5000"),
         "cnf loss")
    time.sleep(1)
    sa = Sniff("ticnt1", 0, "icmp").start()
    sb = Sniff("ticntB", 0, "icmp").start()
    loss, _ = ping_stats("ticnt1", "10.2.2.2", 100)
    na, nb = sa.stop(), sb.stop()
    if not 20 <= loss <= 80:
        raise Fail("loss=5000 (50%%): ping reports %d%% lost" % loss)
    if nb >= na or nb < 20:
        raise Fail("loss A2B: %d icmp before the cable, %d after"
                   % (na, nb))
    must(cli("cnf", "tia2b", "A2B loss=0"),
         "cnf loss 0")
    time.sleep(1)
    loss, rtt = ping_stats("ticnt1", "10.2.2.2", 20)
    if loss != 0 or rtt is None or rtt > 50:
        raise Fail("a2b restored: loss %s%%, rtt %s ms" % (loss, rtt))
    must(cli("del", "tia2b"), "del a2b")
    wait_topo("tia2b", False)
    must(cli("del", "ticntB"), "del")
    wait_topo("ticntB", False)
    must(cli("unplug", "ticnt1", "eth0", "tilanA"), "unplug")
    must(cli("plug", "ticnt1", "eth0", "tilan1"), "plug")


def c2c_evidence(name):
    """What a c2c that does not carry frames looks like, for the report."""
    out = []
    for net in (NET, PEER):
        for c in ftopo(net).get("c2c", []) or []:
            if c.get("name") == name:
                out.append("%s: master=%s tcp=%s udp=%s loc_udp_port=%s "
                           "dist_udp_port=%s"
                           % (net, c.get("local_is_master"),
                              c.get("tcp_connection"), c.get("udp_connection"),
                              c.get("loc_udp_port"), c.get("dist_udp_port")))
    for net in (NET, PEER):
        d = "/tmp/cloonix_proxymous_%s" % net
        socks = sorted(f for f in os.listdir(d) if f.startswith("dgram_c2c")) \
            if os.path.isdir(d) else []
        out.append("%s: %s" % (d, " ".join(socks) or "no dgram_c2c socket"))
    r = subprocess.run(["pgrep", "-af", "cloonix-ovs-c2c"], capture_output=True,
                       text=True)
    out.append("cloonix-ovs-c2c processes:\n    "
               + (r.stdout.strip().replace("\n", "\n    ") or "none"))
    out.append("the syslog between the two 'add c2c' of this run says the "
               "rest: grep -E 'c2c|udp proxy' /var/log/syslog")
    return "\n".join(out)


def c2c_peered(name, net):
    for c in ftopo(net).get("c2c", []) or []:
        if c.get("name") == name:
            return c.get("tcp_connection") == 1 and c.get("udp_connection") == 1
    return False


def t_c2c():
    name = "%s2%s" % (NET, PEER)
    add_cnt("tipeer", "s", net=PEER)
    must(cli("add", "c2c", name, PEER), "add c2c")
    end = time.time() + 60
    while time.time() < end:
        if in_topo(name) and in_topo(name, PEER) and c2c_peered(name, NET):
            break
        time.sleep(1)
    else:
        raise Fail("the c2c %s is not peered on tcp and udp within 60s\n%s"
                   % (name, c2c_evidence(name)))
    must(cli("plug", name, "tilan1"), "plug c2c")
    must(cli("plug", name, "tilanp", net=PEER), "plug c2c peer")
    must(cli("plug", "tipeer", "eth0", "tilanp", net=PEER), "plug peer")
    wait_answer("tipeer", PEER)
    ssh("ticnt1", "ip addr flush dev eth0")
    address("ticnt1", 0, "10.3.3.1/24")
    address("tipeer", 0, "10.3.3.2/24", net=PEER)
    s_arp = Sniff("tipeer", 0, "arp", net=PEER).start()
    s_icmp = Sniff("tipeer", 0, "icmp", net=PEER).start()
    ping("ticnt1", "10.3.3.2", 4)
    expect_count(s_arp, 1, what="an arp crossed the c2c")
    expect_count(s_icmp, 8, what="4 requests and 4 replies over the c2c")
    ping("tipeer", "10.3.3.1", 3, net=PEER)
    must(cli("del", name), "del c2c")
    wait_topo(name, False)
    wait_topo(name, False, net=PEER)
    must(cli("del", "tipeer", net=PEER), "del peer")
    wait_topo("tipeer", False, net=PEER)


def t_kvm():
    wait_answer("tikvm1", timeout=180)
    wait_answer("tikvm2", timeout=180)
    must(cli("plug", "tikvm1", "eth0", "tilanK", ack=5), "plug")
    must(cli("plug", "tikvm2", "eth0", "tilanK", ack=5), "plug")
    must(cli("plug", "tikvm1", "eth1", "tilan1", ack=5), "plug")
    time.sleep(2)
    address("tikvm1", 0, "10.4.4.1/24")
    address("tikvm2", 0, "10.4.4.2/24")
    address("tikvm1", 1, "10.3.3.5/24")
    s = Sniff("tikvm1", 0, "icmp").start()
    ping("tikvm1", "10.4.4.2", 4)
    expect_count(s, 8, what="4 requests and 4 replies kvm to kvm")
    # the not sniffed eth of a kvm, armed by the sniff itself
    s = Sniff("tikvm1", 1, "icmp").start()
    ping("ticnt1", "10.3.3.5", 4)
    expect_count(s, 8, what="4 requests and 4 replies cnt to kvm")
    tmp = tempfile.mkdtemp(prefix="testitall_")
    try:
        src = os.path.join(tmp, "kput.txt")
        open(src, "w").write("kvm %d\n" % os.getpid())
        must(cli("scp", src, "tikvm1:/tmp/"), "scp to a kvm")
        if ssh("tikvm1", "cat /tmp/kput.txt") != open(src).read():
            raise Fail("the file did not arrive in the kvm as sent")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def t_nat_kvm():
    must(cli("unplug", "tikvm2", "eth0", "tilanK"), "unplug")
    must(cli("add", "nat", "tinatk"), "add nat")
    wait_topo("tinatk", True)
    must(cli("plug", "tinatk", "tinatK"), "plug nat")
    must(cli("plug", "tikvm2", "eth0", "tinatK"), "plug")
    ssh("tikvm2", "ip addr flush dev eth0")
    s = Sniff("tikvm2", 0, "udp port 67 or udp port 68").start()
    ssh("tikvm2", "timeout 30 dhclient -1 eth0")
    expect_count(s, 2, what="the dhcp exchange with passt")
    got = ssh("tikvm2", "ip -4 -o addr show dev eth0")
    if "10.117.0.10/" not in got:
        raise Fail("dhclient did not get 10.117.0.10: %s" % got.strip())
    ssh("tikvm2", "iperf3 -s -D")
    hport = host_port_free()
    must(cli("cnf", "tinatk", "publish=%d:5201" % hport),
         "cnf publish")
    end = time.time() + 15
    while time.time() < end and not host_connect(hport):
        time.sleep(1)
    if not host_connect(hport):
        raise Fail("127.0.0.1:%d published to the kvm's 5201 refuses the "
                   "connection" % hport)
    must(cli("cnf", "tinatk", "publish=none"), "cnf none")
    end = time.time() + 15
    while time.time() < end and host_connect(hport):
        time.sleep(1)
    if host_connect(hport):
        raise Fail("127.0.0.1:%d still answers after publish=none" % hport)
    show("ping -c 1 -W 2 1.1.1.1")
    host_out = subprocess.run(["ping", "-c", "1", "-W", "2", "1.1.1.1"],
                              capture_output=True).returncode == 0
    if host_out:
        s = Sniff("tikvm2", 0, "icmp and host 1.1.1.1").start()
        ping("tikvm2", "1.1.1.1", 3)
        expect_count(s, 4, what="the internet through the nat")
    else:
        print("      (the host does not reach 1.1.1.1: the internet through "
              "the nat is not checked)")
    must(cli("del", "tinatk"), "del nat")
    wait_topo("tinatk", False)


def t_save():
    path, seen = shared_file("testitall_%s_tikvm1.qcow2" % NET)
    if os.path.exists(path):
        os.unlink(path)
    try:
        must(cli("save", "tikvm1", seen, ack=300), "save")
        end = time.time() + 30
        while time.time() < end and not os.path.exists(path):
            time.sleep(1)
        if not os.path.exists(path):
            raise Fail("save answered ok and %s does not exist" % path)
        if open(path, "rb").read(4) != b"QFI\xfb":
            raise Fail("%s is not a qcow2" % path)
    finally:
        if os.path.exists(path):
            os.unlink(path)


def t_web():
    port = cfg_value(NET, "cloonix_web")
    passwd = cfg_value(NET, "cloonix_passwd")
    if not port or not passwd or not shutil.which("curl"):
        print("      (no cloonix_web for %s or no curl: not checked)" % NET)
        return
    def code(pw):
        show("curl -sk -o /dev/null -w %%{http_code} -u %s:... https://127.0.0.1:%s/"
             % (NET, port))
        r = subprocess.run(["curl", "-sk", "-o", "/dev/null", "-w",
                            "%{http_code}", "-u", "%s:%s" % (NET, pw),
                            "https://127.0.0.1:%s/" % port],
                           capture_output=True, text=True)
        return r.stdout.strip()
    got = code("not_the_password")
    if got != "401":
        raise Fail("https with a wrong password: expected 401, got %s" % got)
    got = code(passwd)
    if got != "200":
        raise Fail("https with the right password: expected 200, got %s" % got)


def t_rmall():
    for net in (NET, PEER):
        erase(net)


# the flows: every byte path under load: iperf3 cnt to cnt, sniff to a reading
# and to a non reading client, scp through the agent, c2c, a2b rate, nat.

IPERF_PORT = 5201
FA, FB = "tiflowA", "tiflowB"


def iperf_server(node, net=None, port=IPERF_PORT):
    """iperf3 -D forks before it listens, so the process being there is not
    the port being open: a client that connects in between is reset, and
    through a nat the reset reaches the host as the answer of the guest.
    The listening socket is what is waited for."""
    ssh(node, "pkill -f 'iperf3 -s' 2>/dev/null; true", net=net, check=False)
    # its own account in /tmp/iperf3_s.log of the guest, for a failure
    ssh(node, "rm -f /tmp/iperf3_s.log; iperf3 -s -p %d -D "
              "--logfile /tmp/iperf3_s.log >/dev/null 2>&1" % port, net=net)
    for _ in range(40):
        if ssh(node, "ss -Hltn | grep -q ':%d ' && echo up" % port, net=net,
               check=False).strip():
            return
        time.sleep(0.5)
    raise Fail("iperf3 -s does not listen on %d in %s" % (port, node))


def iperf_parse(out):
    """(sender Mbit/s, receiver Mbit/s, udp loss percent or None)."""
    snd = rcv = loss = None
    for line in out.splitlines():
        m = None
        for tag in ("sender", "receiver"):
            if tag in line:
                m = tag
        if not m:
            continue
        w = line.split()
        try:
            i = w.index("Mbits/sec")
        except ValueError:
            continue
        val = float(w[i - 1])
        if m == "sender":
            snd = val
        else:
            rcv = val
            if "%" in line:
                loss = float(line.split("(")[1].split("%")[0])
    return snd, rcv, loss


def iperf_run(node, target, secs=4, extra="", net=None, port=IPERF_PORT):
    out = ssh(node, "iperf3 -c %s -p %d -t %d -f m %s 2>&1"
                    % (target, port, secs, extra), net=net, check=False,
              timeout=secs + 60)
    snd, rcv, loss = iperf_parse(out)
    if snd is None and rcv is None:
        raise Fail("iperf3 %s -> %s gave no result:\n%s"
                   % (node, target, out.strip()[-600:]))
    return snd, rcv, loss


def iperf_bg(node, target, secs, extra="", net=None, port=IPERF_PORT):
    """The same run left in the background; returns the Popen."""
    cmd = "iperf3 -c %s -p %d -t %d -f m %s 2>&1" % (target, port, secs, extra)
    show('cloonix cli %s ssh %s "%s" &' % (net or NET, node, cmd))
    return subprocess.Popen(CLOONIX + ["cli", net or NET, "ssh", node, cmd],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, stdin=subprocess.DEVNULL)


def mbits(label, snd, rcv=None, loss=None):
    extra = ""
    if rcv is not None:
        extra += "  receiver %.1f" % rcv
    if loss is not None:
        extra += "  loss %.1f%%" % loss
    print("      %-32s %8.1f Mbit/s%s" % (label, snd if snd is not None
                                           else rcv, extra))


def big_file(path, mega):
    show("dd if=/dev/urandom of=%s bs=1M count=%d" % (path, mega))
    with open(path, "wb") as f:
        for _ in range(mega):
            f.write(os.urandom(1 << 20))


def md5_host(path):
    import hashlib
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def md5_guest(node, path, net=None):
    return ssh(node, "md5sum %s" % path, net=net).split()[0]


def t_flow_lan():
    add_cnt(FA, "s", rootfs=PERF_ROOTFS)
    add_cnt(FB, "s", rootfs=PERF_ROOTFS)
    must(cli("plug", FA, "eth0", "tilanF", ack=5), "plug")
    must(cli("plug", FB, "eth0", "tilanF", ack=5), "plug")
    wait_answer(FA)
    wait_answer(FB)
    address(FA, 0, "10.6.6.1/24")
    address(FB, 0, "10.6.6.2/24")
    iperf_server(FB)
    snd, rcv, _ = iperf_run(FA, "10.6.6.2")
    mbits("cnt to cnt on one lan", snd, rcv)
    if snd < 50:
        raise Fail("a lan between two containers carries %.1f Mbit/s" % snd)


def t_flow_sniff():
    """The sniff stream to a client that reads it, then to one that does not."""
    s = Sniff(FB, 0, "").start()
    snd, rcv, _ = iperf_run(FA, "10.6.6.2")
    n = s.stop()
    mbits("iperf under an unfiltered sniff", snd, rcv)
    print("      the sniff client received %d packets" % n)
    if n < 1000:
        raise Fail("the unfiltered sniff of an iperf run delivered %d "
                   "packets" % n)
    if snd < 20:
        raise Fail("iperf fell to %.1f Mbit/s under a sniff that is read" % snd)
    must(cli("dump", "commands", quiet=True), "dump commands after the sniff")

    # A client that does not read: nothing on the lan or from the server may
    # depend on it.
    show("cloonix cli %s sniff %s eth0 --bytes   (stdout never read)"
         % (NET, FB))
    stuck = subprocess.Popen(CLOONIX + ["cli", NET, "sniff", FB, "eth0",
                                        "--bytes"],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.DEVNULL)
    try:
        time.sleep(2)
        snd, rcv, _ = iperf_run(FA, "10.6.6.2", secs=6)
        mbits("iperf under a sniff nobody reads", snd, rcv)
        if snd < 20:
            raise Fail("iperf fell to %.1f Mbit/s under a stalled sniff "
                       "client" % snd)
        r = cli("dump", "commands", timeout=10)
        must(r, "dump commands while a sniff client is stalled")
        ssh(FA, "echo alive", timeout=30)
    finally:
        stuck.kill()
        stuck.wait()
    time.sleep(2)
    s = Sniff(FB, 0, "icmp").start()
    ping(FA, "10.6.6.2", 3)
    expect_count(s, 6, what="a new sniff after the stalled one")


def t_flow_scp():
    """scp through the agent, alone, then while iperf and a sniff both run."""
    tmp = tempfile.mkdtemp(prefix="testitall_flow_")
    try:
        src = os.path.join(tmp, "big.bin")
        big_file(src, 48)
        want = md5_host(src)
        t = time.time()
        must(cli("scp", src, FA + ":/tmp/big.bin"), "scp up")
        up = time.time() - t
        if md5_guest(FA, "/tmp/big.bin") != want:
            raise Fail("48 MB to a cnt: the md5 differs")
        back = os.path.join(tmp, "back.bin")
        t = time.time()
        must(cli("scp", FA + ":/tmp/big.bin", back), "scp down")
        down = time.time() - t
        if md5_host(back) != want:
            raise Fail("48 MB back from a cnt: the md5 differs")
        print("      48 MB through the agent: up %.1f MB/s, down %.1f MB/s"
              % (48 / up, 48 / down))

        s = Sniff(FB, 0, "").start()
        bg = iperf_bg(FA, "10.6.6.2", 20)
        try:
            t = time.time()
            must(cli("scp", src, FB + ":/tmp/big.bin"), "scp up under load")
            up = time.time() - t
            if md5_guest(FB, "/tmp/big.bin") != want:
                raise Fail("48 MB into the iperf receiver, under a sniff: "
                           "the md5 differs")
            os.unlink(back)
            t = time.time()
            must(cli("scp", FB + ":/tmp/big.bin", back), "scp down under load")
            down = time.time() - t
            if md5_host(back) != want:
                raise Fail("48 MB back under load: the md5 differs")
            print("      48 MB while iperf and a sniff run: up %.1f MB/s, "
                  "down %.1f MB/s" % (48 / up, 48 / down))
        finally:
            n = s.stop()
            bg.kill()
            bg.wait()
            ssh(FA, "pkill -f 'iperf3 -c' 2>/dev/null; true", check=False)
        print("      the sniff beside the scp received %d packets" % n)
        if n < 1000:
            raise Fail("the sniff beside the scp received %d packets" % n)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def t_flow_a2b():
    must(cli("add", "a2b", "tiflowab"), "add a2b")
    wait_topo("tiflowab", True)
    must(cli("unplug", FB, "eth0", "tilanF"), "unplug")
    must(cli("plug", "tiflowab", "A", "tilanF", ack=5), "plug")
    must(cli("plug", "tiflowab", "B", "tilanG", ack=5), "plug")
    must(cli("plug", FB, "eth0", "tilanG", ack=5), "plug")
    time.sleep(1)
    ping(FA, "10.6.6.2", 3)
    must(cli("cnf", "tiflowab", "A2B rate=1000"),
         "cnf rate")
    time.sleep(1)
    snd, rcv, _ = iperf_run(FA, "10.6.6.2")
    mbits("a2b rate 1000 kB/s, A to B", snd, rcv)
    if rcv is None or not 2 <= rcv <= 12:
        raise Fail("rate=1000 kB/s (8 Mbit/s) one way: the receiver got "
                   "%s Mbit/s" % rcv)
    snd, rcv, _ = iperf_run(FA, "10.6.6.2", extra="-R")
    mbits("the other direction, not limited", snd, rcv)
    if rcv is None or rcv < 30:
        raise Fail("rate on A2B slowed B2A too: %s Mbit/s" % rcv)
    must(cli("cnf", "tiflowab", "A2B rate=0"),
         "cnf rate 0")
    time.sleep(1)
    snd, rcv, _ = iperf_run(FA, "10.6.6.2")
    mbits("rate removed", snd, rcv)
    if snd < 50:
        raise Fail("rate 0 did not restore the cable: %.1f Mbit/s" % snd)
    must(cli("del", "tiflowab"), "del a2b")
    wait_topo("tiflowab", False)
    must(cli("unplug", FB, "eth0", "tilanG"), "unplug")
    must(cli("plug", FB, "eth0", "tilanF", ack=5), "plug")


def t_flow_c2c():
    name = "%s2%s" % (NET, PEER)
    add_cnt("tiflowP", "s", net=PEER, rootfs=PERF_ROOTFS)
    must(cli("add", "c2c", name, PEER), "add c2c")
    end = time.time() + 60
    while time.time() < end:
        if in_topo(name) and in_topo(name, PEER) and c2c_peered(name, NET):
            break
        time.sleep(1)
    else:
        raise Fail("the c2c %s is not peered on tcp and udp within 60s\n%s"
                   % (name, c2c_evidence(name)))
    must(cli("plug", name, "tilanF"), "plug c2c")
    must(cli("plug", name, "tilanP", net=PEER), "plug c2c peer")
    must(cli("plug", "tiflowP", "eth0", "tilanP", net=PEER), "plug peer")
    wait_answer("tiflowP", PEER)
    address("tiflowP", 0, "10.6.6.3/24", net=PEER)
    # The cable is asked for on both nets; how long after the two ok the
    # frames cross is measured rather than assumed.
    t = time.time()
    end = t + 30
    while not ping(FA, "10.6.6.3", 1, check=False):
        if time.time() > end:
            raise Fail("nothing crosses the c2c 30s after both ends are "
                       "cabled\n" + c2c_evidence(name))
        time.sleep(1)
    print("      the c2c carried a ping %.0fs after the cabling"
          % (time.time() - t))
    iperf_server("tiflowP", net=PEER)
    snd, rcv, _ = iperf_run(FA, "10.6.6.3", secs=5)
    mbits("tcp across the c2c", snd, rcv)
    if rcv is None or rcv < 5:
        raise Fail("tcp across the c2c: the receiver got %s Mbit/s" % rcv)
    snd, rcv, loss = iperf_run(FA, "10.6.6.3", secs=4, extra="-u -b 20M")
    mbits("udp 20 Mbit/s across the c2c", snd, rcv, loss)
    if loss is None or loss > 50:
        raise Fail("udp 20 Mbit/s across the c2c: %s%% lost" % loss)
    must(cli("del", name), "del c2c")
    wait_topo(name, False)
    wait_topo(name, False, net=PEER)
    must(cli("del", "tiflowP", net=PEER), "del peer")
    wait_topo("tiflowP", False, net=PEER)


def publish_evidence(hport):
    """What a failed publish leaves behind: who holds the host port, what
    passt said (proxymous keeps its log next to its socket, bundle and toy
    alike), what the guest server saw, and the nat lines of the syslog."""
    out = []
    r = subprocess.run(["ss", "-ltnp"], capture_output=True, text=True)
    out.append("  host port %d: %s" % (hport, " | ".join(
        l.strip() for l in r.stdout.splitlines() if ":%d " % hport in l)
        or "nobody listens"))
    for log in sorted(glob.glob("/tmp/*proxymous*/nat_p/tiflownat_passt.log")):
        try:
            txt = open(log).read().strip()
        except OSError as e:
            txt = str(e)
        out.append("  %s:\n    %s" % (log, txt[-4000:].replace("\n", "\n    ")
                                       or "(empty)"))
    out.append("  guest iperf3 -s log:\n    %s"
               % ssh(FB, "cat /tmp/iperf3_s.log", check=False).strip()
                 .replace("\n", "\n    "))
    # The toy's proxymous, which spawns passt, logs in the host syslog, not
    # in the toy's own: both are read.
    syslog = os.environ.get("TESTITALL_SYSLOG", "/var/log/syslog")
    if syslog != "/var/log/syslog":
        syslog += " /var/log/syslog"
    r = subprocess.run("grep -ih 'nat\\|passt' %s | tail -25" % syslog,
                       shell=True, capture_output=True, text=True)
    out.append("  syslog nat lines:\n    %s"
               % (r.stdout.strip().replace("\n", "\n    ") or "(none)"))
    return "\n".join(out)


def tcp_lines(packets, limit=12):
    """The first tcp frames of a sniff, one line each: who, to whom, flags."""
    out = []
    for p in packets[:limit]:
        if len(p) < 54 or p[12:14] != b"\x08\x00" or p[23] != 6:
            out.append("    %d bytes, not tcp over ipv4" % len(p))
            continue
        ihl = (p[14] & 15) * 4
        t = p[14 + ihl:]
        flags = "".join(c for c, b in zip("FSRPAU", (1, 2, 4, 8, 16, 32))
                        if t[13] & b)
        out.append("    %s:%d > %s:%d [%s] from mac %s"
                   % (".".join(map(str, p[26:30])), (t[0] << 8) | t[1],
                      ".".join(map(str, p[30:34])), (t[2] << 8) | t[3],
                      flags, ":".join("%02x" % x for x in p[6:12])))
    return "\n".join(out) or "    (no frame)"


def t_flow_nat():
    if not shutil.which("iperf3"):
        print("      (no iperf3 on the host: the nat flow is not measured)")
        return
    port = host_port_free()
    show("iperf3 -s -1 -p %d -B 127.0.0.1" % port)
    srv = subprocess.Popen(["iperf3", "-s", "-1", "-p", str(port), "-B",
                            "127.0.0.1"], stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
    try:
        must(cli("add", "nat", "tiflownat"), "add nat")
        wait_topo("tiflownat", True)
        must(cli("plug", "tiflownat", "tilanN"), "plug nat")
        must(cli("unplug", FB, "eth0", "tilanF"), "unplug")
        must(cli("plug", FB, "eth0", "tilanN"), "plug")
        ssh(FB, "ip addr flush dev eth0")
        address(FB, 0, "10.117.0.10/24")
        ssh(FB, "ip route replace default via 10.117.0.1")
        time.sleep(1)
        snd, rcv, _ = iperf_run(FB, "10.117.0.1", secs=4, port=port)
        mbits("cnt to the host through the nat", snd, rcv)
        if rcv is None or rcv < 5:
            raise Fail("iperf through the nat to the host: %s Mbit/s" % rcv)
        srv.wait(timeout=10)
        hport = host_port_free()
        must(cli("cnf", "tiflownat", "publish=%d:%d"
                 % (hport, IPERF_PORT)), "cnf publish")
        iperf_server(FB)
        # No probing connect (iperf3 -s takes it as a test, then resets): the ok of
        # the publish is under test. A passt reset (guest silent to the syn) is
        # retried once 3 s later, sniffed on the guest: an answer then is a race
        # fault, none a broken publish.
        first = None
        seen = ""
        for attempt in (1, 2):
            sn = None
            if attempt == 2:
                sn = Sniff(FB, 0, "tcp port %d" % IPERF_PORT).start()
            show("iperf3 -c 127.0.0.1 -p %d -t 4 -f m" % hport)
            t0 = time.time()
            r = subprocess.run(["iperf3", "-c", "127.0.0.1", "-p", str(hport),
                                "-t", "4", "-f", "m"], capture_output=True,
                               text=True, timeout=60)
            took = time.time() - t0
            snd, rcv, _ = iperf_parse(r.stdout)
            if sn:
                sn.stop()
                seen = ("  second try, ended after %.1f s, %d frame(s) of port "
                        "%d seen on %s eth0:\n%s\n  in %s:\n    %s"
                        % (took, sn.count, IPERF_PORT, FB,
                           tcp_lines(sn.packets), FB,
                           ssh(FB, "ip -4 addr show eth0 | grep inet; "
                                   "ip neigh; ss -Htan | grep ':%d '; "
                                   "pgrep -a iperf3" % IPERF_PORT,
                               check=False).strip().replace("\n", "\n    ")))
            if snd is not None or rcv is not None:
                break
            if first is None:
                first = ("first try, ended after %.1f s: %s"
                         % (took, (r.stdout + r.stderr).strip()[-400:]))
                time.sleep(3)
        if snd is None and rcv is None:
            raise Fail("iperf3 from the host into the published port, twice "
                       "3 s apart:\n%s\n%s\n%s"
                       % (first, seen, publish_evidence(hport)))
        if first is not None:
            raise Fail("the published port answered only on the second try, "
                       "3 s after the ok of cnf publish=: the new passt was not "
                       "ready for the guest when the ok was given\n%s\n%s\n%s"
                       % (first, seen, publish_evidence(hport)))
        mbits("host to cnt through publish=", snd, rcv)
        if rcv is None or rcv < 5:
            raise Fail("iperf from the host through publish=: %s Mbit/s" % rcv)
        must(cli("del", "tiflownat"), "del nat")
        wait_topo("tiflownat", False)
        must(cli("unplug", FB, "eth0", "tilanN"), "unplug")
    except Fail as e:
        # Every failure of the phase, a hang included, comes with the
        # evidence, once.
        if "passt.log" in str(e):
            raise
        raise Fail("%s\n%s" % (e, publish_evidence(port)))
    finally:
        srv.kill()


def t_flow_end():
    for name in (FA, FB):
        must(cli("del", name), "del")
        wait_topo(name, False)


def t_flow_scp_kvm():
    tmp = tempfile.mkdtemp(prefix="testitall_flow_")
    try:
        src = os.path.join(tmp, "big.bin")
        big_file(src, 32)
        want = md5_host(src)
        t = time.time()
        must(cli("scp", src, "tikvm1:/tmp/big.bin"), "scp up to a kvm")
        up = time.time() - t
        if md5_guest("tikvm1", "/tmp/big.bin") != want:
            raise Fail("32 MB to a kvm over the virtio serial: the md5 differs")
        back = os.path.join(tmp, "back.bin")
        t = time.time()
        must(cli("scp", "tikvm1:/tmp/big.bin", back), "scp down from a kvm")
        down = time.time() - t
        if md5_host(back) != want:
            raise Fail("32 MB back from a kvm: the md5 differs")
        print("      32 MB over the virtio serial: up %.1f MB/s, down %.1f MB/s"
              % (32 / up, 32 / down))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


CHECKS = (
    ("list", t_net),
    ("kvm boot", t_kvm_boot),
    ("cnt+lan, sniff", t_cnt_lan),
    ("eth kinds", t_eth_kinds),
    ("unplug", t_del_lan),
    ("del", t_del_sat),
    ("scp", t_scp),
    ("vmount, startup_env", t_vmount),
    ("cnf caps", t_cnf_cnt),
    ("rstp", t_rstp),
    ("dump", t_dump),
    ("position", t_position),
    ("tap", t_tap),
    ("nat cnt", t_nat_cnt),
    ("a2b", t_a2b),
    ("c2c", t_c2c),
    ("flow lan", t_flow_lan),
    ("flow sniff, read and unread", t_flow_sniff),
    ("flow scp through the agent", t_flow_scp),
    ("flow a2b rate", t_flow_a2b),
    ("flow c2c", t_flow_c2c),
    ("flow nat", t_flow_nat),
    ("flow end", t_flow_end),
    ("kvm", t_kvm),
    ("flow scp to a kvm", t_flow_scp_kvm),
    ("nat kvm", t_nat_kvm),
    ("save", t_save),
    ("web", t_web),
    ("del all", t_rmall),
)


def phase(name):
    """A marker in the syslog so every KERR after it belongs to a named step.

    Outside the KERR namespace: the collector greps for both.
    """
    try:
        syslog.syslog("CLOONIX-PHASE %s" % name)
    except Exception:
        pass


def run_checks(checks, title, done):
    """The loop testitall.py and every sub_*.py share: first failure ends it."""
    print(title)
    for name, fn in checks:
        t = time.time()
        phase(name)
        print("[%6.1fs] %s" % (t - T0, name))
        try:
            fn()
        except Fail as err:
            print()
            print("FAILED: %s" % name)
            print("  %s" % str(err).replace("\n", "\n  "))
            print("  the last commands given:")
            for line in LAST:
                print("    %s" % line)
            print("  the nets are left as they are: cloonix cli %s dump topo"
                  % NET)
            sys.exit(1)
        print("      ok in %.1fs" % (time.time() - t))
    print()
    print("%s in %.0fs" % (done, time.time() - T0))


def main():
    run_checks(CHECKS, "testitall: every item on %s, peer %s" % (NET, PEER),
               "EVERY ITEM OK")


if __name__ == "__main__":
    main()
