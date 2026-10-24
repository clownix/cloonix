#!/usr/bin/env python3
r"""ebpf - a program loaded into the kernel to drop packets at the wire.

    ./ebpf.py [net]                the net defaults to nemo

Two machines on one lan::

  h1 --- lan1 --- h2

  h2 carries a tiny eBPF program of its own, compiled from C on the machine
  itself and attached to its network card with XDP -- the hook that runs on a
  packet the instant it arrives, before the kernel's own stack sees it. The
  program reads far enough into each frame to find IP and its protocol, and
  drops ICMP; everything else passes.

  ACT 1  no program is loaded. h1 pings h2 and it answers, the baseline.
  ACT 2  the C is compiled to a bpf object and attached to h2's card. h1
         pings again: the echo requests still arrive on the wire but are
         dropped by the program before the stack, so nothing comes back.
  ACT 3  the program is detached. h1 pings once more and h2 answers again --
         the drop was the program's doing and nothing else's.

Image: ebpf_trixie (clang, llvm, libbpf, bpftool)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
  The point is that the packet reaches the wire and is dropped past it. Watch
  the echo requests arrive while no replies come back:

    cloonix cli nemo cnf h2 eth0 sniff=on
    cloonix cli nemo sniff h2 eth0 --dump icmp

Important
  Attaching an XDP program loads it into the kernel, and the kernel checks
  that permission against the initial user namespace -- which a container run
  as your normal user never has. Unlike a kernel module, this cannot be
  prepared on the host: no host command grants it. This one demo therefore
  needs the net run as root. Nothing is baked into the image: the C is
  written, compiled and loaded at run time.
"""
import os
import sys

HISTORY = """\
eBPF grew out of the Berkeley Packet Filter that tcpdump has used since the
1990s. From 2014 Alexei Starovoitov and Daniel Borkmann turned it into a
general way to run safe, verified programs inside the kernel. XDP, the
eXpress Data Path, is the hook that runs on a raw frame at the driver,
before any allocation: the earliest point of the kernel where a packet can
be dropped or redirected.
"""

import base64

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    NET, build, check_net, choose_rootfs, count_on, cut_off, erase_net, hold,
    place, reached, ssh, step, story, tools_present, unavailable)

ROOTFS_CHOICES = ("ebpf_trixie",)
# XDP loads are checked against the initial user namespace
NEEDS_ROOT = True

H1, H2 = "h1", "h2"
ALL = (H1, H2)
LAN = "lan1"

WIRING = {
    H1: [(0, LAN, "10.0.0.1/24")],
    H2: [(0, LAN, "10.0.0.2/24")],
}
GATEWAY = {}

BPF_C = """\
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

SEC("xdp")
int drop_icmp(struct xdp_md *ctx)
{
    void *data = (void *)(long)ctx->data;
    void *data_end = (void *)(long)ctx->data_end;
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return XDP_PASS;
    if (eth->h_proto != bpf_htons(ETH_P_IP))
        return XDP_PASS;
    struct iphdr *ip = (void *)(eth + 1);
    if ((void *)(ip + 1) > data_end)
        return XDP_PASS;
    if (ip->protocol == IPPROTO_ICMP)
        return XDP_DROP;
    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
"""

def ship_source():
    ssh(H2, "mkdir -p /root/bpf")
    b64 = base64.b64encode(BPF_C.encode()).decode()
    # base64 has no quote or percent, so it goes through ssh untouched and
    # sidesteps printf entirely -- the C keeps its % and quotes intact.
    ssh(H2, "echo %s | base64 -d > /root/bpf/drop.c" % b64)

def compile_obj():
    arch_inc = "/usr/include/x86_64-linux-gnu"
    r = ssh(H2, "clang -O2 -g -target bpf -c /root/bpf/drop.c "
                "-o /root/bpf/drop.o -I%s 2>&1; "
                "test -f /root/bpf/drop.o && echo BUILT" % arch_inc,
            check=False)
    if "BUILT" not in r:
        sys.exit("the bpf object did not compile:\n%s" % r.strip())

def attach():
    r = ssh(H2, "ip link set dev eth0 xdpgeneric obj /root/bpf/drop.o "
                "sec xdp 2>&1; ip -d link show dev eth0 | grep -q xdp "
                "&& echo ATTACHED || echo NO", check=False)
    if "ATTACHED" not in r:
        unavailable("the XDP program would not attach.\n"
                 "  loading an XDP program is checked against the initial user\n"
                 "  namespace, which a container run as your normal user does\n"
                 "  not have, and no host command grants it. This one demo\n"
                 "  needs the net run as root.\n"
                 "  the kernel said: %s" % r.strip())

def detach():
    ssh(H2, "ip link set dev eth0 xdpgeneric off", check=False)

def ping(count=4):
    return ssh(H1, "ping -c %d -W 1 10.0.0.2" % count, check=False)

LAYOUT = {H1: (-260, 0), LAN: (0, 0), H2: (260, 0)}
ETHS = {(H1, "eth0"): 0, (H2, "eth0"): 180}

CARD = r"""
   h1 --- lan1 --- h2   (XDP program on h2 eth0)

  h2 compiles a small eBPF program and attaches it to its card. It drops
  ICMP at the wire, before the kernel stack -- so h1's ping goes unanswered.
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
    tools_present(H2, "clang", "bpftool")

    step("act 1: no program loaded, the ping is answered")
    story()
    r = ping()
    story("    h1 -> h2 with nothing loaded:")
    story(r.strip())
    ok_before = reached(r)
    hold()

    step("act 2: compile, attach, and watch ICMP arrive but die")
    story()
    ship_source()
    compile_obj()
    story("    h2's loaded bpf programs:")
    for line in ssh(H2, "bpftool prog show 2>/dev/null | grep -A1 xdp "
                        "| head -n 4", check=False).strip().split("\n"):
        if line:
            story("      %s" % line)
    got = count_on(H2, 0, "icmp",
                   lambda: (attach(), ping())[1], settle=3.0)
    if got is not None:
        story("    ICMP still reaching h2's wire while dropped: %d packet(s)"
              % got["count"])
    lost = ping()
    story("    h1 -> h2 with the program attached:")
    story(lost.strip())
    ok_dropped = cut_off(lost)
    story()
    story("    The echo requests arrive -- the sniff on the wire proves it --")
    story("    but the program drops them before the stack, so none is")
    story("    answered.")
    story()

    step("act 3: detach, and h2 answers again")
    story()
    detach()
    r = ping()
    story("    h1 -> h2 with the program removed:")
    story(r.strip())
    ok_after = reached(r)
    story()

    if not (ok_before and ok_dropped and ok_after):
        sys.exit("a check failed: before=%s dropped=%s after=%s"
                 % (ok_before, ok_dropped, ok_after))

    story("""
the network is left up. Things to try:

  the loaded program, and attach or detach it by hand:
    cloonix cli %s ssh h2 "bpftool prog show"
    cloonix cli %s ssh h2 "ip link set dev eth0 xdpgeneric off"

  watch ICMP arrive at the wire while it is dropped:
    cloonix cli %s sniff h2 eth0 --dump icmp
""" % (NET, NET, NET))

main()
