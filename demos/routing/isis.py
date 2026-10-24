#!/usr/bin/env python3
r"""isis - the routing protocol of the carriers, which does not run over IP.

    ./isis.py [net]                the net defaults to nemo

Three routers in a triangle, a host behind two of them::

                  r2
                /    \
    hA ---- r1 -------- r3 ---- hB

  IS-IS runs on the three sides of the triangle. From r1 to r3 the direct
  side costs 10, the way through r2 costs 20.

  ACT 1  the adjacencies come up, every router computes its routes, hA
         reaches hB over the direct side.
  ACT 2  the direct side is cut. The two routers flood a new link state,
         every router computes again, the traffic goes through r2.
  ACT 3  the side is restored and the direct path is taken again.

Image: frr_trixie (zebra, isisd)
Size:  5 containers, 5 lans

THE PACKET TO CATCH
-------------------
  IS-IS has no IP header: its hellos and link states are ethernet frames
  with an LLC header, the OSI network layer it was written for. The filter
  has no word for it, it is reached by exclusion:

    cloonix cli nemo cnf r1 eth1 sniff=on
    cloonix cli nemo sniff r1 eth1 --dump "not ip and not ip6 and not arp"

  The packet to catch is the link state r1 floods at the cut of
  ACT 2: one frame, and r2 learns a link it is not attached to is gone.

Important
  Nothing is baked into the image: the configuration of frr is written at
  run time.
"""
import os
import sys
import time

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    NET, build, check_net, choose_rootfs, count_on, erase_net, frr_setup,
    hold, in_parallel, place, reached, run, ssh, step, story, tools_present)

HISTORY = """\
IS-IS was written at Digital Equipment Corporation in the 1980s for DECnet
Phase V, largely by Radia Perlman and Dave Oran, and became the ISO standard
10589 for the OSI protocols. RFC 1195, by Ross Callon in 1990, taught it to
carry IP routes. The OSI network layer it was made for disappeared; the
routing protocol stayed, because it runs beside IP and not over it, and
carrying a new address family, IPv6 then MPLS labels, took a new field and
no new protocol.

Today: IS-IS is the interior protocol of most large carriers and of many
data centre fabrics, where OSPF holds the enterprise. The daemon here is
isisd of frr.
"""

ROOTFS_CHOICES = ("frr_trixie",)

HA, HB = "hA", "hB"
R1, R2, R3 = "r1", "r2", "r3"
ROUTERS = (R1, R2, R3)
ALL = (HA, R1, R2, R3, HB)
LA, L13, L12, L23, LB = "lanA", "lan13", "lan12", "lan23", "lanB"

WIRING = {
    HA: [(0, LA, "10.1.0.10/24")],
    R1: [(0, LA, "10.1.0.1/24"), (1, L13, "10.0.13.1/24"),
         (2, L12, "10.0.12.1/24")],
    R2: [(0, L12, "10.0.12.2/24"), (1, L23, "10.0.23.2/24")],
    R3: [(0, LB, "10.3.0.1/24"), (1, L13, "10.0.13.3/24"),
         (2, L23, "10.0.23.3/24")],
    HB: [(0, LB, "10.3.0.10/24")],
}
GATEWAY = {HA: "10.1.0.1", HB: "10.3.0.1"}
SYSID = {R1: "0001", R2: "0002", R3: "0003"}
HOST_SIDE = {R1: 0, R3: 0}

def frr_conf(node):
    c = ["hostname %s" % node]
    for num, _lan, _addr in WIRING[node]:
        c.append("interface eth%d" % num)
        c.append(" ip router isis lab")
        if HOST_SIDE.get(node) == num:
            c.append(" isis passive")
        else:
            c.append(" isis network point-to-point")
            c.append(" isis hello-interval 1")
            c.append(" isis hello-multiplier 3")
    c.append("router isis lab")
    c.append(" net 49.0001.0000.0000.%s.00" % SYSID[node])
    c.append(" is-type level-2-only")
    c.append(" lsp-gen-interval 1")
    return "\n".join(c)

