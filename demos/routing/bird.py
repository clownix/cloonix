#!/usr/bin/env python3
r"""bird - the other routing daemon: OSPF the same, in a different program.

    ./bird.py [net]                the net defaults to nemo

Three routers in a line, each pair on its own lan, a host at each end::

  hA -- l1 -- r1 -- l2 -- r2 -- l3 -- r3 -- l4 -- hB
       .10   .1 .1      .2 .2      .3 .3     .10
              10.0.1     10.0.2     10.0.3

  hA 10.0.1.10          hB 10.0.4.10

Every rN runs BIRD speaking OSPF on its links between routers, so hA
reaches hB across three hops none was told of by hand.

  ACT 1  the routers up and addressed, no routing daemon: hA does not reach
         hB, the middle routers having no route to the far lans.
  ACT 2  bird starts on all three: the adjacencies form, the link-state
         database converges, every router has a route to every lan, and hA
         pings hB.
  ACT 3  the middle link r2--r3 cut: OSPF floods the change and, with no
         second path, the far side goes away; restored, the route comes
         back on its own.

Image: bird_trixie (bird2)
Size:  7 containers, 4 lans

THE PACKET TO CATCH
-------------------
  OSPF is IP protocol 89, reached by exclusion:

    cloonix cli nemo cnf r2 eth0 sniff=on
    cloonix cli nemo sniff r2 eth0 --dump ip and not udp and not tcp

  Hellos every few seconds; the packet to catch is the LSA flooded the
  instant a link changes, in ACT 3.

Important
  bird is installed, its configuration written at run time. The frr demos
  of the routing area run the same OSPF in another program.
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
    NET, build, check_net, choose_rootfs, count_on, cut_off, erase_net,
    hold, in_parallel, place, put, reached, run, ssh, step, story,
    tools_present)

HISTORY = """\
BIRD, the BIRD Internet Routing Daemon, began in 1998 as a student project
at the Faculty of Mathematics and Physics of Charles University in Prague,
by Martin Mares, Pavel Machek and Ondrej Filip; CZ.NIC, the Czech domain
registry, maintains it since 2009. It is one of the two open routing daemons
carrying real internet routes at scale: many route servers at internet
exchanges run BIRD. Where frr descends from Zebra and mimics a vendor's
command line, BIRD has a configuration language of its own.

