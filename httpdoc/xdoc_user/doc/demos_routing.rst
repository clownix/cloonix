=======
Routing
=======

routing/frrspider.py and frrspider_bgp.py
=========================================

.. image:: /svg/demo_frrspider.svg
   :align: center
   :width: 100%

OSPF was designed by John Moy for the Internet Engineering Task Force, first
published as RFC 1131 in 1989 and settled as OSPFv2 in RFC 2328 in 1998, as an
open replacement for RIP: every router floods what it sees, every router holds
the same map and runs Dijkstra's shortest path algorithm of 1956 on it.
FRRouting descends from GNU Zebra, begun by Kunihiro Ishiguro in 1996,
continued as Quagga from 2002 and forked as FRR in 2017 under the Linux
Foundation; vtysh, the cisco-like shell used below, comes from Zebra.

Today: OSPF is the interior protocol of most enterprise and campus networks;
FRR runs in data-centre switches (SONiC, Cumulus), in cloud routers and in
the routing of many Kubernetes clusters.

BGP was sketched by Kirk Lougheed of Cisco and Yakov Rekhter of IBM at an IETF
meeting in January 1989, on napkins by legend, and published as RFC 1105 the
same year; BGP-4 of 1994 still runs the internet. Every network is an
autonomous system, and a router tells its neighbours which prefixes it can
reach and through which systems the path goes. Using it between the switches
of one data centre, one AS per switch, was described by Petr Lapukhov in RFC
7938 in 2016.

Today: BGP holds the internet together, close to a million IPv4 prefixes
exchanged between some seventy-five thousand autonomous systems, and the same
protocol runs inside data centres between racks.

Sixty routers in a web, image *frr_trixie*, configured in parallel::

    cloonix cli nemo demo routing/frrspider.py
    cloonix cli nemo ssh tod1 vtysh -c "show ip ospf neighbor"

*frrspider_bgp.py* is the same sixty, each its own autonomous system peering
with its neighbours, 280 sessions::

    cloonix cli nemo demo routing/frrspider_bgp.py
    cloonix cli nemo ssh tod1 vtysh -c "show bgp summary"

The demo asks for containers with no init and checks the count with *dump
commands* after the creation loop::

    the server has 59 of the 60 containers. Missing: nod22


routing/frrmix.py
=================

.. image:: /svg/demo_frrmix.svg
   :align: center
   :width: 100%

Two protocols with two authors. OSPF, John Moy, RFC 1131 in 1989: an interior
protocol where every router holds the map of its own network. BGP, Kirk
Lougheed and Yakov Rekhter, RFC 1105 the same year: an exterior protocol where
an autonomous system tells the next what it can reach and nothing more. The
split is the architecture of the internet since the early 1990s. FRRouting
runs both in one process family, descended from GNU Zebra begun by Kunihiro
Ishiguro in 1996, Quagga from 2002, forked as FRR in 2017 under the Linux
Foundation.

Today: the pair shown here is the usual shape of a real network: an interior
protocol inside each site, BGP between sites and toward providers, and
redistribution at the border.

Four autonomous systems in a ring, four routers each: ospf inside, ebgp
between the borders, ibgp between the two borders of an AS over their
loopbacks::

    cloonix cli nemo demo routing/frrmix.py
    cloonix cli nemo ssh i41 ping -c 3 10.1.0.1

Five pieces, each load bearing: loopbacks advertised into ospf, ibgp on the
loopback, *next-hop-self* on the borders, a default originated into ospf,
and *ip route 10.n.0.0/16 blackhole* so the aggregate is real. Remove one
and every session is up and carries nothing. *NB_AS* at the top of the
script is the size knob::

    cloonix cli nemo sniff b12 eth2 --dump tcp port 179


routing/bird.py
===============

.. image:: /svg/demo_bird.svg
   :align: center
   :width: 100%

BIRD, the BIRD Internet Routing Daemon, has been developed at CZ.NIC since
1998, led for years by Ondrej Filip and Martin Mares. It is one of the two
open routing daemons carrying real internet routes at scale: many route
servers at internet exchanges run BIRD. Where frr descends from Zebra and
mimics a vendor's command line, BIRD has a configuration language of its own.