def configure(node):
    ssh(node, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    frr_setup(node, ["isisd"], frr_conf(node))

def converged(via, timeout=60):
    """hA reaches hB, and r1 forwards to hB through the address given."""
    end = time.time() + timeout
    while time.time() < end:
        r = run('cloonix cli %s ssh %s "ping -c1 -W1 10.3.0.10"'
                % (NET, HA), quiet=True)
        if r.returncode == 0 and via in path_of(R1):
            return True
        time.sleep(2)
    return False

def path_of(node):
    return ssh(node, "ip route get 10.3.0.10 2>/dev/null | head -1",
               check=False).strip()

def ping(src, dst, count=5):
    return ssh(src, "ping -c %d %s" % (count, dst), check=False)

LAYOUT = {
    HA: (-560, 120), LA: (-430, 120), R1: (-300, 120),
    L13: (0, 120), R3: (300, 120), LB: (430, 120), HB: (560, 120),
    L12: (-150, -30), R2: (0, -180), L23: (150, -30),
}
ETHS = {
    (HA, "eth0"): 90, (R1, "eth0"): 270, (R1, "eth1"): 90, (R1, "eth2"): 45,
    (R2, "eth0"): 225, (R2, "eth1"): 135,
    (R3, "eth0"): 90, (R3, "eth1"): 270, (R3, "eth2"): 315, (HB, "eth0"): 270,
}

CARD = r"""
                  r2
                /    \
    hA ---- r1 -------- r3 ---- hB

  IS-IS on the three sides. The direct side costs 10, the way through r2
  costs 20: the direct side carries the traffic until it is cut.
"""

def main():
    story(HISTORY)
    step("checking the server")
    check_net()
    step("looking for the image")
    rootfs = choose_rootfs()
    erase_net()
    place()
    story(CARD)
    story("  image: %s" % rootfs)

    step("creating and cabling the five machines")
    build(rootfs)
    tools_present(R1, "vtysh")

    step("act 1: the adjacencies come up, hA reaches hB over the direct side")
    story()
    in_parallel(configure, list(ROUTERS), "frr start")
    if not converged("10.0.13.3"):
        sys.exit("isis did not converge on the direct side:\n%s\n%s"
                 % (path_of(R1), ssh(R1, "vtysh -c 'show isis neighbor'",
                                     check=False)))
    story("    r1, its neighbours:")
    story(ssh(R1, "vtysh -c 'show isis neighbor'", check=False).rstrip())
    story()
    story("    r1, its way to hB:")
    story("      %s" % path_of(R1))
    story()
    r = ping(HA, "10.3.0.10")
    story("    hA -> hB:")
    story(r.strip())
    ok_direct = reached(r)
    story()
    story("    The neighbours are named by system id, not by address: IS-IS")
    story("    knows its neighbours before it knows anything about IP.")
    hold()

    step("act 2: the direct side is cut, the traffic goes through r2")
    story()
    got = count_on(R1, 2, "not ip and not ip6 and not arp",
                   lambda: (ssh(R1, "ip link set eth1 down"),
                            time.sleep(6))[0], settle=4.0)
    if got is not None:
        story("    %d IS-IS frame(s) on the side r1-r2 during the cut:"
              % got["count"])
        story("    hellos, and the link states that tell r2 of the change.")
        story()
    if not converged("10.0.12.2"):
        sys.exit("isis did not move to the way through r2:\n%s" % path_of(R1))
    story("    r1, its way to hB now:")
    story("      %s" % path_of(R1))
    story()
    r = ping(HA, "10.3.0.10")
    story("    hA -> hB, through r2:")
    story(r.strip())
    ok_reroute = reached(r)
    hold()

    step("act 3: the side is restored, the direct path is taken again")
    story()
    ssh(R1, "ip link set eth1 up")
    ssh(R1, "ip addr add 10.0.13.1/24 dev eth1", check=False)
    ok_back = converged("10.0.13.3")
    story("    r1, its way to hB:")
    story("      %s" % path_of(R1))
    story()
    r = ping(HA, "10.3.0.10", count=3)
    story(r.strip())
    story()

    if not (ok_direct and ok_reroute and ok_back):
        sys.exit("a check failed: direct=%s reroute=%s back=%s"
                 % (ok_direct, ok_reroute, ok_back))

    story("""
the network is left up. Things to try:

  the link state database, the same on the three routers:
    cloonix cli %s ssh r2 "vtysh -c 'show isis database'"

  the routes isis gave to the kernel:
    cloonix cli %s ssh r1 "vtysh -c 'show ip route isis'"

  the hellos, one a second, with no IP header:
    cloonix cli %s sniff r1 eth2 --dump "not ip and not ip6 and not arp"
""" % (NET, NET, NET))

main()
