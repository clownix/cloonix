#!/usr/bin/env python3
"""special_needs - the files of an image that the demos of a specialised toy
need, printed one per line, relative to the root of the image.

    special_needs.py <image root> <needs file> <source>...

Taken: every command of the image in a command position (line start, after
; | & $( ` do then else) of a string of a source, docstrings, HISTORY and
CARD left out, or of a shell script taken, unless the needs file says
"!word" (a word of a configuration, not a command); every absolute path of
the image named there; the packages of the needs file, whole; the shared
libraries of all that, transitively. usr and etc only, without doc, man,
info, locale, lintian and the python test files. Standard library only.

"?<glob>" in the needs file: a file of the packages matching it is taken
only when its name, up to its first dot, is a word of a source.

"@python" in the needs file: of the python of the image, only what the
python scripts the demos write into the containers import, found by that
python itself in a chroot of the image: a module of a package brings the
whole package, encodings is whole; "@python:<module>" adds a module only
imported at run time. The modules those scripts import are written to the
file named by SPECIAL_NEEDS_PYMODS, for the check of the root of the toy.
"""
import ast
import fnmatch
import subprocess
import glob
import warnings
import os
import re
import shutil
import struct
import sys

ROOT = os.path.abspath(sys.argv[1])
SKIP = ("usr/share/doc/", "usr/share/man/", "usr/share/info/",
        "usr/share/locale/", "usr/share/lintian/",
        "usr/share/python3/runtime.d/")
WORD = re.compile(r"[A-Za-z0-9_.+/-]+")
SHELL = re.compile(rb"#! ?\S*/(env )?(ba|da)?sh\b")
CUT = re.compile(r"(?:^|[;&|`\n]|\$\(|\bdo\b|\bthen\b|\belse\b)\s*")
PROSE = ("HISTORY", "CARD")

def canon(rel, links=None):
    """rel inside ROOT with every symlink followed; the file symlinks met
    are added to links."""
    parts = [p for p in rel.strip("/").split("/") if p not in ("", ".")]
    done = []
    hops = 0
    while parts:
        p = parts.pop(0)
        if p == "..":
            if done:
                done.pop()
            continue
        cur = os.path.join(ROOT, *(done + [p]))
        if os.path.islink(cur):
            hops += 1
            if hops > 40:
                return None
            if links is not None and not parts:
                links.add("/".join(done + [p]))
            tgt = os.readlink(cur)
            if tgt.startswith("/"):
                done = []
            parts = [x for x in tgt.split("/") if x not in ("", ".")] + parts
            continue
        done.append(p)
    return "/".join(done)

def wanted(rel):
    base = os.path.basename(rel)
    if rel.endswith(".py") and (base.startswith("test_") or
                                base.endswith("_test.py") or
                                "/tests/" in rel or "/test/" in rel):
        return False
    return (rel.startswith("usr/") or rel.startswith("etc/")) and \
        not rel.startswith(SKIP)

