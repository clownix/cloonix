#!/usr/bin/env python3
"""cisco - three ios-xe routers running ospf, configured through the console.

    ./cisco.py [net]                      default net: nemo
    ./cisco.py [net] --conf <vm> <file>   load one config into one router
    ./cisco.py [net] --show <vm> "<cmd>"  run one show and print it

A cisco has no cloonix agent and cannot be given one, an ios-xe image having
no place for its binary: this demo drives the serial console every guest
has::

              lan1                              lan7
  trix1 ----------- cisco1 --- lan3 --- cisco2 ----------- trix2
  1.0.0.1           1.0.0.2    2.0.0.x  5.0.0.2           5.0.0.1
                      |                   |
                    lan5                lan6
                      |                   |
                      +----- cisco3 ------+
                        3.0.0.2   4.0.0.2

A router arrives with its three interfaces in shutdown and no address: the
demo types at it, reading up to the prompt, one config line at a time.

  ACT 1  the machines, the lans, the two containers addressed
  ACT 2  the three consoles, waited for one by one
  ACT 3  a config into each router, through its console
  ACT 4  ospf converges, then trix1 pings trix2 across all three

Image: c8000v.qcow2 in bulk, and trixie for the containers
Size:  3 kvm, 2 containers, 5 lans

THE PACKET TO CATCH
-------------------
  The link between two routers:

    cloonix cli nemo cnf cisco1 eth1 sniff=on
    cloonix cli nemo sniff cisco1 eth1 --dump "ip and not udp and not tcp"

  The hello goes to 224.0.0.5 every ten seconds, answered or not. Caught
  during ACT 3, cisco1 configured and cisco2 not yet, it is one router
  talking into a lan where nothing replies: the router says no neighbour
  for a link down and for a neighbour absent, the capture tells which.

THE CONSOLE
-----------
The console is the qemu chardev, wrapped at launch in a dtach session whose
socket takes a second attach, as the Console panel of the canvas does:

    /var/lib/cloonix/<net>/dtach/<vm>

cisco.py attaches the same way and reads up to the prompt:

  * the pty raw: cooked, the line discipline holds input until a newline,
    so the space that answers --More-- never arrives, and it echoes what is
    written into what is read;
  * a carriage return, not a line feed, as a serial terminal sends;
  * one line at a time, the prompt waited for after each: a serial console
    has no flow control, and a config poured faster than ios parses it
    loses characters.

--conf and --show attach to a router already up: the way to configure a
router created by hand.

Important
  c8000v.qcow2 is not downloadable: Cisco does not redistribute the image,
  installed from a Catalyst 8000V iso obtained with a CCO account. The
  three steps of vm_create/cisco make it, the last putting the console on
  the serial port, without which the router says nothing where the demo
  listens. The routers stay red on the canvas: red is no cloonix agent.
"""
import os

# The only demo that is not portable: it types at three serial consoles
# through a pty, and windows has none.
if os.name == "nt":
    raise SystemExit("cisco drives the routers through their serial "
                     "consoles, which needs a pty: run this one from a "
                     "linux or a macos machine.")

import pty
import re
import select
import subprocess
import sys
import time
import tty

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    ACK_FAST, ARGS, CLOONIX, NET, erase_net, show, ssh, step, story,
    unavailable)

FLAGS = [a for a in sys.argv[1:] if a.startswith("--")]

ROOTFS_CHOICES = ("trixie",)
CISCO_ROOTFS = "c8000v.qcow2"

CISCOS = ("cisco1", "cisco2", "cisco3")
CNT = ("trix1", "trix2")

RAM = 6000
CPU = 4

# node, interface, lan
WIRES = (
    ("trix1", 0, "lan1"), ("cisco1", 0, "lan1"),
    ("trix2", 0, "lan7"), ("cisco2", 0, "lan7"),
    ("cisco1", 1, "lan3"), ("cisco2", 1, "lan3"),
    ("cisco1", 2, "lan5"), ("cisco3", 0, "lan5"),
    ("cisco2", 2, "lan6"), ("cisco3", 1, "lan6"),
)

