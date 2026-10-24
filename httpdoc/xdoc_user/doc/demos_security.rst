==============================
Encryption, tunnels, filtering
==============================

security/tls.py
===============

.. image:: /svg/demo_tls.svg
   :align: center
   :width: 100%

SSL was written at Netscape in 1994 by Taher Elgamal and his team so that a
card number could be typed into a browser; SSL 3.0, by Paul Kocher, Phil
Karlton and Alan Freier in 1996, is the design everything after it kept.
The IETF renamed it TLS in 1999. Eric Young and Tim Hudson wrote SSLeay in
Australia from 1995, a free implementation outside the export rules of the
United States; in 1998 it became OpenSSL. TLS 1.3, RFC 8446 by Eric Rescorla
in 2018, removed every old cipher and encrypts the certificate itself.

Today: OpenSSL is in nearly every server of the internet, and more than
nine pages out of ten load over https, a share that Let's Encrypt, giving
certificates for free since 2015, took from under a half.

Two containers, one lan, image *openvpn_trixie*, used for its openssl, curl
and python3::

    cloonix cli nemo demo security/tls.py

*web* makes a key and a self-signed certificate with openssl and serves one
page on port 8080 in http and on port 8443 in https. The demo keeps the bytes
caught on *web* 0 during each fetch and looks for the string of the page:
present on 8080, absent on 8443. The client then fetches trusting another
certificate, one it made itself, and curl refuses, exit 60::

    cloonix cli nemo sniff web eth0 --dump tcp port 8080
    cloonix cli nemo sniff web eth0 --dump tcp port 8443
    cloonix cli nemo ssh client "openssl s_client -connect 10.0.0.2:8443 </dev/null"


security/wg.py
==============

.. image:: /svg/demo_wg.svg
   :align: center
   :width: 100%

WireGuard was written by Jason A. Donenfeld and published in 2016, and merged
into linux 5.6 in March 2020 after Linus Torvalds called its few thousand
lines a work of art next to the ipsec code. It negotiates nothing: one key
exchange from the Noise framework of Trevor Perrin, Curve25519 by Daniel J.
Bernstein, ChaCha20-Poly1305 for the data, and a peer is a public key with a
list of allowed addresses. The 1-RTT handshake caught below is the whole
protocol.

Today: WireGuard is in the linux, windows, macos, ios and android of
commercial VPN providers, in Tailscale and Netbird meshes and in kubernetes
network plugins; its code is about four thousand lines, where IPsec stacks
count hundreds of thousands.

Four containers, three lans, image *wg_trixie*::

    cloonix cli nemo demo security/wg.py

The same fetch three times: without a tunnel, the string served by hostB is
readable on all three wires; with it, lanC carries wireguard data packets
and the string is searched for in the raw bytes and found on lanA and lanB
only. Nothing changed on the hosts::

    cloonix cli nemo sniff gwA eth1 --dump udp port 51820


security/openvpn.py
===================

.. image:: /svg/demo_openvpn.svg
   :align: center
   :width: 100%

OpenVPN was written by James Yonan and first released in 2001. It runs
entirely in user space on top of a tun device and uses OpenSSL for its
cryptography; on the wire it is one UDP flow, which crosses NAT and
firewalls as any other. For two decades it was the default do-it-yourself
VPN before WireGuard; the wg demo in this area is the modern comparison.

Two containers, one lan, image *openvpn_trixie*::

    cloonix cli nemo demo security/openvpn.py

Its simplest mode, one pre-shared static key and two peers: a private
10.8.0.0/24 rides inside a single UDP flow on port 1194, and the ping over it
is not readable on the underlay::

    cloonix cli nemo ssh ovs "ip addr show tun0"
    cloonix cli nemo sniff ovs eth0 --dump udp port 1194


security/tinc.py
================

.. image:: /svg/demo_tinc.svg
   :align: center
   :width: 100%

tinc was started by Guus Sliepen and Ivo Timmermans in the late 1990s. Its
distinctive idea is the automatic full mesh: tell each node how to reach one
or two others and tinc works out direct paths between every pair, routing
around a node that goes down: a peer-to-peer VPN, where a classic OpenVPN
server is a hub with spokes.

Today: tinc links small fleets of servers and home networks where no node can
be the hub; its mesh idea is the one later tools such as ZeroTier, Nebula and
Tailscale built products on.

Two containers, one lan, image *tinc_trixie*::

    cloonix cli nemo demo security/tinc.py

Each node has a host file with its public key; the nodes exchange them and
connect, and the ping over the VPN rides port 655 while staying off the wire
in clear::

    cloonix cli nemo ssh na "ping -c3 10.9.0.2"
    cloonix cli nemo sniff na eth0 --dump port 655


security/ipsec.py
=================

.. image:: /svg/demo_ipsec.svg
   :align: center
   :width: 100%

IPsec is the IETF standard for encryption at the IP layer, RFC 4301 with the
IKEv2 key exchange in RFC 7296. Two long-lived open stacks implement it on
linux: strongSwan, led by Andreas Steffen, and Libreswan, led by Paul Wouters
and shipped by Red Hat. They are separate code bases that must agree on the
wire: pairing them tests the standard, not one code base.

Today: IPsec carries most site-to-site VPNs between companies and cloud
providers, the voice of 4G and 5G phones toward their operator (VoWiFi), and
the IKEv2 client built into windows, macos, ios and android.

Two containers, one lan, images *strongswan_trixie* and *libreswan_trixie*::

    cloonix cli nemo demo security/ipsec.py

