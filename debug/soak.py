#!/usr/bin/env python3
r"""soak - create and destroy cloonix objects at random until something breaks.

    ./soak.py [net] [workers] [seed]
    ./soak.py nemo 4 12345

This is not a demo. It makes no topology worth looking at and teaches
nothing about any protocol. Its whole job is to keep asking the server for
things at awkward moments, from several directions at once, and to notice
the first time an answer is wrong.

WHAT IT DOES

Several worker threads, each in a loop: pick an object kind by weight, do
something to it, check the server agrees. Objects are given a random
lifetime when they are made and a reaper deletes them when it expires, so
creation and destruction are always interleaved rather than done in tidy
phases. The pace changes on its own between bursts, a normal rhythm and
long pauses, because a fault that needs a burst and a fault that needs a
quiet moment are different faults.

Ctrl-C is caught. It stops the workers, deletes everything the run made,
waits for it to go, and then checks the machine is as clean as it found it.

WHAT IT CHECKS, WHICH IS THE POINT

  after every create   the object appears in "lst" within a timeout
  after every delete   it disappears within a timeout
  continuously         a reconciliation both ways: nothing the server has
                       that this script did not make, nothing this script
                       made that the server has lost
  at the end           no object of ours left, and no mount or namespace
                       left behind on the machine

Every one of those has been a real bug in this code base at least once. A
create reported as successful and silently dropped, a delete that left its
mounts, a cable filed where nothing would look for it: all of them pass a
test that only asks "did the command return zero".

WEIGHTS

A container is what people make, so it is made twenty times more often
than the rarest thing here. The rest are guesses at how much use they get,
and they are one table at the top of the file precisely so they can be
argued with.

Needs cloonix on the PATH and nothing else. Standard library only.
"""

import json
import os
import random
import re
import signal
import subprocess
import sys
import threading
import time
from datetime import datetime

NET = sys.argv[1] if len(sys.argv) > 1 else "nemo"
WORKERS = int(sys.argv[2]) if len(sys.argv) > 2 else 4
SEED = int(sys.argv[3]) if len(sys.argv) > 3 else random.randrange(1 << 30)

# The seed is printed and taken as an argument so a failure can be run again.
random.seed(SEED)

PREFIX = "sk"

# On the toys: testitalltoy.sh exports TESTITALL_TOY, the directory that
# holds them, and CLOONIX_CLIENT, their client, as testitall.py takes it. A
# net's work zone and syslog are then inside its rootfs, and the crun that
# reads its state is the host-side one of the toy's bin.
TOY = os.environ.get("TESTITALL_TOY", "")
CLOONIX = ([sys.executable, os.environ["CLOONIX_CLIENT"]]
           if os.environ.get("CLOONIX_CLIENT") else ["cloonix"])

def lib_cloonix(net):
    """/var/lib/cloonix/<net> of the bundle, or of the toy's rootfs."""
    if TOY:
        return "%s/%s/rootfs/var/lib/cloonix/%s" % (TOY, net, net)
    return "/var/lib/cloonix/%s" % net

def host_syslog():
    """Where the daemons of NET write their KERR lines."""
    if TOY:
        return "%s/%s/rootfs/cloonix/cloonfs/var/log/syslog" % (TOY, NET)
    return "/var/log/syslog"

# The crun cloonix ships, used read-only to ask whether a container is still
# alive.
if TOY:
    CRUN_BIN = "%s/%s/bin/cloonix-crun-%s" % (TOY, NET, NET)
else:
    CRUN_BIN = "/usr/libexec/cloonix/cloonfs/bin/cloonix-crun"
    if not os.path.exists(CRUN_BIN):
        for _c in ("/usr/local/libexec/cloonix/cloonfs/bin/cloonix-crun",
                   "/usr/libexec/cloonix/cloonfs/bin/crun"):
            if os.path.exists(_c):
                CRUN_BIN = _c
                break

WEIGHTS = {
    "cnt":  20,
    "lan":  14,
    "snf":   6,
    "tap":   3,
    "nat":   3,
    "a2b":   1,
    "kvm":   1,
    "c2c":   1,
}

# c2c needs a second net, so it is off unless one is named.
AGENT = os.environ.get("SOAK_AGENT", "").strip() not in ("", "0", "no")
# A container's agent is up in seconds; a kvm has to boot a whole OS first.
AGENT_WAIT = float(os.environ.get("SOAK_AGENT_WAIT", "45"))
AGENT_WAIT_KVM = float(os.environ.get("SOAK_AGENT_WAIT_KVM", "240"))
# A windows guest boots slowest and its agent (launched through qemu-ga) comes later still.
AGENT_WAIT_WIN = float(os.environ.get("SOAK_AGENT_WAIT_WIN", "300"))

KVM_RAM = int(os.environ.get("SOAK_KVM_RAM", "1024"))
KVM_CPU = int(os.environ.get("SOAK_KVM_CPU", "1"))
MAX_LIVE_KVM = int(os.environ.get("SOAK_MAX_LIVE_KVM", "0"))   # 0 = no cap

MAX_LIVE_SYSTEMD_CNT = int(os.environ.get("SOAK_MAX_LIVE_SYSTEMD_CNT", "5"))

def agent_ping_ok(name):
    """Is this object's agent answering? The same ping_ok the gui colours
    on, read from the topology rather than from an event. Works for a
    container or a kvm."""
    try:
        r = subprocess.run(CLOONIX + ["cli", NET, "dump", "json"],
                           capture_output=True, text=True, timeout=60)
        obj = json.loads(r.stdout)
        for c in obj.get("cnt", []) or []:
            if c.get("name") == name:
                return bool(c.get("ping_ok"))
        for k in obj.get("kvm", []) or []:
            if k.get("name") == name:
                if "ping_ok" in k:
                    return bool(k.get("ping_ok"))
                return bool(int(k.get("vm_config_flags", 0)) & 0x80000)
    except Exception:                                   # noqa: BLE001
        pass
    return False

PEER = os.environ.get("SOAK_PEER", "").strip()
if not PEER:
    WEIGHTS.pop("c2c", None)

C2C_TARGET = int(os.environ.get("SOAK_C2C", "2")) if PEER else 0

_wspec = os.environ.get("SOAK_WEIGHTS", "").strip()
if _wspec:
    for _item in _wspec.split(","):
        _item = _item.strip()
        if not _item:
            continue
        if "=" not in _item:
            raise SystemExit("SOAK_WEIGHTS: expected kind=number, got '%s'"
                             % _item)
        _k, _v = _item.split("=", 1)
        _k = _k.strip()
        if _k not in WEIGHTS:
            raise SystemExit("SOAK_WEIGHTS: no such kind: %s. Known: %s"
                             % (_k, ", ".join(sorted(WEIGHTS))))
        try:
            _v = int(_v.strip())
        except ValueError:
            raise SystemExit("SOAK_WEIGHTS: %s is not a number for %s"
                             % (_v.strip(), _k))
        if _v < 0:
            raise SystemExit("SOAK_WEIGHTS: %s cannot be negative" % _k)
        if _v == 0:
            WEIGHTS.pop(_k, None)
        else:
            WEIGHTS[_k] = _v
    if not WEIGHTS:
        raise SystemExit("SOAK_WEIGHTS left nothing to do")

MAX_LIVE = int(os.environ.get("SOAK_MAX_LIVE", "30"))

_only = os.environ.get("SOAK_ONLY", "").strip()
if _only:
    _keep = set(k.strip() for k in _only.split(",") if k.strip())
    _bad = _keep - set(WEIGHTS)
    if _bad:
        raise SystemExit("SOAK_ONLY: no such kind: %s. Known: %s"
                         % (", ".join(sorted(_bad)),
                            ", ".join(sorted(WEIGHTS))))
    WEIGHTS = {k: v for k, v in WEIGHTS.items() if k in _keep}
    if not WEIGHTS:
        raise SystemExit("SOAK_ONLY left nothing to do")

LIFE_MIN, LIFE_MAX = 8.0, 90.0      # seconds an object is allowed to live
CHECK_APPEAR = 45.0                 # how long a create has to show up
CHECK_VANISH = 60.0                 # how long a delete has to take effect
RECONCILE_EVERY = 20.0

# Pace, picked at random per phase.
PHASES = [("burst", 0.0, 0.03), ("normal", 0.1, 0.6), ("slow", 1.0, 3.0)]

g_stop = threading.Event()
g_lock = threading.RLock()
g_run_started = time.time()
g_preexisting_pids = set()
g_preexisting_objects = set()
g_model = {}            # name -> dict(kind, born, life, eths, lans)
g_deleting = {}
# Creates of a c2c in flight: see count_c2c_locked.
g_c2c_pending = 0
g_lans = {}             # lan name -> list of (item, eth)
g_stats = {}
g_reasons = {}
g_refused = {}
g_preexisting_dirt = set()   # mounts and netns already there at the start
g_adding = {}
g_faults = []
# How many times one KIND of fault is printed in full before it goes quiet.
FAULT_PRINT_MAX = int(os.environ.get("SOAK_FAULT_PRINT_MAX", "5"))
g_fault_seen = {}
g_ops = 0
# Names must be unique across RUNS, not just within one.
g_counter = random.randrange(100000, 8000000)
g_fd_first = None
g_fd_last = None
g_rootfs = None
g_kvm_image = None

def note(kind, detail=""):
    with g_lock:
        g_stats[kind] = g_stats.get(kind, 0) + 1
        if detail:
            g_stats.setdefault("_detail", {}).setdefault(kind, detail)

def why(kind, r, name=None):
    """Record WHY something was refused, not just that it was.

    A run reporting "cnt refused 1348" against "cnt created 120" says the
    server spent the whole time saying no and gives no clue what it was
    saying. The message is right there in the command's output and was
    being thrown away.
    """
    msg = ((r.stdout or "") + (r.stderr or "")).strip()
    msg = " ".join(msg.split())[:70] or "(said nothing)"
    if name:
        with g_lock:
            g_adding.pop(name, None)
            g_refused[name] = (r.returncode,
                               " ".join(((r.stdout or "") +
                                         (r.stderr or "")).split())[:300],
                               time.time())
    if "in use" in msg or "IN USE" in msg:
        msg += "  <-- a leftover from an earlier run, not a server fault"
    with g_lock:
        g_reasons.setdefault(kind, {})
        g_reasons[kind][msg] = g_reasons[kind].get(msg, 0) + 1
    note("%s refused" % kind)

def evidence(name, kind=""):
    """What lst actually said, kept next to the complaint.

    Every check here rests on finding a name in the output of lst, and that
    assumption was never tested against the real thing for anything but a
    container. A report saying "lst has never shown it" is worth very
    little without the output it is talking about -- it is as likely to
    mean the check is looking for the wrong shape as that the object is
    missing.
    """
    text = lst()
    lines = [l for l in text.split("\n") if l.strip()]
    pat = name_pattern(name)
    hit = [l for l in lines if pat.search(l)]
    same = [l for l in lines if kind and (" %s " % kind) in l]
    out = ["lst has %d line(s)" % len(lines)]
    if hit:
        out.append("lines naming it: %s" % " | ".join(hit[:3]))
    else:
        out.append("no line names it")
    if same:
        out.append("other %s lines look like: %s" % (kind, same[0].strip()))
    elif kind:
        out.append("lst has no %s line at all, so the check may be looking "
                   "for the wrong thing" % kind)
    try:
        path = "/tmp/soak_lst_%s.txt" % name
        with open(path, "w") as f:
            f.write(text)
        out.append("full output in %s" % path)
    except OSError:
        pass
    return "; ".join(out)

