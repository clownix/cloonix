#!/usr/bin/env python3
r"""batman - a mesh that routes at layer 2, below IP entirely.

    ./batman.py [net]              the net defaults to nemo

Four nodes cabled in a chain, so not every node hears every other::

  n1 -- lanA -- n2 -- lanB -- n3 -- lanC -- n4

  Every node runs batman-adv (B.A.T.M.A.N. advanced), a routing protocol
  that lives in the kernel and forwards ETHERNET frames. On top of the mesh
  each node has a single interface, bat0, and to the IP layer the four nodes
  look like one flat switch even though no node is cabled to more than two
  others.

  ACT 1  the mesh forms. n1 and n4, which share no lan, reach each other
         over bat0 -- their frames are carried hop by hop by n2 and n3, and
         neither end has an IP route pointing at a middle node.
  ACT 2  batctl shows the originator table: who each node can reach and
         through which neighbour, the mesh's own map.
  ACT 3  n2 is taken out of the mesh. n1 and n4 lose their only path and
         cannot reach each other -- a chain has no spare -- which proves the
         traffic was riding the mesh, not going around it.

Image: batman_trixie (batctl, batman-adv)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
  batman-adv speaks its own ethertype, 0x4305, carrying OGMs, the messages
  nodes broadcast to advertise themselves. There is no IP in them, so the
  capture is unfiltered and read by eye:

    cloonix cli nemo cnf n2 eth0 sniff=on
    cloonix cli nemo sniff n2 eth0 --dump

Important
  batman-adv is a kernel module. A container run as your normal user cannot
  load a module, but it can use one already loaded on the host. On the host,
  once per boot, load it as yourself:

    sudo modprobe batman-adv

  then run this demo as your normal user -- no need to run the net as root.
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
    NET, cable_all, check_net, choose_rootfs, count_on, create, cut_off,
    erase_net, hold, in_parallel, place, reached, run, ssh, step, story,
    tools_present, wait_answer)

HISTORY = """\
B.A.T.M.A.N. -- Better Approach To Mobile Ad-hoc Networking -- came out of
the Freifunk community wireless movement in Germany around 2006, an answer to
how a town's worth of rooftop radios could route among themselves with no
central authority. Its advanced form, batman-adv, moved the whole protocol
into the linux kernel and down to layer 2: the mesh carries ethernet frames,
so anything above -- IP, DHCP, whatever -- runs across it none the wiser. It
is maintained with the kernel by the Open-Mesh project.

