#!/usr/bin/env python3
"""Every 'cloonix cli <net> ...' line of the docs, parsed by the client.

The line is expanded and turned into its wire json by the client's own
code, offline: a verb, a keyword or an argument count the client refuses
is a doc that no longer matches the code. Lines with a <placeholder> are
checked up to the verb only. The forms the client still takes from older
scripts, and the forms of before the redesign, are refused anywhere in the
tree: the docs, their figures, the demos, the tests and the scripts.
    ./check_doc_cli.py
"""
import glob
import html
import importlib.machinery
import importlib.util
import io
import os
import re
import shlex
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
CLIENT = os.path.join(ROOT, "cloonix", "cloonix")
DOCS = [os.path.join(ROOT, "httpdoc", "*.html"),
        os.path.join(ROOT, "httpdoc", "xdoc_user", "**", "*.rst"),
        os.path.join(ROOT, "httpdoc", "xdoc_dev", "**", "*.rst")]
# Where the older forms are looked for, beyond the docs.
TREE = [os.path.join(ROOT, "httpdoc", "**", "*.svg"),
        os.path.join(ROOT, "demos", "**", "*.py"),
        os.path.join(ROOT, "debug", "**", "*.py"),
        os.path.join(ROOT, "debug", "**", "*.sh"),
        os.path.join(ROOT, "vm_create", "**", "*.sh"),
        os.path.join(ROOT, "build_tools", "**", "*.sh"),
        os.path.join(ROOT, "toy_targz", "**", "*"),
        os.path.join(ROOT, "win_client", "*")]
# an interface word: a plain number, where eth<i>, wlan<i>, A or B is written
NUM = r"(?:\d+|%d)"
OLDER = [
    (re.compile(r"\beth=[sv]+\b"), "eth=<letters>, eth=<n> now"),
    (re.compile(r"--vwifi"), "wlan=, wlan= now"),
    (re.compile(r"\bdel sat\b"), "del sat, del <name> now"),
    (re.compile(r"\bcnf (?:eth|air|snf_eth|snf_air|a2b|nat|lan|cnt|c2c) "),
     "cnf <kind> <name>, cnf <name> [<interface>] key=value now"),
    (re.compile(r"\bdir=(?:[01]|%d)\b"), "dir=, A2B or B2A now"),
    (re.compile(r"\b(?:add|del) lan\b"), "add lan, del lan: plug, unplug now"),
    (re.compile(r"\b(?:un)?plug \S+ " + NUM + r" \S"),
     "an interface by number: eth<i>, wlan<i>, A or B, none for one"),
    (re.compile(r"\bsniff (?!\S*:)\S+ " + NUM + r"\b"),
     "an interface by number: eth<i>, wlan<i>, A or B, none for one"),
    (re.compile(r"\brmall\b"), "rmall, del all now"),
    (re.compile(r"\bposition \S+ -?\d|\bposeth \S+ (?:(?:eth|wlan)\d+|[AB]|-?\d+)\b|"
                r"\"(?:position|poseth)\", "),
     "position, poseth: cnf <n> x= y=, cnf <n> [<interface>] angle= now"),
    (re.compile(r"\badd phy \S+ (?:absorb|macvlan)\b"),
     "add phy <n> absorb|macvlan: add phy <n> [--macvlan] now"),
    (re.compile(r"\bwhatip="), "whatip=: dump ip <nat> <guest> now"),
    (re.compile(r"\bcli \S+ (?:kill|norton)\b"),
     "cli <net> kill|norton, cloonix stop <net> and files now"),
    (re.compile(r"\bcloonix net (?!called\b|started\b)[\w<${%]"),
     "cloonix net, cloonix start now"),
    (re.compile(r"\bdump (?:list|atopo|ftopo)\b"),
     "dump list|atopo|ftopo, dump commands|drawing|json now"),
    (re.compile(r"--ack-wait|\bcloonix -t\b"), "--ack-wait, -t: --wait now"),
    (re.compile(r"--(?:publish|startup_env|vmount|mac_addr|install_cdrom|"
                r"added_cdrom|added_disk)="), "--<value>=, <value>= now"),
]
# a clip, then editor and patch leftovers, which are no part of the tree
LEFTOVER = (".ogg", "~", ".orig", ".rej", ".bak", ".swp", ".swo")
# the client's own lines on what it still takes, and the tests of it
OLDER_OK = re.compile(r"older scripts|76 clients|as older|still names a wlan")


