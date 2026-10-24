====================
The demos of cloonix
====================

Each demo of *demos/cloonix/* shows one piece of cloonix at work: an item, a
capture, a cable to another net. They are the reference of what cloonix
does; the demos of the other areas, in the part *The demos: networking
culture*, use cloonix to show the protocols of the internet.

Layout
======

One level under *demos/*, in the source tree, in */usr/share/cloonix/demos*
and in the toy: an area, then one python file per demo::

    demos/routing/frrmix.py
           ^       ^
           |       the demo
           the subject area

Three sit at the top, *windows.py*, *freebsd.py* and *cisco.py*: there the
guest is the subject. The areas::

    cloonix       a2b c2c cnt cntkvmwifi cntwifi kvm kvmwifi limit mount nat nginx phy realnet snf systemd
    switching     ovn stp vlan vxlan
    routing       babel batman bird frrmix frrspider frrspider_bgp isis
    address       dhcp dns ipv6 knot ntp unbound
    security      arp ebpf ipsec nftables openvpn radius suricata tinc tls wg
    services      asterisk mqtt proxy smtp squid
    availability  cluster vrrp
    watching      perf
    wifi          cnt_bridge cnt_mesh cnt_open cnt_roaming cnt_wpa2 cnt_wpa3
                  kvm_bridge kvm_mesh kvm_open kvm_roaming kvm_wpa2 kvm_wpa3
                  cntkvm_enterprise cntkvm_home cntkvm_monitor cntkvm_roaming

The Demo picker of the canvas does not show the areas: it shows the packages
of the chapter *Learning with cloonix*, read from *demos/packages*.

What a demo is about is the docstring at its top: the network as ascii
art, the acts, the image, the packet to catch. *cloonix cli <net> demo*
lists each demo with the first line of that docstring, and the canvas shows
the whole of it in the *About* view of its Comments panel.


One form
========

A demo is a python programme, shebang, standard library, the net name as
its only argument, *nemo* by default::

    ./routing/frrmix.py nemo
    cloonix cli nemo demo routing/frrmix.py
    cloonix cli nemo demo frrmix

Outside the *cloonix* area, a demo opens its history with *HISTORY*, a
paragraph on where the protocol comes from and who wrote the software it
runs; the same paragraph heads its section in the part *The demos:
networking culture*.

The functions every demo shares, *run*, *ssh*, the waits, *erase_net*,
*place*, *story*, are in *demos/lib/demolib.py*, and what the wifi demos
share in *demos/lib/wifilib.py*: *lib* is the one directory of *demos* that
is not an area, absent from every demo list; a demo imports from it the
names it uses.

In the source every command is written whole, *cloonix cli* included, and
every command prints itself as it runs, so a run is a worked example::

    $ cloonix cli nemo add cnt a10 eth=1 trixie
    $ cloonix cli nemo plug a10 eth0 lan_trunk
    $ cloonix cli nemo ssh a10 "ip link add link eth0 name eth0.10 type vlan id 10"

A demo checks that the net is up and its image in bulk before building
anything, and prints the refusal of the server, which names the build script
and the download, when it is not. It then erases the net, *del all*, waits
for the topology to be empty, and sends the position of each item and the
angle of each interface before creating them, so every run starts on a clean
canvas and draws the figure of this chapter. On the canvas a result is shown
at reading speed: a ping is five pings, and what an act proved stays four
seconds, blank lines around it, before the next act; a terminal run has no
pause, nor a script with *CLOONIX_DEMO_FAST=1*, which *debug/demo_test.py*
sets. A demo with something to try by hand, a published port to open in a
browser, stops for Enter, in the terminal or in the Demo panel; run by a
script, with nobody to answer, it goes on. Every demo ends with the network
running.

The image of a demo is *ROOTFS_CHOICES* at the top of the script; the
builders are in *vm_create/cnt_kvm_demo*, keyed by the demo name, *frr* for
the three frr demos::

    cd vm_create/cnt_kvm_demo
    ./doitall.sh frr                the images the frr demos need
    ./doitall.sh --all              every image every demo needs

