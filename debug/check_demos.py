#!/usr/bin/env python3
r"""check_demos - everything about the demos that can be known without a server.

    ./check_demos.py                    checks ../demos
    ./check_demos.py /path/to/demos     checks that tree instead
    ./check_demos.py --quiet            print only what is wrong

Exit code 0 means every check passed. Anything else means at least one did
not, and every failure is printed with the file and the reason.

It runs in under a second, needs no cloonix server, no images and no
network, and it catches the class of fault that costs the most time: a demo
that fails ninety seconds in, on a machine with the images built, for a
reason that was visible in the source all along.

WHAT IT CHECKS

  the tree      one level at most: demos/<area>/<demo>.py, or a guest demo
                at the top, demos/windows.py; nothing but python in it,
                demos/packages and demos/voice
  the packages  demos/packages, the list the Demo picker of the canvas
                shows, holds every demo once and is the course of learn.rst:
                same packages, same titles, same demos, same order, same lines;
                demos.rst and demos_*.rst have one section per demo, each
                chapter in that order
  the voice     demos/voice, the phrase the Demo panel says when a demo
                starts, holds every demo once and a phrase for each, and
                each phrase has its clip in jscloonix/audio
  the history   the HISTORY of a demo is word for word the paragraph that
                heads its section in the demo chapters of the user book
  the commands  the demo prints every cloonix command it runs, as a line
                that can be pasted into a terminal -- the dev book, The demos,
                says why -- and prints nothing else: the description of the steps is
                story(), for the Comments panel of the canvas
  the script    compiles; has a shebang; has the summary line that
                cloonix_demo.py reads to list it; takes the net as its
                first argument and defaults to nemo; names the images it
                needs; erases the net and places its items before creating
                anything; and does not start a net itself
  the guests    every source a demo writes INTO a guest compiles as python,
                and contains no quote and no percent -- the two characters
                put() cannot send through printf
  the filters   every "sniff ... --dump <filter>" line a demo shows uses the
                words of the capture filter of the server and no other
  the canvas    every angle= of an eth faces the lan its interface is cabled to
  the images    every image a demo names is built by a script that exists
                in vm_create/cnt_kvm_demo or vm_create/cnt_kvm

WHY THE LAST TWO ARE HERE

A guest-side program is a string here and a program there. It is never
compiled on this side, so a syntax error in it survives every review and
appears as a container that "did not answer" much later. And a % in one of
those strings is not a syntax error at all: printf eats it, the file
arrives subtly wrong, and the demo fails somewhere unrelated. Both have
happened. Both are one regexp.

Standard library only.
"""
import ast
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 \
    and not sys.argv[1].startswith("-") else os.path.join(HERE, "..", "demos")
TREE = os.path.abspath(TREE)
QUIET = "--quiet" in sys.argv

ROOT = os.path.abspath(os.path.join(TREE, ".."))
IMAGE_DIRS = [os.path.join(ROOT, "vm_create", "cnt_kvm_demo"),
              os.path.join(ROOT, "vm_create", "cnt_kvm")]

KNOWN_ABSENT = {}

FAILURES = []
CHECKED = [0]

def bad(where, why):
    FAILURES.append((where, why))

def ok(what):
    CHECKED[0] += 1
    if not QUIET:
        print("  ok    %s" % what)

def demo_scripts():
    """Every demo, as (relative name, path): area/demo.py, or demo.py."""
    out = []
    for dirpath, dirnames, files in os.walk(TREE):
        # lib/ holds what the demos share, not demos
        dirnames[:] = [d for d in dirnames
                       if not d.startswith(".") and d != "__pycache__" and
                       not (dirpath == TREE and d == "lib")]
        rel = os.path.relpath(dirpath, TREE)
        depth = 0 if rel == "." else len(rel.split(os.sep))
        if depth > 1:
            bad(rel, "deeper than area/demo.py: a demo is one file in its "
                     "area, with no directory of its own")
            dirnames[:] = []
            continue
        for f in sorted(files):
            if f.startswith("."):
                continue
            where = f if rel == "." else "%s/%s" % (rel, f)
            if f in ("packages", "voice") and rel == ".":
                continue
            if not f.endswith(".py"):
                bad(where, "not a demo: a demo is a python file and the "
                           "tree holds nothing else")
                continue
            out.append((where, os.path.join(dirpath, f)))
    ok("the tree is areas holding python files, and nothing else")
    return out

