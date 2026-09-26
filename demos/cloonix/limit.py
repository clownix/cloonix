#!/usr/bin/env python3
"""limit - a cpu and ram cap put on a running container, and measured.

    ./limit.py [net]                    default net: nemo

Two containers from the same image, one capped, one not::

      hog ---- lan1 ---- free
    10.0.0.1          10.0.0.2
    cpu=25 ram=100    no cap

    cloonix cli nemo cnf hog cpu=25 ram=100     the cap, on a running one
    cloonix cli nemo cnf hog cpu=0 ram=0        lifted

cpu is a percent of ONE core, ram is in megabytes, 0 means no cap. The
server answers ok only after reading the cap back from the kernel's
cgroup files, so an ok is a cap that is in force.

  ACT 1  the two containers, the lan, a ping
  ACT 2  the cap, and the cgroup files inside hog that now say so
  ACT 3  cpu: a busy loop in each, the cgroup counts what it got over 5s
  ACT 4  ram: 300 M allocated in each -- hog is killed at 100 M, free is not
  ACT 5  the cap lifted, the files read again
  ACT 6  dump pid, the cost of every guest as the server sees it

A privileged container has no cgroup and cannot be capped; the server
says so instead of pretending. So does a net whose host has no systemd
bus: crun then uses the cgroupfs manager, which cannot delegate a cgroup
to a user.

Standard library only, like cloonix itself.

Image: trixie
Size:  2 containers, 1 lan
"""
import os
import shlex
import subprocess
import sys
import time

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    CLOONIX, NET, erase_net, show, step, story)

CNT_ROOTFS_CHOICES = ("trixie",)

HOG = "hog"
FREE = "free"
CNT = (HOG, FREE)
LAN = "lan1"
ADDR = {HOG: "10.0.0.1", FREE: "10.0.0.2"}

CPU_CAP = 25          # percent of one core
RAM_CAP = 100         # megabytes
RAM_TRY = 300         # what ACT 4 allocates, well past the cap

def run(line, ack=None):
    """Show a cloonix command line, then give it to the client."""
    show(line)
    words = shlex.split(line)[1:]
    if ack is not None:
        words = ["--wait", str(ack)] + words
    return subprocess.run(CLOONIX + words, capture_output=True, text=True,
                          stdin=subprocess.DEVNULL)

def ssh(node, cmd, check=True):
    """One shell command inside a guest; the command is one argument."""
    show('cloonix cli %s ssh %s "%s"' % (NET, node, cmd))
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node, cmd],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if check and r.returncode:
        raise RuntimeError("%s: %s -> %s"
                           % (node, cmd,
                              (r.stderr or r.stdout).strip() or "no output"))
    return r

