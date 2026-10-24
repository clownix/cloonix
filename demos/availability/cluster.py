#!/usr/bin/env python3
r"""cluster - two nodes, one service, and a failover when one dies.

    ./cluster.py [net]             the net defaults to nemo

Two machines on one lan::

  nodeA --- lan1 --- nodeB
  10.0.0.1          10.0.0.2

  Both run Corosync, which keeps them agreed on who is alive, and Pacemaker,
  which decides where a managed resource should run. The resource here is a
  floating service address, 10.0.0.100, that must live on exactly one node at
  a time. Pacemaker starts it on one of them; if that node fails, Pacemaker
  moves it to the survivor with no operator.

  ACT 1  the cluster forms, both nodes online, and Pacemaker starts the
         floating address on one of them.
  ACT 2  that node is put into standby, as if it had failed. Pacemaker
         notices through Corosync and moves the address to the other node.
  ACT 3  the address answers a ping the whole way through, from before the
         failure to after it -- the service survived the loss of its node.

Image: cluster_trixie (corosync, pacemaker, crmsh)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
  Corosync's membership traffic (the totem protocol) is UDP on port 5405:

    cloonix cli nemo cnf nodeA eth0 sniff=on
    cloonix cli nemo sniff nodeA eth0 --dump udp port 5405

Important
  Nothing is baked into the image: Corosync and Pacemaker are installed, and
  the demo writes the cluster configuration and defines the resource at run
  time. Fencing is turned off, as there is no power controller to fence with.
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
    in_parallel, place, put, reached, ssh, step, story, tools_present)

HISTORY = """\
This is the long-running linux high-availability stack. Corosync, with a
lineage back through OpenAIS to the early 2000s, is the membership layer: it
tells every node who is currently in the cluster. Pacemaker, led for many
years by Andrew Beekhof, is the resource manager on top that decides where
each service runs and what to do when a node leaves. Both are under
ClusterLabs.

