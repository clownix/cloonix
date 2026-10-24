==================================
Host system required customisation
==================================

Cloonix carries every binary and library it needs; the host gives a few
kernel features and the permission to use them. Four of them stop a first
run: kvm not usable, unprivileged user namespaces locked, the inotify limit
reached by systemd containers, the wifi module absent. Each section below
gives the symptom and the command that fixes it.


What the host provides
======================

Every program the daemons run is in */usr/libexec/cloonix/cloonfs*, listed
in one table, *pthexec.c*: no package is installed for the server, and the
distribution under it does not matter. Taken from the host:

Server, mandatory::

    the kernel and its modules      kvm (kvm_intel or kvm_amd), vhost_net, tun,
                                    openvswitch: the sections below
    /dev/kvm /dev/vhost-net         readable and writable by the user
    /dev/net/tun
    unprivileged user namespaces    containers and the toy

Server, optional, and what happens without each::

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

Client::

    python3                         mandatory: the client, standard library only,
                                    and the demos
    a browser                       optional: the canvas and webshark

Nothing else: no sudo at run time, no compiler, no tool of the host on the
path. The packages the machines of *debug/autotest/* add are for the test
suites, not for cloonix.


KVM
===

The kvm guests need hardware virtualisation: a count above zero here, else
activate it in the bios::

    egrep -c '(vmx|svm)' /proc/cpuinfo


Access to the devices
=====================

The user that starts cloonix reads and writes */dev/kvm*, */dev/vhost-net*
and */dev/net/tun*. Check, then grant for that user::

    getfacl /dev/kvm /dev/vhost-net /dev/net/tun
    sudo setfacl -m u:${USER}:rw /dev/kvm /dev/vhost-net /dev/net/tun

Or by group, effective at the next login::

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

For every user, at the price of the security::

    sudo chmod 0666 /dev/kvm /dev/vhost-net /dev/net/tun

The *setup.sh* of the toy checks the three devices and prints what is
missing. It gives the toy the */var/lib/cloonix/bulk* of the host only if
that directory exists when it runs: the images go in bulk before *setup.sh*.


Unprivileged user namespaces
============================

The toy starts cloonix in a crun container made by the user, and both
installations make such a container for every **cnt**. A locked kernel
answers *unshare: Operation not permitted*. On Ubuntu, in
*/etc/sysctl.conf*::

    kernel.unprivileged_userns_clone=1
    kernel.apparmor_restrict_unprivileged_userns=0

then::

    sudo sysctl -p

Fedora and Rocky: the machines of *debug/autotest* run with
*SELINUX=disabled* in */etc/sysconfig/selinux*.


Many containers running systemd
===============================

A container runs no init by default. One made with
*startup_env=CLOONIX_WITH_SYSTEMD=1* takes about five inotify instances,
counted by the kernel **per user**, 128 by default::

    cat /proc/sys/fs/inotify/max_user_instances

Past about twenty-four such containers, *inotify_init1* returns EMFILE,
systemd exits, pid 1 with it, and the container dies a second after it
started, with a clean status and nothing logged. Raise the limit::

    sudo sysctl -w fs.inotify.max_user_instances=8192
    echo 'fs.inotify.max_user_instances = 8192' | sudo tee /etc/sysctl.d/90-cloonix.conf


Wifi emulation
==============

A container and a kvm get radios the same way, *wlan=N*, 4 at most, named
*wlan0* to *wlan3* for *plug*. They are *mac80211_hwsim* radios, made in one
of two places:

container
    By suid-power, in the kernel of the host, then moved into the container.
    Cloonix never loads a module of the host; load it yourself::

        sudo modprobe mac80211_hwsim radios=0

    It can be loaded while a net runs: the containers created from then on
    get wifi. Asked while the module is absent, *wlan=* is refused with the
    reason *mac80211_hwsim_not_loaded_on_host*.

kvm
    By the cloonix agent, in the kernel of the guest: nothing is loaded on
    the host. The agent is a linux one, so *wlan=* is refused with
    *--no_qemu_ga*, *add win* and *add freebsd*. Under systemd the agent
    writes, before the radios, */run/udev/rules.d/00-cloonix-radio.rules*
    (*ID_NET_MANAGED_BY* set for their macs) and
    */run/systemd/network/00-cloonix-radio.network* (*Unmanaged=yes*, for an
    older networkd), so that *systemd-networkd* leaves them to the user and
    keeps the addresses given by hand. A guest whose radios cannot be made
    leaves a WARNING *kvmwifi* with the reason in the syslog of the host.

