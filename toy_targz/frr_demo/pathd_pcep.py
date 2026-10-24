#!/usr/bin/env python3
r"""pathd_pcep - SR-TE driven by a controller, the Pola PCE, over PCEP.

Needs: mpls_router, mpls_iptunnel.

    ./pathd_pcep.py [net]     the net defaults to nemo

The FRR User Guide, page pathd, section "PCEP Support": pathd started with
the module pathd_pcep is a PCC, a client of a PCE, the controller. The PCE
here is Pola (NTT Communications, MIT licence), whose SR-MPLS example with
frr this demo follows: the policy is initiated by the PCE, r1 holds none.
https://docs.frrouting.org/en/latest/pathd.html#pcep-support
https://github.com/nttcom/pola

                     pce (10.0.255.254, polad)
                      |
              r2      |  PCEP, tcp 4189
            /    \    |
    h1 -- r1      r4 -- h4
            \    /
             r5 - r3

  !!! KERNEL MODULES NEEDED ON THE HOST: mpls_router and mpls_iptunnel !!!
  Label switching is done by the kernel, and a container cannot load a
  module. Before playing this demo, on the host, once per boot:

sudo modprobe mpls_router
sudo modprobe mpls_iptunnel

  or do not run this demo. It stops with a big warning if a module is
  missing.

  The network of pathd_explicit.py: IS-IS SR in the core, rN has the SID
  1600N, iBGP r1-r4, r1 colors the routes of r4 with the color 1. What
  changes is where the policy comes from.

  ACT 1  the PCEP session r1-pce comes up; no policy: h1 reaches h4 over
         r2.
  ACT 2  on the pce, "pola sr-policy add": the PCE sends a PCInitiate,
         r1 creates the policy of color 1 to r4 with the segment list
         16003 16004; the traffic goes r1 r5 r3 r4.

Image: frr_trixie (zebra, isisd, bgpd, pathd, and polad, pola)
Size:  8 containers, 8 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf r1 eth3 sniff=on
    cloonix cli nemo sniff r1 eth3 --dump "tcp port 4189"

  In ACT 2, the PCInitiate of the PCE: an SRP, an LSP, and an ERO holding
  the two SIDs, the path the controller chose.

Important
  polad and pola 1.3.0 and their configuration live in the pce container,
  /etc/pola. The policy is sent with --no-link-state: no topology
  database, pola is told the labels, as in its example. Pola 1.3.2 and
  1.3.3 cannot do that (their client drops what the server needs).
  No removal by the PCE: pola finds a policy by its color, and pathd
  reports the color 0 back. The name of the policy stays short: frr 10.2.1
  writes one byte past the copy of the name it receives (fixed later).
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, frrlib.py on top.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, frr_setup, hold, in_parallel, reached, \
    ssh, step, story
from frrlib import forwarding, has, mpls_on, opening, ping, proof, until, \
    verdict, vtysh
from toylib import put_any

HISTORY = """\
The path computation element, a server that computes the paths the
routers then follow, is RFC 4655 of 2006, and PCEP, the protocol between
them, RFC 5440 of 2009 by JP Vasseur and Jean-Louis Le Roux. The stateful
PCE of RFC 8231 learns the paths in use; RFC 8281 of 2017 lets it create
them, and RFC 8664 of 2019 made the paths segment lists. That is the
software defined network of the carriers: the controller decides, the
routers forward. Pola, written in Go at NTT Communications, was released
in 2022; the PCEP client of pathd came from NetDEF and Volta Networks in
2020.

