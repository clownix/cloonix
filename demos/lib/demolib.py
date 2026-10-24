"""demolib - what every demo shares, written once.

A demo keeps what is its own: the docstring, the topology, the acts, every
cloonix command written whole. What is here is the plumbing under them:
run() and ssh() that print a command and give it to the client, the waits,
erase_net() and place(), story() for the Comments panel of the canvas.

A demo finds this file in demos/lib, the one directory of demos/ that is
not an area: no demo list shows it. Names in capitals read through DEMO (WIRING, LAYOUT, ETHS,
ROOTFS_CHOICES, GATEWAY, ALL) are the constants of the demo being run.
Standard library only.
"""
import os
import shlex
import shutil
import subprocess
import sys
import threading
import time
import __main__ as DEMO

sys.stdout.reconfigure(line_buffering=True)
# The words that start the client.
CLOONIX = ([sys.executable, os.environ["CLOONIX_CLIENT"]]
           if os.environ.get("CLOONIX_CLIENT") else ["cloonix"])
ARGS = [a for a in sys.argv[1:] if not a.startswith("--")]
NET = ARGS[0] if ARGS else "nemo"
# What goes where. The console, a terminal or the log of the Demo panel,
# carries the cloonix commands and nothing else. The history of the demo,
# what each step is for and what it proved, belongs to the Comments panel of
# the canvas: the Demo panel asks for it with CLOONIX_DEMO_STORY=1 and takes
# every line that carries the mark. From a terminal it is not printed.
STORY = os.environ.get("CLOONIX_DEMO_STORY") == "1"
T_START = time.time()
HOLD = 0 if os.environ.get("CLOONIX_DEMO_FAST") == "1" else 4
ACK_FAST = 0.3
TRACE = True
TRACE_SSH = False
# one write per line under a lock: in_parallel threads print at once
_OUT = threading.Lock()

# A demo that cannot run here, its image, a module or a privilege missing:
# all.py goes on with the next demo, a failure stops it.
EXIT_UNAVAILABLE = 3

def unavailable(msg):
    """Stop the demo, it cannot run here: msg says why and what to do."""
    sys.stderr.write("%s\n" % msg)
    sys.exit(EXIT_UNAVAILABLE)

def guarded(main, nodes):
    """main(); a failure says why, which of nodes are still up, and exits 1.
    unavailable() and success go through as they are."""
    try:
        main()
    except SystemExit as stop:
        if stop.code in (None, 0, EXIT_UNAVAILABLE):
            raise
        sys.stderr.write("%s\n" % stop.code)
        up = [n for n in nodes if in_lst(n)]
        if up:
            sys.stderr.write("Still running: %s\n" % ", ".join(up))
        raise SystemExit(1)
    except RuntimeError as err:
        sys.stderr.write("%s\n" % err)
        raise SystemExit(1)

def toy_dir():
    """The toy whose client runs this demo (<toy>/bin beside <toy>/rootfs),
    None for a bundle."""
    client = os.environ.get("CLOONIX_CLIENT") or shutil.which("cloonix")
    if not client:
        return None
    top = os.path.dirname(os.path.dirname(os.path.realpath(client)))
    return top if os.path.isdir(os.path.join(top, "rootfs", "cloonix",
                                             "cloonfs")) else None

def needs_subuid():
    """Unavailable in a toy with a single uid mapping: setgroups is denied
    there, a daemon that drops its privileges dies."""
    top = toy_dir()
    if not top:
        return
    try:
        with open(os.path.join(top, "config", "idmap")) as f:
            if f.read().startswith("subuid"):
                return
    except OSError:
        pass
    unavailable("not possible in this toy: it has a single uid mapping, and "
                "the daemon of this demo drops its privileges.\n"
                "newuidmap and newgidmap on the host (package uidmap or "
                "shadow-utils), then ./setup.sh again, give it a second one.")

def shared_dir(name):
    """(host path, the path the server names) of a directory both touch: a
    toy sees nothing of the host but its bulk, as /root/bulk_host."""
    if toy_dir():
        return ("/var/lib/cloonix/bulk/" + name, "/root/bulk_host/" + name)
    return ("/tmp/" + name, "/tmp/" + name)

def story(*args):
    if STORY:
        with _OUT:
            for line in " ".join(str(a) for a in args).split("\n"):
                sys.stdout.write("#story# %s\n" % line)
            sys.stdout.flush()

def hold(seconds=None):
    """A proof stays in the Comments panel a moment; a terminal shows
    commands only and waits for nothing."""
    if not STORY:
        return
    story()
    time.sleep(HOLD if seconds is None else min(seconds, HOLD))
    story()