Two tools in *debug/*::

    debug/check_demos.py            static: depth, docstring, erase and place, scripts compile
    debug/demo_test.py nemo         runs every demo under a timeout and reports
    debug/demo_test.py nemo --only vlan,stp


Two readers
===========

A demo writes for two readers and keeps them apart.

The **console**, a terminal or the log of the Demo panel, receives the
cloonix commands and nothing else, so that it reads as the list of what to
type to build the same thing by hand::

    $ cloonix cli nemo add cnt gwA eth=2 wg_trixie
    $ cloonix cli nemo plug gwA eth1 lanC
    $ cloonix cli nemo ssh gwA "ip link add wg0 type wireguard"

The **history** is everything else: the paragraph on the protocol, what
each step is for, the output of the ping or the capture that proves it, the
commands it suggests once it is over. It is shown by the *Comments*
panel that the Demo panel opens on the canvas, step by step, each step
folding the commands it ran; the chapter on the web canvas describes it.
A terminal run does not print it, and does not pause for it either::

    ./security/wg.py                          the commands
    CLOONIX_DEMO_STORY=1 ./security/wg.py     the commands and the history,
                                              each history line marked #story#

A refusal, *the image is not in bulk*, or a failure, *the string was still
readable on lanC*, is for both readers: it is printed on the error output,
shows in the terminal and in the log, and the popup repeats it in red.


all.py
======

Every demo of the Demo picker, in the order of *demos/packages*, one after
the other, for a reader who only watches::

    cloonix cli nemo demo all.py

Each demo erases the net, builds its network, proves it and leaves it up
until the next one starts; each is first printed as the line that plays it
alone. A key a demo would wait for is not awaited. Before the first demo,
when the server runs on this machine, all.py checks the kernel modules of
every demo of its list and names the missing ones at once, with the
*modprobe* lines that load them. all.py is a check: the
demos it plays run with *CLOONIX_DEMO_FAST=1*, no pause for a reader and
none between two demos; *CLOONIX_DEMO_FAST=0* in its environment gives the
pace of a reader back. A demo that cannot run here, its image absent, a
kernel module not loaded on the host, a net not run as root, a toy with a
single uid mapping, says why and stops the run there, as a demo that fails
does: its network is left for inspection, and the voice says that the demo
stopped and reads the first line of the reason. The Comments panel tells one
demo at a time: its title names the demo being played, *7/69
cloonix/snf.py*, and its current step; at its end a line says *End of
demo*; after the last, the number of demos played to their end. all.py has
no phrase of its own: the voice says the phrase of each demo as it begins.
In a toy, all.py plays the demos of that toy.


cloonix/cnt.py and cloonix/kvm.py
=================================

The two networks of the chapter *First run*, played by a program: two
containers on one lan and a ping, then the same with two virtual machines,
image *trixie* and *trixie.qcow2*::

    cloonix cli nemo demo cloonix/cnt.py
    cloonix cli nemo demo cloonix/kvm.py

Each prints every command as it runs it, the same lines as the chapter, so
the output is the chapter with the addresses filled in and the ping answered.
The chapter *The first network* says the same for the canvas.


cloonix/a2b.py
==============

.. image:: /svg/demo_a2b.svg
   :align: center
   :width: 100%

Two containers, one a2b, image *wg_trixie* or *proxy_trixie*, whichever is
in bulk::

    cloonix cli nemo demo cloonix/a2b.py

Both wires are sniffed. Delay is timed per direction, the echo request on
hostA's wire and on hostB's; loss names the sequence numbers that never
arrived; rate sends udp at 130 kB/s through a 100 kB/s cap and counts the
datagrams on each wire::

    cloonix cli nemo cnf cable A2B delay=100
    cloonix cli nemo cnf cable A2B loss=100
    cloonix cli nemo cnf cable A2B rate=100
    cloonix cli nemo cnf cable A2B silentms=200
    cloonix cli nemo sniff hostB eth0 --dump icmp


cloonix/snf.py
==============

.. image:: /svg/demo_snf.svg
   :align: center
   :width: 100%

Four containers on one lan, image *wg_trixie*, *proxy_trixie*, *smtp_trixie*
or *mqtt_trixie*, the first one found in bulk. *noisy1* broadcasts five
hundred datagrams a second on ports 10000-10100; *alice* sends *bob* one
beacon a second on port 53 and one ping. Four subscriptions to the same
interface, four filters::

    cloonix cli nemo demo cloonix/snf.py

::

    filter                              packets  of the whole
    (none)                                 2012        100.0%
    not udp portrange 10000-10100            12          0.6%
    udp port 53                               4          0.2%
    icmp                                      8          0.4%

The icmp count includes the port unreachable that bob's kernel answers to
each beacon.


cloonix/nat.py
==============

.. image:: /svg/demo_nat.svg
   :align: center
   :width: 100%

Three containers, two lans, one nat, image *nft_trixie*::

    cloonix cli nemo demo cloonix/nat.py

passt answers the dhcp of *gw* with 10.117.0.10, *inside1* reaches the
outside by routing through *gw*, and the loopback of the host is reached
from inside at 10.117.0.1::

    cloonix cli nemo sniff gw eth1 --dump "udp port 67 or udp port 68"


cloonix/c2c.py
==============

.. image:: /svg/demo_c2c.svg
   :align: center
   :width: 100%

Three servers, two c2c, one container each, image *trixie*::

    cloonix start nemo ; cloonix start fido ; cloonix start mito
    cloonix cli nemo demo cloonix/c2c.py nemo fido mito

Both c2c are created from the centre, *nemocnt* routes between the two
subnets, and the last act pings from *fidocnt* to *mitocnt*::

    64 bytes from 10.20.0.2: icmp_seq=1 ttl=63 time=0.5 ms

*ttl=63*: one router on the path, and two c2c crossed without a decrement.
Needs three net blocks in *cloonix.cfg*, each with its own
*c2c_udp_port_low*.


cloonix/nginx.py
================

One container, one lan, one nat with a published port, image
*proxy_trixie*::

    cloonix cli nemo demo cloonix/nginx.py
    cloonix cli nemo add nat nat1 publish=8080:80
    curl http://127.0.0.1:8080/

The page answers *hello from cloonix container Cnt1* from a container with
no address on any network of the host.


cloonix/mount.py
================

.. image:: /svg/demo_mount.svg
   :align: center
   :width: 100%

Two containers, image *trixie*, both given a host directory and a
variable::

    cloonix cli nemo demo cloonix/mount.py
    cloonix cli nemo add cnt cnt1 eth=1 trixie startup_env="NODE_ID=cnt1 ROLE=writer" \
            vmount="/tmp/cloonix_mount_nemo:/shared /tmp/cloonix_mount_nemo/conf:/etc/demo"

The variables are read from */proc/1/environ* of each; *cnt1* writes in
*/shared*, *cnt2* and the host read it; the host writes *conf/demo.conf*
and *cnt1* reads it at */etc/demo*; then *cnt1* is deleted and made again:
the file in the mount is still there, the one written in */root* is gone.
The demo makes the directory on the machine of the server, where a mount
takes its source; in a toy, in its bulk, which the server sees as
*/root/bulk_host*.