def elf_deps(path):
    """DT_NEEDED and the RUNPATH/RPATH of an executable or shared object,
    None for anything else."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        return None
    if struct.unpack_from("<H", data, 16)[0] not in (2, 3):
        return None
    phoff, = struct.unpack_from("<Q", data, 32)
    phsz, phnum = struct.unpack_from("<HH", data, 54)
    loads, dyn = [], None
    for i in range(phnum):
        typ, _fl, off, vaddr, _pa, filesz = struct.unpack_from(
            "<IIQQQQ", data, phoff + i * phsz)
        if typ == 1:
            loads.append((vaddr, off, filesz))
        elif typ == 2:
            dyn = (off, filesz)
    if dyn is None:
        return [], []
    ents = []
    for i in range(dyn[1] // 16):
        tag, val = struct.unpack_from("<qQ", data, dyn[0] + i * 16)
        if tag == 0:
            break
        ents.append((tag, val))
    strtab = [v for t, v in ents if t == 5]
    if not strtab:
        return [], []
    base = next((o + strtab[0] - v for v, o, s in loads
                 if v <= strtab[0] < v + s), None)

    def string(off):
        end = data.index(b"\0", base + off)
        return data[base + off:end].decode()
    needed = [string(v) for t, v in ents if t == 1]
    paths = [string(v) for t, v in ents if t in (15, 29)]
    return needed, ":".join(paths).split(":") if paths else []

def ld_dirs():
    dirs = []
    for conf in ["etc/ld.so.conf"] + sorted(
            glob.glob(os.path.join(ROOT, "etc/ld.so.conf.d/*.conf"))):
        conf = os.path.join(ROOT, conf)
        if os.path.isfile(conf):
            for ln in open(conf):
                ln = ln.split("#")[0].strip()
                if ln.startswith("/"):
                    dirs.append(ln)
    return dirs + ["/lib", "/usr/lib"]

def exec_map():
    found = {}
    for d in ("usr/local/sbin", "usr/local/bin", "usr/sbin", "usr/bin",
              "sbin", "bin"):
        full = os.path.join(ROOT, d)
        if os.path.isdir(full):
            for n in os.listdir(full):
                found.setdefault(n, "%s/%s" % (d, n))
    return found

def package_files(globs):
    status = open(os.path.join(ROOT, "var/lib/dpkg/status")).read()
    names = re.findall(r"^Package: (\S+)\n(?:(?!\n\n).)*?^Status: install ok "
                       r"installed", status, re.M | re.S)
    out = []
    for g in globs:
        hit = [n for n in names if fnmatch.fnmatch(n, g)]
        if not hit:
            sys.exit("special_needs: no package %s in the image" % g)
        for n in hit:
            lst = [p for p in glob.glob(os.path.join(
                ROOT, "var/lib/dpkg/info/%s.list" % n)) + glob.glob(
                os.path.join(ROOT, "var/lib/dpkg/info/%s:*.list" % n))]
            for p in lst:
                out += [ln.strip() for ln in open(p) if ln.strip()]
    return out

def literals(src):
    """The strings of a python source, docstrings and HISTORY, CARD out."""
    tree = ast.parse(open(src).read())
    prose = set()
    for node in ast.walk(tree):
        body = getattr(node, "body", None)
        if isinstance(body, list) and body and isinstance(body[0], ast.Expr) \
                and isinstance(body[0].value, ast.Constant):
            prose.add(id(body[0].value))
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id in PROSE for t in node.targets):
            prose.add(id(node.value))
    return [n.value for n in ast.walk(tree) if isinstance(n, ast.Constant)
            and isinstance(n.value, str) and id(n) not in prose]

def py_imports(text):
    """Absolute imports of a python text, None if it does not parse or
    imports nothing: "from a import b" gives a and the candidate a.b."""
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            tree = ast.parse(text)
    except (SyntaxError, ValueError):
        return None
    mods, cands = set(), set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            mods.update(a.name for a in node.names)
        elif isinstance(node, ast.ImportFrom) and node.level == 0 and \
                node.module and node.module != "__future__":
            mods.add(node.module)
            cands.update("%s.%s" % (node.module, a.name) for a in node.names)
    return (mods, cands) if mods else None

PYRUN = r"""
import importlib, sys, sysconfig
direct = sys.argv[1].split(",") if sys.argv[1] else []
cands = sys.argv[2].split(",") if sys.argv[2] else []
soft = sys.argv[3].split(",") if sys.argv[3] else []
for m in direct:
    try:
        importlib.import_module(m)
    except Exception as e:
        print("MISS %s %s: %s" % (m, type(e).__name__, e))
for m in cands:
    try:
        importlib.import_module(m)
    except Exception:
        pass
for m in soft:
    try:
        importlib.import_module(m)
        print("OK " + m)
    except Exception:
        pass
try:
    "x".encode("idna")
except Exception:
    pass