Today: BIRD is the route server of most large internet exchange points, DE-
CIX, AMS-IX and LINX among them, where one daemon holds hundreds of BGP
sessions and millions of paths.

Seven containers, four lans, image *bird_trixie*::

    cloonix cli nemo demo routing/bird.py

Three BIRD routers in a line speak OSPF, the same protocol frr speaks in the
routing/frr demos, so hA reaches hB across three hops none was told about;
cut the middle link and the far side goes away, proving the route was OSPF's::

    cloonix cli nemo ssh r2 "birdc show route"
    cloonix cli nemo sniff r1 eth1 --dump "ip and not udp and not tcp"


routing/isis.py
===============

.. image:: /svg/demo_isis.svg
   :align: center
   :width: 100%

IS-IS was written at Digital Equipment Corporation in the 1980s for DECnet
Phase V, largely by Radia Perlman and Dave Oran, and became the ISO standard
10589 for the OSI protocols. RFC 1195, by Ross Callon in 1990, taught it to
carry IP routes. The OSI network layer it was made for disappeared; the
routing protocol stayed, because it runs beside IP and not over it, and
carrying a new address family, IPv6 then MPLS labels, took a new field and
no new protocol.

Today: IS-IS is the interior protocol of most large carriers and of many
data centre fabrics, where OSPF holds the enterprise. The daemon here is
isisd of frr.

Five containers, five lans, image *frr_trixie*::

    cloonix cli nemo demo routing/isis.py

Three routers in a triangle, a host behind *r1* and one behind *r3*. The
direct side costs 10 and carries the traffic; it is cut, the link states are
flooded, the traffic goes through *r2*; it is restored and taken again.
IS-IS frames carry no IP header, the filter reaches them by exclusion::

    cloonix cli nemo ssh r1 "vtysh -c 'show isis neighbor'"
    cloonix cli nemo ssh r2 "vtysh -c 'show isis database'"
    cloonix cli nemo sniff r1 eth2 --dump "not ip and not ip6 and not arp"


routing/babel.py
================

.. image:: /svg/demo_babel.svg
   :align: center
   :width: 100%

Babel was designed by Juliusz Chroboczek around 2007, standardised as RFC
6126 and later RFC 8966. It is a distance-vector protocol, like the old RIP,
but built to be loop-free and to work over links that appear, vanish and vary
in quality, wireless meshes especially. Where OSPF floods a full map, Babel
keeps only distances and reasons about them carefully enough never to loop.

Today: Babel routes community mesh networks and is the routing protocol of
the IETF Homenet architecture; it ships in OpenWrt as babeld and inside BIRD
and FRR.

Six containers, five lans, image *babel_trixie*::

    cloonix cli nemo demo routing/babel.py

A diamond with two disjoint paths: Babel picks one, and when it is cut moves
to the other with no static route and no operator. It speaks UDP 6696::

    cloonix cli nemo ssh r1 "ip route"
    cloonix cli nemo sniff r4 eth0 --dump udp port 6696


routing/batman.py
=================

.. image:: /svg/demo_batman.svg
   :align: center
   :width: 100%

B.A.T.M.A.N., Better Approach To Mobile Ad-hoc Networking, came out of the
Freifunk community wireless movement in Germany around 2006. Its advanced
form, batman-adv, moved the whole protocol into the linux kernel and down to
layer 2: the mesh carries ethernet frames, so anything above runs across it
none the wiser. It is maintained with the kernel by the Open-Mesh project.

Today: batman-adv carries the Freifunk city meshes of Germany, with tens of
thousands of nodes, and is part of the mainline kernel since 2.6.38; OpenWrt
routers run it unchanged.

Four containers, three lans, image *batman_trixie*::

    cloonix cli nemo demo routing/batman.py

Four nodes in a chain reach each other over one interface, bat0, though no
node is cabled to more than two others; take the middle out and the chain
splits. The OGMs it floods use ethertype 0x4305, with no IP in them::

    cloonix cli nemo ssh n1 "batctl o"
    cloonix cli nemo sniff n2 eth0 --dump
