#!/usr/bin/env python3
"""frrspider_bgp - the same web of sixty routers, speaking bgp.

    ./frrspider_bgp.py [net]       the net defaults to nemo

The topology of frrspider.py with another protocol: each router an
autonomous system of its own, peering with its direct neighbours over the
shared /24, the way a data centre fabric is built. The peerings are written
nowhere: everything on a lan shares a /24, so who peers with whom follows
from the two tables of the script.

Image: frr_trixie
Size:  60 containers, 35 lans

THE PACKET TO CATCH
-------------------
  bgp is tcp 179:

    cloonix cli nemo cnf cod1 eth0 sniff=on
    cloonix cli nemo sniff cod1 eth0 --dump tcp port 179

  The packet to catch is an UPDATE after a link is taken down: the
  withdrawn prefixes travel from neighbour to neighbour, each router
  telling the next what it can no longer reach.

Important
  No configuration is in the image: frr is installed, everything else is
  written at run time.
"""
import concurrent.futures
import math
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
    CLOONIX, NET, erase_net, show, story, unavailable)

# frr_trixie only.
ROOTFS_CHOICES = ("frr_trixie",)

HISTORY = """\
BGP was sketched by Kirk Lougheed of Cisco and Yakov Rekhter of IBM at an
IETF meeting in January 1989, on napkins by legend, and published as RFC
1105 the same year; BGP-4 of 1994 still runs the internet. Every network is
an autonomous system, and a router tells its neighbours which prefixes it
can reach and through which systems the path goes. Using it between the
switches of one data centre, one AS per switch, was described by Petr
Lapukhov in RFC 7938 in 2016.

Today: BGP holds the internet together, close to a million IPv4 prefixes
exchanged between some seventy-five thousand autonomous systems, and the
same protocol runs inside data centres between racks.
"""

# Private range, one per router.
ASN_BASE = 65000

# name, interfaces, host number, router id, the /24s it sits on
NODES_TABLE = """
tod1   2 1  1  10,11  nod1   2 11 11 17,18  cod1   4 6  6  10,15,16,17
lod11  2 31 31 30,31  nod11  2 16 16 15,30  lod12  2 32 32 31,32
nod12  2 17 17 16,31  lod13  2 33 33 32,33  nod13  2 18 18 17,32
tod2   2 2  2  11,12  nod2   2 12 12 23,24  cod2   4 7  7  11,18,19,23
lod21  2 34 34 33,34  nod21  2 19 19 18,33  lod22  2 35 35 34,35
nod22  2 20 20 19,34  lod23  2 36 36 35,36  nod23  2 21 21 23,35
tod3   2 3  3  12,13  nod3   2 13 13 25,26  cod3   4 8  8  12,24,20,25
lod31  2 37 37 36,37  nod31  2 22 22 24,36  lod32  2 38 38 37,38
nod32  2 23 23 20,37  lod33  2 39 39 38,39  nod33  2 24 24 25,38
tod4   2 4  4  13,14  nod4   2 14 14 27,28  cod4   4 9  9  13,26,21,27
lod41  2 40 40 39,40  nod41  2 25 25 26,39  lod42  2 41 41 40,41
nod42  2 26 26 21,40  lod43  2 42 42 41,42  nod43  2 27 27 27,41
tod5   2 5  5  14,10  nod5   2 15 15 29,15  cod5   4 10 10 14,28,22,29
lod51  2 43 43 42,43  nod51  2 28 28 28,42  lod52  2 44 44 43,44
nod52  2 29 29 22,43  lod53  2 45 45 44,30  nod53  2 30 30 29,44
sod30  1 1  56 30  sod31  1 1  57 31  sod32  1 1  58 32
sod33  1 1  59 33  sod34  1 1  60 34  sod35  1 1  61 35
sod36  1 1  62 36  sod37  1 1  63 37  sod38  1 1  64 38
sod39  1 1  65 39  sod40  1 1  77 40  sod41  1 1  78 41
sod42  1 1  79 42  sod43  1 1  80 43  sod44  1 1  81 44
"""

