#!/usr/bin/env python3
"""sub_item_note - cnf <n> note= is kept by the switch and replayed to a
topology subscriber (what the gui sends), spaces, accents and quotes intact;
dump commands gives a line that sets it back; too long, a control
character, a line dump commands cannot hold and a missing item are
refused; the POST /ctl/note of the canvas sets it too; note= removes it,
and a deleted item's note is forgotten. The refusal is logged as every KERR
line is: "KERR: <net> <file>.c:<function> line:<n> <text>".
"""
import json
import re
import shlex
import time
from sublib import *
import testitall as ti

C = "tisubNote"
SUB = '{"fctident":"event_topo_sub","tid":0}'
TEXT = "it's réseau A, area 0"


def notes():
    out = {}
    for m in raw_rpc(SUB):
        if m.get("fctident") == "item_note" and m.get("txt"):
            out[m.get("name")] = m.get("txt")
    return out


def rpc_note(txt, name=C):
    """The verbatim item_note, past the checks of the client: the exit code
    says the status_ok or status_ko of the switch."""
    return cli("rpc", json.dumps({"fctident": "item_note", "tid": 0,
                                  "name": name, "txt": txt}), ack=5)


def refused(r, what):
    if r.returncode == 0:
        raise Fail("%s: accepted by the switch" % what)


def note_line():
    for line in dump_list().splitlines():
        if " cnf %s note=" % C in line:
            return line
    return None


def sub_item_note():
    clean(C)
    try:
        add_cnt(C, "s")
        must(cli("cnf", C, "note=" + TEXT), "note")
        got = notes().get(C)
        if got != TEXT:
            raise Fail("note %r read back as %r" % (TEXT, got))

        line = note_line()
        if not line:
            raise Fail("dump commands has no note line for %s" % C)
        words = shlex.split(line)
        if words[-1] != "note=" + TEXT:
            raise Fail("dump commands line %r gives %r" % (line, words[-1]))
        must(cli("cnf", C, "note=other"), "note other")
        must(cli(*words[3:]), "the dump commands line run again")
        if notes().get(C) != TEXT:
            raise Fail("the dump commands line did not set the note back")

        refused(rpc_note("x" * 101), "a note of 101 bytes")
        refused(rpc_note("a\tb"), "a note with a tab")
        refused(rpc_note("'" * 60), "a note dump commands cannot hold")
        mark = ti.syslog_mark()
        refused(rpc_note("x", "tisubNoGhost"), "a note on no item")
        rx = re.compile(r"KERR: \S+ recv_from_client\.c:recv_item_note line:\d+ "
                        r"WARNING note refused: tisubNoGhost does not exist")
        end = time.time() + 15
        while time.time() < end and not rx.search(
                ti.syslog_after(mark).decode("utf-8", "replace")):
            time.sleep(1)
        if not rx.search(ti.syslog_after(mark).decode("utf-8", "replace")):
            raise Fail("no KERR line <file>.c:<function> line:<n> for the "
                       "refused note in %s" % ti.SYSLOG)
        if cli("cnf", "tisubNoGhost", "note=x").returncode == 0:
            raise Fail("cnf note= on an item that does not exist was accepted")
        if cli("cnf", C, "note=" + "y" * 101).returncode == 0:
            raise Fail("a note of 101 bytes was accepted by the client")
        if notes().get(C) != TEXT:
            raise Fail("a refused note changed the one in force")

        w = Web()
        st, _ = w.request("POST", "/ctl/note",
                          json.dumps({"name": C, "txt": "from the canvas"}),
                          "https://" + w.host)
        if st != 200:
            raise Fail("POST /ctl/note: %d" % st)
        end = time.time() + 15
        while time.time() < end and notes().get(C) != "from the canvas":
            time.sleep(1)
        if notes().get(C) != "from the canvas":
            raise Fail("POST /ctl/note did not reach the switch")

        must(cli("cnf", C, "note="), "note= removes")
        if C in notes():
            raise Fail("note= did not remove the note")
        if note_line():
            raise Fail("dump commands still has a note line after note=")

        must(cli("cnf", C, "note=" + TEXT), "note again")
        delete(C)
        if C in notes():
            raise Fail("the note of a deleted item is still replayed")
    finally:
        clean(C)


if __name__ == "__main__":
    run("item note", sub_item_note)
