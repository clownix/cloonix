#!/usr/bin/env python3
"""freebsd - a freebsd guest and two trixie containers pinging on one lan.

    ./freebsd.py [net] [--no-addr]      default net: nemo

The words freebsd uses where linux has others, all sent from the host over
"cloonix cli ssh"::

      bsd1 ---- lan1 ---- trix1
    10.0.0.1            10.0.0.2
       |
      lan1 ---- trix2
              10.0.0.3

    ifconfig vtnet0 10.0.0.1/24 up          the address
    ifconfig vtnet0                         read it back
    netstat -rn                             the routing table

The interface is vtnet0, not eth0: freebsd names an interface after its
driver and unit. ifconfig does what ip does, the mask on the address with a
slash and "up" a word of the same command. pf is not enabled in a stock
image, so an address is all the guest needs before it answers a ping, where
windows.py needs a fourth command, for its firewall.

  ACT 1  the machines, the lan, the containers addressed
  ACT 2  trix1 pings trix2, so the lan is known good before freebsd
  ACT 3  the freebsd interface found by its mac, and addressed
  ACT 4  the ping both ways

--no-addr leaves out the address of act 3, the state of a guest first put
on a lan. freebsd.qcow2 is built by vm_create/cnt_kvm/kvm_freebsd.sh.

Image: freebsd.qcow2 in bulk, and trixie for the containers
Size:  1 kvm, 2 containers, 1 lan

THE PACKET TO CATCH
-------------------
  The freebsd interface, the demo run with --no-addr:

    cloonix cli nemo cnf bsd1 eth0 sniff=on
    cloonix cli nemo sniff bsd1 eth0 --dump "icmp or arp"

  The packet to catch is the ARP request left unanswered: trix1 asks who
  has 10.0.0.1, the request reaches the interface, and nothing replies, no
  interface of the guest holding the address. The ping reports "Destination
  Host Unreachable" from the kernel of trix1, and the capture tells a far
  end with no address from a bad cable.

Important
  freebsd is the slow guest: it boots, loads virtio_console, starts
  qemu-guest-agent, then cloonix writes and runs the launcher of the agent;
  the demo waits for the node. A node that stays red, its Desktop tells why:

    a root that will not fsck    CYLINDER GROUP n: INTEGRITY CHECK FAILED,
                                 ABORTING BOOT, a single user prompt: the
                                 image is damaged; build it again,
                                 kvm_freebsd.sh writing to bulk only a
                                 finished install
    no /dev/vtcon                virtio_console not loaded, its line of
                                 /boot/loader.conf missing
    stuck at ping=1 in syslog    the guest agent is on another channel;
                                 in the guest:
                                   cat /var/log/qemu-ga.log
                                   ls -l /dev/vtcon/
                                 "error opening channel ... No such file":
                                 no devfs alias, the raw device being
                                 /dev/ttyV0.2
    an agent that started        /cloonixmnt/tmp/cloonix-agent.log in the
                                 guest, read over the Desktop
"""
import os
import re
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
    ACK_FAST, CLOONIX, NET, cli, erase_net, show, step, story, unavailable,
    EXIT_UNAVAILABLE)

WANT_ADDR = "--no-addr" not in sys.argv[1:]

ROOTFS_CHOICES = ("trixie",)
BSD_ROOTFS = "freebsd.qcow2"

HISTORY = """\
BSD is the Berkeley Software Distribution, begun by Bill Joy at the
University of California in 1977 as additions to AT&T's Unix. 4.2BSD
of 1983, by Joy, Sam Leffler and Mike Karels, carried the first TCP/IP
stack in wide use and the socket API every operating system still
copies; ifconfig, netstat and route are its words. FreeBSD 1.0 was
released in November 1993 by Jordan Hubbard, Nate Williams and Rod
Grimes, from the 386BSD of Bill and Lynne Jolitz, and the project
continues under the FreeBSD Foundation with Marshall Kirk McKusick as
its historian.

Today: FreeBSD serves the video of Netflix, runs inside the PlayStation,
Juniper routers and NetApp filers, and is the base of pfSense and OPNsense
firewalls.
"""

