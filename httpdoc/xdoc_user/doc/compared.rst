===================================
Cloonix among the network emulators
===================================

Four other tools build a network of machines on one computer. This chapter
puts cloonix beside them, point by point, on what each one is made of and
what a user of it does. Nothing here is a benchmark; every line about
cloonix is checkable in the chapters that follow.

============  =====================================================
tool          what it is made of
============  =====================================================
gns3          a qt gui and a python server; the guests are the
              qemu, docker, virtualbox and vmware of the host or of
              the gns3 vm, plus dynamips and iou for cisco images;
              the links are udp relays held by ubridge
eve-ng        an ubuntu system, installed on a machine or as a vm,
              with a web gui; the guests are qemu, docker, iol and
              dynamips; the links are bridges of that system
containerlab  one binary driving the docker or podman of the host;
              the guests are containers, a kvm guest is a qemu run
              inside a container; the links are veth pairs
mininet       a python library; the guests are network namespaces
              of the host kernel, the switches openvswitch or the
              linux bridge; made for openflow controllers
cloonix       C daemons and one python client; qemu-kvm,
              openvswitch, crun, passt and the wireshark dissectors
              are compiled into the delivery; the guests are kvm and
              cnt, the links are openvswitch bridges
============  =====================================================

.. image:: /svg/compared.svg
   :align: center
   :width: 100%


Nothing from the host
=====================

Every binary and library cloonix uses is under
*/usr/libexec/cloonix/cloonfs*, interpreter and rpath rewritten by patchelf
at packing time. The host has no qemu, no openvswitch, no docker, no crun
and no wireshark to install, and the version of each is the one of the
tarball, whatever the distribution. One tar.gz runs on debian, fedora and
arch, and on a release later than the build. Removing */usr/libexec/cloonix*
removes it.

gns3 and containerlab drive a hypervisor and a container engine that are
installed and updated separately, either on the host or in the gns3 vm;
eve-ng is a whole operating system. mininet uses the kernel of the host as
the only kernel.

To see it::

    ls /usr/libexec/cloonix/cloonfs/bin
    readelf -l /usr/libexec/cloonix/cloonfs/bin/cloonix-qemu-system | grep interpreter


Root once, or never
===================

The bundle asks for root once, at install, and runs as ordinary processes
of the user afterwards; the toy asks for root at no moment, install or run:
cloonix itself runs in a crun of the user's own uid. The one setuid binary
of the bundle, *cloonix-suid-power*, speaks a fixed vocabulary and is
listed, verb by verb, in the chapter *Safety and attack surface*.

A gns3 or a containerlab user is a member of the docker group of the host,
or root, to reach the daemon; eve-ng runs its nodes as the root of its
system.

To see it::

    find /usr/libexec/cloonix -perm -4000
    ps -eo user,comm | grep cloonix


The cable is a switch
=====================

The one thing a port plugs into is a **lan**, an openvswitch bridge that
learns: a frame whose destination is known leaves by one port, a broadcast
or an unknown one by every other port. A cable between two machines is a
lan with two ports; a segment with ten machines is the same lan with ten.
A capture on a port that is not on the path shows arp and broadcast and no
unicast, which is what a real switch shows.

In gns3 and containerlab a link joins two ports and a switch is a node,
added and cabled like a machine. In mininet a switch is an openflow
switch driven by a controller.

::

    cloonix cli nemo plug pc eth0 lan1
    cloonix cli nemo plug r1 eth0 lan1
    cloonix cli nemo dump ovs-vsctl show


A shell without an address
==========================

*cloonix cli nemo ssh r1* opens a root shell in the guest over a virtio
serial port for a kvm, over a shared mount for a cnt. *scp* and *files*
travel the same way. The guest has no management interface, no ip address
reserved for the tool and no daemon of the tool listening on its network:
the topology holds the interfaces of the model and nothing else, and a
guest with every interface down is still reachable.

gns3 and eve-ng reach a guest by its console, a telnet or a vnc port of
the server; containerlab plugs every node into a management docker
network and reaches it by ssh on that network.

::

    cloonix cli nemo ssh r1 "ip -br addr"
    cloonix cli nemo scp /tmp/f r1:/root/
    cloonix cli nemo files r1


The capture is in the switch
============================

A sniff is a subscription on one interface with a capture filter, and the
filter is applied by *cloonix-ovs-snf* in the switch: only the packets that
match leave the namespace. The same packets open in **webshark**, the
dissection engine of wireshark in the browser, with no wireshark on the
client. Two sniffs with one filter on the two sides of a tunnel show what
was encrypted or dropped.

