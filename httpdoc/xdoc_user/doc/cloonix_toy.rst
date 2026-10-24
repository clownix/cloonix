===========
Cloonix toy
===========

This chapter is **way 2**, the rootless installation: the binaries, the
canvas and the demos of the bundle, six demos aside (*The demos a toy does
not carry*, below), packed for an unprivileged user. Cloonix starts inside a
crun container your account creates, and nothing is written outside the
extracted directory.

.. image:: /svg/toy_flow.svg
   :align: center
   :width: 100%


Install and setup
=================

The images first. *setup.sh* gives the toy the */var/lib/cloonix/bulk* of the
host, seen inside as */root/bulk_host*, only if that directory exists when
*setup.sh* runs; a bulk made after it is not seen by the toy. So the
container and machine images go in bulk before *setup.sh*::

    sudo mkdir -p /var/lib/cloonix/bulk
    sudo chown ${USER}: /var/lib/cloonix/bulk
    cd /var/lib/cloonix/bulk
    wget https://clownix.net/downloads/cnt_bulk/trixie.tar.gz

A bulk made too late is repaired by a new extraction: images in place, then
*setup.sh* in the new directory. Then the toy::

    cd
    wget https://clownix.net/downloads/cloonix-toy.tar.xz
    tar xvf cloonix-toy.tar.xz
    cd cloonix-toy
    ./setup.sh

*tar xvf* unpacks it with *xz*, there on every desktop distribution; a
minimal host without it gets it from the package *xz-utils* (debian,
ubuntu) or *xz* (the others).

*setup.sh* takes one optional parameter, the net name from *cloonix.cfg*,
*nemo* by default::

    ./setup.sh mito

It patches the rpaths of the few binaries that run outside the container,
fills *config/config.json* with your uid and gid and, when the host has
them, your subuid and subgid ranges (the section *The one host dependency*
below), writes the command script *./nemocmd*, and prints what the host
allows::

    /dev/kvm usable as <user>
    /dev/vhost-net usable as <user>
    /dev/net/tun usable as <user>
    trixie.qcow2 exists in bulk, kvm demos should work
    user namespace: <user> onto 0 and subuid 100000+65536 onto 1..,
                    written by newuidmap/newgidmap of the host: every daemon runs
    mac80211_hwsim loaded wifi emulation should work

    CHOICE:
    ./nemocmd start
    ./nemocmd stop
    ./nemocmd shell

    THE WEB GUI of nemo, the demos are in its Demo button:
    https://127.0.0.1:4444  or  https://<ip>:4444   user: nemo   password: nemoclown

*setup.sh* runs once per extraction: run again in the same directory, it
changes nothing and prints the same choice, for the net it was run for; a
second net is a second extraction.

A missing device or module is reported and is not fatal: the container demos
need neither kvm nor wifi. *setup.sh* also loads the two host-side binaries
with the loader of *bin* and stops, naming the library, if the tarball
lacks one; *./nemocmd start* stops at a *crun* that fails, naming
*worktmp/crun.log*.


The commands
============

::

    ./nemocmd start      proxymous on the host, then the crun; waits for the server, prints the web gui
    ./nemocmd stop       kills the container, removes the shared directory
    ./nemocmd shell      a bash inside the container

Everything else is done in the web gui: the demos from its *Demo* button,
the objects from the canvas, *Delete all* from its header. A *vmount=* names
a directory the server sees, and the toy's server sees nothing of the host
but its bulk, as */root/bulk_host*: a host directory to share with a
container is made under */var/lib/cloonix/bulk* and given as
*/root/bulk_host/<name>*.

*cloonix stop nemo* also ends the toy: the container init lives as long as
the main server, and the next *./nemocmd start* clears what is left.


A first run
===========

::

    ./nemocmd start
    firefox https://127.0.0.1:4444

The page asks for user *nemo*, password *nemoclown*: the name of the net and
its *cloonix_passwd* in *rootfs/cloonix/cloonfs/etc/cloonix.cfg* of the toy
directory, where they can be changed.

