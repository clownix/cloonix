#!/usr/bin/env python3
"""sub_c2c_udp_garbage - the udp port of a c2c is open to anybody: datagrams
that are not seqtap (short, bad magic, a length that lies, a probe with no
number, an unknown kind) are dropped. The c2c process is the same one
afterwards, and the ping still crosses.
"""
from sublib import *

C, P = "tisubUGc", "tisubUGp"
L, LP = "tisubUGl", "tisubUGlp"
C2C = "%s2%s" % (NET, PEER)
BAD = [b"\x00", b"\xca\xfe", b"junk" * 300,
       b"\xca\xfe\xde\xca\x05\xdc\x00\x01" + b"A" * 10,    # says 1500, holds 10
       b"\xca\xfe\xde\xca\xff\xff\x00\x01" + b"A" * 100,   # length over the max
       b"\xca\xfe\xbe\xef\x00\x05\x00\x00hello",           # probe, no number
       b"\xca\xfe\xbe\xef\x00\x09\x00\x00",                # probe, length lies
       b"\xca\xfe\xaa\xbb\x00\x00\x00\x00",                # ready, not for udp
       b"\xca\xfe\x12\x34\x00\x04\x00\x00abcd"]            # unknown kind


def c2c_pids():
    r = subprocess.run(["pgrep", "-f", "cloonix-ovs-c2c"], capture_output=True,
                       text=True)
    return sorted(r.stdout.split())


def udp_port():
    for c in ftopo(NET).get("c2c", []) or []:
        if c.get("name") == C2C:
            return int(c.get("loc_udp_port") or 0)
    return 0


def sub_c2c_udp_garbage():
    clean(C, C2C)
    if in_topo(P, PEER):
        cli("del", P, net=PEER, quiet=True)
        wait_topo(P, False, net=PEER)
    try:
        add_cnt(C, "s")
        add_cnt(P, "s", net=PEER)
        must(cli("plug", C, "eth0", L), "plug")
        must(cli("plug", P, "eth0", LP, net=PEER), "plug peer")
        must(cli("add", "c2c", C2C, PEER), "add c2c")
        end = time.time() + 60
        while not (in_topo(C2C) and in_topo(C2C, PEER)
                   and ti.c2c_peered(C2C, NET)):
            if time.time() > end:
                raise Fail("the c2c is not peered in 60s\n"
                           + ti.c2c_evidence(C2C))
            time.sleep(1)
        must(cli("plug", C2C, L), "plug c2c")
        must(cli("plug", C2C, LP, net=PEER), "plug c2c peer")
        wait_answer(C)
        wait_answer(P, PEER)
        address(C, 0, "10.5.6.1/24")
        address(P, 0, "10.5.6.2/24", net=PEER)
        end = time.time() + 30
        while not ping(C, "10.5.6.2", 1, check=False):
            if time.time() > end:
                raise Fail("nothing crosses the c2c\n" + ti.c2c_evidence(C2C))
            time.sleep(1)
        port, before = udp_port(), c2c_pids()
        if not port:
            raise Fail("dump json gives no loc_udp_port for %s" % C2C)
        ip = cfg_value(NET, "c2c_udp_ip") or "127.0.0.1"
        show("python socket -> udp %s:%d: %d malformed datagrams, 20 times"
             % (ip, port, len(BAD)))
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        for _ in range(20):
            for blob in BAD:
                s.sendto(blob, (ip, port))
        s.close()
        time.sleep(3)
        after = c2c_pids()
        if after != before:
            raise Fail("the c2c processes changed under garbage: %s then %s\n%s"
                       % (before, after, ti.c2c_evidence(C2C)))
        if not ping(C, "10.5.6.2", 3, check=False):
            raise Fail("no ping across the c2c after the garbage\n"
                       + ti.c2c_evidence(C2C))
        print("      %d datagrams dropped, same c2c processes, ping crosses"
              % (20 * len(BAD)))
        delete(C2C)
        wait_topo(C2C, False, net=PEER)
    finally:
        clean(C, C2C)
        if in_topo(P, PEER):
            cli("del", P, net=PEER, quiet=True)
            wait_topo(P, False, net=PEER)


if __name__ == "__main__":
    run("c2c_udp_garbage", sub_c2c_udp_garbage)
