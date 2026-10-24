#!/usr/bin/env python3
"""systemd - a container with an init beside two without, and what it costs.

    ./systemd.py [net]                  default net: nemo

A cloonix container runs no init unless it asks for one::

      sysd1 ---- lan1 ---- plain1
    10.0.0.1             10.0.0.2
        |
       lan1 ---- plain2
               10.0.0.3

sysd1 is created on the image trixie_systemd with
startup_env=CLOONIX_WITH_SYSTEMD=1, the other two with nothing extra: no
systemd is the default, and no flag says so.

  ACT 1  the three containers, the lan, the addresses
  ACT 2  the ping, which works for all three
  ACT 3  the difference, read from inside: pid 1, systemctl, process count
  ACT 4  the host side: the inotify instances and the limit they count against

Image: trixie_systemd for the one with init, trixie for the others
Size:  3 containers, 1 lan

WHY NO INIT BY DEFAULT
----------------------
Each systemd takes several inotify instances, counted by the kernel per
user across every container of the machine. With the debian default of
128, near the twenty-fourth container inotify_init returns EMFILE, systemd
exits, pid 1 with it, and the container disappears having logged nothing: a
soak run measured 66 of 1324 containers dying that way, between 0.2 and 4.8
seconds old, the crun scope reporting success. A guest that routes packets
has no use for an init.

An init is for a demonstration whose subject is a service unit: started by
systemd, ordered against another unit, socket-activated or restarted by it.
For many of them, raise the limit on the host first, and make it permanent
in /etc/sysctl.d/:

    sudo sysctl -w fs.inotify.max_user_instances=1024

Important
  trixie_systemd is a build of its own, every other cnt image being built
  without systemd; the demo stops and says so when it is absent:

    cd vm_create/cnt_kvm && ./cnt_trixie_systemd.sh
"""
import os
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
    ACK_FAST, CLOONIX, NET, cli, erase_net, ssh, step, story, unavailable,
    guarded)

SYSD_ROOTFS = "trixie_systemd"
PLAIN_ROOTFS_CHOICES = ("trixie",)
WITH_SYSTEMD = "startup_env=CLOONIX_WITH_SYSTEMD=1"

SYSD = "sysd1"
PLAIN = ("plain1", "plain2")
ALL = (SYSD,) + PLAIN
LAN = "lan1"
ADDR = {SYSD: "10.0.0.1", "plain1": "10.0.0.2", "plain2": "10.0.0.3"}

def in_lst(name):
    return name in cli("dump", "commands", quiet=True).stdout.split()

