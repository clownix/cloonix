=======
Recipes
=======

Complete command sequences for the situations that come back. Every block
is cut and paste on a running net called *nemo*; the host commands need
*sudo*. Each recipe says what it builds, then the commands, then what to
check.


A guest on the real network
===========================

The machine runs cloonix and is a station of an ethernet segment, here
192.168.100.0/24, with its cable on *eno1* and its address on a kernel
bridge *br0* that holds *eno1*. A guest of the net is to become another
station of that segment, 192.168.100.153, seen by every machine on the
cable as if it were plugged into the same switch.

The path is one segment from end to end: guest interface, cloonix lan,
tap, *br0*, *eno1*, cable::

    guest eth0 --- lan1 --- tap1 (cloonix side) === tap1 (host) --- br0 --- eno1 --- cable

::

    cloonix cli nemo add kvm Cloon1 ram=2000 cpu=2 eth=1 trixie.qcow2
    cloonix cli nemo add tap tap1
    cloonix cli nemo plug Cloon1 eth0 lan1
    cloonix cli nemo plug tap1 lan1
    sudo ip link set tap1 master br0
    sudo ip link set tap1 up
    cloonix cli nemo ssh Cloon1 "ip addr add 192.168.100.153/24 dev eth0"
    cloonix cli nemo ssh Cloon1 "ip link set eth0 up"
    cloonix cli nemo ssh Cloon1 "ping -c 3 192.168.100.238"

The line that is forgotten is ``ip link set tap1 master br0``. *add tap*
makes the pair and puts one end on the lan; the host end is an interface
attached to nothing, and a frame from the guest that reaches it is
delivered to the host's ip stack and goes no further. The bridge is what
carries it on to *eno1*. Without that line the guest and the host can talk
if *tap1* is given an address, and nothing beyond the host answers.

The guest's default route, if it needs one, is the real gateway of the
segment, set inside the guest: ``ip route add default via 192.168.100.1``.
No nat is involved and none must be on that interface: a second path out
of the guest makes the answers leave the wrong way.

To check::

    bridge link                                 tap1 is a port of br0, state forwarding
    sudo ip netns exec cloonix_nemo ip link     _k_1_<id>_0 and the cloonix end of tap1 on _b_lan1
    cloonix cli nemo dump ovs-vsctl list-ports _b_lan1

If the ping answers only thirty seconds after the cable is made, *br0*
runs spanning tree and holds a new port in listening and learning first.
One path from the guest to the cable cannot loop, so::

    sudo ip link set br0 type bridge stp_state 0

or, keeping it, ``sudo ip link set br0 type bridge forward_delay 200`` for
two seconds instead of fifteen. A bridge that does not exist yet is made
the same way from the cable::

    sudo ip link add br0 type bridge
    sudo ip link set eno1 master br0
    sudo ip addr flush dev eno1
    sudo ip addr add 192.168.100.175/24 dev br0
    sudo ip link set br0 up


The host and a guest, nothing else
==================================

The same tap without a bridge: an address on the host end, and the host
and the guests of the lan share a segment of their own. This is how a
server, a capture or a test tool running on the host faces a guest::

    cloonix cli nemo add cnt cnt1 eth=1 trixie
    cloonix cli nemo add tap tap1
    cloonix cli nemo plug cnt1 eth0 lan1
    cloonix cli nemo plug tap1 lan1
    sudo ip addr add 10.0.0.1/24 dev tap1
    sudo ip link set tap1 up
    cloonix cli nemo ssh cnt1 "ip addr add 10.0.0.2/24 dev eth0"
    cloonix cli nemo ssh cnt1 "ip link set eth0 up"
    ping -c 3 10.0.0.2
    sudo tcpdump -ni tap1

The address stays on the host; nothing outside the machine sees 10.0.0.0/24.


A whole physical interface to the guests
========================================

A spare cable, *enp6s0*, with no address of the host on it, given entirely
to the net: the guests of *lanx* are stations of whatever is at the far end
of that cable, and the host no longer sees the interface::

    cloonix cli nemo add phy enp6s0
    cloonix cli nemo plug enp6s0_0 lanx
    cloonix cli nemo plug Cloon1 eth0 lanx
    sudo ip netns exec cloonix_nemo ip link       enp6s0 is here now
    cloonix cli nemo del enp6s0_0             gives it back to the host

When the host keeps using the interface, *macvlan* makes a second station
on it with its own mac and leaves *enp6s0* where it is; the upstream switch
then sees two macs on one port, which a home switch accepts and a managed
one may not::

    cloonix cli nemo add phy enp6s0 --macvlan
    cloonix cli nemo plug enp6s0_1 lanx

*add phy* followed by a tab lists the interfaces the server has.


The internet for a guest
========================

A guest that only needs to go out, not to be reached, takes a nat: passt
answers dhcp with 10.117.0.10 and gateway 10.117.0.1, the same addresses
set by hand below, forwards dns and carries tcp, udp and icmp out through
the host's own connectivity, with no bridge, no address and no privilege
on the host::

    cloonix cli nemo add cnt cnt1 eth=1 trixie
    cloonix cli nemo add nat nat1
    cloonix cli nemo plug cnt1 eth0 lan1
    cloonix cli nemo plug nat1 lan1
    cloonix cli nemo ssh cnt1 "ip addr add 10.117.0.10/24 dev eth0"
    cloonix cli nemo ssh cnt1 "ip link set eth0 up"
    cloonix cli nemo ssh cnt1 "ip route add default via 10.117.0.1"
    cloonix cli nemo ssh cnt1 "ping -c 3 9.9.9.9"

A service of the guest reached from the host goes through a published
port of the same nat::

    cloonix cli nemo cnf nat1 publish=8080:80
    curl http://127.0.0.1:8080/

A guest that must be reached by its own address from other machines is the
first recipe, not this one.


Seeing the frames
=================

Three instruments, on the same cable, from the quickest to the most
complete::

    cloonix cli nemo cnf cnt1 eth0 sniff=on        the interface becomes capturable, its dot green
    double-click the green dot                       webshark, live, filter box: icmp / tcp.port == 80 / ip.addr == 10.0.0.2
    PcapRec button of the header                     one .pcap per capturable interface, in /usr/share/cloonix/pcap
    sudo ip netns exec cloonix_nemo tcpdump -ni _b_lan1
    cloonix cli nemo dump ovs-vsctl list Mirror      the mirror ports the captures read

webshark and PcapRec can read the same interface at once: one dumpcap per
interface serves both.


Looking inside the machine
==========================

What runs, and in which namespace::

    cloonix cli nemo dump pid            every process of the net, cpu and memory
    cloonix cli nemo dump namespaces     every namespace, its members, and the nsenter to get in

The two commands that are always looked up, written by *dump namespaces*
with the right pid filled in::

    sudo nsenter -t <pid of the ovs> -n ip addr         the lans and the vhost ports
    sudo nsenter -t <pid of a cnt> -p -m ps aux         the processes of a container

*ps* needs *-p* and *-m* together: the pid namespace gives it the
processes, the mount namespace gives it the */proc* that lists them.
``sudo ip netns exec cloonix_nemo`` is the same as ``nsenter -n`` on the
ovs, by the name of the namespace instead of a pid.


From a session to a script
==========================

Once a topology works, *dump commands* prints the command lines that
rebuild it, captures, settings and places included, one shell line each::

    cloonix cli nemo dump commands > mytopo.sh
    cloonix cli nemo del all
    sh mytopo.sh

The *Commands* button of the Demo window prints the same lines in its log.