Today: batman-adv carries the Freifunk city meshes of Germany, with tens of
thousands of nodes, and is part of the mainline kernel since 2.6.38; OpenWrt
routers run it unchanged.
"""

ROOTFS_CHOICES = ("batman_trixie",)

N1, N2, N3, N4 = "n1", "n2", "n3", "n4"
ALL = (N1, N2, N3, N4)
LANA, LANB, LANC = "lanA", "lanB", "lanC"

# The mesh interfaces carry no IP: batman rides raw ethernet, bat0 gets the
# address. address() skips the empty strings and only brings the links up.
WIRING = {
    N1: [(0, LANA, "")],
    N2: [(0, LANA, ""), (1, LANB, "")],
    N3: [(0, LANB, ""), (1, LANC, "")],
    N4: [(0, LANC, "")],
}
GATEWAY = {}
BAT_IP = {N1: "10.7.0.1/24", N2: "10.7.0.2/24",
          N3: "10.7.0.3/24", N4: "10.7.0.4/24"}
MESH_IFACES = {N1: [0], N2: [0, 1], N3: [0, 1], N4: [0]}

def batman_ready(node):
    # A user-run container cannot load the module; it uses the host's. The
    # module, once loaded on the host, is visible here as /sys/module.
    return ssh(node, "test -d /sys/module/batman_adv && echo yes",
               check=False).strip() == "yes"

def mesh_up(node):
    for num in MESH_IFACES[node]:
        ssh(node, "ip link set eth%d up" % num)
        ssh(node, "batctl if add eth%d" % num)
    ssh(node, "ip link set up dev bat0")
    ssh(node, "ip addr add %s dev bat0" % BAT_IP[node])

def originators(node):
    return ssh(node, "batctl o 2>/dev/null", check=False)

def mesh_ready(timeout=45):
    end = time.time() + timeout
    while time.time() < end:
        r = run('cloonix cli %s ssh %s "ping -c1 -W1 10.7.0.4"'
                % (NET, N1), quiet=True)
        if r.returncode == 0:
            return True
        time.sleep(2)
    return False

def ping(src, dst, count=5):
    return ssh(src, "ping -c %d %s" % (count, dst), check=False)

LAYOUT = {
    N1: (-450, 0), LANA: (-300, 0), N2: (-150, 0), LANB: (0, 0),
    N3: (150, 0), LANC: (300, 0), N4: (450, 0),
}
ETHS = {
    (N1, "eth0"): 90, (N2, "eth0"): 270, (N2, "eth1"): 90, (N3, "eth0"): 270,
    (N3, "eth1"): 90,
    (N4, "eth0"): 270,
}

CARD = r"""
   n1 -- lanA -- n2 -- lanB -- n3 -- lanC -- n4
  10.7.0.1     10.7.0.2     10.7.0.3     10.7.0.4  (all on bat0)

  Four batman-adv nodes in a chain. n1 and n4 share no lan yet reach each
  other over bat0: the mesh carries their frames through n2 and n3.
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

    step("creating and cabling the four nodes")
    in_parallel(lambda n: create(n, rootfs), list(ALL), "add cnt")
    for name in ALL:
        wait_answer(name)
    cable_all()
    tools_present(N1, "batctl")

    step("act 1: the mesh forms, n1 and n4 reach each other over it")
    story()
    if not all(batman_ready(n) for n in ALL):
        sys.exit("batman-adv is not loaded.\n"
                 "  a container run as your normal user cannot load a kernel\n"
                 "  module, but it can use one already on the host. On the\n"
                 "  host, once per boot, run as yourself:\n"
                 "    sudo modprobe batman-adv\n"
                 "  then start this demo again as your normal user.")
    in_parallel(mesh_up, list(ALL), "mesh up")
    if not mesh_ready():
        sys.exit("the mesh did not form: n1 never reached n4 over bat0")
    r = ping(N1, "10.7.0.4")
    story("    n1 -> n4, two mesh hops apart, over bat0:")
    story(r.strip())
    ok_reach = reached(r)
    story()
    story("    n1 and n4 are on different lans and neither has an IP route")
    story("    toward the other. The frames were carried at layer 2 by n2")
    story("    and n3, which is what a mesh does.")
    hold()

    step("act 2: the mesh's own map, from batctl")
    story()
    story("    n1's originator table -- who it reaches, via which neighbour:")
    for line in originators(N1).strip().split("\n"):
        story("      %s" % line)
    story()

    step("act 3: take out the middle, and the chain breaks")
    story()
    got = count_on(N3, 0, "",
                   lambda: (ssh(N2, "ip link set eth0 down", check=False),
                            ssh(N2, "ip link set eth1 down", check=False),
                            time.sleep(6))[0], settle=5.0)
    story("    n2's links to both lans are taken down, splitting the chain.")
    story("    On n3's lanB side, frames still flying while it happens")
    story("    (OGMs, ethertype 0x4305):")
    if got is not None:
        story("      %d ethernet frame(s) crossed as n2 left" % got["count"])
    story()
    lost = ping(N1, "10.7.0.4", count=3)
    story("    n1 -> n4 with the middle gone:")
    story(lost.strip())
    ok_broken = cut_off(lost)
    story()
    story("    A chain has one path and no spare. n2 was carrying every")
    story("    frame between the two halves; without it the mesh is split")
    story("    in two -- proof the traffic was riding the mesh.")
    story()

    if not (ok_reach and ok_broken):
        sys.exit("a check failed: reach=%s broken=%s" % (ok_reach, ok_broken))

    story("""
the network is left up. Things worth trying:

  the mesh tables on any node:
    cloonix cli %s ssh n1 "batctl o"
    cloonix cli %s ssh n1 "batctl n"

  watch the OGMs flooding a cell (ethertype 0x4305, no IP):
    cloonix cli %s sniff n2 eth0 --dump
""" % (NET, NET, NET))

main()
