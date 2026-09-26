#!/usr/bin/env python3
"""realnet - the recipe "A guest on the real network", played inside a kvm.

    ./realnet.py [net]              the net defaults to nemo

The user book gives the recipe for a machine that runs cloonix and is a
station of 192.168.100.0/24: a guest of its net becomes another station of
that segment through a tap put into the kernel bridge that holds the cable.
Here the "real network" is a lan of the outer net, so the recipe can be
played on any machine without touching its cables::

  outer net (nemo)
  .-----------------------------------------------------------------.
  |                                                                 |
  |  kvm endu  192.168.100.175 (br0)         kvm station            |
  |  .------------------------------.        .---------------.      |
  |  | inner net endu               |        |               |      |
  |  |  cnt1 -- lan1 -- tap1 -- br0 -- eth0 --- lan1 --- eth0 |      |
  |  |  .153                        |        | .238          |      |
  |  '------------------------------'        '---------------'      |
  '-----------------------------------------------------------------'

  ACT 1  two kvm on one lan: endu (python_trixie.qcow2, trixie plus
         python3 for the client), which will run a cloonix of its own, and
         station (trixie.qcow2), left untouched at 192.168.100.238
  ACT 2  the cloonix tree installed on this host is copied whole into
         endu, and the container image trixie from the bulk of this host
         into the bulk of endu
  ACT 3  in endu: br0 holds eth0 and 192.168.100.175, then cloonix start endu
  ACT 4  the recipe, on the inner net: cnt1, tap1, the lan, tap1 into br0,
         192.168.100.153 on cnt1, and a ping to station
  ACT 5  the icmp seen on the outer lan, at the eth0 of station

The inner net is driven through the outer one: every inner command is a
"cloonix cli nemo ssh endu" carrying a "cloonix cli endu ..." -- the host
has no address on the lan of the two kvm, so the inner server is reachable
from the outside only that way. The kvm endu carries the net endu of
cloonix_config.py, whose cloonix_ip is 192.168.100.175: the config is the
same file on both sides, so the inner server binds the address the recipe
gives the bridge.

Needs cloonix INSTALLED on this host (/usr/libexec/cloonix), not only on
the PATH, python_trixie.qcow2 (vm_create/cnt_kvm_demo/kvm_python_trixie.sh),
trixie.qcow2 and the container image trixie in /var/lib/cloonix/bulk. Standard library only.

Image: python_trixie.qcow2, trixie.qcow2, trixie
Size:  2 kvm, 1 lan, and inside one kvm a second cloonix with

THE PACKET TO CATCH
-------------------
  The icmp of the inner container, seen on the OUTER lan at the eth0 of
  station, where nothing of the inner net is visible:

    cloonix cli nemo cnf station eth0 sniff=on
    cloonix cli nemo sniff station eth0 --bytes icmp

  The demo counts them itself while cnt1 pings.

Important
  Needs cloonix installed on this host (the bundle, /usr/libexec/cloonix),
  not only a client on the PATH: it is that installation that is copied.
  The kvm endu needs python3 for the client, which trixie.qcow2 has not:
  python_trixie.qcow2 is trixie.qcow2 plus that package. Nothing of the host network is touched.
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
    CLOONIX, NET, erase_net, run, show, step, story)

INNER = "endu"                       # the net of cloonix_config.py at .175
USER = "cloonuser"                   # runs the inner net
KVM_ROOTFS = "python_trixie.qcow2"       # trixie plus python3, for the client
STATION_ROOTFS = "trixie.qcow2"
CNT_ROOTFS = "trixie"

IP_ENDU = "192.168.100.175"
IP_STATION = "192.168.100.238"
IP_CNT1 = "192.168.100.153"

TREE = ["usr/libexec/cloonix", "usr/bin/cloonix", "usr/share/cloonix/cert"]
BULK = "/var/lib/cloonix/bulk"
TAR_TREE = "/tmp/cloonix_tree.tar.gz"
TAR_BULK = "/tmp/%s.tar.gz" % CNT_ROOTFS

def ssh(node, cmd, check=True):
    """One shell command inside a guest of the outer net."""
    show('cloonix cli %s ssh %s "%s"' % (NET, node, cmd))
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node, cmd],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if check and r.returncode:
        sys.exit("%s: %s\n  -> %s" % (node, cmd,
                                       (r.stderr + r.stdout).strip()
                                       or "no output"))
    return r

def inner(cmd, check=True):
    """One cloonix command of the INNER net, typed inside endu.

    Printed on two lines: the outer ssh that carries it, then the inner
    command as it reads from a shell of endu.
    """
    print("cloonix cli %s ssh %s '%s'" % (NET, INNER, cmd))
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", INNER, cmd],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if check and r.returncode:
        sys.exit("inside %s: %s\n  -> %s" % (INNER, cmd,
                                              (r.stderr + r.stdout).strip()
                                              or "no output"))
    return r

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

# ACT 0 what must be there before anything is created

r = run("cloonix cli %s dump commands" % NET)
if r.returncode:
    sys.exit("no answer from a cloonix net called '%s'.\n"
             "  the client said: %s\n"
             "  start it:  cloonix start %s"
             % (NET, (r.stderr + r.stdout).strip() or "nothing", NET))
if NET == INNER:
    sys.exit("the outer net cannot be %s: that name is the inner one" % INNER)

for d in ("/" + TREE[0] + "/cloonfs", "/" + TREE[1], "/" + TREE[2]):
    if not os.path.exists(d):
        sys.exit("%s is not here: this demo copies the cloonix installed on "
                 "this host,\n  so the host needs the bundle install "
                 "(sudo ./install_cloonfs), not only a client" % d)
bulk_tgz = os.path.join(BULK, CNT_ROOTFS + ".tar.gz")
bulk_dir = os.path.join(BULK, CNT_ROOTFS)
if not (os.path.isfile(bulk_tgz) or os.path.isdir(bulk_dir)):
    sys.exit("no container image %s in %s: fetch it\n"
             "    cd %s\n"
             "    wget https://clownix.net/downloads/cnt_bulk/"
             "%s.tar.gz\n"
             "  or build it:  cd vm_create/cnt_kvm && ./cnt_trixie.sh"
             % (CNT_ROOTFS, BULK, BULK, CNT_ROOTFS))

# ACT 1 the two kvm and their lan
step("ACT 1  two kvm on lan1: %s (will run cloonix) and station" % INNER)
# Where the canvas draws it, sent before the items exist: x,y is the centre of
# an item, the eth angle runs clockwise from the top.
def place(left, right):
    for line in ("cnf %s x=-250 y=0" % left, "cnf lan1 x=0 y=0",
                 "cnf %s x=250 y=0" % right,
                 "cnf %s eth0 angle=90" % left, "cnf %s eth0 angle=270" % right):
        run("cloonix --wait 0.3 cli %s %s" % (NET, line))

erase_net()
place(INNER, "station")
r = run("cloonix cli %s add kvm %s ram=4000 cpu=2 eth=1 %s"
        % (NET, INNER, KVM_ROOTFS))
if r.returncode:
    sys.exit("no virtual machine on %s:\n  %s\n  build it: "
             "cd vm_create/cnt_kvm_demo && ./kvm_python_trixie.sh"
             % (KVM_ROOTFS, (r.stdout + r.stderr).strip()))
r = run("cloonix cli %s add kvm station ram=1000 cpu=2 eth=1 %s"
        % (NET, STATION_ROOTFS))
if r.returncode:
    sys.exit("no virtual machine on %s:\n  %s\n  build it: "
             "cd vm_create/cnt_kvm && ./kvm_trixie.sh"
             % (STATION_ROOTFS, (r.stdout + r.stderr).strip()))
run("cloonix cli %s plug %s eth0 lan1" % (NET, INNER))
run("cloonix cli %s plug station eth0 lan1" % NET)
wait_answer(INNER)
wait_answer("station")

ssh("station", "ip addr add %s/24 dev eth0" % IP_STATION)
ssh("station", "ip link set eth0 up")

r = ssh(INNER, "python3 --version", check=False)
if r.returncode:
    sys.exit("no python3 in %s: the cloonix client needs it there.\n"
             "  rebuild %s:  cd vm_create/cnt_kvm_demo && ./kvm_python_trixie.sh"
             % (INNER, KVM_ROOTFS))

# ACT 2 the cloonix of this host into endu, the bulk image into its bulk
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

step("copying both into %s" % INNER)
for src in (TAR_TREE, bulk_send):
    line = "cloonix cli %s scp %s %s:/tmp/%s" % (NET, src, INNER,
                                                os.path.basename(src))
    show(line)
    # Not captured: the progress of a big file is the thing to watch.
    r = subprocess.run(CLOONIX + shlex.split(line)[1:],
                       stdin=subprocess.DEVNULL)
    if r.returncode:
        sys.exit("scp of %s failed" % src)

step("installing in %s" % INNER)
ssh(INNER, "tar xzpf /tmp/%s -C /" % os.path.basename(TAR_TREE))
ssh(INNER, "chmod u+s /usr/libexec/cloonix/cloonfs/bin/cloonix-suid-power")
ssh(INNER, "mkdir -p %s && chmod 1777 /var/lib/cloonix && chmod 777 %s"
    % (BULK, BULK))
# The tar.gz is the image: the inner server extracts it on first use.
ssh(INNER, "mv /tmp/%s %s/%s.tar.gz" % (os.path.basename(bulk_send), BULK,
                                        CNT_ROOTFS))
# The inner net is run by a user, as on a host: a net run by root has no
# container.
ssh(INNER, "id %s >/dev/null 2>&1 || useradd -m %s" % (USER, USER))
ssh(INNER, "cloonix list")

# ACT 3 the bridge of the recipe, then the inner server
step("ACT 3  br0 holds eth0 and %s, as the recipe makes it from the cable"
     % IP_ENDU)
# The agent of the outer net reaches endu on a virtio channel, not on eth0, so
# eth0 can be moved into the bridge under the ssh that does it.
ssh(INNER, "ip link add br0 type bridge")
ssh(INNER, "ip link set eth0 master br0")
ssh(INNER, "ip addr flush dev eth0")
ssh(INNER, "ip addr add %s/24 dev br0" % IP_ENDU)
ssh(INNER, "ip link set eth0 up")
ssh(INNER, "ip link set br0 up")
ssh(INNER, "ip link set br0 type bridge stp_state 0")
r = ssh(INNER, "ping -c 5 %s" % IP_STATION, check=False)
if r.returncode:
    sys.exit("%s does not reach station through br0 before any cloonix is "
             "started inside:\n%s" % (INNER, r.stdout + r.stderr))
story("      %s pings station from br0: the outer lan carries the segment"
      % INNER)

step("cloonix start %s, inside %s, as %s" % (INNER, INNER, USER))
# The server daemonizes itself, the ssh comes back; its first words go to a
# file in case it does not come up.
ssh(INNER, "runuser -u %s -- sh -c 'cloonix start %s > /tmp/net_%s.log "
           "2>&1 < /dev/null'" % (USER, INNER, INNER))
end = time.time() + 60
while True:
    r = inner("cloonix cli %s dump commands" % INNER, check=False)
    if not r.returncode:
        break
    if time.time() > end:
        r = ssh(INNER, "cat /tmp/net_%s.log; ip addr show br0" % INNER,
                check=False)
        sys.exit("the inner net %s never answered inside %s:\n%s"
                 % (INNER, INNER, r.stdout + r.stderr))
    time.sleep(2)
story("      the inner net answers")

# ACT 4 the recipe
step("ACT 4  the recipe on the inner net: cnt1 as a station of the segment")
r = inner("cloonix cli %s add cnt cnt1 eth=1 %s" % (INNER, CNT_ROOTFS),
          check=False)
if r.returncode:
    sys.exit("the inner server would not start a container on %s:\n  %s"
             % (CNT_ROOTFS, (r.stdout + r.stderr).strip()))

def inner_evidence(why):
    """What the inner server said, read inside endu, when it drops something.

    The refusal reaching this side names the symptom only ("endpoint was
    deleted"); the cause is in the syslog of endu and in the log directory
    of the inner net.
    """
    print(why, file=sys.stderr)
    for cmd in ("journalctl -b --no-pager | grep -E 'ERROR|WARNING' | tail -40",
                "ls -l /var/lib/cloonix/%s/log" % INNER,
                "grep -n -E 'ERROR|KO|crun' /var/lib/cloonix/%s/log/"
                "debug_suid_power.log | head -40" % INNER,
                "cat /var/lib/cloonix/%s/log/debug_crun.log" % INNER,
                "cloonix cli %s dump pid" % INNER,
                "mount | grep cgroup; uname -r; ls /var/lib/cloonix/bulk"):
        r = ssh(INNER, cmd, check=False)
        print((r.stdout + r.stderr).rstrip(), file=sys.stderr)
    sys.exit(1)

seen = False
end = time.time() + 30
while time.time() < end:
    r = inner("cloonix cli %s dump commands" % INNER, check=False)
    now = " cnt1 " in r.stdout
    if now:
        seen = True
    elif seen:
        inner_evidence("cnt1 was created inside %s and then dropped:"
                       % INNER)
    time.sleep(1)
if not seen:
    inner_evidence("cnt1 never appeared in the list of the inner net:")

inner("cloonix cli %s add tap tap1" % INNER)
r = inner("cloonix cli %s plug cnt1 eth0 lan1" % INNER, check=False)
if r.returncode:
    inner_evidence("the inner net refused the lan on cnt1:\n  %s"
                   % (r.stdout + r.stderr).strip())
inner("cloonix cli %s plug tap1 lan1" % INNER)
ssh(INNER, "ip link set tap1 master br0")
ssh(INNER, "ip link set tap1 up")

end = time.time() + 60
while True:
    r = inner('cloonix cli %s ssh cnt1 "echo yes"' % INNER, check=False)
    if "yes" in r.stdout:
        break
    if time.time() > end:
        sys.exit("cnt1 does not answer inside %s" % INNER)
    time.sleep(1)
inner('cloonix cli %s ssh cnt1 "ip addr add %s/24 dev eth0"'
      % (INNER, IP_CNT1))
inner('cloonix cli %s ssh cnt1 "ip link set eth0 up"' % INNER)

r = ssh(INNER, "bridge link", check=False)
story("      bridge link in %s:\n%s" % (INNER, r.stdout))

# ACT 5 the ping, watched from the outer lan
step("ACT 5  cnt1 pings station; the outer net sniffs the eth0 of station")
run("cloonix cli %s cnf station eth0 sniff=on" % NET)
time.sleep(2)
line = "cloonix cli %s sniff station eth0 --bytes icmp" % NET
show(line)
snf = subprocess.Popen(CLOONIX + shlex.split(line)[1:],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       text=True, bufsize=1)
time.sleep(2)

r = inner('cloonix cli %s ssh cnt1 "ping -c 5 %s"' % (INNER, IP_STATION),
          check=False)
story(r.stdout)
ok = r.returncode == 0

time.sleep(2)
snf.terminate()
try:
    out, _ = snf.communicate(timeout=5)
except subprocess.TimeoutExpired:
    snf.kill()
    out, _ = snf.communicate()
pkts = [l for l in out.splitlines() if l.startswith("pkt ")]
story("      %d icmp packets seen at the eth0 of station" % len(pkts))

if not ok:
    print("the ping failed. Look in this order:", file=sys.stderr)
    print("  inside %s:  bridge link          tap1 and eth0 both on br0, "
          "forwarding" % INNER, file=sys.stderr)
    print("  inside %s:  cloonix cli %s dump ovs-vsctl list-ports _b_lan1"
          % (INNER, INNER), file=sys.stderr)
    print("  inside %s:  tcpdump -ni br0" % INNER, file=sys.stderr)
    print("  station:    tcpdump -ni eth0", file=sys.stderr)
    sys.exit(1)

r = ssh("station", "ping -c 5 %s" % IP_CNT1, check=False)
story(r.stdout)
story("cnt1, a container of a cloonix running inside %s, is a station of "
      "the segment:\n  station reaches %s, %s reaches station."
      % (INNER, IP_CNT1, IP_CNT1))
story("Inside %s the canvas is https://%s:4494 seen from station, and "
      "everything\ngoes with the two kvm:" % (INNER, IP_ENDU))
show("cloonix cli %s del all" % NET)
