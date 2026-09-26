=========================================
Switching: vlans, spanning tree, overlays
=========================================

switching/vlan.py
=================

.. image:: /svg/demo_vlan.svg
   :align: center
   :width: 100%

IEEE 802.1Q, ratified in 1998, puts four bytes inside the ethernet header: a
tag protocol id 0x8100 where the ethertype used to be, and a 12-bit vlan id,
so one wire carries up to 4094 separate networks. The first linux
implementation was written by Ben Greear in 1999 and went into kernel 2.4 with
its vconfig tool; the eth0.10 naming used here dates from it, and the ip link
command of iproute2 has replaced vconfig since.

Today: every enterprise switch port is in a vlan; the same tag separates
voice from data on a desk, tenants in a hosting rack and wifi networks on one
access point cable.

Five containers on one lan, image *trixie*; addresses on *eth0.10* and
*eth0.20*, *rtr* on both::

    cloonix cli nemo demo switching/vlan.py
    cloonix cli nemo ssh a10 "ip link add link eth0 name eth0.10 type vlan id 10"

A vlan tag puts 0x8100 where the ethertype is read, so on the trunk every
filter above the link layer matches nothing; the demo measures it::

    cloonix cli nemo sniff a10 eth0 --dump icmp       nothing
    cloonix cli nemo sniff a10 eth0 --dump            the tagged frames


switching/stp.py
================

.. image:: /svg/demo_stp.svg
   :align: center
   :width: 100%

Spanning tree was written by Radia Perlman at Digital Equipment Corporation in
1985, in one afternoon by her own account, with a poem in the paper that
states the algorithm in verse. Bridges elect a root, each keeps its cheapest
path to it and blocks every other port, so a loop of wires becomes a tree
without anybody drawing one. IEEE 802.1D adopted it in 1990; the rapid
version, 802.1w, came in 2001 and is what the kernel runs today. The bridge
under study is the one of the linux kernel, written by Lennert Buytenhek in
2000 for the 2.4 series, driven here with the bridge command of iproute2.

Today: every managed switch ships with spanning tree enabled, in its rapid
form 802.1w since 2001; a second cable plugged between two office switches is
the loop it silently removes.

Two containers each running a kernel bridge, two lans between them, image
*trixie*: a loop. The demo keeps it unprotected for three seconds, counts
the storm and ends it by taking a port down; then spanning tree elects a
root and blocks one port, and the forwarding wire is cut under a ping::

    cloonix cli nemo demo switching/stp.py
    cloonix cli nemo ssh br1 "bridge link"
    cloonix cli nemo sniff br1 eth0 --dump


switching/vxlan.py
==================

.. image:: /svg/demo_vxlan.svg
   :align: center
   :width: 100%

VXLAN was written by VMware, Cisco, Arista and others and published as RFC
7348 in 2014, with Mallik Mahalingam and Dinesh Dutt among its authors. A
data centre had run out of the 4094 VLAN tags of 802.1Q long before it ran
out of tenants, and moving a virtual machine meant its layer 2 network had
to follow it across routers. VXLAN answers both: a 24-bit identifier, sixteen
million networks, and the frames wrapped in UDP so any routed IP fabric
carries them unchanged. The linux side was written by Stephen Hemminger and
David Stevens.

Today: VXLAN is the overlay of nearly every large data centre and of the
public clouds, usually with EVPN, a BGP family, distributing the addresses in
place of the flood-and-learn this demo shows. It is the transport OVN builds
on, the subject of switching/ovn.py.

Five containers, three lans, image *frr_trixie*::

    cloonix cli nemo demo switching/vxlan.py

Two sites on 10.0.0.0/24, joined by a routed core that does not carry that
subnet. Each site has a VTEP; a vxlan interface, vni 100, wraps the site's
frames in udp 4789 and sends them to the far VTEP across the core. The hosts
believe they share one wire; the arp of one crosses the core as a unicast
datagram to the other::

    cloonix cli nemo sniff vtep1 eth1 --dump udp port 4789
    cloonix cli nemo ssh vtep1 "bridge fdb show dev vxlan100"

  Needs the vxlan module on the host, as kvm needs kvm_intel::

    sudo modprobe vxlan


switching/ovn.py
================

.. image:: /svg/demo_ovn.svg
   :align: center
   :width: 100%

OVN, the Open Virtual Network, was started around 2015 by the Open vSwitch
team, Ben Pfaff, Justin Pettit and others, to put a control plane on top of
Open vSwitch: a logical network described once, in a northbound database, and
realised across many hosts by a controller on each. It is the network layer
under much of OpenStack and Kubernetes networking.

Three containers, one lan, image *ovn_trixie*::

    cloonix cli nemo demo switching/ovn.py

One logical switch whose two ports land on different hypervisors, joined by a
Geneve overlay so they share a layer-2 network with no cable between them;
cut the underlay of one host and the overlay goes with it::

    cloonix cli nemo ssh ovnc "ovn-nbctl show"
    cloonix cli nemo sniff hv1 eth0 --dump udp port 6081
