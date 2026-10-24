#!/usr/bin/env python3
r"""windows - a windows guest and two trixie containers pinging on one lan.

    ./windows.py [net] [--no-fw]        default net: nemo

What a windows guest needs before it is on the network, sent from the host
over "cloonix cli ssh"::

      Win1 ---- lan1 ---- trix1
    10.0.0.1            10.0.0.2
       |
      lan1 ---- trix2
              10.0.0.3

    getmac /v /fo csv /nh                       which adapter is eth0
    netsh interface set interface ... enable    it may be disabled
    netsh interface ip set address ... static   the address
    netsh advfirewall firewall add rule ...     let the echo request in

A container arrives on a lan with its address the only thing missing.
Windows arrives with an adapter that may be disabled, no address, and a
firewall that drops the echo request: it pings out and cannot be pinged,
which reads as a broken lan and is not.

  ACT 1  the machines, the lan, the containers addressed
  ACT 2  trix1 pings trix2, so the lan is known good before windows
  ACT 3  the windows adapter found by its mac, enabled, addressed, opened
  ACT 4  the ping both ways

--no-fw leaves out the firewall rule of act 3, the state of a windows guest
first put on a lan: the demo then fails one way only. win.qcow2 is prepared
as the windows chapter of the user book describes.

Image: win.qcow2 in bulk, and trixie for the container
Size:  1 kvm, 2 containers, 1 lan

THE PACKET TO CATCH
-------------------
  The windows interface, the demo run with --no-fw:

    cloonix cli nemo cnf Win1 eth0 sniff=on
    cloonix cli nemo sniff Win1 eth0 --dump icmp

  The packet to catch is the echo request that arrives and is never
  answered: it crossed the lan and reached the interface, and the firewall
  dropped it above the capture point, with nothing reported inside windows.
  The capture tells a host that does not answer from a broken network.

WHICH ADAPTER IS eth0
---------------------
Windows names an adapter after its pci slot, cloonix pinning one slot per
interface: "Ethernet" with one interface, with several a numbering in the
order the miniports finished installing. The last byte of the mac is the
cloonix interface number, and getmac prints name and mac together, csv with
no header, which the demo parses:

    "Ethernet","Red Hat VirtIO...","02-3F-A1-90-2C-00","\Device\..."
                                                 ^^ eth0

Important
  windows boots for a minute or more before its agent answers; the demo
  waits for the node. A node that never turns blue is up without its agent:
  the windows chapter of the user book has the cases.
"""
import csv
import io
import os
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

WANT_FW = "--no-fw" not in sys.argv[1:]

ROOTFS_CHOICES = ("trixie",)
WIN_ROOTFS = "win.qcow2"

WIN = "Win1"
CNT = ("trix1", "trix2")
LAN = "lan1"
NET24 = "10.0.0"
ADDR = {WIN: NET24 + ".1", "trix1": NET24 + ".2", "trix2": NET24 + ".3"}
MASK = "255.255.255.0"