print("STDLIB " + sysconfig.get_paths()["stdlib"])
print("STDLIB " + sysconfig.get_paths()["platstdlib"])
for mod in list(sys.modules.values()):
    f = getattr(mod, "__file__", None)
    if f:
        print("FILE " + f)
"""

def py_closure(direct, cands, soft):
    """The files of the python of the image that direct imports, run in a
    chroot of the image; soft, the imports of the python programs taken,
    as far as the image has them: (files, stdlib dirs, misses, soft ok)."""
    chroot = shutil.which("chroot", path="/usr/sbin:/sbin:/usr/bin:/bin")
    cmd = ["unshare", "-rm", chroot or "chroot", ROOT, "/usr/bin/python3",
           "-c", PYRUN,
           ",".join(sorted(direct)), ",".join(sorted(cands)),
           ",".join(sorted(soft))]
    r = subprocess.run(cmd, capture_output=True, text=True,
                       env={"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8"})
    if r.returncode != 0:
        sys.exit("special_needs: the python of the image does not run in a "
                 "chroot of it: %s" % (r.stderr.strip() or r.returncode))
    files, std, miss, ok = set(), [], [], set()
    for ln in r.stdout.splitlines():
        kind, _, rest = ln.partition(" ")
        if kind == "FILE":
            files.add(rest.lstrip("/"))
        elif kind == "STDLIB":
            std.append(rest.lstrip("/").rstrip("/") + "/")
        elif kind == "MISS":
            miss.append(rest)
        elif kind == "OK":
            ok.add(rest)
    return files, std, miss, ok

def py_take(files, std):
    """files mapped to what is taken: a package whole, encodings whole, a
    module alone; __pycache__ and tests left out."""
    out, pkgs = set(), set()
    for f in files:
        top = next((d for d in std if f.startswith(d)), None)
        if top is None:
            out.add(f)
            continue
        rel = f[len(top):]
        if "/" in rel and not rel.startswith("lib-dynload/"):
            pkgs.add(top + rel.split("/")[0])
        else:
            out.add(f)
    pkgs.update(d + "encodings" for d in std)
    for pkg in sorted(pkgs):
        for d, dn, fn in os.walk(os.path.join(ROOT, pkg)):
            dn[:] = [x for x in dn if x not in ("__pycache__", "test", "tests",
                                                "idle_test")]
            out.update(os.path.relpath(os.path.join(d, x), ROOT) for x in fn
                       if not x.endswith(".pyc"))
    return out, pkgs

def main():
    needs = [ln.split("#")[0].strip() for ln in open(sys.argv[2])]
    execs = exec_map()
    for n in needs:
        if n.startswith("!"):
            execs.pop(n[1:], None)
    dirs = ld_dirs()
    take, scanned, todo, missing = set(), set(), [], set()

    def add(rel, why):
        links = set()
        real = canon(rel, links)
        if real is None or not os.path.lexists(os.path.join(ROOT, real)):
            return
        if os.path.isdir(os.path.join(ROOT, real)):
            return
        take.update(ln for ln in links if wanted(ln))
        if wanted(real) and real not in take:
            take.add(real)
            todo.append(real)
            if real.split("/")[-2] in ("bin", "sbin"):
                sys.stderr.write("  %-28s for %s\n" % (real, why))

    def scan(text, why):
        for w in WORD.findall(text):
            if w.startswith("/"):
                add(w.rstrip(".,"), why)
        for seg in CUT.split(text):
            words = [w for w in seg.split() if "=" not in w] or [""]
            if words[0] in execs:
                add(execs[words[0]], why)

    for src in sys.argv[3:]:
        for text in literals(src):
            scan(text, os.path.basename(src))
    cond = [n[1:].lstrip("/") for n in needs if n.startswith("?")]
    words = set()
    for src in sys.argv[3:]:
        for text in literals(src):
            words.update(re.findall(r"[A-Za-z0-9_]+",
                                    re.sub(r"\\[a-z]", " ", text)))
    left = set()
    for p in package_files([n for n in needs if n and n[0] not in "!@?"]):
        rel = p.lstrip("/")
        if any(fnmatch.fnmatch(rel, c) for c in cond) and \
                os.path.basename(rel).split(".")[0] not in words:
            left.add(os.path.basename(rel))
            continue
        add(p, "needs")
    if left:
        sys.stderr.write("  TOY-NEEDS left out, no demo names them: %s\n"
                         % " ".join(sorted(left)))
    def drain():
        while todo:
            rel = todo.pop()
            full = os.path.join(ROOT, rel)
            with open(full, "rb") as f:
                head = f.readline(80)
            if SHELL.match(head) and rel not in scanned:
                scanned.add(rel)
                scan(open(full, errors="replace").read(), rel)
                continue
            deps = elf_deps(full)
            if not deps:
                continue
            needed, rpath = deps
            origin = "/" + os.path.dirname(rel)
            # its own directory last: a private library is found there by the
            # RUNPATH of whoever loads it (systemd)
            search = [d.replace("$ORIGIN", origin).replace("${ORIGIN}", origin)
                      for d in rpath if d] + dirs + [origin]
            for lib in needed:
                hit = next((os.path.join(d, lib) for d in search
                            if canon(os.path.join(d, lib)) and os.path.isfile(
                                os.path.join(ROOT, canon(os.path.join(d, lib))))),
                           None)
                if hit:
                    add(hit, rel)
                else:
                    missing.add("%s (for %s)" % (lib, rel))
    drain()
    if any(n == "@python" or n.startswith("@python:") for n in needs):
        direct, cands, nb = set(), set(), 0
        for src in sys.argv[3:]:
            for text in literals(src):
                got = py_imports(text)
                if got:
                    nb += 1
                    direct |= got[0]
                    cands |= got[1]
        direct |= {n[len("@python:"):] for n in needs
                   if n.startswith("@python:")}
        direct.discard("__main__")
        # the python programs taken (a package whole): what they import
        soft = set()
        for rel in sorted(take):
            if rel.startswith("usr/lib/python3."):
                continue
            full = os.path.join(ROOT, rel)
            if not os.path.isfile(full):
                continue
            with open(full, "rb") as f:
                head = f.readline(80)
            if rel.endswith(".py") or (head.startswith(b"#!") and
                                       b"python" in head):
                got = py_imports(open(full, errors="replace").read())
                if got:
                    soft |= got[0] | got[1]
        soft -= direct
        soft.discard("__main__")
        files, std, miss, ok = py_closure(direct, cands, soft)
        if miss:
            sys.exit("special_needs: the python of the image misses what a "
                     "demo script imports: %s" % "; ".join(miss))
        files, pkgs = py_take(files, std)
        for f in sorted(files):
            add(f, "python")
        # lib-dynload, even empty: without it python says at every start
        # "Could not find platform dependent libraries <exec_prefix>"
        for d in std:
            if os.path.isdir(os.path.join(ROOT, d + "lib-dynload")):
                take.add(d + "lib-dynload")
        size = sum(os.path.getsize(os.path.join(ROOT, f)) for f in files
                   if os.path.isfile(os.path.join(ROOT, f)))
        sys.stderr.write("  TOY-PY %d scripts import: %s\n"
                         % (nb, " ".join(sorted(direct))))
        if ok:
            sys.stderr.write("  TOY-PY the python programs taken import: %s\n"
                             % " ".join(sorted(m for m in ok if "." not in m)))
        sys.stderr.write("  TOY-PY %d files, %d kB, whole packages: %s\n"
                         % (len(files), size // 1024,
                            " ".join(sorted(os.path.basename(p) for p in pkgs))))
        if os.environ.get("SPECIAL_NEEDS_PYMODS"):
            with open(os.environ["SPECIAL_NEEDS_PYMODS"], "w") as f:
                f.write("\n".join(sorted(direct | ok)) + "\n")
        drain()
    if missing:
        sys.exit("special_needs: not in the image: %s"
                 % ", ".join(sorted(missing)))
    print("\n".join(sorted(take)))

main()
