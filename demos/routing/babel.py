#!/usr/bin/env python3
r"""babel - distance-vector routing that reroutes when a link comes and goes.

    ./babel.py [net]               the net defaults to nemo

A diamond: two disjoint paths between the same two routers, a host on each
end::

          +---- rt2 ----+
          |             |
   hA -- r1             r4 -- hB
          |             |
          +---- rt3 ----+

  Babel runs on r1 rt2 rt3 r4. There are two ways from r1 to r4; Babel picks
  one, and when it is cut it moves to the other with no static route and no
  operator.

  ACT 1  babel converges, hA reaches hB. r1 forwards to hB through one of
         the two middle routers.
  ACT 2  that middle router's far link is cut. Babel reroutes onto the other
         arm; hA keeps reaching hB after a short hiccup and r1's chosen path
         has changed.
  ACT 3  the link is restored and the far side stays reachable.

Image: babel_trixie (babeld)
Size:  6 containers, 5 lans

THE PACKET TO CATCH
-------------------
  Babel speaks UDP on port 6696 to a link-local multicast group:

    cloonix cli nemo cnf r1 eth1 sniff=on
    cloonix cli nemo sniff r1 eth1 --dump udp port 6696

  The packet to catch is the update r1 sends after ACT 2, retracting
  the route it lost and taking up the new one.

Important
  Nothing is baked into the image: babeld is installed, and the demo runs it
  on the router interfaces at run time with no configuration file at all.
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
    NET, build, check_net, choose_rootfs, count_on, erase_net, hold,
    in_parallel, place, reached, run, ssh, step, story, tools_present)

HISTORY = """\
Babel was designed by Juliusz Chroboczek around 2007, standardised as RFC
6126 and later RFC 8966. It is a distance-vector protocol, like the old RIP,
but built to be loop-free and to work over links that appear, vanish and
vary in quality, wireless meshes especially. Where OSPF floods a full map,
Babel keeps only distances and reasons about them carefully enough never to
loop.

