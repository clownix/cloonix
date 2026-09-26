#!/usr/bin/env python3
r"""cnt_wpa2 - WPA2 between an access point and two stations, and a second air.

    ./cnt_wpa2.py [net]            the net defaults to nemo

Three machines and no lan at all::

      ap            sta1          sta2
   192.168.5.1   192.168.5.10  192.168.5.11
       |              |             |
       +------- the air ------------+

This one is deliberately unlike the others in the set, in two ways.

There is NO WIRE. cloonix gives a container a wifi interface with
wlan=N: the switch asks the kernel for a fresh mac80211_hwsim radio and
moves it into the container's network namespace, where it appears as wlan0.
Frames between these machines never cross an ovs lan: they cross an air,
of which cloonix is the medium, and the air is captured by its name.

And its purpose is different. The other demos explain a protocol that is
known to work. This one is a TEST: it asks whether the wifi support in this
build of cloonix does what it claims, on this kernel, and says clearly at
each step whether it did.

  ACT 1  do the radios exist at all? Three containers asked for one wifi
         interface each, and iw is asked whether it got one.
  ACT 2  hostapd turns one of them into an access point.
  ACT 3  the other two scan, and the question is whether they can hear it.
         This is the act that proves the simulated air is shared, which is
         the part most likely not to work.
  ACT 4  they associate with WPA2 and get addresses, and then ping the
         access point and each other.

Needs cloonix on the PATH and nothing else. Standard library only, like
cloonix itself, so it runs wherever the client does.

Image: trixie, or any of the trixie demo images
Size:  3 containers, no lan at all

THE PACKET TO CATCH
-------------------
  The air itself. cloonix is the medium of the radios of the net: every
  frame goes through it, to the radios on the same air only, and each air
  is captured by its name. A radio starts on the air "cntwifi", the amber lan
  of the canvas, captured as "cntwifi 0".

      cloonix cli <net> cnf cntwifi sniff=on
      cloonix cli <net> sniff cntwifi

  ACT 5 counts the beacons of the access point in that capture. In
  webshark (double-click the air cntwifi) the 4-way handshake shows as four
  EAPOL frames, Message 1 of 4 to Message 4 of 4.

  ACT 6 puts sta2 on a second air, where the access point cannot be
  heard, then back on cntwifi, where it joins again: an air is a lan of wlans,
  plugged with plug and unplug like any lan.


Important
  This is a TEST of this build of cloonix on this kernel, and it says clearly
  at each step whether it passed. A failure here is a cloonix or kernel
  finding, not a protocol lesson.
"""
import os
import subprocess
import sys
import threading
import time

# What every demo shares is in demos/lib/demolib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    ACK_FAST, CLOONIX, NET, cli, erase_net, in_parallel, net_answers, show,
    step, story, trace, trace_ssh, wait_ready)

NET_IS_LOCAL = os.path.isdir("/var/lib/cloonix/" + NET)

ROOTFS_CHOICES = ("trixie", "wg_trixie", "mqtt_trixie", "proxy_trixie",
                  "dns_trixie", "frr_trixie")

AP, S1, S2 = "ap", "sta1", "sta2"
STATIONS = (S1, S2)
ALL = (AP, S1, S2)

VWIFI = "wlan=1"

ETH = "eth=1"

SSID = "cloonix-wifi"
PSK = "cloonixdemo"
CHANNEL = 1

WIFI_IP = {AP: "192.168.5.1", S1: "192.168.5.10", S2: "192.168.5.11"}
MASK = "24"

def ssh(node, cmd, check=True, log=None):
    trace_ssh("ssh %s: %s", node, cmd if len(cmd) < 70 else cmd[:67] + "...")
    show('cloonix cli %s ssh %s "%s"' % (NET, node, cmd))
    r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node, cmd],
                       capture_output=True, text=True,
                       stdin=subprocess.DEVNULL)
    if r.returncode:
        trace_ssh("ssh %s: returned %d", node, r.returncode)
    if check and r.returncode:
        said = (r.stderr or r.stdout).strip()
        if not said and log:
            said = ssh(node, "tail -n 20 %s 2>/dev/null" % log,
                       check=False).strip()
        raise RuntimeError("%s: %s\n  -> %s" %
                           (node, cmd, said or "no output anywhere"))
    return r.stdout

