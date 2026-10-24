#!/usr/bin/env python3
r"""demo_test - run every demo against a net and say which ones are broken.

    ./demo_test.py [net]                    run them all on that net
    ./demo_test.py nemo mito                two nets, each taking the next demo
    ./demo_test.py nemo --only wg,vlan      run those two
    ./demo_test.py nemo --skip mobile5g     run all but that
    ./demo_test.py --list                   say what would run, and skip why
    ./demo_test.py nemo --repeat 3          three rounds, for a flapping one

    --timeout N     seconds one demo may take          (default 600)
    --logs DIR      where the per-demo output goes     (default /tmp/demo_log)
    --keep          leave the machines up after a failure, to look at them
    --tree DIR      a demos/ somewhere else than the installed one
    --no-syslog     do not read /var/log/syslog

WHAT IT IS FOR

Twenty demos, each a few minutes, each needing an image that may or may not
be built. Running them by hand means an afternoon, and the run after a
change to the client or the switch means another one. This does the
afternoon and prints a table.

WHAT IT ASSERTS, WHICH IS MORE THAN "IT EXITED ZERO"

  before   the image the demo names can carry a container. A demo skipped
           for a missing image is a SKIP and says which script builds it,
           because that is a different fact from a failure; the
           HOST_MODULES of the demos absent from the host are loaded by
           modprobe through passwordless sudo first, and a demo whose
           module stays absent is a SKIP with the modprobe that loads it;
           so is a demo marked NEEDS_ROOT on a net whose server does not
           run as root
  during   the output is captured whole, and the run is killed at the
           timeout rather than left to hang the batch
  after    exit code, and the demo's own last words when it failed -- every
           demo here ends by naming what did not hold, and that line is
           worth more than the code
  after    the net is EMPTY. del all, then a wait until dump commands is clear.
           A demo that leaves a container behind poisons the next one, and
           the failure lands on the innocent demo
  after    the syslog between the start and the end of that demo. Every
           KERR is counted and the distinct ones are printed, whether the
           demo passed or not -- a demo that passes while the switch logs
           errors is the interesting case, and the one nobody would look for

WHAT IT IS NOT

It does not judge whether a demo TAUGHT anything. Each demo checks its own
claims and exits non-zero when they do not hold, and that judgement stays
in the demo where it belongs. This runs them and collects the verdicts.

It is not soak.py either: that hammers the server at random to find faults,
this walks a fixed list to find regressions. The two answer different
questions and both are worth running.

Standard library only.
"""
import os
import re
import shutil
import subprocess
import syslog
import threading
import sys
import time
from testitall import sudo, sudo_ok

HERE = os.path.dirname(os.path.abspath(__file__))

def opt(name, default=None):
    if name in sys.argv:
        i = sys.argv.index(name)
        if i + 1 < len(sys.argv):
            return sys.argv[i + 1]
    return default

FLAGS = {"--list", "--keep", "--no-syslog"}
TAKES = {"--only", "--skip", "--timeout", "--logs", "--tree", "--repeat"}

_free = []
_skip_next = False
for _a in sys.argv[1:]:
    if _skip_next:
        _skip_next = False
        continue
    if _a in TAKES:
        _skip_next = True
        continue
    if _a in FLAGS or _a.startswith("--"):
        continue
    _free.append(_a)

# Several nets run the demos side by side, one worker each; a demo that
# drives other nets itself (ALONE) runs last, on the first net, alone.
NETS = _free if _free else ["nemo"]
NET = NETS[0]
ONLY = [w for w in (opt("--only", "") or "").split(",") if w]
SKIP = [w for w in (opt("--skip", "") or "").split(",") if w]
TIMEOUT = int(opt("--timeout", "600"))
LOGS = opt("--logs", "/tmp/demo_log")
# On a failure, how many lines of the demo's own narration to fold into the
# main output, so the single results file explains it without the side log.
LOG_TAIL = int(os.environ.get("DEMO_LOG_TAIL", "70"))
REPEAT = int(opt("--repeat", "1"))
LIST_ONLY = "--list" in sys.argv
KEEP = "--keep" in sys.argv
NO_SYSLOG = "--no-syslog" in sys.argv

