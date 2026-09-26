==================================
Host system required customisation
==================================

Cloonix embeds every binary and library it needs. What the host provides is
a few kernel features and the permission to use them. Four of them stop a
first run: kvm not usable, unprivileged user namespaces locked, the inotify
limit reached by systemd containers, the wifi module absent. The sections
below give the symptom of each and the command that fixes it.


What the host provides
======================

Every program the daemons run is in */usr/libexec/cloonix/cloonfs* (the
table is *pthexec.c*, one place); no package is installed for the server,
and the distro under it does not matter. What is taken from the host:

Server, mandatory::

    the kernel and its modules      kvm (kvm_intel or kvm_amd), vhost_net, tun,
                                    openvswitch: the sections below
    /dev/kvm /dev/vhost-net         readable and writable by the user
    /dev/net/tun
    unprivileged user namespaces    containers and the toy

Server, optional, each with what happens without it::

    /run/dbus/system_bus_socket     crun's systemd cgroup manager, for "cnf cpu= ram="
                                    on a running container; absent, a warning and the
                                    cgroupfs manager
    mac80211_hwsim                  the wifi emulation of the containers; absent,
                                    "add cnt wlan=" is refused with the modprobe
                                    to run; the kvm radios need nothing of the host
    vxlan                           the switching/vxlan.py demo; absent, the demo says
                                    the modprobe to run
    batman-adv openvswitch          the routing/batman.py and switching/ovn.py demos
    newuidmap newgidmap             the toy's second uid mapping; absent, the toy takes
    /etc/subuid /etc/subgid         the single mapping and says so (toy chapter)
    a syslog daemon                 the KERR lines of the daemons go through syslog(3);
                                    without one, journald keeps them (journalctl)

Client, mandatory::

    python3                         the one file of the client, standard library only;
                                    the demos are python too

Client, optional::

    a browser                       the web gui and webshark, the section below

Nothing else: no sudo at run time, no compiler, no tool of the host on the
path. The test bench of *debug/autotest/* adds packages to its machines, for the
suites, not for cloonix.


KVM
===

The kvm guests need hardware virtualisation. Non-zero means it is there::

    egrep -c '(vmx|svm)' /proc/cpuinfo

Activate it in the bios if the answer is 0.


Access to the devices
=====================

The user that launches cloonix reads and writes::

    /dev/kvm
    /dev/vhost-net
    /dev/net/tun

Check::

    getfacl /dev/kvm
    getfacl /dev/vhost-net
    getfacl /dev/net/tun

Grant for one user::

    sudo setfacl -m u:${USER}:rw /dev/kvm
    sudo setfacl -m u:${USER}:rw /dev/vhost-net
    sudo setfacl -m u:${USER}:rw /dev/net/tun

Or by group, then log out and in again::

    sudo usermod -a -G kvm ${USER}
    id ${USER}

Permanent at boot::

    cat >> /etc/modules-load.d/modules.conf << EOF
    kvm
    vhost-net
    EOF

    cat > /etc/udev/rules.d/65-kvm.rules << EOF
    KERNEL=="kvm", MODE="0666"
    KERNEL=="vhost-net", MODE="0666"
    EOF

The wide setting, at the price of the security::

    sudo chmod 0666 /dev/kvm
    sudo chmod 0666 /dev/vhost-net
    sudo chmod 0666 /dev/net/tun

The *setup.sh* of the toy performs this check and prints what is missing.


Unprivileged user namespaces
============================

The toy starts cloonix inside a crun container created by the user, and both
installations create such a container for every **cnt**. The symptom of a
locked-down kernel is *unshare: Operation not permitted*.

Ubuntu, in */etc/sysctl.conf*::

    kernel.unprivileged_userns_clone=1
    kernel.apparmor_restrict_unprivileged_userns=0

then::

    sudo sysctl -p

Fedora and centos: set *SELINUX=disabled* in */etc/sysconfig/selinux*, and
allow the nat traffic through firewalld.


Many containers running systemd
===============================

A container runs no init by default. One that is created with
*startup_env=CLOONIX_WITH_SYSTEMD=1* costs about five inotify instances,
counted by the kernel **per user**, with a default of 128::

    cat /proc/sys/fs/inotify/max_user_instances

Past roughly twenty-four such containers, *inotify_init1* returns EMFILE,
systemd exits, pid 1 goes with it and the container dies a second after it
started, with a clean status and nothing logged. Raise the limit::

    sudo sysctl -w fs.inotify.max_user_instances=8192
    echo 'fs.inotify.max_user_instances = 8192' | sudo tee /etc/sysctl.d/90-cloonix.conf


Wifi emulation
==============

A container and a kvm get radios the same way, *wlan=N*, 4 at most.
Each radio is a *wlan* of the guest, *wlan0* up to *wlan3*, named so for
*plug*. The radios are *mac80211_hwsim* radios, made in one
of two places:

container
    By suid-power, in the kernel of the host, then moved into the
    container. Cloonix never loads a module of the host; load it yourself::

        sudo modprobe mac80211_hwsim radios=0

    It can be loaded while a net runs: the net notices and wifi becomes
    available for the containers created from then on. A container asked
    with *wlan=* while the module is absent is refused, the reason
    *mac80211_hwsim_not_loaded_on_host* in the answer of the server.

kvm
    By the cloonix agent, in the kernel of the guest, which loads
    *mac80211_hwsim* there itself: nothing is loaded on the host. The agent
    is a linux one: *wlan=* is refused with *--no_qemu_ga*, *add win*
    and *add freebsd*. Under systemd the agent writes, before the radios,
    */run/udev/rules.d/00-cloonix-radio.rules*, *ID_NET_MANAGED_BY* set for
    their macs, and */run/systemd/network/00-cloonix-radio.network*,
    *Unmanaged=yes* for an older networkd, so that *systemd-networkd*
    leaves them to the user and does not flush the addresses given by
    hand. A guest whose
    radios cannot be made leaves a WARNING *kvmwifi* with the reason in the
    syslog of the host.

