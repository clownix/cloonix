#!/usr/bin/env python3
r"""squid - a caching proxy that speeds up hits and enforces a policy.

    ./squid.py [net]               the net defaults to nemo

Three machines on one lan::

  cl --- lan1 --- px --- web
                  |    10.0.0.80
               proxy

  px runs Squid. cl is a client configured to reach the web through the
  proxy; web is a plain origin server. Squid does two jobs at once here: it
  remembers what it fetched, so a second request for the same page is served
  from its cache, and it enforces an access rule, refusing a forbidden
  destination.

  ACT 1  Squid and the origin start. cl fetches a page through the proxy and
         gets it -- a cache MISS, fetched from web.
  ACT 2  cl fetches the same page again. Squid serves it from cache, a HIT,
         without troubling the origin.
  ACT 3  cl asks the proxy for a forbidden destination. Squid refuses it with
         a 403 before it ever leaves the proxy.

Image: squid_trixie (squid)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
  The client talks to the proxy on TCP port 3128:

    cloonix cli nemo cnf px eth0 sniff=on
    cloonix cli nemo sniff px eth0 --dump tcp port 3128

Important
  Nothing is baked into the image: Squid is installed, and the demo writes its
  configuration and initialises its cache at run time.
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
Squid descends from the Harvest project's cache of the early 1990s and has
been developed as Squid since 1996, long coordinated by Duane Wessels and
Henrik Nordstrom. For years it was how an office got one shared, cached path
to the web, and how it filtered what that path would carry. It is a forward
proxy, a reverse proxy and a cache in one.
"""

ROOTFS_CHOICES = ("squid_trixie",)

CL, PX, WEB = "cl", "px", "web"
ALL = (CL, PX, WEB)
LAN = "lan1"

WIRING = {
    CL:  [(0, LAN, "10.0.0.10/24")],
    PX:  [(0, LAN, "10.0.0.2/24")],
    WEB: [(0, LAN, "10.0.0.80/24")],
}
GATEWAY = {}

SQUID_CONF = """\
http_port 3128
cache_effective_user proxy
dns_nameservers 10.0.0.2
dns_retransmit_interval 1 second
dns_timeout 1 second
coredump_dir /var/spool/squid
cache_dir ufs /var/spool/squid 100 16 256
acl localnet src 10.0.0.0/24
acl blocked dstdomain .blocked.test
http_access deny blocked
http_access allow localnet
http_access deny all
"""

PAGE = "hello from the origin web server\n"

def squid_bin():
    for cand in ("squid", "squid-gnutls"):
        if ssh(PX, "command -v %s" % cand, check=False).strip():
            return cand
    sys.exit("no squid binary on px: not the squid image")

def web_start():
    ssh(WEB, "mkdir -p /root/site")
    put(WEB, "/root/site/index.html", PAGE)
    ssh(WEB, "cd /root/site && (python3 -m http.server 80 "
             ">/tmp/web.log 2>&1 &)")
    time.sleep(2)

def squid_start(binary):
    # With no nameserver squid asks localhost and waits, even for an IP
    # written in the url: it is pointed at a server that is not there and
    # told to give up in a second.
    put(PX, "/etc/squid/squid.conf", SQUID_CONF)
    # squid refuses to run as root and drops to proxy; the files of the
    # image all belong to root inside, so its two trees are given to proxy.
    ssh(PX, "chown -R proxy:proxy /var/spool/squid /var/log/squid")
    ssh(PX, "%s -z 2>/dev/null; sleep 2" % binary, check=False)
    ssh(PX, binary, check=False)
    for _ in range(20):
        if ssh(PX, "ss -ltn 2>/dev/null | grep -q :3128 && echo up",
               check=False).strip():
            return
        time.sleep(1)
    raise RuntimeError("squid did not start:\n%s"
                       % ssh(PX, "tail -n 20 /var/log/squid/cache.log",
                             check=False))

def fetch(url):
    return ssh(CL, "curl -s -o /dev/null -w '%%{http_code}' -m 5 "
                   "-x http://10.0.0.2:3128 %s" % url, check=False).strip()

def last_log_tag():
    return ssh(PX, "tail -n 1 /var/log/squid/access.log 2>/dev/null",
               check=False).strip()

LAYOUT = {CL: (-320, 0), LAN: (-60, 0), PX: (180, 0), WEB: (180, 200)}
ETHS = {(CL, "eth0"): 0, (PX, "eth0"): 180, (WEB, "eth0"): 300}

CARD = r"""
   cl --- lan1 --- px --- web  10.0.0.80
                 proxy 3128

  cl reaches the web only through px. Squid caches what it fetches (second
  request is a HIT) and refuses a forbidden destination with a 403.
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

    step("creating and cabling the three machines")
    build(rootfs)
    tools_present(CL, "curl")
    binary = squid_bin()

    step("act 1: fetch through the proxy, a cache miss")
    story()
    web_start()
    squid_start(binary)
    code = fetch("http://10.0.0.80/")
    story("    cl fetched http://10.0.0.80/ through px, http code: %s" % code)
    story("    squid logged: %s" % last_log_tag())
    ok_first = code == "200"
    hold()

    step("act 2: fetch it again, a cache hit")
    story()
    got = count_on(PX, 0, "tcp port 3128",
                   lambda: fetch("http://10.0.0.80/"), settle=3.0)
    tag = last_log_tag()
    story("    squid logged: %s" % tag)
    if got is not None:
        story("    %d packet(s) client-to-proxy on 3128 for the repeat"
              % got["count"])
    ok_hit = "HIT" in tag.upper()
    story("    served from cache: %s" % ("yes" if ok_hit else "not marked"))
    story()

    step("act 3: a forbidden destination is refused")
    story()
    code = fetch("http://www.blocked.test/")
    story("    cl asked px for http://www.blocked.test/, http code: %s" % code)
    story("    squid logged: %s" % last_log_tag())
    ok_denied = code == "403"
    story("    refused by the proxy: %s" % ("yes (403)" if ok_denied
                                            else "no"))
    story()

    if not (ok_first and ok_denied):
        sys.exit("a check failed: first=%s hit=%s denied=%s"
                 % (ok_first, ok_hit, ok_denied))

    story("""
the network is left up. Things to try:

  fetch through the proxy and watch the cache column:
    cloonix cli %s ssh cl "curl -s -x http://10.0.0.2:3128 http://10.0.0.80/"
    cloonix cli %s ssh px "tail /var/log/squid/access.log"

  try the forbidden destination by hand:
    cloonix cli %s ssh cl "curl -x http://10.0.0.2:3128 http://www.blocked.test/"
""" % (NET, NET, NET))

main()