Today: operators drive their SR policies from controllers, Cisco
Crosswork, Juniper Paragon, Nokia NSP, or open source ones such as Pola.
"""

ROOTFS_CHOICES = ("frr_trixie",)
TOOL = "vtysh"

H1, H4, PCE = "h1", "h4", "pce"
R1, R2, R3, R4, R5 = "r1", "r2", "r3", "r4", "r5"
ROUTERS = (R1, R2, R3, R4, R5)
ALL = ROUTERS + (H1, H4, PCE)
L12, L15, L24, L35, L34 = "lan12", "lan15", "lan24", "lan35", "lan34"
LH1, LH4, LPCE = "lanh1", "lanh4", "lanpce"

WIRING = {
    R1: [(0, L12, "10.0.12.1/24"), (1, L15, "10.0.15.1/24"),
         (2, LH1, "10.10.1.1/24"), (3, LPCE, "10.0.255.1/24")],
    R2: [(0, L12, "10.0.12.2/24"), (1, L24, "10.0.24.2/24")],
    R5: [(0, L15, "10.0.15.5/24"), (1, L35, "10.0.35.5/24")],
    R3: [(0, L35, "10.0.35.3/24"), (1, L34, "10.0.34.3/24")],
    R4: [(0, L24, "10.0.24.4/24"), (1, L34, "10.0.34.4/24"),
         (2, LH4, "10.10.4.1/24")],
    H1: [(0, LH1, "10.10.1.10/24")],
    H4: [(0, LH4, "10.10.4.10/24")],
    PCE: [(0, LPCE, "10.0.255.254/24")],
}
GATEWAY = {H1: "10.10.1.1", H4: "10.10.4.1"}
NUM = {R1: 1, R2: 2, R3: 3, R4: 4, R5: 5}
CORE = {R1: (0, 1), R2: (0, 1), R3: (0, 1), R4: (0, 1), R5: (0, 1)}
PEER = {R1: R4, R4: R1}
DAEMONS = {R1: ["isisd", "bgpd", "pathd"], R4: ["isisd", "bgpd"]}

def lo(node):
    return "%d.%d.%d.%d" % ((NUM[node],) * 4)

def frr_conf(node):
    n = NUM[node]
    c = ["hostname %s" % node,
         "interface lo",
         " ip address %s/32" % lo(node),
         " ip router isis SR",
         " isis passive"]
    for num in CORE[node]:
        c += ["interface eth%d" % num,
              " ip router isis SR",
              " isis network point-to-point",
              " isis hello-interval 1"]
    c += ["router isis SR",
          " net 49.0000.0000.0000.000%d.00" % n,
          " is-type level-1",
          " lsp-gen-interval 1",
          " segment-routing on",
          " segment-routing node-msd 8",
          " segment-routing prefix %s/32 index %d" % (lo(node), n)]
    if node in PEER:
        peer = lo(PEER[node])
        c += ["router bgp 65000",
              " bgp router-id %s" % lo(node),
              " neighbor %s remote-as 65000" % peer,
              " neighbor %s update-source lo" % peer,
              " address-family ipv4 unicast",
              "  network 10.10.%d.0/24" % n]
        if node == R1:
            c += ["  neighbor %s route-map SET_SR_POLICY in" % peer]
        c += [" exit-address-family"]
    if node == R1:
        c += ["route-map SET_SR_POLICY permit 10",
              " set sr-te color 1",
              "segment-routing",
              " traffic-eng",
              "  pcep",
              "   pce POLA",
              "    address ip 10.0.255.254",
              "    source-address ip 10.0.255.1",
              "    pce-initiated",
              "   exit",
              "   pcc",
              "    peer POLA",
              "   exit",
              "  exit",
              " exit",
              "exit"]
    return "\n".join(c)

POLAD_YAML = """\
global:
  pcep:
    address: "10.0.255.254"
    port: 4189
  grpc-server:
    address: "127.0.0.1"
    port: 50051
  log:
    path: "/var/log/pola/"
    name: "polad.log"
  ted:
    enable: false
"""

POLICY_YAML = """\
srPolicy:
  pcepSessionAddr: 10.0.255.1
  srcAddr: 1.1.1.1
  dstAddr: 4.4.4.4
  name: by-pola
  color: 1
  segmentList:
    - sid: 16003
    - sid: 16004
"""

LAYOUT = {
    H1: (-560, 0), LH1: (-440, 0), R1: (-300, 0),
    L12: (-150, -110), R2: (0, -200), L24: (150, -110),
    R4: (300, 0), LH4: (440, 0), H4: (560, 0),
    L15: (-220, 130), R5: (-110, 220), L35: (0, 220), R3: (110, 220),
    L34: (220, 130), LPCE: (-300, -160), PCE: (-300, -280),
}
ETHS = {
    (H1, "eth0"): 90, (R1, "eth2"): 270, (R1, "eth0"): 45, (R1, "eth1"): 135,
    (R1, "eth3"): 0, (PCE, "eth0"): 180,
    (R2, "eth0"): 225, (R2, "eth1"): 135, (R4, "eth0"): 315,
    (R4, "eth1"): 225, (R4, "eth2"): 90, (H4, "eth0"): 270,
    (R5, "eth0"): 315, (R5, "eth1"): 90, (R3, "eth0"): 270, (R3, "eth1"): 45,
}

CARD = r"""
                 pce  (Pola, the controller)
                  |
              r2  |
            /    \|
    h1 -- r1      r4 -- h4      (IS-IS SR, iBGP r1-r4, pathd PCC on r1)
            \    /
             r5 - r3

  The pce decides the path of h4's traffic; r1 only follows.