def put(node, path, text):
    """Write a file into a guest with printf.

    printf reads its first argument as a FORMAT, and that is the whole
    difficulty. Three things in it are not literal:

      %   a conversion. Refused outright below.
      \'   ends the shell argument. Refused too.
      \\   an escape. printf turns \\r into a carriage return, \\t into a tab,
          \\n into a newline -- so any backslash in the text has to be
          doubled before it is sent, or the file that arrives is not the
          file that was meant.

    That last one is not hypothetical. A python script containing
    f.write("\\r\\n") arrived with a real carriage return in the middle of a
    string literal, and the guest reported an unterminated string on a line
    that looked perfectly fine in the source here.

    So: backslashes are doubled FIRST, and only then are real newlines
    turned into the escape that printf will turn back.
    """
    if "'" in text or "%" in text:
        raise RuntimeError("%s cannot be written with printf as it stands"
                           % path)
    trace_ssh("writing %s on %s, %d bytes", path, node, len(text))
    escaped = text.replace("\\", "\\\\").replace("\n", "\\n")
    ssh(node, "printf '%s' > %s" % (escaped, path))

def create_cnt(node, rootfs):
    """Create one container, and check that the server took it.

    Two things made this worth its own function.

    A create issued with the short ack does not report a refusal: the client
    waits a third of a second, hears nothing, and calls that success. So a
    container the server would not make looked created, and the failure
    surfaced ninety seconds later as "never answered" -- which points at
    booting, not at a request that was turned down.

    And it can legitimately be refused for a moment. erase_net waits until
    the old machines stop answering ssh, but ssh stops answering well before
    a container is finished being torn down, so a create issued straight
    afterwards can land on a name the server is still disposing of. Hence
    retries: the full ack so a refusal is seen, and a few goes so a tail end
    of the previous run is ridden out rather than treated as fatal.
    """
    last = ""
    for attempt in range(6):
        r = cli("add", "cnt", node, ETH, rootfs, VWIFI)
        if r.returncode == 0:
            if attempt:
                trace("%s created on attempt %d", node, attempt + 1)
            return
        last = (r.stdout + r.stderr).strip()
        time.sleep(2)
    raise RuntimeError("%s was never created. The server said: %s"
                       % (node, last or "nothing at all"))

def choose_rootfs():
    up, why = net_answers()
    if not up:
        sys.exit("no answer from a cloonix net called '%s'.\n"
                 "  the client said: %s\n"
                 "  A demo takes the net name as its first argument and\n"
                 "  falls back to nemo:   ./cnt_wpa2.py <net>"
                 % (NET, why or "nothing at all"))
    said = ""
    for cand in ROOTFS_CHOICES:
        trace("probing the server with a throwaway cnt on %s", cand)
        r = cli("add", "cnt", "probe_rootfs", "eth=1", cand, ack=240)
        if r.returncode == 0:
            cli("del", "probe_rootfs", ack=ACK_FAST)
            return cand
        said = (r.stdout + r.stderr).strip()
    cli("del", "probe_rootfs", ack=ACK_FAST)
    sys.exit("the server would not start a container on any of %s.\n"
             "  it said: %s\n"
             "  any image will do.\n"
             "  fetch one:\n"
             "    cd /var/lib/cloonix/bulk\n"
             "    wget https://clownix.net/downloads/cnt_bulk/"
             "trixie.tar.gz\n"
             "    (cloonix extracts it at the first use)\n"
             "  or build one:\n"
             "    cd vm_create/cnt_kvm && ./doitall.sh --cnt trixie"
             % (", ".join(ROOTFS_CHOICES), said or "nothing"))

def tools_present(node, *tools):
    for t in tools:
        r = subprocess.run(CLOONIX + ["cli", NET, "ssh", node,
                            "command -v %s >/dev/null 2>&1" % t],
                           capture_output=True, stdin=subprocess.DEVNULL)
        if r.returncode:
            sys.exit("%s has no %s: this image has no wifi tools.\n"
                     "  any image will do.\n"
                     "  fetch one:\n"
                     "    cd /var/lib/cloonix/bulk\n"
                     "    wget https://clownix.net/downloads/cnt_bulk/"
                     "trixie.tar.gz\n"
                     "    (cloonix extracts it at the first use)\n"
                     "  or build one:\n"
                     "    cd vm_create/cnt_kvm && ./doitall.sh --cnt trixie"
                     % (node, t))

