#!/usr/bin/env python3
"""c2c - three cloonix servers, two c2c cables, and one ping across both.

    ./c2c.py [centre] [left] [right]      defaults: nemo fido mito

A c2c joins two cloonix servers. This demo puts one server in the middle
with a c2c to each of the other two, and then routes between them::

      fido                    nemo                     mito
   .--------.             .-----------.             .--------.
   | fidocnt|             |  nemocnt  |             | mitocnt|
   |  eth0  |             | eth0 eth1 |             |  eth0  |
   '---+----'             '--+-----+--'             '----+---'
       |                     |     |                     |
     lanf                  lanf   lanm                 lanm
       |                     |     |                     |
   [nemo2fido]==========[nemo2fido] [nemo2mito]======[nemo2mito]
        the first c2c             the second c2c

   lanf   10.10.0.0/24    nemocnt eth0 .1    fidocnt eth0 .2
   lanm   10.20.0.0/24    nemocnt eth1 .1    mitocnt eth0 .2

A c2c is a cable and not a router: the two ends of one c2c are the SAME
broadcast domain, which is why 10.10.0.0/24 spans nemo and fido, and
10.20.0.0/24 spans nemo and mito. Two lans, two subnets, three servers.

Nothing routes between the two subnets by itself. nemocnt is the only
machine on both, so it is the router, and the two leaves need a static
route each to reach the far side through it. That is the whole point of
the demo: the final ping leaves fido, crosses a c2c into nemo, is routed
there, crosses the other c2c into mito, and comes back the same way.

  ACT 1  the three containers, one per server
  ACT 2  the two c2c, both created from the centre
  ACT 3  the cabling, the addresses and the static routes
  ACT 4  fidocnt pings mitocnt, and the ttl says it was routed

The three servers must be running before this starts::

    cloonix start nemo ; cloonix start fido ; cloonix start mito

Needs cloonix on the PATH and nothing else. Standard library only, like
cloonix itself, so it runs wherever the client does.

Image: trixie
Size:  3 containers on 3 servers, 2 lans, 2 c2c

THE PACKET TO CATCH
-------------------
  On a leaf, because it proves a claim about what a c2c is:

    cloonix cli fido cnf fidocnt eth0 sniff=on
    cloonix cli fido sniff fidocnt eth0 --dump arp

  fidocnt arps for 10.10.0.1, which lives on a different physical machine,
  and gets an answer. An arp crossed a c2c -- only possible because the two
  ends are one broadcast domain and not two routed networks.

  The ping at the end says the same thing with ttl=63: exactly one router
  on the path, nemocnt, with the two c2c crossed transparently.

Important
  c2c is where cloonix stops being a single-host tool. Anything needing
  more cpu or memory than one machine has -- a large frrspider, a 5G core
  next to a radio simulator -- is a c2c topology.
"""
import os
import subprocess
import sys
import threading
import time

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    ACK_FAST, CLOONIX, show, step, story)

CENTRE = sys.argv[1] if len(sys.argv) > 1 else "nemo"
LEFT = sys.argv[2] if len(sys.argv) > 2 else "fido"
RIGHT = sys.argv[3] if len(sys.argv) > 3 else "mito"
NETS = (CENTRE, LEFT, RIGHT)

ROOTFS_CHOICES = ("trixie",)

# One container per server, named after the server it lives in so that the
# canvas of each net says where you are.
CNT = {net: net + "cnt" for net in NETS}

# One c2c per leaf.
C2C = {LEFT: "%s2%s" % (CENTRE, LEFT), RIGHT: "%s2%s" % (CENTRE, RIGHT)}

SIDE = {
    LEFT:  {"lan": "lanf", "net24": "10.10.0", "centre_eth": 0},
    RIGHT: {"lan": "lanm", "net24": "10.20.0", "centre_eth": 1},
}

def cli(net, *args, **kw):
    ack = kw.get("ack")
    cmd = list(CLOONIX)
    if ack is not None:
        cmd += ["--wait", str(ack)]
    a = [str(x) for x in args]
    cmd += ["cli", net, a[0]] + a[1:]
    if not kw.get("quiet"):
        show("cloonix %scli %s %s"
             % ("" if ack is None else "--wait %s " % ack,
                net, " ".join(a)))
    return subprocess.run(cmd, capture_output=True, text=True)