BSD = "bsd1"
CNT = ("trix1", "trix2")
LAN = "lan1"
NET24 = "10.0.0"
ADDR = {BSD: NET24 + ".1", "trix1": NET24 + ".2", "trix2": NET24 + ".3"}
PREFIX = 24

def ssh(node, cmd, check=True, timeout=None):
    """One command in one guest. The same call for linux and for freebsd.

    The cxp server in the guest runs sh -c on both, so a command line means
    there what it means at the prompt of that guest -- and the two prompts
    are not the same language, which is what this demo is about. Nothing
    here pretends otherwise.
    """
    argv = list(CLOONIX)
    if timeout is not None:
        argv += ["--wait", str(timeout)]
    argv += ["cli", NET, "ssh", node, cmd]
    show('cloonix %scli %s ssh %s "%s"'
         % ("" if timeout is None else "-t %s " % timeout, NET, node, cmd))
    r = subprocess.run(argv, capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if check and r.returncode:
        raise RuntimeError("%s: %s -> %s"
                           % (node, cmd,
                              (r.stderr or r.stdout).strip() or "no output"))
    return r.stdout

def in_lst(name):
    """Ask the server what it has: "dump commands" prints the commands
    that would rebuild the topology, so anything that exists is in it."""
    return name in cli("dump", "commands", quiet=True).stdout.split()

def answers(node, timeout=3):
    r = subprocess.run(CLOONIX + ["--wait", str(timeout), "cli", NET, "ssh",
                        node, "echo yes"],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    return "yes" in r.stdout

# before anything is created

def check_net():
    if cli("dump", "commands").returncode:
        unavailable("no answer from %s\n"
                 "  start it:  cloonix start %s" % (NET, NET))

def choose_rootfs():
    """The client cannot list bulk and the server refuses an image it has
    not got, so a throwaway container is the only way to ask."""
    said = ""
    for cand in ROOTFS_CHOICES:
        r = cli("add", "cnt", "probe_bsd", "eth=1", cand)
        if r.returncode == 0 and wait_exists("probe_bsd", 20):
            cli("del", "probe_bsd", ack=ACK_FAST)
            return cand
        if r.returncode == 0:
            said = ("the add of a %s container was taken without a word and "
                    "the container never appeared" % cand)
        else:
            said = (r.stdout + r.stderr).strip()
    cli("del", "probe_bsd", ack=ACK_FAST)
    unavailable("the server would not start a container on %s.\n"
             "  it said: %s\n"
             "  fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_bulk/"
             "trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build it:\n"
             "    cd vm_create/cnt_kvm && ./cnt_trixie.sh"
             % (" or ".join(ROOTFS_CHOICES), said or "nothing"))

# ACT 1 -- the machines

def create_freebsd():
    """add freebsd, which is add kvm with --freebsd on it.

    One word, and what it carries is the LAUNCHER: cloonix mounts cd9660
    from /dev/cd0 in this guest instead of iso9660 from /dev/sr0, and hands
    it the agent disc built for freebsd rather than the linux one. No
    firmware flag -- the release vm image has a freebsd-boot partition
    beside its efi one and seabios starts it.

    No --persistent: each run wants its own overlay over the prepared file,
    and a demo that wrote into freebsd.qcow2 would leave the next one
    starting from whatever this one did to it.
    """
    r = cli("add", "freebsd", BSD, "ram=2000", "cpu=2", "eth=1", BSD_ROOTFS)
    if r.returncode == 0:
        return
    said = (r.stdout + r.stderr).strip()
    sys.exit("the server would not start the freebsd machine.\n"
             "  it said: %s\n"
             "\n"
             "  This demo needs %s in bulk:\n"
             "    cd vm_create/cnt_kvm && ./kvm_freebsd.sh\n"
             "\n"
             "  That builder is the one here that uses the internet, so\n"
             "  doitall.sh does not run it. It needs cloonix installed and\n"
             "  /dev/kvm: it configures the image by booting it, there\n"
             "  being no way to write into a UFS root from linux." % (said or "nothing", BSD_ROOTFS))

def wait_exists(name, timeout=20):
    """Wait for the object to be IN THE SERVER'S LIST.

    A zero from the add is not a creation. The client waits two seconds for
    a status_ko and calls silence success, so an add that the server accepts
    and then cannot carry out comes back as a success and the demo walks on
    with a machine that does not exist.
    """
    end = time.time() + timeout
    while time.time() < end:
        if in_lst(name):
            return True
        time.sleep(1)
    return False

def create_containers(rootfs):
    for name in CNT:
        last = ""
        for attempt in range(6):
            r = cli("add", "cnt", name, "eth=1", rootfs)
            last = (r.stdout + r.stderr).strip()
            if r.returncode == 0 and wait_exists(name):
                break
            if r.returncode == 0:
                last = ("the server took the add without a word and %s "
                        "never appeared in its list" % name)
            time.sleep(2)
        else:
            sys.exit(
                "%s was never created.\n"
                "  the server said: %s\n"
                "\n"
                "  Run the same add by hand -- the demo waits two seconds\n"
                "  for a refusal and this one does not come in two seconds:\n"
                "    cloonix cli %s add cnt %s eth=1 %s\n"
                "    cloonix cli %s dump commands"
                % (name, last or "nothing at all", NET, name, rootfs, NET))

def cable(names):
    """Put eth0 of each named machine on the lan.

    Split from the creation on purpose. Cabling a machine that is not there
    is a refusal that ack_fast throws away, so doing all three at once let a
    missing container pass unnoticed -- lan1 appeared because bsd1 was real,
    and that looked like the cabling had worked.
    """
    for name in names:
        r = cli("plug", name, "eth0", LAN)
        if r.returncode:
            story("  could not cable %s to %s: %s"
                  % (name, LAN, (r.stdout + r.stderr).strip() or "no reason"))

def wait_node(node, timeout, what):
    """Wait for ssh, and on failure say which of the two failures it was.

    "Never answered ssh" covers two very different things: a machine that is
    not there at all, and a machine that is there and silent. The first is
    the server or the image, the second is inside the guest.
    """
    end = time.time() + timeout
    while time.time() < end:
        if answers(node):
            return
        time.sleep(2)
    if not in_lst(node):
        sys.exit("%s IS NOT THERE.\n"
                 "  It never answered ssh because the server has no such\n"
                 "  machine -- the add was taken and the machine never\n"
                 "  appeared. This is not the guest, it is the creation.\n"
                 "  What the server does have:\n%s"
                 % (node, what_exists()))
    sys.exit("%s exists but never answered ssh.\n  %s" % (node, what))

def what_exists():
    r = cli("dump", "commands")
    lines = [l for l in (r.stdout or "").splitlines() if l.strip()]
    if not lines:
        return "    (nothing at all)\n"
    return "".join("    %s\n" % l for l in lines[:20])

def address_containers():
    for name in CNT:
        ssh(name, "ip addr add %s/%d dev eth0" % (ADDR[name], PREFIX))
        ssh(name, "ip link set dev eth0 up")

# ACT 3 -- the freebsd interface, over cloonix cli ssh

def bsd_interface_name(eth=0):
    """The interface name freebsd gave cloonix interface <eth>.

    Read and not assumed, for the same reason the windows demo reads it: a
    guest with several interfaces gets one vtnet per cloonix eth and the
    numbering follows the pci topology, not the order of the qemu line. The
    last byte of the mac IS the cloonix interface number.

    ifconfig prints a stanza per interface: the name at the start of a line
    ending in a colon, then indented lines, one of which is the ether. So
    the current name is carried down until an ether line is found under it.
    """
    out = ssh(BSD, "ifconfig")
    want = ":%02x" % eth
    name = None
    seen = []
    for line in out.splitlines():
        head = re.match(r"^([\w.\-]+):\s", line)
        if head:
            name = head.group(1)
            continue
        mac = re.search(r"\bether\s+([0-9a-fA-F:]{17})\b", line)
        if mac and name:
            addr = mac.group(1).lower()
            seen.append("%s (%s)" % (name, addr))
            if addr.endswith(want):
                return name
    raise RuntimeError(
        "no interface of %s has a mac ending in %s.\n"
        "  freebsd sees: %s\n"
        "  Only lo0 means the vtnet driver did not attach, which is the\n"
        "  virtio_pci and vtnet lines of /boot/loader.conf."
        % (BSD, want, ", ".join(seen) or "no interface at all"))

def bsd_configure(iface, addr):
    """The one command, and it is one for a reason.

    ifconfig does what "ip addr add" and "ip link set up" do together: the
    mask goes on the address and "up" is a word of the same command. And
    there is no fourth command here -- pf is not enabled in a stock image,
    so nothing has to be opened before the guest answers a ping.
    """
    ssh(BSD, "ifconfig %s inet %s/%d up" % (iface, addr, PREFIX))

def bsd_show(iface):
    story(ssh(BSD, "ifconfig %s" % iface, check=False).strip())

# ACT 2 and 4 -- the pings

def ping_linux(node, target, count=5):
    return subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                           "ping -c %d -W 2 %s" % (count, target)],
                          capture_output=True, text=True,
                          stdin=subprocess.DEVNULL)

