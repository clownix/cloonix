==================
User documentation
==================

:Version: |version|
:Date: |today|

Cloonix is a network laboratory on one computer. It builds machines, cables
and switches out of virtual machines, containers and Open vSwitch bridges,
runs on them the open source software that runs the internet, and shows
every packet that crosses a cable. It is made for learning: 69 demos, from a
ping between two containers to BGP, WireGuard and wifi, each one a running
network to look at, break and repair, with the history of its protocol.

Where to start, in this order:

  1. *Instructions for installation*, or *Cloonix toy* for an installation
     with no root at all.
  2. *The first network*: two containers and a ping, typed or played.
  3. *Learning with cloonix*: the course, twelve packages of demos, each
     using only what the packages before it showed.

This book is for the person who uses cloonix; the
`developer book <../build_dev/dev.html>`_ is for the person who changes it.


One command line
================

.. image:: /svg/cli_grammar.svg
   :width: 100%

Everything in this book is done with lines of this shape; the canvas of the
browser does the same with the mouse. The chapter *The command line, one
page* holds them all. Two containers on one lan and a ping, the net *nemo*
started first by *cloonix start nemo* in a terminal of its own::

    cloonix cli nemo add cnt cnt1 eth=1 trixie
    cloonix cli nemo add cnt cnt2 eth=1 trixie
    cloonix cli nemo plug cnt1 eth0 lan1
    cloonix cli nemo plug cnt2 eth0 lan1
    cloonix cli nemo ssh cnt1 "ip addr add dev eth0 1.1.1.1/24"
    cloonix cli nemo ssh cnt1 "ip link set dev eth0 up"
    cloonix cli nemo ssh cnt2 "ip addr add dev eth0 1.1.1.2/24"
    cloonix cli nemo ssh cnt2 "ip link set dev eth0 up"
    cloonix cli nemo ssh cnt1 "ping -c 3 1.1.1.2"
    cloonix cli nemo del all

From a real network to its model
================================

A network is machines talking to each other through cables. The smallest
one is a switch and the machines cabled to it: one **ethernet segment**.
Every interface on it has a mac address; a frame carries the mac of its
destination and the mac of its source; the switch learns which mac is
behind which port and forwards a unicast frame by that port alone, a
broadcast or an unknown destination by every other port. A pc, a printer,
a server on the same segment reach each other with nothing but that.

.. image:: /svg/real_lan.svg
   :align: center
   :width: 100%

The internet is not one segment. It is a chain of them, and at every
junction stands a **router**, a machine with an interface on each of the
segments it joins. A packet from a pc to a server crosses the segment of the
home, the access link of the provider, a few links between core routers, the
segment of the data centre. On each link the packet travels inside a new
frame, addressed to the mac of the next router; the ip addresses inside it
stay the same from the first segment to the last. That is the division of
work between the two layers: ethernet moves a frame across one segment, ip
moves a packet across many.

.. image:: /svg/real_internet.svg
   :align: center
   :width: 100%

Each router decides the next segment by its routing table, and the routing
tables are kept up to date by routing daemons talking to each other: ospf
inside one operator, bgp between operators. A name is turned into an address
by dns servers somewhere on that same graph, an address is handed out by a
dhcp server, a tunnel or a firewall stands on some link. Every piece of the
internet is a machine, a cable or a program running on a machine.

Cloonix builds this on one computer. Each real piece has one item of the
model:

  * a machine is a **kvm**, a virtual machine with its own kernel, or a
    **cnt**, a container with a network stack of its own
  * a switch and its cables are a **lan**, an openvswitch bridge: a frame
    entering by one port leaves by the port that learnt the destination, or
    by every other port when it is broadcast or unknown, as on the real
    switch
  * a router is a kvm or a cnt with several interfaces, running the routing
    daemon a carrier runs, frr, or the kernel forwarding alone
  * a cable is *plug*: one interface of one item plugged into one lan
  * a probe on a cable is *sniff* on one interface, the packets handed back
    with the capture filter applied in the switch
  * the way out toward the real internet is a **nat** item

.. image:: /svg/real_to_model.svg
   :align: center
   :width: 100%

The path of the figure above becomes, in cloonix, one container per
machine and one lan per cable:

