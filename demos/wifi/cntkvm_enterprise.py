#!/usr/bin/env python3
"""cntkvm_enterprise - WPA2-Enterprise, a login per user: the roles of 802.1X.

    ./cntkvm_enterprise.py [net]    the net defaults to nemo

The wifi of a company or a university: no shared passphrase, a login::

      alice (cnt)                                      radius (cnt)
      bob (kvm)    (((  cntwifi  )))  ap (kvm) -- lan1 -- 10.44.0.2
      mallory (cnt)                  10.44.0.1

Each station runs wpa_supplicant, the supplicant. The access point, hostapd
in a kvm, is the authenticator: it knows no password, it relays EAP from
the air into RADIUS on the cable. freeradius in a container is the
authentication server. alice and bob log in with PEAP and MSCHAPv2, then
ping each other; mallory gives a wrong password and never gets a key.

Image: radius_trixie, trixie.qcow2
Size:  3 cnt, 2 kvm, 1 air, 1 lan

THE PACKET TO CATCH
-------------------
  The questions of the access point and the answers of the server:

      cloonix cli nemo sniff radius eth0 --dump udp port 1812

  and on the air the EAP of each station, then its 4-way handshake:

      cloonix cli nemo cnf cntwifi sniff=on
      cloonix cli nemo sniff cntwifi
"""
import os
import sys
import time

# What every demo shares is in demos/lib/demolib.py, the wifi part in
# demos/lib/wifilib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import (
    NET, check_net, choose_rootfs, cli, count_on, create, erase_net, hold,
    in_parallel, place, put, ssh, step, story, tools_present, wait_answer)
from wifilib import (
    address, create_radio_cnt, create_radio_kvm, hostapd, hwsim_on_host,
    link, ping, wait_radios, wifi_state)

HISTORY = """\
802.1X, from 2001, carries EAP, RFC 3748, between a supplicant and an
authenticator, which hands it to a RADIUS server, RFC 2865, written by Carl
Rigney in 1991 for the modem banks. 802.11i of 2004 made it the enterprise
mode of WPA2. PEAP, by Microsoft, Cisco and RSA in 2002, wraps the password
exchange in TLS. hostapd and wpa_supplicant are Jouni Malinen's, freeradius
was begun in 1999 and long led by Alan DeKok."""

ROOTFS_CHOICES = ("radius_trixie",)
KVM_ROOTFS = "trixie.qcow2"
AP, RAD = "ap", "radius"
ALICE, BOB, MAL = "alice", "bob", "mallory"
ALL = (AP, RAD, ALICE, BOB, MAL)
AIR = "cntwifi"
SECRET = "clownixRadiusSecret"
WIRING = {AP: [(0, "lan1", "10.44.0.1/24")],
          RAD: [(0, "lan1", "10.44.0.2/24")],
          ALICE: [(0, None, None)], BOB: [(0, None, None)],
          MAL: [(0, None, None)]}
RADIOS = {AP: 1, ALICE: 1, BOB: 1, MAL: 1}
ADDR = {ALICE: "10.44.1.11", BOB: "10.44.1.12", MAL: "10.44.1.13"}
PASSWORD = {ALICE: "alicepass", BOB: "bobpass", MAL: "guessed"}
LAYOUT = {ALICE: (-260, -100), BOB: (-260, 0), MAL: (-260, 100),
          AIR: (-130, 0), AP: (0, 0), "lan1": (130, 0), RAD: (260, 0)}
ETHS = {(AP, "eth0"): 90, (RAD, "eth0"): 270}

AP_CONF = """interface=wlan0
driver=nl80211
ssid=cloonix-corp
hw_mode=g
channel=6
ieee8021x=1
wpa=2
wpa_key_mgmt=WPA-EAP
rsn_pairwise=CCMP
own_ip_addr=10.44.0.1
nas_identifier=ap
auth_server_addr=10.44.0.2
auth_server_port=1812
auth_server_shared_secret=%s
""" % SECRET

CLIENT_SNIP = """client ap {
    ipaddr = 10.44.0.1
    secret = %s
}
""" % SECRET

USERS_SNIP = """alice Cleartext-Password := "alicepass"
bob Cleartext-Password := "bobpass"
mallory Cleartext-Password := "mallorypass"
"""

def sta_conf(node):
    return """network={
    ssid="cloonix-corp"
    key_mgmt=WPA-EAP
    eap=PEAP
    identity="%s"
    password="%s"
    phase2="auth=MSCHAPV2"
}
""" % (node, PASSWORD[node])