def radios_present(node):
    """Did this container actually get a wifi interface?

    The whole demo turns on this, so it is asked first and answered plainly.
    A missing wlan0 means wlan= did not work, which is a completely
    different problem from an access point that will not start, and there is
    no point going on to the second if the first has failed.
    """
    dev = ssh(node, "iw dev 2>&1", check=False)
    has = "wlan0" in dev
    trace("%s: %s", node, "wlan0 is there" if has else "NO WIFI INTERFACE")
    return has, dev

def wifi_up(node):
    """Bring wlan0 up, which nothing else will do.

    A radio arrives DOWN. iw will not scan on a down interface -- it answers
    "Network is down" and finds nothing -- and hostapd brings its own up but
    a station's stays down until somebody says otherwise. Leaving this out
    made act 3 look like the stations could not hear the access point, when
    in fact they had not switched their receivers on.
    """
    ssh(node, "ip link set dev wlan0 up")
    state = ssh(node, "ip -brief link show wlan0 2>&1", check=False).strip()
    admin_up = ",UP>" in state.replace(" ", "")
    if admin_up:
        trace("%s: wlan0 is up (no carrier yet, which is what a station"
              " looks like before it associates)", node)
    else:
        trace("%s: wlan0 would NOT come up: %s", node, state)
    return admin_up

def phy_info(node):
    """What the radio says it can do. Interesting once, and the fastest way
    to tell a real driver problem from a configuration mistake."""
    out = ssh(node, "iw phy 2>/dev/null | head -n 20", check=False)
    return out

HOSTAPD_CONF = """interface=wlan0
driver=nl80211
ssid=%s
hw_mode=g
channel=%d
auth_algs=1
wpa=2
wpa_passphrase=%s
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
""" % (SSID, CHANNEL, PSK)

WPA_CONF = """network={
    ssid="%s"
    psk="%s"
    key_mgmt=WPA-PSK
}
""" % (SSID, PSK)

def hostapd_start():
    put(AP, "/etc/hostapd.conf", HOSTAPD_CONF)
    ssh(AP, "pkill -x hostapd 2>/dev/null; true", check=False)
    # Backgrounded by the shell, not -B, so its complaints land in a readable file.
    ssh(AP, "hostapd /etc/hostapd.conf </dev/null >/tmp/hostapd.log 2>&1 &",
        check=False)
    for _ in range(30):
        out = ssh(AP, "cat /tmp/hostapd.log 2>/dev/null", check=False)
        if "AP-ENABLED" in out:
            trace("hostapd says AP-ENABLED on channel %d", CHANNEL)
            return True, out
        if "Could not" in out or "failed" in out.lower():
            return False, out
        time.sleep(0.5)
    return False, ssh(AP, "cat /tmp/hostapd.log", check=False)

def scan_for_ssid(node, tries=6):
    """Can this station HEAR the access point?

    The act most likely to fail, and the one worth the demo existing. Every
    mac80211_hwsim radio on a machine shares one simulated medium, so an
    access point in one container should be audible in another -- but that
    is a property of the kernel module and the namespaces, not something
    cloonix can promise, and it is exactly what this asks.
    """
    last = ""
    for attempt in range(tries):
        # The raw output, not a grep of it.
        last = ssh(node, "iw dev wlan0 scan 2>&1 | head -n 40", check=False)
        if SSID in last:
            return True, last
        if "Network is down" in last:
            return False, last
        trace_ssh("%s: scan %d of %d found nothing yet", node,
                  attempt + 1, tries)
        time.sleep(2)
    return False, last

def associate(node):
    put(node, "/etc/wpa_supplicant.conf", WPA_CONF)
    ssh(node, "pkill -x wpa_supplicant 2>/dev/null; true", check=False)
    ssh(node, "rm -f /tmp/wpa.log; wpa_supplicant -B -i wlan0 -D nl80211 "
              "-c /etc/wpa_supplicant.conf -f /tmp/wpa.log </dev/null",
        check=False)
    # associated is not joined: the keys come after, with the 4-way
    # handshake, and CTRL-EVENT-CONNECTED says they are in place
    if wait_joined(node, 0, 30):
        trace("%s associated, keys in place", node)
        return True, link(node)
    return False, ssh(node, "cat /tmp/wpa.log 2>/dev/null", check=False)