TREE = opt("--tree")
if not TREE:
    for cand in (os.path.join(HERE, "..", "demos"),
                 "/usr/share/cloonix/demos"):
        if os.path.isdir(cand):
            TREE = cand
            break
TREE = os.path.abspath(TREE) if TREE else None

CLOONIX = ([sys.executable, os.environ["CLOONIX_CLIENT"]]
           if os.environ.get("CLOONIX_CLIENT") else ["cloonix"])

# A toy keeps its syslog in its own tree: TESTITALL_SYSLOG names it.
SYSLOG = os.environ.get("TESTITALL_SYSLOG", "/var/log/syslog")

sys.stdout.reconfigure(line_buffering=True)

# the client

def cli(*args, **kw):
    timeout = kw.get("timeout", 30)
    cmd = list(CLOONIX)
    if kw.get("ack") is not None:
        cmd += ["--wait", str(kw["ack"])]
    cmd += ["cli", kw.get("net") or NET] + [str(a) for a in args]
    try:
        return subprocess.run(cmd, capture_output=True, text=True,
                              timeout=timeout, stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired:
        return subprocess.CompletedProcess(cmd, 124, "", "timed out")

def net_up(net=None):
    cmd = CLOONIX + ["cli", net or NET, "dump", "commands"]
    try:
        return subprocess.run(cmd, capture_output=True, text=True,
                              timeout=30, stdin=subprocess.DEVNULL
                              ).returncode == 0
    except subprocess.TimeoutExpired:
        return False

# The nets a demo drives beside NET; started here when they do not answer,
# and the ones started here are killed at the end.
EXTRA_NETS = {"cloonix/c2c.py": ("fido", "mito")}
# Alone too: kvm-booting demos (disk shared with the other worker's
# extractions), kvm wifi demos (100 ms beats starve), the two frr
# spiders (60 routers each) and stp (a real broadcast storm).
ALONE = set(EXTRA_NETS) | {"cloonix/phy.py", "cloonix/realnet.py",
                           "cloonix/kvm.py", "freebsd.py", "windows.py",
                           "cisco.py", "cloonix/kvmwifi.py",
                           "cloonix/cntkvmwifi.py",
                           "wifi/kvm_wpa2.py", "wifi/kvm_open.py",
                           "wifi/kvm_wpa3.py", "wifi/kvm_bridge.py",
                           "wifi/kvm_roaming.py", "wifi/kvm_mesh.py",
                           "wifi/cntkvm_home.py", "wifi/cntkvm_enterprise.py",
                           "wifi/cntkvm_roaming.py", "wifi/cntkvm_monitor.py",
                           "routing/frrspider.py", "routing/frrspider_bgp.py",
                           "switching/stp.py"}
g_started_nets = []

def stale_server(net):
    """The server of the net runs a binary replaced since it started (an
    install over a running net): its exe is a deleted file."""
    for pid in os.listdir("/proc"):
        if not pid.isdigit():
            continue
        try:
            with open("/proc/%s/cmdline" % pid, "rb") as f:
                argv = f.read().split(b"\0")
            exe = os.readlink("/proc/%s/exe" % pid)
        except OSError:
            continue
        argv = [a for a in argv if a]
        if (argv and argv[0].endswith(b"cloonix-main-server") and
                argv[-1] == net.encode() and exe.endswith(" (deleted)")):
            return True
    return False

def nets_for(name):
    """The extra nets of a demo up, or the reason they are not."""
    for net in EXTRA_NETS.get(name, ()):
        if net_up(net) and stale_server(net):
            print("  %s runs the binaries of an earlier install: restarted"
                  % net)
            subprocess.run(CLOONIX + ["stop", net], capture_output=True,
                           text=True, timeout=60, stdin=subprocess.DEVNULL)
            end = time.time() + 40
            while time.time() < end and net_up(net):
                time.sleep(1)
        if net_up(net):
            continue
        subprocess.run(CLOONIX + ["start", net], capture_output=True,
                       text=True, timeout=60, stdin=subprocess.DEVNULL)
        end = time.time() + 30
        while time.time() < end and not net_up(net):
            time.sleep(1)
        if not net_up(net):
            return "the net %s would not start" % net
        g_started_nets.append(net)
    return ""

def kill_started_nets():
    for net in g_started_nets:
        subprocess.run(CLOONIX + ["stop", net], capture_output=True,
                       text=True, timeout=60, stdin=subprocess.DEVNULL)

def objects(net=None):
    """What the server has right now, as a list of lines.

    Used only to decide whether the net is empty. The exact shape of a line
    is not parsed: a line that is not blank is something that is there, and
    that is the whole question being asked.
    """
    r = cli("dump", "commands", net=net)
    if r.returncode:
        return None
    return [ln.strip() for ln in r.stdout.split("\n") if ln.strip()]

def clean_net(timeout=90, net=None):
    """del all, then wait until the server really has nothing.

    del all returns long before the containers are gone. Starting the next
    demo on top of the last one's leftovers produces a failure in the wrong
    demo, which is the single most expensive kind of false report there is.
    """
    cli("del", "all", timeout=60, net=net)
    end = time.time() + timeout
    while time.time() < end:
        left = objects(net)
        if left is None:
            return False, "the net stopped answering while being cleared"
        if not left:
            time.sleep(2)
            return True, ""
        time.sleep(2)
    return False, "still holding %d object(s) after del all" % len(left or [])

# the demos

# The demos that take long: many routers, a measure, a guest that boots.
LONG = ("routing/frrmix.py", "routing/frrspider.py",
        "routing/frrspider_bgp.py", "wifi/cnt_wpa2.py", "wifi/cnt_roaming.py",
        "watching/perf.py",
        "cloonix/phy.py", "cloonix/realnet.py", "freebsd.py", "windows.py",
        "cisco.py")

def demos():
    """Every runnable demo, as (name, path, images it names)."""
    out = []
    for dirpath, dirnames, files in os.walk(TREE):
        # lib/ holds what the demos share, not demos
        dirnames[:] = [d for d in dirnames
                       if not d.startswith(".") and d != "__pycache__" and
                       not (dirpath == TREE and d == "lib")]
        rel = os.path.relpath(dirpath, TREE)
        if rel != "." and len(rel.split(os.sep)) > 2:
            continue
        for f in sorted(files):
            if not f.endswith(".py") or f.startswith("."):
                continue
            path = os.path.join(dirpath, f)
            name = os.path.join(rel, f) if rel != "." else f
            out.append((name.replace(os.sep, "/"), path, images_of(path)))
    # The long ones last, in the order of LONG: a failure is known at once.
    return sorted(out, key=lambda d: (LONG.index(d[0]) if d[0] in LONG
                                      else -1, d[0]))

def images_of(path):
    """The images a demo names, read out of its source without running it."""
    try:
        with open(path) as f:
            text = f.read()
    except OSError:
        return []
    # ROOTFS_CHOICES are alternatives, one of them is enough; a _ROOTFS
    # is one image the demo cannot do without
    choices, needed = [], []
    for m in re.finditer(r"^[A-Z_]*ROOTFS_CHOICES\s*=\s*\((.*?)\)",
                         text, re.S | re.M):
        choices += re.findall(r"[\"']([^\"']+)[\"']", m.group(1))
    for m in re.finditer(r"^[A-Z_]*_ROOTFS\s*=\s*[\"']([^\"']+)[\"']",
                         text, re.M):
        needed.append(m.group(1))
    return (list(dict.fromkeys(choices)), list(dict.fromkeys(needed)))

def modules_of(path):
    """The kernel modules the host must have loaded, HOST_MODULES of the
    demo, read out of its source without running it."""
    try:
        with open(path) as f:
            text = f.read()
    except OSError:
        return []
    m = re.search(r"^HOST_MODULES\s*=\s*\((.*?)\)", text, re.S | re.M)
    return re.findall(r"[\"']([^\"']+)[\"']", m.group(1)) if m else []

def host_has_module(mod):
    """Loaded on the host: /sys/module/<name>, underscores for dashes."""
    return any(os.path.isdir("/sys/module/" + n)
               for n in (mod, mod.replace("-", "_")))

def load_host_modules(found):
    """The HOST_MODULES of the demos to run that the host lacks, loaded by
    modprobe through passwordless sudo, as sub_air_medium.py loads its own:
    cloonix never loads a module, its containers cannot."""
    mods = sorted({m for name, path, _ in found if wanted(name)[0]
                   for m in modules_of(path) if not host_has_module(m)})
    if not mods:
        return
    if not sudo_ok():
        print("  not loaded, no passwordless sudo: %s, their demos skipped"
              % " ".join(mods))
        return
    r = sudo("modprobe", "-a", *mods)
    left = [m for m in mods if not host_has_module(m)]
    if left:
        print("  still not loaded: %s, their demos skipped (%s)"
              % (" ".join(left), (r.stderr or r.stdout).strip()
                 or "modprobe exit %d" % r.returncode))

def plays_the_packages(path):
    """all.py: PLAYS_THE_PACKAGES = True, every other demo in a row."""
    try:
        with open(path) as f:
            return re.search(r"^PLAYS_THE_PACKAGES\s*=\s*True\b", f.read(),
                             re.M) is not None
    except OSError:
        return False

def needs_root(path):
    """NEEDS_ROOT = True in the demo source: it works on a root net only."""
    try:
        with open(path) as f:
            return re.search(r"^NEEDS_ROOT\s*=\s*True\b", f.read(), re.M) is not None
    except OSError:
        return False

def net_runs_as_root(net):
    """The uid of cloonix-main-server <config> <net>, read in /proc."""
    for pid in os.listdir("/proc"):
        if not pid.isdigit():
            continue
        try:
            with open("/proc/%s/cmdline" % pid, "rb") as f:
                argv = f.read().split(b"\0")
            if (len(argv) > 2 and os.path.basename(argv[0]) ==
                    b"cloonix-main-server" and argv[2].decode() == net):
                with open("/proc/%s/status" % pid) as f:
                    uid = re.search(r"^Uid:\s+(\d+)", f.read(), re.M)
                return uid is not None and uid.group(1) == "0"
        except OSError:
            continue
    return False

BULK = "/var/lib/cloonix/bulk"

# Not the toy's: toy_targz/toy_drop, the demos the toy leaves out.
def toy_drop():
    path = os.path.join(HERE, "..", "toy_targz", "toy_drop")
    try:
        with open(path) as f:
            return tuple(l.split()[0] for l in f
                         if l.strip() and not l.startswith("#"))
    except OSError:
        return ()
# c2c.py needs a third net, fido, which the toy runs do not start.
TOY_NO = toy_drop() + ("cloonix/c2c.py",)
# Nor with a single uid mapping (TESTITALL_TOY_SINGLE_IDMAP): setgroups is
# denied, daemons that drop privileges die.
TOY_SINGLE_NO = ("address/dns.py", "address/dhcp.py", "services/smtp.py",
                 "services/mqtt.py", "services/proxy.py",
                 "services/asterisk.py", "cloonix/nginx.py")


def wanted(name):
    short = name.split("/")[-1][:-3]
    if ONLY and not any(w in name or w == short for w in ONLY):
        return False, "not in --only"
    if SKIP and any(w in name or w == short for w in SKIP):
        return False, "in --skip"
    if os.environ.get("TESTITALL_TOY") and name in TOY_NO:
        return False, "toy: not possible in a toy, see TOY_NO"
    if os.environ.get("TESTITALL_TOY_SINGLE_IDMAP") and name in TOY_SINGLE_NO:
        return False, ("toy: single uid mapping, setgroups denied, its daemon "
                       "cannot drop privileges (see TOY_SINGLE_NO)")
    # cloonix never loads a module (RHEL has none): cnt radios need it loaded
    # (sub_cntwifi.py does), kvm radios nothing on the host.
    if (name in ("cloonix/cntwifi.py", "cloonix/cntkvmwifi.py") or
            name.startswith("wifi/cnt")) and not hwsim_loaded():
        return False, ("mac80211_hwsim is not loaded: "
                       "sudo modprobe mac80211_hwsim radios=0")
    return True, ""


def hwsim_loaded():
    return os.path.isdir("/sys/module/mac80211_hwsim")

IMAGE_OK = {}

g_probe_lock = threading.Lock()

def image_works(image, net=None):
    """Probed on the worker's own net: a probe on another net would land in
    the middle of the demo running there, and its del all would eat it. One
    probe at a time: the first add of a tar.gz image extracts it into bulk,
    and two servers extracting the same image at once is not a thing to
    try."""
    with g_probe_lock:
        return image_probe(image, net or NET)

def image_probe(image, net):
    if image in IMAGE_OK:
        return IMAGE_OK[image]
    # a guest disk is one file in bulk; a container image is a directory
    # that only the server can vouch for, so a throwaway container asks
    if image.endswith(".qcow2"):
        IMAGE_OK[image] = os.path.exists(os.path.join(BULK, image))
        return IMAGE_OK[image]
    # A tar.gz image is extracted by the server at this first add.
    r = cli("add", "cnt", "probe_demotest", "eth=1", image, timeout=300,
            ack=240, net=net)
    good = r.returncode == 0
    if good:
        end = time.time() + 20
        good = False
        while time.time() < end:
            left = objects(net) or []
            if any(" probe_demotest " in (" " + ln + " ") for ln in left):
                good = True
                break
            time.sleep(1)
    # A refused add made no object: a del then would only be a refusal more
    # in the syslog.
    if r.returncode == 0:
        cli("del", "probe_demotest", timeout=30, net=net)
    IMAGE_OK[image] = good
    return good

def builder_for(image):
    if image == "c8000v.qcow2":
        return "vm_create/cisco/doitall.sh --all <the cisco iso>"
    if image.endswith(".qcow2"):
        return "vm_create/cnt_kvm/kvm_%s.sh" % image[:-6]
    for d in ("cnt_kvm_demo", "cnt_kvm"):
        p = os.path.join(TREE, "..", "vm_create", d, "cnt_%s.sh" % image)
        if os.path.exists(p):
            return "vm_create/%s/cnt_%s.sh" % (d, image)
    return "vm_create/cnt_kvm_demo/cnt_%s.sh (not in this tree)" % image

def kerr_source():
    """Where the KERR lines are read: the syslog file, else the journal (a
    host without rsyslog has no /var/log/syslog), else nowhere, said."""
    if NO_SYSLOG:
        return None
    if os.path.isfile(SYSLOG):
        return "file"
    if shutil.which("journalctl"):
        return "journal"
    return None

KERR_SOURCE = kerr_source()

def syslog_size():
    """A mark in the log: an offset in the file, a time in the journal."""
    if KERR_SOURCE == "file":
        try:
            return os.path.getsize(SYSLOG)
        except OSError:
            return None
    if KERR_SOURCE == "journal":
        return ("journal", time.time())
    return None

def syslog_since(offset):
    if offset is None:
        return []
    if isinstance(offset, tuple):
        r = subprocess.run(["journalctl", "-q", "-o", "cat", "--since",
                            "@%d" % int(offset[1])], capture_output=True,
                           text=True, stdin=subprocess.DEVNULL)
        if r.returncode != 0 or (not r.stdout and r.stderr.strip()):
            print("  WARNING journalctl: %s, the KERR of this demo are not "
                  "counted" % (r.stderr.strip() or "exit %d" % r.returncode))
        return r.stdout.split("\n")
    try:
        with open(SYSLOG, "rb") as f:
            f.seek(offset)
            raw = f.read()
    except OSError:
        return []
    return raw.decode("utf-8", "replace").split("\n")

KERR = re.compile(r"KERR:\s*(.*)$")

def errors_in(lines, net=None):
    """The distinct KERR lines, with how many times each appeared.

    Distinct, because one fault repeated four hundred times is one thing to
    look at. Counted, because how often it happened is the difference
    between a race and a certainty. With several nets running demos at
    once, a line naming another net is theirs; a line naming no net is
    counted for every demo of the moment.
    """
    others = [n for n in NETS if n != net] if net else []
    got = {}
    for line in lines:
        m = KERR.search(line)
        if not m:
            continue
        if others and (net not in line) and any(n in line for n in others):
            continue
        what = m.group(1).strip()
        what = re.sub(r"\s+", " ", what)
        got[what] = got.get(what, 0) + 1
    return got

def crashes_in(lines):
    return [ln.strip() for ln in lines if "CRASH " in ln]

def run_one(name, path, net=None):
    """One demo, start to finish, with everything worth knowing collected."""
    net = net or NET
    logfile = os.path.join(LOGS, name.replace("/", "_") + ".log")
    started = time.time()
    mark = syslog_size()
    env = dict(os.environ)
    env["CLOONIX_DEMO_FAST"] = "1"   # no pauses for a reader here
    env["CLOONIX_DEMO_STORY"] = "1"  # the log keeps the history, marked
    env.setdefault("PYTHONUNBUFFERED", "1")
    try:
        r = subprocess.run([sys.executable, path, net],
                           capture_output=True, text=True, timeout=TIMEOUT,
                           stdin=subprocess.DEVNULL, env=env,
                           cwd=os.path.dirname(path))
        code, out, err = r.returncode, r.stdout, r.stderr
        timed_out = False
    except subprocess.TimeoutExpired as stop:
        code, timed_out = 124, True
        out = (stop.stdout or b"").decode("utf-8", "replace") \
            if isinstance(stop.stdout, bytes) else (stop.stdout or "")
        err = (stop.stderr or b"").decode("utf-8", "replace") \
            if isinstance(stop.stderr, bytes) else (stop.stderr or "")
    took = time.time() - started

    with open(logfile, "w") as f:
        f.write("%s %s\n\n" % (path, net))
        f.write(out)
        if err:
            f.write("\n--- stderr ---\n")
            f.write(err)

    logged = syslog_since(mark)
    return {"name": name, "code": code, "took": took, "log": logfile,
            "timed_out": timed_out, "out": out, "err": err,
            "kerr": errors_in(logged, net), "crash": crashes_in(logged)}

def last_words(got):
    """What the demo said about its own failure.

    Every demo here ends by naming the claim that did not hold and exits
    non-zero, some with the state of the guests under it. That is the
    report; the exit code only says there is one.
    """
    for text in (got["err"], got["out"]):
        lines = [ln.replace("#story# ", "", 1).rstrip()
                 for ln in (text or "").split("\n")
                 if ln.replace("#story#", "", 1).strip()]
        if lines:
            return lines[-40:]
    return []

def tail_story(got, n):
    """The last n non-empty lines of the demo's own output, the #story#
    marker stripped, for folding into the main results."""
    text = got.get("out") or ""
    lines = [ln.replace("#story# ", "", 1).rstrip()
             for ln in text.split("\n")
             if ln.replace("#story#", "", 1).strip()]
    return lines[-n:]

g_print_lock = threading.Lock()

def phase(name):
    """A marker in the syslog: every KERR after it belongs to this demo."""
    try:
        syslog.syslog("CLOONIX-PHASE %s" % name)
    except Exception:
        pass


def run_demo(name, path, images, net, say):
    """One demo on one net: the verdict, printed through say()."""
    want, why = wanted(name)
    if not want:
        if "guest disk" in why or why.startswith("toy:"):
            say("  SKIP  %-32s %s" % (name, why))
            return {"name": name, "verdict": "SKIP", "took": 0.0,
                    "kerr": {}, "crash": [], "why": why}
        return None
    choices, needed = images
    missing = [i for i in needed if not image_works(i, net)]
    if choices and not any(image_works(i, net) for i in choices):
        missing = choices[:1] + missing
    if missing:
        say("  SKIP  %-32s no image: %s" % (name, missing[0]))
        say("        build it:  ./%s" % builder_for(missing[0]))
        return {"name": name, "verdict": "SKIP", "took": 0.0, "kerr": {},
                "crash": [], "why": "no %s" % missing[0]}
    unloaded = [m for m in modules_of(path) if not host_has_module(m)]
    if unloaded:
        say("  SKIP  %-32s host module not loaded: %s"
            % (name, " ".join(unloaded)))
        say("        load it:")
        say("sudo modprobe %s" % " ".join(unloaded) if len(unloaded) == 1
            else "sudo modprobe -a %s" % " ".join(unloaded))
        return {"name": name, "verdict": "SKIP", "took": 0.0, "kerr": {},
                "crash": [], "why": "module %s" % unloaded[0]}
    if plays_the_packages(path):
        say("  SKIP  %-32s plays the others, each tested on its own" % name)
        return {"name": name, "verdict": "SKIP", "took": 0.0, "kerr": {},
                "crash": [], "why": "plays the others"}
    if needs_root(path) and not net_runs_as_root(net):
        say("  SKIP  %-32s needs a net run as root, %s is not" % (name, net))
        return {"name": name, "verdict": "SKIP", "took": 0.0, "kerr": {},
                "crash": [], "why": "net not root"}
    phase("demo %s" % name)
    why = nets_for(name)
    if why:
        say("  FAIL  %-32s %s" % (name, why))
        return {"name": name, "verdict": "FAIL", "took": 0.0, "kerr": {},
                "crash": [], "why": why}
    if len(NETS) == 1:
        print("  RUN   %-32s ..." % name, end=" ", flush=True)
    else:
        say("  RUN   %-32s on %s" % (name, net))
    got = run_one(name, path, net)
    verdict = "PASS" if got["code"] == 0 else \
              ("TIMEOUT" if got["timed_out"] else "FAIL")
    if len(NETS) == 1:
        print("%-7s %5.0fs" % (verdict, got["took"]))
    else:
        say("  %-5s %-32s %5.0fs on %s" % (verdict, name, got["took"], net))
    if verdict != "PASS":
        for line in last_words(got):
            say("        %s" % line[:100])
        say("        full output: %s" % got["log"])
        say("        --- last %d lines of its own log ---" % LOG_TAIL)
        for line in tail_story(got, LOG_TAIL):
            say("        | %s" % line[:200])
        say("        --- end of %s log ---" % name)
    if got["crash"]:
        say("        %d CRASH line(s) in the syslog during this one"
            % len(got["crash"]))
    if got["kerr"]:
        say("        %d KERR line(s), %d distinct:"
            % (sum(got["kerr"].values()), len(got["kerr"])))
        for what, nb in sorted(got["kerr"].items(),
                               key=lambda kv: -kv[1])[:5]:
            say("          x%-5d %s" % (nb, what[:88]))
    if verdict != "PASS" and KEEP:
        say("        --keep: the machines are left up. Clear them")
        say("        with  cloonix cli %s del all  before the next" % net)
        got["verdict"] = verdict
        got["keep"] = True
        return got
    cleared, why = clean_net(net=net)
    if not cleared:
        say("        the net would not clear after it: %s" % why)
        verdict = verdict if verdict != "PASS" else "DIRTY"
    got["verdict"] = verdict
    return got

def run_all(found):
    """Every demo once, the nets of NETS each taking the next one; the
    ALONE ones after all that, on the first net."""
    shared = [d for d in found if d[0] not in ALONE]
    alone = [d for d in found if d[0] in ALONE]
    results = []
    lock = threading.Lock()
    stop = []

    def say_now(line):
        print(line, flush=True)

    def worker(net):
        while not stop:
            with lock:
                if not shared:
                    return
                name, path, images = shared.pop(0)
            lines = []
            got = run_demo(name, path, images, net, lines.append)
            with g_print_lock:
                for line in lines:
                    print(line, flush=True)
            with lock:
                if got:
                    results.append(got)
                    if got.get("keep"):
                        stop.append(1)

    if len(NETS) == 1:
        for name, path, images in shared + alone:
            got = run_demo(name, path, images, NET, say_now)
            if got:
                results.append(got)
                if got.get("keep"):
                    break
        return results
    threads = [threading.Thread(target=worker, args=(net,)) for net in NETS]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    if not stop:
        for name, path, images in alone:
            got = run_demo(name, path, images, NET, say_now)
            if got:
                results.append(got)
                if got.get("keep"):
                    break
    order = {d[0]: i for i, d in enumerate(found)}
    results.sort(key=lambda g: order.get(g["name"], 0))
    return results

def main():
    if not TREE or not os.path.isdir(TREE):
        sys.exit("no demos tree found. Say where with --tree DIR")
    os.makedirs(LOGS, exist_ok=True)

    found = demos()
    if not found:
        sys.exit("no demo found under %s" % TREE)

    print()
    print("demo test, on the cloonix demos")
    print("  tree     %s" % TREE)
    print("  net      %s" % " ".join(NETS))
    print("  logs     %s" % LOGS)
    print("  timeout  %d s per demo" % TIMEOUT)
    print("  kerr     %s" % {"file": SYSLOG, "journal": "the journal (no %s)"
                             % SYSLOG}.get(KERR_SOURCE, "NOT COUNTED: no %s "
                             "and no journalctl" % SYSLOG
                             if not NO_SYSLOG else "not counted, --no-syslog"))
    print()

    if LIST_ONLY:
        for name, _path, images in found:
            want, why = wanted(name)
            print("  %-34s %-28s %s"
                  % (name, ",".join(images[0] + images[1]) or "-",
                     "" if want else "skipped: " + why))
        return 0

    for net in NETS:
        if not net_up(net):
            sys.exit("no answer from a cloonix net called %r.\n"
                     "  A demo drives a net that is already running:\n"
                     "    cloonix start %s" % (net, net))
        ok, why = clean_net(net=net)
        if not ok:
            sys.exit("the net %s was not empty at the start and would not "
                     "clear: %s\n  every result after this would be about "
                     "the wrong demo." % (net, why))
    print("  the net is empty, so the first demo starts from nothing")
    load_host_modules(found)
    print()

    results = []
    for rnd in range(REPEAT):
        if REPEAT > 1:
            print("=== round %d of %d ===" % (rnd + 1, REPEAT))
        results += run_all(found)

    print()
    print("  %-34s %-8s %7s %8s" % ("demo", "verdict", "seconds", "kerr"))
    print("  " + "-" * 62)
    for got in results:
        print("  %-34s %-8s %7.0f %8d"
              % (got["name"], got["verdict"], got["took"],
                 sum(got.get("kerr", {}).values())))

    tally = {}
    for got in results:
        tally[got["verdict"]] = tally.get(got["verdict"], 0) + 1
    print()
    print("  " + "   ".join("%s %d" % (k, v) for k, v in sorted(tally.items())))

    noisy = [g for g in results if g.get("kerr")]
    if noisy:
        print()
        print("  the switch logged errors during %d demo(s), including ones"
              % len(noisy))
        print("  that passed. Worth reading -- a demo can get its result and")
        print("  still have made the server unhappy on the way:")
        for got in noisy:
            print("    %-30s %d line(s), %d distinct"
                  % (got["name"], sum(got["kerr"].values()),
                     len(got["kerr"])))

    kill_started_nets()
    bad = tally.get("FAIL", 0) + tally.get("TIMEOUT", 0) + tally.get("DIRTY", 0)
    if bad:
        print()
        print("  the logs of every run are in %s" % LOGS)
    return 1 if bad else 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print()
        print("stopped. The net may still be holding what the last demo made:")
        print("cloonix cli %s del all" % NET)
        sys.exit(130)