gns3 writes a pcap file that a wireshark installed beside the gui opens;
containerlab and mininet run tcpdump in the namespace of the node.

::

    cloonix cli nemo sniff r1 eth1 --dump udp port 53


A cable that degrades
=====================

**a2b** is an item, cabled like a machine: loss, delay, rate and silentms,
each direction apart, from the command line or from sliders in the
right-click menu of the canvas, on a running network.

gns3 sets a filter on a link, containerlab and mininet call the netem of
the host kernel on a link.

::

    cloonix cli nemo add a2b cable
    cloonix cli nemo cnf cable A2B delay=100
    cloonix cli nemo cnf cable B2A loss=100


One net, several machines, the real world
=========================================

  * **c2c** cables a lan of this net to a lan of a net on another
    computer, over one udp port pair: a topology spread on two machines
    is two demos and one *add c2c*.
  * **phy** puts a physical interface of the host into a lan, taken away
    from the host or as a macvlan on it; **tap** gives the host an
    interface into a lan; **nat** gives the guests a way out to the
    internet through passt, egress only unless a port is published.
  * A net lives in its own network namespace, *cloonix_<net>*: the host
    sees no interface of it, and two nets on one machine do not collide.

::

    cloonix cli nemo add c2c nemo2fido fido
    cloonix cli nemo add phy enp6s0 --macvlan
    cloonix cli nemo add nat nat1


The guest is a stock image
==========================

A kvm boots a plain qcow2 of *bulk* and a cnt runs a plain directory, or
the tar.gz of one, extracted at its first use:
nothing of cloonix is inside, the agent arrives on a cd-rom built by
cloonix. A cloud image of a distribution is used as downloaded; a windows,
a freebsd and a cisco ios-xe disk have a verb each, *add win*, *add
freebsd*, *add cisco*, which adds what that disk always needs. A cnt runs
in a user namespace on the user's subuid window, its image mounted
idmapped, so one image in bulk serves several users and several nets at
once and is never written by cloonix.

gns3 and eve-ng ship a catalogue of appliance definitions, one per vendor
image, and run cisco ios of the old routers under dynamips, which cloonix
does not: the cisco guest of cloonix is the catalyst 8000v under qemu.

::

    cloonix cli nemo add kvm r1  ram=2000 cpu=2 eth=2 trixie.qcow2
    cloonix cli nemo add cnt pc  eth=1 trixie
    cloonix cli nemo add win Win1 ram=4000 cpu=2 eth=1 win.qcow2


The topology is a program
=========================

A network is the sequence of *add* lines that built it, and a demo is that
sequence in a python file with the acts that follow: a routing daemon
started, a packet caught, a link cut. 69 demos are in
*/usr/share/cloonix/demos*, one file each, played by name from the command
line or from the canvas, each running the daemon a carrier runs, frr,
keepalived, wireguard, nftables, kea, bind, asterisk, mosquitto,
postfix, on a topology that costs the seconds it takes to boot.

gns3 keeps a project file, eve-ng a lab file, containerlab a yaml file;
cloonix has no topology file format: *dump commands* prints the command
lines that rebuild the running net, settings and places included, and that
output is a shell script; *save* writes a running kvm back to a qcow2.

::

    cloonix cli nemo demo routing
    cloonix cli nemo demo routing frrspider
    cloonix cli nemo dump commands > mynet.sh


The client is one file
======================

The client is one python file on the standard library, with a helper file
per large verb, run as it is on linux, macos and windows; the canvas is a
web page on the https port of the net, with the root shells, the desktops
and webshark as panels of that page. Nothing is installed on the client
side but python, or a browser.

The gns3 gui is a qt application to install on the client, with its web
gui as the other way in; eve-ng is a browser; containerlab and mininet are
a terminal on the server.

::

    cloonix list
    cloonix cli nemo ssh r1


Two ports, one password
=======================

A net listens on two tcp ports, 45211 for the client with tls and an hmac
on every frame, 4444 for the browser with https and basic auth, and on no
other: no guest exposes a port. Whoever holds the password of a net drives
it; there is no account and no audit. The chapter *Safety and attack
surface* takes every way in one by one.

To see it::

    ss -ltnp | grep cloonix


What cloonix does not have
==========================

  * no dynamips, iou or iol: no ios image of the old cisco routers
  * no virtualbox and no vmware guest
  * no catalogue of vendor appliances: an image is a qcow2 or a directory
    put in bulk
  * no topology file format: a net is saved as the shell script *dump
    commands* prints
  * no accounts, one password per net
  * no openflow controller and no sdn switch: a lan is a learning bridge
