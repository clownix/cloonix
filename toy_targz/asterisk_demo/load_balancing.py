#!/usr/bin/env python3
r"""load_balancing - Kamailio Dispatcher load balancing.

    ./load_balancing.py [net]     the net defaults to nemo

The dispatcher module of Kamailio, as its documentation configures it: a
SIP proxy that spreads the calls over a set of switches, round robin
(algorithm 4), watches them with OPTIONS (ds_ping_interval) and, in a
failure_route, sends a call that got no answer to the next switch. Three
asterisk behind one proxy; one of them is drained in the middle of the run
(core stop gracefully), as a switch is taken out of service.
https://www.kamailio.org/docs/modules/stable/modules/dispatcher.html

    caller (10.0.0.2) ---- lan1 ---- proxy (10.0.0.10) ---- pbx1 (10.0.0.11)
    SIPp, 60 calls                   kamailio              pbx2 (10.0.0.12)
                                     dispatcher            pbx3 (10.0.0.13)

  ACT 1  the proxy and the three asterisk start; kamcmd dispatcher.list
         shows the set, each destination active.
  ACT 2  SIPp sends 60 calls at 3 a second; at 8 s pbx2 is stopped
         gracefully: its calls end normally, a new call gets 503 and the
         proxy gives it to the next switch, its OPTIONS answer 503 and the
         proxy sets it inactive. SIPp counts 60 successful.

Image: asterisk_trixie (kamailio, asterisk, sipp)
Size:  5 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf proxy eth0 sniff=on
    cloonix cli nemo sniff proxy eth0 --dump "udp port 5060 and host 10.0.0.12"
  The OPTIONS of the proxy to pbx2 left unanswered once it is stopped, and
  an INVITE sent to it, then the same INVITE sent to pbx3.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py, asterisklib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import hold, step
from asterisklib import (
    asterisk_start, hosts, ip_of, opening, proof, put_any, rx, sipp,
    sipp_stats, sipp_wait, ssh, trunk, UDP_TRANSPORT, until, verdict)

HISTORY = """\
SER, the SIP Express Router, was written at Fraunhofer FOKUS in Berlin from
2001 by Jiri Kuthan, Andrei Pelinescu-Onciul, Bogdan-Andrei Iancu, Jan Janak
and Daniel-Constantin Mierla; its fork OpenSER of 2005 was renamed Kamailio
in 2008. Written in C, it relays thousands of calls a second and keeps none
of their voice: carriers, VoIP operators and the IMS of 4G voice put it in
front of their media servers, its dispatcher choosing one for each call.
"""

ROOTFS_CHOICES = ("asterisk_trixie",)
TOOL = "kamailio"
PROXY, CALLER = "proxy", "caller"
PBXS = ("pbx1", "pbx2", "pbx3")
ALL = (PROXY,) + PBXS + (CALLER,)
WIRING = {PROXY: [(0, "lan1", "10.0.0.10/24")], CALLER: [(0, "lan1", "10.0.0.2/24")]}
WIRING.update({p: [(0, "lan1", "10.0.0.%d/24" % (11 + i))] for i, p in enumerate(PBXS)})
GATEWAY = {}
LAYOUT = {CALLER: (-330, 0), "lan1": (-110, 0), PROXY: (-110, -120),
          "pbx1": (110, -110), "pbx2": (110, 0), "pbx3": (110, 110)}
ETHS = {(CALLER, "eth0"): 90, (PROXY, "eth0"): 180, ("pbx1", "eth0"): 225,
        ("pbx2", "eth0"): 270, ("pbx3", "eth0"): 315}
CARD = r"""
    caller (SIPp) ---- proxy (kamailio dispatcher) ---- pbx1 pbx2 pbx3
  Calls spread over three switches, one stopped, none lost.
"""

KAMAILIO_CFG = """\
#!KAMAILIO
debug=2
log_stderror=no
children=4
listen=udp:10.0.0.10:5060
mpath="/usr/lib/x86_64-linux-gnu/kamailio/modules/"
loadmodule "tm.so"
loadmodule "sl.so"
loadmodule "rr.so"
loadmodule "pv.so"
loadmodule "maxfwd.so"
loadmodule "siputils.so"
loadmodule "textops.so"
loadmodule "dispatcher.so"
loadmodule "ctl.so"
modparam("dispatcher", "list_file", "/etc/kamailio/dispatcher.list")
modparam("dispatcher", "flags", 2)
modparam("dispatcher", "ds_ping_interval", 2)
modparam("dispatcher", "ds_probing_mode", 1)
modparam("dispatcher", "ds_probing_threshold", 1)
modparam("dispatcher", "ds_ping_reply_codes", "class=2")
modparam("tm", "fr_timer", 1500)
modparam("tm", "fr_inv_timer", 10000)