def fault(what, detail):
    """A fault is recorded and the run carries on.

    Stopping at the first one would find one bug per run. Carrying on finds
    whether it is one fault or a pattern, and whether the machine degrades
    after it -- which is usually the more useful thing to know.

    Every fault is COUNTED, but the same one is only PRINTED a few times.
    One run filed the same fault three hundred and twenty times, the same
    seven names in every one, and nothing else in that run's output could
    be found past them. Over a day that is tens of thousands of identical
    lines burying the one thing that happened once. The full tally is in
    the grouped summary at the end, so nothing is lost by going quiet
    after the first few -- and a fault that keeps arriving still says so,
    once, when it crosses the limit.
    """
    with g_lock:
        g_faults.append((time.time(), what, detail))
        n = len(g_faults)
        seen = g_fault_seen.get(what, 0) + 1
        g_fault_seen[what] = seen
    if seen <= FAULT_PRINT_MAX:
        print("\n  FAULT %d: %s\n           %s" % (n, what, detail))
    elif seen == FAULT_PRINT_MAX + 1:
        print("\n  FAULT %d: %s\n           (seen %d times now -- further "
              "ones are counted but not printed, see the summary)"
              % (n, what, seen))

def adding(name):
    with g_lock:
        g_adding[name] = time.time()

def cli(*args, **kw):
    """One cloonix command. There is no cli verb any more: the verb comes
    first and the net after it, so callers still write cli("add", "cnt", ..)
    and this puts NET where it now goes."""
    ack = kw.get("ack")
    a = [str(x) for x in args]
    cmd = list(CLOONIX)
    if ack is not None:
        cmd += ["--wait", str(ack)]
    cmd += ["cli", NET, a[0]] + a[1:]
    # the client must outlive the answer it waits for
    limit = 90 if ((ack is None) or (ack < 60)) else ack + 30
    try:
        return subprocess.run(cmd, capture_output=True, text=True,
                              timeout=limit)
    except subprocess.TimeoutExpired:
        fault("the client never returned", " ".join(cmd))
        return subprocess.CompletedProcess(cmd, 1, "", "timeout")

g_lst_rc = 0     # exit code of the last dump commands: 0 is a server that answers

def lst():
    global g_lst_rc
    r = subprocess.run(CLOONIX + ["cli", NET, "dump", "commands"],
                       capture_output=True, text=True, timeout=60)
    g_lst_rc = r.returncode
    return r.stdout

def name_pattern(name):
    """Match a name as a whole word, whatever is printed around it.

    lst does not print bare words. It prints quoted commands::

        "cloonix cli nemo plug skc119 eth0 skl121",

    so the name is followed by a quote, not by whitespace or end of line.
    The first version of this check demanded whitespace on both sides and
    therefore matched nothing but containers -- which is why an earlier run
    reported ninety faults, most of them this function, including several
    where the evidence printed alongside the complaint plainly showed the
    object present.

    A lookaround for name characters is the right test: it does not care
    what the delimiter is, and it still refuses to find skl1 inside skl11.
    """
    return re.compile(r"(?<![A-Za-z0-9_])%s(?![A-Za-z0-9_])"
                      % re.escape(name))

def in_lst(text, name):
    return name_pattern(name).search(text) is not None

def count_in_lst(text, name):
    return len(name_pattern(name).findall(text))

def wait_present(name, timeout=CHECK_APPEAR):
    end = time.time() + timeout
    while time.time() < end and not g_stop.is_set():
        if in_lst(lst(), name):
            return True
        time.sleep(1)
    return in_lst(lst(), name)

def wait_cabled(lan, timeout=CHECK_APPEAR):
    """A lan needs BOTH its ends, and checking the name is not enough.

    A cable is two commands. If one of them is lost the lan still appears
    in lst, attached to the one machine that took it -- and a check that
    only asks "is the name there" passes while the topology has a wire
    hanging loose. That is precisely the fault this whole exercise was
    about, and the first version of this test would have missed it.
    """
    end = time.time() + timeout
    while time.time() < end and not g_stop.is_set():
        if count_in_lst(lst(), lan) >= 2:
            return True
        time.sleep(1)
    return count_in_lst(lst(), lan) >= 2

def wait_absent(name, timeout=CHECK_VANISH):
    end = time.time() + timeout
    while time.time() < end and not g_stop.is_set():
        if not in_lst(lst(), name):
            return True
        time.sleep(1)
    return not in_lst(lst(), name)

def new_name(kind):
    """A name that has never been used before and never will be again.

    Reusing a freed name looked tidy and made the checks lie. A worker
    could ask for skc11, have it deleted, and another worker create a new
    skc11 while the first delete was still in flight -- so the check for
    "it should be gone now" found the new one and called it a fault.

    It would also hide a real bug rather than find one: a name reused
    before the server has finished disposing of the old object is exactly
    the case that gets refused, and a soak test should be provoking that
    deliberately, not stumbling into it and misreading the result.
    """
    global g_counter
    with g_lock:
        g_counter += 1
        return "%s%s%d" % (PREFIX, kind, g_counter)

def still_ours(name):
    """Is this object still one of ours, or has another worker deleted it?

    The agent waits below park a worker for a minute or two on one name,
    and three separate paths delete a random name while they do it: the
    cap in worker(), the one-in-ten early death in worker(), and reap().
    Nothing in those waits ever asked whether the thing being waited on
    was still there, and agent_ping_ok() cannot tell the difference -- a
    name that is not in the topology at all reads exactly like one whose
    agent has not come up. So a delete landing on a guest mid-wait was
    reported as an agent failure.

    Measured: five kvm faults in one run, four of which left NOT ONE line
    in the syslog, and the fifth left an ordered teardown -- read error,
    then ECONNREFUSED on the qga socket, then the socket file gone, then
    the cfg record gone, all inside 2.4 seconds, and seventy seconds
    BEFORE the fault was filed. Those guests were destroyed, not stuck.

    destroy() pops the name before it sends anything, so this goes false
    at the earliest possible moment. It can also go false and then true
    again when a delete is refused and the object is put back; skipping
    the check in that case loses one observation, which is the right way
    round -- a missed fault costs a run, a false fault costs a night.

    make_lan() has had this guard since the mid-cabling deletes were
    understood ("lan skipped, an end was deleted while cabling"). The
    agent waits never got it.
    """
    with g_lock:
        return name in g_model

def make_cnt():
    name = new_name("c")
    eths = random.choice([1, 2, 2, 3, 4])
    want_systemd = False
    if MAX_LIVE_SYSTEMD_CNT > 0:
        with g_lock:
            live_systemd = len([1 for o in g_model.values()
                                if o.get("systemd")])
        if live_systemd < MAX_LIVE_SYSTEMD_CNT:
            want_systemd = True
    if want_systemd:
        adding(name)
        r = cli("add", "cnt", name, "eth=%d" % eths, g_rootfs,
                "startup_env=CLOONIX_WITH_SYSTEMD=1")
    else:
        # Nothing to pass: no systemd IS the default.
        adding(name)
        r = cli("add", "cnt", name, "eth=%d" % eths, g_rootfs)
    if r.returncode:
        why("cnt", r, name)
        return
    with g_lock:
        g_model[name] = {"kind": "cnt", "born": time.time(),
                         "life": random.uniform(LIFE_MIN, LIFE_MAX),
                         "eths": eths, "used": set(), "lans": set(),
                         "systemd": want_systemd}
    note("cnt created with systemd" if want_systemd else "cnt created")
    if (not wait_present(name)) and (not g_stop.is_set()):
        fault("a container was created and never appeared",
              "%s: the command returned 0. %s" % (name, evidence(name, "cnt")))
        return
    if AGENT:
        end = time.time() + AGENT_WAIT
        started = time.time()
        seen_running = False
        while (time.time() < end) and (not g_stop.is_set()):
            if not still_ours(name):
                note("cnt agent wait dropped, deleted while waiting")
                return
            if agent_ping_ok(name):
                note("agent up")
                return
            st = container_crun_state(name)
            if st == "running":
                seen_running = True
            elif seen_running and (st in ("stopped", "unknown")):
                if not still_ours(name):
                    note("cnt agent wait dropped, deleted while waiting")
                    return
                where = container_death_evidence(name)
                with g_lock:
                    had_systemd = g_model.get(name, {}).get("systemd", False)
                fault("a container died seconds after it started",
                      "%s: crun said running and now says %s, %ds after the "
                      "add, and it was NOT deleted by this test. It was "
                      "started %s. %s Its logs, config and the scope journal "
                      "were read while the rootfs was still mounted: %s"
                      % (name, st, int(time.time() - started),
                         "WITH systemd" if had_systemd
                         else "without systemd (the default)",
                         container_scope_lifetime(name),
                         where if where else "(could not be written)"))
                return
            elif (st == "unknown") and (time.time() - started > 8):
                # Never seen running, and crun still does not know it after
                # eight seconds.
                if container_scope_ran(name) and (not container_scope_alive(name)):
                    where = container_death_evidence(name)
                    fault("a container ran and left no crun state behind",
                          "%s: systemd started and stopped crun-%s_%s.scope, "
                          "so the init ran and exited, while crun never had "
                          "a status file for it. Caught %ds after the add, "
                          "rootfs still mounted: %s"
                          % (name, NET, name, int(time.time() - started),
                             where if where else "(could not be written)"))
                    return
            time.sleep(2)
        if g_stop.is_set():
            return
        if not still_ours(name):
            note("cnt agent wait dropped, deleted while waiting")
            return
        st = container_crun_state(name)
        where = container_death_evidence(name)
        if seen_running or st == "running":
            fault("the agent inside a container never answered",
                  "%s: created, in lst, and crun says RUNNING, but ping_ok "
                  "stayed 0 for %ds -- this is what leaves it dark in the "
                  "web gui. Its logs and config: %s"
                  % (name, int(AGENT_WAIT),
                     where if where else "(could not be written)"))
        elif container_scope_ran(name):
            # crun does not know it, but systemd watched it run.
            fault("a container ran and left no crun state behind",
                  "%s: systemd started crun-%s_%s.scope, so the init ran, but "
                  "crun state reads '%s' and ping_ok stayed 0 for %ds. The "
                  "scope journal and its logs: %s"
                  % (name, NET, name, st, int(AGENT_WAIT),
                     where if where else "(could not be written)"))
        elif st == "cannot-ask":
            fault("a container failed and could not be diagnosed",
                  "%s: ping_ok stayed 0 for %ds, and neither crun state nor "
                  "the systemd journal could be read from this process "
                  "(uid %d). Nothing here says what the container did. Run "
                  "the soak as root or with passwordless sudo. %s"
                  % (name, int(AGENT_WAIT), os.geteuid(),
                     where if where else ""))
        else:
            fault("a container never reached crun at all",
                  "%s: in lst for %ds, crun says '%s' and systemd never "
                  "started a scope for it, so the switch accepted it and "
                  "nothing was ever built. Its config: %s"
                  % (name, int(AGENT_WAIT), st,
                     where if where else "(could not be written)"))

# SOAK_SNF_KILL: does a capture come back after its snf process dies?
SNF_KILL_EVERY = int(os.environ.get("SOAK_SNF_KILL", "0"))