Today: Pacemaker and Corosync are the high-availability add-on of Red Hat,
SUSE and Debian, behind clustered databases, NFS heads and SAP
installations; fencing, absent from this two-node demo, is what production
adds.
"""

ROOTFS_CHOICES = ("cluster_trixie",)

NODEA, NODEB = "nodeA", "nodeB"
ALL = (NODEA, NODEB)
LAN = "lan1"
VIP = "10.0.0.100"

WIRING = {
    NODEA: [(0, LAN, "10.0.0.1/24")],
    NODEB: [(0, LAN, "10.0.0.2/24")],
}
GATEWAY = {}

COROSYNC_CONF = """\
totem {
    version: 2
    cluster_name: democluster
    transport: udpu
    crypto_cipher: none
    crypto_hash: none
}
nodelist {
    node {
        ring0_addr: 10.0.0.1
        nodeid: 1
        name: nodeA
    }
    node {
        ring0_addr: 10.0.0.2
        nodeid: 2
        name: nodeB
    }
}
quorum {
    provider: corosync_votequorum
    two_node: 1
}
logging {
    to_syslog: yes
}
"""

# name: the hostname inside every container is crun, so pacemaker is told
# the node names here, else both nodes are crun.
def cluster_start(node):
    put(node, "/etc/corosync/corosync.conf", COROSYNC_CONF)
    ssh(node, "mkdir -p /var/lib/pacemaker/cib /run/corosync")
    ssh(node, "corosync")
    time.sleep(3)
    ssh(node, "pacemakerd -f >/var/log/pacemaker/run.log 2>&1 &", check=False)

# Two pacemakers electing a DC and exchanging the CIB: near a minute on a
# slow (nested) machine.
def cib_ready(timeout=150):
    end = time.time() + timeout
    while time.time() < end:
        r = ssh(NODEA, "crm_mon -1 2>/dev/null", check=False)
        if "Online:" in r and "nodeA" in r and "nodeB" in r:
            return True
        time.sleep(3)
    return False

def has_ipaddr2():
    return ssh(NODEA, "test -f /usr/lib/ocf/resource.d/heartbeat/IPaddr2 "
                      "&& echo yes", check=False).strip() == "yes"

def define_vip():
    ssh(NODEA, "crm configure property stonith-enabled=false", check=False)
    ssh(NODEA, "crm configure property no-quorum-policy=ignore", check=False)
    ssh(NODEA, "crm configure primitive vip ocf:heartbeat:IPaddr2 "
               "params ip=%s cidr_netmask=24 "
               "op monitor interval=5s" % VIP, check=False)

def define_dummy():
    ssh(NODEA, "crm configure property stonith-enabled=false", check=False)
    ssh(NODEA, "crm configure property no-quorum-policy=ignore", check=False)
    ssh(NODEA, "crm configure primitive vip ocf:pacemaker:Dummy "
               "op monitor interval=5s", check=False)

def where_vip(timeout=40, want=None):
    """The node the vip runs on; with want, wait until it is that one --
    right after a standby crm_mon still shows the old node for a while."""
    seen = ""
    end = time.time() + timeout
    while time.time() < end:
        r = ssh(NODEA, "crm_mon -1 2>/dev/null", check=False)
        for line in r.split("\n"):
            if "vip" in line and "Started" in line:
                seen = line.strip().split()[-1]
        if seen and (want is None or seen == want):
            return seen
        time.sleep(2)
    return seen

def ping_vip(count=3):
    return ssh(NODEB, "ping -c %d -W 1 %s" % (count, VIP), check=False)

LAYOUT = {NODEA: (-260, 0), LAN: (0, 0), NODEB: (260, 0)}
ETHS = {(NODEA, "eth0"): 0, (NODEB, "eth0"): 180}

CARD = r"""
   nodeA --- lan1 --- nodeB
  10.0.0.1          10.0.0.2      service address 10.0.0.100

  Corosync agrees who is alive; Pacemaker keeps the floating address on one
  live node and moves it to the other when that node goes away.
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

    step("creating and cabling the two machines")
    build(rootfs)
    tools_present(NODEA, "corosync", "pacemakerd", "crm")

    step("act 1: the cluster forms and starts the service")
    story()
    in_parallel(cluster_start, list(ALL), "cluster start")
    if not cib_ready():
        sys.exit("the cluster never formed with both nodes online\n%s"
                 % ssh(NODEA, "crm_mon -1 2>&1; corosync-quorumtool -s 2>&1; "
                              "tail -n 30 /var/log/pacemaker/pacemaker.log "
                              "/var/log/pacemaker/run.log", check=False))
    floating = has_ipaddr2()
    if floating:
        define_vip()
    else:
        story("    (no IPaddr2 agent in the image; using a Dummy resource")
        story("     to show the same failover)")
        define_dummy()
    first = where_vip()
    if not first:
        sys.exit("the resource never started on either node")
    story("    both nodes online; the resource is running on: %s" % first)
    if floating:
        p = ping_vip()
        story("    the service address answers:")
        story(p.strip())
    hold()

    step("act 2: fail the active node, watch the resource move")
    story()
    other = NODEB if first == NODEA else NODEA
    # crm_attribute, pacemaker's own: crm node standby was seen to change
    # nothing, and say nothing, on a node named with a capital.
    got = count_on(other, 0, "udp port 5405",
                   lambda: (ssh(NODEA, "crm_attribute -t nodes -N %s "
                                       "-n standby -v on 2>&1" % first,
                                check=False), time.sleep(10))[0], settle=8.0)
    if got is not None:
        if got["out"].strip():
            story("    " + got["out"].strip())
        story("    corosync membership packets on %s while %s left: %d"
              % (other, first, got["count"]))
    moved = where_vip(90, want=other)
    story("    the resource is now running on: %s" % (moved or "(nowhere)"))
    ok_moved = moved == other
    story()

    step("act 3: the service survived the loss of its node")
    story()
    if floating:
        p = ping_vip()
        story("    the same service address, now on %s:" % moved)
        story(p.strip())
        ok_ping = reached(p)
    else:
        ok_ping = ok_moved
    ssh(NODEA, "crm_attribute -t nodes -N %s -n standby -v off" % first,
        check=False)
    story()

    if not (ok_moved and ok_ping):
        sys.exit("a check failed: moved=%s ping=%s (from %s to %s)\n%s"
                 % (ok_moved, ok_ping, first, moved,
                    ssh(NODEA, "crm_mon -1rf 2>&1; "
                               "tail -n 30 /var/log/pacemaker/pacemaker.log",
                        check=False)))

    story("""
the network is left up. Things to try:

  the live cluster status:
    cloonix cli %s ssh nodeA "crm_mon -1"

  move the resource by hand and watch it go:
    cloonix cli %s ssh nodeA "crm_attribute -t nodes -N nodeB -n standby -v on"
    cloonix cli %s ssh nodeA "crm_attribute -t nodes -N nodeB -n standby -v off"
""" % (NET, NET, NET))

main()
