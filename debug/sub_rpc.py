#!/usr/bin/env python3
"""sub_rpc - rpc sends one json_interface.c message verbatim: a tap_add makes
the tap, a del_name removes it, a message with no fctident the server knows
is not acknowledged and the client says so and returns non-zero.
"""
from sublib import *

T = "tisubRpc"


def sub_rpc():
    clean(T)
    try:
        must(cli("rpc", '{"fctident":"tap_add","tid":0,"name":"%s"}' % T,
                 ack=30), "rpc tap_add")
        wait_topo(T, True)
        must(cli("rpc", '{"fctident":"del_name","tid":0,"name":"%s"}' % T,
                 ack=30), "rpc del_name")
        wait_topo(T, False)
        r = cli("rpc", '{"fctident":"no_such_message","tid":0}', ack=3)
        if r.returncode == 0:
            raise Fail("an unknown fctident was reported ok")
        if not (r.stdout + r.stderr).strip():
            raise Fail("an unknown fctident failed with no word")
        r = cli("rpc", "this is not json")
        if r.returncode == 0:
            raise Fail("rpc accepted a non json argument")
    finally:
        clean(T)


if __name__ == "__main__":
    run("rpc verbatim", sub_rpc)
