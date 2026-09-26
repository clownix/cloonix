======================
Addresses, names, time
======================

address/dhcp.py
===============

.. image:: /svg/demo_dhcp.svg
   :align: center
   :width: 100%

BOOTP, RFC 951 by Bill Croft and John Gilmore in 1985, let a diskless machine
ask the network who it was. DHCP, written by Ralph Droms as RFC 1531 in 1993
and settled in RFC 2131 in 1997, kept the packet format and added leases and a
pool: the four messages seen here, DISCOVER, OFFER, REQUEST, ACK, and the
giaddr field a relay fills in. Kea is the Internet Systems Consortium server
of 2014, the successor of the ISC dhcpd that Ted Lemon had maintained since
the 1990s. dnsmasq is the work of Simon Kelley, published in 2001 for home
routers; dhcpcd is by Roy Marples, and udhcpc belongs to busybox.

Today: every home router, campus and cloud network hands out addresses with
it; a phone joining a wifi gets address, gateway and resolver in four
packets, and the relay shown here is what lets one server serve hundreds of
lans.

Five containers, three lans, image *dhcp_trixie*: kea, dnsmasq as the relay,
*dhcpcd* and busybox *udhcpc* as clients::

    cloonix cli nemo demo address/dhcp.py

Both clients start with no address. The relay writes its own address in
**giaddr** and forwards the broadcast as a unicast; kea picks the pool on
that field. One exchange seen at the client lan, at the relay and at the
server::

    t+ms  seen on             from                to                  what it said
    0     clientA eth0  lanA  0.0.0.0:68          255.255.255.255:67  DISCOVER giaddr=0.0.0.0
    3     relay1 eth1  lanC   10.1.0.1:67         10.0.0.3:67         DISCOVER giaddr=10.1.0.1
    9     kea    eth0  lanC   10.0.0.3:67         10.1.0.1:67         OFFER    yiaddr=10.1.0.100
    12    clientA eth0  lanA  10.1.0.1:67         255.255.255.255:68  OFFER    yiaddr=10.1.0.100

The two client programs get different addresses on the same mac: option 61,
the client identifier. To break: *ip route del 10.1.0.0/24* on kea; the
server answers giaddr and the offer is dropped while the log says it was
sent::

    cloonix cli nemo sniff relay1 eth1 --dump "udp port 67 or udp port 68"


address/ipv6.py
===============

.. image:: /svg/demo_ipv6.svg
   :align: center
   :width: 100%

IPv6 was specified in RFC 1883 in 1995 by Steve Deering and Robert Hinden,
when the end of the 32-bit IPv4 address space was already in sight: 128-bit
addresses, no broadcast, no arp. Stateless autoconfiguration, RFC 1971 in
1996 by Susan Thomson and Thomas Narten, now RFC 4862, lets a host build its
own address from a prefix the router announces, with no server keeping a
lease. The KAME project in Japan wrote the stack of the BSDs; Pedro Roque
wrote the first one of linux, and the USAGI project of Hideaki Yoshifuji
completed it.

Today: more than forty percent of the traffic reaching Google arrives over
IPv6, and several mobile networks hand out nothing else. The router side is
zebra, of frr, the descendant of GNU Zebra written by Kunihiro Ishiguro.

Three containers, two lans, image *frr_trixie*::

    cloonix cli nemo demo address/ipv6.py

*r1* holds *::1* of a prefix on each lan and runs zebra, which sends router
advertisements. The hosts are given no address: each has a link-local one,
*fe80::*, made from its mac, then makes a global one from the prefix it hears
and takes *r1*, by its link-local address, as default router. *h1* pings the
address *h2* made. All of it is icmp6::

    cloonix cli nemo sniff r1 eth0 --dump icmp6
    cloonix cli nemo ssh r1 "vtysh -c 'show interface eth0'"
    cloonix cli nemo ssh h1 "ip -6 neigh show dev eth0"


address/dns.py
==============

.. image:: /svg/demo_dns.svg
   :align: center
   :width: 100%