def in_lst(name):
    """Asked over and over, so not printed: cloonix cli <net> dump commands."""
    r = subprocess.run(CLOONIX + ["cli", NET, "dump", "commands"],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    return name in r.stdout.split()

def answers(node, timeout=3):
    """Asked over and over, so not printed: cloonix --wait 3 cli <net> ssh."""
    r = subprocess.run(CLOONIX + ["--wait", str(timeout), "cli", NET, "ssh",
                        node, "echo yes"],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    return "yes" in r.stdout

def wait_exists(name, timeout=20):
    end = time.time() + timeout
    while time.time() < end:
        if in_lst(name):
            return True
        time.sleep(1)
    return False

def wait_answer(node, timeout=90):
    end = time.time() + timeout
    while time.time() < end:
        if answers(node):
            return
        time.sleep(2)
    sys.exit("%s is in the list and never answered ssh" % node)

def check_net():
    r = subprocess.run(CLOONIX + ["cli", NET, "dump", "commands"],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if r.returncode:
        sys.exit("no answer from %s\n"
                 "  start it:  cloonix start %s" % (NET, NET))

def choose_image():
    """One throwaway container per candidate image, the first that
    appears in the list is the one."""
    said = ""
    for cand in CNT_ROOTFS_CHOICES:
        name = "probe_" + cand
        r = run("cloonix cli %s add cnt %s eth=1 %s" % (NET, name, cand))
        ok = (r.returncode == 0) and wait_exists(name, 20)
        run("cloonix cli %s del %s" % (NET, name), ack=0.3)
        if ok:
            return cand
        said = (r.stdout + r.stderr).strip() or "nothing"
    sys.exit("the server would not start a container on %s.\n"
             "  it said: %s\n"
             "  fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_bulk/"
             "trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm && ./cnt_trixie.sh"
             % (" or ".join(CNT_ROOTFS_CHOICES), said))

# Where the canvas draws it, sent before the items exist: x,y is the centre
# of an item, the eth angle runs clockwise from the top.
def place():
    for line in ("cnf %s x=-250 y=0" % HOG, "cnf %s x=0 y=0" % LAN,
                 "cnf %s x=250 y=0" % FREE,
                 "cnf %s eth0 angle=90" % HOG, "cnf %s eth0 angle=270" % FREE):
        run("cloonix cli %s %s" % (NET, line), ack=0.3)

# ACT 1

def create(name, rootfs):
    line = "cloonix cli %s add cnt %s eth=1 %s" % (NET, name, rootfs)
    last = ""
    for _ in range(6):
        r = run(line)
        last = (r.stdout + r.stderr).strip()
        if r.returncode == 0 and wait_exists(name):
            return
        if r.returncode == 0:
            last = "the add was taken and %s never appeared" % name
        time.sleep(2)
    sys.exit("%s was never created.\n  the server said: %s\n  by hand:  %s"
             % (name, last or "nothing", line))

def build(rootfs):
    for name in CNT:
        create(name, rootfs)
    for name in CNT:
        run("cloonix cli %s plug %s eth0 %s" % (NET, name, LAN))
    for name in CNT:
        wait_answer(name)
    for name in CNT:
        ssh(name, "ip addr add %s/24 dev eth0" % ADDR[name])
        ssh(name, "ip link set dev eth0 up")

def ping(node, target, count=5):
    r = ssh(node, "ping -c %d -W 2 %s" % (count, target), check=False)
    story()
    story(r.stdout)
    return r.returncode == 0

# ACT 2 and 5

def cap(cpu, ram):
    """The cap, answered ok only once the cgroup files hold it."""
    r = run("cloonix cli %s cnf %s cpu=%d ram=%d" % (NET, HOG, cpu, ram))
    said = (r.stdout + r.stderr).strip()
    if r.returncode:
        sys.exit("the cap was refused: %s\n"
                 "  A privileged container cannot be capped, and neither can\n"
                 "  any container when the host has no systemd bus for crun\n"
                 "  to delegate a cgroup through." % (said or "no reason"))
    story("      the server said: %s" % (said or "ok"))

def read_cgroup(node):
    """The two files the kernel enforces, seen from inside the container.

    /sys/fs/cgroup inside a container is its own cgroup, so cpu.max and
    memory.max there are the cap on it and nothing else.
    """
    out = ssh(node, "echo cpu.max=$(cat /sys/fs/cgroup/cpu.max) "
                    "memory.max=$(cat /sys/fs/cgroup/memory.max)").stdout
    story("      %-5s %s" % (node, out.strip()))
    return out

def show_caps(expect_cpu, expect_ram):
    story()
    got = read_cgroup(HOG)
    read_cgroup(FREE)
    want_cpu = ("%d 100000" % (expect_cpu * 1000)) if expect_cpu \
               else "max 100000"
    want_ram = str(expect_ram * 1048576) if expect_ram else "max"
    if ("cpu.max=%s" % want_cpu) not in got or \
       ("memory.max=%s" % want_ram) not in got:
        sys.exit("hog's cgroup does not hold the cap the server said ok to")
    story()
    story("  cpu.max is <quota> <period> in microseconds: %s of every"
          % ("%d000" % expect_cpu if expect_cpu else "all"))
    story("  100000, that is %s%% of one core. memory.max is bytes."
          % (expect_cpu if expect_cpu else "100"))

# ACT 3

def cpu_measure(seconds=5):
    """A busy loop in each container, and the cpu each cgroup counted.

    cpu.stat holds usage_usec, the cpu time the whole container has had.
    Two readings %d seconds apart, divided, is the share it is getting.
    """
    for name in CNT:
        ssh(name, "sh -c 'while :; do :; done' >/dev/null 2>&1 & "
                  "echo $! > /tmp/burn.pid")
    story("      one busy loop running in each, measuring over %d seconds"
          % seconds)
    before = {}
    for name in CNT:
        before[name] = usage_usec(name)
    time.sleep(seconds)
    story()
    for name in CNT:
        pct = (usage_usec(name) - before[name]) / (seconds * 10000.0)
        story("      %-5s used %5.1f%% of one core" % (name, pct))
        if name == HOG and pct > CPU_CAP * 1.5:
            sys.exit("hog got %.0f%%, the %d%% cap is not holding"
                     % (pct, CPU_CAP))
    for name in CNT:
        ssh(name, "kill $(cat /tmp/burn.pid)", check=False)
    story()
    story("  Both loops want a whole core. hog is held at its %d%%."
          % CPU_CAP)

def usage_usec(node):
    out = ssh(node, "grep usage_usec /sys/fs/cgroup/cpu.stat").stdout
    return int(out.split()[1])

# ACT 4

def ram_measure():
    """perl is in every debian image: it allocates RAM_TRY megabytes.

    Past memory.max the kernel kills the process, which comes back to us
    as a failed ssh with nothing printed. Under the cap it prints.
    """
    for name in CNT:
        r = ssh(name, "perl -e '$x = q(x) x (%d*1024*1024); "
                      "print qq(allocated %d M\\n)'" % (RAM_TRY, RAM_TRY),
                check=False)
        said = r.stdout.strip() or "killed (exit %d): the %d M cap" \
               % (r.returncode, RAM_CAP)
        story("      %-5s %s" % (name, said))
        if name == HOG and r.returncode == 0:
            sys.exit("hog allocated %d M under a %d M cap" % (RAM_TRY, RAM_CAP))
        if name == FREE and r.returncode:
            sys.exit("free could not allocate %d M: the host is short of ram"
                     % RAM_TRY)
    story()
    story("  Above memory.max the kernel kills the process, not the")
    story("  container: hog is still there and still answers.")

# ACT 6

def dump_pid():
    r = run("cloonix cli %s dump pid" % NET)
    story()
    for line in r.stdout.splitlines():
        story("      " + line)
    story()
    story("  cpu there is the average over the life of the process,")
    story("  rss its resident memory now.")

def main():
    global g_rootfs
    step("checking the server")
    check_net()

    step("looking for the image")
    g_rootfs = choose_image()
    erase_net()
    place()

    story()
    story("      %s ---- %s ---- %s" % (HOG, LAN, FREE))
    story("    %-8s          %-8s" % (ADDR[HOG], ADDR[FREE]))
    story("    cpu=%d ram=%d     no cap" % (CPU_CAP, RAM_CAP))
    story()

    step("ACT 1: the two containers")
    build(g_rootfs)
    if not ping(HOG, ADDR[FREE]):
        sys.exit("hog does not ping free")

    step("ACT 2: the cap on hog, cpu=%d ram=%d" % (CPU_CAP, RAM_CAP))
    cap(CPU_CAP, RAM_CAP)
    show_caps(CPU_CAP, RAM_CAP)

    step("ACT 3: cpu, a busy loop in each")
    cpu_measure()

    step("ACT 4: ram, %d M allocated in each" % RAM_TRY)
    ram_measure()

    step("ACT 5: the cap lifted")
    cap(0, 0)
    show_caps(0, 0)

    step("ACT 6: dump pid")
    dump_pid()

    story()
    story("Still running: %s" % ", ".join(n for n in CNT if in_lst(n)))
    for name in CNT:
        story("    cloonix cli %s del %s" % (NET, name))

if __name__ == "__main__":
    try:
        main()
    except SystemExit as stop:
        if stop.code:
            sys.stderr.write("%s\n" % stop.code)
            up = [n for n in CNT if in_lst(n)]
            if up:
                sys.stderr.write("Still running: %s\n" % ", ".join(up))
            raise SystemExit(1)
        raise
