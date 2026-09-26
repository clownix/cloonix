#!/usr/bin/env python3
"""sub_c2c_mac_mangle - cnf <name> mac_mangle=<mac>, given after the c2c
is cabled: what comes in from the peer wears that source mac on this side
(the arp sender and the frame), the ping still works both ways because the
c2c puts the real mac back on the way out; a malformed mac and a name that
is not a c2c are refused.
"""
from sublib import *

C, P = "tisubMMc", "tisubMMp"
L, LP = "tisubMMl", "tisubMMlp"
C2C = "%s2%s" % (NET, PEER)
MAC = "02:c2:c0:00:00:01"


def mac(b):
    return ":".join("%02x" % x for x in b)


def sub_c2c_mac_mangle():
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
        address(C, 0, "10.5.5.1/24")
        address(P, 0, "10.5.5.2/24", net=PEER)
        end = time.time() + 30
        while not ping(C, "10.5.5.2", 1, check=False):
            if time.time() > end:
                raise Fail("nothing crosses the c2c\n" + ti.c2c_evidence(C2C))
            time.sleep(1)
        real = ssh(P, "cat /sys/class/net/eth0/address", net=PEER).strip()
        mine = ssh(C, "cat /sys/class/net/eth0/address").strip()
        r = cli("cnf", C2C, "mac_mangle=zz:00:00:00:00:00")
        if r.returncode == 0:
            raise Fail("mac_mangle=zz:... was accepted")
        r = cli("cnf", C, "mac_mangle=" + MAC)
        if r.returncode == 0:
            raise Fail("cnf on a cnt was accepted")
        must(cli("cnf", C2C, "mac_mangle=" + MAC), "cnf mac_mangle")
        time.sleep(1)
        ssh(C, "ip neigh flush all")
        ssh(P, "ip neigh flush all", net=PEER)
        s = Sniff(C, 0, "").start()
        ping(C, "10.5.5.2", 4)
        ping(P, "10.5.5.1", 3, net=PEER)
        n = s.stop()
        seen = []
        for pkt in s.packets:
            if len(pkt) < 42:
                continue
            src, dst, typ = mac(pkt[6:12]), mac(pkt[0:6]), pkt[12:14]
            if typ == b"\x08\x06":
                seen.append(("arp", src, mac(pkt[22:28])))
            elif typ == b"\x08\x00" and pkt[23] == 1:
                seen.append(("icmp", src, dst))
        got = [x for x in seen if x[1] not in (mine, "ff:ff:ff:ff:ff:ff")]
        if not got:
            raise Fail("no frame from the peer among %d captured" % n)
        wrong = [x for x in got if x[1] != MAC]
        if wrong:
            raise Fail("frames from the peer without the mangled mac %s: %s"
                       % (MAC, wrong[:5]))
        arp = [x for x in got if x[0] == "arp"]
        if any(x[2] != MAC for x in arp):
            raise Fail("arp from the peer with a sender mac that is not %s: %s"
                       % (MAC, arp[:5]))
        neigh = ssh(C, "ip neigh show 10.5.5.2").strip()
        if MAC not in neigh:
            raise Fail("%s holds %s for the peer, not %s (real %s)"
                       % (C, neigh, MAC, real))
        print("      %d frames from the peer, every one with %s; %s"
              % (len(got), MAC, neigh))
        delete(C2C)
        wait_topo(C2C, False, net=PEER)
    finally:
        clean(C, C2C)
        if in_topo(P, PEER):
            cli("del", P, net=PEER, quiet=True)
            wait_topo(P, False, net=PEER)


if __name__ == "__main__":
    run("c2c_mac_mangle", sub_c2c_mac_mangle)
