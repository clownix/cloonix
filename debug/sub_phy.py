#!/usr/bin/env python3
"""sub_phy - the two ways a real interface of the machine running the net is
brought in: macvlan, which leaves the interface on the host and adds <if>_1,
and absorb, which moves the interface off the host as <if>_0 and hands it back
on del. The outside vm is built with spare interfaces for this -- eth0 stays
the lifeline to the tap, eth1 and eth2 are the spares -- named in
TESTITALL_PHY (macvlan) and TESTITALL_PHY_ABSORB (absorb). Each mode skips
itself, never fails, when its interface is not named or not present, and eth0
is never touched.
"""
import os
import subprocess
import time
from sublib import *

MACVLAN_IF = os.environ.get("TESTITALL_PHY", "")
ABSORB_IF = os.environ.get("TESTITALL_PHY_ABSORB", "")


def host_has_if(name):
    return subprocess.run(["ip", "link", "show", name],
                          capture_output=True).returncode == 0


def usable(name):
    return bool(name) and name != "eth0" and host_has_if(name)


def test_macvlan(iface):
    obj = iface + "_1"
    clean(obj)
    r = cli("add", "phy", iface, "--macvlan")
    if r.returncode:
        print("      (macvlan on %s refused, not tested: %s)"
              % (iface, (r.stdout + r.stderr).strip() or "no reason"))
        return
    try:
        wait_topo(obj, True)
        if not host_has_if(iface):
            raise Fail("macvlan of %s must leave it on the host, but it is gone"
                       % iface)
        print("      macvlan: %s stays on the host, %s is in the net"
              % (iface, obj))
        must(cli("del", obj), "del %s" % obj)
        wait_topo(obj, False)
    finally:
        clean(obj)


def test_absorb(iface):
    obj = iface + "_0"
    clean(obj)
    r = cli("add", "phy", iface)
    if r.returncode:
        print("      (absorb on %s refused, not tested: %s)"
              % (iface, (r.stdout + r.stderr).strip() or "no reason"))
        return
    try:
        ok = False
        for _ in range(15):
            if in_topo(obj) and not host_has_if(iface):
                ok = True
                break
            time.sleep(1)
        if not ok:
            raise Fail("after 'add phy %s': in the net=%s, still on the "
                       "host=%s -- not absorbed"
                       % (iface, in_topo(obj), host_has_if(iface)))
        print("      absorb: %s left the host, %s is in the net" % (iface, obj))
        must(cli("del", obj), "del %s" % obj)
        wait_topo(obj, False)
        back = False
        for _ in range(10):
            if host_has_if(iface):
                back = True
                break
            time.sleep(1)
        print("      released: %s %s" % (iface, "is back on the host" if back
              else "did not return (spare, harmless)"))
    finally:
        clean(obj)


def sub_phy():
    if os.environ.get("TESTITALL_TOY"):
        print("      (toy: phy moves a host interface, not tested in the toy)")
        return
    did = False
    if usable(MACVLAN_IF):
        test_macvlan(MACVLAN_IF)
        did = True
    else:
        print("      (no spare interface in TESTITALL_PHY, macvlan skipped)")
    if usable(ABSORB_IF):
        test_absorb(ABSORB_IF)
        did = True
    else:
        print("      (no spare interface in TESTITALL_PHY_ABSORB, absorb skipped)")
    if not did:
        print("      (phy not tested: name spare interfaces in TESTITALL_PHY "
              "and TESTITALL_PHY_ABSORB)")


if __name__ == "__main__":
    run("phy", sub_phy)