# demos/packages against the tree and against learn.rst

LEARN = os.path.join(ROOT, "httpdoc", "xdoc_user", "doc", "learn.rst")

def read_packages():
    """[(number, title, [(demo, line)])] of demos/packages."""
    out = []
    path = os.path.join(TREE, "packages")
    if not os.path.isfile(path):
        bad("packages", "is missing: the Demo picker of the canvas has "
                        "nothing to show")
        return out
    for ln in open(path).read().split("\n"):
        if not ln.strip() or ln.startswith("#"):
            continue
        m = re.match(r"^(\d+)\s+(.+)$", ln)
        if m:
            out.append((m.group(1), m.group(2).strip(), []))
            continue
        m = re.match(r"^(\S+\.py)\s*(.*)$", ln)
        if m and out:
            out[-1][2].append((m.group(1), m.group(2).strip()))
        else:
            bad("packages", "line %r is neither a package nor a demo" % ln)
    return out

def read_learn():
    """The same shape from learn.rst; the c2c line carries no text there."""
    out = []
    for ln in open(LEARN).read().split("\n"):
        m = re.match(r"^Package (\d+) -- (.+)$", ln)
        if m:
            out.append((m.group(1), m.group(2).strip(), []))
            continue
        m = re.match(r"^\s+cloonix cli nemo demo (\S+\.py)\s+(.+)$", ln) or \
            re.match(r"^\s+/usr/share/cloonix/demos/(\S+\.py)()", ln)
        if m and out:
            out[-1][2].append((m.group(1), m.group(2).strip()))
    return out

def check_voice(scripts):
    path = os.path.join(TREE, "voice")
    if not os.path.exists(path):
        bad("voice", "is missing: the Demo panel has no phrase to say")
        return
    said = []
    for ln in open(path):
        if not ln.strip() or ln.startswith("#"):
            continue
        f = ln.split(None, 1)
        if len(f) < 2:
            bad("voice", "%s has no phrase" % f[0])
        said.append(f[0])
    for d in sorted(set(scripts) | set(said)):
        if said.count(d) != 1:
            bad("voice", "%s is listed %d time(s), not once"
                         % (d, said.count(d)))
        elif d not in scripts:
            bad("voice", "%s is not a demo of the tree" % d)
    # the clip of each phrase, audio/<area>/<demo>.ogg of jscloonix, or the
    # Demo panel gets a 404: made by debug/demo_voice.py
    audio = os.path.join(os.path.dirname(TREE), "cloonix", "cloonsrc",
                         "jscloonix", "jscloonix", "audio")
    if os.path.isdir(audio):
        for d in sorted(set(said)):
            if not os.path.exists(os.path.join(audio, d[:-3] + ".ogg")):
                bad("voice", "%s has no clip audio/%s.ogg: ./demo_voice.py %s"
                             % (d, d[:-3], d))
    ok("voice has one phrase and one clip for every demo")