"""

def configure(node):
    forwarding(node)
    mpls_on(node, CORE[node])
    frr_setup(node, DAEMONS.get(node, ["isisd"]), frr_conf(node),
              modules=("pathd_pcep",) if node == R1 else ())

def polad_start():
    ssh(PCE, "mkdir -p /etc/pola /var/log/pola")
    put_any(PCE, "/etc/pola/polad.yaml", POLAD_YAML)
    put_any(PCE, "/etc/pola/policy1.yaml", POLICY_YAML)
    ssh(PCE, "cd /etc/pola && polad -f polad.yaml "
             "</dev/null >/var/log/pola/polad.out 2>&1 &")

def pola(args):
    return ssh(PCE, "pola %s" % args, check=False)

def route_via(addr, labels="16004"):
    """r1 sends to h4 through addr, pushing labels (a stack shows as
    16003/16004)."""
    return lambda: has(R1, "show ip route 10.10.4.0/24",
                       [addr, "label " + labels])

def count(node, num, filt):
    got = count_on(node, num, filt, lambda: ping(H1, "10.10.4.10"),
                   settle=1.0)
    if got is None:
        return False, ping(H1, "10.10.4.10")
    story("    %d packet(s) \"%s\" on %s eth%d during the ping."
          % (got["count"], filt, node, num))
    return got["count"] > 0, got["out"]

def main():
    opening("mpls_router", "mpls_iptunnel")
    if not ssh(PCE, "command -v polad pola", check=False).strip():
        sys.exit("the pce has no polad: this frr_trixie image is older "
                 "than the Pola PCE.\n  rebuild it:\n"
                 "    cd vm_create/cnt_kvm_demo && ./cnt_frr_trixie.sh")

    step("act 1: the PCEP session comes up, no policy yet")
    polad_start()
    in_parallel(configure, list(ROUTERS), "frr start")
    ok_session = until(lambda: has(R1, "show sr-te pcep session",
                                   ["Session Status UP"]), 90)
    proof("r1, its PCEP session:", vtysh(R1, "show sr-te pcep session"))
    proof("pce, its sessions:", pola("session"))
    ok_bgp = until(lambda: has(R1, "show ip route 10.10.4.0/24", ["bgp"]),
                   120)
    ok_short = until(route_via("10.0.12.2"), 60)
    proof("r1, its route to h4:", vtysh(R1, "show ip route 10.10.4.0/24"))
    ok_r2, out = count(R2, 0, "mpls 16004")
    ok_ping1 = reached(out)
    hold()

    step("act 2: the PCE initiates the policy, 16003 16004")
    said = pola("sr-policy add -f /etc/pola/policy1.yaml --no-link-state")
    proof("pce, pola sr-policy add:", said)
    ok_added = "success" in said
    ok_active = until(lambda: has(R1, "show sr-te policy detail",
                                  ["Active", "PCEP"]), 30)
    proof("r1, the policy the PCE created:",
          vtysh(R1, "show sr-te policy detail"))
    proof("pce, the policies it holds:", pola("sr-policy list"))
    ok_long = until(route_via("10.0.15.5", "16003/16004"), 30)
    proof("r1, its route to h4, through the policy:",
          vtysh(R1, "show ip route 10.10.4.0/24"))
    ok_stack, out = count(R5, 0, "mpls 16003 and mpls 16004")
    proof("h1 -> h4, the way the PCE chose:", out)
    ok_ping2 = reached(out)

    verdict([("the PCEP session r1-pce is UP", ok_session),
             ("iBGP gives r1 the network of h4", ok_bgp),
             ("without a policy, the route goes over r2", ok_short),
             ("16004 crosses r1-r2", ok_r2),
             ("h1 reaches h4", ok_ping1),
             ("pola accepts the policy", ok_added),
             ("r1 holds an Active policy, origin PCEP", ok_active),
             ("the route of h4 now leaves r1 towards r5", ok_long),
             ("two labels, 16003 above 16004, on r1-r5", ok_stack),
             ("h1 reaches h4 the way of the PCE", ok_ping2)])

    story("""
the network is left up, with the policy of the PCE. Things to try:

  the policies the controller holds:
    cloonix cli %s ssh pce "pola sr-policy list"

  the PCEP messages counted by r1:
    cloonix cli %s ssh r1 "vtysh -c 'show sr-te pcep counters'"

  the PCEP exchange on the wire:
    cloonix cli %s sniff r1 eth3 --dump "tcp port 4189"
""" % (NET, NET, NET))

main()