request_route {
    if (!mf_process_maxfwd_header("10")) {
        sl_send_reply("483", "Too Many Hops");
        exit;
    }
    if (has_totag()) {
        if (loose_route()) {
            t_relay();
            exit;
        }
        if (is_method("ACK") && t_check_trans()) {
            t_relay();
        }
        exit;
    }
    if (is_method("CANCEL")) {
        if (t_check_trans()) {
            t_relay();
        }
        exit;
    }
    if (is_method("INVITE")) {
        record_route();
        if (!ds_select_dst("1", "4")) {
            sl_send_reply("503", "No switch");
            exit;
        }
        t_on_failure("NEXT");
    }
    t_relay();
}

failure_route[NEXT] {
    if (t_is_canceled()) {
        exit;
    }
    if (t_branch_timeout() || t_check_status("5[0-9][0-9]")) {
        ds_mark_dst("ip");
        if (ds_next_dst()) {
            t_on_failure("NEXT");
            t_relay();
            exit;
        }
    }
}
"""
DISPATCHER_LIST = "".join("1 sip:10.0.0.%d:5060\n" % (11 + i) for i in range(3))
PJSIP_CONF = UDP_TRANSPORT + trunk("proxy", "10.0.0.10", "lb")
EXTENSIONS_CONF = """\
[lb]
exten => _X.,1,Answer()
 same => n,Wait(30)
 same => n,Hangup()
"""


def processed(pbx):
    out = rx(pbx, "core show calls")
    m = [int(l.split()[0]) for l in out.split("\n") if "processed" in l]
    return m[0] if m else 0


def main():
    opening()
    hosts()
    step("act 1: the proxy and the three asterisk, the dispatcher set")
    for p in PBXS:
        asterisk_start(p, {"pjsip.conf": PJSIP_CONF, "extensions.conf": EXTENSIONS_CONF})
    ssh(PROXY, "mkdir -p /etc/kamailio /var/run/kamailio")
    put_any(PROXY, "/etc/kamailio/kamailio.cfg", KAMAILIO_CFG)
    put_any(PROXY, "/etc/kamailio/dispatcher.list", DISPATCHER_LIST)
    check = ssh(PROXY, "kamailio -c -f /etc/kamailio/kamailio.cfg 2>&1", check=False)
    ssh(PROXY, "kamailio -f /etc/kamailio/kamailio.cfg -P /var/run/kamailio/kamailio.pid "
               "-m 64 -M 8 </dev/null >/dev/null 2>&1")
    ok_up = until(lambda: "10.0.0.13" in ssh(PROXY, "kamcmd dispatcher.list",
                                              check=False), 20, 1)
    proof("proxy, kamcmd dispatcher.list:", ssh(PROXY, "kamcmd dispatcher.list",
                                                 check=False) or check)
    hold()
    step("act 2: 60 calls, pbx2 stopped at 8 s, none lost")
    sipp(CALLER, "-sn uac %s:5060 -s 100 -i %s -p 5060 -r 3 -m 60 -d 3000 "
         "-timeout 120 -timeout_error" % (ip_of(PROXY), ip_of(CALLER)),
         background=True)
    until(lambda: False, 8, 1)
    rx("pbx2", "core stop gracefully")
    rc = sipp_wait(CALLER, timeout=120)
    ok, ko = sipp_stats(CALLER)
    ds = ssh(PROXY, "kamcmd dispatcher.list", check=False)
    n1, n3 = processed("pbx1"), processed("pbx3")
    block = ds[ds.find("10.0.0.12"):][:200]
    ok_inactive = "FLAGS: I" in block or "FLAGS: D" in block
    proof("caller, SIPp exit %d, %d successful, %d failed; pbx1 %d calls, pbx3 %d"
          " calls; proxy, the set:" % (rc, ok, ko, n1, n3), ds)
    verdict([("the proxy starts with the three switches active", ok_up),
             ("SIPp: the 60 calls went through (exit 0)", rc == 0),
             ("pbx1 and pbx3 share the calls", n1 > 0 and n3 > 0),
             ("pbx2, stopped, is set inactive", ok_inactive)])


main()