def address_wifi(node):
    ssh(node, "ip addr flush dev wlan0", check=False)
    ssh(node, "ip addr add %s/%s dev wlan0" % (WIFI_IP[node], MASK))
    ssh(node, "ip link set dev wlan0 up")

def ping(node, target, count=5):
    out = ssh(node, "ping -c %d -W 2 %s 2>&1" % (count, target), check=False)
    return (" 0% packet loss" in out), out

LAYOUT = {
    AP: (-95, 0),
    S1: (95, -75),
    S2: (95, 75),
    "cntwifi": (0, 0),
}

ETHS = {
    (AP, "eth0"): 180,
    (S1, "eth0"): 180,
    (S2, "eth0"): 180,
}

def place():
    for name, (x, y) in LAYOUT.items():
        cli("cnf", name, "x=%s" % x, "y=%s" % y, ack=ACK_FAST)
    for (name, num), deg in ETHS.items():
        cli("cnf", name, num, "angle=%s" % deg, ack=ACK_FAST)

AIR_SECS = 6

def air_beacons():
    """ACT 5: the beacons of the access point, seen in the capture of the air."""
    step("act 5: the air, captured by cloonix")
    story()
    cli("cnf", "cntwifi", "sniff=on")
    cmd = ["cli", NET, "sniff", "cntwifi", "--line"]
    show("cloonix " + " ".join(cmd))
    p = subprocess.Popen(CLOONIX + cmd, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, text=True,
                         stdin=subprocess.DEVNULL)
    lines = []
    reader = threading.Thread(target=lambda: lines.extend(p.stdout),
                              daemon=True)
    reader.start()
    time.sleep(AIR_SECS)
    p.terminate()
    try:
        err = p.communicate(timeout=5)[1] or ""
    except subprocess.TimeoutExpired:
        p.kill()
        err = ""
    reader.join(2)
    beacons = total = 0
    for line in lines:
        f = line.split()
        if len(f) < 5 or f[0] != "pkt":
            continue
        total += 1
        b = bytes.fromhex(f[4])
        if len(b) > 4:
            rt = b[2] | (b[3] << 8)
            if len(b) > rt and b[rt] == 0x80:
                beacons += 1
    if not any(l.startswith("hdr ") for l in lines):
        story("  nothing captures the air: %s" % (err.strip().split("\n")[-1]
              if err.strip() else "no answer"))
        sys.exit("\nthe air cntwifi is not captured")
    story("  %d frames in %d s, %d of them beacons of %s" %
          (total, AIR_SECS, beacons, SSID))
    story()
    if beacons == 0:
        sys.exit("\nthe air is captured but no beacon of the access point"
                 " is in it")

AIR2 = "air2"

def link(node):
    return ssh(node, "iw dev wlan0 link 2>&1", check=False)

def joins(node):
    """The joins with the keys in place: CTRL-EVENT-CONNECTED of the log."""
    out = ssh(node, "grep -c CTRL-EVENT-CONNECTED /tmp/wpa.log",
              check=False).strip()
    return int(out) if out.isdigit() else 0

def wait_joined(node, before, secs):
    end = time.time() + secs
    while time.time() < end:
        if "Connected to" in link(node) and joins(node) > before:
            return True
        time.sleep(2)
    return False

def wait_link(node, connected, secs):
    end = time.time() + secs
    while time.time() < end:
        if ("Connected to" in link(node)) == connected:
            return True
        time.sleep(2)
    return False

def second_air():
    """ACT 6: sta2 onto a second air, out of reach of the access point,
    then back on cntwifi, where it joins again."""
    step("act 6: a second air, then back")
    story()
    cli("cnf", AIR2, "x=190", "y=75", ack=ACK_FAST)
    r = cli("plug", S2, "wlan0", AIR2)
    if r.returncode:
        sys.exit("\nwlan0 of %s would not go on the air %s: %s"
                 % (S2, AIR2, (r.stdout + r.stderr).strip()))
    lost = wait_link(S2, False, 30)
    ok, out = ping(S2, WIFI_IP[AP], 2)
    story("  %s on %s: %s, ping of the access point %s" %
          (S2, AIR2, "disconnected" if lost else "STILL CONNECTED",
           "fails, as it must" if not ok else "PASSES"))
    if (not lost) or ok:
        sys.exit("\n%s still reaches the access point from another air" % S2)
    before = joins(S2)
    r = cli("plug", S2, "wlan0", "cntwifi")
    if r.returncode:
        sys.exit("\nwlan0 of %s would not come back on cntwifi: %s"
                 % (S2, (r.stdout + r.stderr).strip()))
    back = wait_joined(S2, before, 60)
    ok, out = ping(S2, WIFI_IP[AP], 5)
    story("  %s back on cntwifi: %s, ping of the access point %s" %
          (S2, "joined again" if back else "NOT JOINED", "OK" if ok else "FAILS"))
    story()
    if not (back and ok):
        sys.exit("\n%s did not join the access point again on cntwifi" % S2)

