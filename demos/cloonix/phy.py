#!/usr/bin/env python3
"""phy - a real interface brought into a net: absorb, and macvlan.

    ./phy.py [net]              the net defaults to nemo

A phy is a physical interface of the machine running the net, taken into
cloonix_<net>. absorb takes it away from that machine; macvlan leaves it
its interface and makes an instance with a mac of its own. To play both
without touching the cables of this host, each is done inside a kvm that
runs a cloonix of its own, the two kvm sharing one lan of the outer net::

  outer net (nemo)
  .------------------------------------------------------------------.
  |                                                                  |
  |  kvm mcv  2.2.2.1 on eth0            kvm abs  eth0 taken away    |
  |  .-------------------------.         .------------------------.  |
  |  | inner net fido          |         | inner net mito         |  |
  |  |  c1 -- lanm -- eth0_1 --+- eth0 --- lan1 --- eth0 -+-- eth0_0 -- lana -- c2
  |  |  .2       (macvlan)     |         |          (absorb)       |  .3
  |  '-------------------------'         '------------------------'  |
  '------------------------------------------------------------------'

  ACT 1  two kvm on one lan, both python_trixie.qcow2 (trixie plus python3,
         for the client): mcv, which will macvlan its eth0, and abs, which
         will absorb it
  ACT 2  the cloonix tree installed on this host, and the container image
         trixie, copied into both and installed; a cloonix net started in
         each, fido in mcv, mito in abs -- both nets of cloonix_config.py
         with cloonix_ip 127.0.0.1, so no address on eth0 is needed by the
         server, which matters where eth0 is about to disappear
  ACT 3  in mcv: 2.2.2.1/24 on eth0, then "add phy eth0 --macvlan" makes
         eth0_1, on lanm with the container c1 at 2.2.2.2/24
  ACT 4  in abs: "add phy eth0" makes eth0_0 and eth0 is gone from
         the kvm, ifconfig says so; the container c2 at 2.2.2.3/24 is on
         lana with it
  ACT 5  the pings: c2 to 2.2.2.1 and to 2.2.2.2 across the outer lan,
         c1 to c2, and mcv itself, 2.2.2.1 on eth0, to c1 on the macvlan
         of that same eth0

The inner nets are driven through the outer one: every inner command is a
"cloonix cli nemo ssh <kvm>" carrying a "cloonix cli fido ..." or
"cloonix cli mito ...", printed on two lines, the outer ssh and the inner
command as it reads from a shell of the kvm.

Needs cloonix INSTALLED on this host (/usr/libexec/cloonix), not only on
the PATH, python_trixie.qcow2 (vm_create/cnt_kvm_demo/kvm_python_trixie.sh)
and the container image trixie in /var/lib/cloonix/bulk. Standard library
only.

Image: python_trixie.qcow2, trixie
Size:  2 kvm, 1 lan, and inside each kvm a cloonix with 1 cnt, 1 phy, 1 lan

THE PACKET TO CATCH
-------------------
  The icmp of c2, whose eth0 is in a container of the net mito, seen on
  the OUTER lan: two hops of virtualisation below, nothing but a frame on
  the eth0 of the kvm, which the net mito took away from it:

    cloonix cli nemo cnf abs eth0 sniff=on
    cloonix cli nemo sniff abs eth0 --bytes icmp

  And the two macs of mcv on the same lan, its eth0 and the macvlan eth0_1
  of c1, each answering arp for its own address:

    cloonix cli nemo sniff mcv eth0 --dump arp

ON YOUR OWN
-----------
  The kernel rule the last ping shows: a macvlan in bridge mode talks to
  the wire and to the other macvlans of its parent, never to the parent's
  own address. Inside mcv, with the two inner nets left up:

    cloonix cli fido ssh c1 'ping -c 5 2.2.2.3'      the wire: answers
    ping -c 5 2.2.2.2                                 the parent: does not
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
    CLOONIX, NET, erase_net, hold, run, show, step, story)

USER = "cloonuser"                   # runs the inner nets
KVM_ROOTFS = "python_trixie.qcow2"   # trixie plus python3, for the client
CNT_ROOTFS = "trixie"

# kvm name -> the inner net it runs, and what it does to its eth0
MCV, ABS = "mcv", "abs"
INNER = {MCV: "fido", ABS: "mito"}   # both at 127.0.0.1 in cloonix_config.py
KIND = {MCV: "macvlan", ABS: "absorb"}
PHY = {MCV: "eth0_1", ABS: "eth0_0"} # the name cloonix gives the phy
LAN = {MCV: "lanm", ABS: "lana"}
CNT = {MCV: "c1", ABS: "c2"}
IP_MCV = "2.2.2.1"
IP_C1 = "2.2.2.2"
IP_C2 = "2.2.2.3"

TREE = ["usr/libexec/cloonix", "usr/bin/cloonix", "usr/share/cloonix/cert"]
BULK = "/var/lib/cloonix/bulk"
TAR_TREE = "/tmp/cloonix_tree.tar.gz"
TAR_BULK = "/tmp/%s.tar.gz" % CNT_ROOTFS

def ssh(node, cmd, check=True):
    """One shell command inside a kvm of the outer net."""
    show('cloonix cli %s ssh %s "%s"' % (NET, node, cmd))
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node, cmd],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if check and r.returncode:
        sys.exit("%s: %s\n  -> %s" % (node, cmd,
                                       (r.stderr + r.stdout).strip()
                                       or "no output"))
    return r

def inner(kvm, cmd, check=True):
    """One cloonix command of the inner net of that kvm, typed inside it.

    Printed on two lines: the outer ssh that carries it, then the inner
    command as it reads from a shell of the kvm.
    """
    print("cloonix cli %s ssh %s '%s'" % (NET, kvm, cmd))
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", kvm, cmd],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if check and r.returncode:
        sys.exit("inside %s: %s\n  -> %s" % (kvm, cmd,
                                              (r.stderr + r.stdout).strip()
                                              or "no output"))
    return r

def icli(kvm, args):
    """The inner client line of a kvm, as one string: cloonix cli fido ..."""
    return "cloonix cli %s %s" % (INNER[kvm], args)

def wait_answer(node, timeout=180):
    """A kvm answers ssh a while after its creation."""
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

def wait_inner_cnt(kvm, cnt, timeout=90):
    """A container of an inner net answers its inner ssh."""
    end = time.time() + timeout
    while time.time() < end:
        r = inner(kvm, "cloonix --wait 3 cli %s ssh %s 'echo yes'"
                       % (INNER[kvm], cnt), check=False)
        if "yes" in r.stdout:
            return
        time.sleep(2)
    sys.exit("%s inside %s does not answer" % (cnt, kvm))

def place():
    for line in ("cnf %s x=-250 y=0" % MCV, "cnf lan1 x=0 y=0",
                 "cnf %s x=250 y=0" % ABS,
                 "cnf %s eth0 angle=90" % MCV, "cnf %s eth0 angle=270" % ABS):
        run("cloonix --wait 0.3 cli %s %s" % (NET, line))

# ACT 0 what must be there before anything is created
story(">>> phy.py %s (on the host, outside the crun)" % NET)
r = run("cloonix cli %s dump commands" % NET)
if r.returncode:
    sys.exit("the net %s does not answer:\n  %s\n  start it: cloonix start %s"
             % (NET, (r.stderr + r.stdout).strip(), NET))
if NET in INNER.values():
    sys.exit("the outer net cannot be %s: that name is an inner net here"
             % NET)
for d in ("/" + TREE[0] + "/cloonfs", "/" + TREE[1], "/" + TREE[2]):
    if not os.path.exists(d):
        sys.exit("%s is not here: this demo copies the cloonix INSTALLED on "
                 "this host into the kvm, the bundle (sudo ./install_cloonfs)"
                 " rather than a client on the PATH" % d)
bulk_tgz = os.path.join(BULK, CNT_ROOTFS + ".tar.gz")
bulk_dir = os.path.join(BULK, CNT_ROOTFS)
if not (os.path.isfile(bulk_tgz) or os.path.isdir(bulk_dir)):
    sys.exit("no container image %s in %s: the inner nets need it.\n"
             "  build it: cd vm_create/cnt_kvm && ./cnt_trixie.sh\n"
             "  or fetch: cd %s && wget https://clownix.net/downloads/"
             "cnt_bulk/%s.tar.gz"
             % (CNT_ROOTFS, BULK, BULK, CNT_ROOTFS))

# ACT 1 the two kvm and their lan
step("ACT 1  two kvm on lan1: %s (macvlan) and %s (absorb)" % (MCV, ABS))
erase_net()
place()
for kvm in (MCV, ABS):
    r = run("cloonix cli %s add kvm %s ram=4000 cpu=2 eth=1 %s"
            % (NET, kvm, KVM_ROOTFS))
    if r.returncode:
        sys.exit("no virtual machine on %s:\n  %s\n  build it: cd "
                 "vm_create/cnt_kvm_demo && ./kvm_python_trixie.sh"
                 % (KVM_ROOTFS, (r.stderr + r.stdout).strip()))
    run("cloonix cli %s plug %s eth0 lan1" % (NET, kvm))
for kvm in (MCV, ABS):
    wait_answer(kvm)

# ACT 2 the cloonix of this host and the image into both, a net in each
step("ACT 2  packing %s of this host" % ", ".join("/" + t for t in TREE))
# tar keeps the suid bit of cloonix-suid-power; scp -r would not, and a tree
# of thousands of files travels faster as one.
cmd = ["tar", "czf", TAR_TREE, "-C", "/", "--warning=no-file-changed"]
cmd += TREE + ["usr/bin/" + f for f in os.listdir("/usr/bin")
               if f.startswith("cloonix_") and f.endswith(".py")]
print("tar " + " ".join(cmd[1:]))
r = subprocess.run(cmd, capture_output=True, text=True)
if r.returncode:
    sys.exit("tar of the cloonix tree failed:\n  %s" % r.stderr.strip())
story("      %d MB" % (os.path.getsize(TAR_TREE) // (1024 * 1024)))

if os.path.isfile(bulk_tgz):
    bulk_send = bulk_tgz
else:
    # Packed from inside a container running the image: in bulk the files belong to subuids.
    step("packing %s from inside a container running it" % bulk_dir)
    run("cloonix cli %s add cnt packer eth=1 %s" % (NET, CNT_ROOTFS))
    wait_answer("packer")
    ssh("packer", "tar czf /tmp/bulk.tar.gz --one-file-system "
                  "--warning=no-file-changed --exclude=./tmp/bulk.tar.gz "
                  "--exclude=./cloonixmnt -C / .")
    r = run("cloonix cli %s scp packer:/tmp/bulk.tar.gz %s" % (NET, TAR_BULK))
    if r.returncode:
        sys.exit("scp of the packed image out of packer failed")
    run("cloonix --wait 0.3 cli %s del packer" % NET)
    bulk_send = TAR_BULK
    story("      %d MB" % (os.path.getsize(TAR_BULK) // (1024 * 1024)))

for kvm in (MCV, ABS):
    step("copying both into %s, installing, starting the net %s"
         % (kvm, INNER[kvm]))
    for src in (TAR_TREE, bulk_send):
        line = "cloonix cli %s scp %s %s:/tmp/%s" % (NET, src, kvm,
                                                    os.path.basename(src))
        show(line)
        # Not captured: the progress of a big file is the thing to watch.
        r = subprocess.run(CLOONIX + shlex.split(line)[1:],
                           stdin=subprocess.DEVNULL)
        if r.returncode:
            sys.exit("scp of %s into %s failed" % (src, kvm))
    ssh(kvm, "tar xzpf /tmp/%s -C /" % os.path.basename(TAR_TREE))
    ssh(kvm, "chmod u+s /usr/libexec/cloonix/cloonfs/bin/cloonix-suid-power")
    ssh(kvm, "mkdir -p %s && chmod 1777 /var/lib/cloonix && chmod 777 %s"
        % (BULK, BULK))
    # The tar.gz is the image: the inner server extracts it on first use.
    ssh(kvm, "mv /tmp/%s %s/%s.tar.gz" % (os.path.basename(bulk_send), BULK,
                                          CNT_ROOTFS))
    # The inner net is run by a user, as on a host: a net run by root has no
    # container.
    ssh(kvm, "id %s >/dev/null 2>&1 || useradd -m %s" % (USER, USER))
    # The server daemonizes itself, the ssh comes back; its first words go to
    # a file in case it does not come up.
    ssh(kvm, "runuser -u %s -- sh -c 'cloonix start %s > /tmp/net_%s.log "
             "2>&1 < /dev/null'" % (USER, INNER[kvm], INNER[kvm]))
    end = time.time() + 60
    while True:
        r = inner(kvm, icli(kvm, "dump commands"), check=False)
        if not r.returncode:
            break
        if time.time() > end:
            r = ssh(kvm, "cat /tmp/net_%s.log" % INNER[kvm], check=False)
            sys.exit("the inner net %s never answered inside %s:\n%s"
                     % (INNER[kvm], kvm, r.stdout + r.stderr))
        time.sleep(2)
    story("      the inner net %s answers inside %s" % (INNER[kvm], kvm))

# ACT 3 mcv: the address on eth0, the macvlan, the container beside it
step("ACT 3  in %s: %s on eth0, then a macvlan of eth0 for the net %s"
     % (MCV, IP_MCV, INNER[MCV]))
ssh(MCV, "ip addr add %s/24 dev eth0" % IP_MCV)
ssh(MCV, "ip link set eth0 up")
inner(MCV, icli(MCV, "add phy eth0 --macvlan"))
inner(MCV, icli(MCV, "add cnt %s eth=1 %s" % (CNT[MCV], CNT_ROOTFS)))
inner(MCV, icli(MCV, "plug %s %s" % (PHY[MCV], LAN[MCV])))
inner(MCV, icli(MCV, "plug %s eth0 %s" % (CNT[MCV], LAN[MCV])))
wait_inner_cnt(MCV, CNT[MCV])
inner(MCV, icli(MCV, "ssh %s 'ip addr add %s/24 dev eth0'"
                     % (CNT[MCV], IP_C1)))
inner(MCV, icli(MCV, "ssh %s 'ip link set eth0 up'" % CNT[MCV]))
r = ssh(MCV, "ip -brief link show")
story("      the interfaces of %s: eth0 is still there, the macvlan "
      "instance went into cloonix_%s" % (MCV, INNER[MCV]))
for line in r.stdout.strip().split("\n"):
    story("        " + line)

# ACT 4 abs: eth0 absorbed, gone from the kvm, the container behind it
step("ACT 4  in %s: eth0 absorbed by the net %s" % (ABS, INNER[ABS]))
r = ssh(ABS, "ifconfig -a | grep -o '^[a-z0-9_]*'")
story("      ifconfig before: " + " ".join(r.stdout.split()))
inner(ABS, icli(ABS, "add phy eth0"))
r = ssh(ABS, "ifconfig -a | grep -o '^[a-z0-9_]*'")
story("      ifconfig after:  " + " ".join(r.stdout.split()))
if "eth0" in r.stdout.split():
    sys.exit("eth0 is still in %s: absorb did not take it" % ABS)
story("      eth0 has left the kvm; it is %s in the namespace cloonix_%s"
      % (PHY[ABS], INNER[ABS]))
inner(ABS, icli(ABS, "add cnt %s eth=1 %s" % (CNT[ABS], CNT_ROOTFS)))
inner(ABS, icli(ABS, "plug %s %s" % (PHY[ABS], LAN[ABS])))
inner(ABS, icli(ABS, "plug %s eth0 %s" % (CNT[ABS], LAN[ABS])))
wait_inner_cnt(ABS, CNT[ABS])
inner(ABS, icli(ABS, "ssh %s 'ip addr add %s/24 dev eth0'"
                     % (CNT[ABS], IP_C2)))
inner(ABS, icli(ABS, "ssh %s 'ip link set eth0 up'" % CNT[ABS]))

# ACT 5 the pings
step("ACT 5  who reaches whom")

def ping(kvm, who, target, via_cnt=None):
    """Five pings, from a kvm or from a container of its inner net, their
    last lines on the screen and kept there a moment."""
    cmd = "ping -c 5 -W 3 %s 2>&1 | tail -3" % target
    if via_cnt:
        r = inner(kvm, icli(kvm, "ssh %s '%s'" % (via_cnt, cmd)),
                  check=False)
    else:
        r = ssh(kvm, cmd, check=False)
    ok = " 0% packet loss" in r.stdout
    story()
    story("      %s -> %s: %s" % (who, target, "answers" if ok else "no reply"))
    for line in r.stdout.strip().split("\n"):
        story("        " + line)
    hold(3)
    return ok

ok1 = ping(ABS, "%s (%s)" % (CNT[ABS], IP_C2), IP_MCV, CNT[ABS])
ok2 = ping(ABS, "%s (%s)" % (CNT[ABS], IP_C2), IP_C1, CNT[ABS])
ok3 = ping(MCV, "%s (%s)" % (CNT[MCV], IP_C1), IP_C2, CNT[MCV])
ping(MCV, "%s eth0 (%s)" % (MCV, IP_MCV), IP_C1)
story()
story("""    c2 sits in a container two levels down and its frames leave abs on
    the eth0 that abs no longer has: the absorbed interface is the
    container's wire. They reach the eth0 of mcv, which kept its
    address, and the macvlan instance of that same eth0, which has a
    mac of its own on the lan and belongs to c1.

    The last line is the kernel's macvlan rule, not a fault: a macvlan
    in bridge mode exchanges frames with the wire and with the other
    macvlans of its parent, never with the parent's own stack. mcv and
    c1 share a cable and cannot talk; the wire, and every other machine
    on it, reaches both.""")
if not (ok1 and ok2 and ok3):
    sys.exit("\na ping across the outer lan failed, the network is left up")
story("\nthe two inner nets are left up. To look at them:")
story("    cloonix cli %s ssh %s 'cloonix cli %s dump topo'"
      % (NET, MCV, INNER[MCV]))
story("    cloonix cli %s ssh %s 'ip netns exec cloonix_%s ip link'"
      % (NET, ABS, INNER[ABS]))