def answers(node, timeout=3):
    r = subprocess.run(CLOONIX + ["--wait", str(timeout), "cli", NET, "ssh",
                        node, "echo yes"],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    return "yes" in r.stdout

def wait_exists(name, timeout=20):
    """An add that returns zero is not a container that exists.

    The client waits two seconds for a refusal and reads silence as
    success, so an add the server accepts and then cannot carry out comes
    back as a success. What the server lists is the only answer."""
    end = time.time() + timeout
    while time.time() < end:
        if in_lst(name):
            return True
        time.sleep(1)
    return False

def check_net():
    if cli("dump", "commands").returncode:
        unavailable("no answer from %s\n"
                 "  start it:  cloonix start %s" % (NET, NET))

def probe_image(cand, extra=()):
    """Make one throwaway container on that image and see if it appears.

    One name per image: a del answers before the container is gone,
    and a second add under the same name a moment later is refused as
    already existing, which read as the image being absent.
    """
    name = "probe_" + cand
    r = cli("add", "cnt", name, "eth=1", cand, *extra)
    ok = (r.returncode == 0) and wait_exists(name, 20)
    cli("del", name, ack=ACK_FAST)
    if ok:
        return None
    if r.returncode:
        return (r.stdout + r.stderr).strip() or "nothing"
    return "the add was taken without a word and nothing appeared"

def choose_images():
    said = probe_image(SYSD_ROOTFS, (WITH_SYSTEMD,))
    if said:
        unavailable(
            "this demo needs the %s image and the server would not start\n"
            "  a container on it. It said: %s\n"
            "\n"
            "  That image is a SEPARATE build: every other cnt image is\n"
            "  built without systemd, so the plain trixie cannot show this.\n"
            "  fetch it:\n"
            "    cd /var/lib/cloonix/bulk\n"
            "    wget https://clownix.net/downloads/cnt_bulk/"
            "trixie_systemd.tar.gz\n"
            "    (cloonix extracts it at the first use)\n"
            "  or build it, it is the one container of cnt_kvm/doitall.sh\n"
            "  with no kvm beside it:\n"
            "    cd vm_create/cnt_kvm && ./doitall.sh --cnt trixie_systemd\n"
            "\n"
            "  Making three ordinary containers instead would demonstrate\n"
            "  nothing, so this stops here." % (SYSD_ROOTFS, said))
    for cand in PLAIN_ROOTFS_CHOICES:
        said = probe_image(cand)
        if said is None:
            return cand
    unavailable("the server would not start a plain container on %s.\n"
             "  it said: %s\n"
             "  fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_bulk/"
             "trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm && ./cnt_trixie.sh"
             % (" or ".join(PLAIN_ROOTFS_CHOICES), said))

# ACT 1

def create(name, rootfs, extra=()):
    last = ""
    for _ in range(6):
        r = cli("add", "cnt", name, "eth=1", rootfs, *extra)
        last = (r.stdout + r.stderr).strip()
        if r.returncode == 0 and wait_exists(name):
            return
        if r.returncode == 0:
            last = "the add was taken and %s never appeared" % name
        time.sleep(2)
    sys.exit("%s was never created.\n  the server said: %s\n"
             "  by hand:  cloonix cli %s add cnt %s eth=1 %s %s"
             % (name, last or "nothing", NET, name, rootfs, " ".join(extra)))

def build():
    # The one with an init is the ONLY one given anything.
    create(SYSD, SYSD_ROOTFS, (WITH_SYSTEMD,))
    for name in PLAIN:
        create(name, g_plain)
    for name in ALL:
        cli("plug", name, "eth0", LAN)
    for name in ALL:
        end = time.time() + 90
        while time.time() < end and not answers(name):
            time.sleep(2)
        if not answers(name):
            sys.exit("%s is in the list and never answered ssh.\n"
                     "  A container that runs systemd and dies at once is\n"
                     "  the inotify limit -- see ACT 4 of this demo and\n"
                     "  fs.inotify.max_user_instances on the host." % name)
    for name in ALL:
        ssh(name, "ip addr add %s/24 dev eth0" % ADDR[name])
        ssh(name, "ip link set dev eth0 up")

# ACT 3 -- the difference, from inside

def pid_one(node):
    out = ssh(node, "cat /proc/1/comm", check=False).strip()
    return out or "unreadable"

def has_systemctl(node):
    out = ssh(node, "systemctl is-system-running 2>&1 || true",
              check=False).strip()
    if not out:
        return "no answer"
    return out.splitlines()[0][:40]

def nb_procs(node):
    out = ssh(node, "ls -d /proc/[0-9]* 2>/dev/null | wc -l",
              check=False).strip()
    return out or "?"

def show_difference():
    story("  %-8s %-14s %-24s %s" % ("node", "pid 1", "systemctl", "procs"))
    for name in ALL:
        story("  %-8s %-14s %-24s %s"
              % (name, pid_one(name), has_systemctl(name), nb_procs(name)))
    for name in ALL:
        why = ssh(name, "cat /run/cloonix_no_systemd 2>/dev/null; true",
                  check=False).strip()
        if why:
            story("  %s: %s" % (name, why))
    story()
    story("  the same thing seen with ps, process 1 of each container:")
    for name in ALL:
        # ps if the image has it, /proc for the ones that do not
        out = ssh(name, "ps -o pid,user,comm,args -p 1 2>/dev/null || "
                        "(echo 'PID COMM'; echo \"1 $(cat /proc/1/comm)\")",
                  check=False)
        for line in out.splitlines():
            story("      " + line)
        story()

# ACT 4 -- the host side

def inotify_picture():
    """What this user holds against the per user limit.

    Counted on the HOST and not in a container on purpose: the limit is
    per uid across every container, which is exactly why one container
    cannot see the problem it is part of.
    """
    try:
        with open("/proc/sys/fs/inotify/max_user_instances") as f:
            limit = f.read().strip()
    except OSError as err:
        limit = "unreadable (%s)" % err
    used = "?"
    try:
        out = subprocess.run(["sh", "-c",
                              "find /proc/*/fd -lname anon_inode:inotify "
                              "2>/dev/null | wc -l"],
                             capture_output=True, text=True).stdout.strip()
        used = out or "?"
    except OSError:
        pass
    story("  fs.inotify.max_user_instances = %s" % limit)
    story("  inotify instances open right now (all visible pids) = %s" % used)
    story()
    story("  The limit is counted PER USER, over every container together.")
    story("  Past it, inotify_init gives EMFILE, systemd exits, pid 1 goes")
    story("  with it, and the container disappears having logged nothing.")
    story("  To carry many of them:")
    story("    sudo sysctl -w fs.inotify.max_user_instances=1024")

def ping(node, target, count=5):
    return subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                           "ping -c %d -W 2 %s" % (count, target)],
                          capture_output=True, text=True,
                          stdin=subprocess.DEVNULL).returncode == 0

