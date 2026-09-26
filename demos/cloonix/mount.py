#!/usr/bin/env python3
"""mount - a host directory inside two containers, and a variable for each.

    ./mount.py [net]                    default net: nemo

Two options of add cnt that nothing else in demos/ uses::

      cnt1 ---- lan1 ---- cnt2
    10.0.0.1          10.0.0.2
        \\                /
         \\   host dir   /
          /tmp/cloonix_mount_<net>
            seen as /shared in both
            conf/ seen as /etc/demo in cnt1 only

  vmount="<host dir>:<guest path> [<host dir>:<guest path> ...]"
      the host directory IS the guest directory, docker's -v. What one
      container writes there the other reads, the host too, and it stays
      when the container is deleted. Several mounts go in ONE vmount=,
      separated by a space, so the value is quoted.

  startup_env="VAR=value [VAR=value ...]"
      environment of pid 1 of the container, docker's -e. The same image
      started twice with a different value plays two roles.

  ACT 1  the host directory and the two containers, each with both options
  ACT 2  the variable, read from pid 1 of each container
  ACT 3  the shared directory: cnt1 writes, cnt2 reads, the host lists
  ACT 4  the host configures cnt1 through conf/, without touching the image
  ACT 5  what survives a delete: the mount does, the overlay does not

Standard library only, like cloonix itself.

Image: trixie
Size:  2 containers, 1 lan

Important
  The host directory is made by this program ON THE MACHINE THAT RUNS
  THE SERVER: a mount takes its source from the server's file system,
  not from the client's. Run this demo where cloonix start <net> runs.
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
    CLOONIX, NET, erase_net, show, ssh, step, story)

CNT_ROOTFS_CHOICES = ("trixie",)

HOST_DIR = "/tmp/cloonix_mount_%s" % NET
CONF_DIR = HOST_DIR + "/conf"

CNT = ("cnt1", "cnt2")
LAN = "lan1"
ADDR = {"cnt1": "10.0.0.1", "cnt2": "10.0.0.2"}

# What each container is given: the same image, a different role.
ENV = {"cnt1": "NODE_ID=cnt1 ROLE=writer",
       "cnt2": "NODE_ID=cnt2 ROLE=reader"}
VMOUNT = {"cnt1": "%s:/shared %s:/etc/demo" % (HOST_DIR, CONF_DIR),
          "cnt2": "%s:/shared" % HOST_DIR}

def run(line, ack=None):
    """Show a cloonix command line, then give it to the client."""
    show(line)
    words = shlex.split(line)[1:]
    if ack is not None:
        words = ["--wait", str(ack)] + words
    return subprocess.run(CLOONIX + words, capture_output=True, text=True,
                          stdin=subprocess.DEVNULL)

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

def wait_gone(name, timeout=30):
    end = time.time() + timeout
    while time.time() < end:
        if not in_lst(name):
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
    for line in ("cnf cnt1 x=-250 y=0", "cnf %s x=0 y=0" % LAN,
                 "cnf cnt2 x=250 y=0",
                 "cnf cnt1 eth0 angle=90", "cnf cnt2 eth0 angle=270"):
        run("cloonix cli %s %s" % (NET, line), ack=0.3)

# ACT 1

def host_dir():
    """The directory the two containers will share, on the server's disk.

    An earlier run may have left files: they are the point of ACT 5, so
    the directory is emptied here and not deleted, to show the delete
    later touches none of it.
    """
    os.makedirs(CONF_DIR, exist_ok=True)
    for d in (CONF_DIR, HOST_DIR):
        for f in os.listdir(d):
            p = os.path.join(d, f)
            if os.path.isfile(p):
                os.unlink(p)
    story("  host directory %s made, empty" % HOST_DIR)

def create(name, rootfs):
    line = ('cloonix cli %s add cnt %s eth=1 %s startup_env="%s" '
            'vmount="%s"' % (NET, name, rootfs, ENV[name], VMOUNT[name]))
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

# ACT 2

def show_env():
    """The variables as pid 1 of each container holds them.

    /proc/1/environ is the certain place: it is what crun gave the init.
    cloonix also writes the same string in /cloonixmnt/cnf_fs/startup_env.
    """
    for name in CNT:
        out = ssh(name, "tr '\\0' '\\n' < /proc/1/environ | grep -E "
                        "'NODE_ID|ROLE'")
        story()
        for line in out.splitlines():
            story("      %s says: %s" % (name, line.strip()))
        if ("NODE_ID=%s" % name) not in out:
            sys.exit("%s: NODE_ID is not in the environment of its pid 1"
                     % name)
    story()
    story("  One image, two roles: the value came from the add cnt line.")

# ACT 3

def share():
    ssh("cnt1", "echo written by $(cat /proc/1/comm) of cnt1 "
                "> /shared/hello.txt")
    out = ssh("cnt2", "cat /shared/hello.txt")
    story()
    story("      cnt2 reads: %s" % out.strip())
    if "cnt1" not in out:
        sys.exit("cnt2 does not see what cnt1 wrote in /shared")
    story("      the host sees: %s" % ", ".join(sorted(os.listdir(HOST_DIR))))
    with open(os.path.join(HOST_DIR, "hello.txt")) as f:
        story("      the host reads: %s" % f.read().strip())
    story()
    story("  One directory, three views: the mount is the directory itself,")
    story("  not a copy of it.")

# ACT 4

def configure_from_host():
    """A settings file written by the host, read by the container.

    The second mount of cnt1 puts conf/ at /etc/demo: the host is the
    one that writes there, the container only reads. Nothing in the
    image changed, and the same image with another conf/ is another
    configuration.
    """
    with open(os.path.join(CONF_DIR, "demo.conf"), "w") as f:
        f.write("greeting=hello from the host\n")
    story("  the host wrote %s/demo.conf" % CONF_DIR)
    out = ssh("cnt1", "cat /etc/demo/demo.conf")
    story()
    story("      cnt1 reads /etc/demo/demo.conf: %s" % out.strip())
    if "hello from the host" not in out:
        sys.exit("cnt1 does not see the file the host wrote in conf/")
    out = ssh("cnt2", "ls /etc/demo 2>&1 || true")
    story("      cnt2 has no /etc/demo: %s" % out.strip().splitlines()[0])
    story()
    story("  Two mounts in one vmount=, one for data, one for settings.")

# ACT 5

def survives():
    """Delete cnt1: the file in the mount stays, the file elsewhere goes."""
    ssh("cnt1", "echo gone with the container > /root/gone.txt")
    ssh("cnt1", "echo still here > /shared/still.txt")
    run("cloonix cli %s del cnt1" % NET, ack=0.3)
    if not wait_gone("cnt1"):
        sys.exit("cnt1 was not deleted")
    story()
    story("      cnt1 deleted. On the host, %s holds: %s"
          % (HOST_DIR, ", ".join(sorted(os.listdir(HOST_DIR)))))
    if not os.path.exists(os.path.join(HOST_DIR, "still.txt")):
        sys.exit("still.txt did not survive the delete")
    create("cnt1", g_rootfs)
    run("cloonix cli %s plug cnt1 eth0 %s" % (NET, LAN))
    wait_answer("cnt1")
    out = ssh("cnt1", "ls /root/gone.txt 2>&1 || true").strip()
    story("      cnt1 again, /root/gone.txt: %s" % out.splitlines()[0])
    out = ssh("cnt1", "cat /shared/still.txt").strip()
    story("      cnt1 again, /shared/still.txt: %s" % out)
    if os.path.exists(os.path.join(HOST_DIR, "gone.txt")):
        sys.exit("gone.txt reached the host, it should not have")
    story()
    story("  What a container writes outside a mount lives in its overlay")
    story("  and dies with it. A mount is the one place that outlives it.")

def ping(node, target, count=5):
    out = ssh(node, "ping -c %d -W 2 %s" % (count, target), check=False)
    story()
    story(out)
    return " 0% packet loss" in out

def main():
    global g_rootfs
    step("checking the server")
    check_net()

    step("looking for the image")
    g_rootfs = choose_image()
    erase_net()
    place()

    story()
    story("      cnt1 ---- %s ---- cnt2" % LAN)
    story("    %-8s          %-8s" % (ADDR["cnt1"], ADDR["cnt2"]))
    story("        \\                /")
    story("         \\   host dir   /")
    story("          %s" % HOST_DIR)
    story("            seen as /shared in both")
    story("            conf/ seen as /etc/demo in cnt1 only")
    story()

    step("ACT 1: the host directory and the two containers")
    host_dir()
    build(g_rootfs)
    if not ping("cnt1", ADDR["cnt2"]):
        sys.exit("cnt1 does not ping cnt2")
    story("  Both options are on the add cnt lines above: startup_env=")
    story("  for the variables, vmount= for the directories.")

    step("ACT 2: the variable, read from pid 1 of each container")
    show_env()

    step("ACT 3: the shared directory")
    share()

    step("ACT 4: the host configures cnt1 through conf/")
    configure_from_host()

    step("ACT 5: what survives a delete")
    survives()

    story()
    story("Still running: %s" % ", ".join(n for n in CNT if in_lst(n)))
    for name in CNT:
        story("    cloonix cli %s del %s" % (NET, name))
    story("  the host directory stays: rm -rf %s" % HOST_DIR)

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