def step(msg):
    if time.time() - T_START > 1:
        hold()
    story("[%5.1fs] %s" % (time.time() - T_START, msg))

def show(line):
    """Print a cloonix command exactly as it can be typed by hand.

    Every command this demo gives the server is printed here as it runs, so
    the demo is also a transcript: any line below can be pasted into a
    terminal as it stands. A demo is read at least as often as it is run,
    and the commands are the thing being read.
    """
    with _OUT:
        sys.stdout.write(line + "\n")
        sys.stdout.flush()

def run(line, quiet=False):
    """Give the server the command written in the line.

    The line begins with the word cloonix and is split as a shell would
    split it, so what is printed and what is run cannot drift apart. Only
    that first word is replaced, by whatever starts the client here.
    """
    if not quiet:
        show(line)
    return subprocess.run(CLOONIX + shlex.split(line)[1:],
                          capture_output=True, text=True,
                          stdin=subprocess.DEVNULL)

def ssh(node, cmd, check=True):
    """One shell command inside a guest.

    Its own function rather than a run() line because the command is a
    single argument full of spaces and quotes: quoting it into a line and
    splitting it back would corrupt it. So the line is printed for reading
    and the argument vector is built straight from cmd.
    """
    show('cloonix cli %s ssh %s "%s"' % (NET, node, cmd))
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node, cmd],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if check and r.returncode:
        raise RuntimeError("%s: %s -> %s"
                           % (node, cmd,
                              (r.stderr or r.stdout).strip() or "no output"))
    return r.stdout

def put(node, path, text):
    """Write a file into a guest with printf.

    printf reads its first argument as a FORMAT: a % is a conversion, a
    quote ends the shell argument, and a backslash is an escape printf acts
    on. So backslashes are doubled FIRST and only then are real newlines
    turned into the escape printf turns back.
    """
    if "'" in text or "%" in text:
        raise RuntimeError("%s cannot be written with printf as it stands"
                           % path)
    escaped = text.replace("\\", "\\\\").replace("\n", "\\n")
    ssh(node, "printf '%s' > %s" % (escaped, path))

def in_lst(name):
    """Is this name in what the server lists? By hand:

      cloonix cli <net> dump commands
    """
    return name in run("cloonix cli %s dump commands" % NET,
                       quiet=True).stdout.split()

def answers(node, timeout=3):
    """Is this guest answering yet? By hand:

      cloonix --wait 3 cli <net> ssh <node> "echo yes"
    """
    r = run('cloonix --wait %s cli %s ssh %s "echo yes"' % (timeout, NET, node),
            quiet=True)
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
        time.sleep(1)
    raise RuntimeError("%s never answered ssh" % node)

def in_parallel(fn, items, what):
    results = {}

    def one(item):
        try:
            results[item] = fn(item)
        except Exception as err:                        # noqa: BLE001
            results[item] = err
    threads = [threading.Thread(target=one, args=(i,)) for i in items]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    bad = [i for i in items if isinstance(results[i], Exception)]
    if bad:
        raise RuntimeError("%s failed for %s: %s"
                           % (what, ", ".join(str(i) for i in bad),
                              results[bad[0]]))
    return results

def check_net():
    r = run("cloonix cli %s dump commands" % NET)
    if r.returncode:
        unavailable("no answer from a cloonix net called '%s'.\n"
                 "  the client said: %s\n"
                 "  start it:  cloonix start %s"
                 % (NET, (r.stderr + r.stdout).strip() or "nothing", NET))

def choose_rootfs():
    said = ""
    for cand in DEMO.ROOTFS_CHOICES:
        r = run("cloonix cli %s add cnt probe_img eth=1 %s" % (NET, cand))
        ok = (r.returncode == 0) and wait_exists("probe_img", 20)
        run("cloonix --wait 0.3 cli %s del probe_img" % NET)
        if ok:
            return cand
        said = (r.stdout + r.stderr).strip() or "nothing"
    first = DEMO.ROOTFS_CHOICES[0]
    unavailable("the server would not start a container on %s.\n"
             "  it said: %s\n"
             "  fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_demo_bulk/%s.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm_demo && ./cnt_%s.sh"
             % (" or ".join(DEMO.ROOTFS_CHOICES), said, first, first))