def main():
    # The radios come from a kernel module of the machine running the net,
    # never loaded by cloonix.
    if os.path.isdir("/sys/module") and \
       not os.path.isdir("/sys/module/mac80211_hwsim") and \
       NET_IS_LOCAL:
        sys.exit("the kernel module mac80211_hwsim is not loaded, and the\n"
                 "  radios of wlan= are its phys. cloonix never loads a\n"
                 "  module; load it, then run this demo again:\n"
                 "    sudo modprobe mac80211_hwsim radios=0")
    step("looking for an image with the wifi tools in it")
    rootfs = choose_rootfs()
    story("rootfs %s: an access point and two stations, and no lan" % rootfs)
    story("""
        ap            sta1          sta2
     192.168.5.1   192.168.5.10  192.168.5.11
         |              |             |
         +------- the air ------------+

    cloonix gives each container a wifi interface with wlan=1: the
    switch asks the kernel for a mac80211_hwsim radio and moves it into
    the container, where it appears as wlan0.

    There is no lan here and nothing to sniff. Every act below is judged
    by what iw, hostapd and wpa_supplicant report about themselves.
""")

    step("erasing the net, then placing the items")
    erase_net()
    place()

    step("creating the three containers, each asking for one radio")
    in_parallel(lambda n: create_cnt(n, rootfs), list(ALL), "add cnt")

    step("waiting for them to answer")
    in_parallel(wait_ready, list(ALL), "wait_ready")
    tools_present(AP, "hostapd", "iw")
    tools_present(S1, "wpa_supplicant", "iw")

    step("act 1: did the radios arrive?")
    story()
    ok_radios = True
    for n in ALL:
        got, dev = radios_present(n)
        ok_radios = ok_radios and got
        if not got:
            story("  %s has no wlan0. What iw dev says:" % n)
            for line in dev.rstrip().split("\n")[:10]:
                story("      " + line)
    story()
    if not ok_radios:
        story("  wlan= did not produce an interface. A radio is a phy of")
        story("  the kernel module mac80211_hwsim on the machine running the")
        story("  net, and cloonix never loads a module: it is loaded by hand,")
        story("  and a container created before that got no radio:")
        story("        lsmod | grep mac80211_hwsim")
        story("        sudo modprobe mac80211_hwsim radios=0")
        story("  then run this demo again. The reason is also in the log of")
        story("  suid_power there:")
        story("        grep -i hwsim /var/log/syslog")
        sys.exit("\nno wifi interfaces, so nothing below can work")
    ok_up = True
    for n2 in ALL:
        ok_up = wifi_up(n2) and ok_up
    if not ok_up:
        sys.exit("\na wlan0 would not come up, so nothing below can work")
    story()
    story("  all three have a wlan0 and all three are up. What the radio")
    story("  says it can do:")
    for line in phy_info(AP).rstrip().split("\n")[:12]:
        story("      " + line)
    story()

    step("act 2: hostapd turns one of them into an access point")
    story()
    ok_ap, hlog = hostapd_start()
    for line in hlog.rstrip().split("\n")[-8:]:
        story("      " + line)
    story()
    if not ok_ap:
        sys.exit("\nhostapd did not come up,"
                 " see its log in the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")
    address_wifi(AP)
    story("  what the interface looks like now:")
    for line in ssh(AP, "iw dev wlan0 info", check=False).rstrip().split("\n"):
        story("      " + line)
    story()
    story("    type AP is the thing to look for. The same radio was type")
    story("    managed a moment ago -- a station -- and hostapd changed it.")
    story()

    step("act 3: can the stations hear it?")
    story()
    ok_scan = True
    for n in STATIONS:
        found, out = scan_for_ssid(n)
        ok_scan = ok_scan and found
        story("  %s scanning:" % n)
        for line in out.rstrip().split("\n")[:8]:
            story("      " + line.strip())
        story("      -> %s" % ("heard %s" % SSID if found
                               else "did NOT hear it"))
        story()
    if not ok_scan:
        story("    If the output above says \"Network is down\", the")
        story("    interface is not up and that is all this is.")
        story()
        story("    Otherwise it is the interesting failure. Every")
        story("    mac80211_hwsim radio on a machine shares a single")
        story("    simulated medium, so an access point in one container")
        story("    should be audible in another. If it is not, the radios")
        story("    exist but the air between them does not, and that is a")
        story("    kernel or namespace question rather than a cloonix one.")
        sys.exit("\nthe stations could not hear the access point")

    step("act 4: associate, address, and talk")
    story()
    ok_assoc = True
    for n in STATIONS:
        joined, link = associate(n)
        ok_assoc = ok_assoc and joined
        story("  %s:" % n)
        for line in link.rstrip().split("\n")[:8]:
            story("      " + line.strip())
        story()
        if joined:
            address_wifi(n)
    if not ok_assoc:
        sys.exit("\nassociation failed,"
                 " see the wpa_supplicant log in the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")

    # One station entry is about twenty lines: no head here.
    story("  what the access point thinks is connected to it:")
    dump = ssh(AP, "iw dev wlan0 station dump 2>&1", check=False)
    stations = []
    cur = None
    for line in dump.split("\n"):
        t = line.strip()
        if t.startswith("Station "):
            cur = {"mac": t.split()[1], "signal": "?", "rx": "?", "auth": "?"}
            stations.append(cur)
        elif cur and t.startswith("signal:"):
            cur["signal"] = t.split(":", 1)[1].strip()
        elif cur and t.startswith("rx bitrate:"):
            cur["rx"] = t.split(":", 1)[1].strip()
        elif cur and t.startswith("authorized:"):
            cur["auth"] = t.split(":", 1)[1].strip()
    if stations:
        for st in stations:
            story("      %-20s signal %-10s rx %-14s authorized %s"
                  % (st["mac"], st["signal"], st["rx"], st["auth"]))
        story("      -> %d station(s) associated" % len(stations))
    else:
        story("      (none, which contradicts the association above)")
        story(dump)
    story()
    if len(stations) < len(STATIONS):
        story("    fewer stations than expected. An access point holds one")
        story("    entry per client, so this is worth looking at.")
        story()

    results = []
    for n in STATIONS:
        ok, out = ping(n, WIFI_IP[AP])
        results.append(ok)
        story("  %s ping the access point: %s" % (n, "OK" if ok else "FAIL"))
        if not ok:
            for line in out.rstrip().split("\n")[-4:]:
                story("      " + line)
    ok, out = ping(S1, WIFI_IP[S2])
    results.append(ok)
    story("  %s ping %s, which goes through the access point: %s"
          % (S1, S2, "OK" if ok else "FAIL"))
    if not ok:
        for line in out.rstrip().split("\n")[-4:]:
            story("      " + line)
        story("      (station to station traffic is relayed by the access")
        story("       point. hostapd does that by default; ap_isolate=1")
        story("       would stop it, and that is the first thing to check.)")
    story()

    if not all(results):
        sys.exit("\nthe wifi came up but traffic did not pass,"
                 " see the history"
                 "\n  the history says where: on a terminal, CLOONIX_DEMO_STORY=1 prints it")

    air_beacons()
    second_air()

    story("""
IT WORKS. Three containers, three simulated radios, WPA2 between them, and
traffic passing over the air with no lan involved.

the network is left up. Things worth trying:

  watch the access point's view of its clients change as you take one away:
    cloonix cli %s ssh %s "iw dev wlan0 station dump"
    cloonix cli %s ssh %s "pkill wpa_supplicant"

  see the signal and the negotiated rate, which hwsim invents but reports
  the way a real driver would:
    cloonix cli %s ssh %s "iw dev wlan0 link"

  and watch the air: every frame between the radios, the handshake too:
    cloonix cli %s cnf cntwifi sniff=on
    cloonix cli %s sniff cntwifi
  or double-click the air cntwifi on the canvas for webshark.
""" % (NET, AP, NET, S1, NET, S1, NET, NET))

main()