A radio is on an **air**, the lan of the wlans: its frames reach the
radios on the same air and no other. Cloonix is the medium of the radios
of a net, so the airs of two nets never meet: suid-power is the medium of
every air, the frames of a kvm radio travelling on the agent channel to
doorways, which relays them to suid-power. A container radio starts on the
air **cntwifi**, a kvm radio on the air **kvmwifi**; no item or lan may take
either name. A wlan goes on an air as an eth goes on a lan, and *unplug*
leaves it on no air, heard by nobody::

    cloonix cli nemo add cnt sta1 eth=1 trixie wlan=1
    cloonix cli nemo plug sta1 wlan0 air2
    cloonix cli nemo unplug sta1 wlan0 air2

    cloonix cli nemo add kvm k1 ram=1000 cpu=2 eth=1 trixie.qcow2 wlan=2
    cloonix cli nemo plug k1 wlan1 air3
    cloonix cli nemo unplug k1 wlan1 air3

An air holds container radios and kvm radios alike: a kvm and a container
on one air hear each other, as two containers or two kvm do. To join the
first radio of k1 to the containers on *cntwifi*::

    cloonix cli nemo plug k1 wlan0 cntwifi

The *plug* of a wlan is answered once suid-power has done it.

An eth never goes on an air, a wlan never on a lan of eths. On the canvas
an air is amber, each wlan an amber dot on it. An air is born not captured,
the lighter way: *cnf <air> sniff=on* captures it and *off* stops it; an
air whose last wlan leaves is gone with its capture, and born again not
captured. On the canvas, the right-click of the air::

    cloonix cli nemo cnf cntwifi sniff=on
    cloonix cli nemo sniff cntwifi
    cloonix cli nemo cnf kvmwifi sniff=on
    cloonix cli nemo sniff kvmwifi

A double-click on an air opens its webshark, one per air, raised when
already open; its right-click menu has *Select*, then a click on a wlan
plugs or unplugs it. The webshark of an air decodes radiotap, 802.11 and
EAPOL, and its capture starts when it opens: open it before the traffic.
The capture filter of *sniff* reads ethernet only: on an air it passes
every frame.

The chapter *Wifi: from the radio to the ip stack* explains these frames.
The display filters of webshark on an air::

    ip                              the frames carrying IPv4: an open air only
    icmp                            the pings of an open air
    arp                             the arp of an open air
    eapol                           the 4-way handshake, Message 1 of 4 to 4 of 4
    wlan.fc.type == 2               the data frames, encrypted or not
    wlan.fc.protected == 1          the encrypted frames
    wlan.fc.type_subtype != 0x08    everything but the beacons
    wlan.ssid == "office"           the beacons, probes and joins of one network

A WPA2 or WPA3 air carries the ping as *QoS Data ... (encrypted)*: its IP
is not readable, *ip* matches nothing there; *wifi/cnt_open.py* has it in clear.
On an air, webshark has one more button, *Data only*. It applies::

    (wlan.fc.type_subtype == 0x20 || wlan.fc.type_subtype == 0x28) && !eapol

and *Show all* gives back the filter there was before; a filter typed by
hand ends it. What the filter keeps and leaves out::

    kept        Data and QoS Data: the 802.11 frames that carry a payload
    left out    Null function: data frames with no payload, a station
                telling the access point it sleeps or wakes
    left out    EAPOL: the 4-way handshake, signalling carried in data frames
    left out    management: beacons, probes, authentication, association
    left out    control: ACK, RTS, CTS

What a data frame carries, after an LLC/SNAP header naming the protocol, is
what a lan carries::

    ARP         before the first IPv4 packet between two machines
    IPv4        and above it ICMP (ping), UDP (DNS, DHCP, NTP), TCP (ssh,
                http)
    IPv6        a Linux station sends some as soon as it joins: neighbour
                discovery, router solicitations, mDNS; webshark decodes
                IPv4, not IPv6, so these show as Data
    EAPOL       carried in data frames too, left out by the filter
    any other   what a guest sends on its wlan: a VLAN tag, LLDP

On an open air, *wifi/cnt_open.py*, the payload is decoded. On a WPA2 or WPA3
air, *wifi/cnt_wpa2.py* and *wifi/cnt_wpa3.py*, the same frames show as
*QoS Data ... (encrypted)*: their count, length and times are there, not
their payload.


Nested guests
=============

A cloonix inside a cloonix kvm needs the *nested* parameter::

    cat /sys/module/kvm_intel/parameters/nested

If N::

    sudo rmmod kvm_intel
    sudo rmmod kvm
    sudo modprobe kvm_intel nested=1

At boot, append *kvm-intel.nested=1* to *GRUB_CMDLINE_LINUX* in
*/etc/default/grub*, then::

    sudo grub-mkconfig -o /boot/grub/grub.cfg


A browser
=========

The canvas is canvas 2d, websockets and xterm.js. Any current firefox or
chromium. The browser does not have to run on the machine hosting cloonix.


Looking inside the namespaces
=============================

The network namespace of the net::

    sudo ip netns exec cloonix_nemo ip link

or by pid::

    pgrep -f "cloonix-ovs-vswitchd.*nemo"
    sudo nsenter --target <pid> --net

The mount namespace of suid-power::

    pgrep -f "cloonix-suid-power nemo"
    sudo nsenter --mount=/proc/<pid>/ns/mnt