# lan, then the interfaces on it as node:number
LANS_TABLE = """
lan10  tod1:0 tod5:1 cod1:0  lan11  tod1:1 tod2:0 cod2:0
lan12  tod2:1 tod3:0 cod3:0  lan13  tod3:1 tod4:0 cod4:0
lan14  tod4:1 tod5:0 cod5:0  lan15  nod5:1 nod11:0 cod1:1
lan16  nod12:0 cod1:2  lan17  nod1:0 nod13:0 cod1:3
lan18  nod1:1 nod21:0 cod2:1  lan19  nod22:0 cod2:2
lan20  nod32:0 cod3:2  lan21  nod42:0 cod4:2
lan22  nod52:0 cod5:2  lan23  nod2:0 nod23:0 cod2:3
lan24  nod2:1 nod31:0 cod3:1  lan25  nod3:0 nod33:0 cod3:3
lan26  nod3:1 nod41:0 cod4:1  lan27  nod4:0 nod43:0 cod4:3
lan28  nod4:1 nod51:0 cod5:1  lan29  nod5:0 nod53:0 cod5:3
lan30  nod11:1 lod11:0 lod53:1 sod30:0  lan31  nod12:1 lod11:1 lod12:0 sod31:0
lan32  nod13:1 lod12:1 lod13:0 sod32:0  lan33  nod21:1 lod13:1 lod21:0 sod33:0
lan34  nod22:1 lod21:1 lod22:0 sod34:0  lan35  nod23:1 lod22:1 lod23:0 sod35:0
lan36  nod31:1 lod23:1 lod31:0 sod36:0  lan37  nod32:1 lod31:1 lod32:0 sod37:0
lan38  nod33:1 lod32:1 lod33:0 sod38:0  lan39  nod41:1 lod33:1 lod41:0 sod39:0
lan40  nod42:1 lod41:1 lod42:0 sod40:0  lan41  nod43:1 lod42:1 lod43:0 sod41:0
lan42  nod51:1 lod43:1 lod51:0 sod42:0  lan43  nod52:1 lod51:1 lod52:0 sod43:0
lan44  nod53:1 lod52:1 lod53:0 sod44:0
"""

def parse_nodes(text):
    """name -> (interfaces, host number, router id, [networks])"""
    out, tok = {}, text.split()
    while tok:
        name, neth, nb, num, nets = tok[:5]
        del tok[:5]
        out[name] = (int(neth), int(nb), int(num),
                     [int(x) for x in nets.split(",")])
    return out

def parse_lans(text):
    """[(node, interface, lan), ...]"""
    out = []
    for line in text.strip().split("\n"):
        cur = None
        for word in line.split():
            if ":" in word:
                node, num = word.split(":")
                out.append((node, int(num), cur))
            else:
                cur = word
    return out

NODES = parse_nodes(NODES_TABLE)
LANS = parse_lans(LANS_TABLE)

RING = {"tod": 110, "cod": 240, "nod": 370, "lod": 500, "sod": 580}

def angle_of(name):
    kind, idx = name[:3], name[3:]
    if kind == "tod":
        return (int(idx) - 1) * 72
    if kind == "cod":
        return (int(idx) - 1) * 72 - 36
    if kind == "sod":
        return -60 + 24 * (int(idx) - 30)
    if len(idx) == 1:
        return (int(idx) - 1) * 72
    k, j = int(idx[0]), int(idx[1])
    deg = (k - 1) * 72 - 36 + (j - 2) * 24
    return deg + 12 if kind == "lod" else deg

def xy_of(name):
    a = math.radians(angle_of(name))
    r = RING[name[:3]]
    return int(r * math.sin(a)), int(-r * math.cos(a))