def ping_freebsd(target, count=5):
    """-c like linux, but -W is in MILLISECONDS here and not seconds, and
    -t is the whole deadline. The count of replies is read rather than the
    return code, which is what the windows half of this pair had to do for
    a different reason -- one habit, two guests."""
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", BSD,
                        "ping -c %d -W 2000 -t 10 %s" % (count, target)],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    ok = bool(re.search(r"bytes from .*: icmp_seq=", r.stdout))
    return ok, r.stdout

def wait_ping(node, target, timeout=60):
    end = time.time() + timeout
    last = ""
    while time.time() < end:
        r = ping_linux(node, target, count=5)
        if r.returncode == 0:
            return True, r.stdout
        last = (r.stdout + r.stderr).strip()
        time.sleep(2)
    return False, last

def leftovers():
    """What is still up when this stops, whether it worked or not."""
    up = [n for n in [BSD] + list(CNT) if in_lst(n)]
    if not up:
        return
    story()
    story("Still running: %s" % ", ".join(up))
    for name in up:
        story("    cloonix cli %s del %s" % (NET, name))

LAYOUT = {
    BSD: (-190, 0),
    LAN: (0, 0),
    "trix1": (190, -75),
    "trix2": (190, 75),
}

ETHS = {
    (BSD, "eth0"): 90,
    ("trix1", "eth0"): 248,
    ("trix2", "eth0"): 292,
}