def erase_net(net=None):
    """Erase every object of the net: a demo starts on an empty canvas.

    del all is answered once the server has removed everything; the topology
    is then read until it says so, a container taking a moment to unmount
    after its delete.
    """
    _CAPTURABLE.clear()
    net = net or NET
    def empty():
        r = subprocess.run(CLOONIX + ["cli", net, "dump", "topo"],
                           capture_output=True, text=True,
                           stdin=subprocess.DEVNULL)
        return r.returncode == 0 and "(no objects)" in r.stdout
    if empty():
        return
    show("cloonix cli %s del all" % net)
    subprocess.run(CLOONIX + ["cli", net, "del", "all"], capture_output=True,
                   text=True, stdin=subprocess.DEVNULL)
    end = time.time() + 60
    while time.time() < end:
        if empty():
            time.sleep(1)
            return
        time.sleep(1)
    sys.exit("could not erase the net %s.\n"
             "  clear it by hand:  cloonix cli %s del all" % (net, net))

def eth_spec(node):
    """One "s" per interface, all sniffable. Taken from WIRING so an
    interface added there cannot be forgotten here."""
    return "eth=%d" % len(DEMO.WIRING[node])

def create(name, rootfs):
    last = ""
    for _ in range(6):
        r = run("cloonix cli %s add cnt %s %s %s"
                % (NET, name, eth_spec(name), rootfs))
        last = (r.stdout + r.stderr).strip()
        if r.returncode == 0 and wait_exists(name):
            return
        if r.returncode == 0:
            last = "the add was taken and %s never appeared" % name
        time.sleep(2)
    raise RuntimeError("%s was never created. The server said: %s"
                       % (name, last or "nothing at all"))

def address(node):
    for num, _lan, addr in DEMO.WIRING[node]:
        if addr:
            ssh(node, "ip addr add %s dev eth%d" % (addr, num))
        ssh(node, "ip link set dev eth%d up" % num)
    if node in DEMO.GATEWAY and DEMO.GATEWAY[node]:
        ssh(node, "ip route add default via %s" % DEMO.GATEWAY[node])

def tools_present(node, *tools):
    for t in tools:
        r = run('cloonix cli %s ssh %s "command -v %s"' % (NET, node, t))
        if r.returncode:
            first = DEMO.ROOTFS_CHOICES[0]
            unavailable("%s has no %s: this is not the %s image.\n"
                     "  fetch it:\n"
                     "    cd /var/lib/cloonix/bulk\n"
                     "    wget https://clownix.net/downloads/cnt_demo_bulk/"
                     "%s.tar.gz\n"
                     "    (cloonix extracts it at the first use)\n"
                     "  or build it:\n"
                     "    cd vm_create/cnt_kvm_demo && ./cnt_%s.sh"
                     % (node, t, first, first, first))

def cable_all():
    """A refused plug stops the demo there, with the reason of the server:
    a machine off its lan fails every check minutes later."""
    for node in DEMO.ALL:
        for num, lan, _ in DEMO.WIRING[node]:
            r = run("cloonix cli %s plug %s eth%d %s" % (NET, node, num, lan))
            if r.returncode:
                sys.exit("plug %s eth%d %s refused, the machines cannot be "
                         "cabled. The server said: %s"
                         % (node, num, lan,
                            (r.stdout + r.stderr).strip() or "nothing"))

def build(rootfs):
    """The seven steps every demo shares: check, image, erase, place,
    create, wait, cable, address. What differs between demos is what they
    do AFTER their machines are addressed."""
    in_parallel(lambda n: create(n, rootfs), list(DEMO.ALL), "add cnt")
    for name in DEMO.ALL:
        wait_answer(name)
    cable_all()
    in_parallel(address, list(DEMO.ALL), "address")

def parse_bytes_line(line):
    f = line.split()
    if len(f) < 5:
        return None
    try:
        return {"nb": int(f[1]), "when": float(f[2]),
                "data": bytes(int(x, 16) for x in f[4:])}
    except ValueError:
        return None

_CAPTURABLE = set()

def capturable(node, num):
    """An eth is born not capturable: cnf <node> eth<num> sniff=on, once."""
    if (node, num) not in _CAPTURABLE:
        cli("cnf", node, "eth%d" % num, "sniff=on")
        _CAPTURABLE.add((node, num))

