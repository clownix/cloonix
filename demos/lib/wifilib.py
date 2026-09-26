"""wifilib - what the wifi demos share, written once.

The radios of a cnt or a kvm come from wlan=N; they are wlan0.. inside
it, and plug puts wlan<i> on an air. The cnt radios start on the air
cntwifi, the kvm radios on kvmwifi. An air is captured by its name:
"cloonix cli <net> sniff <air>" gives radiotap
802.11 frames. Names in capitals read through DEMO (WIRING, RADIOS,
ROOTFS_CHOICES, ALL) are the constants of the demo being run. Standard
library only.
"""
import os
import subprocess
import sys
import threading
import time
import __main__ as DEMO

from demolib import CLOONIX, NET, cli, put, show, ssh, story, trace


def hwsim_on_host():
    """The radios are phys of mac80211_hwsim, a module cloonix never loads."""
    local = os.path.isdir("/var/lib/cloonix/" + NET)
    if local and os.path.isdir("/sys/module") and \
       not os.path.isdir("/sys/module/mac80211_hwsim"):
        sys.exit("the kernel module mac80211_hwsim is not loaded, and the\n"
                 "  radios of wlan= are its phys. cloonix never loads a\n"
                 "  module; load it, then run this demo again:\n"
                 "    sudo modprobe mac80211_hwsim radios=0")


def create_radio_cnt(node, rootfs):
    """One container with its eths and its radios, retried while a name of
    the previous run is still torn down."""
    eth = "eth=%d" % len(DEMO.WIRING[node])
    last = ""
    for attempt in range(15):
        r = cli("add", "cnt", node, eth, rootfs,
                "wlan=%d" % DEMO.RADIOS[node])
        if r.returncode == 0:
            if attempt:
                trace("%s created on attempt %d", node, attempt + 1)
            return
        last = (r.stdout + r.stderr).strip()
        time.sleep(2)
    raise RuntimeError("%s was never created. The server said: %s"
                       % (node, last or "nothing at all"))


def create_radio_kvm(node, rootfs):
    """One kvm with its eths and its radios, if any; eth0 answers ssh
    before the radios exist."""
    eth = "eth=%d" % len(DEMO.WIRING[node])
    args = ["add", "kvm", node, "ram=1000", "cpu=2", eth, rootfs]
    if DEMO.RADIOS.get(node):
        args.append("wlan=%d" % DEMO.RADIOS[node])
    r = cli(*args)
    if r.returncode:
        raise RuntimeError("%s: %s\n  fetch %s into /var/lib/cloonix/bulk "
                           "from clownix.net/downloads/kvm_bulk, or build "
                           "it: cd vm_create/cnt_kvm && ./kvm_trixie.sh"
                           % (node, (r.stdout + r.stderr).strip(), rootfs))


def wait_radios(node, secs=60):
    """The agent of a kvm makes its radios a few seconds after it answers."""
    last = DEMO.RADIOS.get(node, 0) - 1
    if last < 0:
        return
    end = time.time() + secs
    while time.time() < end:
        if "radio" in ssh(node, "test -d /sys/class/net/wlan%d && echo radio"
                          % last, check=False):
            return
        time.sleep(2)
    sys.exit("%s has no wlan%d after %d s: its agent did not make the "
             "radios, the reason is in the syslog of the host (kvmwifi)"
             % (node, last, secs))


def hostapd(node, conf, name="hostapd"):
    """An access point: hostapd on the configuration given, AP-ENABLED."""
    put(node, "/etc/%s.conf" % name, conf)
    ssh(node, "hostapd /etc/%s.conf </dev/null >/tmp/%s.log 2>&1 &"
              % (name, name), check=False)
    for _ in range(40):
        out = ssh(node, "cat /tmp/%s.log 2>/dev/null" % name, check=False)
        if "AP-ENABLED" in out:
            return
        if "Could not" in out or "failed" in out.lower():
            break
        time.sleep(0.5)
    sys.exit("hostapd of %s did not come up:\n%s"
             % (node, ssh(node, "cat /tmp/%s.log" % name, check=False)))


def link(node, iface="wlan0"):
    return ssh(node, "iw dev %s link 2>&1" % iface, check=False)


def wait_link(node, connected, secs, iface="wlan0"):
    end = time.time() + secs
    while time.time() < end:
        if ("Connected to" in link(node, iface)) == connected:
            return True
        time.sleep(2)
    return False


def bssid(node, iface="wlan0"):
    """The access point a station is joined to, or None."""
    for line in link(node, iface).splitlines():
        if line.startswith("Connected to"):
            return line.split()[2]
    return None


def joins(node, iface="wlan0"):
    """The joins of wpa_supplicant with its keys in place: associated is not
    joined, CTRL-EVENT-CONNECTED comes after the 4-way handshake."""
    out = ssh(node, "grep -c CTRL-EVENT-CONNECTED /tmp/wpa_%s.log" % iface,
              check=False).strip()
    return int(out) if out.isdigit() else 0


