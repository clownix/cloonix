#!/usr/bin/env python3
r"""rpki_example - RPKI, Examples: BGP origin validation.

    ./rpki_example.py [net]     the net defaults to nemo

The first example of the BIRD User's Guide, chapter RPKI, section
"Examples": "BGP origin validation", policy "Don't import ROA_INVALID
routes".
https://bird.nic.cz/doc/latest/bird-6.html#rpki-example-bgp-origin-validation

    cache (10.0.0.10, RTR port 8282)
      |
    lan0 10.0.0.0/24 ---- r (AS 65000) ---- p (AS 65001)

  r carries the guide's configuration word for word but for the cache
  address: the ROA tables r4 and r6, protocol rpki towards the cache with
  retry 5, refresh 30, expire 600, the filter peer_in_v4 that refuses what
  roa_check finds ROA_INVALID, and the session to AS 65001 with it.
  The cache is a small RTR server written in python in this demo, holding
  two ROAs: 192.0.2.0/24 for AS 65001, 198.51.100.0/24 for AS 64999.

  ACT 1  r fetches the ROAs from the cache over RTR.
  ACT 2  p announces three prefixes from AS 65001: 192.0.2.0/24 is valid
         and accepted, 198.51.100.0/24 is invalid and refused, 203.0.113.0/24
         has no ROA, unknown, and is accepted.

Image: bird_trixie (bird2, python3 for the cache)
Size:  3 containers, 1 lan

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf cache eth0 sniff=on
    cloonix cli nemo sniff cache eth0 --dump "tcp port 8282"

  The answer of the cache to the Reset Query of r: a Cache Response, two
  IPv4 Prefix PDUs of 20 bytes, and an End of Data.

Important
  The guide points at rpki-validator.realmv6.org, a public test cache;
  the demo brings its own, so it needs no internet.
"""
import os
import sys

# What every demo shares is in demos/lib/demolib.py, toylib.py and birdlib.py.
sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [os.path.join(_d, "lib") for _d in (
    _HERE, os.path.dirname(_HERE),
    os.environ.get("CLOONIX_DEMO_DIR", "/usr/share/cloonix/demos"))]
from demolib import NET, count_on, hold, ssh, step, story
from birdlib import bird_start, birdc, has, lacks, opening, proof, put_any, \
    until, verdict

HISTORY = """\
The RPKI attaches certificates to the address blocks the regional
registries hand out, RFC 6480 in 2012; a ROA says which AS may originate a
prefix, and a router learns the ROAs from a cache over the RPKI to Router
protocol, RFC 6810 and RFC 8210, by Randy Bush and Rob Austein. The RPKI of
BIRD was written by Pavel Tvrdik of CZ.NIC in 2016.

BIRD began in 1998 as a student project at the Faculty of Mathematics and
Physics of Charles University in Prague, by Martin Mares, Pavel Machek and
Ondrej Filip; CZ.NIC maintains it since 2009.

Today: most large networks drop RPKI invalid routes, which stops many
hijacks and route leaks.
"""

ROOTFS_CHOICES = ("bird_trixie",)
TOOL = "birdc"

R, P, CACHE = "r", "p", "cache"
ALL = (R, P, CACHE)
L0 = "lan0"

WIRING = {
    R: [(0, L0, "10.0.0.1/24")],
    P: [(0, L0, "10.0.0.2/24")],
    CACHE: [(0, L0, "10.0.0.10/24")],
}
GATEWAY = {}

# The guide's configuration, the cache address changed.
R_CONF = """\
router id 10.0.0.1;
protocol device { }
roa4 table r4;
roa6 table r6;

protocol rpki {
	debug all;

	roa4 { table r4; };
	roa6 { table r6; };

	remote "10.0.0.10" port 8282;

	retry keep 5;
	refresh keep 30;
	expire 600;
}

filter peer_in_v4 {
	if (roa_check(r4, net, bgp_path.last) = ROA_INVALID) then
	{
		print "Ignore RPKI invalid ", net, " for ASN ", bgp_path.last;
		reject;
	}
	accept;
}

protocol bgp {
	debug all;
	local as 65000;
	neighbor 10.0.0.2 as 65001;
	ipv4 {
		import filter peer_in_v4;
		export none;
	};
}
"""