def place():
    for name, (x, y) in LAYOUT.items():
        cli("cnf", name, "x=%s" % x, "y=%s" % y, ack=ACK_FAST)
    for (name, num), deg in ETHS.items():
        cli("cnf", name, num, "angle=%s" % deg, ack=ACK_FAST)

def main():
    story(HISTORY)
    step("checking the server")
    check_net()

    step("looking for the container image")
    rootfs = choose_rootfs()
    erase_net()
    place()

    story()
    story("      %s ---- %s ---- %s" % (BSD, LAN, CNT[0]))
    story("    %-8s          %s" % (ADDR[BSD], ADDR[CNT[0]]))
    story("       |")
    story("      %s ---- %s" % (LAN, CNT[1]))
    story("              %s" % ADDR[CNT[1]])
    story()

    step("ACT 1: the machines")
    create_freebsd()
    cable([BSD])
    create_containers(rootfs)
    for name in CNT:
        wait_node(name, 60, "the container is in the list and does not "
                            "answer: look at its console")
    cable(list(CNT))
    address_containers()
    story("  %s and %s are up and addressed" % CNT)

    step("ACT 2: the lan, proved between the two containers")
    ok, out = wait_ping(CNT[0], ADDR[CNT[1]])
    if not ok:
        sys.exit("%s cannot reach %s, and neither is freebsd.\n"
                 "  The lan itself is wrong, so nothing after this would\n"
                 "  mean anything. The last attempt said:\n    %s"
                 % (CNT[0], CNT[1], out.replace("\n", "\n    ")))
    story("  %s -> %s ok, the lan carries traffic" % (CNT[0], CNT[1]))

    step("ACT 3: the freebsd interface")
    story("  waiting for %s, which is the slow one -- a container answers"
          % BSD)
    story("  in a second or two, freebsd boots, loads virtio_console, starts")
    story("  qemu-guest-agent, and only then is its own agent launched")
    wait_node(BSD, 300,
              "The machine is running but its agent never pinged, so the\n"
              "  circle is red and there is no way in. Two things to look\n"
              "  at from the guest console:\n"
              "    /cloonixmnt/tmp/cloonix-agent.log -- it started\n"
              "    ls /dev/vtcon -- no such directory means virtio_console\n"
              "  was not loaded, and that is the one line of loader.conf\n"
              "  kvm_freebsd.sh writes for exactly this.")
    iface = bsd_interface_name(0)
    story("  eth0 is %r to freebsd, found by the last byte of its mac"
          % iface)
    if WANT_ADDR:
        bsd_configure(iface, ADDR[BSD])
        bsd_show(iface)
    else:
        story("  --no-addr: the address is NOT set, which is how a guest")
        story("  arrives on a lan")

    step("ACT 4: the ping, both ways")
    ok_out, out = ping_freebsd(ADDR[CNT[0]])
    story("  %s -> %s : %s" % (BSD, CNT[0], "ok" if ok_out else "NO REPLY"))
    ok_in, _ = wait_ping(CNT[0], ADDR[BSD], timeout=30)
    story("  %s -> %s : %s" % (CNT[0], BSD, "ok" if ok_in else "NO REPLY"))

    story()
    if ok_out and ok_in:
        story("Both ways, and both from the one ifconfig. That is the whole")
        story("difference from the windows demo, where the same point needs")
        story("a firewall rule as well.")
    elif not WANT_ADDR:
        story("Neither way, which is what --no-addr asks for: the guest has")
        story("no address, so nothing in it owns %s." % ADDR[BSD])
        story("The packet to catch is the ARP request that goes unanswered:")
        story("    cloonix cli %s cnf %s eth0 sniff=on" % (NET, BSD))
        story("    cloonix cli %s sniff %s eth0 --dump arp" % (NET, BSD))
        story("It is on the capture, so it crossed the lan and reached the")
        story("interface -- and nothing replied. trix1 then reports the")
        story("host unreachable from its own kernel, which reads as a broken")
        story("cable and is not one.")
    elif ok_out and not ok_in:
        story("Out but not in. freebsd enables no firewall, so this is not")
        story("the windows case: look for a pf or ipfw rule that something")
        story("put in the image.")
        story("    cloonix cli %s ssh %s \"pfctl -s rules\"" % (NET, BSD))
    else:
        story("freebsd did not reach the lan at all. Its address is set, so")
        story("look at the cable first:")
        story("    cloonix cli %s dump commands" % NET)
        story("Then at the interface, which must be UP and not just")
        story("addressed:")
        story("    cloonix cli %s ssh %s \"ifconfig %s\"" % (NET, BSD, iface))
    leftovers()

if __name__ == "__main__":
    try:
        main()
    except SystemExit as stop:
        if stop.code and stop.code != EXIT_UNAVAILABLE:
            sys.stderr.write("%s\n" % stop.code)
            leftovers()
            raise SystemExit(1)
        raise