def snf_processes():
    """Every running snf of this net: (pid, tap, name, num).

    Its argv carries all of it -- net, root, tap, "<name>_<num>" -- which
    is why this needs no bookkeeping of its own.
    """
    out = []
    try:
        r = subprocess.run(["pgrep", "-a", "cloonix-ovs-snf"],
                           capture_output=True, text=True, timeout=10)
    except (OSError, subprocess.SubprocessError):
        return out
    for line in r.stdout.split("\n"):
        f = line.split()
        if len(f) < 6:
            continue
        if f[2] != NET:
            continue
        tap = f[4]
        name_num = f[5]
        if "_" not in name_num:
            continue
        name, _, num = name_num.rpartition("_")
        if not num.isdigit():
            continue
        out.append((int(f[0]), tap, name, int(num)))
    return out

def snf_kill_and_check():
    """Kill one snf, then ask for the capture back the documented way.

    The verdict has to survive the soak deleting the object underneath it.
    Eight workers are destroying things continuously, and the first version
    of this test picked an snf at random, killed it, and filed a FAULT when
    "cnf sniff=on" was refused -- without ever asking whether the object was
    still there to be refused FOR. It reported one, and the object had been
    deleted in the three seconds in between: a real refusal, a correct one,
    and not the bug.

    So presence is checked at every step, and the failing case is only
    called a fault when the object is demonstrably still there at the
    moment the request is refused.
    """
    procs = snf_processes()
    if not procs:
        return
    with g_lock:
        alive = set(g_model)
    candidates = [p for p in procs if p[2] in alive]
    if not candidates:
        note("snf kill test", "no snf belongs to a live object just now")
        return
    pid, tap, name, num = random.choice(candidates)

    lst = cli("dump", "commands").stdout
    if not in_lst(lst, name):
        note("snf kill test", "%s went before the test could start" % name)
        return

    note("snf kill test", "killing the snf %s of %s %d (pid %d)"
         % (tap, name, num, pid))
    r = subprocess.run(priv(["kill", "-9", str(pid)]),
                       capture_output=True, text=True)
    if r.returncode:
        note("snf kill test", "could not kill pid %d: %s -- test skipped"
             % (pid, (r.stderr or r.stdout).strip()))
        return

    # The symptom: the process gone, the eth still sniffable.
    time.sleep(3)
    if any(p[0] == pid for p in snf_processes()):
        note("snf kill test", "%s survived the kill, test skipped" % tap)
        return
    if not in_lst(cli("dump", "commands").stdout, name):
        note("snf kill test",
             "%s was deleted while its snf was being killed, nothing to "
             "check" % name)
        return

    r = cli("cnf", name, *(itf(name, num) + ["sniff=on"]))
    time.sleep(4)
    back = [p for p in snf_processes() if p[2] == name and p[3] == num]
    if back:
        note("snf kill test", "%s %d: the capture came back on 'snf on'"
             % (name, num))
        return

    # Not back after four seconds.
    time.sleep(4)
    back = [p for p in snf_processes() if p[2] == name and p[3] == num]
    if back:
        note("snf kill test",
             "%s %d: the capture came back, but only between four and "
             "eight seconds after 'snf on' -- slower than this test's "
             "first look" % (name, num))
        return

    lst = cli("dump", "commands").stdout
    if not in_lst(lst, name):
        note("snf kill test",
             "%s was deleted before the capture could be asked back "
             "(cnf sniff=on returned %d), which is not a fault"
             % (name, r.returncode))
        return
    if r.returncode not in (0, 255):
        fault("the snf test cannot run: its command was rejected",
              "'cloonix cli %s cnf %s %s sniff=on' exited %d without reaching the "
              "server. That is a fault in this test, not in cloonix: the "
              "command line is wrong or the wrapper has changed. Output: %s"
              % (NET, name, " ".join(itf(name, num)), r.returncode,
                 (r.stdout + r.stderr).strip()[:200] or "(nothing)"))
        return
    try:
        raw = subprocess.run(["pgrep", "-a", "cloonix-ovs-snf"],
                             capture_output=True, text=True, timeout=10)
        mine = [l.strip() for l in raw.stdout.split("\n") if NET in l]
        hit = [l for l in mine if (" %s " % tap) in l or l.endswith(tap)]
        if hit:
            seen = ("the tap IS running and this test failed to match it: "
                    "%s" % hit[0][:160])
        else:
            seen = ("the tap is NOT among the %d snf processes of this net"
                    % len(mine))
    except (OSError, subprocess.SubprocessError):
        seen = "(pgrep failed)"
    where = [l for l in lst.split("\n") if name in l][:2]
    fault("a capture did not come back after its snf was killed",
          "%s %d is STILL in lst and its capture is gone: its snf %s (pid "
          "%d) was killed, 'cloonix cli %s cnf %s %s sniff=on' returned %d, and no "
          "snf process for it was there four seconds later nor eight. "
          "Whether the switch started one is in its log, beside this "
          "timestamp. lst says: %s. Process check: %s"
          % (name, num, tap, pid, NET, name, " ".join(itf(name, num)),
             r.returncode,
             " | ".join(x.strip() for x in where) if where else "(nothing)",
             seen))

def priv(argv):
    """Run argv with privilege if this process does not already have it.

    Most of what says WHY a container died is root-only: crun's state
    directory, the systemd journal, dmesg. Run as an ordinary user, every
    one of those probes comes back "Permission denied" and an evidence
    file is produced that proves nothing -- which is exactly what happened
    the first time this collector fired.

    sudo -n so it never blocks on a password prompt in the middle of a
    run. When sudo is not available the command runs unprivileged and
    fails visibly, which is better than not running it at all: the failure
    text in the file is what tells the reader the collection was crippled.
    """
    if os.geteuid() == 0:
        return argv
    return ["sudo", "-n"] + argv

g_priv_ok = None

def priv_available():
    """Can this process read root-only things? Asked once and remembered."""
    global g_priv_ok
    if g_priv_ok is not None:
        return g_priv_ok
    if os.geteuid() == 0:
        g_priv_ok = True
        return True
    try:
        r = subprocess.run(["sudo", "-n", "true"],
                           capture_output=True, text=True, timeout=20)
        g_priv_ok = (r.returncode == 0)
    except (OSError, subprocess.SubprocessError):
        g_priv_ok = False
    return g_priv_ok

def container_scope_lifetime(name):
    """How long systemd saw the container's scope alive, as a short phrase.

    Put in the fault line itself because it is the one number that says
    what kind of failure this is without opening any file. Every early
    death seen so far has lasted about 1.3 seconds; a container killed by
    this test's own random delete will show whatever age it had reached.
    Having it in the summary means a run's faults can be sorted by cause
    at a glance instead of one evidence file at a time.
    """
    try:
        r = subprocess.run(priv(["systemctl", "show", "crun-%s_%s.scope" % (NET, name),
                                 "--property=ActiveEnterTimestampMonotonic",
                                 "--property=InactiveEnterTimestampMonotonic",
                                 "--property=Result",
                                 "--property=ExecMainStatus"]),
                           capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.SubprocessError):
        return ""
    vals = {}
    for line in (r.stdout or "").split("\n"):
        if "=" in line:
            k, v = line.split("=", 1)
            vals[k.strip()] = v.strip()
    try:
        a = int(vals.get("ActiveEnterTimestampMonotonic", "0"))
        i = int(vals.get("InactiveEnterTimestampMonotonic", "0"))
    except ValueError:
        a = i = 0
    bits = []
    if a and i and i > a:
        bits.append("the scope lived %.2fs" % ((i - a) / 1000000.0))
    else:
        bits.append(scope_lifetime_from_journal(name))
    if vals.get("Result") and vals["Result"] not in ("success", ""):
        bits.append("result=%s" % vals["Result"])
    if vals.get("ExecMainStatus") not in (None, "", "0"):
        bits.append("exit status %s" % vals["ExecMainStatus"])
    bits = [b for b in bits if b]
    return ("(" + ", ".join(bits) + ")") if bits else ""

def scope_lifetime_from_journal(name):
    """Lifetime from the Started/Deactivated pair the journal keeps."""
    try:
        r = subprocess.run(priv(["journalctl", "--no-pager",
                                 "-o", "short-unix",
                                 "-u", "crun-%s_%s.scope" % (NET, name), "-n", "40"]),
                           capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.SubprocessError):
        return ""
    started = stopped = None
    for line in (r.stdout or "").split("\n"):
        tok = line.split(" ", 1)[0]
        try:
            when = float(tok)
        except ValueError:
            continue
        if "Started" in line and started is None:
            started = when
        elif "Deactivated" in line or "Stopped" in line:
            stopped = when
    if (started is not None) and (stopped is not None) and stopped > started:
        return "the scope lived %.2fs" % (stopped - started)
    if started is not None:
        return "the scope started and the journal shows no stop"
    return ""

def container_scope_ran(name):
    """Did systemd ever start a scope for this container?

    crun's own state is not enough to tell "never built" from "built, ran,
    and left nothing behind". A container that dies early can end up with
    NO crun status file at all -- "crun state" then says the container does
    not exist, which reads identically to never having been created.

    systemd knows better: it logs "Started crun-<net>_<name>.scope" when the
    container's init actually ran, and that record survives the container.
    A run showed exactly this -- crun said unknown for the full 45 seconds
    while the journal had the scope starting and deactivating 1.37 seconds
    later.
    """
    try:
        r = subprocess.run(priv(["journalctl", "--no-pager", "-o", "cat",
                                 "-u", "crun-%s_%s.scope" % (NET, name), "-n", "20"]),
                           capture_output=True, text=True, timeout=30)
        return "Started" in (r.stdout or "")
    except (OSError, subprocess.SubprocessError):
        return False

def container_scope_alive(name):
    """Is the scope still active? Used only to confirm it has ENDED.

    Deliberately conservative in the same direction as everything else
    here: anything it cannot read counts as still alive, so an unreadable
    answer never manufactures a death.
    """
    try:
        r = subprocess.run(priv(["systemctl", "is-active",
                                 "crun-%s_%s.scope" % (NET, name)]),
                           capture_output=True, text=True, timeout=20)
        return "inactive" not in (r.stdout or "") and \
               "failed" not in (r.stdout or "")
    except (OSError, subprocess.SubprocessError):
        return True

def container_crun_state(name):
    """"running", "stopped", or "unknown" -- and unknown is NOT dead.

    The distinction is the whole point. "cloonix cli <net> add cnt" returns, and
    the object appears in lst, BEFORE crun has created anything: the switch
    accepts the request and does the work behind it. So for the first
    seconds of a perfectly healthy container, crun has never heard of the
    name and "crun state" exits non-zero.

    An earlier version folded that into a boolean and read non-zero as
    gone. It reported 24 dead containers in the first minute of a run, all
    of them alive -- the syslog has "Started crun-<net>_<name>.scope" for each
    and no deactivation. A name crun does not know yet and a name crun has
    finished with look identical from here, so the caller has to separate
    them by having seen the container running first.

    Anything unreadable is "cannot-ask", never "stopped" and never
    "unknown": those two drive the classification, and a permission error
    is not evidence about the container at all. That distinction was
    missing and it cost a wrong diagnosis -- run as an ordinary user every
    call returned Permission denied, was folded into "unknown", and a
    container was reported as never having reached crun when the truth was
    only that this process could not look.
    """
    try:
        r = subprocess.run(priv([CRUN_BIN,
                                 "--root=%s/crun" % lib_cloonix(NET),
                                 "state", name]),
                           capture_output=True, text=True, timeout=20)
    except (OSError, subprocess.SubprocessError):
        return "cannot-ask"
    both = (r.stdout or "") + (r.stderr or "")
    if r.returncode:
        if ("Permission denied" in both) or ("ermission" in both) or \
           ("sudo:" in both):
            return "cannot-ask"
        if "No such file" in both or "does not exist" in both:
            return "unknown"
        return "cannot-ask"
    if "stopped" in r.stdout:
        return "stopped"
    if "running" in r.stdout:
        return "running"
    return "cannot-ask"