.. image:: /svg/model_topology.svg
   :align: center
   :width: 100%

::

    cloonix cli nemo add cnt pc  eth=1 trixie
    cloonix cli nemo add cnt r1  eth=3 frr_trixie
    cloonix cli nemo add cnt r2  eth=2 frr_trixie
    cloonix cli nemo add cnt r3  eth=2 frr_trixie
    cloonix cli nemo add cnt srv eth=1 trixie
    cloonix cli nemo add nat nat1
    cloonix cli nemo plug pc  eth0 lan1
    cloonix cli nemo plug r1  eth0 lan1
    cloonix cli nemo plug r1  eth1 lan2
    cloonix cli nemo plug r2  eth0 lan2
    cloonix cli nemo plug r2  eth1 lan3
    cloonix cli nemo plug r3  eth0 lan3
    cloonix cli nemo plug r3  eth1 lan4
    cloonix cli nemo plug srv eth0 lan4
    cloonix cli nemo plug r1  eth2 natlan
    cloonix cli nemo plug nat1 natlan

Nothing in it is simulated: the frames on *lan2* are ethernet frames on a
bridge, the routing table of *r2* is the one frr computed, and a capture on
*r2* eth1 shows the mac of *r3* as destination and the ip of *srv*
unchanged, as it would on the wire. The demos are such networks, written
down, each with the packet that shows its protocol at work.


Architecture
============

Cloonix is a set of C daemons and one python client. It carries the open
sources it uses, compiled into the delivery: **qemu-kvm** for the virtual
machines, **openvswitch** for the lans, **crun** for the containers,
**passt** for the nat, and the dissection engine of **wireshark**, without
Qt, for the capture viewer. Nothing comes from the host: every binary and
library cloonix uses is in */usr/libexec/cloonix/cloonfs*. The chapter *The
open sources cloonix is built on* says where each comes from and who wrote
it.

.. image:: /svg/overview.svg
   :align: center
   :width: 100%

One net is one server, **cloonix-main-server**, started by *cloonix start
<net>* with a net name of *cloonix.cfg*. It holds the topology, starts the
helpers, and is reached two ways:

  * *cloonix cli <net> ...* over the tcp port 45211, every frame carrying an
    hmac of the password of the net;
  * a browser over the https port 4444, with the net name and its password:
    user *nemo*, password *nemoclown*, in
    */usr/libexec/cloonix/cloonfs/etc/cloonix.cfg*.

**cloonix-proxymous** binds both ports, ends the tls and hands the bytes on
over unix sockets: the doors frames to **cloonix-doorways**, which checks
the hmac, the http to **cloonix-gui-server**. The lans and the guest
interfaces live in a private network namespace, *cloonix_<net>*, where
**cloonix-ovs-vswitchd** runs; privileged operations go through
**cloonix-suid-power**, the single setuid binary of the delivery.

The client is one command, *cloonix*, python with the standard library only
and a helper file beside it for each large verb; it runs on linux, macos and
windows. Its top level::

    cloonix list                  every net of the config, and the versions
    cloonix start <net>           start the server of a net on this machine
    cloonix stop <net>            delete every object of the net, stop its server
    cloonix cli <net> <verb> ...  everything done to one net


Two ways to install
===================

.. image:: /svg/two_ways.svg
   :align: center
   :width: 100%

**Way 1, the bundle**, *cloonix-bundle.tar.gz*: root once, at install
time, to write */usr/libexec/cloonix/cloonfs*, */usr/bin/cloonix* and the
setuid bit of *cloonix-suid-power*. Cloonix then runs as ordinary processes
of the user.

**Way 2, the toy**, *cloonix-toy.tar.xz*: no root at install, no root
at run. Cloonix itself runs inside a crun container created by the user, and
everything stays under the extracted directory.

The toy also comes specialised for one open source project:
*cloonix-frr-toy.tar.xz*, *cloonix-bird-toy.tar.xz*,
*cloonix-suricata-toy.tar.xz* and *cloonix-bind-toy.tar.xz*, containers only,
the project in the root of the toy, its demos and the examples of its own
documentation. The chapter of the toy gives them, in *The toys for frr, BIRD,
Suricata and BIND*.

