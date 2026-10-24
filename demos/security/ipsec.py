#!/usr/bin/env python3
r"""ipsec - an encrypted tunnel between two different implementations.

    ./ipsec.py [net]               the net defaults to nemo

Two security gateways on one lan, each a different program::

  gwA --- lan1 --- gwB
  strongswan      libreswan
  10.0.0.1        10.0.0.2

  Both speak IKEv2 with a shared secret and set up an ESP transport-mode
  association for the traffic between their two addresses. gwA runs
  strongSwan and is driven by swanctl; gwB runs Libreswan and is driven by
  ipsec.conf. Their interoperating is the subject: IPsec is a
  standard, and two independent stacks meet on it.

  ACT 1  both come up and the tunnel is with no protection yet: a ping from
         gwA to gwB is ordinary, readable ICMP on the wire.
  ACT 2  the IKE negotiation completes and the ESP association is installed.
         The same ping now travels as ESP -- IP protocol 50 -- and the ICMP
         inside it is no longer visible in clear.
  ACT 3  the two counts are shown side by side: ESP packets present, cleartext
         ICMP gone, which is the encryption made visible.

Image: strongswan_trixie (gwA), libreswan_trixie (gwB)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
  Encrypted traffic is ESP, IP protocol 50; cleartext ping is ICMP:

    cloonix cli nemo cnf gwA eth0 sniff=on
    cloonix cli nemo sniff gwA eth0 --dump "ip and not tcp and not udp and not icmp"
    cloonix cli nemo sniff gwA eth0 --dump icmp

Important
  IPsec lives in the kernel (XFRM); nothing is written to a disk image. Both
  configurations and the shared secret are written at run time. This demo
  needs two images, one per gateway.
"""
import os
import sys
import time

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    NET, address, cable_all, check_net, count_on, erase_net, eth_spec, hold,
    in_parallel, place, put, run, ssh, step, story, tools_present,
    wait_answer, wait_exists, unavailable)

HISTORY = """\
IPsec is the IETF standard for encryption at the IP layer, RFC 4301 with the
IKEv2 key exchange in RFC 7296. Two long-lived open stacks implement it on
linux: strongSwan, led by Andreas Steffen, and Libreswan, led by Paul
Wouters and shipped by Red Hat. They are separate code bases that must agree
on the wire: pairing them tests the standard, not one code base.

Today: IPsec carries most site-to-site VPNs between companies and cloud
providers, the voice of 4G and 5G phones toward their operator (VoWiFi), and
the IKEv2 client built into windows, macos, ios and android.
"""

ROOTFS_CHOICES = ("strongswan_trixie",)
LIBRESWAN_ROOTFS = "libreswan_trixie"

GWA, GWB = "gwA", "gwB"
ALL = (GWA, GWB)
LAN = "lan1"
PSK = "clownixSharedSecret2024"

WIRING = {
    GWA: [(0, LAN, "10.0.0.1/24")],
    GWB: [(0, LAN, "10.0.0.2/24")],
}
GATEWAY = {}

SWANCTL_CONF = """\
connections {
    link {
        version = 2
        local_addrs = 10.0.0.1
        remote_addrs = 10.0.0.2
        proposals = aes256-sha256-modp2048
        local { auth = psk }
        remote { auth = psk }
        children {
            icmp {
                mode = transport
                local_ts = 10.0.0.1/32
                remote_ts = 10.0.0.2/32
                esp_proposals = aes256-sha256
            }
        }
    }
}
secrets {
    ike-psk {
        id-a = 10.0.0.1
        id-b = 10.0.0.2
        secret = PSKVALUE
    }
}
"""

IPSEC_CONF = """\
config setup
    logfile=/tmp/pluto.log
conn link
    keyexchange=ikev2
    authby=secret
    type=transport
    left=10.0.0.2
    right=10.0.0.1
    ike=aes256-sha256-modp2048
    esp=aes256-sha256
    auto=add
"""

IPSEC_SECRETS = """\
10.0.0.2 10.0.0.1 : PSK "PSKVALUE"
"""

def make(node, rootfs):
    last = ""
    for _ in range(6):
        r = run("cloonix cli %s add cnt %s %s %s"
                % (NET, node, eth_spec(node), rootfs))
        last = (r.stdout + r.stderr).strip()
        if r.returncode == 0 and wait_exists(node):
            return
        time.sleep(2)
    unavailable("%s (%s) was not created. The server said: %s\n"
             "  fetch it:  cd /var/lib/cloonix/bulk && wget "
             "https://clownix.net/downloads/cnt_demo_bulk/%s.tar.gz\n"
             "  or build:  cd vm_create/cnt_kvm_demo && ./cnt_%s.sh"
             % (node, rootfs, last or "nothing", rootfs, rootfs))

def charon_bin(node):
    # strongswan-swanctl ships charon-systemd, not the legacy charon of
    # strongswan-starter; it runs standalone (its sd_notify is a no-op with
    # no systemd) and brings up the vici socket the same way.
    for cand in ("/usr/lib/ipsec/charon", "/usr/libexec/ipsec/charon",
                 "/usr/lib/strongswan/charon",
                 "/usr/lib/ipsec/charon-systemd",
                 "/usr/libexec/ipsec/charon-systemd",
                 "/usr/lib/strongswan/charon-systemd",
                 "/usr/sbin/charon-systemd"):
        if ssh(node, "test -x %s && echo yes" % cand, check=False).strip():
            return cand
    return None

