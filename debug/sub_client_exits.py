#!/usr/bin/env python3
"""sub_client_exits - how the client ends when the line or its reader is
wrong: a reader that closes the pipe early ends it quietly with status 0,
an unknown verb, an unknown net and a wrong word end it with a status and
a sentence, never a python traceback.
"""
import os
import subprocess

from sublib import *


def client(argv, closed=False):
    """(status of the client, its stderr); closed: its stdout is a pipe
    whose reader has already gone, so its first write meets EPIPE."""
    cmd = list(CLOONIX) + argv
    show(" ".join(["cloonix"] + argv) + (" > a closed pipe" if closed else ""))
    if not closed:
        r = subprocess.run(cmd, capture_output=True, text=True,
                           stdin=subprocess.DEVNULL)
        return r.returncode, r.stderr
    rfd, wfd = os.pipe()
    os.close(rfd)
    try:
        r = subprocess.run(cmd, stdout=wfd, stderr=subprocess.PIPE, text=True,
                           stdin=subprocess.DEVNULL, timeout=60)
    finally:
        os.close(wfd)
    return r.returncode, r.stderr


def no_traceback(err, what):
    if "Traceback" in err or "the client itself broke" in err:
        raise Fail("%s: the client broke: %s" % (what, err.strip()[:300]))


def sub_client_exits():
    # An answer into a reader that has gone, as after head: quiet, status 0.
    for argv in (["help"], ["cli", NET, "dump", "commands"],
                 ["cli", NET, "dump", "topo"]):
        rc, err = client(argv, closed=True)
        no_traceback(err, " ".join(argv))
        if rc != 0 or err.strip():
            raise Fail("%s > closed pipe: status %d, stderr %r -- a closed pipe must "
                       "end the client quietly" % (" ".join(argv), rc,
                                                   err.strip()[:200]))
    rc, err = client(["nosuchverb"])
    no_traceback(err, "unknown verb")
    if rc == 0 or "unknown verb" not in err:
        raise Fail("unknown verb: status %d, stderr %r" % (rc, err[:200]))
    rc, err = client(["cli", "nosuchnet", "dump", "commands"])
    no_traceback(err, "unknown net")
    if rc == 0 or "not a net of the config" not in err:
        raise Fail("unknown net: status %d, stderr %r" % (rc, err[:200]))
    rc, err = client(["cli", NET, "titi"])
    no_traceback(err, "wrong word")
    if rc == 0 or "no verb called 'titi'" not in err:
        raise Fail("wrong word: status %d, stderr %r" % (rc, err[:200]))
    print("      a closed pipe ends it quietly; a wrong verb, net or word "
          "ends it with a status and a sentence")


if __name__ == "__main__":
    run("client_exits", sub_client_exits)
