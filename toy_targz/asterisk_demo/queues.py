#!/usr/bin/env python3
r"""queues - Building Queues.

    ./queues.py [net]     the net defaults to nemo

The call queue of Asterisk, app_queue, as the page "Building Queues" of the
documentation builds one: a queue "support" with two members, the agents
7001 and 7002, a strategy (rrmemory: round robin, remembering who took the
last call), music on hold for those who wait, sent as early media before
any answer. Four callers arrive at once from SIPp; two agents can take two;
the two others wait in the queue until an agent is free.
https://docs.asterisk.org/

    agent1 (10.0.0.2) ---+
    agent2 (10.0.0.3) ---+---- lan1 ---- pbx (10.0.0.1)
    callers (10.0.0.4) --+               queue support
    SIPp, 4 calls

  ACT 1  the agents register; the four calls come in at once: the queue
         holds the two that find no agent free (queue show support).
  ACT 2  every call is answered in turn: SIPp counts 4 successful calls,
         the queue 4 completed, each agent took calls.

Image: asterisk_trixie (asterisk, baresip, sipp)
Size:  4 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf pbx eth0 sniff=on
    cloonix cli nemo sniff pbx eth0 --dump "udp port 5060 and host 10.0.0.2"
  The INVITE of asterisk to agent1 when a caller reaches the head of the
  queue: the queue is a dialplan application that dials its members.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py, asterisklib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import hold, step, story
from asterisklib import (
    asterisk_start, endpoint, hosts, ip_of, opening, phone_start, proof,
    put_any, registered, rx, SECRET, sipp, sipp_stats, sipp_uac_xml,
    sipp_wait, trunk, UDP_TRANSPORT, until, verdict)

HISTORY = """\
Automatic call distribution was born in the 1970s: Continental Airlines put
the Rockwell Galaxy ACD into service in 1973, the first system to queue
callers and hand each to the agent free the longest. The call centre grew
around it. app_queue has been in Asterisk since its first releases, with
its strategies (ringall, leastrecent, fewestcalls, random, rrmemory,
linear, wrandom) and its queue_log, the record call centres compute from.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "asterisk"
PBX, A1, A2, CALLERS = "pbx", "agent1", "agent2", "callers"
ALL = (PBX, A1, A2, CALLERS)
WIRING = {PBX: [(0, "lan1", "10.0.0.1/24")],
          A1: [(0, "lan1", "10.0.0.2/24")],
          A2: [(0, "lan1", "10.0.0.3/24")],
          CALLERS: [(0, "lan1", "10.0.0.4/24")]}
GATEWAY = {}
LAYOUT = {A1: (-220, -110), A2: (-220, 0), CALLERS: (-220, 110),
          "lan1": (0, 0), PBX: (220, 0)}
ETHS = {(A1, "eth0"): 135, (A2, "eth0"): 90, (CALLERS, "eth0"): 45,
        (PBX, "eth0"): 270}
CARD = r"""
    agent1, agent2 (baresip) ---+---- pbx (asterisk, queue support)
    callers (SIPp, 4 calls) ----+
  Four callers, two agents, a queue in between.
"""

PJSIP_CONF = UDP_TRANSPORT + endpoint("7001", "demo", SECRET) + \
    endpoint("7002", "demo", SECRET) + trunk("sipp", "10.0.0.4", "demo",
                                             codecs=("alaw", "ulaw"))
QUEUES_CONF = """\
[general]

[support]
strategy = rrmemory
timeout = 15
retry = 1
wrapuptime = 0
joinempty = yes
ringinuse = no
musicclass = default
member => PJSIP/7001
member => PJSIP/7002
"""
MUSICONHOLD_CONF = """\
[default]
mode = playlist
entry = /usr/share/asterisk/sounds/en/queue-thankyou
entry = /usr/share/asterisk/sounds/en/hello-world
"""
EXTENSIONS_CONF = """\
[demo]
exten => 500,1,Progress()
 same => n,Queue(support)
 same => n,Hangup()
"""


def main():
    opening()
    hosts()
    step("act 1: two agents, four callers at once, the queue holds two")
    asterisk_start(PBX, {"pjsip.conf": PJSIP_CONF, "queues.conf": QUEUES_CONF,
                         "musiconhold.conf": MUSICONHOLD_CONF,
                         "extensions.conf": EXTENSIONS_CONF},
                   modules=("app_queue", "res_musiconhold"))
    phone_start(A1, "7001", ip_of(PBX), answer=True, tone=500)
    phone_start(A2, "7002", ip_of(PBX), answer=True, tone=700)
    until(lambda: registered(PBX, "7001") and registered(PBX, "7002"), 20, 1)
    put_any(CALLERS, "/tmp/call.xml", sipp_uac_xml(
        "500", 6000, '  <pause milliseconds="6000"/>\n'))
    sipp(CALLERS, "-sf /tmp/call.xml %s:5060 -i %s -p 5060 -mp 6000 -m 4 "
         "-l 4 -r 4 -timeout 90 -timeout_error" % (ip_of(PBX), ip_of(CALLERS)),
         background=True)
    ok_wait = until(lambda: "Callers:" in rx(PBX, "queue show support")
                    and "1. PJSIP/sipp" in rx(PBX, "queue show support"), 15, 0.5)
    proof("pbx, queue show support, two calls with the agents, two waiting:",
          rx(PBX, "queue show support"))
    hold()
    step("act 2: every caller answered in turn")
    rc = sipp_wait(CALLERS, timeout=90)
    ok, ko = sipp_stats(CALLERS)
    show = rx(PBX, "queue show support")
    ok_sipp = rc == 0
    ok_done = "C:4" in show
    ok_both = show.count("has taken") == 2 and "has taken no calls" not in show
    proof("callers, SIPp exit %d, %d successful, %d failed; pbx, the queue:"
          % (rc, ok, ko), show)
    verdict([("two callers wait in the queue", bool(ok_wait)),
             ("SIPp: the 4 calls went through (exit 0)", ok_sipp),
             ("the queue counts 4 completed calls", ok_done),
             ("each agent took calls", ok_both)])
    story("""
the network is left up. Things to try, in a terminal of pbx:
the SIP dialogs as ladders:
sngrep
the console of asterisk, its channels as they come:
asterisk -rvvv
the queue, its members and its waiting callers:
asterisk -rx "queue show support"
""")


main()
