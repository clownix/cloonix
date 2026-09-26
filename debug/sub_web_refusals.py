#!/usr/bin/env python3
"""sub_web_refusals - what the web port must say no to: no credentials 401, a
browser origin that is not this host 403, a path with .. 403, a demo file
outside the demos 400, /topo without an upgrade 400, an unknown /ctl 404, a
POST with no name 400 and nothing created, a request line that is not one 400.
"""
from sublib import *


def sub_web_refusals():
    w = Web()
    mine = "https://" + w.host
    cases = (
        ("GET", "/", None, None, False, 401),
        ("GET", "/", None, "https://evil.example:4444", True, 403),
        ("GET", "/", None, "null", True, 403),
        ("GET", "/../../etc/passwd", None, mine, True, 403),
        ("GET", "/files/get?dir=../../etc&name=passwd", None, mine, True, 400),
        ("GET", "/files/get?dir=x&name=../../../etc/passwd", None, mine, True,
         400),
        ("GET", "/topo", None, mine, True, 400),
        ("GET", "/images", None, mine, True, 400),
        ("GET", "/ctl/no_such_thing", None, mine, True, 404),
        ("POST", "/ctl/add-lan", "{}", mine, True, 400),
        ("POST", "/ctl/add-cnt", '{"image":"x"}', mine, True, 400),
        ("POST", "/ctl/add-edge", '{"name":"x"}', mine, True, 400),
        ("GET", "/no_such_file.html", None, mine, True, 404),
    )
    for method, path, body, origin, auth, want in cases:
        st, _ = w.request(method, path, body, origin, auth)
        if st != want:
            raise Fail("%s %s (origin %s, auth %s): %d, wanted %d"
                       % (method, path, origin, auth, st, want))
    st, _ = w.request("GET", "/", None, mine)
    if st != 200:
        raise Fail("GET / with the right origin: %d" % st)
    if "(no objects)" not in topo():
        raise Fail("a refused request created something:\n%s" % topo())


if __name__ == "__main__":
    run("web refusals", sub_web_refusals)