From another machine, *https://<ip>:4444*. The certificate is in *config/*,
made once when the tarball was built; the browser warns once. Replace
*config/cloonix_cert.pem* and *config/cloonix_key.pem* with your own on
anything but a private lab machine.

Then the *Demo* button of the header, and *cloonix/cnt.py* in package 0:
two containers on one lan, one ping.

*address/dhcp.py*, in package 3, is five containers and three lans, no kvm;
its daemons need the second uid mapping of the section *The one host
dependency*. *routing/* holds *frrspider.py*, sixty routers; it needs
*frr_trixie* in */var/lib/cloonix/bulk* of the host, which the container
reads when the directory was there before *setup.sh*. *cloonix/cntwifi.py*
and the demos *wifi/cnt_\** and *wifi/cntkvm_\** need *mac80211_hwsim* on
the host; the radios of *wifi/kvm_\** are made in the guests.

Between two demos, *stop and del* of the Demo panel, or *Delete all* of the
header.


One net, one installation
=========================

A bundle and a toy on the same machine share the net name, its doors port,
its work directory under */var/lib/cloonix* and */tmp*. A toy started on top
of a running bundle of the same net is refused before anything starts, with
the name of what is in the way. Give the toy another net, in a new
extraction, with the images already in */var/lib/cloonix/bulk*::

    ./setup.sh mito

Leftovers of a previous run, the state directory or the container init,
are cleared by *./nemocmd start* itself.


Deleting the directory while it runs
====================================

*cloonix-proxymous* runs on the host, outside the crun, and watches the
directory it was started from. When the directory is deleted under a running
net, it asks the main server to stop the ordinary way, the one *cloonix
stop <net>* takes, which ends the container init with the server; if the
init, *cloonix-init-<user>-<net>*, is still there ten seconds later,
proxymous kills it by the pid it holds.
Syslog says which happened::

    ERROR <dir> IS GONE. The installation this net runs from has been
    deleted while it was running. Stopping the net ...

*./nemocmd stop* first is the ordinary way round.


Content of the toy directory
============================

::

    rootfs/        the root of the crun; rootfs/cloonix/cloonfs holds cloonix
    config/        config.json from the template, the certificate pair
    bin/           cloonix-crun-<net>, cloonix-proxymous-<net>, cloonix-passt,
                   cloonix-patchelf, the loader and the libraries the four load,
                   make_cloonix_cert, the python client and its helpers
    worktmp/       the crun state and crun.log

*rootfs* is usable as a guest root file-system under the name *self_rootfs*:
an overlay on it per container, a *tmpfs* on */var*, and */etc/cloonix.cfg*
hidden. The demos of the toy ask for *trixie* and the images of *vm_create*,
read from */var/lib/cloonix/bulk* of the host, put there before *setup.sh*;
those of a specialised toy run on *self_rootfs* (below).


When the start fails
====================

::

    <toy>/worktmp/crun.log
    <toy>/rootfs/root/cloonix-init-starter-crun.log
    <toy>/rootfs/cloonix/cloonfs/var/log/syslog

A start refuses to run while the proxymous process of a previous one is
still there: run *./nemocmd stop* first. The init of the container or
*worktmp/<net>* left alone is cleared by the start.


The one host dependency: newuidmap, newgidmap, /etc/subuid, /etc/subgid
=======================================================================

The toy is a container started by your user with no root anywhere: crun
makes a user namespace in which your uid is 0. The kernel rule for such a
namespace is that its *uid_map* and *gid_map* are written once, and by
whom decides what is allowed inside:

- written by the unprivileged process itself, the map can hold one line
  only, your uid onto 0, and the kernel first sets */proc/<pid>/setgroups*
  to *deny*, for that namespace and for every namespace created inside it,
  with no way back;
- written by *newuidmap* and *newgidmap*, the setuid helpers of shadow-utils
  (package *uidmap* on Debian and Ubuntu, *shadow-utils* on Fedora), the map
  may hold a second line, the range of */etc/subuid* and */etc/subgid* that
  belongs to your user onto 1 and up, and *setgroups* stays *allow*.