# The container ends: address, and the router that is their way out.
HOSTS = {"trix1": ("1.0.0.1/24", "1.0.0.2"),
         "trix2": ("5.0.0.1/24", "5.0.0.2")}

# router-id, then one address per cloonix interface.
ROUTERS = {
    "cisco1": ("1.1.1.1", ("1.0.0.2", "2.0.0.1", "3.0.0.1")),
    "cisco2": ("2.2.2.2", ("5.0.0.2", "2.0.0.2", "4.0.0.1")),
    "cisco3": ("3.3.3.3", ("3.0.0.2", "4.0.0.2")),
}

# Interfaces facing a container: ospf point-to-point, no DR election.
TO_HOST = (("cisco1", 0), ("cisco2", 0))

# the console: the qemu chardev is wrapped in a dtach session that takes a second attach.

DTACH_BIN = "/usr/libexec/cloonix/cloonfs/bin/cloonix-dtach"
DTACH_SOCK = "/var/lib/cloonix/%s/dtach/%s"

# A prompt is a name, an optional (config...) part, then > or #, at the very
# end of what has arrived.
PROMPT = re.compile(r"[\r\n]?([\w.\-]+)(\([\w\-]+\))?\s?([>#])\s*$")
MORE = re.compile(r"--\s?More\s?--\s*$")
RETURN_ME = re.compile(r"Press RETURN to get started", re.I)
DIALOG = re.compile(r"initial configuration dialog.*\]\s*:\s*$", re.I | re.S)
TERMINATE = re.compile(r"terminate autoinstall.*\]\s*:\s*$", re.I | re.S)

class ConsoleError(Exception):
    pass