def strongswan_start():
    put(GWA, "/etc/swanctl/conf.d/demo.conf",
        SWANCTL_CONF.replace("PSKVALUE", PSK))
    binary = charon_bin(GWA)
    if not binary:
        sys.exit("no charon binary found on %s: not the strongswan image"
                 % GWA)
    ssh(GWA, "mkdir -p /var/run")
    ssh(GWA, "nohup %s >/tmp/charon.log 2>&1 &" % binary, check=False)
    for _ in range(20):
        if ssh(GWA, "test -S /var/run/charon.vici && echo up",
               check=False).strip():
            break
        time.sleep(1)
    else:
        raise RuntimeError("charon vici socket never appeared:\n%s"
                           % ssh(GWA, "cat /tmp/charon.log", check=False))
    ssh(GWA, "swanctl --load-all", check=False)

def libreswan_start():
    put(GWB, "/etc/ipsec.conf", IPSEC_CONF)
    put(GWB, "/etc/ipsec.secrets", IPSEC_SECRETS.replace("PSKVALUE", PSK))
    ssh(GWB, "ipsec initnss >/dev/null 2>&1", check=False)
    ssh(GWB, "ipsec pluto --config /etc/ipsec.conf", check=False)
    time.sleep(4)
    ssh(GWB, "ipsec auto --add link", check=False)

def up(timeout=50):
    # Only strongSwan initiates; libreswan is the responder (auto=add). Wait
    # for the ESP child to be INSTALLED, not merely the IKE SA ESTABLISHED --
    # the child is what encrypts the traffic.
    ssh(GWA, "swanctl --initiate --child icmp >/dev/null 2>&1", check=False)
    end = time.time() + timeout
    while time.time() < end:
        r = ssh(GWA, "swanctl --list-sas 2>/dev/null", check=False)
        if "INSTALLED" in r:
            return True
        ssh(GWA, "swanctl --initiate --child icmp >/dev/null 2>&1",
            check=False)
        time.sleep(3)
    return False

def esp_and_clear(count=6):
    """Ping gwA->gwB while counting ESP and cleartext ICMP on the wire.
    cloonix's sniff grammar has no proto keyword, so ESP (IP protocol 50,
    no port, not tcp/udp/icmp) is caught by exclusion."""
    esp = count_on(GWA, 0, "ip and not tcp and not udp and not icmp",
                   lambda: ssh(GWA, "ping -c %d 10.0.0.2" % count,
                               check=False), settle=3.0)
    clr = count_on(GWA, 0, "icmp",
                   lambda: ssh(GWA, "ping -c %d 10.0.0.2" % count,
                               check=False), settle=3.0)
    return (esp["count"] if esp else -1, clr["count"] if clr else -1)

LAYOUT = {GWA: (-260, 0), LAN: (0, 0), GWB: (260, 0)}
ETHS = {(GWA, "eth0"): 0, (GWB, "eth0"): 180}

CARD = r"""
   gwA --- lan1 --- gwB
 strongswan       libreswan
 10.0.0.1         10.0.0.2

  Two different IPsec stacks agree on IKEv2 and a shared secret, then carry
  the ping between them as ESP. The ICMP inside is no longer on the wire.
"""

def main():
    story(HISTORY)
    step("checking the server")
    check_net()
    story(CARD)
    erase_net()
    place()

    step("creating gwA (strongswan) and gwB (libreswan)")
    rootfs_a = ROOTFS_CHOICES[0]
    make(GWA, rootfs_a)
    make(GWB, LIBRESWAN_ROOTFS)
    for name in ALL:
        wait_answer(name)
    cable_all()
    in_parallel(address, list(ALL), "address")
    tools_present(GWA, "swanctl")
    tools_present(GWB, "ipsec")

    step("act 1: no protection yet, the ping is readable ICMP")
    story()
    before = count_on(GWA, 0, "icmp",
                      lambda: ssh(GWA, "ping -c 4 10.0.0.2", check=False),
                      settle=3.0)
    story("    cleartext ICMP on the wire before IPsec: %d packet(s)"
          % (before["count"] if before else -1))
    ok_clear_before = bool(before and before["count"] > 0)
    hold()

    step("act 2: negotiate IKE and install the ESP association")
    story()
    strongswan_start()
    libreswan_start()
    if not up():
        sys.exit("the two stacks did not establish an SA")
    story("    strongSwan reports the association:")
    for line in ssh(GWA, "swanctl --list-sas", check=False).strip()\
            .split("\n")[:8]:
        story("      %s" % line)
    story()

    step("act 3: the same ping, now as ESP")
    story()
    esp, clr = esp_and_clear()
    story("    with IPsec up, a ping from gwA to gwB shows on the wire as:")
    story("      ESP (IP proto 50) packets : %d" % esp)
    story("      cleartext ICMP packets    : %d" % clr)
    story()
    story("    The ping still works, but the ICMP is now inside ESP and no")
    story("    longer readable. Two different programs, one standard, one")
    story("    encrypted channel.")
    ok_esp = esp > 0
    ok_hidden = clr == 0
    story()

    if not (ok_clear_before and ok_esp and ok_hidden):
        sys.exit("a check failed: clear_before=%s esp=%s hidden=%s"
                 % (ok_clear_before, ok_esp, ok_hidden))

    story("""
the network is left up. Things to try:

  the association from each side's point of view:
    cloonix cli %s ssh gwA "swanctl --list-sas"
    cloonix cli %s ssh gwB "ipsec status"

  see ESP on the wire while you ping:
    cloonix cli %s sniff gwA eth0 --dump "ip and not tcp and not udp and not icmp"
""" % (NET, NET, NET))

main()
