#!/usr/bin/env python3
r"""radius - a server that says yes to the right password and no to the wrong.

    ./radius.py [net]              the net defaults to nemo

Two machines on one lan::

  rac --- lan1 --- ras

  ras runs FreeRADIUS, the authentication server. It is told about one user,
  alice, and one client network allowed to ask it, sharing a secret. rac is
  that client: it sends access requests on alice's behalf, the way a switch
  or a wifi access point would when someone tries to log in.

  ACT 1  ras starts with alice known and rac allowed to ask.
  ACT 2  rac authenticates alice with her correct password. The server
         answers Access-Accept.
  ACT 3  rac tries again with a wrong password. The server answers
         Access-Reject -- the same protocol, the opposite verdict.

Image: radius_trixie (freeradius, radtest)
Size:  2 containers, 1 lan

THE PACKET TO CATCH
-------------------
  RADIUS authentication is UDP on port 1812, request and reply:

    cloonix cli nemo cnf ras eth0 sniff=on
    cloonix cli nemo sniff ras eth0 --dump udp port 1812

Important
  Nothing is baked into the image: FreeRADIUS is installed, and the demo adds
  the client and the user to its configuration at run time.
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
RADIUS -- Remote Authentication Dial-In User Service -- was written by Carl
Rigney at Livingston Enterprises in 1991 for dial-up modem banks, and
standardised as RFC 2865. It is the protocol that sits behind "log in and be
let onto the network": a device that guards access, the client, asks a central
server whether a set of credentials is good, and the server answers accept or
reject. FreeRADIUS, begun in 1999 and long led by Alan DeKok, is the dominant
open implementation and runs a large share of the world's wifi and wired
authentication.
"""

ROOTFS_CHOICES = ("radius_trixie",)

RAS, RAC = "ras", "rac"
ALL = (RAS, RAC)
LAN = "lan1"
SECRET = "clownixRadiusSecret"

WIRING = {
    RAS: [(0, LAN, "10.0.0.2/24")],
    RAC: [(0, LAN, "10.0.0.10/24")],
}
GATEWAY = {}

CLIENT_SNIP = """\
client democlient {
    ipaddr = 10.0.0.0/24
    secret = SECRETVALUE
}
"""

USER_SNIP = """\
alice Cleartext-Password := "secret123"
"""

def conf_dir():
    # Debian keeps the config under a versioned directory; find it.
    r = ssh(RAS, "ls -d /etc/freeradius/*/ 2>/dev/null | head -n1",
            check=False).strip()
    return r or "/etc/freeradius/3.0"

def radius_start():
    d = conf_dir().rstrip("/")
    put(RAS, "/tmp/client.snip", CLIENT_SNIP.replace("SECRETVALUE", SECRET))
    put(RAS, "/tmp/user.snip", USER_SNIP)
    ssh(RAS, "cat /tmp/client.snip >> %s/clients.conf" % d)
    # The files module reads mods-config/files/authorize; older layouts use
    # a plain users file. Append alice to whichever exists.
    auth = "%s/mods-config/files/authorize" % d
    users = "%s/users" % d
    ssh(RAS, "if [ -f %s ]; then cat /tmp/user.snip >> %s; "
             "else cat /tmp/user.snip >> %s; fi" % (auth, auth, users),
        check=False)
    # The files of the image all belong to root inside: freeradius stays
    # root, as freerad it could not read its own configuration.
    ssh(RAS, "sed -i -E '/^[[:space:]]*(user|group) = freerad/d' "
             "%s/radiusd.conf" % d)
    ssh(RAS, "freeradius", check=False)
    for _ in range(20):
        if ssh(RAS, "ss -lun 2>/dev/null | grep -q :1812 && echo up",
               check=False).strip():
            return
        time.sleep(1)
    raise RuntimeError("freeradius did not start:\n%s"
                       % ssh(RAS, "freeradius -XC 2>&1 | tail -n 20",
                             check=False))

def radtest(user, password):
    # the last argument names the nas: without it radtest resolves the
    # hostname, crun in every container, and there is no dns.
    return ssh(RAC, "radtest %s %s 10.0.0.2 0 %s 0 10.0.0.10 2>&1"
               % (user, password, SECRET), check=False)

LAYOUT = {RAC: (-260, 0), LAN: (0, 0), RAS: (260, 0)}
ETHS = {(RAC, "eth0"): 0, (RAS, "eth0"): 180}

CARD = r"""
   rac --- lan1 --- ras  (FreeRADIUS)

  ras knows one user, alice, and allows rac to ask. rac sends access
  requests; the server accepts the right password and rejects the wrong one.
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
    tools_present(RAS, "freeradius")
    tools_present(RAC, "radtest")

    step("act 1: the server starts, alice known, rac allowed")
    story()
    radius_start()
    story("    FreeRADIUS is up on ras, listening on UDP 1812.")
    hold()

    step("act 2: the right password is accepted")
    story()
    got = count_on(RAS, 0, "udp port 1812",
                   lambda: radtest("alice", "secret123"), settle=3.0)
    out = got["out"] if got else ""
    story("    rac authenticates alice with the correct password:")
    for line in (out or "").strip().split("\n")[-4:]:
        story("      %s" % line)
    ok_accept = "Access-Accept" in (out or "")
    story("    verdict: %s" % ("Access-Accept" if ok_accept else "no accept"))
    story()

    step("act 3: the wrong password is rejected")
    story()
    out = radtest("alice", "wrongpassword")
    story("    rac tries again with a wrong password:")
    for line in out.strip().split("\n")[-4:]:
        story("      %s" % line)
    ok_reject = "Access-Reject" in out
    story("    verdict: %s" % ("Access-Reject" if ok_reject else "no reject"))
    story()

    if not (ok_accept and ok_reject):
        sys.exit("a check failed: accept=%s reject=%s\n%s"
                 % (ok_accept, ok_reject,
                    ssh(RAS, "tail -n 20 /var/log/freeradius/radius.log",
                        check=False)))

    story("""
the network is left up. Things worth trying:

  authenticate by hand, right and wrong:
    cloonix cli %s ssh rac "radtest alice secret123 10.0.0.2 0 %s 0 10.0.0.10"
    cloonix cli %s ssh rac "radtest alice nope 10.0.0.2 0 %s 0 10.0.0.10"

  read the server's decision live by running it in the foreground:
    cloonix cli %s ssh ras "pkill freeradius; freeradius -X"
""" % (NET, SECRET, NET, SECRET, NET))

main()
