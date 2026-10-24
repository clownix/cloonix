=========================
Services and availability
=========================

services/proxy.py
=================

.. image:: /svg/demo_proxy.svg
   :align: center
   :width: 100%

A reverse proxy ends the client's connection and opens its own, an idea as old
as HTTP/1.1, RFC 2068 of 1997 by Roy Fielding and the others of the HTTP
working group, whose Via and Host headers exist for it. HAProxy was started by
Willy Tarreau in 2000 as a few hundred lines to spread load between servers
and has been the event-driven proxy of most large sites since. nginx, the web
server behind it, was written by Igor Sysoev from 2002 at Rambler, in Moscow,
and published in October 2004. X-Forwarded-For was invented by the Squid
developers in the 1990s and standardised late, as the Forwarded header of RFC
7239 in 2014.

Today: nearly every public web site is reached through a reverse proxy;
HAProxy fronts GitHub, Reddit and Stack Overflow, and the health check shown
here is what takes a dead server out of rotation without an operator.

Four containers, two lans, image *proxy_trixie* (haproxy, nginx, curl)::

    cloonix cli nemo demo services/proxy.py

One request, two connections: the client's ends at haproxy, which opens its
own to a backend with *X-Forwarded-For*. The third act stops web1 and times
how long the health check takes to notice::

    cloonix cli nemo sniff haproxy eth1 --dump tcp port 80


services/squid.py
=================

.. image:: /svg/demo_squid.svg
   :align: center
   :width: 100%

Squid descends from the Harvest project's cache of the early 1990s and has
been developed as Squid since 1996, long coordinated by Duane Wessels and
Henrik Nordstrom. For years it was how an office got one shared, cached path
to the web, and how it filtered what that path would carry. It is a forward
proxy, a reverse proxy and a cache in one.

Three containers, one lan, image *squid_trixie*::

    cloonix cli nemo demo services/squid.py

The client reaches the web only through the proxy: the first fetch is a cache
miss from the origin, the second a hit served from cache, and a forbidden
destination is refused with a 403 before it leaves the proxy::

    cloonix cli nemo ssh px "tail /var/log/squid/access.log"
    cloonix cli nemo sniff px eth0 --dump tcp port 3128


services/smtp.py
================

.. image:: /svg/demo_smtp.svg
   :align: center
   :width: 100%

Jon Postel wrote SMTP as RFC 821 in August 1982: a text dialogue, HELO, MAIL
FROM, RCPT TO, DATA, that still runs unchanged, the current text being RFC
5321 by John Klensin in 2008. The MX record that tells a sender which host
takes mail for a domain was added to the DNS by Craig Partridge in RFC 974 in
1986. Postfix was written by Wietse Venema at the IBM T.J. Watson Research
Center and released in December 1998 as a replacement for Eric Allman's
sendmail of 1983, built as a set of small processes that trust each other as
little as possible.

Today: every mail still crosses the internet as the SMTP dialogue of 1982,
now wrapped in TLS and checked by SPF, DKIM and DMARC; Postfix is the default
mail server of most linux distributions and relays a large share of it.

Five containers, three lans, image *smtp_trixie* (postfix, bind9)::

    cloonix cli nemo demo services/smtp.py

alice@a.lab sends to bob@b.lab; mailA asks dns for the MX of b.lab, then
the A of the host it names, then speaks SMTP to mailB. The demo writes
*/etc/aliases* and runs *newaliases*, and sets *maillog_file* since a
container has no mail.log::

    cloonix cli nemo sniff mailA eth1 --dump "tcp port 25 or udp port 53"
    cloonix cli nemo ssh userA "nc 10.1.0.1 25"


services/mqtt.py
================

.. image:: /svg/demo_mqtt.svg
   :align: center
   :width: 100%

MQTT was written in 1999 by Andy Stanford-Clark of IBM and Arlen Nipper of
Arcom to carry readings from oil pipeline sensors over satellite links that
were paid by the byte: a two-byte fixed header, a broker in the middle,
topics, and a quality of service per message. IBM released the specification
royalty free in 2010, OASIS standardised version 3.1.1 in 2014 and version 5
in 2019. Mosquitto was written by Roger Light in 2009 and has been an Eclipse
Foundation project since 2014.

Today: MQTT is the transport of most connected sensors, home automation (Home
Assistant, Zigbee2MQTT), car telemetry and the device gateways of AWS, Azure
and Google clouds; it has been an ISO standard since 2016.

Four containers, two lans, image *mqtt_trixie* (mosquitto)::

    cloonix cli nemo demo services/mqtt.py

One PUBLISH crosses lanA, two cross lanB; a wildcard subscription on
display2 alone; a retained message delivered to a display that connects
later. Clients speak MQTT 3.1.1 with *-V mqttv311*::

    cloonix cli nemo sniff broker eth1 --dump tcp port 1883


services/asterisk.py
====================

.. image:: /svg/demo_asterisk.svg
   :align: center
   :width: 100%