Today: BIRD is the route server of most large internet exchange points, DE-
CIX, AMS-IX and LINX among them, where one daemon holds hundreds of BGP
sessions and millions of paths.
"""

ROOTFS_CHOICES = ("bird_trixie",)

HA, HB = "hA", "hB"
R1, R2, R3 = "r1", "r2", "r3"
ROUTERS = (R1, R2, R3)
ALL = (HA, R1, R2, R3, HB)
L1, L2, L3, L4 = "l1", "l2", "l3", "l4"

# interface -> (num, lan, addr/plen)
WIRING = {
    HA: [(0, L1, "10.0.1.10/24")],
    R1: [(0, L1, "10.0.1.1/24"), (1, L2, "10.0.2.1/24")],
    R2: [(0, L2, "10.0.2.2/24"), (1, L3, "10.0.3.2/24")],
    R3: [(0, L3, "10.0.3.3/24"), (1, L4, "10.0.4.3/24")],
    HB: [(0, L4, "10.0.4.10/24")],
}
GATEWAY = {HA: "10.0.1.1", HB: "10.0.4.3"}

# Each router's OSPF interfaces are its two links; the daemon needs no
# knowledge of the far lans, which is the whole point of a routing protocol.
def bird_conf(node):
    ifaces = "".join('    interface "eth%d";\n' % num
                     for num, _lan, _addr in WIRING[node])
    return (
        'log syslog all;\n'
        'router id %s;\n'
        'protocol device { }\n'
        'protocol direct { ipv4; }\n'
        'protocol kernel {\n'
        '    ipv4 { export all; };\n'
        '    learn;\n'
        '}\n'
        'protocol ospf v2 {\n'
        '    ipv4 { import all; export all; };\n'
        '    area 0 {\n'
        '%s'
        '    };\n'
        '}\n' % (WIRING[node][0][2].split("/")[0], ifaces))

def bird_start(node):
    put(node, "/etc/bird/bird.conf", bird_conf(node))
    ssh(node, "mkdir -p /run/bird")
    ssh(node, "bird -c /etc/bird/bird.conf")
    # bird answers birdc once its socket is up.
    for _ in range(20):
        if ssh(node, "birdc -r show status 2>/dev/null | grep -q Daemon "
                     "&& echo up", check=False).strip():
            return
        time.sleep(1)
    raise RuntimeError("bird did not start on %s:\n%s"
                       % (node, ssh(node, "tail -n 20 /var/log/syslog",
                                    check=False)))

# OSPF waits 40 s on a lan before electing its DR, then floods: the routes
# land close to a minute after bird starts.
def converged(timeout=100):
    """hA can reach hB only once every router has learnt the far lans."""
    end = time.time() + timeout
    while time.time() < end:
        r = run('cloonix cli %s ssh %s "ping -c1 -W1 10.0.4.10"'
                % (NET, HA), quiet=True)
        if r.returncode == 0:
            return True
        time.sleep(2)
    return False

def routes(node):
    return ssh(node, "ip route", check=False)

def ping(src, dst, count=5):
    return ssh(src, "ping -c %d %s" % (count, dst), check=False)

LAYOUT = {
    HA: (-600, 0), L1: (-450, 0), R1: (-300, 0), L2: (-150, 0),
    R2: (0, 0), L3: (150, 0), R3: (300, 0), L4: (450, 0), HB: (600, 0),
}
ETHS = {
    (HA, "eth0"): 90, (R1, "eth0"): 270, (R1, "eth1"): 90, (R2, "eth0"): 270,
    (R2, "eth1"): 90,
    (R3, "eth0"): 270, (R3, "eth1"): 90, (HB, "eth0"): 270,
}

CARD = r"""
    hA -- l1 -- r1 -- l2 -- r2 -- l3 -- r3 -- l4 -- hB
   10.0.1.10  .1 .1     .2 .2     .3 .3    10.0.4.10

  Three BIRD routers in a line. hA and hB are two hops apart at each end
  and three routers apart from each other; OSPF gives every router a route
  to every lan, so hA reaches hB with nothing configured by hand but the
  addresses on the wires.
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

    step("creating and cabling the seven machines")
    build(rootfs)
    tools_present(R1, "bird", "birdc")
    tools_present(HA, "ping")
    for r in ROUTERS:
        ssh(r, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")

    step("act 1: addressed, but no routing daemon yet")
    story()
    r = ping(HA, "10.0.4.10", count=3)
    story("    hA -> hB with no routing:")
    story(r.strip() or "(no output)")
    ok_before = cut_off(r)
    story()
    story("    The two ends are three routers apart and the routers know")
    story("    only their own two lans. Nothing has told r1 where 10.0.4.0")
    story("    is, so the packet dies in the middle. This is the problem a")
    story("    routing protocol exists to solve.")
    hold()

    step("act 2: bird starts, OSPF converges, hA reaches hB")
    story()
    in_parallel(bird_start, list(ROUTERS), "bird start")
    if not converged():
        sys.exit("OSPF did not converge: hA never reached hB")
    story("    r2 has learnt every lan, not just its own two:")
    for line in routes(R2).strip().split("\n"):
        story("      %s" % line)
    story()
    r = ping(HA, "10.0.4.10")
    story("    hA -> hB, now that OSPF has filled the tables:")
    story(r.strip())
    ok_after = reached(r)
    story()

    step("act 3: cut the middle link, and watch OSPF notice")
    story()
    got = count_on(R2, 0, "ip and not udp and not tcp",
                   lambda: (run("cloonix cli %s unplug r2 eth1 l3" % NET),
                            time.sleep(6))[0],
                   settle=6.0)
    story("    r2--r3 unplugged. On r2 eth0, OSPF packets while it reacts:")
    if got is not None:
        story("      %d OSPF packet(s) crossed r2 eth0 as the topology changed"
              % got["count"])
    story()
    lost = ping(HA, "10.0.4.10", count=3)
    story("    hA -> hB with the only path broken:")
    story(lost.strip())
    ok_broken = cut_off(lost)
    story()
    story("    There was one path and it is gone, so the far side is")
    story("    unreachable -- which proves the route was OSPF's doing. A")
    story("    static line would still be pointing at a dead link.")
    hold()
    run("cloonix cli %s plug r2 eth1 l3" % NET)
    ssh(R2, "ip addr add 10.0.3.2/24 dev eth1", check=False)
    ssh(R2, "ip link set dev eth1 up")
    if converged():
        r = ping(HA, "10.0.4.10", count=3)
        story("    link restored, OSPF reconverged, hA -> hB again:")
        story(r.strip())
    story()

    if not (ok_before and ok_after and ok_broken):
        sys.exit("a check failed: before=%s after=%s broken=%s"
                 % (ok_before, ok_after, ok_broken))

    story("""
the network is left up. Things to try:

  the OSPF neighbours bird sees, and the link-state database:
    cloonix cli %s ssh r2 "birdc show ospf neighbors"
    cloonix cli %s ssh r2 "birdc show route"

  catch OSPF flooding when you break a link by hand:
    cloonix cli %s sniff r1 eth1 --dump ip and not udp and not tcp
    cloonix cli %s ssh r2 "ip link set eth1 down"
""" % (NET, NET, NET, NET))

main()