Today: Babel routes community mesh networks and is the routing protocol of
the IETF Homenet architecture; it ships in OpenWrt as babeld and inside BIRD
and FRR.
"""

ROOTFS_CHOICES = ("babel_trixie",)

HA, HB = "hA", "hB"
R1, RT2, RT3, R4 = "r1", "rt2", "rt3", "r4"
ROUTERS = (R1, RT2, RT3, R4)
ALL = (HA, R1, RT2, RT3, R4, HB)
LH1, LU, LD, LU2, LD2, LH2 = "lh1", "lu", "ld", "lu2", "ld2", "lh2"

WIRING = {
    HA:  [(0, LH1, "10.0.1.10/24")],
    R1:  [(0, LH1, "10.0.1.1/24"), (1, LU, "10.0.2.1/24"),
          (2, LD, "10.0.3.1/24")],
    RT2: [(0, LU, "10.0.2.2/24"), (1, LU2, "10.0.4.2/24")],
    RT3: [(0, LD, "10.0.3.3/24"), (1, LD2, "10.0.5.3/24")],
    R4:  [(0, LU2, "10.0.4.4/24"), (1, LD2, "10.0.5.4/24"),
          (2, LH2, "10.0.6.4/24")],
    HB:  [(0, LH2, "10.0.6.10/24")],
}
GATEWAY = {HA: "10.0.1.1", HB: "10.0.6.4"}

def babel_start(node):
    # babeld redistributes the routes of its own interfaces, so it runs on
    # every interface of the router; the host lans carry harmless hellos the
    # hosts ignore, and their connected routes get shared as a side effect.
    ifs = " ".join("eth%d" % num for num, _lan, _ in WIRING[node])
    # babeld announces only its own addresses unless told: the lans too.
    ssh(node, "babeld -D -I /run/babeld.pid "
              "-C 'redistribute ip 10.0.0.0/8 le 24 allow' %s" % ifs)
    for _ in range(20):
        if ssh(node, "test -f /run/babeld.pid && echo up",
               check=False).strip():
            return
        time.sleep(1)
    raise RuntimeError("babeld did not start on %s:\n%s"
                       % (node, ssh(node, "tail -n 20 /var/log/syslog",
                                    check=False)))

def converged(timeout=120):
    end = time.time() + timeout
    while time.time() < end:
        r = run('cloonix cli %s ssh %s "ping -c1 -W1 10.0.6.10"'
                % (NET, HA), quiet=True)
        if r.returncode == 0:
            return True
        time.sleep(2)
    return False

def path_via(node):
    return ssh(node, "ip route get 10.0.6.10 2>/dev/null | head -1",
               check=False).strip()

def ping(src, dst, count=5):
    return ssh(src, "ping -c %d %s" % (count, dst), check=False)

LAYOUT = {
    HA: (-560, 0), R1: (-300, 0), RT2: (0, -190), RT3: (0, 190),
    R4: (300, 0), HB: (560, 0),
    LH1: (-430, 0), LU: (-150, -120), LU2: (150, -120),
    LD: (-150, 120), LD2: (150, 120), LH2: (430, 0),
}
ETHS = {
    (HA, "eth0"): 90, (R1, "eth0"): 270, (R1, "eth1"): 30, (R1, "eth2"): 150,
    (RT2, "eth0"): 210, (RT2, "eth1"): 120, (RT3, "eth0"): 300,
    (RT3, "eth1"): 30,
    (R4, "eth0"): 300, (R4, "eth1"): 150, (R4, "eth2"): 90, (HB, "eth0"): 270,
}

CARD = r"""
          +---- rt2 ----+
          |             |
   hA -- r1             r4 -- hB
          |             |
          +---- rt3 ----+

  Two disjoint paths between r1 and r4. Babel chooses one and reroutes onto
  the other when it is cut -- no static route, no operator.
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

    step("creating and cabling the six machines")
    build(rootfs)
    tools_present(R1, "babeld")
    tools_present(HA, "ping")
    for r in ROUTERS:
        ssh(r, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")

    step("act 1: babel converges over the diamond")
    story()
    in_parallel(babel_start, list(ROUTERS), "babel start")
    if not converged():
        sys.exit("babel did not converge: hA never reached hB\n%s\n%s"
                 % (ssh(R1, "ip -br addr; ip route; tail -n 30 /var/log/syslog",
                        check=False),
                    ssh(R4, "ip route", check=False)))
    hop1 = path_via(R1)
    story("    r1's chosen path to hB:")
    story("      %s" % hop1)
    story()
    r = ping(HA, "10.0.6.10")
    story("    hA -> hB:")
    story(r.strip())
    ok_after = reached(r)
    story()
    story("    Babel picked one of the two arms. Which one is not the point;")
    story("    that it picked without being told is.")
    hold()

    step("act 2: cut the chosen arm, watch babel move to the other")
    story()
    victim = RT2 if "10.0.2.2" in hop1 else RT3
    got = count_on(R1, 1 if victim == RT2 else 2, "udp port 6696",
                   lambda: (ssh(victim, "ip link set eth1 down"),
                            time.sleep(8))[0], settle=6.0)
    story("    %s far link down. Babel updates on r1's link to it:" % victim)
    if got is not None:
        story("      %d babel packet(s) while the route was withdrawn"
              % got["count"])
    story()
    if not converged():
        sys.exit("babel did not reroute after the cut")
    hop2 = path_via(R1)
    story("    r1's path to hB now:")
    story("      %s" % hop2)
    story()
    r = ping(HA, "10.0.6.10")
    story("    hA -> hB, still reachable over the other arm:")
    story(r.strip())
    ok_reroute = reached(r) and hop2 != hop1
    story()

    step("act 3: restore the link")
    story()
    ssh(victim, "ip link set eth1 up", check=False)
    ssh(victim, "ip addr add %s dev eth1"
        % dict((n, a) for n, _l, a in WIRING[victim])[1], check=False)
    converged()
    r = ping(HA, "10.0.6.10", count=3)
    story("    link back, hA -> hB:")
    story(r.strip())
    story()

    if not (ok_after and ok_reroute):
        sys.exit("a check failed: after=%s reroute=%s"
                 % (ok_after, ok_reroute))

    story("""
the network is left up. Things to try:

  the routes babel holds:
    cloonix cli %s ssh r1 "ip route"

  catch a babel update by breaking a link by hand:
    cloonix cli %s sniff r4 eth0 --dump udp port 6696
    cloonix cli %s ssh rt2 "ip link set eth1 down"
""" % (NET, NET, NET))

main()
