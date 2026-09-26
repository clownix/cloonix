#!/usr/bin/env python3
"""sub_c2c_replug - a c2c unplugged and plugged back at once, and after a
pause: dump commands follows each move on the spot, the plug is done, not
taken for "already on", the ping crosses again, and the next unplug is
accepted. Five rounds, as the storm throws them.
"""
from sublib import *

C, P = "tisubRgc", "tisubRgp"
L, LP = "tisubRgl", "tisubRglp"
C2C = "%s2%s" % (NET, PEER)
LINE = "plug %s %s" % (C2C, L)


def listed():
    return LINE in dump_list()


def crosses(secs):
    end = time.time() + secs
    while time.time() < end:
        if ping(C, "10.6.6.2", 1, check=False):
            return True
        time.sleep(1)
    return False


def sub_c2c_replug():
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
        while time.time() < end:
            if (in_topo(C2C) and in_topo(C2C, PEER)
                    and ti.c2c_peered(C2C, NET)):
                break
            time.sleep(1)
        else:
            raise Fail("the c2c is not peered in 60s\n" + ti.c2c_evidence(C2C))
        must(cli("plug", C2C, L), "plug c2c")
        must(cli("plug", C2C, LP, net=PEER), "plug c2c peer")
        wait_answer(C)
        wait_answer(P, PEER)
        address(C, 0, "10.6.6.1/24")
        address(P, 0, "10.6.6.2/24", net=PEER)
        if not crosses(30):
            raise Fail("nothing crosses the c2c\n" + ti.c2c_evidence(C2C))
        for pause in (0, 1, 0, 2, 0):
            must(cli("unplug", C2C, L), "unplug c2c")
            if listed():
                raise Fail("dump commands still has '%s' after the unplug"
                           % LINE)
            time.sleep(pause)
            r = cli("plug", C2C, L)
            said = (r.stdout + r.stderr).strip()
            must(r, "plug c2c back")
            if "already" in said:
                raise Fail("the plug back after %ds was taken for already "
                           "done: %s" % (pause, said))
            if not listed():
                raise Fail("dump commands lacks '%s' after the plug back"
                           % LINE)
            if not crosses(30):
                raise Fail("nothing crosses the c2c after an unplug and a "
                           "plug %ds apart\n%s"
                           % (pause, ti.c2c_evidence(C2C)))
    finally:
        clean(C, C2C)
        if in_topo(P, PEER):
            cli("del", P, net=PEER, quiet=True)
            wait_topo(P, False, net=PEER)


if __name__ == "__main__":
    run("c2c_replug", sub_c2c_replug)