class Console:
    def __init__(self, net, vm):
        self.net = net
        self.vm = vm
        self.sock = DTACH_SOCK % (net, vm)
        self.fd = -1
        self.pid = -1
        self.buf = ""
        self.hostname = None
        self.mode = None

    def open(self):
        if not os.path.exists(self.sock):
            raise ConsoleError("no console socket %s: is %s started?"
                               % (self.sock, self.vm))
        if not os.path.exists(DTACH_BIN):
            raise ConsoleError("no %s" % DTACH_BIN)
        pid, fd = pty.fork()
        if pid == 0:
            env = {"TERM": "vt100", "LANG": "C", "LC_ALL": "C"}
            try:
                os.execve(DTACH_BIN, [DTACH_BIN, "-a", self.sock, "-E", "-z"],
                          env)
            finally:
                os._exit(1)
        self.pid, self.fd = pid, fd
        # Raw pty: cooked input holds the space that answers --More-- until a newline.
        try:
            tty.setraw(self.fd)
        except tty.error:
            pass
        return self

    def close(self):
        if self.fd >= 0:
            try:
                os.close(self.fd)
            except OSError:
                pass
            self.fd = -1
        if self.pid > 0:
            try:
                os.kill(self.pid, 9)
                os.waitpid(self.pid, 0)
            except OSError:
                pass
            self.pid = -1

    def __enter__(self):
        return self.open()

    def __exit__(self, *exc):
        self.close()

    def _read(self, timeout):
        try:
            r, _, _ = select.select([self.fd], [], [], timeout)
        except (OSError, ValueError):
            raise ConsoleError("console of %s went away" % self.vm)
        if not r:
            return ""
        try:
            data = os.read(self.fd, 8192)
        except OSError:
            raise ConsoleError("console of %s closed" % self.vm)
        if not data:
            raise ConsoleError("console of %s closed" % self.vm)
        return data.decode("utf-8", "replace")

    def _write(self, text):
        # CR and not LF: that is what a serial terminal sends and what ios
        # answers to.
        os.write(self.fd, text.encode())

    def expect(self, timeout=60):
        """Read up to a prompt, answering what stands in the way of one."""
        end = time.time() + timeout
        while True:
            m = PROMPT.search(self.buf)
            if m:
                out = self.buf[:m.start()]
                self.hostname, self.mode = m.group(1), m.group(3)
                self.buf = ""
                return out
            if MORE.search(self.buf):
                self.buf = MORE.sub("", self.buf)
                self._write(" ")
                continue
            if DIALOG.search(self.buf) or TERMINATE.search(self.buf):
                self.buf = ""
                self._write("no\r")
                continue
            if RETURN_ME.search(self.buf):
                self.buf = ""
                self._write("\r")
                continue
            left = end - time.time()
            if left <= 0:
                raise ConsoleError("%s: no prompt after %ds, last saw:\n%s"
                                   % (self.vm, timeout, self.buf[-400:]))
            self.buf += self._read(min(left, 1.0))

    def wait_ready(self, timeout=900):
        """Wait for a usable prompt, through as much boot as is left.

        Listening comes first and the carriage return only after a silent
        stretch. The other way round, the nudge lands in whatever question
        the router is sitting on, and a bare return is an answer to the
        autoinstall dialog rather than a no-op.
        """
        end = time.time() + timeout
        while time.time() < end:
            try:
                self.expect(timeout=min(10, max(1, end - time.time())))
            except ConsoleError:
                self._write("\r")
                continue
            if self.mode == ">":
                self._write("enable\r")
                self.expect(timeout=20)
            return self
        raise ConsoleError("%s: no console prompt after %ds" %
                           (self.vm, timeout))

    def send(self, line, timeout=60):
        """One line, then the prompt it comes back with.

        Waiting for that prompt after every line IS the flow control: a
        serial console has none, and a config poured in faster than ios
        parses it loses characters.
        """
        self._write(line + "\r")
        return self.expect(timeout=timeout)

    def show(self, cmd, timeout=60):
        out = self.send(cmd, timeout=timeout)
        return out.split("\n", 1)[1] if "\n" in out else ""

    def load_config(self, text, timeout=60):
        self.send("configure terminal", timeout=timeout)
        for raw in text.split("\n"):
            line = raw.rstrip()
            if not line.strip() or line.strip().startswith("!"):
                continue
            if line.strip() == "end":
                continue
            out = self.send(line, timeout=timeout)
            if "% Invalid input" in out or "% Incomplete" in out:
                raise ConsoleError("%s refused: %s\n%s"
                                   % (self.vm, line.strip(), out.strip()))
        self.send("end", timeout=timeout)
        return self

def router_config(name):
    rid, addrs = ROUTERS[name]
    out = ["hostname %s" % name,
           "interface Loopback0",
           " ip address %s 255.255.255.255" % rid,
           "exit"]
    for num, addr in enumerate(addrs):
        out.append("interface GigabitEthernet%d" % (num + 1))
        out.append(" ip address %s 255.255.255.0" % addr)
        if (name, num) not in TO_HOST:
            out.append(" ip ospf network point-to-point")
            out.append(" ip ospf cost 1")
        # virtio-net does not negotiate
        out.append(" speed 1000")
        out.append(" no negotiation auto")
        out.append(" no shutdown")
        out.append("exit")
    out.append("router ospf 1")
    out.append(" router-id %s" % rid)
    out.append(" redistribute connected subnets")
    out.append(" network %s 0.0.0.0 area 0" % rid)
    for addr in addrs:
        net24 = addr.rsplit(".", 1)[0] + ".0"
        out.append(" network %s 0.0.0.255 area 0" % net24)
    out.append("exit")
    return "\n".join(out)

# the client

def cli(*args, **kw):
    ack = kw.get("ack")
    cmd = list(CLOONIX)
    if ack is not None:
        cmd += ["--wait", str(ack)]
    a = [str(x) for x in args]
    cmd += ["cli", NET, a[0]] + a[1:]
    if not kw.get("quiet"):
        show("cloonix %scli %s %s"
             % ("" if ack is None else "--wait %s " % ack,
                NET, " ".join(a)))
    return subprocess.run(cmd, capture_output=True, text=True,
                          stdin=subprocess.DEVNULL)

