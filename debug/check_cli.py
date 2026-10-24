#!/usr/bin/env python3
# Does the tree still spell the cloonix command the way the client reads it?
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
CLIENT = os.path.join(ROOT, "cloonix", "cloonix")
SKIP_DIR = (".git", "__pycache__", "build", "build_user", "build_dev", "rootfs_targz_store")
# binaries, then editor and patch leftovers, which are no part of the tree
SKIP_EXT = (".png", ".jpg", ".jpeg", ".ico", ".gz", ".qcow2", ".pyc", ".o",
            "~", ".orig", ".rej", ".bak", ".swp", ".swo")

src = open(CLIENT).read().replace('if __name__ == "__main__":', "if False:")
G = {"__file__": CLIENT, "__name__": "cloonix_grammar"}
exec(compile(src, CLIENT, "exec"), G)
sys.path.insert(0, os.path.join(ROOT, "cloonix"))
import cloonix_completion as COMP

TOP = set(G["VERBS"])
CLI = set(G["CLI_VERBS"])
VERB = TOP | CLI
KIND = {"dump": set(COMP.COMPLETE_DUMP), "add": set(G["ADD_KINDS"])}
# the client in a list, the options before the verb
LISTED = re.compile(r"CLOONIX \+ \[([^\]]*)")
VALUED = ("--wait", "-s", "--server", "--cfg", "--via-unix")

GONE = {"dcf": "dump config",
        "top": "dump topo", "dmp": "dump json", "atop": "dump drawing",
        "lst": "dump commands",
        "snf_rx": "sniff", "snf_on": "cnf <n> <ethN> sniff=on",
        "snf_off": "cnf <n> <ethN> sniff=off", "listnet": "list",
        "rmall": "del all", "kill": "stop <net>", "norton": "files",
        "position": "cnf <n> x= y=", "poseth": "cnf <n> [<eth>] angle="}

# "${NET ssh}": the brace closing after the verb instead of the variable.
BRACE = re.compile(r"\$\{[A-Za-z_][A-Za-z_0-9]*\s+(?:%s)\}" % "|".join(VERB))
# The few words after the command, stopped at anything that ends a line.
AFTER = re.compile(
    r"(?:\bcloonix\b(?![-_.])|\$\{?[A-Za-z_]*CLOONIX\}?|\$\{?[A-Za-z_]*cloonix\}?)"
    r"((?:[ \t]+[^\s|;`\"'<>()]+){0,4})")

def verb_places(words):
    """The indexes of words that stand where a verb of the line stands.

    Right after cloonix, and right after "cli <net>". Deeper than that a
    word is a name: "demo cloonix snf" is a directory of demos/, not a verb.
    """
    out = [0] if words else []
    if len(words) > 2 and words[0] == "cli":
        out.append(2)
    return out

def complain(short, n, line, why):
    print("%s:%d: %s" % (short, n, why))
    print("    %s" % line.strip()[:100])

bad = 0
for dirpath, dirnames, filenames in os.walk(ROOT):
    dirnames[:] = [d for d in dirnames if d not in SKIP_DIR]
    for fn in sorted(filenames):
        if fn.endswith(SKIP_EXT) or fn == os.path.basename(__file__):
            continue
        path = os.path.join(dirpath, fn)
        try:
            text = open(path, errors="replace").read()
        except OSError:
            continue
        short = os.path.relpath(path, ROOT)
        for n, line in enumerate(text.split("\n"), 1):
            for m in BRACE.finditer(line):
                print("%s:%d: %s never closes -- a rename ate the brace"
                      % (short, n, m.group(0)))
                bad += 1
            # CLOONIX + ["cli", NET, "kill"]: the verbs of a python list
            for m in LISTED.finditer(line):
                # a string literal is its word, anything else unknown
                words = [e.strip()[1:-1] if re.fullmatch(r'\s*"[^"]*"\s*', e)
                         else None for e in m.group(1).split(",")]
                while words and words[0] and words[0].startswith("-"):
                    words = words[2:] if words[0] in VALUED else words[1:]
                if not words or words[0] is None:
                    continue
                if words[0] not in TOP:
                    complain(short, n, line, "'%s' is not a verb of cloonix: "
                             "%s" % (words[0], " ".join(sorted(TOP))))
                    bad += 1
                elif words[0] == "cli" and len(words) > 2 and \
                        words[2] is not None and words[2] not in CLI:
                    complain(short, n, line, "'%s' is not a verb of cli"
                             % words[2])
                    bad += 1
            for m in AFTER.finditer(line):
                # A word carrying a closing emphasis ends the command:
                # "*cloonix cli nemo dump* prints ..." is prose after it.
                words = []
                for raw in m.group(1).split():
                    w = raw.strip(",*`)]")
                    if w:
                        words.append(w)
                    if raw.rstrip(",.)]").endswith(("*", "`", '"')):
                        break
                for i in verb_places(words):
                    w = words[i]
                    if i == 0 and w in CLI:
                        if not re.search(r"cli\s+\S+\s+" + w, line):
                            complain(short, n, line,
                                     "'%s' is a verb of cli: the net comes "
                                     "first, cloonix cli <net> %s" % (w, w))
                            bad += 1
                    elif w in GONE:
                        complain(short, n, line,
                                 "'%s' was renamed to '%s'" % (w, GONE[w]))
                        bad += 1
                    elif w in KIND and i + 1 < len(words):
                        nxt = G["unabbrev"](words[i + 1], KIND[w])
                        if nxt.isalpha() and nxt.islower() \
                                and nxt not in KIND[w] and nxt not in VERB:
                            complain(short, n, line,
                                     "'%s %s' is no kind of %s: %s"
                                     % (w, nxt, w, " ".join(sorted(KIND[w]))))
                            bad += 1

if bad:
    print("\n%d line(s) disagree with the client" % bad)
    sys.exit(1)
print("no renamed verb and no eaten brace left in the tree")
