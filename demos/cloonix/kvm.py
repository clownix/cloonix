#!/usr/bin/env python3
"""kvm - two virtual machines on one lan, and a ping between them.

    ./kvm.py [net]              the net defaults to nemo

The first network of the user book, played by a program::

  kvm1 -- lan1 -- kvm2
 1.1.1.1        1.1.1.2

Every cloonix command it runs is printed as it can be typed by hand, so
the demo is the chapter *First run* without the cut and paste. Needs
cloonix on the PATH and the image trixie.qcow2 in /var/lib/cloonix/bulk.

Image: trixie.qcow2
Size:  2 kvm, 1 lan
"""
import os
import shlex
import subprocess
import sys
import time

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    CLOONIX, NET, STORY, erase_net, show, story)

KVM_ROOTFS = "trixie.qcow2"

def run(line):
    """Show a cloonix command line, then give it to the client."""
    show(line)
    return subprocess.run(CLOONIX + shlex.split(line)[1:],
                          capture_output=True, text=True,
                          stdin=subprocess.DEVNULL)

def ssh(node, cmd):
    """One shell command inside a guest; the command is one argument."""
    show('cloonix cli %s ssh %s "%s"' % (NET, node, cmd))
    return subprocess.run(CLOONIX + ["cli", NET, "ssh", node, cmd],
                          capture_output=True, text=True,
                          stdin=subprocess.DEVNULL)

def wait_answer(node, timeout=120):
    """A guest answers ssh a few seconds after its creation."""
    end = time.time() + timeout
    while time.time() < end:
        r = subprocess.run(CLOONIX + ["--wait", "3", "cli", NET, "ssh", node,
                                      "echo yes"],
                           capture_output=True, text=True,
                           stdin=subprocess.DEVNULL)
        if "yes" in r.stdout:
            return
        time.sleep(1)
    sys.exit("%s does not answer" % node)

# Where the canvas draws it, sent before the items exist: x,y is the centre of
# an item, the eth angle runs clockwise from the top.
def place(left, right):
    for line in ("cnf %s x=-250 y=0" % left, "cnf lan1 x=0 y=0",
                 "cnf %s x=250 y=0" % right,
                 "cnf %s eth0 angle=90" % left, "cnf %s eth0 angle=270" % right):
        run("cloonix --wait 0.3 cli %s %s" % (NET, line))

story("kvm1 -- lan1 -- kvm2")
story(" 1.1.1.1        1.1.1.2")
story()
story("1. an empty canvas, and where the three items will be drawn")
story("   del all erases whatever an earlier run left. cnf x= y= and angle= are")
story("   sent before the items exist: the canvas then draws the line above")
story("   instead of choosing places itself.")
erase_net()
place("kvm1", "kvm2")
story("2. two virtual machines on the image trixie, one cable each to lan1")
story("   A virtual machine is a qcow2 of bulk run by qemu/kvm, here with 1000")
story("   MB and 2 cpus. eth=1 gives it one interface, eth0, of the sniffable")
story("   kind. plug cables an interface to a lan, which exists from its")
story("   first cable on: there is no command to create a lan.")
r = run("cloonix cli %s add kvm kvm1 ram=1000 cpu=2 eth=1 %s"
        % (NET, KVM_ROOTFS))
if r.returncode:
    sys.exit("no virtual machine on %s:\n  %s\n  build it: "
             "cd vm_create/cnt_kvm && ./kvm_trixie.sh"
             % (KVM_ROOTFS, (r.stdout + r.stderr).strip()))
run("cloonix cli %s add kvm kvm2 ram=1000 cpu=2 eth=1 %s" % (NET, KVM_ROOTFS))
run("cloonix cli %s plug kvm1 eth0 lan1" % NET)
run("cloonix cli %s plug kvm2 eth0 lan1" % NET)

story("3. the guests answer ssh once started; then an address each, the link up")
story("   the ssh verb reaches a guest by a channel of its own, not through its")
story("   network: it works before any address exists, which is how the")
story("   addresses get there. A virtual machine boots a kernel first: allow")
story("   half a minute.")
wait_answer("kvm1")
wait_answer("kvm2")
ssh("kvm1", "ip addr add dev eth0 1.1.1.1/24")
ssh("kvm1", "ip link set dev eth0 up")
ssh("kvm2", "ip addr add dev eth0 1.1.1.2/24")
ssh("kvm2", "ip link set dev eth0 up")

story("4. the proof: kvm1 pings kvm2 across lan1")
story("   Five echo requests cross the open vswitch bridge that is lan1. The")
story("   summary of ping is printed below; 0% packet loss is the success.")
r = ssh("kvm1", "ping -c 5 1.1.1.2")
story()
story(r.stdout)
if r.returncode:
    sys.exit("the ping failed")
# The result stays in the History popup a moment before the erase.
if STORY:
    time.sleep(4)
story()
story("kvm1 pings kvm2. Erase everything, the server stays:")
show("cloonix cli %s del all" % NET)
