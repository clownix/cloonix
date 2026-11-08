"""asterisklib - what the demos of asterisk_demo share, on top of demolib and
toylib. Each demo plays one page of the Asterisk documentation, or of the SIPp
and Kamailio ones: asterisk is written and started here, the phones are set up
and driven here (baresip, a real softphone, and SIPp, a call generator), and
what they say is read back here. Standard library only.
"""
import base64
import io
import json
import math
import re
import struct
import wave
import __main__ as DEMO

from demolib import in_parallel, ssh
from toylib import opening, proof, put_any, until, verdict

SECRET = "cloonix"
RTP_START, RTP_END = 10000, 20000
SIPP_PCAP = "/usr/share/sip-tester/g711a.pcap"

# modules.conf has autoload = no: every demo loads these, and adds its own;
# res_pjsip_nat is what makes force_rport and rewrite_contact act
BASE_MODULES = (
    "res_sorcery_config", "res_sorcery_memory", "res_sorcery_astdb",
    "res_pjproject", "res_rtp_asterisk", "res_timing_timerfd", "res_pjsip",
    "res_pjsip_session", "res_pjsip_pubsub", "res_pjsip_authenticator_digest",
    "res_pjsip_endpoint_identifier_user", "res_pjsip_endpoint_identifier_ip",
    "res_pjsip_registrar", "res_pjsip_sdp_rtp", "res_pjsip_dtmf_info",
    "res_pjsip_caller_id", "res_pjsip_rfc3326", "res_pjsip_logger",
    "res_pjsip_nat", "chan_pjsip", "pbx_config", "app_dial", "app_playback",
    "app_echo", "app_verbose", "app_senddtmf", "app_mixmonitor",
    "func_callerid", "func_channel", "func_logic", "bridge_simple",
    "bridge_native_rtp", "codec_ulaw", "codec_alaw", "codec_gsm", "format_gsm",
    "format_wav", "format_pcm", "res_clioriginate")

ASTERISK_CONF = "[options]\nverbose = 3\nnocolor = yes\n"
LOGGER_CONF = "[general]\n[logfiles]\nmessages => notice,warning,error,verbose\n"
RTP_CONF = "[general]\nrtpstart = %d\nrtpend = %d\n" % (RTP_START, RTP_END)


def ip_of(node):
    """The first address of node in WIRING, without its length."""
    for _num, _lan, addr in DEMO.WIRING[node]:
        if addr:
            return addr.split("/")[0]
    raise RuntimeError("%s has no address in WIRING" % node)


def hosts():
    """Every name of the demo in /etc/hosts of every machine and no
    resolv.conf: no name server here, so a lookup that misses fails at once
    instead of waiting 10 s (asterisk looks up its own name for every
    channel)."""
    names = "127.0.0.1 localhost\n" + "".join(
        "%s %s\n" % (ip_of(n), n) for n in DEMO.ALL)
    def one(node):
        put_any(node, "/etc/hosts", names)
        ssh(node, 'echo "127.0.1.1 $(cat /proc/sys/kernel/hostname)" >> /etc/hosts; '
                  "rm -f /etc/resolv.conf")
    in_parallel(one, list(DEMO.ALL), "hosts")


def modules_conf(extra=()):
    return "[modules]\nautoload = no\n" + "".join(
        "load => %s.so\n" % m for m in BASE_MODULES + tuple(extra))


def rx(node, cmd):
    """One command of the asterisk console of node."""
    return ssh(node, 'asterisk -rx "%s"' % cmd, check=False).strip()


def asterisk_start(node, files, modules=()):
    """files, names of /etc/asterisk with their text, written beside the
    common ones, asterisk started and waited for until fully booted."""
    ssh(node, "mkdir -p /etc/asterisk /var/lib/asterisk /var/spool/asterisk "
              "/var/log/asterisk /var/run/asterisk")
    conf = {"asterisk.conf": ASTERISK_CONF, "modules.conf": modules_conf(modules),
            "logger.conf": LOGGER_CONF, "rtp.conf": RTP_CONF}
    conf.update(files)
    for name in sorted(conf):
        put_any(node, "/etc/asterisk/" + name, conf[name])
    ssh(node, "asterisk </dev/null >/dev/null 2>&1")
    if not until(lambda: "Asterisk" in rx(node, "core show version"), 40, 1):
        raise RuntimeError("asterisk never answered on %s, its log:\n%s" % (
            node, ssh(node, "tail -n 20 /var/log/asterisk/messages", check=False)))
    rx(node, "core waitfullybooted")
    return rx(node, "core show version").split("\n")[0]


