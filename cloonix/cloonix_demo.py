#!/usr/bin/env python3
# cloonix_demo.py -- "cloonix cli <net> demo [name]": list the demos, play
# one.
import os
import subprocess
import sys

DEMO_DIR = os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos")

def demo_files(sub):
    """The playable files of one directory: a python programme, nothing else.

    Python only. A demo is read as much as it is run, and one language is
    one thing to read; a shell demo was also a second player to keep in
    step, for the one form that was already the less used.
    """
    try:
        return sorted(f for f in os.listdir(sub)
                      if not f.startswith(".")
                      and os.path.isfile(os.path.join(sub, f))
                      and f.endswith(".py"))
    except OSError:
        return []

DEMO_MAX_DEPTH = 1

def demo_list():
    """What can be played, as the names that play it.

    A bare listing of DEMO_DIR would print areas, which are not names a demo
    can be asked for. What is printed is what works: "routing/frrmix.py",
    and the short "frrmix.py" resolves to it as well.
    """
    out = []

    def walk(sub, depth):
        full = os.path.join(DEMO_DIR, sub) if sub else DEMO_DIR
        out.extend(os.path.join(sub, f) for f in demo_files(full))
        if depth >= DEMO_MAX_DEPTH:
            return
        try:
            entries = sorted(os.listdir(full))
        except OSError:
            return
        for name in entries:
            if name.startswith(".") or ((depth == 0) and (name == "lib")):
                continue
            if os.path.isdir(os.path.join(full, name)):
                walk(os.path.join(sub, name), depth + 1)

    walk("", 0)
    return out

# The ways a docstring can open, longest first.
QUOTES = ['r"""', "r'''", 'r"', '"""', "'''", '"', "'"]

def demo_summary(path):
    """The one line a demo says about itself: the head of its docstring.

    Line two of every demo, the shebang being line one. What follows
    "<name> - " there is the summary; a demo written another way gives its
    whole first line instead.
    """
    try:
        with open(path) as f:
            f.readline()
            head = f.readline().strip()
    except OSError:
        return ""
    for quote in QUOTES:
        if head.startswith(quote):
            head = head[len(quote):]
            break
    _, sep, rest = head.partition(" - ")
    head = (rest if sep else head).strip().rstrip(".")
    if len(head) > 52:
        head = head[:52].rsplit(" ", 1)[0] + "..."
    return head

def demo_target(rest):
    """The demo name a command line asks for.

    The guide walks demos/ a directory at a time, so the name arrives as
    words -- routing frr frrmix.py -- while a line typed by hand carries the
    path in one word. A slash join makes the two the same thing, a single
    word being its own join.
    """
    return "/".join(w for w in rest if w)

# What the next word of a demo name is called, one per level of demos/.
DEMO_PLACES = ("<area>", "<demo>")

def demo_place(prefix):
    """What to call the place reached, so each step names what it asks for.

    By depth, except at the top, where a guest demo, windows.py, sits
    beside the areas: a word ending in .py there is a demo.
    """
    depth = len([w for w in prefix.split("/") if w])
    under = demo_level(prefix)
    if under and all(w.endswith(".py") for w in under):
        return DEMO_PLACES[-1]
    return DEMO_PLACES[min(depth, len(DEMO_PLACES) - 1)]

def demo_level(prefix):
    """What lies directly under one place in demos/, as {word: its line}.

    One component at a time: the areas, then the programmes of an area. A
    directory comes back as a bare word with no slash on it, because the
    guide adds one word per tab and joins them itself.

    Except where the level holds a single directory, which is no choice at
    all: the walk goes through it and the words come back with the step it
    passed in front of them, so no place is ever offered with one word in
    it, and the name built is still the whole path.

    A directory says what is under it and a programme says what it does, so
    every step of the walk carries its own line.
    """
    prefix = prefix.strip("/")
    prefix = (prefix + "/") if prefix else ""
    depth = prefix.count("/")
    under = {}
    for name in demo_list():
        if not name.startswith(prefix):
            continue
        parts = name.split("/")
        under.setdefault(parts[depth], []).append(name)
    if len(under) == 1:
        word = list(under)[0]
        if len(under[word]) > 1:
            return dict(("%s/%s" % (word, w), line)
                        for w, line in demo_level(prefix + word).items())
    out = {}
    for word in sorted(under):
        held = under[word]
        if len(held) == 1 and len(held[0].split("/")) <= depth + 2:
            out[word] = demo_summary(os.path.join(DEMO_DIR, held[0]))
        else:
            children = sorted(set(n.split("/")[depth + 1] for n in held))
            line = ", ".join(children)
            out[word] = line if len(line) <= 52 else line[:49] + "..."
    return out

