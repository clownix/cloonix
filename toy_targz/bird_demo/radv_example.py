#!/usr/bin/env python3
r"""radv_example - RAdv, Example.

    ./radv_example.py [net]     the net defaults to nemo

The example of the BIRD User's Guide, chapter RAdv, section "Example":
https://bird.nic.cz/doc/latest/bird-6.html#radv-exam

    ha ---- lanA 2001:db8:1234:1::/64 ----+
    hb ---- lanB 2001:db8:2000:1::/64 ----+ r
    hc ---- lanC 2001:db8:3000:1::/64 ----+ (eth2: managed, autonomous off)

  r carries the guide's configuration word for word: the table radv_routes
  with three unreachable /48, the third with high preference and a
  lifetime of 3600; protocol radv propagating those routes; "eth2" managed
  with ::/0 not autonomous; the prefixes 2001:DB8:1234::/48 (preferred
  lifetime 0) and 2001:DB8:2000::/48 (autonomous off); RDNSS and DNSSL.

  ACT 1  each host reads the advertisement of its lan:
           ha makes an address in 2001:db8:1234:1::/64, born deprecated;
           hb makes none, the prefix is not autonomous;
           hc makes none and sees the managed flag, the DHCPv6 hint.
  ACT 2  the routes of radv_routes reach the hosts as route information
         options: each host has 2001:db8:4020::/48 via r, preference high.

Image: bird_trixie (bird2)
Size:  4 containers, 3 lans

THE PACKET TO CATCH
-------------------
    cloonix cli nemo cnf ha eth0 sniff=on
    cloonix cli nemo sniff ha eth0 --dump "icmp6"

  A router advertisement: the prefix option with a preferred lifetime of
  0, three route information options, RDNSS and DNSSL options.

Important
  The hosts accept route information options up to /48
  (accept_ra_rt_info_max_plen 48); linux takes none by default.
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
from birdlib import bird_start, opening, proof, until, verdict

HISTORY = """\
Neighbor Discovery, RFC 4861, and stateless autoconfiguration, RFC 4862,
let a host build its address from the prefix a router advertises; RFC 4191
by Richard Draves and Dave Thaler added the route information option and
router preferences, RFC 8106 the DNS options. The RAdv of BIRD was written
by Ondrej Zajicek in 2011.

BIRD began in 1998 as a student project at the Faculty of Mathematics and
Physics of Charles University in Prague, by Martin Mares, Pavel Machek and
Ondrej Filip; CZ.NIC maintains it since 2009.

Today: SLAAC is how most phones, laptops and home devices get their IPv6
address.
"""

ROOTFS_CHOICES = ("bird_trixie",)
TOOL = "birdc"

R, HA, HB, HC = "r", "ha", "hb", "hc"
ALL = (R, HA, HB, HC)
HOSTS = (HA, HB, HC)
LA, LB, LC = "lanA", "lanB", "lanC"

WIRING = {
    R: [(0, LA, "2001:db8:1234:1::1/64"), (1, LB, "2001:db8:2000:1::1/64"),
        (2, LC, "2001:db8:3000:1::1/64")],
    HA: [(0, LA, None)],
    HB: [(0, LB, None)],
    HC: [(0, LC, None)],
}
GATEWAY = {}

# The guide's configuration.
R_CONF = """\
router id 10.0.0.1;
protocol device { }
ipv6 table radv_routes;			# Manually configured routes go here

protocol static {
	ipv6 { table radv_routes; };

	route 2001:0DB8:4000::/48 unreachable;
	route 2001:0DB8:4010::/48 unreachable;

	route 2001:0DB8:4020::/48 unreachable {
		ra_preference = RA_PREF_HIGH;
		ra_lifetime = 3600;
	};
}

protocol radv {
	propagate routes yes;		# Propagate the routes from the radv_routes table
	ipv6 { table radv_routes; export all; };

	interface "eth2" {
		max ra interval 5;	# Fast failover with more routers
		managed yes;		# Using DHCPv6 on eth2
		prefix ::/0 {
			autonomous off;	# So do not autoconfigure any IP
		};
	};

	interface "eth*";		# No need for any other options

	prefix 2001:0DB8:1234::/48 {
		preferred lifetime 0;	# Deprecated address range
	};

	prefix 2001:0DB8:2000::/48 {
		autonomous off;		# Do not autoconfigure
	};

	rdnss 2001:0DB8:1234::10;	# Short form of RDNSS

	rdnss {
		lifetime mult 10;
		ns 2001:0DB8:1234::11;
		ns 2001:0DB8:1234::12;
	};

	dnssl {
		lifetime 3600;
		domain "abc.com";
		domain "xyz.com";
	};
}
"""

LAYOUT = {HA: (-420, -180), LA: (-200, -180), HB: (-420, 0), LB: (-200, 0),
          HC: (-420, 180), LC: (-200, 180), R: (60, 0)}
ETHS = {(HA, "eth0"): 90, (HB, "eth0"): 90, (HC, "eth0"): 90,
        (R, "eth0"): 315, (R, "eth1"): 270, (R, "eth2"): 225}

CARD = r"""
    ha --+
    hb --+-- r   (protocol radv: prefixes, routes, RDNSS, DNSSL)
    hc --+

  Three lans, three different readings of one radv configuration.
"""

def addrs(node):
    return ssh(node, "ip -6 addr show dev eth0 scope global", check=False)

def rt_info(node):
    out = ssh(node, "ip -6 route show 2001:db8:4020::/48", check=False)
    return "fe80::" in out and "pref high" in out

def main():
    opening()
    for h in HOSTS:
        ssh(h, "sysctl -w net.ipv6.conf.eth0.accept_ra=1 "
               "net.ipv6.conf.eth0.autoconf=1 "
               "net.ipv6.conf.eth0.accept_ra_rt_info_max_plen=48 >/dev/null")

    step("act 1: each host reads the advertisement of its lan")

    def start_and_solicit():
        bird_start(R, R_CONF)
        for h in HOSTS:
            ssh(h, "ip link set eth0 down; ip link set eth0 up")
        return until(lambda: "2001:db8:1234:1:" in addrs(HA), 60)
    got = count_on(HA, 0, "icmp6", start_and_solicit, settle=1.0)
    ok_a = got["out"] if got else "2001:db8:1234:1:" in addrs(HA)
    ok_dep = "deprecated" in addrs(HA)
    proof("ha, its address, deprecated:", addrs(HA))
    ok_b = "2001:db8:2000:1:" not in addrs(HB)
    ok_c = "2001:db8:3000:1:" not in addrs(HC)
    proof("hb, no global address:", addrs(HB) or "(none)")
    hold()

    step("act 2: the routes of radv_routes reach the hosts")
    ok_rt = until(lambda: all(rt_info(h) for h in HOSTS), 60)
    proof("ha, the routes r announced:",
          ssh(HA, "ip -6 route show proto ra", check=False))

    verdict([("ha makes an address in 2001:db8:1234:1::/64", ok_a),
             ("that address is deprecated, preferred lifetime 0", ok_dep),
             ("hb makes none, autonomous off", ok_b),
             ("hc makes none on the managed lan", ok_c),
             ("each host has 2001:db8:4020::/48, preference high", ok_rt)])

    story("""
the network is left up. Things to try:

  the advertisements as they pass:
    cloonix cli %s sniff ha eth0 --dump "icmp6"

  what radv announces, interface by interface:
    cloonix cli %s ssh r "birdc show interfaces"
""" % (NET, NET))

main()