*setgroups* matters because a daemon that starts as root and drops its
privileges calls *initgroups* or *setgroups* to do it, and dies when the
call is refused: named (*initgroups(): Operation not permitted*), dnsmasq,
postfix, mosquitto, nginx, haproxy, asterisk. frr, keepalived, wireguard
and the cloonix agent do not, which is why the routing, availability and
security demos run either way.

So *setup.sh* looks for the four things, in this order, and stops at the
first missing one::

    command -v newuidmap newgidmap          the two helpers, in PATH
    grep "^<user>:" /etc/subuid             a line <user>:<start>:<count>
    grep "^<user>:" /etc/subgid             the same for gids
                                            count 65536 at least, both

With all four it writes the two mappings into *config/config.json* and
*config/idmap* says *subuid <start> <count> <start> <count>*; crun calls the
two helpers itself at *start*. Otherwise it writes the single mapping,
*config/idmap* says *single*, and the message says which of the four was
missing and what follows::

    user namespace: <user> onto 0 only, because /etc/subuid has no line for <user> (uid 1000)
                    setgroups is denied inside: named, dnsmasq, postfix, mosquitto,
                    nginx, haproxy, asterisk cannot drop their privileges and die at start;
                    the demos address/dns, address/dhcp, services/* will not run here
                    (sudo usermod --add-subuids 100000-165535 --add-subgids 100000-165535 <user>,
                     then ./setup.sh nemo again in a fresh extraction)

Those demos say so and stop, and *all.py* stops with them.
Everything else of the toy works with the single mapping: the container
and kvm demos of *cloonix/* it carries, the routing, switching,
availability, security and watching demos, the web GUI, ssh, scp, the nat
and its masquerade.
The difference is those seven daemons, and only inside a toy: a bundle
installation runs its containers through *cloonix-suid-power*, which is
root and writes the maps itself, so the bundle needs the subuid lines for
its own reasons and never the helpers.

Most distributions give a user the lines at creation: *adduser* on Debian
and Ubuntu, *useradd* on Fedora, 65536 ids from 100000 up, one range per
user. A user made another way, or an old account, may have none; the
*usermod* line above adds them, and the ranges must not overlap between
users (*cat /etc/subuid* shows the ones taken).

Two consequences of the second mapping to know:

- a file an inner container writes as one of its own uids is owned on the
  host by a subuid, 100000 and up, that your user is not: erasing the toy
  directory is then ``unshare --map-root-user --map-auto rm -rf <dir>``,
  which *setup.sh* prints and *./nemocmd stop* does for the state directory
  it erases; a plain *rm -rf* stops on those files and says *Permission
  denied*;
- nothing of the host is otherwise used: the toy's own crun, patchelf'd to
  the toy's libraries, does the *exec* of *newuidmap* and *newgidmap*, and
  that is the whole of the dependency.

The demos a toy does not carry
==============================

A toy started by a user leaves out the demos it cannot play, listed in
*toy_targz/toy_drop*: their files, their lines of the Demo picker and of
the voice, their clips. *all.py* in a toy plays the others::

    cloonix/limit.py      a container of a toy has no cgroup to cap
    cloonix/systemd.py    systemd as pid 1 needs a cgroup the toy does not have
    cloonix/phy.py        copies the cloonix installed on the host into its kvm
    cloonix/realnet.py    copies the cloonix installed on the host into its kvm
    security/ebpf.py      an XDP program needs a net started by root
    cisco.py              drives the console with the dtach of an installed cloonix

The bundle plays them. *cloonix/c2c.py* plays in a toy when its two other
nets, *mito* and *fido*, answer: started before it, from a bundle or as toys
of their own; otherwise it stops with a warning naming the ones missing and
the commands that start them. *cloonix/nat.py* plays in a toy: its
passt runs on the host, outside the crun, spawned by the proxymous of the
toy.
*cloonix/mount.py* plays in a toy: its directory is made under
*/var/lib/cloonix/bulk* and given to the server as
*/root/bulk_host/cloonix_mount_<net>*.

No cgroup of its own, and systemd in a container of the toy
===========================================================