def in_lst(name):
    """Ask the server what it has: "dump commands" prints the commands
    that would rebuild the topology, so anything that exists is in it."""
    return name in cli("dump", "commands", quiet=True).stdout.split()

def wait_exists(name, timeout=30):
    end = time.time() + timeout
    while time.time() < end:
        if in_lst(name):
            return True
        time.sleep(1)
    return False

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
        r = cli("add", "cnt", "probe_cisco", "eth=1", cand)
        if r.returncode == 0 and wait_exists("probe_cisco", 20):
            cli("del", "probe_cisco", ack=ACK_FAST)
            return cand
        said = ((r.stdout + r.stderr).strip() if r.returncode else
                "the add was taken without a word and nothing appeared")
    cli("del", "probe_cisco", ack=ACK_FAST)
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

def no_image(said):
    """Where a c8000v comes from, which is not a download.

    Cisco does not redistribute it and neither does cloonix, so there is
    nothing to fetch: it is installed from an iso obtained with a CCO
    account, and vm_create/cisco does that in three steps.
    """
    unavailable("%s is not in /var/lib/cloonix/bulk.\n"
             "  the server said: %s\n"
             "\n"
             "  It is not downloadable. It is installed from a Cisco\n"
             "  Catalyst 8000V iso, which needs a CCO account, and the\n"
             "  three steps that turn that iso into this image are in:\n"
             "\n"
             "    vm_create/cisco/README\n"
             "    cd vm_create/cisco && ./doitall.sh --all <the iso>\n"
             "\n"
             "  The last of the three matters for this demo: it puts the\n"
             "  console on the serial port, and without it the router\n"
             "  boots and says nothing where this demo is listening."
             % (CISCO_ROOTFS, said or "nothing"))

def bad_start(name, said):
    """The image is there and the guest still did not come up.

    Two different faults answered the same way would send someone to
    rebuild an image that is fine, so this one says what it is instead:
    the server names a file holding what qemu printed, and that file is
    the answer nearly every time.
    """
    where = "/var/lib/cloonix/%s/dtach/%s.stderr" % (NET, name)
    sys.exit("%s did not start, though %s is there.\n"
             "  the server said: %s\n"
             "\n"
             "  What qemu printed is in:\n"
             "    %s\n"
             "\n"
             "  A guest asked for %d MB and %d cpus: check free memory and\n"
             "  that /dev/kvm is usable before looking further."
             % (name, CISCO_ROOTFS, said or "nothing", where, RAM, CPU))

def add_failed(name, said):
    """Which of the two it was. The server refuses a rootfs it has not got
    with a plain "not found"; anything else happened after the launch."""
    low = said.lower()
    if "not found" in low or "no such file" in low or "is not in" in low:
        no_image(said)
    bad_start(name, said)

# the acts

def act1(rootfs):
    step("ACT 1  the machines and the lans")
    for name in CNT:
        cli("add", "cnt", name, "eth=1", rootfs)
    for name in CISCOS:
        r = cli("add", "cisco", name, "ram=%d" % RAM, "cpu=%d" % CPU,
                "eth=3", CISCO_ROOTFS)
        if r.returncode:
            add_failed(name, (r.stdout + r.stderr).strip())
    for name in list(CNT) + list(CISCOS):
        if wait_exists(name, 60):
            continue
        if name in CISCOS:
            bad_start(name, "the add was taken and nothing appeared")
        sys.exit("%s never appeared" % name)
    for node, num, lan in WIRES:
        cli("plug", node, "eth%d" % num, lan)
    step("  %d machines, %d cables" % (len(CNT) + len(CISCOS), len(WIRES)))

    for name, (addr, gw) in HOSTS.items():
        end = time.time() + 120
        while time.time() < end and not answers(name):
            time.sleep(2)
        if not answers(name):
            sys.exit("%s never answered ssh" % name)
        ssh(name, "ip addr add dev eth0 %s" % addr)
        ssh(name, "ip link set dev eth0 up")
        ssh(name, "ip route add default via %s" % gw)
        step("  %s is %s, out through %s" % (name, addr, gw))

