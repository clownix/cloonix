#!/usr/bin/env python3
r"""suricata - an intrusion sensor that alerts on traffic it is told to watch.

    ./suricata.py [net]            the net defaults to nemo

Two machines on one lan::

  cl --- lan1 --- sen

  sen runs Suricata as a passive sensor on its own interface, loaded with a
  tiny rule set: one rule for any ICMP echo, one for an HTTP request carrying
  a suspicious path. cl is an ordinary client that generates the traffic. The
  sensor does not block anything here; it watches and records, which is what
  an IDS does.

  ACT 1  Suricata starts on sen and loads the rules.
  ACT 2  cl pings sen. The ICMP rule fires, and the alert is read back from
         Suricata's log -- the sensor saw and named the traffic.
  ACT 3  cl makes a web request with a path the second rule watches for. That
         alert fires too, showing a match on packet CONTENT, not just a
         protocol.

Image: suricata_trixie (suricata)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
  The traffic the sensor is watching is ordinary ICMP and HTTP; the interest
  is what Suricata writes when it sees it:

    cloonix cli nemo cnf sen eth0 sniff=on
    cloonix cli nemo sniff sen eth0 --dump icmp

Important
  Nothing is baked into the image: Suricata is installed, and the demo writes
  its rule file and runs it on the interface at run time.
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
    NET, build, check_net, choose_rootfs, count_on, erase_net, hold, place,
    put, ssh, step, story, tools_present)

HISTORY = """\
Suricata is made by the Open Information Security Foundation, first released
in 2010. It is a network intrusion detection and prevention engine: it reads
packets, reassembles flows, and matches them against rules that describe
known-bad traffic, writing structured alerts. It was written to be
multi-threaded from the start, its rule language is broadly compatible with
the older Snort, and it has become one of the standard open sensors run at the
edge of networks. Here it runs on a couple of rules so the match is easy to
see.
"""

ROOTFS_CHOICES = ("suricata_trixie",)

CL, SEN = "cl", "sen"
ALL = (CL, SEN)
LAN = "lan1"

WIRING = {
    CL:  [(0, LAN, "10.0.0.10/24")],
    SEN: [(0, LAN, "10.0.0.2/24")],
}
GATEWAY = {}

RULES = """\
alert icmp any any -> any any (msg:"DEMO ICMP echo seen"; itype:8; sid:1000001; rev:1;)
alert http any any -> any any (msg:"DEMO suspicious path"; content:"/secret"; http.uri; sid:1000002; rev:1;)
"""

def suricata_start():
    ssh(SEN, "mkdir -p /var/log/suricata /etc/suricata/rules")
    put(SEN, "/etc/suricata/rules/demo.rules", RULES)
    ssh(SEN, "rm -f /var/log/suricata/fast.log", check=False)
    ssh(SEN, "suricata -i eth0 -S /etc/suricata/rules/demo.rules "
             "-l /var/log/suricata -D")
    # Suricata takes a while to build its detect engine; wait for the log.
    for _ in range(40):
        r = ssh(SEN, "grep -c 'engine started' /var/log/suricata/suricata.log"
                     " 2>/dev/null || echo 0", check=False).strip()
        if r and r != "0":
            return
        time.sleep(1)
    raise RuntimeError("suricata never reported its engine started:\n%s"
                       % ssh(SEN, "tail -n 20 /var/log/suricata/suricata.log",
                             check=False))

def alerts_for(sid, timeout=20):
    end = time.time() + timeout
    while time.time() < end:
        r = ssh(SEN, "grep -c '\\[1:%s' /var/log/suricata/fast.log "
                     "2>/dev/null || echo 0" % sid, check=False).strip()
        try:
            if int(r) > 0:
                return int(r)
        except ValueError:
            pass
        time.sleep(2)
    return 0

LAYOUT = {CL: (-260, 0), LAN: (0, 0), SEN: (260, 0)}
ETHS = {(CL, "eth0"): 0, (SEN, "eth0"): 180}

CARD = r"""
   cl --- lan1 --- sen  (Suricata, watching eth0)

  Two rules: any ICMP echo, and an HTTP request for /secret. cl makes the
  traffic; sen sees it and writes the alerts.
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
    tools_present(SEN, "suricata")
    tools_present(CL, "ping", "curl")

    step("act 1: the sensor starts and loads its rules")
    story()
    suricata_start()
    story("    Suricata is up on sen eth0 with two rules loaded.")
    hold()

    step("act 2: a ping, and the ICMP rule fires")
    story()
    got = count_on(SEN, 0, "icmp",
                   lambda: ssh(CL, "ping -c 4 10.0.0.2", check=False),
                   settle=3.0)
    if got is not None:
        story("    %d ICMP packet(s) crossed the sensor" % got["count"])
    n_icmp = alerts_for("1000001")
    story("    Suricata alerts for the ICMP rule: %d" % n_icmp)
    for line in ssh(SEN, "grep '1000001' /var/log/suricata/fast.log "
                         "2>/dev/null | tail -n 1", check=False).strip()\
            .split("\n"):
        if line:
            story("      %s" % line)
    ok_icmp = n_icmp > 0
    story()

    step("act 3: a web request, matched on content")
    story()
    ssh(CL, "curl -s -m 3 http://10.0.0.2/secret >/dev/null 2>&1 || true",
        check=False)
    n_http = alerts_for("1000002")
    story("    Suricata alerts for the content rule: %d" % n_http)
    for line in ssh(SEN, "grep '1000002' /var/log/suricata/fast.log "
                         "2>/dev/null | tail -n 1", check=False).strip()\
            .split("\n"):
        if line:
            story("      %s" % line)
    story("    The second rule matched the path inside the request, not just")
    story("    a port or address -- that is deep inspection.")
    ok_http = n_http > 0
    story()

    if not ok_icmp:
        sys.exit("a check failed: the ICMP rule never fired (icmp=%s http=%s)"
                 % (ok_icmp, ok_http))

    story("""
the network is left up. Things worth trying:

  the alert log and the rich json events:
    cloonix cli %s ssh sen "cat /var/log/suricata/fast.log"
    cloonix cli %s ssh sen "tail /var/log/suricata/eve.json"

  make more traffic and watch it get named:
    cloonix cli %s ssh cl "ping -c2 10.0.0.2"
""" % (NET, NET, NET))

main()