Until 1983 every host on the ARPANET fetched one HOSTS.TXT file from SRI. Paul
Mockapetris designed the Domain Name System that year, RFC 882 and 883,
rewritten as RFC 1034 and 1035 in 1987: a tree of names, delegated zone by
zone, and resolvers that ask, cache and forward. BIND, the Berkeley Internet
Name Domain, was written at the University of California in 1984 by Douglas
Terry, Mark Painter, David Riggle and Songnian Zhou, maintained by Paul Vixie
from 1988, and rewritten as BIND 9 in 2000 by the Internet Systems Consortium,
which still publishes it.

Today: every connection on the internet starts with a DNS query. BIND serves
a large share of the authoritative zones and several root servers, and the
zone file format it introduced is the one every other server reads.

Five containers, four lans, image *dns_trixie* (bind9 and frr)::

    cloonix cli nemo demo address/dns.py

Both resolvers forward to 10.255.0.3, a loopback of *dnstop* announced by
ospf. Every interface of the path is sniffed and the dns messages printed in
one time-ordered table; the second query is answered from the cache with
lanC silent. The servers need *ip_forward=1*, which the demo sets::

    cloonix cli nemo ssh dns1 "ip link set eth1 down"      the name stops resolving
    cloonix cli nemo sniff dns1 eth1 --dump udp port 53


address/unbound.py
==================

.. image:: /svg/demo_unbound.svg
   :align: center
   :width: 100%

Unbound was written by NLnet Labs, first released in 2008, with Wouter
Wijngaards as its main author: a validating, recursive, caching resolver, the
piece that turns a name into an address for a client, checks DNSSEC as it
goes, and remembers what it learnt. It was built as a clean, security-minded
alternative to BIND for the resolver job.

Today: Unbound is the default resolver of FreeBSD and OpenBSD, sits in
pfSense, OPNsense and Pi-hole setups, and answers for large public resolvers;
validating DNSSEC at the resolver is its usual role.

Three containers, one lan, image *unbound_trixie*::

    cloonix cli nemo demo address/unbound.py

The resolver, authoritative for one small local zone, answers a name, serves
the repeat from cache, and refuses a name it does not hold rather than
inventing one; the client then reaches the address it was given::

    cloonix cli nemo ssh cl "dig @10.0.0.53 www.shop.test"
    cloonix cli nemo sniff ns eth0 --dump udp port 53


address/knot.py
===============

.. image:: /svg/demo_knot.svg
   :align: center
   :width: 100%

Knot DNS is made by CZ.NIC, the Czech domain registry, the same house as the
BIRD routing daemon, with a first release around 2011. It is an
authoritative-only server: its job is to hold zones and answer for them, not
to resolve on a client's behalf, and CZ.NIC runs it for the .cz top-level
domain.

Today: Knot DNS serves the .cz zone and several other top-level domains and
runs on some root server instances; notify and transfer, shown here, are how
any zone with more than one server stays identical on all of them.

Three containers, one lan, image *knot_trixie*::

    cloonix cli nemo demo address/knot.py

A primary is given the zone; it notifies a secondary, the secondary transfers
the whole zone across, and from then on both answer the same data. A lookup
is UDP 53; the transfer is TCP 53::

    cloonix cli nemo ssh cl "kdig @10.0.0.3 www.shop.test"
    cloonix cli nemo sniff kp eth0 --dump tcp port 53


address/ntp.py
==============

.. image:: /svg/demo_ntp.svg
   :align: center
   :width: 100%

chrony was started by Richard Curnow in 1997 and is now maintained mainly by
Miroslav Lichvar at Red Hat. It implements NTP, the Network Time Protocol
whose design goes back to David Mills in the 1980s, and was written to keep
good time on machines that are not always connected and whose clocks drift.
It is the default time daemon on most linux distributions.

Today: chrony is the default time daemon of Red Hat, Fedora, SUSE and Ubuntu
server; a virtual machine that pauses and resumes, or a laptop that sleeps,
is the case it was written for.

Two containers, one lan, image *ntp_trixie*::

    cloonix cli nemo demo address/ntp.py

The server offers its own clock as a stratum-8 source; the client finds it,
marks it reachable and follows it. chronyd runs with *-x*: the clock of a
container is its host's and not its to set. NTP is one small request
and reply on UDP 123::

    cloonix cli nemo ssh ntpc "chronyc -n sources -v"
    cloonix cli nemo sniff ntps eth0 --dump udp port 123