cloonix/limit.py
================

.. image:: /svg/demo_limit.svg
   :align: center
   :width: 100%

Two containers, image *trixie*, one capped::

    cloonix cli nemo demo cloonix/limit.py
    cloonix cli nemo cnf hog cpu=25 ram=100

*cpu.max* and *memory.max* are read inside *hog*; a busy loop in each
container is measured over five seconds with *cpu.stat*; 300 M are
allocated in each and *hog* is killed at 100; then the cap is lifted with
*cpu=0 ram=0* and *dump pid* prints the cost of every guest.


cloonix/systemd.py
==================

.. image:: /svg/demo_systemd.svg
   :align: center
   :width: 100%

Three containers, one with an init, images *trixie_systemd* for that one
and *trixie* for the others::

    cd vm_create/cnt_kvm && ./cnt_trixie_systemd.sh
    cloonix cli nemo demo cloonix/systemd.py

The three ping each other; the demo reads pid 1, *systemctl* and the process
count of each, then prints *fs.inotify.max_user_instances* and how many
instances the host has open.


cloonix/cntwifi.py
==================

Two containers with a radio each, *wlan=1*, on the air *cntwifi*, the amber
lan of the canvas. The radios join one ad hoc network, no access point and
no key, and ping each other::

    sudo modprobe mac80211_hwsim radios=0
    cloonix cli nemo demo cloonix/cntwifi.py
    cloonix cli nemo cnf cntwifi sniff=on
    cloonix cli nemo sniff cntwifi

