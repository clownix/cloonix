#!/usr/bin/env python3
"""frrmix - ospf inside, bgp between: the way real networks are put together.

Four autonomous systems in a ring. Inside each one, four routers running
ospf in area 0. Between them, ebgp on the border routers. And inside each
one, ibgp between the two borders so that what one learns from outside is
known to the other.

The point of the arrangement, and the reason it is worth having as a demo:

  * ospf carries only what belongs to the operator -- the internal links and
    the loopbacks. It never sees an external route.
  * bgp carries the external routes, and its sessions are between loopbacks,
    not interfaces. A loopback does not go down when a cable does, so an
    ibgp session survives any failure ospf can route around.
  * that only works because ospf advertises the loopbacks, which is what
    makes the two protocols depend on each other rather than merely coexist.
  * the border routers set next-hop-self on the ibgp sessions: an internal
    router has no route to a peer in a foreign AS, so the next hop has to be
    rewritten to something ospf knows.

    ./frrmix.py [net]              the net defaults to nemo

Needs cloonix on the PATH, nothing else.

Topology, with n the AS number from 1 to 4:

    b<n>1 ---- lan a ---- i<n>1 ---- lan b ---- i<n>2 ---- lan c ---- b<n>2
      |                                                                |
      +------------------------ lan d ---------------------------------+
      |                                                                |
    ebgp to AS n-1                                        ebgp to AS n+1

Image: frr_trixie
Size:  60 containers and 45 lans (spider) / 16 containers (mix)

THE PACKET TO CATCH
-------------------
  Sixty routers make a lot of traffic and the filter is what keeps a
  capture readable.

  ospf, which is IP protocol 89 and so is reached by exclusion:

    cloonix cli nemo cnf cod1 eth0 sniff=on
    cloonix cli nemo sniff cod1 eth0 --dump ip and not udp and not tcp

  The packet to catch is one LSA flooding outwards after a link is taken
  down: the same LSA arrives on several interfaces of the same router,
  which is the mechanism people describe as "flooding" and rarely see.

  bgp, in frrspider_bgp.py and frrmix.py, is tcp 179:

    cloonix cli nemo sniff b11 eth0 --dump tcp port 179

  In frrmix.py the packet to catch is on the ibgp session, because of
  where it is addressed: between two loopbacks, over a wire whose own
  addresses appear nowhere in it.

    cloonix cli nemo sniff i11 eth0 --dump host 10.1.0.1

  That is the demo's whole argument in one filter -- the session
  survives a link failure because it was never bound to the link, and
  the capture shows the packets riding whichever wire ospf chose.

Important
  No configuration is baked into the image: frr is installed and everything
  else is written at run time. The cloonix rootfs itself carries no routing
  daemon -- frr is one piece of software to test with cloonix among many.
  The bird and babel demos are the deliberate counterweight.
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
    CLOONIX, NET, erase_net, show, story)

NB_AS = 4
ASN_BASE = 65000
# frr_trixie only: the cloonix rootfs is kept minimal and carries no routing
# daemons, so self_rootfs cannot run this demo.
ROOTFS_CHOICES = ("frr_trixie",)

HISTORY = """\
Two protocols with two authors. OSPF, John Moy, RFC 1131 in 1989: an
interior protocol where every router holds the map of its own network.
BGP, Kirk Lougheed and Yakov Rekhter, RFC 1105 the same year: an
exterior protocol where an autonomous system tells the next what it can
reach and nothing more. The split is the architecture of the internet
since the early 1990s. FRRouting runs both in one process family,
descended from GNU Zebra begun by Kunihiro Ishiguro in 1996, Quagga
from 2002, forked as FRR in 2017 under the Linux Foundation.

