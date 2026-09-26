#!/usr/bin/env python3
"""progress - a stdin -> stdout pass-through that adds, every N seconds, a
time stamp line: the clock, the time elapsed against the expected total,
the phase of the suite and what is pending on the current line (the demo
that runs).

The stamp comes on the clock and not on the lines, so a phase that stays
silent for half an hour is stamped all the same. It is a line of its own: an
unfinished line of the suite is held until its end. The expected total is
the duration of the last runs seen, given by the caller.

    ... | progress.py <expected_seconds> [seconds]   (seconds defaults to 60)
"""
import os
import re
import select
import sys
import time

expect = max(1, int(sys.argv[1]) if len(sys.argv) > 1 else 8000)
every = max(1, int(sys.argv[2]) if len(sys.argv) > 2 else 60)

PHASE = re.compile(r"^\d+\.\d* ")
start = time.time()
due = start + every
phase = ""
pending = b""
fd = sys.stdin.fileno()
out = sys.stdout.buffer

def hms(sec):
    return "%d:%02d:%02d" % (sec // 3600, sec // 60 % 60, sec % 60)

def stamp():
    el = int(time.time() - start)
    what = pending.decode("utf-8", "replace").strip()
    line = "      [%s . %s of ~%s expected" % (time.strftime("%H:%M:%S"),
                                              hms(el), hms(expect))
    if phase:
        line += " . phase %s" % phase
    if what:
        line += " . %s" % what
    out.write((line + "]\n").encode())
    out.flush()

while True:
    ready, _, _ = select.select([fd], [], [], max(0, due - time.time()))
    if ready:
        data = os.read(fd, 65536)
        if not data:
            break
        lines = (pending + data).split(b"\n")
        pending = lines.pop()
        for raw in lines:
            out.write(raw + b"\n")
            text = raw.decode("utf-8", "replace")
            if PHASE.match(text):
                phase = text.strip()
        out.flush()
    if time.time() >= due:
        stamp()
        due = time.time() + every
if pending:
    out.write(pending)
    out.flush()