def check_packages(scripts):
    packages = read_packages()
    listed = [d for _, _, demos in packages for d, _ in demos]
    for where in scripts:
        if listed.count(where) != 1:
            bad("packages", "%s is listed %d time(s), not once"
                            % (where, listed.count(where)))
    for d in listed:
        if d not in scripts:
            bad("packages", "%s is not a demo of the tree" % d)
    if not os.path.isfile(LEARN):
        print("  note  no learn.rst beside this tree, so the packages are "
              "not compared with the book")
        return
    learn = read_learn()
    if [(n, t) for n, t, _ in packages] != [(n, t) for n, t, _ in learn]:
        bad("packages", "its packages are not those of learn.rst, number "
                        "and title, in order")
        return
    for (num, _, mine), (_, _, theirs) in zip(packages, learn):
        if [d for d, _ in mine] != [d for d, _ in theirs]:
            bad("packages", "package %s: the demos are not those of "
                            "learn.rst, in its order" % num)
            continue
        for (d, text), (_, booktext) in zip(mine, theirs):
            if booktext and text != booktext:
                bad("packages", "%s: %r here, %r in learn.rst"
                                % (d, text, booktext))
    ok("packages is the tree, and the course of learn.rst")
    # the demo sections: demos.rst for demos/cloonix, demos_<part>.rst for the
    # rest; "routing/frrspider.py and frrspider_bgp.py" names two demos
    doc = os.path.dirname(LEARN)
    chapters = sorted(f for f in os.listdir(doc)
                      if re.match(r"^demos(_\w+)?\.rst$", f))
    seen = []
    for f in chapters:
        lines = open(os.path.join(doc, f)).read().split("\n")
        mine = []
        for i in range(1, len(lines)):
            if not (re.match(r"^={4,}$", lines[i]) and ".py" in lines[i - 1]):
                continue
            names = re.findall(r"[\w/]+\.py", lines[i - 1])
            area = names[0].rsplit("/", 1)[0] + "/" if "/" in names[0] else ""
            mine += [n if "/" in n or not area else area + n for n in names]
        want = [d for d in listed if d in mine]
        if [d for d in mine if d in listed] != want:
            bad("packages", "%s: sections %s, the order of packages is %s"
                            % (f, " ".join(mine), " ".join(want)))
        seen += mine
    for d in listed:
        if seen.count(d) != 1:
            bad("packages", "%s has %d section(s) in the demo chapters, not one"
                            % (d, seen.count(d)))
    for d in sorted(set(seen) - set(listed)):
        bad("packages", "a section of the demo chapters names %s, not a demo "
                        "of packages" % d)
    ok("the demo chapters have one section per demo, in the order of packages")

def check_history(scripts):
    # the HISTORY of a demo, told first by its story(), heads its section of
    # the demo chapters: one text, in the two places
    doc = os.path.dirname(LEARN)
    book = ""
    for f in sorted(os.listdir(doc)):
        if re.match(r"^demos(_\w+)?\.rst$", f):
            book += open(os.path.join(doc, f)).read() + "\n"
    book = re.sub(r"``([^`]+)``", r"\1", book)
    book = re.sub(r"\*([^*]+)\*", r"\1", book)
    book = re.sub(r"\s+", " ", book)
    told = 0
    for where, path in scripts:
        for node in ast.parse(open(path).read()).body:
            if (isinstance(node, ast.Assign)
                    and any(getattr(t, "id", "") == "HISTORY" for t in node.targets)):
                text = re.sub(r"\s+", " ", ast.literal_eval(node.value)).strip()
                told += 1
                if text not in book:
                    bad(where, "its HISTORY is not the paragraph that heads its "
                               "section of the demo chapters")
    ok("the %d HISTORY texts head their sections of the demo chapters" % told)

# one script

SUMMARY = re.compile(r'^\s*r?["\']{1,3}([a-z0-9_]+)\s+-\s+(.+)$')