def ssh(node, cmd, check=True, timeout=None):
    """One command in one guest. The same call for linux and for windows.

    On a linux guest this is sh -c, on a windows guest the cxp server runs
    cmd.exe /c, so a command line means there what it means when it is
    typed at the prompt of that guest -- and the two prompts are not the
    same language. Nothing here pretends otherwise.
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
        r = cli("add", "cnt", "probe_win", "eth=1", cand)
        if r.returncode == 0 and wait_exists("probe_win", 20):
            cli("del", "probe_win", ack=ACK_FAST)
            return cand
        if r.returncode == 0:
            said = ("the add of a %s container was taken without a word and "
                    "the container never appeared" % cand)
        else:
            said = (r.stdout + r.stderr).strip()
    cli("del", "probe_win", ack=ACK_FAST)
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

def create_windows():
    """add win, which is add kvm with --windows and --uefi on it.

    No --persistent: each run wants its own overlay over the prepared file,
    and a demo that wrote into win.qcow2 would leave the next one starting
    from whatever this one did to it.
    """
    r = cli("add", "win", WIN, "ram=4000", "cpu=2", "eth=1", WIN_ROOTFS)
    if r.returncode == 0:
        return
    said = (r.stdout + r.stderr).strip()
    sys.exit("the server would not start the windows machine.\n"
             "  it said: %s\n"
             "\n"
             "  This demo needs %s in bulk, prepared as the windows\n"
             "  chapter of the documentation describes -- the virtio\n"
             "  drivers and the cloonix agent written into it once. An\n"
             "  unprepared microsoft image boots and then sits there with\n"
             "  no interface, no agent and a red circle.\n"
             "\n"
             "  Take win.qcow2.vars.fd with it if you copied it from\n"
             "  another machine: the firmware variables are half of that\n"
             "  pair." % (said or "nothing", WIN_ROOTFS))

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
                "    cloonix cli %s dump commands\n"
                "  and look in the syslog for the crun scope of %s:\n"
                "    journalctl -b | grep -i \"crun\\|%s\""
                % (name, last or "nothing at all",
                   NET, name, rootfs, NET, name, name))

def cable(names):
    """Put eth0 of each named machine on the lan.

    Split from the creation on purpose. Cabling a machine that is not there
    is a refusal that ack_fast throws away, so doing all three at once let
    a missing container pass unnoticed -- lan1 appeared because Win1 was
    real, and that looked like the cabling had worked.
    """
    for name in names:
        r = cli("plug", name, "eth0", LAN)
        if r.returncode:
            story("  could not cable %s to %s: %s"
                  % (name, LAN, (r.stdout + r.stderr).strip() or "no reason"))

def wait_node(node, timeout, what):
    """Wait for ssh, and on failure say which of the two failures it was.

    "Never answered ssh" covers two very different things: a machine that
    is not there at all, and a machine that is there and silent. The first
    is the server or the image, the second is inside the guest. Saying only
    the symptom sent the reader looking in the wrong half.
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
        ssh(name, "ip addr add %s/24 dev eth0" % ADDR[name])
        ssh(name, "ip link set dev eth0 up")

# ACT 3 -- the windows interface, over cloonix cli ssh

def win_adapter_name(eth=0):
    """The connection name windows gave cloonix interface <eth>.

    Read and not assumed. Windows names an adapter after its pci slot, so
    the name is stable, but it is stable at whatever windows chose -- with
    one interface it is "Ethernet", with several the numbering follows the
    order the miniports finished installing and not the slot order.

    The last byte of the mac is the cloonix interface number, and getmac
    prints the name and the mac on one csv line with no header, which
    parses here without a quote of powershell anywhere near cmd.exe.
    """
    out = ssh(WIN, "getmac /v /fo csv /nh")
    want = "-%02X" % eth
    seen = []
    for row in csv.reader(io.StringIO(out)):
        if len(row) < 3:
            continue
        name, mac = row[0].strip(), row[2].strip().upper()
        if not mac or "-" not in mac:
            continue
        seen.append("%s (%s)" % (name, mac))
        if mac.endswith(want):
            return name
    raise RuntimeError(
        "no adapter of %s has a mac ending in %s.\n"
        "  windows sees: %s\n"
        "  An empty list means NetKVM is not installed in the image: the\n"
        "  guest then has no interface at all, whatever the qemu line says."
        % (WIN, want, ", ".join(seen) or "no adapter at all"))

def win_configure(adapter, addr, with_firewall):
    """The three commands, and the fourth that is always forgotten.

    Every one of them is sent from the host. Nothing is typed in the guest
    and nothing is prepared in the image beyond its drivers.
    """
    ssh(WIN, 'netsh interface set interface name="%s" admin=enable' % adapter,
        check=False)

    ssh(WIN, 'netsh interface ip set address name="%s" static %s %s'
             % (adapter, addr, MASK))

    if with_firewall:
        ssh(WIN, 'netsh advfirewall firewall add rule'
                 ' name="cloonix icmp in" protocol=icmpv4:8,any'
                 ' dir=in action=allow', check=False)

def win_show(adapter):
    story(ssh(WIN, 'netsh interface ip show config name="%s"' % adapter,
              check=False).strip())

# ACT 2 and 4 -- the pings

def ping_linux(node, target, count=5):
    return subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                           "ping -c %d -W 2 %s" % (count, target)],
                          capture_output=True, text=True,
                          stdin=subprocess.DEVNULL)

