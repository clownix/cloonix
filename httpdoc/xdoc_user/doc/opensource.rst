====================================
The open sources cloonix is built on
====================================

Cloonix is a set of C daemons and a python client; the work of emulating a
machine, switching a frame, running a container, reaching the internet or
decoding a packet is done by projects it recompiles into
*/usr/libexec/cloonix/cloonfs*. This chapter says where each one comes from
and who wrote it, as the demos chapter does for the software the demos run.


qemu and kvm, the kvm item
==========================

QEMU was written by Fabrice Bellard and released in 2003, a machine
emulator that translates the guest's instructions into the host's as it
runs; Bellard is also the author of FFmpeg and of the Tiny C Compiler. KVM
was written by Avi Kivity at Qumranet in Israel in 2006 and merged in linux
2.6.20 in February 2007: the kernel exposes the virtualisation extensions of
the processor as */dev/kvm* and qemu uses them to run the guest's code
without translating it. The virtio devices a cloonix guest sees, its disc and
its network interfaces, were designed by Rusty Russell in 2007 as one
device model for every hypervisor; *vtnet0* of the freebsd guest is the same
device. The qcow2 disc format of bulk is qemu's own, from 2006. QEMU is the
work of a community led since Bellard by Anthony Liguori, then Peter
Maydell and Paolo Bonzini.


openvswitch, the lan
====================

Open vSwitch was started in 2009 at Nicira Networks by Martin Casado, Ben
Pfaff and Justin Pettit, after Casado's doctoral work at Stanford on
OpenFlow, a switch whose forwarding table is programmed from outside.
Its kernel datapath was merged in linux 3.3 in 2012, Nicira became part of
VMware the same year, and the project has been hosted by the Linux
Foundation since 2016. A cloonix lan is one of its bridges; the
*cloonix-ovs-vswitchd* of the private namespace is its vswitchd, and
*dump ovs-vsctl* reads it as the ovs chapter shows.


crun, the cnt item and the toy
==============================

A container is a process with its own view of the kernel: namespaces,
written into linux from 2002 by Eric Biederman among others, and cgroups,
by Paul Menage and Rohit Seth at Google in 2007. Docker, released by
Solomon Hykes in 2013, made the assembly usable and its runtime became
runc, the reference implementation of the Open Container Initiative
founded in 2015. crun was written by Giuseppe Scrivano at Red Hat in 2017
as an implementation of the same specification in C, one small binary
where runc is a Go program. Cloonix creates every cnt with it, and way 2 of
the install runs cloonix itself inside a crun container.


passt, the nat item
===================

User-mode networking, a tcp/ip stack in a process that needs no privilege,
was written by Danny Gasparovski in 1995 as SLiRP, so that a dial-up shell
account could carry a ppp link; qemu adopted it in 2004 as its default
network. passt was written by Stefano Brivio at Red Hat in 2021 to replace
it: it moves the bytes of each connection between the guest's sockets and
sockets of its own on the host, forwards nothing at the ip layer, and holds
no privilege. The cloonix nat is one passt per nat item.


the dissection engine of wireshark, the capture
===============================================

A capture of cloonix is a pcap stream, the format of libpcap, written in
1988 at Lawrence Berkeley Laboratory by Van Jacobson, Craig Leres and Steven
McCanne together with tcpdump. The capture filter of *sniff*, *udp port 53*,
is written in the language of tcpdump, that of the BPF of McCanne and
Jacobson from 1992; cloonix reads a subset of it and applies it in its main
server, *pcap_filter.c*. Decoding the packets is Wireshark, begun by Gerald Combs in 1998 under the name
Ethereal and renamed in 2006 when Combs changed employer and the old name
stayed behind. Cloonix compiles its dissectors without the Qt interface:
webshark shows the result in the browser, and PcapRec records the captures
as pcap files.


openssl, the two ports
======================

Both ports of a net are tls. The library is OpenSSL, which began in 1995
as SSLeay by Eric A. Young and Tim Hudson in Australia, became OpenSSL in
December 1998, and has been run by the OpenSSL Software Foundation since
the Heartbleed bug of 2014 brought it funding and full-time developers.
TLS itself is the IETF's 1999 rewrite of Netscape's SSL of 1994.


dtach, the serial console
=========================

The console of a kvm, and the way *cisco.py* drives a guest that has no
agent, go through dtach, written by Ned T. Crigler in 2004: the detach and
reattach of GNU screen, a few hundred lines, and nothing else. GNU screen
itself dates from 1987, by Oliver Laumann.


patchelf, one tree of libraries
===============================

patchelf was written by Eelco Dolstra in 2004 for the Nix package manager,
whose packages each carry their own dependencies and therefore need the
interpreter and the rpath of every binary rewritten. Cloonix uses it at
packing time for the same reason, so that nothing in cloonfs opens a
library of the host.


noVNC and xterm.js, the browser
===============================

VNC and its RFB protocol were written at the Olivetti Research Laboratory
in Cambridge by Tristan Richardson and Quentin Stafford-Fraser and released
in 1998; noVNC, by Joel Martin in 2010, speaks RFB from a browser over a
websocket and is the desktop of a kvm in the web GUI. xterm.js, begun in
2016 at SourceLair from the term.js of Christopher Jeffrey, is the terminal
of the shell panels; the same component is the terminal of Visual Studio
Code.


debian, the guest images
========================

The container and kvm images of bulk are Debian, the distribution founded
by Ian Murdock in August 1993 and run since by its developers under a
social contract of 1997; trixie is its release 13. The base root
file-system is assembled by mmdebstrap, written by Johannes Schauer Marin
Rodrigues in 2018 to build a Debian tree without root.
