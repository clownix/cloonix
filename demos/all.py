#!/usr/bin/env python3
"""all - every demo of the Demo picker in a row, stopping at the first problem.

    ./all.py [net]              the net defaults to nemo

Plays the demos listed in demos/packages, the list of the Demo picker of
the canvas, in its order: each erases the net, builds its network, proves
it and leaves it up until the next one erases it. The first demo that does
not end well stops the run, its network left as it is for inspection: one
that fails, or one that cannot run here (exit EXIT_UNAVAILABLE: its image
absent, a kernel module missing on the host, a net not run as root, a toy).
Nobody has to press a key: a demo that would wait for one goes on by
itself.
Each demo is printed first as the line that plays it alone.

Image: those of each demo
"""
import ast
import os
import re
import shutil
import subprocess
import sys
import time

sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    EXIT_UNAVAILABLE, NET, check_net, erase_net, modules_warning, show, story)

# demo_test.py and check_demos.py: this one plays the others, needs no image
PLAYS_THE_PACKAGES = True
ME = "all.py"
# A check: no pause for a reader in the demos played, none between them,
# unless the caller sets CLOONIX_DEMO_FAST=0.
FAST = os.environ.get("CLOONIX_DEMO_FAST", "1") == "1"
END_PAUSE = 0 if FAST else 5

def demos_dir():
    """Where packages and the demos are: beside this file, or, run from the
    copy the Demo panel writes into /tmp, the demos of the installation."""
    for d in (_HERE, os.environ.get("CLOONIX_DEMO_DIR", ""),
              "/usr/share/cloonix/demos"):
        if d and os.path.isfile(os.path.join(d, "packages")):
            return d
    sys.exit("no demos/packages beside %s, in CLOONIX_DEMO_DIR or in "
             "/usr/share/cloonix/demos" % _HERE)

DIR = demos_dir()

def listed():
    """[(package title, demo, line)] of demos/packages, all.py left out."""
    out, title = [], ""
    with open(os.path.join(DIR, "packages")) as f:
        for ln in f:
            if not ln.strip() or ln.startswith("#"):
                continue
            word = ln.split(None, 1)
            if word[0].isdigit():
                title = ln.strip()
            elif word[0].endswith(".py") and word[0] != ME:
                out.append((title, word[0],
                            word[1].strip() if len(word) > 1 else ""))
    return out

def modules_of(demo):
    """The kernel modules a demo needs: its HOST_MODULES, the names given to
    opening() in the demos of a toy."""
    try:
        with open(os.path.join(DIR, demo)) as f:
            tree = ast.parse(f.read())
    except (OSError, SyntaxError):
        return []
    out = []
    for n in ast.walk(tree):
        if isinstance(n, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "HOST_MODULES"
                for t in n.targets):
            out += [c.value for c in ast.walk(n.value)
                    if isinstance(c, ast.Constant) and isinstance(c.value, str)]
        elif isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and \
                n.func.id == "opening":
            out += [a.value for a in n.args
                    if isinstance(a, ast.Constant) and isinstance(a.value, str)]
    return out

def server_here():
    """The server of the net runs on this machine: a loopback address in the
    configuration of the client. Its /sys/module is then the host's."""
    client = os.environ.get("CLOONIX_CLIENT") or shutil.which("cloonix")
    if not client:
        return False
    cfg = os.path.join(os.path.dirname(os.path.realpath(client)),
                       "cloonix_config.py")
    try:
        with open(cfg) as f:
            txt = f.read()
    except OSError:
        return False
    net = re.search(r"CLOONIX_NET:\s*%s\s*\{([^}]*)\}" % re.escape(NET), txt)
    ip = re.search(r"cloonix_ip\s+(\S+)", net.group(1)) if net else None
    return bool(ip) and ip.group(1).startswith("127.")

def modules_missing(demos):
    """{module: [demos]} of the modules not loaded on the host, all demos at
    once: the run would stop at the first of them, demos later."""
    out = {}
    if not (server_here() and os.path.isdir("/sys/module")):
        return out
    for _, demo, _ in demos:
        for m in modules_of(demo):
            if not any(os.path.isdir("/sys/module/" + x)
                       for x in (m, m.replace("-", "_"))):
                out.setdefault(m, []).append(demo)
    return out

def play(demo):
    """One demo as the Demo panel plays it, keys not awaited: its commands
    to the console, its history to the Comments panel, its refusal to the
    error output. Its exit code."""
    env = dict(os.environ)
    env.pop("CLOONIX_DEMO_KEY", None)
    env["CLOONIX_DEMO_FAST"] = "1" if FAST else "0"
    return subprocess.run([sys.executable, os.path.join(DIR, demo), NET],
                          stdin=subprocess.DEVNULL, env=env).returncode

def main():
    demos = listed()
    if not demos:
        sys.exit("no demo in %s" % os.path.join(DIR, "packages"))
    check_net()
    missing = modules_missing(demos)
    if missing:
        sys.stderr.write(modules_warning(sorted(missing)) + "\n")
        for m, ds in sorted(missing.items()):
            sys.stderr.write("%s: %s\n" % (m, ", ".join(ds)))
        sys.exit(EXIT_UNAVAILABLE)
    erase_net()
    for n, (title, demo, line) in enumerate(demos, 1):
        # the Comments panel: this demo and its step in its title
        story("#demo# %d/%d %s %s" % (n, len(demos), demo, line))
        story("Package %s" % title)
        show("cloonix cli %s demo %s" % (NET, demo))
        rc = play(demo)
        story("#demoend# %s %d" % (demo, rc))
        if rc:
            sys.exit("%s %s, exit %d: the run stops there, its network left "
                     "as it is" % (demo, "cannot run here"
                                   if rc == EXIT_UNAVAILABLE else "failed", rc))
        time.sleep(END_PAUSE)
    # the network of the last demo stays up, as after any demo
    story("\nthe %d demos played to their end" % len(demos))

if __name__ == "__main__":
    main()