def place():
    pos = {n: xy_of(n) for n in NODES}
    lans = {}
    for node, num, lan in LANS:
        lans.setdefault(lan, []).append(node)
    for lan, members in lans.items():
        pos[lan] = (sum(pos[m][0] for m in members) // len(members),
                    sum(pos[m][1] for m in members) // len(members))
    for name, (x, y) in pos.items():
        cli("cnf", name, "x=%s" % x, "y=%s" % y, ack=0.3, quiet=True)
    for node, num, lan in LANS:
        dx, dy = pos[lan][0] - pos[node][0], pos[lan][1] - pos[node][1]
        deg = int(math.degrees(math.atan2(dx, -dy))) % 360
        cli("cnf", node, "eth%d" % num, "angle=%d" % deg, ack=0.3,
            quiet=True)

# node -> the lans it is on, so the peerings can be worked out
LANS_OF = {}
for _n, _i, _l in LANS:
    LANS_OF.setdefault(_n, []).append(_l)

# Where the debian package puts it.
FRRINIT = "/usr/lib/frr/frrinit.sh"

VTYSH_CONF = "service integrated-vtysh-config\\nusername root nopassword"

# The daemons are told to stay root, and root is put in the vty group.
DAEMON_OPTS = ('zebra_options="-A 127.0.0.1 -u root -g root"\\n'
               'ospfd_options="-A 127.0.0.1 -u root -g root"\\n'
               'bgpd_options="-A 127.0.0.1 -u root -g root"\\n'
               'mgmtd_options="-A 127.0.0.1 -u root -g root"\\n'
               'staticd_options="-A 127.0.0.1 -u root -g root"\\n')

VTY_GROUP_FIX = ("grep -q '^frrvty:.*root' /etc/group || "
                 "sed -i '/^frrvty:/ s/$/,root/' /etc/group; true")

def frr_setup(node, daemons, conf, expect):
    """Write the files frr needs, then start it and wait until it is up."""
    ssh(node, "mkdir -p /etc/frr /var/log/frr /var/lib/frr /var/tmp/frr")
    ssh(node, "printf '%s\\n' > /etc/frr/vtysh.conf" % VTYSH_CONF)
    ssh(node, "printf '%s' > /etc/frr/daemons" % daemons)
    ssh(node, "printf '%s' >> /etc/frr/daemons" % DAEMON_OPTS)
    ssh(node, "printf '%s\\n' > /etc/frr/frr.conf" % conf.replace("\n", "\\n"))
    ssh(node, VTY_GROUP_FIX)
    ssh(node, "chmod 640 /etc/frr/frr.conf /etc/frr/daemons /etc/frr/vtysh.conf")
    ssh(node, "rm -rf /run/frr; mkdir -p /run/frr; chmod 755 /run/frr")
    ssh(node, "%s start </dev/null >/dev/null 2>&1" % FRRINIT)
    ssh(node, "for i in $(seq 1 20); do pgrep -x %s >/dev/null && exit 0; "
              "sleep 1; done; "
              "echo '%s did not start, see /var/log/syslog'; exit 1"
              % (expect, expect))
    # frr.conf applied and checked: under load the daemons can come up after
    # frrinit has run vtysh -b, and watchfrr restarts them empty.
    marker = next((l.strip() for l in conf.split("\n")
                   if l.startswith("router ")), None)
    if marker:
        ssh(node, "for i in $(seq 1 30); do vtysh -b >/dev/null 2>&1; "
                  "vtysh -c 'show running-config' 2>/dev/null | "
                  "grep -qF '%s' && exit 0; sleep 1; done; "
                  "echo 'frr.conf not applied, see /var/log/syslog'; exit 1"
                  % marker)

def frr_present(node):
    """Fail early and clearly rather than at the first missing file."""
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node, "test -x " + FRRINIT],
                       capture_output=True, stdin=subprocess.DEVNULL)
    if r.returncode:
        unavailable("%s has no %s: the image carries no frr.\n"
                 "  fetch it:\n"
                 "    cd /var/lib/cloonix/bulk\n"
                 "    wget https://clownix.net/downloads/cnt_demo_bulk/"
                 "frr_trixie.tar.gz\n"
                 "    (cloonix extracts it at the first use)\n"
                 "  or build it:\n"
                 "    cd vm_create/cnt_kvm_demo && ./cnt_frr_trixie.sh"
                 % (node, FRRINIT))

def cli(*args, **kw):
    ack = kw.get("ack")
    cmd = list(CLOONIX)
    if ack is not None:
        cmd += ["--wait", str(ack)]
    a = [str(x) for x in args]
    if not kw.get("quiet"):
        show("cloonix %scli %s %s"
             % ("" if ack is None else "--wait %s " % ack,
                NET, " ".join(a)))
    return subprocess.run(cmd + ["cli", NET, a[0]] + a[1:],
                          capture_output=True, text=True)