def ping_windows(target, count=5):
    """-n and not -c, -w in milliseconds, and the return code lies.

    The ping of windows exits 0 on "Destination host unreachable" because
    something answered, so the count of replies is what has to be read.
    """
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", WIN,
                        "ping -n %d -w 2000 %s" % (count, target)],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    ok = "TTL=" in r.stdout.upper()
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
    up = [n for n in [WIN] + list(CNT) if in_lst(n)]
    if not up:
        return
    story()
    story("Still running: %s" % ", ".join(up))
    for name in up:
        story("    cloonix cli %s del %s" % (NET, name))

LAYOUT = {
    WIN: (-190, 0),
    LAN: (0, 0),
    "trix1": (190, -75),
    "trix2": (190, 75),
}

ETHS = {
    (WIN, "eth0"): 90,
    ("trix1", "eth0"): 248,
    ("trix2", "eth0"): 292,
}

def place():
    for name, (x, y) in LAYOUT.items():
        cli("cnf", name, "x=%s" % x, "y=%s" % y, ack=ACK_FAST)
    for (name, num), deg in ETHS.items():
        cli("cnf", name, num, "angle=%s" % deg, ack=ACK_FAST)

def main():
    step("checking the server")
    check_net()

    step("looking for the container image")
    rootfs = choose_rootfs()
    erase_net()
    place()

    story()
    story("      %s ---- %s ---- %s" % (WIN, LAN, CNT[0]))
    story("    %-8s          %s" % (ADDR[WIN], ADDR[CNT[0]]))
    story("       |")
    story("      %s ---- %s" % (LAN, CNT[1]))
    story("              %s" % ADDR[CNT[1]])
    story()

    step("ACT 1: the machines")
    create_windows()
    cable([WIN])
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
        sys.exit("%s cannot reach %s, and neither is windows.\n"
                 "  The lan itself is wrong, so nothing after this would\n"
                 "  mean anything. The last attempt said:\n    %s"
                 % (CNT[0], CNT[1], out.replace("\n", "\n    ")))
    story("  %s -> %s ok, the lan carries traffic" % (CNT[0], CNT[1]))

    step("ACT 3: the windows interface")
    story("  waiting for %s, which is the slow one -- a container answers"
          % WIN)
    story("  in a second or two, windows boots and then has to start its"
          " agent")
    wait_node(WIN, 300,
              "The machine is running but its agent never pinged, so the\n"
              "  circle is red and there is no way in. C:\\cloonix-agent.log\n"
              "  in the guest says which disc it started from and whether\n"
              "  the host ping ever arrived. See the windows chapter.")
    adapter = win_adapter_name(0)
    story("  eth0 is %r to windows, found by the last byte of its mac"
          % adapter)
    win_configure(adapter, ADDR[WIN], WANT_FW)
    if not WANT_FW:
        story("  --no-fw: the icmp rule is NOT added, which is how a windows")
        story("  guest arrives on a lan")
    win_show(adapter)

    step("ACT 4: the ping, both ways")
    ok_out, out = ping_windows(ADDR[CNT[0]])
    story("  %s -> %s : %s" % (WIN, CNT[0], "ok" if ok_out else "NO REPLY"))
    ok_in, _ = wait_ping(CNT[0], ADDR[WIN], timeout=30)
    story("  %s -> %s : %s" % (CNT[0], WIN, "ok" if ok_in else "NO REPLY"))

    story()
    if ok_out and ok_in:
        story("Both ways. The windows guest is on the lan.")
    elif ok_out and not ok_in:
        story("Out but not in, which is the firewall and not the network.")
        story("The echo request reaches the interface and is dropped above")
        story("it. Sniff eth0 of %s and it is on the capture with no reply" % WIN)
        story("behind it:")
        story("    cloonix cli %s cnf %s eth0 sniff=on" % (NET, WIN))
        story("    cloonix cli %s sniff %s eth0 --dump icmp" % (NET, WIN))
        if WANT_FW:
            story("The rule was asked for, so something else refused it --")
            story("a group policy, or a profile the rule does not cover.")
    else:
        story("Windows did not reach the lan at all. Its address is set, so")
        story("look at the cable first:")
        story("    cloonix cli %s dump commands" % NET)
        story("Then at the adapter, which must be connected and not just")
        story("enabled:")
        story("    cloonix cli %s ssh %s \"netsh interface show interface\""
              % (NET, WIN))
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
