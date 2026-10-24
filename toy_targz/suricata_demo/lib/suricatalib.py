"""suricatalib - the talk to Suricata of the suricata_demo demos: rules
written as the guide prints them, suricata started and waited for, the
alerts read back from eve.json and fast.log. The rest is toylib.py."""
import time
from demolib import ssh
from toylib import (
    forwarding, need_kernel, opening, ping, pings, proof,
    put_any, until, verdict)

# What the demos import from here, toylib re-exported with it.
__all__ = ["forwarding", "need_kernel", "opening", "ping",
           "pings", "proof", "put_any", "until", "verdict", "suricata_start",
           "suricata_stop", "count", "events", "fast", "web_server",
           "listening"]

LOG = "/var/log/suricata"
RULES = "/etc/suricata/rules/demo.rules"

# Counts the eve.json events of a type, and of a rule for the alerts.
EVE_COUNT = """\
import json, sys
kind, sid = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 0
n = 0
try:
    for line in open("/var/log/suricata/eve.json"):
        try:
            e = json.loads(line)
        except ValueError:
            continue
        if e.get("event_type") != kind:
            continue
        if sid and e.get("alert", {}).get("signature_id") != sid:
            continue
        n += 1
except OSError:
    pass
print(n)
"""

def suricata_start(node, rules, capture="-i eth0", sets=(), rule_opt=None):
    """The rules in place, suricata started as a daemon, back once its engine
    runs. rule_opt replaces "-S demo.rules" when the demo loads its rules
    through suricata.yaml instead."""
    suricata_stop(node)
    ssh(node, "mkdir -p %s /etc/suricata/rules" % LOG)
    ssh(node, "rm -f %s/eve.json %s/fast.log %s/suricata.log" % (LOG, LOG, LOG))
    put_any(node, "/usr/local/bin/eve_count.py", EVE_COUNT)
    if rules is not None:
        put_any(node, RULES, rules)
    opt = rule_opt if rule_opt is not None else "-S %s" % RULES
    line = "suricata -c /etc/suricata/suricata.yaml -k none %s -l %s -D %s" \
        % (opt, LOG, capture)
    line += "".join(" --set %s" % s for s in sets)
    ssh(node, line)
    for _ in range(60):
        if "engine started" in ssh(node, "grep -i 'engine started' "
                                   "%s/suricata.log 2>/dev/null" % LOG,
                                   check=False).lower():
            return
        time.sleep(1)
    raise RuntimeError("suricata did not start on %s:\n%s" % (node, ssh(
        node, "tail -n 20 %s/suricata.log" % LOG, check=False)))

def suricata_stop(node):
    """suricata stopped, its datasets and logs flushed on the way out."""
    ssh(node, "pkill -TERM -x Suricata-Main; pkill -TERM -x suricata; "
              "for i in 1 2 3 4 5 6 7 8 9 10; do "
              "pgrep -x Suricata-Main >/dev/null || pgrep -x suricata "
              ">/dev/null || exit 0; sleep 1; done", check=False)

def count(node, sid):
    """The alerts of one rule in eve.json."""
    out = ssh(node, "python3 /usr/local/bin/eve_count.py alert %d" % sid,
              check=False).strip()
    return int(out) if out.isdigit() else 0

def events(node, kind):
    """The eve.json events of one type: http, dns, flow, drop..."""
    out = ssh(node, "python3 /usr/local/bin/eve_count.py %s" % kind,
              check=False).strip()
    return int(out) if out.isdigit() else 0

def fast(node):
    return ssh(node, "cat %s/fast.log 2>/dev/null" % LOG, check=False)

def listening(node, port, log):
    """Back once a server listens on the tcp port of node, else its log in
    the fault: the cut python of a toy compiles its modules at its first
    start, seconds on a loaded host."""
    if not until(lambda: ssh(node, "ss -Hltn sport = :%d" % port,
                             check=False).strip(), 30, 0.5):
        raise RuntimeError("nothing listens on port %d of %s:\n%s" % (
            port, node, ssh(node, "cat %s" % log, check=False)))

def web_server(node, pages):
    """A web server of python3 on port 80, serving {path: text}, back once it
    listens."""
    ssh(node, "mkdir -p /srv/www")
    for path, text in pages.items():
        ssh(node, "mkdir -p /srv/www/%s" % path.rsplit("/", 1)[0]
            if "/" in path else "true")
        put_any(node, "/srv/www/%s" % path, text)
    ssh(node, "cd /srv/www && python3 -m http.server 80 "
              "</dev/null >/tmp/http.log 2>&1 &")
    listening(node, 80, "/tmp/http.log")