def ssh(net, node, cmd, check=True):
    show('cloonix cli %s ssh %s "%s"' % (net, node, cmd))
    r = subprocess.run(CLOONIX + ["cli", net, "ssh", node, cmd],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if check and r.returncode:
        raise RuntimeError("%s/%s: %s -> %s"
                           % (net, node, cmd,
                              (r.stderr or r.stdout).strip() or "no output"))
    return r.stdout

def in_parallel(fn, items, what):
    results = {}

    def one(item):
        try:
            results[item] = fn(item)
        except Exception as err:                        # noqa: BLE001
            results[item] = err
    threads = [threading.Thread(target=one, args=(i,)) for i in items]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    bad = [i for i in items if isinstance(results[i], Exception)]
    if bad:
        raise SystemExit("%s failed for %s: %s"
                         % (what, ", ".join(str(i) for i in bad),
                            results[bad[0]]))
    return results

def in_lst(net, name):
    """Ask the server what it really has.

    "dump commands" prints the commands that would rebuild the topology, so anything
    that exists appears in it. This is the only check that catches a request
    the server dropped without answering: to a client that waited and heard
    nothing, that is indistinguishable from success.
    """
    return name in cli(net, "dump", "commands").stdout.split()

# the three servers

def check_nets():
    """All three, before anything is created.

    A c2c needs both of its servers, and the second one is not consulted
    until the master tries to connect -- so a missing far server shows up
    later as a c2c that never turns green, which reads like a c2c fault
    rather than a server that was never started.
    """
    missing = [n for n in NETS if cli(n, "dump", "commands").returncode]
    if missing:
        sys.exit("no answer from: %s\n"
                 "  this demo needs three servers running:\n"
                 "    %s\n"
                 "  or give it three other names:  ./c2c.py <centre> "
                 "<left> <right>"
                 % (", ".join(missing),
                    " ; ".join("cloonix start " + n for n in NETS)))
    if len(set(NETS)) != 3:
        sys.exit("the three names must differ, got: %s" % ", ".join(NETS))

def choose_rootfs():
    """Create a throwaway container to find out whether the image is there:
    the client cannot list bulk, and the server refuses an image it has not
    got, so asking is the only way to find out."""
    said = ""
    for cand in ROOTFS_CHOICES:
        r = cli(CENTRE, "add", "cnt", "probe_c2c", "eth=1", cand)
        if r.returncode == 0:
            cli(CENTRE, "del", "probe_c2c", ack=ACK_FAST)
            return cand
        said = (r.stdout + r.stderr).strip()
    cli(CENTRE, "del", "probe_c2c", ack=ACK_FAST)
    sys.exit("the server would not start a container on %s.\n"
             "  it said: %s\n"
             "  fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_bulk/"
             "trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm && ./cnt_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said or "nothing"))

def answers(net, node, timeout=2):
    r = subprocess.run(CLOONIX + ["--wait", str(timeout), "cli", net, "ssh", node,
                        "echo yes"],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    return r.returncode == 0

def erase_net(net=None):
    """Erase every object of the net: a demo starts on an empty canvas.

    del all is answered once the server has removed everything; the topology
    is then read until it says so, a container taking a moment to unmount
    after its delete.
    """
    net = net or CENTRE
    def empty():
        r = subprocess.run(CLOONIX + ["cli", net, "dump", "topo"],
                           capture_output=True, text=True,
                           stdin=subprocess.DEVNULL)
        return r.returncode == 0 and "(no objects)" in r.stdout
    if empty():
        return
    show("cloonix cli %s del all" % net)
    subprocess.run(CLOONIX + ["cli", net, "del", "all"], capture_output=True,
                   text=True, stdin=subprocess.DEVNULL)
    end = time.time() + 60
    while time.time() < end:
        if empty():
            time.sleep(1)
            return
        time.sleep(1)
    sys.exit("could not erase the net %s.\n"
             "  clear it by hand:  cloonix cli %s del all" % (net, net))

# building it

# Canvas positions, sent before the items exist.
def place():
    at = {CENTRE: ((C2C[LEFT], -400, 0), (SIDE[LEFT]["lan"], -200, 0),
                   (CNT[CENTRE], 0, 0), (SIDE[RIGHT]["lan"], 200, 0),
                   (C2C[RIGHT], 400, 0)),
          LEFT:   ((CNT[LEFT], -200, 0), (SIDE[LEFT]["lan"], 0, 0),
                   (C2C[LEFT], 200, 0)),
          RIGHT:  ((C2C[RIGHT], -200, 0), (SIDE[RIGHT]["lan"], 0, 0),
                   (CNT[RIGHT], 200, 0))}
    for net, items in at.items():
        for name, x, y in items:
            cli(net, "cnf", name, "x=%d" % x, "y=%d" % y, ack=ACK_FAST)
    cli(CENTRE, "cnf", CNT[CENTRE], "eth%d" % SIDE[LEFT]["centre_eth"],
        "angle=270", ack=ACK_FAST)
    cli(CENTRE, "cnf", CNT[CENTRE], "eth%d" % SIDE[RIGHT]["centre_eth"],
        "angle=90", ack=ACK_FAST)
    cli(LEFT, "cnf", CNT[LEFT], "eth0", "angle=90", ack=ACK_FAST)
    cli(RIGHT, "cnf", CNT[RIGHT], "eth0", "angle=270", ack=ACK_FAST)

def create_cnt(net, rootfs, nb_eth):
    """Create one container, and check that the server took it.

    A create issued with the short ack does not report a refusal: the client
    waits, hears nothing and calls that success. And it can legitimately be
    refused for a moment, on a name the server is still disposing of from
    the previous run. Hence the full ack and a few retries.
    """
    name = CNT[net]
    last = ""
    for attempt in range(6):
        r = cli(net, "add", "cnt", name, "eth=%d" % nb_eth, rootfs)
        if r.returncode == 0:
            if attempt:
                story("  %s/%s created on attempt %d" % (net, name,
                                                         attempt + 1))
            return
        last = (r.stdout + r.stderr).strip()
        time.sleep(2)
    raise RuntimeError("%s/%s was never created. The server said: %s"
                       % (net, name, last or "nothing at all"))

def create_c2c(leaf):
    """One command, on the centre only, and it builds BOTH ends.

    A c2c goes in pairs, master and slave. The master is the one given the
    name of the distant cloonix: it sends a peer_create over the tcp
    signalling and the far switch allocates its slave from that, with the
    same name.

    Running "add c2c" on both servers is the mistake to avoid. It produces
    two masters and no slave, which the protocol has no way to settle: each
    fills in a session number from its own counter, every ping then carries
    the sender's and is compared against the receiver's, the two only agree
    by coincidence, and each destroys the other and restarts for ever.
    """
    name = C2C[leaf]
    r = cli(CENTRE, "add", "c2c", name, leaf)
    if r.returncode:
        raise RuntimeError("%s: %s" % (name, (r.stdout + r.stderr).strip()))

def wait_c2c(leaf, timeout=60):
    """Wait for the slave end to exist on the leaf server.

    The item on the centre appears at once; the one on the leaf appears
    only when the master has reached the far switch over tcp. Cabling the
    leaf end before it exists would be refused, so this is waited for
    rather than slept on.
    """
    name = C2C[leaf]
    end = time.time() + timeout
    while time.time() < end:
        if in_lst(leaf, name) and in_lst(CENTRE, name):
            return
        time.sleep(1)
    raise SystemExit(
        "the c2c %s never appeared on %s.\n"
        "  The master is on %s and it connects to %s over tcp, so what\n"
        "  this usually means is that %s cannot be reached at the address\n"
        "  and port its block of cloonix.cfg gives, or that the two\n"
        "  passwords differ.\n"
        "  Check both blocks:   grep -A8 'CLOONIX_NET: %s' <config>"
        % (name, leaf, CENTRE, leaf, leaf, leaf))

def cable():
    """Every cable of the topology, in one place.

    The c2c ends are cabled like any other endpoint: "plug <item> <num>
    <lan>", num 0, because a c2c has exactly one interface on each side; a
    container interface is named, eth0.
    """
    for leaf in (LEFT, RIGHT):
        lan = SIDE[leaf]["lan"]
        # On the centre: the hub container and the master end of the c2c.
        cli(CENTRE, "plug", CNT[CENTRE],
            "eth%d" % SIDE[leaf]["centre_eth"], lan, ack=ACK_FAST)
        cli(CENTRE, "plug", C2C[leaf], lan)
        # On the leaf: its container and the slave end.
        cli(leaf, "plug", CNT[leaf], "eth0", lan)
        cli(leaf, "plug", C2C[leaf], lan)

def wait_ready(pair, timeout=90):
    net, node = pair
    end = time.time() + timeout
    while time.time() < end:
        if answers(net, node, timeout=3):
            return
        time.sleep(1)
    raise RuntimeError("%s/%s never answered" % (net, node))

def address_centre():
    """The centre is on both subnets, and it is the only machine that is.

    It needs no static route: both networks are directly connected, so its
    routing table already has them. What it does need is forwarding, which
    is off by default -- a machine with two interfaces is not a router
    until it is told to be one.
    """
    node = CNT[CENTRE]
    for leaf in (LEFT, RIGHT):
        num = SIDE[leaf]["centre_eth"]
        ssh(CENTRE, node, "ip addr add %s.1/24 dev eth%d"
            % (SIDE[leaf]["net24"], num))
        ssh(CENTRE, node, "ip link set dev eth%d up" % num)
    ssh(CENTRE, node, "sysctl -w net.ipv4.ip_forward=1 >/dev/null")
    got = ssh(CENTRE, node, "sysctl -n net.ipv4.ip_forward", check=False)
    if got.strip() != "1":
        raise RuntimeError("%s would not enable ip_forward, it reads %r"
                           % (node, got.strip()))

def address_leaf(leaf):
    """One address, and one static route to the other leaf.

    The route is the interesting line. The two leaves are on different
    subnets and neither has an interface on the other, so without it a ping
    to the far side is answered by the kernel itself with "Network is
    unreachable" and never reaches a cable at all.
    """
    node = CNT[leaf]
    other = RIGHT if leaf == LEFT else LEFT
    mine = SIDE[leaf]["net24"]
    theirs = SIDE[other]["net24"]
    ssh(leaf, node, "ip addr add %s.2/24 dev eth0" % mine)
    ssh(leaf, node, "ip link set dev eth0 up")
    ssh(leaf, node, "ip route replace %s.0/24 via %s.1" % (theirs, mine))

def ping(net, node, target, count=5, timeout=5):
    return subprocess.run(CLOONIX + ["cli", net, "ssh", node,
                           "ping -c %d -W %d %s" % (count, timeout, target)],
                          capture_output=True, text=True,
                          stdin=subprocess.DEVNULL)

def wait_ping(net, node, target, timeout=90):
    """Retry until the tunnel carries traffic.

    There is no command that reports a c2c as peered -- the canvas shows it as
    a colour, green when tcp and udp are both up -- so the honest test from
    a script is the traffic itself. The udp ports are negotiated over the
    tcp connection after the two ends have found each other, and that takes
    a few seconds; before it, a ping is simply lost.
    """
    end = time.time() + timeout
    last = None
    while time.time() < end:
        r = ping(net, node, target, count=5, timeout=5)
        if r.returncode == 0:
            return r.stdout
        last = (r.stdout + r.stderr).strip()
        time.sleep(2)
    story("\nthe ping never got through. The last attempt said:")
    story("    " + (last or "nothing at all").replace("\n", "\n    "))
    sys.exit("\n  Look at the canvas of %s: a c2c is green when both tcp and\n"
             "  udp are peered, light red when only tcp is. Light red means\n"
             "  the udp is not getting through -- c2c_udp_ip in cloonix.cfg\n"
             "  must be an address the distant cloonix can send udp back to,\n"
             "  and a firewall between the two must let the whole port range\n"
             "  above c2c_udp_port_low through, not one port." % CENTRE)

def main():
    step("checking the three servers")
    check_nets()
    story("  centre %s, leaves %s and %s" % (CENTRE, LEFT, RIGHT))

    step("looking for the image")
    rootfs = choose_rootfs()
    story()
    for leaf in (LEFT, RIGHT):
        story("    %s/%s eth0 -- %s -- [%s] === [%s] -- %s -- eth%d %s/%s"
              % (leaf, CNT[leaf], SIDE[leaf]["lan"], C2C[leaf], C2C[leaf],
                 SIDE[leaf]["lan"], SIDE[leaf]["centre_eth"],
                 CENTRE, CNT[CENTRE]))
        story("       %s.2/24                                        %s.1/24"
              % (SIDE[leaf]["net24"], SIDE[leaf]["net24"]))
    story("""
    Two subnets, not one: a c2c is a cable, so each of them spans two
    servers. Only %s is on both, so only %s can route between them.
""" % (CNT[CENTRE], CNT[CENTRE]))

    step("erasing the net, then placing the items")
    for net in NETS:
        erase_net(net)
    place()

    step("act 1: one container on each of the three servers")
    in_parallel(lambda n: create_cnt(n, rootfs, 2 if n == CENTRE else 1),
                list(NETS), "add cnt")
    for net in NETS:
        if not in_lst(net, CNT[net]):
            sys.exit("%s/%s was accepted and is not in the topology"
                     % (net, CNT[net]))
    story("  %s has two interfaces, the leaves one each" % CNT[CENTRE])

    step("waiting for them to answer")
    in_parallel(wait_ready, [(n, CNT[n]) for n in NETS], "wait_ready")

    step("act 2: the two c2c, both created from %s" % CENTRE)
    for leaf in (LEFT, RIGHT):
        create_c2c(leaf)
    for leaf in (LEFT, RIGHT):
        wait_c2c(leaf)
        story("  %s exists on %s and on %s" % (C2C[leaf], CENTRE, leaf))
    story()
    story("    One command built each pair. The master is on %s because" % CENTRE)
    story("    that is where the command was given; the slave appeared on")
    story("    the other server by itself, with the same name.")
    story()

    step("act 3: cabling, addresses and the two static routes")
    cable()
    address_centre()
    in_parallel(address_leaf, [LEFT, RIGHT], "address_leaf")
    story("  routing table of %s/%s:" % (CENTRE, CNT[CENTRE]))
    for line in ssh(CENTRE, CNT[CENTRE], "ip route").rstrip().split("\n"):
        story("      " + line)
    for leaf in (LEFT, RIGHT):
        story("  routing table of %s/%s:" % (leaf, CNT[leaf]))
        for line in ssh(leaf, CNT[leaf], "ip route").rstrip().split("\n"):
            story("      " + line)
    story()
    story("    The leaves have one static route each, and it is the only")
    story("    reason they can see one another. The centre has none: both")
    story("    subnets are on its own interfaces, so they are already in")
    story("    its table as connected routes.")
    story()

    step("act 4: %s/%s pings %s/%s, across both c2c"
         % (LEFT, CNT[LEFT], RIGHT, CNT[RIGHT]))
    target = "%s.2" % SIDE[RIGHT]["net24"]
    out = wait_ping(LEFT, CNT[LEFT], target)
    story()
    for line in out.rstrip().split("\n"):
        story("      " + line)
    story()
    story("    That packet left %s, crossed a c2c into %s, was routed" % (LEFT, CENTRE))
    story("    there from one subnet to the other, crossed the second c2c")
    story("    into %s, and came back the same way." % RIGHT)
    story()
    story("    ttl=63 is the proof it was ROUTED and not bridged. A packet")
    story("    leaves with ttl 64 and every router on the way takes one off.")
    story("    A c2c takes none: it is a cable, and a cable does not touch")
    story("    the ip header. One decrement, one router -- %s." % CNT[CENTRE])
    story()

    story("""the three nets are left up. Things worth trying:

  the same ping the other way, which proves the return path on its own:
    cloonix cli %s ssh %s "ping -c 5 %s.2"

  watch the frames on the centre, on either side of the routing:
    cloonix cli %s sniff %s eth0
    cloonix cli %s sniff %s eth1

  break one static route and watch it fail as unreachable, at the leaf,
  without a packet ever being sent:
    cloonix cli %s ssh %s "ip route del %s.0/24"
    cloonix cli %s ssh %s "ping -c 5 %s.2"

  and delete one c2c from the centre, which takes its far end with it:
    cloonix cli %s del %s
""" % (RIGHT, CNT[RIGHT], SIDE[LEFT]["net24"],
       CENTRE, CNT[CENTRE], CENTRE, CNT[CENTRE],
       LEFT, CNT[LEFT], SIDE[RIGHT]["net24"],
       LEFT, CNT[LEFT], SIDE[RIGHT]["net24"],
       CENTRE, C2C[LEFT]))

main()
