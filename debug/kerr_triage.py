#!/usr/bin/env python3
"""kerr_triage - the KERR lines of a run, folded into one message per kind.

A run leaves hundreds of thousands of syslog lines and a few hundred kinds
of message. Reading them in order hides the frequent ones among the rare;
reading them by frequency hides the rare ones under the frequent. This
folds them: one row per kind, counted, with the phase it first appeared in
and one real example kept whole.

    ./kerr_triage.py kerr1_outputs.log [kerr2_outputs.log ...]

Every row is something to act on or to recognise. The blocks at the top are
the ones that cannot wait: lines the log daemon dropped, so the record has
holes in it, and messages that carry no sentence, only values.
"""
import re
import sys
from collections import OrderedDict

# what changes run to run and must not split one kind into many rows
# applied to the text after the KERR head only, never to a whole line
VOLATILE = [
    (re.compile(r"0x[0-9a-fA-F]+"), "0xH"),
    (re.compile(r"\b[0-9a-fA-F]{8}-[0-9a-fA-F-]{20,}\b"), "UUID"),
    (re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b"), "IP"),
    # digits glued to a name: the soak builds skc3946442, _b_skl3952678,
    # _k_1_357_0. Without this one kind of message becomes one row per run.
    (re.compile(r"(?<=[A-Za-z_])\d+"), "N"),
    (re.compile(r"\b\d+\b"), "N"),
]

# "KERR: <net> <file>.c:<function> line:<n> <text>", the function is kept
HEAD = re.compile(r"KERR:\s+(\S*)\s+(\S+?\.c(?::\w+)?) line:(\d+)\s*(.*)$",
                  re.S)
# "KERR: <net> guest <name>: [<file>.c:<fn> line:<n>] <text>", a guest's own
GUEST = re.compile(r"^\s*\S*\s+guest\s+\S+:\s+(?:(\S+?\.c(?::\w+)?) line:(\d+)\s*)?(.*)$",
                   re.S)
STAMP = re.compile(r"^(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d)")
# a marker names what follows it, not the rest of the run: past this a line
# is only known to come after that step, and saying otherwise misleads
STALE_S = 600
LEVEL = re.compile(r"^(ERROR|WARNING|KO|INFO|[A-Z]*TRACE)\b")
PHASE = re.compile(r"CLOONIX-PHASE (.+?)\s*$")
DROPPED = re.compile(r"(\d+) line\(s\) before this one were dropped")
WORD = re.compile(r"\b[a-z]{3,}\b")


def secs(stamp):
    h, m, sec = stamp[11:].split(":")
    return int(h) * 3600 + int(m) * 60 + int(sec)


def fold(text):
    for pat, rep in VOLATILE:
        text = pat.sub(rep, text)
    return text.strip()


def daemon_of(line):
    m = re.search(r"\s(\S+?)(?:\[\d+\])?:\s+KERR:", line)
    return m.group(1) if m else "?"


def read(paths):
    rows = OrderedDict()
    phase = "(before the first phase)"
    phase_at = None
    dropped = []
    total = 0
    for path in paths:
        try:
            fd = open(path, errors="replace")
        except OSError as err:
            print("cannot read %s: %s" % (path, err))
            continue
        with fd:
            for line in fd:
                ms = STAMP.match(line)
                when = ms.group(1) if ms else None
                mp = PHASE.search(line)
                if mp:
                    phase = mp.group(1)
                    phase_at = when
                    continue
                if "KERR:" not in line:
                    continue
                total += 1
                here = phase
                if phase_at and when and secs(when) - secs(phase_at) > STALE_S:
                    here = "after %s" % phase
                md = DROPPED.search(line)
                if md:
                    dropped.append((int(md.group(1)), phase))
                m = HEAD.search(line)
                if m:
                    net, src, num, rest = m.groups()
                    key = (src, num, fold(rest))
                else:
                    rest = line.split("KERR:", 1)[1]
                    mg = GUEST.search(rest)
                    if mg:
                        src, num, rest = mg.groups()
                        # "<daemon>: LEVEL ...": the level after its daemon,
                        # the daemon named where the guest line has no file
                        md = re.match(r"^(\S+): (?=(ERROR|KO|WARNING|INFO|TRACE)\b)",
                                      rest)
                        if md:
                            rest = rest[md.end():]
                            src = src or md.group(1)
                        key = ("guest " + (src or ""), num or "", fold(rest))
                    else:
                        key = ("", "", fold(rest))
                row = rows.get(key)
                if row is None:
                    rows[key] = row = {"n": 0, "first": here,
                                       "sample": line.rstrip(),
                                       "daemon": daemon_of(line),
                                       "phases": set()}
                row["n"] += 1
                row["phases"].add(here)
    return rows, dropped, total


def level_of(text):
    m = LEVEL.match(text)
    if not m:
        return "-"
    return "TRACE" if m.group(1).endswith("TRACE") else m.group(1)


def report(paths):
    rows, dropped, total = read(paths)
    print("=" * 74)
    print("KERR triage of %s" % ", ".join(paths))
    print("=" * 74)
    print("  %d KERR line(s), %d distinct message(s)" % (total, len(rows)))
    levels = {}
    for (src, num, text), row in rows.items():
        levels[level_of(text)] = levels.get(level_of(text), 0) + 1
    print("  by level: %s" % ", ".join("%s %d" % (k, levels[k])
                                       for k in sorted(levels)))

    if dropped:
        print()
        print("  LINES LOST: the log daemon did not keep up, so this record "
              "has holes")
        print("  %d report(s), %d line(s) gone, first in phase %s"
              % (len(dropped), sum(n for n, _ in dropped), dropped[0][1]))

    # fewer than two plain words: the line says what it saw, never what
    # happened, and means nothing to whoever reads it next month
    mute = [(k, r) for k, r in rows.items()
            if len(WORD.findall(LEVEL.sub("", k[2]))) < 2]
    if mute:
        print()
        print("  MESSAGES WITH NO SENTENCE (values only, unreadable later):")
        for (src, num, text), row in mute:
            print("    %6d  %-24s %s"
                  % (row["n"], "%s:%s" % (src, num), text[:110]))

    for want in ("ERROR", "KO", "WARNING", "-", "TRACE"):
        part = [(k, r) for k, r in rows.items() if level_of(k[2]) == want]
        if not part:
            continue
        part.sort(key=lambda kr: -kr[1]["n"])
        print()
        print("-" * 74)
        print("%s  (%d kind(s))" % (want, len(part)))
        print("-" * 74)
        for (src, num, text), row in part:
            where = "%s:%s" % (src, num)
            ph = row["first"]
            if len(row["phases"]) > 1:
                ph += " +%d more" % (len(row["phases"]) - 1)
            print("%6d  %-26s %s" % (row["n"], where, ph))
            print("        %s" % text[:200])
    print()
    print("=" * 74)
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    sys.exit(report(sys.argv[1:]))