def wait_joined(node, before, secs, iface="wlan0"):
    """Joined again since joins() gave before."""
    end = time.time() + secs
    while time.time() < end:
        if ("Connected to" in link(node, iface) and
                joins(node, iface) > before):
            return True
        time.sleep(2)
    return False


def station(node, conf, iface="wlan0", secs=40):
    """A station: wpa_supplicant on the configuration given, joined."""
    put(node, "/etc/wpa_%s.conf" % iface, conf)
    ssh(node, "ip link set dev %s up" % iface)
    ssh(node, "rm -f /tmp/wpa_%s.log; wpa_supplicant -B -i %s -D nl80211 "
              "-c /etc/wpa_%s.conf -f /tmp/wpa_%s.log"
        % (iface, iface, iface, iface), check=False)
    if not wait_joined(node, 0, secs, iface):
        sys.exit("%s did not join:\n%s" % (node, ssh(
            node, "tail -n 20 /tmp/wpa_%s.log" % iface, check=False)))


def wifi_state(nodes, iface="wlan0"):
    """On a failure: what each radio, hostapd and wpa_supplicant say, into
    the log of the demo, kept short so that the tail of the log a test
    shows holds every node."""
    for n in nodes:
        story("  --- %s ---" % n)
        for cmd in ("iw dev %s link | head -n 3" % iface,
                    "iw dev %s station dump | grep -E 'Station|inactive|"
                    "tx failed|authorized|associated:'" % iface,
                    "ip -br addr show dev %s; ip neigh show dev %s"
                    % (iface, iface),
                    "tail -n 6 /tmp/hostapd.log /tmp/wpa_%s.log" % iface):
            for line in ssh(n, "(%s) 2>&1" % cmd, check=False).splitlines():
                if "No such file" not in line and "impossible d" not in line:
                    story("    " + line.strip())


def address(node, iface, addr):
    ssh(node, "ip addr add %s dev %s" % (addr, iface))
    ssh(node, "ip link set dev %s up" % iface)


def ping(node, target, count=5):
    """Every echo answered, or the ping printed: a failure explains itself."""
    out = ssh(node, "ping -c %d -W 2 %s 2>&1" % (count, target), check=False)
    ok = " 0% packet loss" in out
    if not ok:
        story("  ping from %s to %s, not every echo answered:" % (node, target))
        for ln in (out.strip() or "no output from the ssh").splitlines():
            story("  | " + ln)
    return ok, out


def air(name, secs, during=None):
    """The 802.11 frames of an air while during() runs, radiotap cut off.
    An air is born not captured: cnf <air> sniff=on."""
    cli("cnf", name, "sniff=on")
    cmd = ["cli", NET, "sniff", name, "--line"]
    show("cloonix " + " ".join(cmd))
    p = subprocess.Popen(CLOONIX + cmd, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, text=True,
                         stdin=subprocess.DEVNULL)
    lines = []
    reader = threading.Thread(target=lambda: lines.extend(p.stdout),
                              daemon=True)
    reader.start()
    try:
        time.sleep(1)
        if during:
            during()
        time.sleep(secs)
    finally:
        p.terminate()
    try:
        err = p.communicate(timeout=5)[1] or ""
    except subprocess.TimeoutExpired:
        p.kill()
        err = ""
    reader.join(2)
    if not any(ln.startswith("hdr ") for ln in lines):
        sys.exit("the air %s is not captured: %s"
                 % (name, err.strip() or "no answer"))
    frames = []
    for ln in lines:
        f = ln.split()
        if len(f) >= 5 and f[0] == "pkt":
            b = bytes.fromhex(f[4])
            rt = b[2] | (b[3] << 8) if len(b) > 4 else len(b)
            frames.append(b[rt:])
    return frames


def kind(frame):
    """(type, subtype) of an 802.11 frame: 0 management, 2 data."""
    return ((frame[0] >> 2) & 3, frame[0] >> 4) if frame else (-1, -1)


def payload(frame):
    """The ethertype and the bytes after LLC/SNAP of a clear data frame."""
    t, sub = kind(frame)
    if t != 2 or (frame[1] & 0x40) or (sub & 4):
        return None, b""
    off = 24 + (6 if (frame[1] & 3) == 3 else 0) + (2 if sub & 8 else 0)
    snap = frame[off:off + 8]
    if len(snap) < 8 or snap[:3] != b"\xaa\xaa\x03":
        return None, b""
    return (snap[6] << 8) | snap[7], frame[off + 8:]


def count_icmp(frames):
    return sum(1 for f in frames
               if payload(f)[0] == 0x0800 and len(payload(f)[1]) > 9
               and payload(f)[1][9] == 1)


def count_eapol(frames):
    return sum(1 for f in frames if payload(f)[0] == 0x888e)


def count_protected(frames):
    return sum(1 for f in frames if kind(f)[0] == 2 and (f[1] & 0x40))


def count_mgmt(frames, subtype):
    return sum(1 for f in frames if kind(f) == (0, subtype))


def count_sae(frames):
    """Authentication frames of algorithm 3, SAE: commit and confirm."""
    return sum(1 for f in frames if kind(f) == (0, 11) and len(f) >= 26
               and (f[24] | (f[25] << 8)) == 3)