def check_script(rel, path):
    with open(path) as f:
        text = f.read()

    try:
        tree = ast.parse(text)
    except SyntaxError as err:
        bad(rel, "does not compile: line %s, %s" % (err.lineno, err.msg))
        return None, None
    ok("%s compiles" % rel)

    lines = text.split("\n")
    if not lines or not lines[0].startswith("#!"):
        bad(rel, "has no shebang, so it cannot be run as a programme")

    m = SUMMARY.match(lines[1] if len(lines) > 1 else "")
    if not m:
        bad(rel, "line 2 is not '<name> - <one line>', so the demo list "
                 "will show something unhelpful beside it")
    else:
        want = os.path.basename(path)[:-3]
        if m.group(1) != want:
            bad(rel, "line 2 calls itself %r and the file is %r"
                     % (m.group(1), want))
        if len(m.group(2)) > 70:
            bad(rel, "the summary on line 2 is %d characters; it is cut at "
                     "52 in the list" % len(m.group(2)))

    # show(), story() and NET are the demo's own or come from demos/lib
    m = re.search(r"from demolib import \((.*?)\)", text, re.S)
    shared = set(re.findall(r"\w+", m.group(1))) if m else set()
    if m and not os.path.isfile(os.path.join(TREE, "lib", "demolib.py")):
        bad(rel, "imports demolib and demos/lib/demolib.py is missing")
    if "def show(line):" not in text and not shared:
        bad(rel, "has no show(): every command a demo runs must print "
                 "itself as \"cloonix cli ...\", ready to paste")
    if "def story(*args):" not in text and "story" not in shared:
        bad(rel, "has no story(): the description of the steps goes to the "
                 "Comments panel of the canvas, marked, and is not printed "
                 "on a terminal")
    keep = [(f.lineno, f.end_lineno) for f in ast.walk(tree)
            if isinstance(f, ast.FunctionDef) and f.name in ("show", "pause")]
    for node in ast.walk(tree):
        if not (isinstance(node, ast.Call) and
                isinstance(node.func, ast.Name) and node.func.id == "print"):
            continue
        if any(a <= node.lineno <= b for a, b in keep):
            continue
        if any(k.arg == "file" for k in node.keywords):
            continue
        first = node.args[0] if node.args else None
        while isinstance(first, ast.BinOp):
            first = first.left
        if (isinstance(first, ast.Constant) and isinstance(first.value, str)
                and re.match(r"(cloonix|tar) ", first.value)):
            continue
        bad(rel, "line %d prints to the console: only the cloonix commands "
                 "go there, the rest is story()" % node.lineno)

    if "sys.argv" not in text and "NET" not in shared:
        bad(rel, "never looks at sys.argv: every demo takes the net name "
                 "as its only argument")
    if '"nemo"' not in text and "'nemo'" not in text and "NET" not in shared:
        bad(rel, "does not default to the net nemo")

    if re.search(r'["\'\[]\s*start\s*["\'\],]', text) and "cloonix start" in text:
        for n, line in enumerate(lines, 1):
            if re.search(r'\[\s*["\']start["\']', line):
                bad(rel, "line %d looks like it starts a net itself; a demo "
                         "checks and tells the user to" % n)
                break

    images = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id.endswith("ROOTFS_CHOICES"):
                    try:
                        images = list(ast.literal_eval(node.value))
                    except (ValueError, SyntaxError):
                        pass
                elif isinstance(t, ast.Name) and t.id.endswith("_ROOTFS"):
                    try:
                        images.append(ast.literal_eval(node.value))
                    except (ValueError, SyntaxError):
                        pass
    player = re.search(r"^PLAYS_THE_PACKAGES\s*=\s*True\b", text, re.M)
    if player:
        ok("%s plays the demos of packages, each names its image" % rel)
    elif not images:
        bad(rel, "names no image: a ROOTFS_CHOICES tuple says what to build "
                 "when the demo will not start")
    else:
        ok("%s names %s" % (rel, ", ".join(images)))
    return images, tree

