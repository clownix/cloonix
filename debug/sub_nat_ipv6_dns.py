#!/usr/bin/env python3
"""sub_nat_ipv6_dns - what passt gives beyond the ipv4 of testitall: on a kvm
behind a nat, dhclient brings the nameserver passt forwards to (the host's
first non-loopback resolver), so a name resolves in the guest when it resolves
on the host; and when the host has an ipv6 default route, the guest gets a
global ipv6 by the router advertisement of passt and a v6 default route.
Each half is skipped, and says so, when the host has not got it.
"""
import socket
import subprocess
from sublib import *

K, N, L = "tisubV6", "tisubV6n", "tisubV6l"
NAME = "one.one.one.one"


def host_resolves():
    try:
        socket.getaddrinfo(NAME, 80)
        return True
    except OSError:
        return False


def host_v6():
    r = subprocess.run(["ip", "-6", "route", "show", "default"],
                       capture_output=True, text=True)
    return bool(r.stdout.strip())


def sub_nat_ipv6_dns():
    clean(K, N)
    try:
        must(cli("add", "nat", N), "add nat")
        wait_topo(N, True)
        must(cli("plug", N, L), "plug")
        must(cli("add", "kvm", K, "ram=1024", "cpu=1", "eth=1", KVM_ROOTFS),
             "add kvm")
        must(cli("plug", K, "eth0", L), "plug")
        wait_answer(K, timeout=BOOT)
        ssh(K, "ip addr flush dev eth0; ip link set dev eth0 up")
        ssh(K, "timeout 30 dhclient -1 eth0")
        got = ssh(K, "ip -4 -o addr show dev eth0")
        if "10.117.0.10/" not in got:
            raise Fail("dhclient did not get 10.117.0.10: %s" % got.strip())
        ns = ssh(K, "grep -h nameserver /etc/resolv.conf /run/resolvconf/resolv.conf 2>/dev/null; true")
        if "nameserver" not in ns:
            raise Fail("dhclient left no nameserver in the guest")
        if host_resolves():
            out = ssh(K, "getent hosts %s" % NAME, check=False)
            if NAME not in out:
                raise Fail("%s does not resolve in the guest through the nat "
                           "(nameserver %s)" % (NAME, ns.split()))
        else:
            print("      (the host does not resolve %s: dns not checked)"
                  % NAME)
        if host_v6():
            end = __import__("time").time() + 20
            addr = ""
            while __import__("time").time() < end:
                addr = ssh(K, "ip -6 -o addr show dev eth0 scope global")
                if "inet6" in addr:
                    break
                __import__("time").sleep(1)
            if "inet6" not in addr:
                raise Fail("no global ipv6 on the guest after 20 s")
            route = ssh(K, "ip -6 route show default")
            if "default" not in route:
                raise Fail("no ipv6 default route in the guest")
            print("      ipv6: %s" % addr.split()[3])
        else:
            print("      (the host has no ipv6 default route: not checked)")
        delete(K)
        delete(N)
    finally:
        clean(K, N)


if __name__ == "__main__":
    run("nat dns and ipv6", sub_nat_ipv6_dns)