def container_death_evidence(name):
    """Everything about a dead container, while it is still on the machine.

    Written because a container that exits about 1.3 seconds after start --
    four times in 21000 operations -- could not be explained from the host
    side. crun creates it cleanly, the interfaces are renamed, the cgroup
    manager is right, and then its init leaves. Whatever it says on the way
    out is INSIDE the container, and nothing was capturing it.

    The window is real but short. The scope deactivates when the init dies;
    the rootfs and tmpfs stay mounted until the delete, about a minute
    later. In that gap the container's own /var/log is readable from the
    host, and after it there is nothing left to read at all.

    One file per container, so a run leaves a set of them rather than one
    that each failure overwrites.
    """
    out = ["container %s died early, collected %s" % (name, time.ctime()),
           "net=%s" % NET,
           "running as uid %d, privileged probes %s"
           % (os.geteuid(),
              "available" if priv_available() else
              "NOT AVAILABLE -- crun state, the journal and dmesg below are "
              "expected to fail, and this file will not explain anything. "
              "Run the soak as root, or give it passwordless sudo."),
           ""]

    def add(title, argv, timeout=30):
        out.append("=== %s" % title)
        try:
            r = subprocess.run(argv, capture_output=True, text=True,
                               timeout=timeout)
            txt = ((r.stdout or "") + (r.stderr or "")).strip()
            out.append(txt if txt else "(nothing)")
        except (OSError, subprocess.SubprocessError) as e:
            out.append("(could not run: %s)" % e)
        out.append("")

    def addp(title, argv, timeout=30):
        """Same, but with privilege -- for anything root owns."""
        add(title, priv(argv), timeout)

    def addsh(title, script, timeout=30):
        add(title, priv(["sh", "-c", script]), timeout)

    croot = "%s/crun" % lib_cloonix(NET)
    cdir = "%s/cnt/%s" % (lib_cloonix(NET), name)
    clog = "%s/log/debug_crun.log" % lib_cloonix(NET)

    addp("crun state", [CRUN_BIN, "--root=" + croot, "state", name])
    addp("crun list", [CRUN_BIN, "--root=" + croot, "list"])
    addsh("crun state directory", "ls -la %s/%s/ 2>&1" % (croot, name))
    addsh("crun status file", "cat %s/%s/status 2>&1" % (croot, name))
    addsh("crun log lines for this name",
          "grep -F %s %s | tail -40" % (name, clog))
    addsh("crun log around the LAST mention of this name",
          "grep -n -F %s %s | tail -1 | cut -d: -f1 | xargs -I@ sh -c "
          "'sed -n \"$((@-25)),$((@+25))p\" %s' 2>&1" % (name, clog, clog))

    addsh("config.json process section",
          "sed -n '/\"process\"/,/^  }/p' %s/%s/config.json 2>&1"
          % (croot, name))

    # systemd's view of the scope, which carries an exit status the syslog
    # line does not.
    addp("journal for the scope",
         ["journalctl", "--no-pager", "-o", "short-precise",
          "-u", "crun-%s_%s.scope" % (NET, name), "-n", "80"])
    addp("systemd scope properties",
         ["systemctl", "show", "crun-%s_%s.scope" % (NET, name),
          "--property=Result,ActiveState,SubState,ExecMainStatus,"
          "ExecMainCode,ActiveEnterTimestamp,InactiveEnterTimestamp"])

    addsh("is the rootfs still mounted",
          "mount | grep -F %s || echo NO-LONGER-MOUNTED" % name)
    addsh("rootfs /var/log listing",
          "ls -la %s/rootfs/var/log/ 2>&1 | head -40" % cdir)
    for f in ("var/log/messages", "var/log/syslog", "var/log/boot.log",
              "var/log/cloonix_agent.log", "var/log/frr/frr.log"):
        addsh("rootfs %s (tail)" % f,
              "tail -60 %s/rootfs/%s 2>&1" % (cdir, f))
    addsh("rootfs journal, if systemd wrote one",
          "journalctl --no-pager -D %s/rootfs/var/log/journal -n 100 2>&1"
          % cdir)
    addsh("does the init exist in the rootfs",
          "ls -la %s/rootfs/sbin/init %s/rootfs/usr/sbin/init "
          "%s/rootfs/usr/lib/systemd/systemd 2>&1" % (cdir, cdir, cdir))
    addsh("cloonixsbininit log (the container's own account)",
          "cat %s/tmp*/cloonix_sbin_init.log 2>&1 | tail -60" % cdir)
    addsh("cloonixmnt directories",
          "ls -la %s/mnt*/ %s/tmp*/ 2>&1 | head -40" % (cdir, cdir))

    addsh("inotify limits and usage",
          "for f in /proc/sys/fs/inotify/max_user_instances "
          "/proc/sys/fs/inotify/max_user_watches; do "
          "echo \"$f = $(cat $f)\"; done; "
          "echo \"inotify fds open across all processes: "
          "$(ls -l /proc/*/fd/ 2>/dev/null | grep -c inotify)\"")

    addsh("dmesg tail", "dmesg | tail -40")
    add("free memory", ["free", "-m"])

    path = "/tmp/soak_cnt_death_%s.txt" % name
    try:
        with open(path, "w") as f:
            f.write("\n".join(out))
        return path
    except OSError:
        return ""

def make_simple(kind):
    """tap, nat and a2b: one command, one name, nothing else needed."""
    name = new_name(kind[0])
    adding(name)
    r = cli("add", kind, name)
    if r.returncode:
        why(kind, r)
        return
    with g_lock:
        g_model[name] = {"kind": kind, "born": time.time(),
                         "life": random.uniform(LIFE_MIN, LIFE_MAX),
                         "eths": 2 if kind == "a2b" else 1,
                         "used": set(), "lans": set()}
    note("%s created" % kind)
    if (not wait_present(name)) and (not g_stop.is_set()):
        fault("a %s was created and never appeared" % kind,
              "%s: the command returned 0. %s" % (name, evidence(name, kind)))

def make_kvm():
    if not g_kvm_image:
        return
    if MAX_LIVE_KVM:
        with g_lock:
            live = len([1 for o in g_model.values() if o["kind"] == "kvm"])
            live += len([1 for n in g_deleting
                         if n[len(PREFIX):len(PREFIX)+1] == "k"])
        if live >= MAX_LIVE_KVM:
            note("kvm skipped, at the kvm cap")
            return
    name = new_name("k")
    image = random.choice(g_kvm_image)
    eths = random.choice([1, 2, 2, 3, 4])
    eth_desc = "".join(random.choice("sv") for _ in range(eths))
    # windows is the ide rootfs, the driver disc and the bat launcher; --uefi
    # is OVMF, which a GPT microsoft disk cannot boot without.
    win = is_win_image(image)
    extra = ["--windows", "--uefi"] if win else []
    adding(name)
    # The server answers add kvm when the qemu process is up, 20 s at most;
    # the agent inside is waited for below, on its own clock.
    r = cli("add", "kvm", name, "ram=%d" % KVM_RAM, "cpu=%d" % KVM_CPU,
            "eth=%d" % eths, *extra, image)
    if r.returncode:
        why("kvm", r, name)
        return
    # eths are born not sniffed: the ones drawn s are made so
    for i, ch in enumerate(eth_desc):
        if ch == "s":
            r = cli("cnf", name, "eth%d" % i, "sniff=on")
            if r.returncode:
                why("kvm", r, name)
    with g_lock:
        g_model[name] = {"kind": "kvm", "born": time.time(),
                         # long enough to boot and answer, when we wait
                         "life": (random.uniform(
                                      (AGENT_WAIT_WIN if win
                                       else AGENT_WAIT_KVM) + 30,
                                      (AGENT_WAIT_WIN if win
                                       else AGENT_WAIT_KVM) + 120)
                                  if AGENT else
                                  random.uniform(30.0, LIFE_MAX)),
                         "eths": eths, "used": set(), "lans": set()}
    note("kvm created")
    if AGENT:
        wait = AGENT_WAIT_WIN if win else AGENT_WAIT_KVM
        end = time.time() + wait
        while (time.time() < end) and (not g_stop.is_set()):
            if not still_ours(name):
                note("kvm agent wait dropped, deleted while waiting")
                return
            if agent_ping_ok(name):
                note("kvm agent up")
                # the boot time under this load, for the eye: a fault is
                # one that crossed the wait, and this says how close the
                # others came to it
                with g_lock:
                    n_live = len(g_model)
                print("    %s agent up after %ds with %d objects live"
                      % (name, int(time.time() - end + wait), n_live))
                return
            time.sleep(2)
        if g_stop.is_set():
            return
        if not still_ours(name):
            note("kvm agent wait dropped, deleted while waiting")
            return
        fault("the agent inside a kvm never answered",
              "%s (image %s%s): alive throughout, ping_ok stayed 0 for %ds. "
              "The switch logs where it stopped -- grep the syslog for '%s'"
              % (name, image, ", added as windows" if win else "",
                 int(wait), name))

def count_c2c_locked():
    """Live c2c, counting the deletes and the creates still in flight.

    Called with g_lock held.

    destroy() drops the name from g_model before the far end has gone, and
    a c2c takes a moment to unwind on both nets, so g_deleting counts too:
    a replacement started against a slot the old one still holds is exactly
    what the count is there to prevent.

    g_c2c_pending is the other half of it. Eight workers all reading the
    count before any of them has created anything would every one of them
    decide a c2c was needed, and the target would be overshot by seven --
    which matters here more than it does for kvm, because proxymous serves
    a bounded number of udp proxies and the point of the exercise is to sit
    just under that bound rather than to trip it.
    """
    live = len([1 for o in g_model.values() if o["kind"] == "c2c"])
    live += len([1 for n in g_deleting
                 if n[len(PREFIX):len(PREFIX)+1] == "x"])
    return live + g_c2c_pending

def live_c2c():
    with g_lock:
        return count_c2c_locked()

def make_c2c():
    """ONE c2c, on this side only. The far end creates itself.

    This used to run "add c2c <name>" on BOTH nets, and that is not how a
    c2c works. From the manual: "A c2c goes in pairs, master and slave.
    The master is the one given the name of the distant cloonix; it
    initiates the connection." One command builds both ends -- the master
    sends a peer_create over tcp and the far switch allocates its slave
    from it.

    Issuing the command on both sides produced two MASTERS and no slave,
    which the protocol has no way to settle. Each master allocates with
    ref_tid left at zero (alloc_c2c_master never sets it; only the slave
    path does, from the tid of the create it received), then each fills in
    a ref_tid from its own peer traffic -- two independent counters on two
    machines. Every ping then carries the sender's ref_tid and is compared
    against the receiver's, so the two can only ever agree by coincidence:
    the log shows nemo holding 13 and receiving 12 while mito holds 12 and
    receives 13, at the same instant. Each destroys the other, each
    restarts because nobody asked for the delete, and the pair spins --
    one object restarted 24 times, 2500 log lines across the run.

    So the far side is no longer created here, and no longer tracked for
    cleanup: deleting the master takes the slave with it.
    """
    # The place is taken under the lock before the command is sent, and given
    # back on every way out.
    global g_c2c_pending
    if C2C_TARGET:
        with g_lock:
            if count_c2c_locked() >= C2C_TARGET:
                note("c2c skipped, at the c2c target")
                return
            g_c2c_pending += 1
    try:
        name = new_name("x")
        adding(name)
        r = cli("add", "c2c", name, PEER)
        if r.returncode:
            why("c2c", r, name)
            return
        with g_lock:
            g_model[name] = {"kind": "c2c", "born": time.time(),
                             "life": random.uniform(LIFE_MIN, LIFE_MAX),
                             "eths": 1, "used": set(), "lans": set()}
    finally:
        if C2C_TARGET:
            with g_lock:
                g_c2c_pending -= 1
    note("c2c created")
    if not wait_present(name):
        fault("a c2c was created and never appeared",
              "%s: the add returned 0. %s" % (name, evidence(name, "c2c")))