The toy makes no cgroup: it stays in the one it is started from, beside
its proxymous, and that is the same on every host, with or without
systemd. It is a choice. Given a cgroup of its own by the host's systemd
user manager, a loaded toy met kernels (RHEL 10, 6.12) that left what runs
outside that busy cgroup without a cpu for tens of seconds; proxymous runs
outside, and a net that loses its proxymous for thirty seconds destroys
itself.

The price is systemd inside a container of the toy. As pid 1 it must
create cgroups, and a toy with no cgroup of its own has none it may write
in. *startup_env=CLOONIX_WITH_SYSTEMD=1* is then not an error: the
container comes up without systemd, as the default does, and
*/run/cloonix_no_systemd* in it holds the reason. The bundle is not
concerned, its containers are made by root.

Where systemd in a container of the toy matters more, on a host with a
systemd user manager and a user bus::

    CLOONIX_TOY_CGROUPM=systemd ./nemocmd start

asks that manager for a delegated cgroup; the first
line of the start says which was used.

The toys for frr, BIRD, Suricata and BIND
=========================================

Four more toys carry one open source project each, containers only::

    cloonix-frr-toy.tar.xz         frr: routing/frrspider.py, frrspider_bgp.py, frrmix.py
                                   and 26 examples of the FRR User Guide in frr_demo/
    cloonix-bird-toy.tar.xz        BIRD: routing/bird.py and 13 examples of the BIRD
                                   User's Guide in bird_demo/
    cloonix-suricata-toy.tar.xz    Suricata: security/suricata.py and 7 examples of the
                                   Suricata User Guide in suricata_demo/
    cloonix-bind-toy.tar.xz        BIND 9: address/dns.py and 12 examples of the BIND 9
                                   Administrator Reference Manual in bind_demo/

Each one is used as the toy::

    wget https://clownix.net/downloads/cloonix-frr-toy.tar.xz
    tar xf cloonix-frr-toy.tar.xz
    cd cloonix-frr-toy
    ./setup.sh nemo
    ./nemocmd start

The demos are in the *Demo* button of the canvas, with *all.py*, which plays
them all in a row and stops with a voiced error at the first problem, as in
every toy. The image of the project
is merged into *rootfs* at build time, *usr* and *etc*, what *rootfs* has
already being kept, and every program added is checked to load there; the
containers run on that root, the image *self_rootfs*, so the bulk can stay
empty. There is no kvm: no qemu, no firmware, no agent disc, no vnc console,
no *xorriso*; the canvas offers cnt only, *add kvm* answers that this
cloonix has containers only, and neither *setup.sh* nor the start says
anything about */dev/kvm*. *toy_targz/create_<name>_toy_targz.sh* builds
each of them.

Some of their demos use kernel modules that a container cannot load; on
the host, once per boot, for every demo of the frr toy::

    sudo modprobe vrf
    sudo modprobe mpls_router
    sudo modprobe mpls_iptunnel
    sudo modprobe vxlan
    sudo modprobe bridge
    sudo modprobe ip_gre
    sudo modprobe macvlan

and *vrf*, *mpls_router*, *mpls_iptunnel* for the BIRD toy. *all.py* names
the missing ones before its first demo.

Of the image of the project, a toy of this kind takes what its demos use:
the commands their lines run, the packages of its *needs* file, and the
libraries of both. frr keeps the daemons the demos start and no python;
BIRD and Suricata keep the python modules that the scripts their demos
write into the containers import; BIND keeps named and its tools, the frr
daemons *address/dns.py* starts, and no python. A module missing at run
time is a KERR line in the syslog of the toy, with its name.

The demos of *bind_demo/* start named without *-u*: it stays uid 0 of its
container and calls no *initgroups*, so they play with the single mapping
too. *address/dns.py* starts it with *-u root* and needs the second one.


Case of qemu-kvm
================

The kvm demos need the three devices readable and writable by your user,
see the host system chapter, and a qcow2 in */var/lib/cloonix/bulk* of the
host, the directory being there before *setup.sh*::

    getfacl /dev/kvm /dev/vhost-net /dev/net/tun
    sudo setfacl -m u:${USER}:rw /dev/kvm /dev/vhost-net /dev/net/tun