class Tap:
    """One subscription with one filter, counting what got through it."""

    def __init__(self, node, num, filt=""):
        capturable(node, num)
        self.node, self.num, self.filt = node, num, filt
        self.live = False
        self.lock = threading.Lock()
        self.count = 0
        line = ("cloonix cli %s sniff %s eth%d --bytes %s"
                % (NET, node, num, filt)).rstrip()
        show(line)
        self.proc = subprocess.Popen(CLOONIX + shlex.split(line)[1:],
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, text=True,
                                     bufsize=1)
        self.ready_evt = threading.Event()
        threading.Thread(target=self._drain, daemon=True).start()

    def _drain(self):
        for line in self.proc.stdout:
            line = line.rstrip("\n")
            if line.startswith("# waiting"):
                self.live = True
                self.ready_evt.set()
                continue
            if not line.startswith("pkt "):
                continue
            if parse_bytes_line(line):
                with self.lock:
                    self.count += 1
        self.ready_evt.set()

    def ready(self, timeout=15):
        self.ready_evt.wait(timeout)
        if not self.live:
            err = self.proc.stderr.read() if self.proc.stderr else ""
            story("      could not sniff %s eth%d with %r: %s"
                  % (self.node, self.num, self.filt,
                     err.strip() or "no answer"))
        return self.live

    def clear(self):
        with self.lock:
            self.count = 0

    def stop(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()

def count_on(node, num, filt, do_it, settle=2.0):
    """How many packets matching filt cross one interface while do_it runs."""
    tap = Tap(node, num, filt)
    if not tap.ready():
        tap.stop()
        return None
    time.sleep(1.0)
    tap.clear()
    out = do_it()
    time.sleep(settle)
    with tap.lock:
        n = tap.count
    tap.stop()
    return {"out": out, "count": n}

def module_on_host(node, mod):
    """Is a kernel module loaded on the host, seen from inside a container.

    A container run by an ordinary user cannot load a module -- it is not
    root in the initial namespace -- but it uses one already loaded on the
    host, which shows up as /sys/module/<name>. The name there has
    underscores where modprobe takes either, so both are tried. This is the
    same rule as kvm: the host loads it, cloonix uses it.
    """
    for name in dict.fromkeys((mod, mod.replace("-", "_"))):
        if ssh(node, "test -d /sys/module/%s && echo yes" % name,
               check=False).strip() == "yes":
            return True
    return False

def need_host_modules(node, mods):
    """Stop the demo, unavailable here, when a module it needs is not loaded
    on the host: a container cannot load one. demo_test.py reads HOST_MODULES
    and skips."""
    missing = [m for m in mods if not module_on_host(node, m)]
    if missing:
        unavailable(modules_warning(missing))

def modules_warning(missing):
    """What to do about kernel modules not loaded on the host."""
    bar = "!" * 72
    return ("%s\nWARNING: the kernel module(s) %s, not loaded on the host. A "
            "container cannot\nload a module; on the host, once per boot:\n"
            "%s\nthen play the demo again.\n%s"
            % (bar, ", ".join(missing),
               "\n".join("sudo modprobe %s" % m for m in missing), bar))

FRRINIT = "/usr/lib/frr/frrinit.sh"

def frr_setup(node, daemons, conf, modules=()):
    """Write /etc/frr of one node, start frr, wait for the last daemon named.
    The daemons stay root, a container having no frr user of the host map.
    A module <daemon>_<name> (pathd_pcep) is loaded by its daemon."""
    names = ["zebra"] + [d for d in daemons if d != "zebra"]
    lines = ["%s=yes" % d for d in names] + ["vtysh_enable=yes"]
    lines += ['%s_options="-A 127.0.0.1 -u root -g root%s"'
              % (d, "".join(" -M %s" % m for m in modules
                            if m.split("_")[0] == d))
              for d in names + ["mgmtd", "staticd"]]
    ssh(node, "mkdir -p /etc/frr /var/log/frr /var/lib/frr /var/tmp/frr")
    # root needs no password: vtysh then skips PAM, which refuses it in an
    # interactive shell of the container.
    ssh(node, "printf 'service integrated-vtysh-config\\n"
              "username root nopassword\\n' > /etc/frr/vtysh.conf")
    ssh(node, "printf '%s\\n' > /etc/frr/daemons" % "\\n".join(lines))
    ssh(node, "printf '%s\\n' > /etc/frr/frr.conf" % conf.replace("\n", "\\n"))
    ssh(node, "grep -q '^frrvty:.*root' /etc/group || "
              "sed -i '/^frrvty:/ s/$/,root/' /etc/group; true")
    ssh(node, "chmod 640 /etc/frr/frr.conf /etc/frr/daemons /etc/frr/vtysh.conf")
    ssh(node, "rm -rf /run/frr; mkdir -p /run/frr; chmod 755 /run/frr")
    ssh(node, "%s start </dev/null >/dev/null 2>&1" % FRRINIT)
    ssh(node, "for i in $(seq 1 20); do pgrep -x %s >/dev/null && exit 0; "
              "sleep 1; done; echo %s did not start, see /var/log/syslog; exit 1"
              % (names[-1], names[-1]))
    # frr.conf applied and checked: under load the daemons can come up after
    # frrinit has run vtysh -b, and watchfrr restarts them empty.
    marker = next((l.strip() for l in conf.split("\n")
                   if l.startswith("router ")), None)
    if marker:
        ssh(node, "for i in $(seq 1 30); do vtysh -b >/dev/null 2>&1; "
                  "vtysh -c 'show running-config' 2>/dev/null | "
                  "grep -qF '%s' && exit 0; sleep 1; done; "
                  "echo 'frr.conf not applied, see /var/log/syslog'; exit 1"
                  % marker)

class Grab(Tap):
    """A Tap that also keeps the bytes, to look for a string in them."""

    def __init__(self, node, num, filt=""):
        self.data = b""
        Tap.__init__(self, node, num, filt)

    def _drain(self):
        for line in self.proc.stdout:
            line = line.rstrip("\n")
            if line.startswith("# waiting"):
                self.live = True
                self.ready_evt.set()
                continue
            pkt = parse_bytes_line(line) if line.startswith("pkt ") else None
            if pkt:
                with self.lock:
                    self.count += 1
                    self.data += pkt["data"]
        self.ready_evt.set()

def grab_on(node, num, filt, do_it, settle=2.0):
    """count_on, with the bytes that crossed."""
    tap = Grab(node, num, filt)
    if not tap.ready():
        tap.stop()
        return None
    time.sleep(1.0)
    out = do_it()
    time.sleep(settle)
    with tap.lock:
        got = {"out": out, "count": tap.count, "data": tap.data}
    tap.stop()
    return got

def reached(out):
    # A ping that lost nothing. The leading space matters: it stops the
    # "0% packet loss" hidden inside "100% packet loss" from matching.
    return " 0% packet loss" in out

def cut_off(out):
    # A ping that got nothing back -- a broken path.
    return "100% packet loss" in out

def place():
    for name, (x, y) in DEMO.LAYOUT.items():
        run("cloonix --wait 0.3 cli %s cnf %s x=%s y=%s"
            % (NET, name, x, y))
    for (name, num), deg in DEMO.ETHS.items():
        run("cloonix --wait 0.3 cli %s cnf %s %s angle=%s"
            % (NET, name, num, deg))

def trace(fmt, *a):
    if TRACE:
        story("       . " + (fmt % a if a else fmt))

def trace_ssh(fmt, *a):
    if TRACE_SSH:
        story("       . " + (fmt % a if a else fmt))

def cli(*args, **kw):
    ack = kw.get("ack")
    cmd = list(CLOONIX)
    if ack is not None:
        cmd += ["--wait", str(ack)]
    a = [str(x) for x in args]
    cmd += ["cli", NET, a[0]] + a[1:]
    if not kw.get("quiet"):
        show("cloonix %scli %s %s"
             % ("" if ack is None else "--wait %s " % ack,
                NET, " ".join(a)))
    return subprocess.run(cmd, capture_output=True, text=True)

def create_cnt(node, rootfs):
    """Create one container with the full ack, so a refusal is seen.

    Retried: a name of the previous run may still be in teardown.
    """
    last = ""
    for attempt in range(6):
        r = cli("add", "cnt", node, eth_spec(node), rootfs)
        if r.returncode == 0:
            if attempt:
                trace("%s created on attempt %d", node, attempt + 1)
            return
        last = (r.stdout + r.stderr).strip()
        time.sleep(2)
    raise RuntimeError("%s was never created. The server said: %s"
                       % (node, last or "nothing at all"))

def net_answers():
    """(True, "") when the net answers, else (False, what the client said)."""
    r = cli("dump", "commands")
    return r.returncode == 0, (r.stderr + r.stdout).strip()

def wait_ready(node, timeout=90):
    end = time.time() + timeout
    while time.time() < end:
        r = subprocess.run(CLOONIX + ["--wait", "3", "cli", NET, "ssh", node,
                            "echo ready"],
                           capture_output=True, text=True,
                           stdin=subprocess.DEVNULL)
        if r.returncode == 0:
            trace("%s answers after %.1fs", node,
                  timeout - (end - time.time()))
            return
        time.sleep(1)
    raise RuntimeError("%s never answered" % node)
