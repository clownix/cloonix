#!/usr/bin/env python3
"""sub_airchurn - an access point and a station of mac80211_hwsim radios
stay associated, and a ping between them loses nothing, while the net adds
and deletes containers: the medium of the airs is not held by the commands
suid-power runs for the containers (crun, umount, ip netns), nor by the iw
of a radio of another container. hostapd and wpa_supplicant as in
wifi/cnt_open.py, on an air of their own; the churn is two containers with
no radio, one with a radio, and one added and deleted at once as the image
probe of a demo does. The module is the host's: loaded here with sudo when
absent, the test is skipped without it.

    ./sub_airchurn.py [net] [peer]          defaults: nemo mito
"""
import os
import re
import subprocess
import time
from sublib import *

AP, STA = "tiacAP", "tiacSTA"
CHURN = ("tiacX0", "tiacX1", "tiacX2", "tiacX3")
AIR = "tiacair"
AP_IP, STA_IP = "192.168.77.1", "192.168.77.10"
PING_SECS = 60
# the old medium in suid-power: 561 ms idle; its own process: 28 ms
WORST_MS = 200

AP_CONF = ("interface=wlan0\\ndriver=nl80211\\nssid=tiac-open\\nhw_mode=g\\n"
           "channel=1\\nauth_algs=1\\n")
STA_CONF = "network={\\n    ssid=\"tiac-open\"\\n    key_mgmt=NONE\\n}\\n"


def module_loaded():
    if os.path.isdir("/sys/module/mac80211_hwsim"):
        return True
    if not sudo_ok():
        print("      (mac80211_hwsim not loaded and no sudo: not checked)")
        return False
    r = sudo("modprobe", "mac80211_hwsim", "radios=0")
    said = (r.stderr or "") + (r.stdout or "")
    if (r.returncode != 0 and "not found" in said
            and "command not found" not in said):
        print("      (no mac80211_hwsim module in this kernel: not checked)")
        return False
    must(r, "modprobe")
    return True


def linked(node):
    return "Connected to" in ssh(node, "iw dev wlan0 link 2>&1", check=False,
                                 quiet=True)


def ap_sta_up():
    ssh(AP, "printf '%s' > /etc/hostapd.conf" % AP_CONF)
    ssh(AP, "hostapd /etc/hostapd.conf </dev/null >/tmp/hostapd.log 2>&1 &",
        check=False)
    end = time.time() + 20
    while "AP-ENABLED" not in ssh(AP, "cat /tmp/hostapd.log", check=False,
                                  quiet=True):
        if time.time() > end:
            raise Fail("hostapd of %s did not come up:\n%s"
                       % (AP, ssh(AP, "cat /tmp/hostapd.log", check=False)))
        time.sleep(0.5)
    ssh(STA, "printf '%s' > /etc/wpa_wlan0.conf" % STA_CONF)
    ssh(STA, "ip link set dev wlan0 up")
    ssh(STA, "wpa_supplicant -B -i wlan0 -D nl80211 -c /etc/wpa_wlan0.conf "
             "-f /tmp/wpa_wlan0.log", check=False)
    end = time.time() + 40
    while not linked(STA):
        if time.time() > end:
            raise Fail("%s did not join %s:\n%s" % (STA, AP, ssh(
                STA, "tail -n 20 /tmp/wpa_wlan0.log", check=False)))
        time.sleep(1)
    for node, addr in ((AP, AP_IP), (STA, STA_IP)):
        ssh(node, "ip addr add %s/24 dev wlan0" % addr)
        ssh(node, "ip link set dev wlan0 up")
    end = time.time() + 20
    while not ping(STA, AP_IP, 3, check=False):
        if time.time() > end:
            raise Fail("%s does not ping %s before the churn" % (STA, AP))
        time.sleep(1)


def churn():
    """The containers come and go as in a run of demos."""
    add_cnt(CHURN[0], "v")
    add_cnt(CHURN[1], "v")
    must(cli("add", "cnt", CHURN[2], "eth=1", CNT_ROOTFS, "wlan=1"),
         "add cnt %s with a radio" % CHURN[2])
    for n in CHURN[:3]:
        wait_answer(n)
    # the image probe of a demo: added, deleted at once
    must(cli("add", "cnt", CHURN[3], "eth=1", CNT_ROOTFS), "add cnt probe")
    must(cli("del", CHURN[3]), "del probe")
    for n in CHURN[:3]:
        must(cli("del", n), "del %s" % n)
    for n in CHURN:
        wait_topo(n, False)


def sub_airchurn():
    if not module_loaded():
        return
    clean(AP, STA, *CHURN)
    try:
        for n in (AP, STA):
            must(cli("add", "cnt", n, "eth=1", CNT_ROOTFS, "wlan=1"),
                 "add cnt %s with a radio" % n)
        for n in (AP, STA):
            wait_answer(n)
            must(cli("plug", n, "wlan0", AIR), "plug %s wlan0 %s" % (n, AIR))
        ap_sta_up()
        cmd = ("timeout -s INT %d ping -i 0.2 -W 1 %s 2>&1"
               % (PING_SECS, AP_IP))
        show('cloonix cli %s ssh %s "%s" &' % (NET, STA, cmd))
        p = subprocess.Popen(CLOONIX + ["cli", NET, "ssh", STA, cmd],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, stdin=subprocess.DEVNULL)
        t0 = time.time()
        try:
            time.sleep(2)
            churn()
            took = time.time() - t0
            if took > PING_SECS - 10:
                raise Fail("the churn took %.0fs, the ping lasts %ds: it "
                           "did not cover it all" % (took, PING_SECS))
            out, _ = p.communicate(timeout=PING_SECS + 30)
        finally:
            if p.poll() is None:
                p.kill()
        m = re.search(r"([\d.]+)% packet loss", out)
        r = re.search(r"= [\d.]+/[\d.]+/([\d.]+)/", out)
        if not m or not r:
            raise Fail("the ping over the air gave:\n%s" % out[-600:])
        loss, worst = float(m.group(1)), float(r.group(1))
        print("      churn of %d containers in %.0fs; ping over the air: "
              "%g%% loss, worst rtt %.0f ms" % (len(CHURN), took, loss, worst))
        beacon = ssh(STA, "grep -c BEACON-LOSS /tmp/wpa_wlan0.log; true",
                     check=False, quiet=True).split()
        lost = int(beacon[-1]) if beacon and beacon[-1].isdigit() else -1
        if loss or worst > WORST_MS or lost:
            raise Fail("the air stalled while containers came and went: "
                       "%g%% loss, worst rtt %.0f ms, %d BEACON-LOSS, "
                       "%s %s\n%s"
                       % (loss, worst, lost, STA,
                          "still joined" if linked(STA) else "not joined",
                          ssh(STA, "tail -n 8 /tmp/wpa_wlan0.log",
                              check=False)))
        if not linked(STA):
            raise Fail("%s is no longer joined to %s after the churn"
                       % (STA, AP))
    finally:
        clean(AP, STA, *CHURN)


if __name__ == "__main__":
    run("airchurn", sub_airchurn)