def radius_start():
    d = ssh(RAD, "ls -d /etc/freeradius/*/ 2>/dev/null | head -n1",
            check=False).strip().rstrip("/") or "/etc/freeradius/3.0"
    put(RAD, "/tmp/client.snip", CLIENT_SNIP)
    put(RAD, "/tmp/users.snip", USERS_SNIP)
    ssh(RAD, "cat /tmp/client.snip >> %s/clients.conf" % d)
    auth = "%s/mods-config/files/authorize" % d
    ssh(RAD, "if [ -f %s ]; then cat /tmp/users.snip >> %s; "
             "else cat /tmp/users.snip >> %s/users; fi" % (auth, auth, d),
        check=False)
    # the image files belong to root inside: freeradius stays root
    ssh(RAD, "sed -i -E '/^[[:space:]]*(user|group) = freerad/d' "
             "%s/radiusd.conf" % d)
    ssh(RAD, "freeradius", check=False)
    for _ in range(20):
        if ssh(RAD, "ss -lun 2>/dev/null | grep -q :1812 && echo up",
               check=False).strip():
            return
        time.sleep(1)
    sys.exit("freeradius did not start:\n%s"
             % ssh(RAD, "freeradius -XC 2>&1 | tail -n 20", check=False))

def supplicant(node):
    put(node, "/etc/wpa_wlan0.conf", sta_conf(node))
    ssh(node, "ip link set dev wlan0 up")
    ssh(node, "wpa_supplicant -B -i wlan0 -D nl80211 -c /etc/wpa_wlan0.conf "
              "-f /tmp/wpa_wlan0.log", check=False)

def authorized(node, secs=40):
    """EAP succeeded and the keys are in place: the log of wpa_supplicant."""
    end = time.time() + secs
    while time.time() < end:
        log = ssh(node, "cat /tmp/wpa_wlan0.log 2>/dev/null", check=False)
        if "CTRL-EVENT-CONNECTED" in log:
            return True
        if "CTRL-EVENT-EAP-FAILURE" in log:
            return False
        time.sleep(2)
    return False

def login_all():
    for n in (ALICE, BOB, MAL):
        supplicant(n)
    return dict((n, authorized(n)) for n in (ALICE, BOB, MAL))

def main():
    hwsim_on_host()
    check_net()
    rootfs = choose_rootfs()
    story(HISTORY)
    story()
    step("erasing the net, then placing the items")
    erase_net()
    place()
    step("a kvm authenticator, a RADIUS server, three stations")
    in_parallel(lambda n: create_radio_kvm(n, KVM_ROOTFS), [AP, BOB],
                "add kvm")
    in_parallel(lambda n: create_radio_cnt(n, rootfs), [ALICE, MAL],
                "add cnt")
    create(RAD, rootfs)
    cli("plug", AP, "eth0", "lan1")
    cli("plug", RAD, "eth0", "lan1")
    for n in ALL:
        wait_answer(n, 180)
        wait_radios(n)
    tools_present(RAD, "freeradius")
    tools_present(ALICE, "wpa_supplicant")
    step("the kvm radios put on cntwifi, the air of the containers")
    for n in (AP, BOB):
        r = cli("plug", n, "wlan0", AIR)
        if r.returncode:
            sys.exit("%s would not go on %s: %s"
                     % (n, AIR, (r.stdout + r.stderr).strip()))
    step("freeradius knows ap and three users; hostapd asks it")
    address(AP, "eth0", "10.44.0.1/24")
    address(RAD, "eth0", "10.44.0.2/24")
    radius_start()
    hostapd(AP, AP_CONF)
    step("alice, bob and mallory log in, RADIUS counted on the cable")
    got = count_on(RAD, 0, "udp port 1812", login_all)
    if got is None:
        sys.exit("the cable of radius could not be sniffed")
    res = got["out"]
    story()
    story("  alice (cnt), right password:   %s"
          % ("in" if res[ALICE] else "REFUSED"))
    story("  bob (kvm), right password:     %s"
          % ("in" if res[BOB] else "REFUSED"))
    story("  mallory (cnt), wrong password: %s"
          % ("IN" if res[MAL] else "refused"))
    story("  %d RADIUS packets between ap and the server" % got["count"])
    story()
    hold()
    if not res[ALICE] or not res[BOB] or res[MAL]:
        wifi_state((AP, ALICE, BOB, MAL))
        sys.exit("the logins did not end as they should")
    step("alice and bob, both let in, ping each other")
    for n in (ALICE, BOB):
        address(n, "wlan0", ADDR[n] + "/24")
    ok, out = False, ""
    end = time.time() + 30
    while time.time() < end and not ok:
        ok, out = ping(ALICE, ADDR[BOB])
    story()
    story("  alice (cnt) -> bob (kvm): %s" % ("OK" if ok else "FAILS"))
    story("  mallory: %s" % (link(MAL).splitlines() or ["?"])[0])
    story()
    hold()
    if not ok:
        wifi_state((AP, ALICE, BOB))
        sys.exit("alice does not reach bob:\n%s" % out)
    story("""
The access point never saw a password: it carried EAP from the air to the
cable and obeyed Access-Accept and Access-Reject. The passwords crossed the
air inside TLS, PEAP; the stations here accept any server certificate, a
real network gives them its CA with ca_cert= in the supplicant.""")
    story("the network is left up. The server, in the foreground:")
    story("    cloonix cli %s ssh radius \"pkill freeradius; freeradius -X\""
          % NET)

main()