def asterisk_log(node, word):
    """The lines of the asterisk log of node that hold word."""
    return ssh(node, "grep -F '%s' /var/log/asterisk/messages" % word,
               check=False).strip()


def endpoint(name, context, password=None, codecs=("ulaw",), extra=()):
    """One phone of pjsip.conf: its endpoint, its auth if it registers with a
    password, its aor (one contact)."""
    lines = ["[%s]" % name, "type = endpoint", "context = %s" % context,
             "disallow = all", "allow = %s" % ",".join(codecs),
             "aors = %s" % name, "direct_media = no"]
    if password:
        lines.append("auth = %s" % name)
    lines += list(extra) + ["", "[%s]" % name, "type = aor", "max_contacts = 1", ""]
    if password:
        lines += ["[%s]" % name, "type = auth", "auth_type = userpass",
                  "username = %s" % name, "password = %s" % password, ""]
    return "\n".join(lines) + "\n"


def trunk(name, ip, context, codecs=("ulaw",)):
    """A peer known by its address, no registration: SIPp, a proxy, a switch."""
    return ("[{0}]\ntype = endpoint\ncontext = {1}\ndisallow = all\n"
            "allow = {2}\naors = {0}\ndirect_media = no\n\n"
            "[{0}]\ntype = aor\ncontact = sip:{3}:5060\n\n"
            "[{0}]\ntype = identify\nendpoint = {0}\nmatch = {3}\n\n").format(
                name, context, ",".join(codecs), ip)


UDP_TRANSPORT = ("[transport-udp]\ntype = transport\nprotocol = udp\n"
                 "bind = 0.0.0.0:5060\n\n")

BARESIP_CONF = """\
poll_method epoll
sip_listen {ip}:5060
sip_verify_server no
dns_server {dns}:53
audio_player aubridge,nil
audio_source ausine,{tone}
ausrc_srate 48000
ausrc_channels 2
auplay_srate 48000
auplay_channels 2
rtp_ports {rtp}
ctrl_tcp_listen 127.0.0.1:4444
module_path /usr/lib/baresip/modules
{modules}
module_app account.so
module_app menu.so
module_app ctrl_tcp.so
"""


def phone_start(node, user, server, answer=False, tone=440, codecs=("g711",),
                transport="udp", srtp=False, apps=()):
    """baresip on node, registered as user with server: a sine of tone Hz as
    its microphone, aubridge as a loudspeaker that plays to nothing, its
    commands as JSON on 127.0.0.1:4444 (phone_cmd)."""
    mods = ["module %s.so" % c for c in codecs] + ["module ausine.so",
                                                   "module aubridge.so"]
    if srtp:
        mods.append("module srtp.so")
    mods += ["module_app %s.so" % a for a in apps]
    conf = BARESIP_CONF.format(ip=ip_of(node), dns=server, tone=tone,
                               rtp="%d-%d" % (RTP_START, RTP_END),
                               modules="\n".join(mods))
    acc = "<sip:%s@%s;transport=%s>;auth_pass=%s;regint=600" % (
        user, server, transport, SECRET)
    if answer:
        acc += ";answermode=auto"
    if srtp:
        acc += ";mediaenc=srtp-mand"
    ssh(node, "mkdir -p /root/.baresip")
    put_any(node, "/root/.baresip/config", conf)
    put_any(node, "/root/.baresip/accounts", acc + "\n")
    ssh(node, "baresip -f /root/.baresip </dev/null >/tmp/baresip.log 2>&1 &")
    if not until(lambda: phone_cmd(node, "listcalls")[0], 20, 1):
        raise RuntimeError("baresip of %s never answered, its log:\n%s"
                           % (node, phone_log(node)))


def phone_cmd(node, command, params=None):
    """(ok, what baresip said): one command of its menu, as JSON over TCP."""
    msg = {"command": command, "token": command}
    if params is not None:
        msg["params"] = params
    js = json.dumps(msg)
    out = ssh(node, "printf '%%s' '%d:%s,' | nc -q 1 127.0.0.1 4444"
              % (len(js), js), check=False)
    return '"ok":true' in out, out


def phone_log(node):
    out = ssh(node, "cat /tmp/baresip.log", check=False)
    return re.sub(r"\x1b\[[0-9;]*m", "", out).replace("\r", "\n")