def act2():
    step("ACT 2  the three consoles")
    ready = {}
    for name in CISCOS:
        con = Console(NET, name).open()
        con.wait_ready()
        step("  %s answers on its console as %s" % (name, con.hostname))
        ready[name] = con
    return ready

def act3(cons):
    step("ACT 3  a config into each router, one line at a time")
    for name in CISCOS:
        cons[name].load_config(router_config(name))
        step("  %s configured, prompt is now %s"
             % (name, cons[name].hostname))

def act4(cons, timeout=300):
    step("ACT 4  ospf, then the ping across all three")
    want = {"cisco1": 2, "cisco2": 2, "cisco3": 2}
    end = time.time() + timeout
    while time.time() < end:
        seen = {}
        for name in CISCOS:
            seen[name] = cons[name].show("show ip ospf neighbor").count("FULL")
        step("  neighbours full: %s"
             % " ".join("%s %d/%d" % (n, seen[n], want[n]) for n in CISCOS))
        if all(seen[n] >= want[n] for n in CISCOS):
            break
        time.sleep(15)
    else:
        sys.exit("ospf did not converge in %ds" % timeout)
    step("  trix1 -> trix2, three routers away")
    story(ssh("trix1", "ping -c 5 5.0.0.1"))
    story(ssh("trix1", "traceroute -n 5.0.0.1", check=False))

def one_shot():
    """--conf and --show: the console driver on its own, no topology built.

    The same code the demo uses, reachable for a router that is already up
    and wants a config or a question.
    """
    if "--conf" in FLAGS:
        if len(ARGS) < 3:
            sys.exit("usage: %s <net> --conf <vm> <config file>" % sys.argv[0])
        vm, path = ARGS[1], ARGS[2]
        with Console(NET, vm) as con:
            con.wait_ready()
            con.load_config(open(path).read())
        story("%s: %s loaded" % (vm, path))
        return 0
    if len(ARGS) < 3:
        sys.exit("usage: %s <net> --show <vm> \"<command>\"" % sys.argv[0])
    vm, cmd = ARGS[1], ARGS[2]
    with Console(NET, vm) as con:
        con.wait_ready()
        story(con.show(cmd))
    return 0

LAYOUT = {
    "trix1": (-570, 0),
    "lan1": (-380, 0),
    "cisco1": (-190, 0),
    "lan3": (0, -75),
    "lan5": (0, 75),
    "cisco2": (190, -75),
    "cisco3": (190, 75),
    "lan7": (380, -75),
    "lan6": (380, 75),
    "trix2": (570, 0),
}

ETHS = {
    ("cisco1", "eth0"): 270,
    ("cisco1", "eth1"): 68,
    ("cisco1", "eth2"): 112,
    ("cisco2", "eth0"): 90,
    ("cisco2", "eth1"): 270,
    ("cisco2", "eth2"): 128,
    ("cisco3", "eth0"): 270,
    ("cisco3", "eth1"): 90,
    ("cisco3", "eth2"): 180,
    ("trix1", "eth0"): 90,
    ("trix2", "eth0"): 292,
}

def place():
    for name, (x, y) in LAYOUT.items():
        cli("cnf", name, "x=%s" % x, "y=%s" % y, ack=ACK_FAST)
    for (name, num), deg in ETHS.items():
        cli("cnf", name, num, "angle=%s" % deg, ack=ACK_FAST)

def main():
    if "--conf" in FLAGS or "--show" in FLAGS:
        return one_shot()
    check_net()
    rootfs = choose_rootfs()
    erase_net()
    place()
    act1(rootfs)
    cons = act2()
    try:
        act3(cons)
        act4(cons)
    finally:
        for con in cons.values():
            con.close()
    step("done")
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except ConsoleError as err:
        sys.exit("console: %s" % err)
    except KeyboardInterrupt:
        sys.exit("interrupted")