def itf(name, num):
    """The words that name an interface: eth<i> of a cnt or kvm, A or B of
    an a2b, none for the one interface of a tap, nat or c2c."""
    kind = (g_model.get(name) or {}).get("kind")
    if kind in ("cnt", "kvm"):
        return ["eth%d" % num]
    if kind == "a2b":
        return ["AB"[num]]
    return []


def free_endpoint():
    """An object with an interface nothing is plugged into.

    The pick is a RESERVATION, made under the lock.

    This used to choose a candidate and let the lock go without marking
    anything, so two workers could pick the same (object, eth) and both
    cable it. The switch takes its "BAD STATE" branch for the second and
    never registers it as a user of the lan -- the count stays at one, and
    when the first end leaves the lan is torn down with the second still
    attached. That is what "the server has lost lans it accepted" was:
    skc898302 eth 1 cabled to skl898323 and skl898325 sixteen milliseconds
    apart, by two workers that had both been told the eth was free.

    Marking it here closes the window. release_endpoint() puts it back if
    the cabling does not happen.
    """
    with g_lock:
        cands = []
        for name, o in g_model.items():
            for e in range(o["eths"]):
                if e not in o["used"]:
                    cands.append((name, e))
        if not cands:
            return None
        pick = random.choice(cands)
        g_model[pick[0]]["used"].add(pick[1])
        return pick

def release_endpoint(end):
    """Give a reserved endpoint back, when the cable did not happen."""
    if not end:
        return
    with g_lock:
        o = g_model.get(end[0])
        if o:
            o["used"].discard(end[1])

def make_lan():
    a = free_endpoint()
    b = free_endpoint()
    if not a or not b or a[0] == b[0]:
        release_endpoint(a)
        release_endpoint(b)
        return
    lan = new_name("l")
    done = []
    for item, eth in (a, b):
        r = cli("plug", item, *(itf(item, eth) + [lan]))
        if r.returncode == 0:
            done.append((item, eth))
    if len(done) != 2:
        for item, eth in done:
            cli("unplug", item, *(itf(item, eth) + [lan]))
        release_endpoint(a)
        release_endpoint(b)
        note("lan refused" if not done else "lan refused, half undone")
        return
    with g_lock:
        g_lans[lan] = [a, b]
        for item, eth in (a, b):
            if item in g_model:
                # already reserved by free_endpoint; add() again is harmless
                g_model[item]["used"].add(eth)
                g_model[item]["lans"].add(lan)
    note("lan created")
    with g_lock:
        both_alive = (a[0] in g_model) and (b[0] in g_model)
    if not both_alive:
        note("lan skipped, an end was deleted while cabling")
        return
    # A c2c end is pending, not missing.
    with g_lock:
        via_c2c = any(g_model.get(i, {}).get("kind") == "c2c"
                      for i in (a[0], b[0]))
    if via_c2c:
        note("lan to a c2c, pending until the peer connects")
        return

    # Both ends, not just the name -- and both still alive AFTERWARDS.
    if not wait_cabled(lan):
        with g_lock:
            still_alive = (a[0] in g_model) and (b[0] in g_model)
        if not still_alive:
            note("lan skipped, an end was deleted while cabling")
            return
        seen = count_in_lst(lst(), lan)
        fault("a lan was created and is not fully cabled",
              "%s between %s eth%d and %s eth%d: both commands returned 0, "
              "lst shows %d of the 2 ends. %s"
              % (lan, a[0], a[1], b[0], b[1], seen, evidence(lan, "lan")))

def do_snf():
    """Sniffing on and off, which is a state change rather than an object."""
    with g_lock:
        cnts = [n for n, o in g_model.items() if o["kind"] == "cnt"]
    if not cnts:
        return
    name = random.choice(cnts)
    with g_lock:
        if name not in g_model:
            return
        eth = random.randrange(g_model[name]["eths"])
    val = random.choice(["off", "on"])
    r = cli("cnf", name, *(itf(name, eth) + ["sniff=" + val]))
    note("sniff %s%s" % (val, "" if r.returncode == 0 else " refused"))

LEAK_GRACE = float(os.environ.get("SOAK_LEAK_GRACE", "45"))

g_deleted_watch = []

def watch_for_leaks(name):
    """Remember to check this name's processes once its teardown is over."""
    if LEAK_GRACE <= 0:
        return
    with g_lock:
        g_deleted_watch.append((time.time() + LEAK_GRACE, name))

def sweep_deleted_for_leaks():
    """One ps for every name whose grace period is up."""
    now = time.time()
    with g_lock:
        due = [n for (t, n) in g_deleted_watch if t <= now]
        if due:
            g_deleted_watch[:] = [(t, n) for (t, n) in g_deleted_watch
                                  if t > now]
    if not due:
        return
    try:
        r = subprocess.run(["ps", "-eo", "pid,args"], capture_output=True,
                           text=True, timeout=30)
    except (OSError, subprocess.SubprocessError):
        return
    lines = [l for l in r.stdout.split("\n")
             if "cloonix" in l and "soak.py" not in l]
    for name in due:
        hits = [l.strip()[:110] for l in lines
                if re.search(r"(?<![a-zA-Z0-9])%s(?![a-zA-Z0-9])"
                             % re.escape(name), l)]
        if hits:
            fault("a deleted object still has processes",
                  "%s was deleted %ds ago and %d cloonix process(es) still "
                  "name it: %s"
                  % (name, int(LEAK_GRACE), len(hits), " | ".join(hits[:3])))

def destroy(name):
    with g_lock:
        o = g_model.pop(name, None)
        if not o:
            return
        # From here the name is out of the model but the server may still be
        # showing it, and that is not a fault until CHECK_VANISH is up.
        g_deleting[name] = time.time()
        # Deleting one end of a cable does not delete the cable.
        for lan in list(o["lans"]):
            ends = g_lans.get(lan, [])
            rest = [(i, e) for (i, e) in ends if i != name]
            for item, eth in ends:
                if item == name:
                    continue
            if rest:
                g_lans[lan] = rest
            else:
                g_lans.pop(lan, None)
    r = cli("del", name)
    if not r.returncode:
        # Accepted: check later that its processes went with it.
        watch_for_leaks(name)
    if r.returncode:
        # "Not ready" is worth retrying.
        gone = not in_lst(lst(), name)
        with g_lock:
            g_deleting.pop(name, None)
            if (not gone) and (name not in g_model):
                o["born"] = time.time()
                o["life"] = 0.0          # the reaper retries it at once
                g_model[name] = o
        if gone:
            # Gone without being deleted: a fault of its own.
            note("delete refused but the object is gone anyway")
            fault("an object disappeared without being deleted",
                  "%s: 'del' was refused and it is not in lst either, "
                  "so it went away by itself. The switch logs why -- grep "
                  "the syslog for '%s'" % (name, name))
            return
        note("delete refused, keeping it to retry")
        why("%s delete" % o.get("kind", "obj"), r)
        return
    if (o.get("kind") == "c2c") and PEER:
        try:
            r = subprocess.run(CLOONIX + ["cli", PEER, "dump", "commands"],
                               capture_output=True, text=True, timeout=90)
            if name in (r.stdout or ""):
                time.sleep(5)
                r = subprocess.run(CLOONIX + ["cli", PEER, "dump", "commands"],
                                   capture_output=True, text=True, timeout=90)
                if name in (r.stdout or ""):
                    fault("a c2c slave outlived its master",
                          "%s: the master was deleted in %s and the slave is "
                          "still in %s five seconds later. Deleting the "
                          "master is supposed to take the slave with it."
                          % (name, NET, PEER))
                    subprocess.run(CLOONIX + ["cli", PEER, "del",
                                    name],
                                   capture_output=True, text=True, timeout=90)
        except (OSError, subprocess.SubprocessError):
            note("c2c far-end check never returned")
    note("deleted")
    vanished = wait_absent(name)
    # Cleared on every path out, refused or not, so nothing is left marked as
    # deleting for ever and quietly exempted from the ghost check.
    with g_lock:
        g_deleting.pop(name, None)
    if not vanished:
        if g_stop.is_set():
            note("delete still in flight when the run stopped")
            return
        fault("an object was deleted and is still there",
              "%s: del returned and lst still shows it after %ds. %s"
              % (name, int(CHECK_VANISH), evidence(name)))

def reap():
    """Delete whatever has outlived its allotted time."""
    now = time.time()
    with g_lock:
        due = [n for n, o in g_model.items() if now - o["born"] > o["life"]]
    for name in due:
        if g_stop.is_set():
            return
        destroy(name)

def confirm(names, want_present):
    """Look again, a moment later, before calling a difference a fault.

    Workers create and delete continuously, so a snapshot of the model and
    a read of lst are never taken at the same instant: something deleted
    between the two looks like an object the server invented, and something
    created between them looks like one it lost. Both are noise, and noise
    in a fault report is worse than no report at all.
    """
    time.sleep(3)
    text = lst()
    with g_lock:
        known = set(g_model.keys()) | set(g_lans.keys())
    out = []
    for n in names:
        there = in_lst(text, n)
        if want_present and (n in known) and not there:
            out.append(n)
        elif (not want_present) and (n not in known) and there:
            out.append(n)
    return out

def log_line_epoch(line):
    """When was this syslog line written, or None if it cannot be told.

    Two formats turn up: rsyslog's ISO stamp with an offset
    ("2026-08-11T03:08:27.205842+02:00 host ...") and journalctl's short
    form ("Aug 11 03:08:27 host ..."), which carries no year.
    """
    tok = line.split(" ", 1)[0]
    try:
        return datetime.fromisoformat(tok).timestamp()
    except (ValueError, TypeError):
        pass
    parts = line.split()
    if len(parts) >= 3:
        try:
            now = datetime.now()
            d = datetime.strptime(" ".join(parts[:3]), "%b %d %H:%M:%S")
            d = d.replace(year=now.year)
            if d.timestamp() > now.timestamp() + 86400:
                d = d.replace(year=now.year - 1)
            return d.timestamp()
        except (ValueError, TypeError):
            pass
    return None

