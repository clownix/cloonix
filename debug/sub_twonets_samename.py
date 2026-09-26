#!/usr/bin/env python3
"""sub_twonets_samename - the same names on the net and on its peer at once.
A mac is derived from the net and the name, so cnt1 of one net and cnt1 of
the other differ on eth0 and on wlan0; the radios of both nets are phys of
the one mac80211_hwsim of the host, and an access point and a station of
the same names on the two nets each join on their own net, both at the same
time, as two demos played in parallel do.
"""
import time
from sublib import *
from sub_cntwifi import module_loaded

AP, STA = "tisubSnAp", "tisubSnSta"
AP_CONF = ("interface=wlan0\\ndriver=nl80211\\nssid=%s\\nhw_mode=g\\n"
           "channel=1\\nauth_algs=1\\n")
STA_CONF = "network={\\n    ssid=\"%s\"\\n    key_mgmt=NONE\\n}\\n"


def both(fn):
    for net in (NET, PEER):
        fn(net)


def clean_both():
    for net in (NET, PEER):
        for n in (AP, STA):
            if in_topo(n, net):
                cli("del", n, net=net, quiet=True)
        for n in (AP, STA):
            wait_topo(n, False, net=net)


def mac(node, iface, net):
    return ssh(node, "cat /sys/class/net/%s/address" % iface, net=net).strip()


def ap_up(net):
    ssid = "tisub-%s" % net
    ssh(AP, "printf '%s' > /etc/hostapd.conf" % (AP_CONF % ssid), net=net)
    ssh(AP, "hostapd /etc/hostapd.conf </dev/null >/tmp/hostapd.log 2>&1 &",
        net=net, check=False)
    for _ in range(40):
        if "AP-ENABLED" in ssh(AP, "cat /tmp/hostapd.log", net=net,
                               check=False, quiet=True):
            return
        time.sleep(0.5)
    raise Fail("hostapd of %s on %s did not come up:\n%s" % (
        AP, net, ssh(AP, "cat /tmp/hostapd.log", net=net, check=False)))


def sta_start(net):
    ssid = "tisub-%s" % net
    ssh(STA, "printf '%s' > /etc/wpa_wlan0.conf" % (STA_CONF % ssid), net=net)
    ssh(STA, "ip link set dev wlan0 up", net=net)
    ssh(STA, "wpa_supplicant -B -i wlan0 -D nl80211 -c /etc/wpa_wlan0.conf "
             "-f /tmp/wpa_wlan0.log", net=net, check=False)


def joined(net):
    return "Connected to" in ssh(STA, "iw dev wlan0 link 2>&1", net=net,
                                 check=False, quiet=True)


def sub_twonets_samename():
    radio = module_loaded()
    ifaces = ("eth0", "wlan0") if radio else ("eth0",)
    extra = ("wlan=1",) if radio else ()
    clean_both()
    try:
        for net in (NET, PEER):
            add_cnt(AP, "s", extra, net=net)
            add_cnt(STA, "s", extra, net=net)
        for net in (NET, PEER):
            wait_answer(AP, net)
            wait_answer(STA, net)
        for n in (AP, STA):
            for iface in ifaces:
                a, b = mac(n, iface, NET), mac(n, iface, PEER)
                if a == b:
                    raise Fail("%s %s has the mac %s on %s and on %s"
                               % (n, iface, a, NET, PEER))
        if not radio:
            return
        both(ap_up)
        both(sta_start)
        end, done = time.time() + 40, set()
        while time.time() < end and len(done) < 2:
            for net in (NET, PEER):
                if net not in done and joined(net):
                    done.add(net)
            time.sleep(2)
        for net in (NET, PEER):
            if net not in done:
                raise Fail("%s on %s did not join its own %s, the same names "
                           "on the other net:\n%s" % (
                               STA, net, AP,
                               ssh(STA, "tail -n 20 /tmp/wpa_wlan0.log",
                                   net=net, check=False)))
    finally:
        clean_both()


if __name__ == "__main__":
    run("twonets_samename", sub_twonets_samename)