A radio is on an **air**, the lan of the wlans: its frames reach the radios
on the same air and no other. Cloonix is the medium of the radios of a net,
so the airs of two nets never meet: suid-power is the medium of every air,
the frames of a kvm radio reaching it through the agent channel and
doorways. A container radio starts on the air **cntwifi**, a kvm radio on
**kvmwifi**; no item or lan may take either name. A wlan goes on an air as
an eth on a lan, and *unplug* leaves it on no air, heard by nobody::

    cloonix cli nemo add cnt sta1 eth=1 trixie wlan=1
    cloonix cli nemo plug sta1 wlan0 air2
    cloonix cli nemo unplug sta1 wlan0 air2

    cloonix cli nemo add kvm k1 ram=1000 cpu=2 eth=1 trixie.qcow2 wlan=2
    cloonix cli nemo plug k1 wlan1 air3
    cloonix cli nemo unplug k1 wlan1 air3

An air holds container and kvm radios alike, and they hear each other. The
first radio of k1 joins the containers on *cntwifi*::

    cloonix cli nemo plug k1 wlan0 cntwifi

The *plug* of a wlan is answered once suid-power has done it. An eth never
goes on an air, a wlan never on a lan of eths.

On the canvas an air is amber, each wlan an amber dot on it. An air is born
not captured; *cnf <air> sniff=on* captures it, *off* stops it, and an air
whose last wlan leaves is gone with its capture, born again not captured::

    cloonix cli nemo cnf cntwifi sniff=on
    cloonix cli nemo sniff cntwifi
    cloonix cli nemo cnf kvmwifi sniff=on
    cloonix cli nemo sniff kvmwifi

A double-click on an air opens its webshark, one per air, raised when
already open; its right-click menu has *Select*, then a click on a wlan
plugs or unplugs it. The webshark of an air decodes radiotap, 802.11 and
EAPOL, and its capture starts when it opens: open it before the traffic.
The capture filter of *sniff* reads ethernet only and passes every frame of
an air. The display filters of webshark, its *Data only* button and what
the frames carry are in the chapter *Wifi: from the radio to the ip stack*.


Nested guests
=============

A cloonix inside a cloonix kvm needs the *nested* parameter::

    cat /sys/module/kvm_intel/parameters/nested

If N::

    sudo rmmod kvm_intel
    sudo rmmod kvm
    sudo modprobe kvm_intel nested=1

At boot, *kvm-intel.nested=1* appended to *GRUB_CMDLINE_LINUX* in
*/etc/default/grub*, then::

    sudo grub-mkconfig -o /boot/grub/grub.cfg


A browser
=========

The canvas is canvas 2d, websockets and xterm.js: any current firefox or
chromium, on any machine that reaches the server.


Looking inside the namespaces
=============================

The network namespace of the net, by its name or by the pid of its ovs::

    sudo ip netns exec cloonix_nemo ip link
    pgrep -f "cloonix-ovs-vswitchd.*nemo"
    sudo nsenter --target <pid> --net

The mount namespace of suid-power::

    pgrep -f "cloonix-suid-power nemo"
    sudo nsenter --mount=/proc/<pid>/ns/mnt


Reading the syslog
==================

Every daemon of a net writes its errors and warnings through syslog(3), one
line each, the net named once after *KERR:*::

    cloonix-main-server[12126]: KERR: nemo recv_from_client.c:recv_cnt_add line:2145 ERROR a/b refused: bad name a/b: ...

    grep 'KERR: ' /var/log/syslog                        every net, the bundle
    grep 'KERR: nemo ' /var/log/syslog                   the net nemo
    <toy>/rootfs/cloonix/cloonfs/var/log/syslog          the toy

A nominal run writes two lines: *INFO started* when the net is ready, with
its version, its ports and, when in use, the shipped password, and *INFO
stopped* at its clean end. A refused command writes its reason, the text the
client prints. A flow above what a path carries writes a WARNING about every
ten seconds while it lasts, with the count dropped so far: every queue has a
bound, and what does not fit is dropped and counted, not held. One place in
the code writes 10 lines at most in 10 seconds. *debug/kerr_triage.py* of
the source tree folds the lines of a long run into one row per kind of
message, counted.