def death_evidence():
    """Which process actually went away, and what did it say last?

    Named for what it was originally asked to decide and immediately got
    wrong. "cannot reach cloonix at 127.0.0.1:45211" is the CLIENT failing
    to connect, and port 45211 belongs to cloonix-doorways, not to the
    switch -- so a refused connection says doorways is gone or has stopped
    accepting, and says nothing at all about the switch.
    
    It reported the switch as GONE while the switch was logging normally
    thirteen seconds later, which sent a whole round chasing a crash that
    had already been fixed. So every process in the chain is checked and
    named, and the answer is a list rather than a verdict.
    """
    who = []
    for proc, what in (("cloonix-main-server", "the switch"),
                       ("cloonix-doorways", "doorways, which owns the port "
                                            "the client connects to"),
                       ("cloonix-proxymous", "proxymous"),
                       ("cloonix-ovs-drv", "the ovs driver")):
        try:
            r = subprocess.run(["pgrep", "-f", "%s.*%s" % (proc, NET)],
                               capture_output=True, text=True, timeout=20)
            up = bool(r.stdout.strip())
        except (OSError, subprocess.SubprocessError):
            up = False
        who.append("%s: %s" % (proc, "up" if up else "GONE (%s)" % what))
    last = ""
    for cmd in (["cat", host_syslog()],
                priv(["cat", host_syslog()]),
                priv(["journalctl", "--no-pager", "-n", "400"])):
        try:
            # Bytes, not text, and decoded forgivingly.
            r = subprocess.run(cmd, capture_output=True, timeout=30)
            out = (r.stdout or b"").decode("utf-8", errors="replace")
            if r.returncode == 0 and out:
                lines = [l for l in out.split("\n")
                         if ("cloonix" in l or "segfault" in l.lower()
                             or "kernel" in l)]
                cut = g_run_started - 5.0
                fresh, undated = [], []
                for l in lines:
                    when = log_line_epoch(l)
                    if when is None:
                        undated.append(l)
                    elif when >= cut:
                        fresh.append(l)
                triggers = ["CRASH signal=",
                            "SELF DESTRUCTION OF THE WHOLE NET requested by",
                            "SIGCRUN DISCONNECT TO PROXYMOUS",
                            "IMPOSSIBLE CTRL CONNECTION TO PROXYMOUS",
                            "NO HEARTBEAT FROM PROXYMOUS",
                            "IMPOSSIBLE SIGCRUN CONNECTION",
                            "INSTALLATION DIRECTORY IS GONE",
                            "segfault", "Server being killed"]
                found = []
                for t in triggers:
                    hit = [l for l in fresh if t in l]
                    if hit:
                        found.append("%s x%d, last: %s"
                                     % (t, len(hit), hit[-1][:130]))
                    else:
                        hit = [l for l in undated if t in l]
                        if hit:
                            found.append("%s x%d (UNDATED, may be from an "
                                         "earlier run), last: %s"
                                         % (t, len(hit), hit[-1][:130]))
                last = ("  ||  ".join(found) if found
                        else "no known shutdown trigger since this run began")
                offs = []
                for l in fresh:
                    m = re.search(r"CRASH frame \d+: (\S*cloonix-main-server)"
                                  r"\(\+(0x[0-9a-f]+)\)", l)
                    if m and m.group(2) not in offs:
                        offs.append(m.group(2))
                if offs:
                    last += ("\n           name the frames with: addr2line "
                             "-f -C -i -e <build-tree>/cloonix-main-server %s"
                             % " ".join(offs))
                try:
                    with open("/tmp/soak_death.txt", "w") as f:
                        f.write("\n".join((fresh or lines)[-2000:]))
                    last += "   (last 2000 in /tmp/soak_death.txt)"
                except OSError:
                    pass
                break
        except (OSError, subprocess.SubprocessError):
            pass
    bt = ""
    try:
        r = subprocess.run(
            priv(["coredumpctl", "-1", "--no-pager", "debug",
                  "cloonix-main-server",
                  "--debugger-arguments=-batch -ex bt -ex quit"]),
            capture_output=True, text=True, timeout=120)
        out = (r.stdout or "") + (r.stderr or "")
        frames = [l.strip() for l in out.split("\n") if l.strip().startswith("#")]
        if frames:
            bt = " / ".join(frames[:8])
            try:
                with open("/tmp/soak_backtrace.txt", "w") as f:
                    f.write(out)
                bt += "   (full trace in /tmp/soak_backtrace.txt)"
            except OSError:
                pass
    except (OSError, subprocess.SubprocessError):
        pass
    if bt:
        last = "BACKTRACE: " + bt + "  ||  " + last
    elif "segfault" in last:
        last += ("  ||  no core found. Turn them on and rerun: "
                 "ulimit -c unlimited; sudo sysctl -w kernel.core_pattern="
                 "'|/lib/systemd/systemd-coredump %P %u %g %s %t %c %h'")
    if not last:
        last = ("this account cannot read the log. Run:  sudo grep -E "
                "'segfault|cloonix-main-server' %s | tail -20" % host_syslog())
    return who, last

def server_is_answering(text):
    """Has the net stopped talking?

    A run where lst suddenly returns nothing produced "29 of 29 missing
    from lst" and called it a fault about twenty-nine objects. It is one
    fault about one thing: the server went away, and everything after that
    point is noise. Better to say so once and stop than to report the
    symptom thirty times.
    """
    return g_lst_rc == 0

def reconcile():
    """Compare what we believe with what the server says, both ways.

    Two different faults hide here. Something we made that the server has
    lost is the one that has bitten this code base repeatedly. Something the
    server has that we never made is rarer and worse: a delete that removed
    the name and left the object, or an object created twice.
    """
    text = lst()
    with g_lock:
        mine = set(g_model.keys())
        lans = set(g_lans.keys())
    # An empty model is not a reason to stop checking.
    with g_lock:
        made = g_ops
    if (not mine) and made > 20 and not server_is_answering(text):
        fault("the server stopped answering",
              "every object is gone and dump commands exits %d after %d "
              "operations. The net is down and this run has nothing left "
              "to test." % (g_lst_rc, made))
        g_stop.set()
        return
    if mine and not server_is_answering(text):
        refused = ""
        with g_lock:
            for kind in g_reasons:
                for msg in g_reasons[kind]:
                    if "rejected the signature" in msg:
                        refused = msg
        if refused:
            fault("this client cannot authenticate to the net",
                  "the server rejected our signature: %s\n"
                  "           The cloonix_passwd in the client does not "
                  "match the running net." % refused[:90])
            g_stop.set()
            return
        # One empty lst is not an outage.
        recovered = False
        for _ in range(3):
            time.sleep(2)
            if server_is_answering(lst()):
                recovered = True
                break
        if recovered:
            note("lst came back empty once, answered on retry")
            return
        # Before anything else: is it dead or just not answering?
        try:
            r = subprocess.run(["pgrep", "-f",
                                "cloonix-main-server.*%s" % NET],
                               capture_output=True, text=True, timeout=20)
            pid = (r.stdout or "").split()[0] if r.stdout.strip() else ""
            if pid:
                st = subprocess.run(priv(["cat", "/proc/%s/stack" % pid]),
                                    capture_output=True, text=True,
                                    timeout=20)
                ws = subprocess.run(priv(["cat", "/proc/%s/wchan" % pid]),
                                    capture_output=True, text=True,
                                    timeout=20)
                with open("/tmp/soak_wedge.txt", "w") as f:
                    f.write("pid %s\nwchan: %s\nstack:\n%s\n"
                            % (pid, ws.stdout.strip(), st.stdout))
        except (OSError, subprocess.SubprocessError, IndexError):
            pass
        who, last = death_evidence()
        fault("the server stopped answering",
              "lst came back empty while %d object(s) were alive.\n"
              "           %s\n"
              "           last words: %s"
              % (len(mine), "\n           ".join(who), last))
        g_stop.set()
        return
    # A young object is not a lost one.
    now = time.time()
    with g_lock:
        young = set(n for n in mine
                    if now - g_model.get(n, {}).get("born", 0) < 30)
    lost = confirm([n for n in mine
                    if (not in_lst(text, n)) and (n not in young)], True)
    if lost:
        fault("the server has lost objects it accepted",
              "%d of %d missing from lst: %s"
              % (len(lost), len(mine), ", ".join(sorted(lost)[:8])))
    # A lan whose endpoint went away is expected to vanish: only one with both ends alive counts.
    lost_lan = []
    with g_lock:
        for n in lans:
            if in_lst(text, n):
                continue
            ends = g_lans.get(n, [])
            if not ends or not all(i in g_model for i, _e in ends):
                continue
            # Same exemption: a lan waiting on a c2c to peer is pending.
            if any(g_model.get(i, {}).get("kind") == "c2c"
                   for i, _e in ends):
                continue
            lost_lan.append(n)
    lost_lan = confirm(lost_lan, True)
    if lost_lan:
        fault("the server has lost lans it accepted",
              "%d of %d missing: %s"
              % (len(lost_lan), len(lans), ", ".join(sorted(lost_lan)[:8])))
    cutoff = time.time() - (CHECK_VANISH + 10.0)
    with g_lock:
        for n, when in list(g_deleting.items()):
            if when < cutoff:
                g_deleting.pop(n, None)
        in_flight = set(g_deleting)
    with g_lock:
        for n, when in list(g_adding.items()):
            if (n in mine) or (when < time.time() - 90.0):
                g_adding.pop(n, None)
        in_flight |= set(g_adding)
    ghosts = []
    for m in re.findall(r"\b%s[a-z]\d+\b" % PREFIX, text):
        if m not in mine and m not in lans and m not in in_flight \
           and m not in g_preexisting_objects:
            ghosts.append(m)
    ghosts = confirm(sorted(set(ghosts)), False)
    with g_lock:
        ghosts = [g for g in ghosts if g not in g_deleting]
    if ghosts:
        with g_lock:
            told = [(g, g_refused.get(g)) for g in ghosts[:8]]
        detail = []
        for g, ref in told:
            lines = [l.strip() for l in text.split("\n") if g in l]
            where = " / ".join(lines[:3]) or "(no line, matched elsewhere)"
            if ref:
                detail.append("%s: its add returned %d %ds ago saying: %s; "
                              "lst says: %s"
                              % (g, ref[0], int(time.time() - ref[2]),
                                 ref[1] or "(nothing)", where))
            else:
                detail.append("%s: never added by this run, or deleted here "
                              "and still in lst; lst says: %s" % (g, where))
        fault("the server has objects nobody asked for",
              " | ".join(detail))

def leftovers_on_the_machine(baseline=None):
    """Mounts, namespaces AND processes: the parts a delete forgets.

    Processes were added after a run left about forty cloonix-passt alive,
    one per nat ever created, and three cloonix-ovs-a2b for each of two
    a2b that could not be deleted -- hours after the objects were gone
    from the topology. None of that is visible in lst, which is exactly
    why it went unnoticed: an object can be deleted, absent from every
    listing, and still be running.

    baseline is the set of pids that were ALREADY there when this run
    started, and they are not this run's leak. The match is on PREFIX,
    which is shared by every run, so anything an earlier run abandoned
    was picked up and reported here as if this one had just done it: a
    run reported "1 still alive: process 120361" and the previous run's
    report named the same pid, orphaned by a switch that crashed nine
    minutes before this run began. Blaming a run for the run before it
    is the same failure as the stale "last words" in death_evidence.
    """
    bad = []
    try:
        r = subprocess.run(["ps", "-eo", "pid,args"], capture_output=True,
                           text=True, timeout=30)
        for line in r.stdout.split("\n"):
            if PREFIX not in line or "soak.py" in line:
                continue
            if "cloonix" not in line:
                continue
            pid = line.split()[0] if line.split() else "?"
            if baseline and pid in baseline:
                continue
            what = line.strip()[:110]
            bad.append("process %s: %s" % (pid, what))
    except (OSError, subprocess.SubprocessError):
        pass
    try:
        with open("/proc/self/mounts") as f:
            for line in f:
                p = line.split()
                nets = [NET] + ([PEER] if PEER else [])
                if len(p) > 1 and PREFIX in p[1] and \
                   any(("/%s/" % n) in p[1] for n in nets):
                    bad.append("mount %s" % p[1])
    except OSError:
        pass
    try:
        r = subprocess.run(["ip", "netns", "list"], capture_output=True,
                           text=True, timeout=20)
        for line in r.stdout.split("\n"):
            n = line.split()[0] if line.split() else ""
            nets = [NET] + ([PEER] if PEER else [])
            if PREFIX in n and any(x in n for x in nets):
                bad.append("netns %s" % n)
    except (OSError, subprocess.SubprocessError):
        pass
    return bad