def verify_created(names):
    """Count what the server has, and name anything missing.

    create_cnt catches a refusal, and a refusal is not the only way to lose
    a container: a request the server drops without answering looks exactly
    like a success to a client that waited and heard nothing. The only
    reliable check is to ask the server what it ended up with.

    "dump commands" prints the commands that would rebuild the topology, so
    every container that exists appears in it. If NONE of the names are
    found the output is not the shape this expects -- a different version,
    most likely -- and the check says nothing rather than crying wolf.
    """
    out = cli("dump", "commands").stdout
    found = [n for n in names if (" " + n + " ") in out]
    if not found:
        return                       # not the format expected, so no opinion
    missing = [n for n in names if n not in found]
    if missing:
        raise SystemExit(
            "the server has %d of the %d containers. Missing: %s\n"
            "  They were asked for and did not arrive, which on a topology\n"
            "  this size is usually the burst being too fast for the switch.\n"
            "  Running the demo again is the quick answer; the slow one is\n"
            "  to add them by hand with cloonix cli %s add cnt <name> ..."
            % (len(found), len(names), ", ".join(missing), NET))
    story("all %d containers are there" % len(names))

def create_cnt(name, spec, rootfs):
    """Create one container, and check the server took it.

    A topology of fifty containers is fifty commands sent one after another
    as fast as the client can send them, and the server can refuse one: a
    name it is still disposing of from a previous run, a moment of pressure,
    anything. Sent without looking at the answer, that refusal is invisible
    -- the demo carries on, configures the ones that exist, and the topology
    comes up with a hole in it that nobody notices until they count the
    circles on the canvas.

    So: the return code is looked at, and a refusal is retried a few times
    before it is called fatal.
    """
    last = ""
    for attempt in range(6):
        r = cli("add", "cnt", name, spec, rootfs)
        if r.returncode == 0:
            if attempt:
                story("  %s created on attempt %d" % (name, attempt + 1))
            return
        last = (r.stdout + r.stderr).strip()
        time.sleep(1)
    raise SystemExit("%s was never created. The server said: %s"
                     % (name, last or "nothing at all"))

def ssh(node, cmd):
    show('cloonix cli %s ssh %s "%s"' % (NET, node, cmd))
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node, cmd],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if r.returncode:
        raise RuntimeError("%s: %s -> %s" %
                           (node, cmd, (r.stderr or r.stdout).strip()))
    return r.stdout

def choose_rootfs():
    """Create a throwaway container to find out whether the image is there:
    the client cannot list bulk, and the server refuses an image it has not
    got, so asking is the only way to find out."""
    for cand in ROOTFS_CHOICES:
        if cli("add", "cnt", "probe_rootfs", "eth=1", cand, ack=240).returncode == 0:
            cli("del", "probe_rootfs")
            return cand
    unavailable("%s is not in /var/lib/cloonix/bulk\n"
             "  build it:   cd vm_create/cnt_kvm_demo && ./cnt_frr_trixie.sh\n"
             "  or fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_demo_bulk/"
             "frr_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)"
             % (" or ".join(ROOTFS_CHOICES),))

def wait_ready(node, timeout=90):
    """The agent answers only once the container is up; retry until it does
    rather than sleeping for a guessed length of time."""
    end = time.time() + timeout
    while time.time() < end:
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node, "echo ready"],
                           capture_output=True, text=True,
                           stdin=subprocess.DEVNULL)
        if r.returncode == 0:
            return
        time.sleep(1)
    raise RuntimeError("%s never answered" % node)

def peers_of(name):
    """(interface, my ip, peer ip, peer asn) for every direct neighbour.

    Derived, not tabulated: the members of a lan all sit on the same /24, so
    a router's neighbours are the other members of the lans it is on.
    """
    out = []
    for lan in LANS_OF[name]:
        my_if = [i for n, i, l in LANS if n == name and l == lan][0]
        net = NODES[name][3][int(my_if)]
        for other, _, l in LANS:
            if l != lan or other == name:
                continue
            out.append((int(my_if), "%d.0.0.%d" % (net, NODES[name][1]),
                        "%d.0.0.%d" % (net, NODES[other][1]),
                        ASN_BASE + NODES[other][2]))
    return out

