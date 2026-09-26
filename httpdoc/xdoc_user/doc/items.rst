=====================
Cloonix items managed
=====================

.. image:: /svg/items.svg
   :align: center
   :width: 100%


lan
===

The switch that connects every other object: an openvswitch bridge named
*_b_<lan>* inside the namespace *cloonix_<net>*. A frame entering by one port
leaves by the port that learnt the destination, or by every other port when
it is broadcast or unknown. A lan appears with its first cable and
disappears with its last::

    cloonix cli nemo plug kvm1 eth0 lan1
    cloonix cli nemo unplug kvm1 eth0 lan1

An interface of a kvm or a container is named as in the guest, *eth<i>* or
*wlan<i>*: an eth goes on a lan, a wlan on an air. An a2b has two sides, *A*
and *B*. A tap, a nat, a c2c or a phy has one interface, and an air one
capture: no interface is named for them::

    cloonix cli nemo plug tap1 lan1
    cloonix cli nemo cnf cntwifi sniff=on
    cloonix cli nemo sniff cntwifi

The same names serve *cnf*, *sniff* and *unplug*, and *dump commands*
writes them.


kvm
===

A qemu virtual machine. The qcow2 of bulk is the backing file of a fresh
overlay in */var/lib/cloonix/<net>/vm/*, so a machine starts clean every
time unless *--persistent* writes into the bulk file::

    cloonix cli nemo add kvm vm1 ram=2000 cpu=2 eth=2 trixie.qcow2
    cloonix cli nemo add kvm vm1 ram=2G cpu=2 eth=2 trixie.qcow2 --persistent

*ram=* is in megabytes, or with *M* or *G*. A windows disk is added with
*add win*, a freebsd disk with *add freebsd*, a cisco ios-xe disk with *add
cisco*: each sets what that kind of disk needs; the chapter on the client
has the options.


cnt
===

A crun container: a process tree in a private file-system with its own
namespaces, the root file-system being a directory of bulk mounted as the
lower layer of an overlay. The image is never modified unless
*--persistent*::

    cloonix cli nemo add cnt cnt1 eth=1 trixie

A container runs **no init by default**. Its pid 1 is *cloonixsbininit* from
the agent disc: it starts the agent, the exec server and the syslog daemon
of the image, waits for the interfaces and keeps the container alive. An
init is run when asked::

    cloonix cli nemo add cnt sysd1 eth=1 trixie_systemd startup_env="CLOONIX_WITH_SYSTEMD=1"

Each systemd costs several inotify instances, counted by the kernel per user
with a default of 128 shared by every container and every other program of
the session; past about two dozen, a systemd container dies a second after
its start with nothing logged. A router has no use for an init, and without
one the same host carries hundreds of containers. For many systemd
containers::

    sudo sysctl -w fs.inotify.max_user_instances=8192

The demo *cloonix/systemd.py* shows one systemd container beside two
plain ones and prints the margin of the host.

The image may be left as the downloaded *<image>.tar.gz*: when no directory
of that name is in bulk, the first *add cnt* extracts the archive into
*<image>_targz* and creates the container on it, as does every later one on
any net of the machine; the topology shows *<image>_targz*. That tree stays
the content of the tar.gz: it is mounted read only for each container,
*--persistent* is refused on it, and it is not to be edited on the host. An
image to modify is a copy under another name, ``cp -a <image>_targz
<other>``, which takes *--persistent*; the chapter on the guest root
file-systems has the rule in full. A directory whose top has no write bit
for its owner is refused *--persistent* too, whatever its name.

The owner of the image directory does not matter: it is mounted for each
container with its owner's ids translated into those of the user who
started the net, so an image unpacked with sudo, by another user, or copied
from another machine runs as it is. An image that is not in bulk is refused
with the builder that makes it named in the refusal.

A container of the toy has no user namespace, the toy's own map leaving
no uid to give it; its root is the user. In the bundle every container has
one, and a request without is refused.


tap
===

.. image:: /svg/tap.svg
   :align: center
   :width: 100%

A veth pair: the end named *tap1* stays in the host namespace, its peer goes
into *cloonix_<net>* and onto the lan::

    cloonix cli nemo add tap tap1
    cloonix cli nemo plug tap1 lan1
    ip link show tap1

On the host side *tap1* is an ordinary interface attached to nothing. What
is done with it decides what the lan reaches:

  * an address on it, and the host talks to the guests of *lan1*, point to
    point: ``sudo ip addr add 10.0.0.1/24 dev tap1``
  * a port of a kernel bridge that also holds a real cable, and the guests
    of *lan1* are stations of that cable's segment, with addresses of that
    segment: ``sudo ip link set tap1 master br0``

Without one or the other the frames of the guests reach the host's ip stack
and go no further. The recipes chapter has both cases written out.


nat
===

.. image:: /svg/nat.svg
   :align: center
   :width: 100%

A cloonix network is isolated; the nat is the way out. It is built on
**passt**: the passt process runs on the host, unprivileged, spawned by
proxymous, and owns the real v4 and v6 sockets; *cloonix-ovs-nat* in the
namespace shuttles the frames between the ovs port and the passt socket::

    cloonix cli nemo add nat nat1
    cloonix cli nemo plug nat1 lan1
    cloonix cli nemo plug cnt1 eth0 lan1
    cloonix cli nemo ssh cnt1 "dhclient eth0"

passt answers the dhcp: the guest gets **10.117.0.10/24**, gateway
**10.117.0.1**, dns forwarded to the resolver of the host; a host with no resolver but a
loopback one has nothing to forward to, and the guests are then given
9.9.9.9 by dhcp. Outbound tcp, udp
and icmp work, v4 and v6. The gateway address is mapped onto the loopback of
the host, so a server on 127.0.0.1 of the host is reached from the guest at
10.117.0.1: this is how a guest installs packages from a local mirror. One
nat serves one guest.

Published ports, spelled as docker's *-p*::

    cloonix cli nemo add nat nat1 publish=8080:80
    cloonix cli nemo add nat nat1 publish=8080:80/tcp publish=5353:53/udp
    cloonix cli nemo cnf nat1 publish=9090:80
    cloonix cli nemo cnf nat1 publish=none

Single ports, sixteen at most, tcp when the protocol is left off. A *cnf*
replaces the whole list and restarts passt: every connection through that
nat is cut, the guest keeps its address. A port already taken on the host is
refused by passt, in the passt log beside the nat socket. The demos
*cloonix/nat.py* and *cloonix/nginx.py* show both.


c2c
===

.. image:: /svg/c2c.svg
   :align: center
   :width: 100%

A cable to another cloonix net, on the same machine or another. The master
is the one given the name of the distant net; it creates both ends, connects
over tcp and negotiates the udp port that carries the frames::

    cloonix cli nemo add c2c nemo2fido fido
    cloonix cli nemo plug nemo2fido lanf
    cloonix cli fido plug nemo2fido lanf

Run the command on one side only; two masters destroy and restart each
other for ever. *dump commands* of the far net writes the *add c2c* as a
comment, so the scripts of both nets replayed make one cable. Light green on the canvas: tcp and udp both peered; light
red: tcp only, which means *c2c_udp_ip* is not an address the far side can
send udp to; red: neither. Each c2c takes the first free udp port at or above *c2c_udp_port_low*,
scanning a hundred; a net serves five c2c relays at a time and refuses a
sixth naming the five. A firewall between the two machines lets the whole
range through. A c2c is a cable: ttl is not decremented across it.


a2b
===

.. image:: /svg/a2b.svg
   :align: center
   :width: 100%

Two sides, *A* and *B*, and a degradation of what crosses, per direction,
*A2B* or *B2A*::

    cloonix cli nemo add a2b cable
    cloonix cli nemo plug hostA eth0 lanA
    cloonix cli nemo plug cable A lanA
    cloonix cli nemo plug cable B lanB
    cloonix cli nemo plug hostB eth0 lanB
    cloonix cli nemo cnf cable A2B delay=100      100 ms, side A to side B
    cloonix cli nemo cnf cable A2B loss=100       1% dropped
    cloonix cli nemo cnf cable A2B rate=100       100 kB/s
    cloonix cli nemo cnf cable A2B silentms=200   bursts: 200 ms closed, 200 ms open

*A2B* is side A to side B, *B2A* the other way. *loss* counts in
1/10000, so 100 is 1%; *delay* is in ms; *rate* in kB/s; *silentms* closes
the wire for that many ms then opens it for as long, so what arrived
during the silence leaves in a burst; 0 removes any of them. The same four
sliders are *Tune a2b* in the right-click menu.

*rate* sizes the queue and the burst with it. Two keys set them by hand::

    cloonix cli nemo cnf cable A2B qsize=64       64 kB held, then dropped
    cloonix cli nemo cnf cable A2B bsize=2        2 MB of burst allowed

*qsize* is the queue kept before a frame is dropped, in kB; *bsize* is what
the bucket lets through at once after an idle time, in MB. A *rate=* after
them sizes both again.

The a2b also rewrites an address as the frame crosses, per direction, ip
header and checksums redone::

    cloonix cli nemo cnf cable A2B replace_ipsrc 10.0.1.2 with 10.0.9.2
    cloonix cli nemo cnf cable A2B replace_ipdst 10.0.2.2 with 10.0.9.9
    cloonix cli nemo cnf cable A2B unreplace_ipsrc
    cloonix cli nemo cnf cable A2B unreplace_ipdst

*replace_ipsrc* changes the source of what crosses that way, *replace_ipdst*
the destination; the ipv4 header and the checksums of icmp, udp and tcp are
redone. An arp crosses untouched, so the hosts resolve the addresses they
are configured with.


phy
===

.. image:: /svg/phy.svg
   :align: center
   :width: 100%

A physical interface of the host brought into *cloonix_<net>*. **absorb**
takes it away from the host; the item gets the suffix *_0*::

    cloonix cli nemo add phy enp6s0
    sudo ip netns exec cloonix_nemo ip link
    cloonix cli nemo plug enp6s0_0 lanx
    cloonix cli nemo del enp6s0_0                gives it back

**macvlan** leaves the host its interface and makes an instance with its own
mac, suffixed by its rank::

    cloonix cli nemo add phy enp6s0 --macvlan          enp6s0_1, then enp6s0_2

A tab after *add phy* lists the physical interfaces of the server, the next
one offers *--macvlan*, absorbed without it. On the canvas the same choice is the
*phy* entry of the menu, one line per interface and kind. A phy is born
without capture capability, like a tap; *sniff=on*, a *sniff*, or *Sniff on* adds it.
The demo *cloonix/phy.py* plays both kinds inside two kvm.

A wifi interface in managed mode carries frames under its own mac only, so
a wlan absorbed or macvlanned sends the guests' frames into the air and
nothing comes back; the server says so when it sees a radio behind the name.
Guests reach the web through such a machine with a **nat**.


air
===

The lan of the radios. A container or a kvm gets radios with *wlan=N*;
each is a *wlan* of the guest, on an air. The radios of the containers
start on the air *cntwifi*, those of the kvm on *kvmwifi*, and *plug* puts
a radio on any other air, which appears with its first radio::

    cloonix cli nemo add cnt sta1 eth=1 wlan=1 trixie
    cloonix cli nemo plug sta1 wlan0 air1
    cloonix cli nemo cnf air1 sniff=on
    cloonix cli nemo sniff air1

A frame sent on an air reaches the radios of that air only; the containers
and the kvm share one air when their radios are put on it. An air is
captured as a whole: a wlan has no capture of its own. The chapter *Wifi:
from the radio to the ip stack* has the frames.


.. image:: /svg/wifi_cloonix.svg
   :width: 100%

The radios are **mac80211_hwsim** radios, the simulated card of the Linux
kernel written by Jouni Malinen in 2008, the author of hostapd and
wpa_supplicant. For the kernel they are real wifi cards: hostapd and
wpa_supplicant run on them unchanged. mac80211_hwsim can give every frame
to a process instead of delivering it itself, and *cloonix-suid-power* is
that process for every radio of a net: it is the medium of every air.

* **A container** gets its radios with *wlan=N*. *cloonix-suid-power* makes
  them in the namespace of the net and moves each into its container, where
  it is *wlan0*, *wlan1*.
* **A kvm** gets its radios with *wlan=N* too. The cloonix agent inside the
  guest loads mac80211_hwsim in the guest kernel, makes the radios, and
  sends their frames over the backdoor of the guest. *cloonix-doorways*
  hands them to *cloonix-suid-power* on the **air link**. So a container
  and a kvm can be on one air: *wifi/cntkvm_home.py*.
* Every frame goes to the radios of its air only. *plug* puts a radio on
  another air; a station leaving the air of its access point loses it, as a
  phone walking out of a room (*wifi/cnt_roaming.py*).
* An air is captured when asked: *cnf <air> sniff=on*, or a *sniff*. Its
  frames are written as a radiotap pcap to *snf/<air>_0*, served by
  *cloonix-dumpcap* to *sniff* and to webshark.

::

    sudo modprobe mac80211_hwsim radios=0
    cloonix cli nemo demo wifi/cnt_wpa2.py
    cloonix cli nemo cnf cntwifi sniff=on
    cloonix cli nemo sniff cntwifi --line

*radios=0* loads the module with no radio of its own: cloonix makes them.
On the canvas, a double-click on the air opens its webshark.


The interfaces
==============

Every interface is born not capturable, and is made capturable when a
capture is wanted::

    capturable        mirrored to a second ovs port, drawn green
    not capturable    no mirror, lighter, drawn cyan

The capability is toggled while the guest runs::

    cloonix cli nemo cnf cnt1 eth0 sniff=on
    cloonix cli nemo cnf cnt1 eth0 sniff=off

or with *Sniff on* / *Sniff off* on the dot. A *sniff* of an interface not
capturable makes it capturable first. The mirror costs a copy of every
frame; a net of many routers keeps it for the interfaces being watched.


Names and limits
================

A name of an item or of a lan, checked by the server in one place
(*name_is_sane()*, *cfg_name_is_in_use()*)::

    letters, digits, _ - .          nothing else
    1 to 63 characters              MAX_NAME_LEN is 64, the last byte ends it
    no leading - or .               no .. anywhere
    bulk, cloonix, lan, all         refused: words of the system and of the client
    cntwifi, kvmwifi                refused: the default airs of the radios
    eth<i>, wlan<i>, A, B           refused for a lan or an air: words of an interface
    one name, one item              a lan and an item cannot share it

The counts the server enforces at creation::

    eth of a kvm or a cnt           32 at most (MAX_ETH_VM)
    radios of a cnt or a kvm        4 at most (MAX_VWIF), wlan=N; a kvm: linux guest
    name of a tap                   15 characters at most, a host interface name
    image named, not a path         63 characters at most, the name rule above
    airs captured at once           32 (MAX_AIRS)

A refused creation answers the client with the rule it broke::

    ~$ cloonix cli nemo add cnt a/b eth=1 trixie
    ... bad name a/b: letters, digits, _ - . only, not starting with - or ., under 64 chars