def check_guest_sources(rel, path, tree):
    """Every string a demo writes into a guest must compile as python.

    Read through ast rather than with a regexp over the file: the string
    put() sends is the EVALUATED one, so a backslash written twice here
    arrives once there. A regexp sees the two and calls valid source
    broken, which is the wrong half of the pair to check.

    A guest-side program is a string here and a program there, so nothing
    on this side ever compiles it. That is the whole reason for this.
    """
    tried = 0
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign) or len(node.targets) != 1:
            continue
        target = node.targets[0]
        if not isinstance(target, ast.Name) or not target.id.isupper():
            continue
        body = None
        if isinstance(node.value, ast.Constant) and \
                isinstance(node.value.value, str):
            body = node.value.value
        else:
            try:
                body = ast.literal_eval(node.value)
            except (ValueError, SyntaxError, TypeError):
                inner = node.value
                while isinstance(inner, ast.Call) and \
                        isinstance(inner.func, ast.Attribute):
                    inner = inner.func.value
                if isinstance(inner, ast.Constant) and \
                        isinstance(inner.value, str):
                    body = inner.value
        if not isinstance(body, str):
            continue
        if "import " not in body or "\n" not in body:
            continue
        tried += 1
        # The placeholders a demo substitutes before sending.
        err = None
        for quoted in (True, False):
            probe = body
            for word, stand in (("PORT", "8080"), ("GAP", "8"),
                                ("HOST", "1.2.3.4"), ("PEER", "1.2.3.4"),
                                ("ADDR", "1.2.3.4")):
                if word in ("HOST", "PEER", "ADDR") and not quoted:
                    stand = '"' + stand + '"'
                probe = re.sub(r"\b%s\b" % word, stand, probe)
            try:
                compile(probe, "%s:%s" % (rel, target.id), "exec")
                err = None
                break
            except SyntaxError as caught:
                err = caught
        if err is not None:
            bad(rel, "the guest-side %s does not compile: line %s, %s"
                     % (target.id, err.lineno, err.msg))
            continue
        for ch, why in (("\'", "a quote ends the printf argument"),
                        ("%", "printf reads a percent as a conversion")):
            if ch in body:
                bad(rel, "the guest-side %s contains %r and %s"
                         % (target.id, ch, why))
    if tried:
        ok("%s: %d guest-side source(s) compile and are printf-safe"
           % (rel, tried))

# images

# the capture filters

FILTER_WORDS = set("ip ip6 arp tcp udp icmp icmp6 src dst host net port "
                   "portrange less greater and or not".split())

def check_filters(rel, text):
    """Every sniff line a demo shows uses the words of pcap_filter.c only."""
    for n, line in enumerate(text.split("\n"), 1):
        m = re.search(r"\bsniff\s+\S+(?:\s+(?:eth\d+|wlan\d+|[AB]))?"
                      r"\s+--dump\b(.*)$", line)
        if not m or "%" in m.group(1).replace("%s", ""):
            continue
        words = re.findall(r"[A-Za-z_][A-Za-z0-9_]*", m.group(1).split("#")[0])
        for w in words:
            if w.lower() not in FILTER_WORDS and w != "s":
                bad(rel, "line %d: %r is not a word of the capture filter "
                         "(pcap_filter.c); a protocol with no word is caught "
                         "by exclusion, \"ip and not udp and not tcp\""
                         % (n, w))
                break

# the canvas

def check_angles(rel, path):
    """An interface is drawn on the side of its machine that faces its lan."""
    import math
    sys.path.insert(0, HERE)
    from demo_svg import constants, eth_numbers
    ns = constants(path)
    lay, eths, wiring = ns.get("LAYOUT"), ns.get("ETHS"), ns.get("WIRING")
    if not all(isinstance(d, dict) for d in (lay, eths, wiring)):
        return
    eths = eth_numbers(eths)
    for node, ifs in wiring.items():
        for one in ifs:
            num, lan = one[0], one[1]
            if node not in lay or lan not in lay or (node, num) not in eths:
                continue
            bearing = math.degrees(math.atan2(lay[lan][0] - lay[node][0],
                                              lay[node][1] - lay[lan][1]))
            off = abs((eths[(node, num)] - bearing + 180) % 360 - 180)
            if off > 90:
                bad(rel, "angle= of %s eth%d is %d degrees and its lan %s lies at "
                         "%d: the cable is drawn across the machine"
                         % (node, num, eths[(node, num)], lan, bearing % 360))

def built_images():
    """Every image name a cnt_*.sh in the tree can build."""
    got = {}
    for d in IMAGE_DIRS:
        if not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            if f.startswith("cnt_") and f.endswith(".sh"):
                got[f[4:-3]] = os.path.join(os.path.basename(d), f)
    return got

def lib_names(path):
    """The names a library of the demos defines or imports at its top."""
    names = set()
    for n in ast.parse(open(path).read()).body:
        if isinstance(n, (ast.FunctionDef, ast.ClassDef)):
            names.add(n.name)
        elif isinstance(n, (ast.Assign, ast.AnnAssign)):
            for t in (n.targets if isinstance(n, ast.Assign) else [n.target]):
                names.update(x.id for x in ast.walk(t)
                             if isinstance(x, ast.Name))
        elif isinstance(n, (ast.Import, ast.ImportFrom)):
            names.update((a.asname or a.name).split(".")[0] for a in n.names)
    return names

