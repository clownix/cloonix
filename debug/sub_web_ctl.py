#!/usr/bin/env python3
"""sub_web_ctl - the control api of the web gui, driven as the browser drives
it: add-cnt, add-lan, add-edge, del-edge, del-node, rma as POSTs with the
json bodies web_ctl_post.c reads; each seen by dump topo; GET /ctl/topo is a
json snapshot naming the objects; GET /ctl/commands, the Commands button of
the Demo panel, is the dump commands, the cabling written eth0.
"""
import json
import time
from sublib import *

A, B, L = "tisubWa", "tisubWb", "tisubWl"


def sub_web_ctl():
    clean(A, B)
    w = Web()
    origin = "https://" + w.host
    try:
        for n in (A, B):
            body = json.dumps({"name": n, "brandtype": "brandcnt",
                               "image": CNT_ROOTFS, "nb_eth": 1,
                               "eth": [{"type": "eths"}]})
            st, _ = w.request("POST", "/ctl/add-cnt", body, origin)
            if st != 200:
                raise Fail("POST /ctl/add-cnt %s: %d" % (n, st))
        for n in (A, B):
            wait_topo(n, True, timeout=60)
        st, _ = w.request("POST", "/ctl/add-lan", json.dumps({"name": L}),
                          origin)
        if st != 200:
            raise Fail("POST /ctl/add-lan: %d" % st)
        for n in (A, B):
            st, _ = w.request("POST", "/ctl/add-edge",
                              json.dumps({"name": n, "num": 0, "lan": L}),
                              origin)
            if st != 200:
                raise Fail("POST /ctl/add-edge %s: %d" % (n, st))
        end = time.time() + 30
        while time.time() < end and "%s eth0" % B not in topo():
            time.sleep(1)
        wait_answer(A)
        wait_answer(B)
        address(A, 0, "10.23.0.1/24")
        address(B, 0, "10.23.0.2/24")
        ping(A, "10.23.0.2")
        st, body = w.request("GET", "/ctl/commands", None, origin)
        if st != 200:
            raise Fail("GET /ctl/commands: %d" % st)
        for want in ("add cnt %s " % A, "plug %s eth0 %s" % (A, L),
                     "plug %s eth0 %s" % (B, L)):
            if want not in body:
                raise Fail("GET /ctl/commands lacks \"%s\":\n%s"
                           % (want, body))
        st, body = w.request("GET", "/ctl/topo", None, origin)
        if st != 200:
            raise Fail("GET /ctl/topo: %d" % st)
        snap = json.loads(body)
        text = json.dumps(snap)
        for n in (A, B, L):
            if '"%s"' % n not in text:
                raise Fail("/ctl/topo does not name %s" % n)
        st, _ = w.request("POST", "/ctl/del-edge",
                          json.dumps({"name": B, "num": 0, "lan": L}), origin)
        if st != 200:
            raise Fail("POST /ctl/del-edge: %d" % st)
        end = time.time() + 30
        while time.time() < end and "%s eth0" % B in topo():
            time.sleep(1)
        if "%s eth0" % B in topo():
            raise Fail("del-edge did not unplug %s" % B)
        st, _ = w.request("POST", "/ctl/del-node", json.dumps({"name": B}),
                          origin)
        if st != 200:
            raise Fail("POST /ctl/del-node: %d" % st)
        wait_topo(B, False, timeout=60)
        st, _ = w.request("POST", "/ctl/rma", "", origin)
        if st != 200:
            raise Fail("POST /ctl/rma: %d" % st)
        wait_topo(A, False, timeout=60)
        if "(no objects)" not in topo():
            raise Fail("rma from the web left objects:\n%s" % topo())
    finally:
        clean(A, B)


if __name__ == "__main__":
    run("web control api", sub_web_ctl)