def registered(pbx, user):
    """The contact of user is in the registrar of pbx."""
    return "%s/sip:" % user in rx(pbx, "pjsip show contacts")


def sipp(node, args, log="/tmp/sipp.log", background=False):
    """SIPp on node; its exit code tells: 0 every call went through, 1 one
    failed at least. In the background it is written to log.rc, for
    sipp_wait."""
    cmd = "sipp %s -nostdin >%s 2>&1" % (args, log)
    if background:
        ssh(node, "rm -f %s.rc; (%s; echo $? > %s.rc) </dev/null >/dev/null 2>&1 &"
            % (log, cmd, log))
        return None
    out = ssh(node, cmd + "; echo sipp-exit=$?", check=False)
    m = re.search(r"sipp-exit=(\d+)", out)
    return int(m.group(1)) if m else -1


def sipp_wait(node, log="/tmp/sipp.log", timeout=180):
    """The exit code of a background SIPp once it ends, -1 if it never does."""
    rc = []
    def ended():
        out = ssh(node, "cat %s.rc" % log, check=False).strip()
        if out.isdigit():
            rc.append(int(out))
        return bool(rc)
    until(ended, timeout, 2)
    return rc[0] if rc else -1


def sipp_stats(node, log="/tmp/sipp.log"):
    """Successful and failed calls in the last screen of a SIPp log."""
    out = ssh(node, "cat %s" % log, check=False)
    ok = re.findall(r"Successful call\s*\|\s*\d+\s*\|\s*(\d+)", out)
    ko = re.findall(r"Failed call\s*\|\s*\d+\s*\|\s*(\d+)", out)
    return (int(ok[-1]) if ok else 0), (int(ko[-1]) if ko else 0)


def sipp_uac_xml(dial, media_port, actions="", switch_hangs_up=False):
    """A SIPp caller: INVITE with an SDP offering PCMA and telephone-event
    101 (the type of the DTMF pcaps of SIPp), then the actions after the
    ACK, then its BYE, or the BYE of the switch when switch_hangs_up."""
    end = SIPP_SWITCH_BYE if switch_hangs_up else SIPP_OWN_BYE
    return """<?xml version="1.0" encoding="ISO-8859-1" ?>
<scenario name="cloonix uac">
  <send retrans="500"><![CDATA[
INVITE sip:%(dial)s@[remote_ip]:[remote_port] SIP/2.0
Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
From: sipp <sip:sipp@[local_ip]:[local_port]>;tag=[pid]SIPpTag00[call_number]
To: <sip:%(dial)s@[remote_ip]:[remote_port]>
Call-ID: [call_id]
CSeq: 1 INVITE
Contact: sip:sipp@[local_ip]:[local_port]
Max-Forwards: 70
Content-Type: application/sdp
Content-Length: [len]

v=0
o=user1 53655765 2353687637 IN IP[local_ip_type] [local_ip]
s=-
c=IN IP[media_ip_type] [media_ip]
t=0 0
m=audio %(port)d RTP/AVP 8 101
a=rtpmap:8 PCMA/8000
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-15
]]></send>
  <recv response="100" optional="true"/>
  <recv response="180" optional="true"/>
  <recv response="183" optional="true"/>
  <recv response="200" rtd="true"/>
  <send><![CDATA[
ACK sip:%(dial)s@[remote_ip]:[remote_port] SIP/2.0
Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
From: sipp <sip:sipp@[local_ip]:[local_port]>;tag=[pid]SIPpTag00[call_number]
To: <sip:%(dial)s@[remote_ip]:[remote_port]>[peer_tag_param]
Call-ID: [call_id]
CSeq: 1 ACK
Contact: sip:sipp@[local_ip]:[local_port]
Max-Forwards: 70
Content-Length: 0
]]></send>
%(actions)s
%(end)s</scenario>
""" % {"dial": dial, "port": media_port, "actions": actions,
       "end": end % {"dial": dial}}


SIPP_OWN_BYE = """  <send retrans="500"><![CDATA[
BYE sip:%(dial)s@[remote_ip]:[remote_port] SIP/2.0
Via: SIP/2.0/[transport] [local_ip]:[local_port];branch=[branch]
From: sipp <sip:sipp@[local_ip]:[local_port]>;tag=[pid]SIPpTag00[call_number]
To: <sip:%(dial)s@[remote_ip]:[remote_port]>[peer_tag_param]
Call-ID: [call_id]
CSeq: 2 BYE
Contact: sip:sipp@[local_ip]:[local_port]
Max-Forwards: 70
Content-Length: 0
]]></send>
  <recv response="200" crlf="true"/>"""