Today: the pair shown here is the usual shape of a real network: an interior
protocol inside each site, BGP between sites and toward providers, and
redistribution at the border.
"""

def borders(n):
    return ["b%d1" % n, "b%d2" % n]

def internals(n):
    return ["i%d1" % n, "i%d2" % n]

def routers(n):
    return borders(n) + internals(n)

ALL = [r for n in range(1, NB_AS + 1) for r in routers(n)]

def asn(name):
    return ASN_BASE + int(name[1])

def loopback(name):
    """b<n>1 -> 10.<n>.0.1, b<n>2 -> .2, i<n>1 -> .11, i<n>2 -> .12

    Deliberately inside the 10.<n>.0.0/16 the AS announces. Put the loopbacks
    outside it and they are unreachable from any other AS, which makes the
    obvious test -- ping the far end -- fail on a network that is otherwise
    working."""
    n, idx = int(name[1]), int(name[2])
    return "10.%d.0.%d" % (n, idx if name[0] == "b" else 10 + idx)

LINKS = {}
for n in range(1, NB_AS + 1):
    b1, b2 = borders(n)
    i1, i2 = internals(n)
    for host, iface, net, addr, lan in (
            (b1, 0, 1, 1, "lan%da" % n), (i1, 0, 1, 2, "lan%da" % n),
            (i1, 1, 2, 1, "lan%db" % n), (i2, 0, 2, 2, "lan%db" % n),
            (i2, 1, 3, 1, "lan%dc" % n), (b2, 0, 3, 2, "lan%dc" % n),
            (b1, 1, 4, 1, "lan%dd" % n), (b2, 1, 4, 2, "lan%dd" % n)):
        LINKS.setdefault(host, []).append(
            (iface, "10.%d.%d.%d" % (n, net, addr), 24, lan))
# The link between two AS is addressed out of the block of the one on the
# left, and not from a range of its own.
for n in range(1, NB_AS + 1):
    nxt = n % NB_AS + 1
    lan = "lanas%d" % n
    LINKS.setdefault("b%d2" % n, []).append((2, "10.%d.9.1" % n, 30, lan))
    LINKS.setdefault("b%d1" % nxt, []).append((2, "10.%d.9.2" % n, 30, lan))

def xy_of(name):
    n, idx = int(name[1]), int(name[2])
    centre = 360.0 * (n - 1) / NB_AS
    spread = 16 if name[0] == "b" else 14
    a = math.radians(centre + (spread if idx == 2 else -spread))
    r = 420 if name[0] == "b" else 250
    return int(r * math.sin(a)), int(-r * math.cos(a))

def place():
    pos = {n: xy_of(n) for n in ALL}
    lans = {}
    for node in ALL:
        for iface, _addr, _plen, lan in LINKS[node]:
            lans.setdefault(lan, []).append(node)
    for lan, members in lans.items():
        pos[lan] = (sum(pos[m][0] for m in members) // len(members),
                    sum(pos[m][1] for m in members) // len(members))
    for name, (x, y) in pos.items():
        cli("cnf", name, "x=%s" % x, "y=%s" % y, ack=0.3, quiet=True)
    for node in ALL:
        for iface, _addr, _plen, lan in LINKS[node]:
            dx, dy = pos[lan][0] - pos[node][0], pos[lan][1] - pos[node][1]
            deg = int(math.degrees(math.atan2(dx, -dy))) % 360
            cli("cnf", node, "eth%d" % iface, "angle=%d" % deg, ack=0.3,
                quiet=True)

def ebgp_peers(name):
    """The router on the other end of an inter-AS link, if this is a border."""
    out = []
    for iface, addr, plen, lan in LINKS[name]:
        if not lan.startswith("lanas"):
            continue
        for other in ALL:
            for _i, other_addr, _p, other_lan in LINKS[other]:
                if other_lan == lan and other != name:
                    out.append((other_addr, asn(other)))
    return out

def ibgp_peers(name):
    """The other border of the same AS. Peered on loopbacks, so the session
    does not follow the fate of any one link."""
    n = int(name[1])
    if name not in borders(n):
        return []
    return [(loopback(o), asn(o)) for o in borders(n) if o != name]

def frr_conf(name):
    n = int(name[1])
    is_border = name in borders(n)
    lo = loopback(name)
    c = ["frr defaults traditional",
         "log syslog informational",
         "interface lo",
         " ip address %s/32" % lo,
         "!",
         "router ospf",
         " ospf router-id %s" % lo,
         # the loopback goes into ospf: without it the ibgp session between
         # the two borders has no path and never comes up
         " network %s/32 area 0.0.0.0" % lo]
    for _i, addr, plen, lan in LINKS[name]:
        if not lan.startswith("lanas"):        # internal links only
            c.append(" network %s/%d area 0.0.0.0"
                     % (addr.rsplit(".", 1)[0] + ".0", plen))
    if is_border:
        c.append(" default-information originate always")
    else:
        return "\n".join(c)

    # bgp "network" advertises only a prefix present in the table: a blackhole /16 puts it there.
    c += ["!",
          "ip route 10.%d.0.0/16 blackhole" % n,
          "!",
          "router bgp %d" % asn(name),
          " bgp router-id %s" % lo,
          # frr drops everything on an ebgp session that has no policy
          " no bgp ebgp-requires-policy"]
    for addr, peer_asn in ebgp_peers(name):
        c.append(" neighbor %s remote-as %d" % (addr, peer_asn))
    for addr, peer_asn in ibgp_peers(name):
        c.append(" neighbor %s remote-as %d" % (addr, peer_asn))
        c.append(" neighbor %s update-source lo" % addr)
    c.append(" address-family ipv4 unicast")
    # what this AS announces to the world: its own internal space
    c.append("  network 10.%d.0.0/16" % n)
    for addr, _a in ebgp_peers(name):
        c.append("  neighbor %s activate" % addr)
    for addr, _a in ibgp_peers(name):
        c.append("  neighbor %s activate" % addr)
        # an internal router cannot reach a peer in another AS, so the next
        # hop must be rewritten to something ospf knows about
        c.append("  neighbor %s next-hop-self" % addr)
    c.append(" exit-address-family")
    return "\n".join(c)

# Where the debian package puts it.
FRRINIT = "/usr/lib/frr/frrinit.sh"

VTYSH_CONF = "service integrated-vtysh-config"

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
              "echo '%s did not start, see /var/log/frr'; exit 1"
              % (expect, expect))

def frr_present(node):
    """Fail early and clearly rather than at the first missing file."""
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node, "test -x " + FRRINIT],
                       capture_output=True, stdin=subprocess.DEVNULL)
    if r.returncode:
        sys.exit("%s has no %s: the image carries no frr.\n"
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
    """Count what the server actually has, and name anything missing.

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

    A topology of many containers is many commands sent as fast as the
    client can send them, and the server can refuse one. Sent without
    looking at the answer, that refusal is invisible -- the demo carries on
    and the topology comes up with a hole in it that nobody notices until
    they count the circles on the canvas.
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
        raise RuntimeError("%s: %s -> %s"
                           % (node, cmd, (r.stderr or r.stdout).strip()))
    return r.stdout