LAYOUT = {
    SYSD: (-190, 0),
    LAN: (0, 0),
    "plain1": (190, -75),
    "plain2": (190, 75),
}

ETHS = {
    ("plain1", "eth0"): 248,
    ("plain2", "eth0"): 292,
    (SYSD, "eth0"): 90,
}

def place():
    for name, (x, y) in LAYOUT.items():
        cli("cnf", name, "x=%s" % x, "y=%s" % y, ack=ACK_FAST)
    for (name, num), deg in ETHS.items():
        cli("cnf", name, num, "angle=%s" % deg, ack=ACK_FAST)

def main():
    global g_plain
    step("checking the server")
    check_net()

    step("looking for the images")
    g_plain = choose_images()
    erase_net()
    place()

    story()
    story("      %s ---- %s ---- %s   (%s, WITH systemd)"
          % (SYSD, LAN, PLAIN[0], SYSD_ROOTFS))
    story("    %-8s             %-8s" % (ADDR[SYSD], ADDR[PLAIN[0]]))
    story("        |")
    story("       %s ---- %s   (%s, the default: no init)"
          % (LAN, PLAIN[1], g_plain))
    story("               %s" % ADDR[PLAIN[1]])
    story()

    step("ACT 1: the containers")
    build()
    story("  %s created with %s" % (SYSD, WITH_SYSTEMD))
    story("  %s and %s created with nothing extra -- that is the default"
          % PLAIN)

    step("ACT 2: the ping")
    ok = True
    for a, b in ((SYSD, PLAIN[0]), (PLAIN[0], PLAIN[1]), (PLAIN[1], SYSD)):
        good = ping(a, ADDR[b])
        ok = ok and good
        story("  %-8s -> %-8s %s" % (a, b, "ok" if good else "NO REPLY"))
    if ok:
        story("  An init changes what runs inside, not whether it is on the")
        story("  lan. Both kinds are ordinary cloonix containers.")

    step("ACT 3: the difference, from inside each one")
    show_difference()

    step("ACT 4: what it costs on the host")
    inotify_picture()

    story()
    story("Still running: %s" % ", ".join(n for n in ALL if in_lst(n)))
    for name in ALL:
        story("    cloonix cli %s del %s" % (NET, name))

if __name__ == "__main__":
    guarded(main, ALL)