P_CONF = """\
router id 10.0.0.2;
protocol device { }
protocol static {
	ipv4;
	route 192.0.2.0/24 blackhole;
	route 198.51.100.0/24 blackhole;
	route 203.0.113.0/24 blackhole;
}
protocol bgp {
	local as 65001;
	neighbor 10.0.0.1 as 65000;
	ipv4 { import none; export all; };
}
"""

# A minimal RTR cache, RFC 6810 and 8210: it answers in the version asked.
RTR = """\
import socket, struct, threading
ROAS = [("192.0.2.0", 24, 24, 65001), ("198.51.100.0", 24, 24, 64999)]
SESSION, SERIAL = 4242, 1

def response(ver):
    return struct.pack("!BBHI", ver, 3, SESSION, 8)

def prefixes(ver):
    return b"".join(struct.pack("!BBHIBBBB4sI", ver, 4, 0, 20, 1, l, m, 0,
                                socket.inet_aton(p), asn)
                    for p, l, m, asn in ROAS)

def end(ver):
    if ver == 0:
        return struct.pack("!BBHII", 0, 7, SESSION, 12, SERIAL)
    return struct.pack("!BBHIIIII", ver, 7, SESSION, 24, SERIAL, 30, 5, 600)

def serve(c):
    while True:
        h = c.recv(8)
        if len(h) < 8:
            return
        ver, typ, _, length = struct.unpack("!BBHI", h)
        if length > 8:
            c.recv(length - 8)
        if typ == 2:
            c.sendall(response(ver) + prefixes(ver) + end(ver))
        elif typ == 1:
            c.sendall(response(ver) + end(ver))

s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("0.0.0.0", 8282))
s.listen(5)
while True:
    c, _ = s.accept()
    threading.Thread(target=serve, args=(c,), daemon=True).start()
"""

LAYOUT = {CACHE: (0, -200), L0: (0, 0), R: (-260, 0), P: (260, 0)}
ETHS = {(CACHE, "eth0"): 180, (R, "eth0"): 90, (P, "eth0"): 270}

CARD = r"""
            cache (RTR)
              |
    r ----- lan0 ----- p

  r drops what the ROAs of the cache call invalid.
"""

def main():
    opening()
    put_any(CACHE, "/tmp/rtr.py", RTR)
    ssh(CACHE, "python3 /tmp/rtr.py </dev/null >/tmp/rtr.log 2>&1 &")

    step("act 1: r fetches the ROAs over RTR")
    got = count_on(CACHE, 0, "tcp port 8282", lambda: (
        bird_start(R, R_CONF),
        until(lambda: has(R, "show route table r4",
                          ["192.0.2.0/24", "198.51.100.0/24"]), 60))[1],
        settle=1.0)
    ok_roa = got["out"] if got else has(R, "show route table r4",
                                        ["192.0.2.0/24"])
    proof("r, the ROA table r4:", birdc(R, "show route table r4"))
    proof("r, the rpki protocol:", birdc(R, "show protocols all rpki1"))
    hold()

    step("act 2: p announces three prefixes, the invalid one is dropped")
    bird_start(P, P_CONF)
    # master4 only: "show route" lists the ROA table r4 too, where
    # 198.51.100.0/24 stands as a ROA
    ok_valid = until(lambda: has(R, "show route table master4",
                                 ["192.0.2.0/24", "203.0.113.0/24"]), 90)
    ok_inval = lacks(R, "show route table master4", ["198.51.100.0/24"])
    proof("r, what it imported:", birdc(R, "show route table master4"))

    verdict([("r holds the two ROAs of the cache", ok_roa),
             ("valid and unknown routes are accepted", ok_valid),
             ("the invalid route is refused", ok_inval)])

    story("""
the network is left up. Things to try:

  the validation state of a prefix:
    cloonix cli %s ssh r "birdc eval roa_check(r4, 198.51.100.0/24, 65001)"

  the RTR exchange, at the next refresh:
    cloonix cli %s sniff cache eth0 --dump "tcp port 8282"
""" % (NET, NET))

main()
