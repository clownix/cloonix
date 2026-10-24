"""toylib - what the demos of the specialised toys share, on top of demolib.

Each demo of <name>_demo/ (frr_demo, bird_demo) plays one example of the
documentation of that project. What they share is here: the opening (image,
erase, place, build, the kernel modules), the waits, the pings, and the
verdict that closes every demo. The talk to the daemon is in frrlib.py,
birdlib.py. Standard library only.
"""
import base64
import errno
import socket
import sys
import time
import __main__ as DEMO
from demolib import (
    build, check_net, choose_rootfs, erase_net, need_host_modules, place, ssh,
    step, story, tools_present, unavailable)

def opening(*modules):
    """History, image, empty canvas, the machines built and addressed, and
    the kernel modules the demo needs checked before anything else."""
    story(DEMO.HISTORY)
    step("checking the server")
    check_net()
    step("looking for the image")
    rootfs = choose_rootfs()
    erase_net()
    place()
    story(DEMO.CARD)
    story("  image: %s" % rootfs)
    step("creating and cabling the %d machines" % len(DEMO.ALL))
    build(rootfs)
    tools_present(DEMO.ALL[0], DEMO.TOOL)
    need_host_modules(DEMO.ALL[0], modules)

def put_any(node, path, text):
    """A file of any content: through base64, so quotes, % and | travel
    unchanged."""
    b64 = base64.b64encode(text.encode()).decode()
    ssh(node, "echo %s | base64 -d > %s" % (b64, path))

def need_kernel(major, minor, why):
    """A host kernel older than major.minor is a stop: the containers run on
    it. Big warning, play the demo on a newer kernel or skip it."""
    rel = ssh(DEMO.ALL[0], "uname -r", check=False).strip()
    try:
        have = tuple(int(x) for x in rel.split("-")[0].split(".")[:2])
    except ValueError:
        have = (0, 0)
    if have >= (major, minor):
        return
    bar = "!" * 72
    unavailable("%s\nWARNING: this demo needs a host kernel %d.%d or later "
                "(%s); this host\nruns %s, and the containers run on the host "
                "kernel.\n%s" % (bar, major, minor, why,
                                  rel or "an unknown kernel", bar))

def has_tcp_ao():
    """The host kernel, the one of the containers, has TCP-AO: 6.7 or later
    is not enough, CONFIG_TCP_AO may be off. Probed on a socket: TCP_AO_INFO
    (40) answers ENOPROTOOPT without it."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.getsockopt(socket.IPPROTO_TCP, 40, 64)
        return True
    except OSError as e:
        return e.errno != errno.ENOPROTOOPT
    finally:
        s.close()

def words_in(out, words):
    """Every word is in the output."""
    return all(w in out for w in words)

def words_out(out, words):
    """The output is there and holds none of the words."""
    return bool(out.strip()) and not any(w in out for w in words)

def forwarding(node):
    ssh(node, "sysctl -w net.ipv4.ip_forward=1 "
              "net.ipv6.conf.all.forwarding=1 >/dev/null")

def until(test, timeout=90, period=2):
    """True as soon as test() is, False at the timeout."""
    end = time.time() + timeout
    while time.time() < end:
        if test():
            return True
        time.sleep(period)
    return False

def ping(src, dst, count=5):
    return ssh(src, "ping -c %d -W 1 %s" % (count, dst), check=False)

def pings(src, dst):
    """A single ping that answers, quiet: for the waits."""
    return " 0% packet loss" in ssh(src, "ping -c 1 -W 1 %s" % dst,
                                    check=False)

def proof(title, text):
    story("    %s" % title)
    for line in text.rstrip().split("\n"):
        story("      %s" % line)
    story()

def verdict(checks):
    """The end of every demo: each check named, any failure is an exit."""
    story("    the checks of this demo:")
    for name, ok in checks:
        story("      %-4s %s" % ("ok" if ok else "FAIL", name))
    story()
    bad = [name for name, ok in checks if not ok]
    if bad:
        sys.exit("%d check(s) failed: %s" % (len(bad), ", ".join(bad)))
    story("    the example of the documentation works as documented.")