def check_lib_imports():
    """Every name taken from a library of the demos, the toys' included, is
    there: a demo whose import fails stops at its first line."""
    toys = os.path.join(HERE, "..", "toy_targz")
    libs = {}
    for path in (glob.glob(os.path.join(TREE, "lib", "*.py")) +
                 glob.glob(os.path.join(toys, "lib", "*.py")) +
                 glob.glob(os.path.join(toys, "*_demo", "lib", "*.py"))):
        libs.setdefault(os.path.basename(path)[:-3], set()).update(
            lib_names(path))
    files = (glob.glob(os.path.join(TREE, "**", "*.py"), recursive=True) +
             glob.glob(os.path.join(toys, "**", "*.py"), recursive=True))
    for path in sorted(files):
        for n in ast.walk(ast.parse(open(path).read())):
            if isinstance(n, ast.ImportFrom) and n.module in libs:
                for a in n.names:
                    if a.name != "*" and a.name not in libs[n.module]:
                        bad(os.path.relpath(path, os.path.join(HERE, "..")),
                            "imports %s from %s, which has no such name"
                            % (a.name, n.module))
    ok("every name the demos and their libraries import from one another is "
       "there")

def main():
    if not os.path.isdir(TREE):
        sys.exit("no demos tree at %s" % TREE)
    print("checking %s" % TREE)
    print()

    images_built = built_images()
    if not images_built:
        print("  note  no vm_create/cnt_* scripts found beside this tree, so "
              "image names are not checked")

    wanted = {}
    written = 0
    scripts = demo_scripts()
    check_packages([where for where, _ in scripts])
    check_history(scripts)
    # all.py says no phrase: each demo it plays says its own
    check_voice([where for where, path in scripts
                 if not re.search(r"^PLAYS_THE_PACKAGES\s*=\s*True\b",
                                  open(path).read(), re.M)])
    for where, path in scripts:
        written += 1
        got, tree = check_script(where, path)
        if tree is not None:
            check_guest_sources(where, path, tree)
            check_filters(where, open(path).read())
            check_angles(where, path)
            names = [n.name for n in tree.body
                     if isinstance(n, ast.FunctionDef)]
            for name in sorted(set(n for n in names if names.count(n) > 1)):
                bad(where, "defines %s twice: the second definition replaces "
                           "the first, and every earlier caller gets the "
                           "wrong one" % name)
            text = open(path).read()
            player = re.search(r"^PLAYS_THE_PACKAGES\s*=\s*True\b", text,
                               re.M)
            for need in ("erase_net(",) if player else ("erase_net(", "place("):
                if need not in text:
                    bad(where, "has no %s: a demo erases the net and places "
                               "its items before creating anything" % need)
        for image in (got or []):
            wanted.setdefault(image, []).append(where)

    check_lib_imports()

    if images_built:
        for image, users in sorted(wanted.items()):
            if image.endswith(".qcow2"):
                # A guest disk, made by hand or by vm_create/cisco: not a
                # container image and not built by any cnt_*.sh.
                ok("image %-18s is a guest disk, not a cnt image" % image)
            elif image in images_built:
                ok("image %-18s built by %s" % (image, images_built[image]))
            elif image in KNOWN_ABSENT:
                ok("image %-18s absent on purpose: %s"
                   % (image, KNOWN_ABSENT[image]))
            else:
                bad(", ".join(users),
                    "needs the image %r and no cnt_%s.sh builds it"
                    % (image, image))

    print()
    print("  %d demo(s), %d check(s) passed" % (written, CHECKED[0]))
    if not FAILURES:
        print("  nothing to report")
        return 0
    print()
    print("  %d PROBLEM(S)" % len(FAILURES))
    print("  " + "-" * 68)
    for where, why in FAILURES:
        print("  %s" % where)
        print("      %s" % why)
    return 1

sys.exit(main())