def worker(idx):
    kinds = list(WEIGHTS.keys())
    weights = [WEIGHTS[k] for k in kinds]
    phase = random.choice(PHASES)
    left = random.randint(10, 40)
    global g_ops
    while not g_stop.is_set():
        if left <= 0:
            phase = random.choice(PHASES)
            left = random.randint(10, 40)
        left -= 1
        kind = random.choices(kinds, weights=weights)[0]
        if C2C_TARGET and (live_c2c() < C2C_TARGET):
            kind = "c2c"
        with g_lock:
            live = len(g_model)
        if live >= MAX_LIVE:
            # At the cap: retire something instead, see pick_victim.
            victim = pick_victim()
            if victim:
                destroy(victim)
            with g_lock:
                g_ops += 1
            time.sleep(random.uniform(phase[1], phase[2]))
            continue
        try:
            if kind == "cnt":
                make_cnt()
            elif kind == "lan":
                make_lan()
            elif kind == "snf":
                do_snf()
            elif kind == "kvm":
                make_kvm()
            elif kind == "c2c":
                make_c2c()
            else:
                make_simple(kind)
        except Exception as exc:                        # noqa: BLE001
            fault("the script itself threw", "%s: %r" % (kind, exc))
        with g_lock:
            g_ops += 1
        # A random chance of an early death, on top of the lifetime, so that a
        # delete can land at any point in another thread's create.
        if random.random() < 0.10:
            victim = pick_victim()
            if victim:
                destroy(victim)
        time.sleep(random.uniform(phase[1], phase[2]))

def pick_victim():
    """A name to delete, never one whose creator is still checking it.

    The model learns a name the moment its add returns, and the creator
    then watches lst for CHECK_APPEAR seconds to see it appear. A worker
    at the cap deleting "any object" could pick that very name, delete it
    cleanly, and the creator filed "created and never appeared" against a
    server that had done exactly what it was told, twice. One nat in
    1229, over two hours: rare, and not a fault. Everything older is fair
    game, at random, so a delete still lands on the young and the settled.
    """
    now = time.time()
    with g_lock:
        names = [n for n, o in g_model.items()
                 if o.get("born", 0) < now - CHECK_APPEAR - 5]
    return random.choice(names) if names else None

def keeper():
    """The thread that reaps, reconciles and notices the server dying.

    Everything in here is wrapped, because this thread dying takes all
    three of those with it. One did die -- a UnicodeDecodeError reading a
    syslog with a stray byte in it -- and the run carried on for another
    seven minutes with nothing being reaped, nothing reconciled and an
    outage going unreported. Silence from a checker is the worst failure
    it can have, so an unexpected exception is now a fault like any other
    and the thread keeps going.
    """
    last = time.time()
    while not g_stop.is_set():
        try:
            reap()
            if time.time() - last > RECONCILE_EVERY:
                last = time.time()
                reconcile()
        except Exception as exc:                        # noqa: BLE001
            fault("the checker itself threw",
                  "%r -- this thread reaps, reconciles and watches for the "
                  "server going away, so it carries on rather than dying "
                  "and leaving the run blind" % exc)
        time.sleep(1)