def demo_find(want):
    """The file a name asks for, or None, saying nothing either way.

    Three ways, because all three are things people type:
    "routing/frrmix.py" is the exact path, "routing" is the area and is
    answerable when it holds one programme, and "frrmix" or "frrmix.py" is
    the name of the demo alone and is looked for in every area.

    Quiet, because line_is_command asks this of every line the guide sees.
    """
    for ext in ("", ".py"):
        cand = os.path.join(DEMO_DIR, want + ext)
        if os.path.isfile(cand):
            return cand
    hits = demo_under(want)
    if len(hits) == 1:
        return os.path.join(DEMO_DIR, hits[0])
    if hits:
        return None
    for name in demo_list():
        base = os.path.basename(name)
        if base == want or os.path.splitext(base)[0] == want:
            return os.path.join(DEMO_DIR, name)
    return None

def demo_under(want):
    """The demos a directory holds, empty when the name is not one."""
    if not want:
        return []
    return [n for n in demo_list()
            if n == want or n.startswith(want.rstrip("/") + "/")]

def demo_resolve(want):
    """demo_find, and a word about why when it comes back empty."""
    path = demo_find(want)
    if path:
        return path
    hits = demo_under(want)
    if hits:
        sys.stderr.write("%s holds several demos, name one:\n" % want)
        for f in hits:
            sys.stderr.write("  %s\n" % f)
    return None

def do_demo(client, net, argv):
    """Play a demo: list them with no name, run one with a name.

    client is the client module, and the only thing wanted of it is the
    path of the file it was started from.

    It is done here and nowhere else. The client is python already, so any
    machine able to run cloonix at all can run a demo, and a second player
    in another language would only be a second thing to keep in step.

    The demo runs on the machine you typed on and drives the server across
    the network, exactly as the client itself does. The server needs nothing.
    """
    if not argv:
        found = demo_list()
        if not found:
            sys.stderr.write("no demo in %s\n" % DEMO_DIR)
            return 1
        print("demos in %s:" % DEMO_DIR)
        for f in found:
            print("  %s" % f)
        return 0

    want = demo_target(argv)
    path = want
    if not os.path.isfile(path):
        path = demo_resolve(want)
        if not path:
            if not demo_under(want):
                sys.stderr.write("no demo called '%s'\n" % want)
            return 1
    with open(path) as f:
        first = f.readline()
    # A demo is a python programme and must say so on its first line.
    if not (first.startswith("#!") and "python" in first):
        sys.stderr.write("%s does not start with a python shebang: a demo "
                         "is a\npython programme and opens with "
                         "#!/usr/bin/env python3\n" % path)
        return 1
    # Run through this interpreter: no shebang or execute bit needed.
    print(">>> %s %s" % (path, net))
    cmd = [sys.executable, path, net]
    # Windows CreateProcess only appends .exe: the demos get the client path in CLOONIX_CLIENT.
    env = dict(os.environ)
    if os.name == "nt":
        env["CLOONIX_CLIENT"] = os.path.abspath(client.__file__)
    try:
        rc = subprocess.call(cmd, env=env)
    except OSError as err:
        sys.stderr.write("cannot run %s: %s\n" % (path, err))
        rc = 1
    print("[DONE ok=%d ko=%d]" % (0 if rc else 1, 1 if rc else 0))
    return rc

def run(client, net, argv):
    """The one entry point, called by the demo verb of the client."""
    return do_demo(client, net, argv)
