#!/usr/bin/env python3
"""sub_web_ws_topo - the websocket the gui draws from: /topo upgrades, and
its first text frame is the topology snapshot, json with type topo, naming
the container that exists; /images upgrades and answers a frame too.
"""
import json
from sublib import *

C = "tisubWs"


def sub_web_ws_topo():
    clean(C)
    w = Web()
    try:
        add_cnt(C, "s")
        wait_topo(C, True)
        frame = w.websocket("/topo")
        try:
            msg = json.loads(frame)
        except ValueError:
            raise Fail("/topo first frame is not json: %r" % frame[:120])
        if msg.get("type") != "topo":
            raise Fail("/topo first frame has type %r" % msg.get("type"))
        if '"%s"' % C not in frame:
            raise Fail("/topo snapshot does not name %s" % C)
        frame = w.websocket("/images")
        try:
            json.loads(frame)
        except ValueError:
            raise Fail("/images first frame is not json: %r" % frame[:120])
        delete(C)
    finally:
        clean(C)


if __name__ == "__main__":
    run("web topo websocket", sub_web_ws_topo)