Same verbs and same canvas in both, and the same demos but six that a toy
started by a user cannot play, named in the chapter of the toy. The crun of the toy wraps
cloonix; it is distinct from the crun containers cloonix creates for the
**cnt** guests, which exist in both ways.


One tree of libraries
=====================

.. image:: /svg/patchelf.svg
   :align: center
   :width: 100%

At packing time **patchelf** rewrites every binary and library of the
delivery: its interpreter is the loader inside *cloonfs*, its rpath the
library directories inside *cloonfs*. No cloonix binary opens
*/lib64/ld-linux-x86-64.so.2* or anything under */usr/lib*, so the same
tar.gz runs on debian, fedora, arch and on a distribution released after the
build, and removing */usr/libexec/cloonix* removes cloonix.


User view
=========

.. image:: /svg/items.svg
   :align: center
   :width: 100%

The items, all cabled to each other through a **lan**::

    kvm   virtual machine driven by qemu-kvm, rootfs is a qcow2 file of bulk
    cnt   container driven by crun, rootfs is a directory or a .tar.gz of bulk
    nat   the way out toward the internet, built on passt
    tap   a veth pair giving cloonix an interface in the host
    phy   a physical interface of the host brought into cloonix
    c2c   a cable to another cloonix net, over udp
    a2b   a cable that degrades what crosses it: loss, delay, rate
    lan   the crossroad, an openvswitch bridge

An item owns **ports**, one cable per port, each cable going to a lan. A lan
is a learning bridge: a frame whose destination is known leaves by one port,
an unknown or broadcast one leaves by every other port.

.. image:: /svg/lan_flood.svg
   :align: center
   :width: 100%

.. image:: /svg/canvas.svg
   :align: center
   :width: 100%

A machine is a circle, big for a kvm, smaller for a cnt. The numbered dots on
its border are the interfaces: green when capturable, cyan when not. Four
gestures on the canvas::

    double-click a lan, then single-click a port     the cable is created
    double-click a machine                            root shell in a panel
    double-click a green port                         webshark on that interface
    right-click a kvm, Desktop                        noVNC desktop


Where things are
================

Way 1, the bundle::

    /usr/bin/cloonix                       the client
    /usr/bin/cloonix_*.py                  its helpers, one per verb
    /usr/libexec/cloonix/cloonfs           binaries, libraries, cloonix.cfg
    /usr/libexec/cloonix/insider_agents    the agent discs given to the guests
    /usr/share/cloonix/demos               the demos
    /usr/share/cloonix/pcap                captures written by PcapRec
    /usr/share/cloonix/cert                certificate of the https port
    /var/lib/cloonix/bulk                  guest root file-systems
    /var/lib/cloonix/<net>                 work directory of a net

Way 2, the toy, after *tar xvf cloonix-toy.tar.xz*::

    cloonix-toy/rootfs            the crun root holding cloonfs
    cloonix-toy/config            config.json and the certificate
    cloonix-toy/bin               the few host-side binaries
    cloonix-toy/worktmp           crun state and crun.log
    cloonix-toy/<net>cmd          the command script written by setup.sh


Content
=======

.. toctree::
     :numbered:
     :maxdepth: 1
     :caption: Cloonix

     doc/installation.rst
     doc/system.rst
     doc/first_demo.rst
     doc/interface.rst
     doc/cloonix_toy.rst
     doc/webgui.rst
     doc/items.rst
     doc/clients.rst
     doc/files.rst
     doc/demos.rst
     doc/recipes.rst
     doc/flows.rst
     doc/agent.rst
     doc/ovs.rst
     doc/rootfs.rst
     doc/windows.rst
     doc/configure.rst
     doc/building.rst
     doc/safety.rst
     doc/compared.rst
     doc/release.rst

.. toctree::
     :numbered:
     :maxdepth: 1
     :caption: The demos: networking culture

     doc/learn.rst
     doc/opensource.rst
     doc/wifi.rst
     doc/demos_switching.rst
     doc/demos_address.rst
     doc/demos_routing.rst
     doc/demos_security.rst
     doc/demos_services.rst
     doc/demos_wifi.rst
     doc/demos_guests.rst
