#!/usr/bin/env python3
"""sub_a2b_replace - the a2b settings that shaping does not reach: qsize and
bsize, and the address rewrite of each direction.

Two hosts on one wire made of two lans joined by an a2b. B carries a second
address on the same eth0, and the rewrite is used as a one-way nat: A2B
turns the destination of what crosses into that second address, B2A turns
the source of the answer back. The proof is the address ping reports each
answer coming from, not the loss: with only the A2B rule the answers arrive
from the second address, with both they arrive from the one A wrote to.
qsize and bsize are set, read back in dump commands, and the wire still
carries a ping under them.
"""
import re

from sublib import *

A, B = "tisubARa", "tisubARb"
LAN_A, LAN_B = "tisubARla", "tisubARlb"
CABLE = "tisubARc"

IP_A = "10.61.0.1"
IP_B = "10.61.0.2"
IP_HIDDEN = "10.61.0.8"
PINGS = 6


def cnf(way, *rest):
    must(cli("cnf", CABLE, way, *rest),
         "cnf %s %s %s" % (CABLE, way, " ".join(rest)))


def replies_from(node, target, what):
    """The set of addresses the answers to a ping say they come from."""
    out = ssh(node, "ping -c %d -i 0.3 -W 2 %s" % (PINGS, target),
              check=False)
    src = set(re.findall(r"bytes from (?:[^ ]+ )?\(?"
                         r"(\d+\.\d+\.\d+\.\d+)\)?", out))
    if not src:
        raise Fail("%s: no answer at all to a ping of %s:\n%s"
                   % (what, target, out))
    return src, out


def dump_has(line, present, what):
    body = dump_list()
    if (line in body) != present:
        raise Fail("%s: dump commands %s\n%s"
                   % (what, "still has it" if present is False
                      else "does not replay it", body))


def sub_a2b_replace():
    clean(A, B, CABLE)
    try:
        add_cnt(A, "s")
        add_cnt(B, "s")
        must(cli("add", "a2b", CABLE), "add a2b")
        must(cli("plug", A, "eth0", LAN_A), "plug a")
        must(cli("plug", CABLE, "A", LAN_A), "cable side A on lan a")
        must(cli("plug", B, "eth0", LAN_B), "plug b")
        must(cli("plug", CABLE, "B", LAN_B), "cable side B on lan b")
        wait_answer(A)
        wait_answer(B)
        address(A, 0, "%s/24" % IP_A)
        address(B, 0, "%s/24" % IP_B)
        ssh(B, "ip addr add %s/24 dev eth0" % IP_HIDDEN)

        src, out = replies_from(A, IP_B, "clean cable")
        if src != {IP_B}:
            raise Fail("on a clean cable a ping of %s must be answered by %s "
                       "alone, answered by %s:\n%s"
                       % (IP_B, IP_B, ", ".join(sorted(src)), out))
        loss, _ = ping_stats(A, IP_HIDDEN, PINGS)
        if loss != 0:
            raise Fail("%s does not answer on its second address: %d%% loss"
                       % (B, loss))
        print("      clean cable: %s and %s both answer" % (IP_B, IP_HIDDEN))

        # qsize and bsize: taken, replayed, and the wire still carries
        print("      qsize and bsize, each direction")
        for way in ("A2B", "B2A"):
            cnf(way, "qsize=64")
            cnf(way, "bsize=2")
            dump_has("cnf %s %s qsize=64" % (CABLE, way), True,
                     "qsize on %s" % way)
            dump_has("cnf %s %s bsize=2" % (CABLE, way), True,
                     "bsize on %s" % way)
        loss, _ = ping_stats(A, IP_B, PINGS)
        if loss != 0:
            raise Fail("qsize=64 bsize=2 stopped the ping: %d%% loss" % loss)

        # a rate sizes both again and takes them back off the dump
        for way in ("A2B", "B2A"):
            cnf(way, "rate=0")
            dump_has("cnf %s %s qsize=" % (CABLE, way), False,
                     "qsize after rate= on %s" % way)
            dump_has("cnf %s %s bsize=" % (CABLE, way), False,
                     "bsize after rate= on %s" % way)

        # the rewrite, one direction only: B answers on the other address
        print("      replace_ipdst on A2B alone: %s answers" % IP_HIDDEN)
        cnf("A2B", "replace_ipdst", IP_B, "with", IP_HIDDEN)
        dump_has("cnf %s A2B replace_ipdst %s with %s"
                 % (CABLE, IP_B, IP_HIDDEN), True, "replace_ipdst")
        src, out = replies_from(A, IP_B, "replace_ipdst on A2B")
        if src != {IP_HIDDEN}:
            raise Fail("A2B rewrites the destination %s into %s, so a ping of "
                       "%s must be answered by %s; answered by %s:\n%s"
                       % (IP_B, IP_HIDDEN, IP_B, IP_HIDDEN,
                          ", ".join(sorted(src)), out))

        # the other direction turns the answer back
        print("      replace_ipsrc on B2A as well: %s answers again" % IP_B)
        cnf("B2A", "replace_ipsrc", IP_HIDDEN, "with", IP_B)
        dump_has("cnf %s B2A replace_ipsrc %s with %s"
                 % (CABLE, IP_HIDDEN, IP_B), True, "replace_ipsrc")
        src, out = replies_from(A, IP_B, "replace_ipsrc on B2A")
        if src != {IP_B}:
            raise Fail("B2A rewrites the source %s back into %s, so the "
                       "answers must say %s; they say %s:\n%s"
                       % (IP_HIDDEN, IP_B, IP_B, ", ".join(sorted(src)), out))

        # and it is undone
        print("      unreplace both: the cable is plain again")
        cnf("A2B", "unreplace_ipdst")
        cnf("B2A", "unreplace_ipsrc")
        dump_has("cnf %s A2B replace_ipdst" % CABLE, False, "unreplace_ipdst")
        dump_has("cnf %s B2A replace_ipsrc" % CABLE, False, "unreplace_ipsrc")
        src, out = replies_from(A, IP_B, "after unreplace")
        if src != {IP_B}:
            raise Fail("after unreplace the answers must come from %s again, "
                       "they come from %s:\n%s"
                       % (IP_B, ", ".join(sorted(src)), out))

        delete(A)
        delete(B)
        delete(CABLE)
    finally:
        clean(A, B, CABLE)


if __name__ == "__main__":
    run("a2b qsize, bsize and address rewrite", sub_a2b_replace)