def older_forms():
    """(path, line, text, why) of every older form in the tree."""
    for pat in DOCS + TREE:
        for path in sorted(glob.glob(pat, recursive=True)):
            if not os.path.isfile(path) or path.endswith(LEFTOVER) or \
               path.endswith("check_doc_cli.py"):
                continue
            try:
                lines = open(path, encoding="utf-8").read().split("\n")
            except UnicodeDecodeError:
                continue
            for nb, line in enumerate(lines, 1):
                if "cloonix" not in line and "cli(" not in line and \
                   "sniff" not in line and "eth=" not in line:
                    continue
                if OLDER_OK.search(line) or \
                   (nb > 1 and OLDER_OK.search(lines[nb - 2])):
                    continue
                for rx, why in OLDER:
                    if rx.search(line):
                        yield path, nb, line.strip(), why
                        break


# Handled by the client without the wire json of to_json.
OWN_PATH = {"ssh", "scp", "files", "sniff", "trace", "demo", "rpc"}
# The refusals the docs show on purpose, as the client answers them.
SHOWN_REFUSED = {"nemo sn", "nemo add cnt", "nemo titi"}
LINE_RE = re.compile(r"cloonix\s+cli\s+(\S+)\s+(.*)")
# A shell operator or rst markup ends the command; a column of comment is
# tried away too, since two spaces also align the words of a command.
CUT_RE = re.compile(r"\s(\||;|&&|\|\||>|>>|2>|#|then)\s|`|\*|\)$")
COMMENT_RE = re.compile(r"\s{3,}")


def load_client():
    loader = importlib.machinery.SourceFileLoader("cloonix_client", CLIENT)
    spec = importlib.util.spec_from_loader("cloonix_client", loader)
    mod = importlib.util.module_from_spec(spec)
    loader.exec_module(mod)
    return mod


def doc_lines():
    for pat in DOCS:
        for path in sorted(glob.glob(pat, recursive=True)):
            text = open(path, encoding="utf-8").read()
            if path.endswith(".html"):
                text = html.unescape(re.sub(r"<[^>]+>", "", text))
            text = text.replace("\\\n", " ")
            for nb, line in enumerate(text.split("\n"), 1):
                m = LINE_RE.search(line)
                if m:
                    yield path, nb, m.group(1), m.group(2)


def check(cl, net, rest):
    """None when the client takes the line, else why not."""
    rest = CUT_RE.split(" " + rest + " ")[0].strip()
    if ("%s %s" % (net, rest)) in SHOWN_REFUSED:
        return None
    why = check_one(cl, net, rest)
    if why and COMMENT_RE.search(rest):
        if not check_one(cl, net, COMMENT_RE.split(rest)[0]):
            return None
    return why


def check_one(cl, net, rest):
    try:
        words = shlex.split(rest)
    except ValueError:
        return None
    if not words or net.startswith("<") or net.startswith("$") or \
       net in cl.CLI_VERBS or not re.match(r"^[A-Za-z0-9_.-]+$", net):
        return None
    try:
        words = cl.expand(["cli", net] + words)[2:]
    except (SystemExit, Exception):
        return None
    verb = words[0]
    if verb.startswith("<") or verb.startswith("$"):
        return None
    if verb not in cl.CLI_VERBS:
        return "no verb '%s' in the client" % verb
    if verb in OWN_PATH:
        return None
    if any(("<" in w) or ("$" in w) or ("..." in w) or ("…" in w)
           for w in words):
        return None
    err = io.StringIO()
    old, sys.stderr = sys.stderr, err
    try:
        js = cl.to_json(words, NETS, net if net in NETS else "nemo")
    except SystemExit as stop:
        return "refused by the client: %s" % (stop.code or err.getvalue())
    except Exception as exc:
        return "the client broke on it: %s: %s" % (type(exc).__name__, exc)
    finally:
        sys.stderr = old
    if js is None and verb not in ("dump",):
        return "the client builds nothing from it"
    if js is None and verb == "dump" and len(words) > 1 and \
       words[1] not in cl.DUMP_REQ and words[1] not in cl.OVS_TOOLS and \
       words[1] != "namespaces":
        return "no dump '%s' in the client" % words[1]
    return None


NETS = {}


def main():
    cl = load_client()
    NETS.update(cl.parse_cfg(open(os.path.join(ROOT, "cloonix",
                                               "cloonix_config.py")).read()))
    bad = nb = 0
    for path, line, net, rest in doc_lines():
        nb += 1
        why = check(cl, net, rest)
        if why:
            bad += 1
            print("DOC CLI: %s:%d: cloonix cli %s %s\n    %s"
                  % (os.path.relpath(path, ROOT), line, net, rest.strip(),
                     why))
    old = 0
    for path, line, text, why in older_forms():
        old += 1
        print("OLDER FORM: %s:%d: %s\n    %s"
              % (os.path.relpath(path, ROOT), line, text[:120], why))
    if bad or old:
        print("\n%d of %d cli line(s) of the docs do not match the client, "
              "%d older form(s) in the tree" % (bad, nb, old))
        return 1
    print("the %d cli line(s) of the docs all parse in the client" % nb)
    return 0


if __name__ == "__main__":
    sys.exit(main())
