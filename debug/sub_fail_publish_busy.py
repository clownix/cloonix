#!/usr/bin/env python3
"""sub_fail_publish_busy - publish= on a host port something else already
holds: passt cannot bind it, proxymous sees it exit within half a second and
puts passt back on the previous ports, the server answers ko "passt could not
hold the ports" at once, the nat still carries traffic, and a publish on a
free port works right after.
"""
import socket
import threading
import http.server
import socketserver
from sublib import *

C, N, L = "tisubPb", "tisubPbn", "tisubPbl"


class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass


def fetch(port):
    return ssh(C, "exec 3<>/dev/tcp/10.117.0.1/%d; printf 'GET / HTTP/1.0"
               "\\r\\n\\r\\n' >&3; head -c 12 <&3" % port)


def sub_fail_publish_busy():
    clean(C, N)
    srv = socketserver.TCPServer(("127.0.0.1", 0), Quiet)
    port = srv.server_address[1]
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    show("python3 -m http.server %d --bind 127.0.0.1" % port)
    busy = socket.socket()
    busy.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    busy.bind(("0.0.0.0", 0))
    busy.listen(1)
    bport = busy.getsockname()[1]
    show("a listener of this test holds 0.0.0.0:%d" % bport)
    try:
        must(cli("add", "nat", N), "add nat")
        wait_topo(N, True)
        must(cli("plug", N, L), "plug")
        add_cnt(C, "s")
        must(cli("plug", C, "eth0", L), "plug")
        wait_answer(C)
        address(C, 0, "10.117.0.10/24")
        ssh(C, "ip route replace default via 10.117.0.1")
        if not fetch(port).startswith("HTTP/1.0 200"):
            raise Fail("the nat does not carry traffic before the test")
        r = cli("cnf", N, "publish=%d:8080" % bport, ack=90)
        if r.returncode == 0:
            raise Fail("a publish on a port already held was reported ok")
        why = (r.stdout + r.stderr).strip()
        if "could not hold" not in why:
            raise Fail("the refusal does not say passt could not hold the "
                       "ports: %s" % why)
        if not fetch(port).startswith("HTTP/1.0 200"):
            raise Fail("the nat does not carry traffic after the refusal")
        must(cli("cnf", N, "publish=%d:8080" % host_port_free(),
                 ack=90), "cnf publish on a free port after the failure")
        must(cli("cnf", N, "publish=none", ack=90),
             "cnf <nat> publish=none")
        if not fetch(port).startswith("HTTP/1.0 200"):
            raise Fail("the nat does not carry traffic after the recovery")
        delete(C)
        delete(N)
    finally:
        busy.close()
        srv.shutdown()
        clean(C, N)


if __name__ == "__main__":
    run("publish on a busy port", sub_fail_publish_busy)