def ticker():
    start = time.time()
    last_snf_kill = time.time()
    while not g_stop.is_set():
        time.sleep(10)
        # In the ticker, not a worker: no race with a worker deleting the checked object.
        if SNF_KILL_EVERY and (time.time() - last_snf_kill
                               > SNF_KILL_EVERY):
            last_snf_kill = time.time()
            try:
                snf_kill_and_check()
            except Exception as err:                        # noqa: BLE001
                note("snf kill test", "the test itself failed: %s" % err)
        try:
            sweep_deleted_for_leaks()
        except Exception as err:                            # noqa: BLE001
            note("leak sweep", "the sweep itself failed: %s" % err)
        with g_lock:
            n_obj = len(g_model)
            for lname in [k for k, ends in g_lans.items()
                          if not any(i in g_model for (i, _e) in ends)]:
                g_lans.pop(lname, None)
            n_lan = len(g_lans)
            ops = g_ops
            nf = len(g_faults)
        fds = ""
        try:
            r = subprocess.run(["pgrep", "-f",
                                "cloonix-proxymous.*%s" % NET],
                               capture_output=True, text=True, timeout=10)
            pid = (r.stdout or "").split()[0] if r.stdout.strip() else ""
            if pid:
                n = len(os.listdir("/proc/%s/fd" % pid))
                global g_fd_first, g_fd_last
                if g_fd_first is None:
                    g_fd_first = n
                g_fd_last = n
                fds = ", %d fds" % n
                if n > 700:
                    fds += " <-- CLIMBING TOWARD THE LIMIT"
        except (OSError, subprocess.SubprocessError, IndexError):
            pass
        # The rss of the three daemons that live all day: a leak is a line
        # that climbs from one report to the next and never comes back.
        rss = ""
        try:
            parts = []
            for name, short in (("cloonix-main-server", "srv"),
                                ("cloonix-doorways", "doors"),
                                ("cloonix-proxymous", "proxy")):
                r = subprocess.run(["pgrep", "-f", "%s.*%s" % (name, NET)],
                                   capture_output=True, text=True, timeout=10)
                pid = (r.stdout or "").split()[0] if r.stdout.strip() else ""
                if pid:
                    for st in open("/proc/%s/status" % pid):
                        if st.startswith("VmRSS:"):
                            parts.append("%s %dM" % (short,
                                                     int(st.split()[1]) // 1024))
            if parts:
                rss = ", rss " + " ".join(parts)
        except (OSError, subprocess.SubprocessError, IndexError, ValueError):
            pass
        print("  [%5ds] %d/%d objects, %d lans, %d operations, %d fault(s)%s%s"
              % (int(time.time() - start), n_obj, MAX_LIVE, n_lan, ops, nf,
                 fds, rss))

def is_win_image(name):
    """A windows qcow2, by its name. Nothing in a qcow2 header says which
    operating system is inside it, so the name is what there is.

    A whole word, not a substring: darwin and winter are not windows.
    """
    return any(re.match(r"^win(dows)?[0-9]*$", w)
               for w in re.split(r"[^a-z0-9]+", name.lower()))

KVM_IMAGES = ("alpine.qcow2",
              "arch.qcow2",
              "fedora44.qcow2",
              "resolute.qcow2",
              "rocky.qcow2",
              "trixie.qcow2",
              "tumbleweed.qcow2",
              "win.qcow2")

def probe_kvm_image():
    """Those of KVM_IMAGES that are actually in bulk, drawn evenly.

    A missing one is not an error -- not every bulk carries every image --
    but it is said, so a draw that has quietly narrowed to two is visible.

    alpine has no cloonix agent in it, so its "agent never answered" is the
    right answer and not a bug; it stays in as the case where the agent
    genuinely cannot start.

    SOAK_KVM_IMAGE names one explicitly and turns the list off. An empty
    result is not fatal: the run carries on without kvm and says so.
    """
    bulk = "/var/lib/cloonix/bulk"
    want = os.environ.get("SOAK_KVM_IMAGE", "").strip()
    if want:
        if os.path.isfile(os.path.join(bulk, want)):
            return [want]
        sys.exit("SOAK_KVM_IMAGE=%s is not in %s" % (want, bulk))
    imgs = [n for n in KVM_IMAGES
            if os.path.isfile(os.path.join(bulk, n))]
    missing = [n for n in KVM_IMAGES if n not in imgs]
    if missing:
        print("not in %s, so not drawn: %s" % (bulk, ", ".join(missing)))
    # A windows qcow2 is not a plain kvm: it is added with --windows and
    # --uefi by make_kvm, and that attaches the driver disc by name.
    if any(is_win_image(n) for n in imgs):
        if not os.path.isfile(os.path.join(bulk, "virtio-win.iso")):
            imgs = [n for n in imgs if not is_win_image(n)]
            print("no virtio-win.iso in %s, so the windows images are left "
                  "out of the draw" % bulk)
    return imgs

def probe_rootfs():
    r = subprocess.run(CLOONIX + ["cli", NET, "dump", "commands"], capture_output=True,
                       text=True, timeout=30)
    if r.returncode:
        sys.exit("no answer from a cloonix net called '%s'\n"
                 "  start one first:   cloonix start %s" % (NET, NET))
    for cand in ("frr_trixie", "trixie", "wg_trixie", "mqtt_trixie",
                 "proxy_trixie", "smtp_trixie", "dhcp_trixie", "alpine"):
        p = subprocess.run(CLOONIX + ["--wait", "6", "cli", NET, "add",
                            "cnt", "%sprobe" % PREFIX, "eth=1", cand],
                           capture_output=True, text=True, timeout=60)
        subprocess.run(CLOONIX + ["--wait", "1", "cli", NET, "del",
                        "%sprobe" % PREFIX], capture_output=True,
                       text=True, timeout=60)
        if p.returncode == 0:
            return cand
    sys.exit("no usable container image found. Build one, for example:\n"
             "  cd vm_create/cnt_kvm && ./doitall.sh --cnt trixie")

def learn_lst_shape():
    """Make one of each kind, look at lst, and say what it saw.

    Written after a run reported forty-eight faults of which most were this
    script not knowing what lst prints for a tap, a nat or a lan. Checking
    the assumption once, out loud, at the start, costs five seconds and
    saves reading a page of confident nonsense.

    It does not stop the run if a kind is invisible -- that might be the
    real bug -- it says so, so the faults that follow can be read in the
    right light.
    """
    print("checking what lst prints for each kind, before trusting it:")
    made = []
    # Only the kinds this run will actually make.
    for kind in [k for k in ("cnt", "tap", "nat", "a2b") if k in WEIGHTS]:
        name = "%sprobe%s" % (PREFIX, kind)
        if kind == "cnt":
            r = cli("add", "cnt", name, "eth=2", g_rootfs)
        else:
            r = cli("add", kind, name)
        if r.returncode:
            print("    %-4s refused: %s" % (kind, (r.stdout + r.stderr).strip()[:60]))
            continue
        made.append(name)
    time.sleep(6)
    text = lst()
    for name in made:
        kind = name.replace("%sprobe" % PREFIX, "")
        hit = [l.strip() for l in text.split("\n") if name in l]
        if hit:
            print("    %-4s -> %s" % (kind, hit[0][:70]))
        else:
            print("    %-4s -> NOT IN LST AT ALL. Checks on a %s will report"
                  % (kind, kind))
            print("            faults that may be this script's fault, not "
                  "the server's.")
    for name in made:
        cli("del", name)
    time.sleep(3)
    print()

def cleanup():
    print("\ncleaning up everything this run made")
    with g_lock:
        names = list(g_model.keys())
        lans = list(g_lans.keys())
    for name in names:
        cli("del", name)
    print("  asked for %d object(s) to go, waiting" % len(names))
    end = time.time() + 120
    while time.time() < end:
        text = lst()
        still = [n for n in names + lans if in_lst(text, n)]
        if not still:
            break
        time.sleep(2)
    if PEER:
        try:
            r = subprocess.run(CLOONIX + ["cli", PEER, "dump", "commands"],
                               capture_output=True, text=True, timeout=60)
            ptext = r.stdout or ""
            pstill = [n for n in names if in_lst(ptext, n)]
            if pstill:
                fault("objects survived the cleanup in the PEER net",
                      "%d still in %s after the delete: %s. Their helper "
                      "processes belong to %s and will keep running until "
                      "%s deletes them"
                      % (len(pstill), PEER, ", ".join(sorted(pstill)[:10]),
                         PEER, PEER))
        except (OSError, subprocess.SubprocessError):
            pass

    text = lst()
    still = [n for n in names + lans if in_lst(text, n)]
    if still:
        fault("objects survived the cleanup",
              "%d still in lst after 120s: %s"
              % (len(still), ", ".join(sorted(still)[:10])))
    else:
        print("  the server is clear of them")
    # Give the teardown time to finish before calling anything a leak.
    print("  waiting for the teardowns to finish")
    if g_preexisting_pids:
        print("  ignoring %d cloonix process(es) that predate this run: %s"
              % (len(g_preexisting_pids),
                 ", ".join(sorted(g_preexisting_pids)[:8])))
    bad = leftovers_on_the_machine(g_preexisting_pids)
    for w in range(60):
        if not bad:
            break
        time.sleep(2)
        now = leftovers_on_the_machine(g_preexisting_pids)
        if len(now) < len(bad):
            bad = now                    # still draining, keep waiting
            continue
        bad = now
        if w > 8:
            break                        # stopped going down: this is real
    if bad:
        print("  %d still there after %ds of waiting" % (len(bad), (w + 1) * 2))
    else:
        print("  everything drained")
    if bad:
        procs = [b for b in bad if b.startswith("process")]
        rest = [b for b in bad if not b.startswith("process")
                and b not in g_preexisting_dirt]
        if procs:
            fault("processes were left running after the delete",
                  "%d still alive, invisible to lst: %s"
                  % (len(procs), "; ".join(procs[:6])))
        if rest:
            fault("the machine was left dirty",
                  "%d mount(s) or namespace(s): %s"
                  % (len(rest), "; ".join(rest[:8])))
    else:
        print("  nothing left behind: no mounts, namespaces or processes")

def report(started):
    dur = time.time() - started
    print("\n" + "=" * 68)
    print("soak on %s: %d worker(s), seed %d" % (NET, WORKERS, SEED))
    print("ran for %dm %ds, %d operations, %.1f a second"
          % (dur // 60, dur % 60, g_ops, g_ops / dur if dur else 0))
    print()
    for k in sorted(g_stats):
        if k.startswith("_"):
            continue
        print("  %-24s %6d" % (k, g_stats[k]))
    if g_fd_first is not None and g_fd_last is not None and g_ops:
        grew = g_fd_last - g_fd_first
        print("  proxymous descriptors: %d -> %d over %d operations "
              "(%.1f per thousand)"
              % (g_fd_first, g_fd_last, g_ops,
                 1000.0 * grew / g_ops if g_ops else 0))
        print()
    if g_reasons:
        print("  why things were refused:")
        for kind in sorted(g_reasons):
            for msg, n in sorted(g_reasons[kind].items(),
                                 key=lambda kv: -kv[1])[:3]:
                print("    %-5s x%-5d %s" % (kind, n, msg))
        print()
    if not g_faults:
        print("NO FAULTS. Every object appeared when created, vanished when")
        print("deleted, and the machine is as clean as it was found.")
        print()
        print("To push harder:")
        print("  ./soak.py %s 8            more threads, more overlap" % NET)
        print("  ./soak.py %s 4 %d   the same run again, exactly"
              % (NET, SEED))
        return 0
    print("%d FAULT(S), grouped:" % len(g_faults))
    print()
    kinds = {}
    for t, what, detail in g_faults:
        kinds.setdefault(what, []).append((t, detail))
    for what in sorted(kinds, key=lambda k: -len(kinds[k])):
        hits = kinds[what]
        print("  %d x  %s" % (len(hits), what))
        names = set()
        for _t, d in hits:
            names.update(re.findall(r"\b%s[a-z]\d+\b" % PREFIX, d))
        if names:
            ordered = sorted(names)
            print("        names: %s%s"
                  % (", ".join(ordered[:10]),
                     " and %d more" % (len(ordered) - 10)
                     if len(ordered) > 10 else ""))
        print("        first at %s: %s"
              % (time.strftime("%H:%M:%S", time.localtime(hits[0][0])),
                 hits[0][1][:150]))
        print()
    print()
    print("The seed reruns this exactly, which is the point of printing it:")
    print("  ./soak.py %s %d %d" % (NET, WORKERS, SEED))
    return 1

def main():
    global g_rootfs, g_kvm_image, g_counter
    for entry in leftovers_on_the_machine():
        if entry.startswith("process "):
            g_preexisting_pids.add(entry.split()[1].rstrip(":"))
        else:
            # a mount or a netns another run left: not this run's dirt.
            g_preexisting_dirt.add(entry)
    try:
        sys.stdout.reconfigure(line_buffering=True)
    except AttributeError:
        pass
    print("soak: random create and delete on %s, %d workers, seed %d"
          % (NET, WORKERS, SEED))
    print("ctrl-c stops it and cleans up.\n")
    if any(k in WEIGHTS for k in ("cnt", "snf")):
        g_rootfs = probe_rootfs()
        print("using the %s image for containers" % g_rootfs)
    else:
        g_rootfs = None
    g_kvm_image = probe_kvm_image()
    if g_kvm_image:
        print("kvm images, drawn evenly:\n    %s" % ", ".join(g_kvm_image))
    elif "kvm" in WEIGHTS:
        print("none of KVM_IMAGES is in /var/lib/cloonix/bulk, so no kvm "
              "will be made")

    try:
        text = lst()
        old = sorted(set(re.findall(r"\b%s[a-z]\d+\b" % PREFIX, text)))
        if old:
            # Recorded, not just announced.
            g_preexisting_objects.update(old)
            print()
            print("  WARNING: %d object(s) from an earlier run are still "
                  "in %s:" % (len(old), NET))
            print("    %s%s" % (", ".join(old[:10]),
                                " and %d more" % (len(old) - 10)
                                if len(old) > 10 else ""))
            print("    They are ignored for the rest of this run, so a "
                  "fault naming one is a real one.")
            print("    Clear them first:  ./fail_spider.sh clean %s" % NET)
            print()
        nums = [int(n) for n in re.findall(r"\b%s[a-z](\d+)\b" % PREFIX,
                                           text)]
        if nums:
            with g_lock:
                if g_counter <= max(nums):
                    g_counter = max(nums) + 10000
                    print("  starting names at %d, past the %d already in %s"
                          % (g_counter, max(nums), NET))
                    print()
    except Exception:                                   # noqa: BLE001
        pass
    learn_lst_shape()
    print("at most %d objects alive at once (SOAK_MAX_LIVE to change)"
          % MAX_LIVE)
    if _only:
        print("SOAK_ONLY=%s, so only those kinds are made" % _only)
    print("weights: %s\n"
          % ", ".join("%s %d" % (k, v) for k, v in
                      sorted(WEIGHTS.items(), key=lambda kv: -kv[1])))
    if "kvm" in WEIGHTS:
        # Said out loud, before the machine finds out the hard way.
        if MAX_LIVE_KVM:
            print("kvm: ram=%d cpu=%d, at most %d at once -- up to %.1f GB "
                  "of guest memory plus roughly %.1f GB of qemu"
                  % (KVM_RAM, KVM_CPU, MAX_LIVE_KVM,
                     (KVM_RAM * MAX_LIVE_KVM) / 1024.0,
                     (200.0 * MAX_LIVE_KVM) / 1024.0))
        else:
            print("kvm: ram=%d cpu=%d, NO separate cap -- the only limit is "
                  "SOAK_MAX_LIVE=%d, so the worst case is %.1f GB of guest "
                  "memory. Set SOAK_MAX_LIVE_KVM to bound it."
                  % (KVM_RAM, KVM_CPU, MAX_LIVE,
                     (KVM_RAM * MAX_LIVE) / 1024.0))
        print()

    if LEAK_GRACE > 0:
        print("leak check: %ds after each delete is accepted, the object's "
              "name is looked for in the process list. A FAULT is filed if "
              "a cloonix process still names it (SOAK_LEAK_GRACE=0 to turn "
              "off)." % int(LEAK_GRACE))
        print()

    if SNF_KILL_EVERY:
        print("snf kill test: every %ds one running snf is killed and the "
              "capture asked for again with 'cnf <name> eth<i> sniff=on'. A "
              "FAULT is filed if it does not come back -- an eth that says "
              "it is sniffable while nothing sniffs it." % SNF_KILL_EVERY)
        if not priv_available():
            print("    WARNING: no privilege to kill an snf, so this test "
                  "will skip every time.")
        print()

    if "c2c" in WEIGHTS:
        if C2C_TARGET:
            print("c2c: %d kept alive at once with %s as the peer net "
                  "(SOAK_C2C to change). The draw is overridden while there "
                  "are fewer, so the run keeps rebuilding its way back to "
                  "%d." % (C2C_TARGET, PEER, C2C_TARGET))
        else:
            print("c2c: peer net %s, no target -- the weight alone decides, "
                  "which usually means one at a time" % PEER)
        print()

    if "cnt" in WEIGHTS:
        if MAX_LIVE_SYSTEMD_CNT > 0:
            print("cnt: no systemd by default; at most %d at once are "
                  "started with CLOONIX_WITH_SYSTEMD=1"
                  % MAX_LIVE_SYSTEMD_CNT)
        else:
            print("cnt: no systemd at all (SOAK_MAX_LIVE_SYSTEMD_CNT=0)")
        print()

    if not priv_available():
        print("WARNING: running as uid %d and 'sudo -n true' failed."
              % os.geteuid())
        print("    crun state, the systemd journal, dmesg, /proc/<pid>/stack "
              "and the syslog are all")
        print("    root-only. Without them a container failure is recorded "
              "with no evidence that")
        print("    explains it, and the crun-state check cannot tell a dead "
              "container from an")
        print("    unreadable one -- which has already caused one wrong "
              "diagnosis.")
        print("    Fix it with either:")
        print("sudo ./soak.py %s ..." % NET)
        print("      or a passwordless sudo rule for this account, then run "
              "as yourself.")
        print()
    elif os.geteuid() != 0:
        print("running as uid %d; the root-only probes go through sudo -n."
              % os.geteuid())
        print()

    started = time.time()

    def on_int(_sig, _frm):
        if g_stop.is_set():
            print("\nsecond ctrl-c: leaving things as they are")
            os._exit(2)
        print("\nctrl-c: stopping the workers")
        g_stop.set()

    signal.signal(signal.SIGINT, on_int)
    signal.signal(signal.SIGTERM, on_int)

    threads = [threading.Thread(target=worker, args=(i,), daemon=True)
               for i in range(WORKERS)]
    threads.append(threading.Thread(target=keeper, daemon=True))
    threads.append(threading.Thread(target=ticker, daemon=True))
    for t in threads:
        t.start()
    try:
        while not g_stop.is_set():
            time.sleep(0.5)
    except KeyboardInterrupt:
        g_stop.set()
    for t in threads:
        t.join(timeout=20)
    cleanup()
    sys.exit(report(started))

main()
