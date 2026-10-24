#!/usr/bin/env python3
r"""suricata - an intrusion sensor that alerts on traffic it is told to watch.

    ./suricata.py [net]            the net defaults to nemo

Two machines on one lan::

  cl --- lan1 --- sen

  sen runs Suricata as a passive sensor on its own interface, loaded with a
  tiny rule set: one rule for any ICMP echo, one for an HTTP request carrying
  a suspicious path. sen also runs a web server, python3 -m http.server, so
  the request crosses the wire. cl is an ordinary client that
  generates the traffic. The sensor does not block anything here; it watches
  and records, which is what an IDS does.

  ACT 1  Suricata starts on sen and loads the rules; the demo waits for its
         "Engine started", written once eth0 is captured.
  ACT 2  cl pings sen. The ICMP rule fires, and the alert is read back from
         Suricata's log -- the sensor saw and named the traffic.
  ACT 3  cl asks the web server of sen for a path the second rule watches
         for. That alert fires too, showing a match on packet CONTENT, not
         just a protocol.

Image: suricata_trixie (suricata, curl, python3)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
  The traffic the sensor is watching is ordinary ICMP and HTTP; the interest
  is what Suricata writes when it sees it:

    cloonix cli nemo cnf sen eth0 sniff=on
    cloonix cli nemo sniff sen eth0 --dump icmp

Important
  Nothing is baked into the image: Suricata is installed, and the demo writes
  its rule file and runs it on the interface at run time. In a rule, a sticky
  buffer such as http.uri comes BEFORE the content it applies to.
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
known-bad traffic, writing structured alerts. It was multi-threaded from the
start and its rule language is broadly compatible with the older Snort.
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
alert http any any -> any any (msg:"DEMO suspicious path"; http.uri; content:"/secret"; sid:1000002; rev:1;)
"""

def suricata_said():
    """For a failure: the end of what Suricata wrote, and whether it runs."""
    return ssh(SEN, "tail -n 15 /var/log/suricata/suricata.log; "
                    "pgrep -x Suricata-Main >/dev/null && echo suricata runs "
                    "|| echo suricata is not running", check=False)

def suricata_start():
    """Back once the engine runs, with what Suricata says of the rules."""
    ssh(SEN, "mkdir -p /var/log/suricata /etc/suricata/rules")
    put(SEN, "/etc/suricata/rules/demo.rules", RULES)
    ssh(SEN, "rm -f /var/log/suricata/fast.log /var/log/suricata/suricata.log")
    ssh(SEN, "suricata -i eth0 -S /etc/suricata/rules/demo.rules "
             "-l /var/log/suricata -D")
    # "Engine started" is written once eth0 is captured.
    for _ in range(60):
        r = ssh(SEN, "grep -qi 'engine started' /var/log/suricata/suricata.log"
                     " && echo up || pgrep -x Suricata-Main >/dev/null"
                     " || echo gone", check=False).strip()
        if r == "gone":
            break
        if r == "up":
            loaded = ssh(SEN, "grep -o '[0-9]* rules successfully loaded, "
                              "[0-9]* rules failed' "
                              "/var/log/suricata/suricata.log",
                         check=False).strip()
            if loaded == "%d rules successfully loaded, 0 rules failed" \
                    % len(RULES.splitlines()):
                return loaded
            raise RuntimeError("suricata did not take every rule of "
                               "demo.rules:\n%s" % suricata_said())
        time.sleep(1)
    raise RuntimeError("suricata did not start on sen:\n%s" % suricata_said())

def alerts_for(sid, timeout=20):
    """The fast.log lines of one rule, waited for."""
    end = time.time() + timeout
    while True:
        r = ssh(SEN, "grep -F '[1:%d:' /var/log/suricata/fast.log 2>/dev/null"
                % sid, check=False).strip()
        if r or time.time() > end:
            return r.split("\n") if r else []
        time.sleep(2)

LAYOUT = {CL: (-260, 0), LAN: (0, 0), SEN: (260, 0)}
ETHS = {(CL, "eth0"): 0, (SEN, "eth0"): 180}

CARD = r"""
   cl --- lan1 --- sen  (Suricata, watching eth0)

  Two rules: any ICMP echo, and an HTTP request for /secret. cl pings sen
  and asks its web server for /secret; Suricata on sen writes the alerts.
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
    tools_present(SEN, "suricata", "python3")
    tools_present(CL, "ping", "curl")

    step("act 1: the sensor starts and loads its rules")
    story()
    loaded = suricata_start()
    story("    Suricata is up on sen eth0: %s." % loaded)
    hold()

    step("act 2: a ping, and the ICMP rule fires")
    story()
    got = count_on(SEN, 0, "icmp",
                   lambda: ssh(CL, "ping -c 4 10.0.0.2", check=False),
                   settle=3.0)
    if got is not None:
        story("    %d ICMP packet(s) crossed the sensor" % got["count"])
    icmp = alerts_for(1000001)
    story("    Suricata alerts for the ICMP rule: %d" % len(icmp))
    if icmp:
        story("      %s" % icmp[-1])
    story()

    step("act 3: a web request, matched on content")
    story()
    ssh(SEN, "cd /tmp && python3 -m http.server 80 "
             "</dev/null >/dev/null 2>&1 &")
    code = ssh(CL, "curl -s -o /dev/null -w '%{http_code}' -m 3 --retry 5 "
                   "--retry-connrefused --retry-delay 1 "
                   "http://10.0.0.2/secret", check=False).strip()
    story("    cl asked the web server of sen for /secret: HTTP %s"
          % (code or "no answer"))
    http = alerts_for(1000002)
    story("    Suricata alerts for the content rule: %d" % len(http))
    if http:
        story("      %s" % http[-1])
    story("    The second rule matched the path inside the request, not just")
    story("    a port or address -- that is deep inspection.")
    story()

    failed = [what for what, alerts in (("the ICMP rule never fired", icmp),
                                        ("the content rule never fired", http))
              if not alerts]
    if failed:
        sys.exit("a check failed: %s\n%s" % ("; ".join(failed),
                                             suricata_said()))

    story("""
the network is left up. Things to try:

  the alert log and the rich json events:
    cloonix cli %s ssh sen "cat /var/log/suricata/fast.log"
    cloonix cli %s ssh sen "tail /var/log/suricata/eve.json"

  make more traffic and watch it get named:
    cloonix cli %s ssh cl "ping -c2 10.0.0.2"
    cloonix cli %s ssh cl "curl -s http://10.0.0.2/secret"
""" % (NET, NET, NET, NET))

main()