def choose_rootfs():
    for cand in ROOTFS_CHOICES:
        if cli("add", "cnt", "probe_rootfs", "eth=1", cand, ack=240).returncode == 0:
            cli("del", "probe_rootfs")
            return cand
    sys.exit("%s is not in /var/lib/cloonix/bulk\n"
             "  build it:   cd vm_create/cnt_kvm_demo && ./cnt_frr_trixie.sh\n"
             "  or fetch it:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_demo_bulk/"
             "frr_trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)"
             % (" or ".join(ROOTFS_CHOICES),))

def wait_ready(node, timeout=90):
    end = time.time() + timeout
    while time.time() < end:
        if subprocess.run(CLOONIX + ["cli", NET, "ssh", node, "echo ready"],
                          capture_output=True,
                          stdin=subprocess.DEVNULL).returncode == 0:
            return
        time.sleep(1)
    raise RuntimeError("%s never answered" % node)

def configure(name):
    wait_ready(name)
    ssh(name, "sysctl -w net.ipv4.ip_forward=1")
    ssh(name, "ip link set dev lo up")
    for iface, addr, plen, _lan in LINKS[name]:
        ssh(name, "ip addr add %s/%d dev eth%d" % (addr, plen, iface))
        ssh(name, "ip link set dev eth%d up" % iface)
    frr_setup(name, "zebra=yes\\nospfd=yes\\nbgpd=yes\\nvtysh_enable=yes\\n", frr_conf(name), "ospfd")

def main():
    story(HISTORY)
    story("1. the image, an empty canvas, and the drawing")
    story("   The net must hold the image frr_trixie. It is erased, then the")
    story("   position of every router and lan is sent before they exist, so")
    story("   the canvas draws the figure of the documentation and not a spiral.")
    rootfs = choose_rootfs()
    erase_net()
    place()
    story("rootfs %s, %d AS of 4 routers" % (rootfs, NB_AS))

    story("2. creating the %d routers" % len(ALL))
    story("   Four AS of four: two borders b<as>1 b<as>2, two interiors i<as>1")
    story("   i<as>2. One container each, a vhost interface per cable.")
    for name in ALL:
        create_cnt(name, "eth=%d" % len(LINKS[name]), rootfs)
    story("3. checking that the server holds every one of them")
    story("   A create that was refused shows here, by name, and not two")
    story("   minutes later as a router that never answers.")
    verify_created(list(ALL))
    story("4. cabling: inside each AS, and between the borders of two AS")
    for name in ALL:
        for iface, _addr, _plen, lan in LINKS[name]:
            cli("plug", name, "eth%d" % iface, lan)

    story("5. waiting for the first router, and for frr inside it")
    story("   One container answering ssh with vtysh in its PATH says the image")
    story("   is the right one; the others are the same image.")
    first = ALL[0]
    wait_ready(first)
    frr_present(first)

    story("6. configuring them all, several at a time")
    story("   Each router gets its addresses with ip, then a frr.conf written")
    story("   into it and the daemons started: ospf inside the AS for all four,")
    story("   bgp on the two borders only, which also originate a default route")
    story("   into ospf -- that is how a router with no bgp leaves its AS.")
    done = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        futures = {pool.submit(configure, n): n for n in ALL}
        for fut in concurrent.futures.as_completed(futures):
            try:
                fut.result()
            except Exception as err:
                sys.exit("FAILED %s" % err)
            done += 1
            story("configured %d/%d" % (done, len(ALL)))

    story("7. what to look at")
    story("   %d routers up. Give bgp a minute to settle, then:" % len(ALL))
    story("  # the borders should have their sessions established")
    story('  cloonix cli %s ssh b11 vtysh -c "show bgp summary"' % NET)
    story("  # and carry the other three AS")
    story('  cloonix cli %s ssh b11 vtysh -c "show ip route bgp"' % NET)
    story("  # ospf inside AS 1, complete, with a default from each border")
    story('  cloonix cli %s ssh i11 vtysh -c "show ip route ospf"' % NET)
    story("  # the whole point: a router that runs no bgp, in AS 4,")
    story("  # reaching a loopback in AS 1")
    story('  cloonix cli %s ssh i41 ping -c 5 10.1.0.1' % NET)

main()