Package 10, *wifi/*, has the access points, WPA2, WPA3, a bridge, roaming
and a mesh. The chapter *Wifi: from the radio to the ip stack* has the frames these
demos show, beacon to encrypted data.


cloonix/kvmwifi.py
==================

Two kvm with a radio each, *wlan=1*, image *trixie.qcow2*: the
*cloonix/cntwifi.py* of the kvm. The cloonix agent of each guest loads
*mac80211_hwsim* in the guest kernel, makes the radio *wlan0* and registers
as the medium of the guest; the frames travel on the agent channel to
*doorways*, relayed to *suid-power*, which gives them to the radios on the
same air and writes them to its capture. The radios start on the air
*kvmwifi*. k1 makes the ad hoc cell, k2 joins it, k1 pings k2, and the ping
is counted on the air::

    cloonix cli nemo demo cloonix/kvmwifi.py
    cloonix cli nemo cnf kvmwifi sniff=on
    cloonix cli nemo sniff kvmwifi

Package 10, *wifi/*, plays each of its demos with kvm too, *wifi/kvm_*.

Double-click the air *kvmwifi* for its webshark, decoded as the air *cntwifi*.


cloonix/cntkvmwifi.py
=====================

A container and a kvm with a radio each, *wlan=1*, images *trixie* and
*trixie.qcow2*. The container radio is made by suid-power in the kernel of
the host, the kvm radio by the cloonix agent in the kernel of the guest;
suid-power is the medium of every air, so both meet on one air. The kvm
radio starts on *kvmwifi* and is put on *cntwifi*, the air of the container.
c1 makes the ad hoc cell, k1 joins it, they ping each other both ways, and
the ping is counted on the air::

    sudo modprobe mac80211_hwsim radios=0
    cloonix cli nemo demo cloonix/cntkvmwifi.py
    cloonix cli nemo plug k1 wlan0 cntwifi
    cloonix cli nemo cnf cntwifi sniff=on
    cloonix cli nemo sniff cntwifi


cloonix/phy.py
==============

A real interface brought into a net, both ways, images *python_trixie.qcow2*
and *trixie*, cloonix installed on the host::

    cloonix cli nemo demo cloonix/phy.py

Two kvm on one lan, each running a cloonix of its own, *fido* in *mcv*
and *mito* in *abs*, both nets of *cloonix_config.py* at 127.0.0.1 so
the inner server needs no address on *eth0*. In *mcv*, 2.2.2.1 stays on
*eth0* and *add phy eth0 --macvlan* makes *eth0_1*, on a lan with the
container *c1* at 2.2.2.2. In *abs*, *add phy eth0* makes *eth0_0*
and *ifconfig* no longer lists *eth0*; the container *c2* at 2.2.2.3 is on
a lan with it. *c2* pings 2.2.2.1 and 2.2.2.2 across the outer lan, *c1*
pings *c2*; *mcv* itself does not reach *c1*, which is the kernel's rule
for a macvlan in bridge mode: frames go to the wire and to the sibling
macvlans, never to the parent's own stack. The icmp of *c2* is caught on
the outer lan at the *eth0* of *abs*, the interface the kvm no longer has.


cloonix/realnet.py
==================

The recipe *A guest on the real network* played inside a kvm, images
*python_trixie.qcow2* (trixie plus python3, for the client), *trixie.qcow2*
and *trixie*, cloonix installed on the host::

    cd vm_create/cnt_kvm_demo && ./kvm_python_trixie.sh
    cloonix cli nemo demo cloonix/realnet.py

Two kvm on one lan. The installed cloonix of the host, and the container
image packed from inside a container running it, since in bulk its files
belong to whoever built it, are copied into *endu*, which makes *br0* with *eth0* and
192.168.100.175 and starts *cloonix start endu* inside; on that inner net a
container and a tap are cabled, the tap goes into *br0*, and the container
at 192.168.100.153 pings the other kvm at 192.168.100.238. The icmp is
caught on the outer lan at the *eth0* of *station*. Every inner command is
a *cloonix cli nemo ssh endu* carrying a *cloonix cli endu*.


How to read the timestamps
==========================

Each sniffed interface has its own *cloonix-ovs-snf* process stamping
packets in its own read loop. Within one interface the order is reliable;
across interfaces a few milliseconds of skew is normal, and a cluster of
packets sharing one timestamp on one interface is that sniffer catching up
after being descheduled. The demos judge themselves on what a participant
received and print the packet counts beside it.