SIPP_SWITCH_BYE = """  <recv request="BYE"/>
  <send><![CDATA[
SIP/2.0 200 OK
[last_Via:]
[last_From:]
[last_To:]
[last_Call-ID:]
[last_CSeq:]
Contact: sip:sipp@[local_ip]:[local_port]
Content-Length: 0
]]></send>"""


def play(path, ms=0):
    """A scenario step: a pcap of RTP played, then a pause of ms if any: a
    scenario waiting for the BYE of the switch must not pause, a BYE that
    comes during a pause fails the call."""
    step = '  <nop><action><exec play_pcap_audio="%s"/></action></nop>\n' % path
    return step + ('  <pause milliseconds="%d"/>\n' % ms if ms else "")


DTMF_EVENTS = "0123456789*#ABCD"


def dtmf_pcap(keys, gap_ms=600):
    """A pcap of RFC 4733 events for SIPp to play (play_pcap_audio): keys
    one every gap_ms, each 7 packets 20 ms apart then 3 end packets, one
    SSRC and sequence numbers and timestamps that only grow, as a phone
    sends them. The pcaps of SIPp, one per digit, restart theirs."""
    out = [struct.pack("<IHHiIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1)]
    seq = 1000
    for i, key in enumerate(keys):
        t0 = i * gap_ms / 1000.0
        for k in range(10):
            end = k >= 7
            dur = 160 * (min(k, 6) + 1)
            rtp = struct.pack("!BBHII", 0x80, (0x80 if k == 0 else 0) | 101,
                              seq, 8000 + int(t0 * 8000), 0x0c1005ee)
            rtp += struct.pack("!BBH", DTMF_EVENTS.index(key),
                               (0x80 if end else 0) | 10, dur)
            udp = struct.pack("!HHHH", 6000, 6000, 8 + len(rtp), 0) + rtp
            ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + len(udp), seq, 0, 64,
                             17, 0, bytes([10, 0, 0, 2]), bytes([10, 0, 0, 1]))
            frame = b"\x00" * 12 + b"\x08\x00" + ip + udp
            t = t0 + 0.02 * min(k, 7)
            out.append(struct.pack("<IIII", int(t), int((t % 1) * 1e6),
                                   len(frame), len(frame)) + frame)
            seq += 1
    return b"".join(out)


def put_dtmf(node, path, keys, gap_ms=600):
    """dtmf_pcap of keys written to path on node."""
    b64 = base64.b64encode(dtmf_pcap(keys, gap_ms)).decode()
    ssh(node, "echo %s | base64 -d > %s" % (b64, path))


def wav_samples(node, path):
    """(rate, samples) of a 16 bits mono wav of node."""
    raw = base64.b64decode(ssh(node, "base64 -w0 %s" % path, check=False).strip() or b"")
    if not raw:
        return 8000, ()
    w = wave.open(io.BytesIO(raw))
    data = w.readframes(w.getnframes())
    return w.getframerate(), struct.unpack("<%dh" % (len(data) // 2), data)


def tone_power(rate, samples, freq):
    """Goertzel: the power of freq in samples, divided by their energy."""
    if not samples:
        return 0.0
    k = 2.0 * math.cos(2.0 * math.pi * freq / rate)
    s1 = s2 = 0.0
    for x in samples:
        s1, s2 = x + k * s1 - s2, s1
    power = s1 * s1 + s2 * s2 - k * s1 * s2
    energy = sum(x * x for x in samples) or 1.0
    return power / (energy * len(samples) / 2.0)


def channelstats(pbx):
    """Per channel of pbx: codec, rx count, rx lost, rx jitter, tx count, from
    pjsip show channelstats: bridge, channel without its PJSIP/, uptime,
    codec, then the numbers, a K after those above 100000."""
    def num(word):
        return int(word[:-1]) * 1000 if word.endswith("K") else int(word)
    rows = {}
    for line in rx(pbx, "pjsip show channelstats").split("\n"):
        f = line.split()
        for i, word in enumerate(f):
            if re.match(r"^\d\d:\d\d:\d\d$", word) and i >= 1 and len(f) > i + 6:
                try:
                    rows[f[i - 1]] = {"codec": f[i + 1], "rx": num(f[i + 2]),
                                      "lost": num(f[i + 3]),
                                      "jitter": float(f[i + 5]),
                                      "tx": num(f[i + 6])}
                except ValueError:
                    pass
                break
    return rows