strongSwan on one gateway and Libreswan on the other agree on IKEv2 and a
shared secret, then carry the ping between them as ESP, IP protocol 50; the
ICMP inside is no longer readable on the wire::

    cloonix cli nemo ssh gwA "swanctl --list-sas"
    cloonix cli nemo sniff gwA eth0 --dump "ip and not tcp and not udp and not icmp"


security/nftables.py
====================

.. image:: /svg/demo_nftables.svg
   :align: center
   :width: 100%

Packet filtering in linux was rewritten three times by the same community:
ipfwadm, then ipchains by Rusty Russell in 1998, then the netfilter framework
and iptables, by Rusty Russell and Harald Welte, in kernel 2.4 of 2001.
nftables is the fourth: Patrick McHardy presented it in 2008 as one virtual
machine in the kernel replacing the separate ip, ip6, arp and ethernet tables,
it was merged in kernel 3.13 in January 2014, and Pablo Neira Ayuso leads the
netfilter project that maintains it. Connection tracking, the state seen here,
dates from the iptables generation.

Today: nftables is the default packet filter of Debian, Ubuntu, Fedora and
RHEL; firewalld, docker and kubernetes rules end up in it, and the iptables
command of those systems is a translator to it.

Four containers, two lans, image *nft_trixie*::

    cloonix cli nemo demo security/nftables.py

A stateful filter, masquerade of the private range, a dnat that picks one
of two inside hosts, named counters saying which rule matched, and a
connection killed by deleting one row of a table::

    cloonix cli nemo ssh fw "nft list ruleset"
    cloonix cli nemo sniff fw eth1 --dump tcp


security/suricata.py
====================

.. image:: /svg/demo_suricata.svg
   :align: center
   :width: 100%

Suricata is made by the Open Information Security Foundation, first released
in 2010. It is a network intrusion detection and prevention engine: it reads
packets, reassembles flows, and matches them against rules that describe
known-bad traffic, writing structured alerts. It was multi-threaded from the
start and its rule language is broadly compatible with the older Snort.

Two containers, one lan, image *suricata_trixie*::

    cloonix cli nemo demo security/suricata.py

The sensor runs on its interface with two rules; a ping fires the ICMP rule
and a web request for a watched path fires a rule that matches on content,
not just a port, which is the deep inspection an IDS does::

    cloonix cli nemo ssh sen "cat /var/log/suricata/fast.log"
    cloonix cli nemo sniff sen eth0 --dump icmp


security/ebpf.py
================

.. image:: /svg/demo_ebpf.svg
   :align: center
   :width: 100%

eBPF grew out of the Berkeley Packet Filter that tcpdump has used since the
1990s. From 2014 Alexei Starovoitov and Daniel Borkmann turned it into a
general way to run safe, verified programs inside the kernel. XDP, the
eXpress Data Path, is the hook that runs on a raw frame at the driver, before
any allocation: the earliest point of the kernel where a packet can be
dropped or redirected.

Two containers, one lan, image *ebpf_trixie*::

    cloonix cli nemo demo security/ebpf.py

A small program is compiled from C on the machine and attached to a card with
XDP; it drops ICMP at the wire, so the echo requests still arrive on the wire
but nothing is answered, and detaching it restores the reply::

    cloonix cli nemo ssh h2 "bpftool prog show"
    cloonix cli nemo sniff h2 eth0 --dump icmp


security/radius.py
==================

.. image:: /svg/demo_radius.svg
   :align: center
   :width: 100%

RADIUS, Remote Authentication Dial-In User Service, was written by Carl
Rigney at Livingston Enterprises in 1991 and standardised as RFC 2865. It is
the protocol behind logging in and being let onto a network: a device that
guards access asks a central server whether credentials are good, and the
server answers accept or reject. FreeRADIUS, led for years by Alan DeKok, is
the open implementation run here.

Two containers, one lan, image *radius_trixie*::

    cloonix cli nemo demo security/radius.py

The server knows one user and allows one client to ask; the client sends an
access request with the right password and is accepted, then a wrong one and
is rejected. Authentication is UDP 1812::

    cloonix cli nemo ssh rac "radtest alice secret123 10.0.0.2 0 clownixRadiusSecret 0 10.0.0.10"
    cloonix cli nemo sniff ras eth0 --dump udp port 1812


security/arp.py
===============

.. image:: /svg/demo_arp.svg
   :align: center
   :width: 100%

ARP was written by David Plummer in 1982, RFC 826, one page: a machine that
needs the mac behind an IP shouts the question on the wire, the owner
answers. It has no notion of who may answer, and a host takes the newest
reply, so a machine on the lan can claim any address by saying so.
The attack was folklore for years; Dug Song made it a command in 1999 with
dsniff, whose arpspoof is used here. The defence, dynamic ARP inspection on
the switch, checking arp against the DHCP leases, came much later and lives
in the switch, not in the protocol.

Today: arp is unchanged and still trusted on every IPv4 lan; the poisoning
is a standard step of a man-in-the-middle on a local network, and the reason
a network pins its gateway and inspects arp at the switch. IPv6
replaces arp with neighbour discovery, protected by SEND, rarely deployed.

Three containers, one lan, image *arp_trixie* (dsniff for arpspoof, arping)::

    cloonix cli nemo demo security/arp.py

A victim, its gateway, and an attacker on one lan. The victim resolves the
gateway honestly; the attacker runs *arpspoof* and the victim's entry for the
gateway moves to the attacker's mac, a machine in the middle; a static entry
pins the gateway back and the same forgery no longer moves it. arp has its
own ethertype::

    cloonix cli nemo sniff victim eth0 --dump arp
    cloonix cli nemo ssh victim "ip neigh show dev eth0"
    cloonix cli nemo ssh attacker "arpspoof -i eth0 -t 10.0.0.10 10.0.0.1"