SIP was published by Henning Schulzrinne, Mark Handley, Eve Schooler and
Jonathan Rosenberg as RFC 2543 in 1999 and rewritten as RFC 3261 in 2002: a
text protocol built like HTTP, INVITE, 200 OK, ACK, BYE, that sets up a call
and lets RTP, RFC 1889 of 1996, carry the audio. Asterisk was started by Mark
Spencer in 1999 in Huntsville, Alabama, because a PBX for his linux support
company cost more than writing one; it became the product of Digium and is
published today by Sangoma. SIPp, the traffic generator on the two phones, was
written at Hewlett-Packard by Richard Gayraud and Olivier Jacques in 2004.

Today: SIP carries business telephony and the voice of 4G and 5G networks
(VoLTE); Asterisk and its FreePBX distribution run the switchboards of small
companies and call centres worldwide.

Three containers on one lan, image *asterisk_alpine* (asterisk, sipp,
sngrep)::

    cd vm_create/cnt_kvm_demo && ./cnt_asterisk_alpine.sh
    cloonix cli nemo demo services/asterisk.py

Call A: phone1 dials 100, asterisk plays a tone and the rtp counters prove
audio. Call B: phone1 dials 200, asterisk bridges to phone2 and relays the
BYE. Call C is judged on the wire: the INVITE seen by *sniff* before the
call is called a success, the BYE seen after *channel request hangup*::

    cloonix cli nemo sniff pbx eth0 --dump udp port 5060
    cloonix cli nemo ssh pbx sngrep


availability/vrrp.py
====================

.. image:: /svg/demo_vrrp.svg
   :align: center
   :width: 100%

Cisco patented the idea of two routers sharing one address as HSRP in 1994,
published as RFC 2281; the IETF wrote the open equivalent, VRRP, as RFC 2338
in 1998, RFC 3768 in 2004 and version 3 for IPv6 in RFC 5798 in 2010. The
master sends an advertisement every second to 224.0.0.18, protocol 112; when
the backups stop hearing it, the highest priority takes the address and
announces the move with a gratuitous ARP. Keepalived was written by Alexandre
Cassen in 2000, first as a health checker for the Linux Virtual Server, and
carries the VRRP implementation most linux routers use.

Today: the default gateway of most data-centre racks and many office lans is
a VRRP address; keepalived is also the usual front of linux load balancers,
holding the service address of an haproxy or nginx pair.

Four containers, two lans, image *vrrp_trixie* (keepalived, arping)::

    cloonix cli nemo demo availability/vrrp.py

rt1 priority 200, rt2 priority 100, virtual addresses 10.1.0.254 and
10.2.0.254. The demo kills keepalived on rt1 mid-ping and prints what both
lans heard: the last advertisement with priority 0, the backup's own, then
the gratuitous arp that moves the traffic::

    cloonix cli nemo sniff client eth0 --dump "ip and not udp and not tcp"

To measure a real outage, take rt1 off the lan instead of killing the
daemon: no resignation, three missed advertisements.


availability/cluster.py
=======================

.. image:: /svg/demo_cluster.svg
   :align: center
   :width: 100%

This is the long-running linux high-availability stack. Corosync, with a
lineage back through OpenAIS to the early 2000s, is the membership layer: it
tells every node who is currently in the cluster. Pacemaker, led for many
years by Andrew Beekhof, is the resource manager on top that decides where
each service runs and what to do when a node leaves. Both are under
ClusterLabs.

Today: Pacemaker and Corosync are the high-availability add-on of Red Hat,
SUSE and Debian, behind clustered databases, NFS heads and SAP installations;
fencing, absent from this two-node demo, is what production adds.

Two containers, one lan, image *cluster_trixie*::

    cloonix cli nemo demo availability/cluster.py

A floating service address must live on one node at a time; put the active
node into standby, as if it had failed, and Pacemaker moves the address to
the survivor. Corosync's membership traffic is UDP 5405::

    cloonix cli nemo ssh nodeA "crm_mon -1"
    cloonix cli nemo sniff nodeA eth0 --dump udp port 5405


watching/perf.py
================

.. image:: /svg/demo_perf.svg
   :align: center
   :width: 100%

The bandwidth-delay product measured here is the 1988 finding of Van Jacobson
at Lawrence Berkeley Laboratory, whose congestion control, written after
the congestion collapses of 1986, ties the throughput of a flow to its
window and its round trip. iperf began
at NLANR around 2003 as a portable heir of ttcp of 1984; iperf3 is the 2014
rewrite of the ESnet group at the same Berkeley laboratory, by Jon Dugan and
Brian Tierney's team. mtr, Matt's traceroute, was written by Matt Kimball in
1997 and merged traceroute, of Van Jacobson again, with ping into one screen;
Roger Wolff maintains it.

Today: iperf3 and mtr are the first two tools of a network engineer facing a
slow path, and the bandwidth-delay product is why a single TCP flow across an
ocean needs large windows.

Four containers, two lans, one a2b, image *perf_trixie* (iperf3, mtr,
socat)::

    cloonix cli nemo demo watching/perf.py

A baseline over one lan, then a routing hop, then delay, loss and a rate cap
on the cable, then two flows sharing the bottleneck::

    cloonix cli nemo ssh receiver "iperf3 -s -D"
    cloonix cli nemo ssh sender "iperf3 -c 10.0.2.10 -t 5"
    cloonix cli nemo ssh sender "mtr -r -c 5 10.0.2.10"