def bgp_conf(name):
    """The whole frr.conf for one router, written in one go rather than
    patched line by line: bgp needs a neighbour statement per peer, and
    appending them to a shipped template would be less legible, not more."""
    neth, nb, num, nets = NODES[name]
    asn = ASN_BASE + num
    lines = ["frr defaults traditional",
             "log syslog informational",
             "router bgp %d" % asn,
             " bgp router-id %d.1.1.1" % num,
             " no bgp ebgp-requires-policy"]
    for _if, _mine, peer_ip, peer_asn in peers_of(name):
        lines.append(" neighbor %s remote-as %d" % (peer_ip, peer_asn))
    lines.append(" address-family ipv4 unicast")
    for net in nets:
        lines.append("  network %d.0.0.0/24" % net)
    for _if, _mine, peer_ip, _asn in peers_of(name):
        lines.append("  neighbor %s activate" % peer_ip)
    lines.append(" exit-address-family")
    return "\n".join(lines)

def configure(name):
    neth, nb, num, nets = NODES[name]
    wait_ready(name)
    ssh(name, "sysctl -w net.ipv4.ip_forward=1")
    for i, net in enumerate(nets):
        ssh(name, "ip addr add %d.0.0.%d/24 dev eth%d" % (net, nb, i))
        ssh(name, "ip link set dev eth%d up" % i)
    frr_setup(name, "bgpd=yes\\nzebra=yes\\nvtysh_enable=yes\\n", bgp_conf(name), "bgpd")

def main():
    story(HISTORY)
    story("1. the image, an empty canvas, and the drawing")
    story("   The net must hold the image frr_trixie. It is erased, then the")
    story("   position of every router and lan is sent before they exist, so")
    story("   the canvas draws the figure of the documentation and not a spiral.")
    rootfs = choose_rootfs()
    erase_net()
    place()
    story("rootfs %s, %d routers, %d cables" % (rootfs, len(NODES), len(LANS)))

    story("2. creating the %d routers" % len(NODES))
    story("   One container each, as many vhost interfaces as it has cables.")
    for name, (neth, _nb, _num, _nets) in NODES.items():
        create_cnt(name, "eth=%d" % neth, rootfs)
    story("3. checking that the server holds every one of them")
    story("   A create that was refused shows here, by name, and not two")
    story("   minutes later as a router that never answers.")
    verify_created(list(NODES))
    story("4. cabling: %d lans" % len(LANS))
    story("   A lan exists from its first cable on; every lan here is a /24,")
    story("   and sharing one is what makes two routers bgp neighbours.")
    for node, num, lan in LANS:
        cli("plug", node, "eth%d" % num, lan)

    story("5. waiting for the first router, and for frr inside it")
    story("   One container answering ssh with vtysh in its PATH says the image")
    story("   is the right one; the others are the same image.")
    first = list(NODES)[0]
    wait_ready(first)
    frr_present(first)

    story("6. configuring them all, several at a time")
    story("   Each router gets its addresses with ip, then a frr.conf written")
    story("   into it and the daemons started: zebra and bgpd, an AS of its own,")
    story("   one neighbor statement per router sharing a /24 with it.")
    done = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=16) as pool:
        futures = {pool.submit(configure, n): n for n in NODES}
        for fut in concurrent.futures.as_completed(futures):
            try:
                fut.result()
            except Exception as err:
                sys.exit("FAILED %s" % err)
            done += 1
            story("configured %d/%d" % (done, len(NODES)))

    story("7. what to look at")
    story("   All %d routers are up. Each session goes Connect, OpenSent," % len(NODES))
    story("   Established; the summary shows a prefix count once it is, and")
    story("   the AS path of a far route names every router crossed:")
    story('  cloonix cli %s ssh tod1 vtysh -c "show bgp summary"' % NET)
    story('  cloonix cli %s ssh tod1 vtysh -c "show ip route bgp"' % NET)

main()
