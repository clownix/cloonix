===========
Cloonix toy
===========

This chapter is **way 2**, the rootless installation. Same binaries, same
canvas, same demos as the bundle, packed so that an unprivileged user runs
it: cloonix itself starts inside a crun container that your account creates,
and nothing is written outside the extracted directory.

.. image:: /svg/toy_flow.svg
   :align: center
   :width: 100%


Install and setup
=================

::

    wget https://clownix.net/downloads/cloonix-toy-__LAST__.tar.gz
    tar xvf cloonix-toy-__LAST__.tar.gz
    cd cloonix-toy-__LAST__
    ./setup.sh

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
    ./nemocmd demo <param>
    ./nemocmd cloonix <args of the cloonix client>

A missing device or module is reported and is not fatal: the container demos
need neither kvm nor wifi. *setup.sh* also loads the two host-side binaries
with the loader of *bin* and stops, naming the library, if the tarball
lacks one; *./nemocmd start* stops at a *crun* that fails, naming
*worktmp/crun.log*.


The commands
============

::

    ./nemocmd start                        proxymous on the host, then the crun; waits for the server
    ./nemocmd stop                         kills the container, removes the shared directory
    ./nemocmd shell                        a bash inside the container
    ./nemocmd demo                         lists the demos
    ./nemocmd demo address/dhcp.py    plays one; dhcp.py, dhcp and address/dhcp resolve too
    ./nemocmd cloonix cli nemo del all       the real client, whatever follows is handed to it
    ./nemocmd cloonix cli nemo ssh cnt1 ip route

*./nemocmd cloonix* runs the python client of the toy on the host with the
toy's own *cloonix.cfg*; the net name is typed as for any client line. A
*vmount=* names a directory the server sees, and the toy's server sees
nothing of the host but its bulk, as */root/bulk_host*: a host directory
to share with a container is made under */var/lib/cloonix/bulk* and given
as */root/bulk_host/<name>*. A demo
runs on the host too, by that client: the container carries no interpreter.


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

Then::

    ./nemocmd demo
    ./nemocmd demo address/dhcp.py

*address/dhcp.py* is five containers and three lans, no kvm. *routing/*
holds *frrspider.py*, sixty routers; it needs *frr_trixie* in
*/var/lib/cloonix/bulk* of the host, which the container reads when it is
readable. *cloonix/cntwifi.py* and the demos of *wifi/* need *mac80211_hwsim*
on the host.

Between two demos::

    ./nemocmd cloonix cli nemo del all


One net, one installation
=========================

A bundle and a toy on the same machine share the net name, its doors port,
its work directory under */var/lib/cloonix* and */tmp*. A toy started on top
of a running bundle of the same net is refused before anything starts, with
the name of what is in the way. Give the toy another net::

    ./setup.sh mito

Leftovers of a previous run are refused in the same way; *./nemocmd stop*
clears them and says whether anything of the net is left.


Deleting the directory while it runs
====================================

*cloonix-proxymous* runs on the host, outside the crun, and watches the
directory it was started from. When the directory is deleted under a running
net, it asks the main server to stop the ordinary way, the one *cloonix
stop <net>* takes; if the container init, *cloonix-init-<user>-<net>*, is
still there ten seconds later, proxymous kills it by the pid it holds.
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

*rootfs* is usable as a guest root file-system under the name *self_rootfs*.
The demos do not use it: they ask for *trixie* and the images of
*vm_create*, read from */var/lib/cloonix/bulk* of the host.


When the start fails
====================

::

    <toy>/worktmp/crun.log
    <toy>/rootfs/root/cloonix-init-starter-crun.log
    <toy>/rootfs/cloonix/cloonfs/var/log/syslog

A start refuses to run if a previous one is still there: the proxymous
process, the init of the container, or *worktmp/<net>*. Run *./nemocmd stop*
first.


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

Everything else of the toy works with the single mapping: the container
and kvm demos of *cloonix/* but the four below, the routing, switching,
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

Four demos of cloonix/ the toy does not play
============================================

Whatever the uid mapping, four demos stop partway in a toy::

    cloonix/mount.py    the directory it mounts is in the /tmp of the host,
                        and a toy sees nothing of the host but its bulk
                        (as /root/bulk_host)
    cloonix/limit.py    the cpu and ram caps need a cgroup per container,
                        and a toy has none: cnf cpu= ram= is refused, the
                        reason given says so
    cloonix/c2c.py      it drives a third net, fido, and a toy is one net
                        per extraction
    cloonix/nat.py      its last step publishes a service of the host's
                        loopback and fetches it from inside through DNAT,
                        which does not cross the extra nesting of the toy

The c2c itself, between two toys, and the masquerade of the nat work: only
these demos go further than a toy allows. The bundle plays all four.

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

Case of qemu-kvm
================

The kvm demos need the three devices readable and writable by your user,
see the host system chapter, and a qcow2 in */var/lib/cloonix/bulk* of the
host::

    getfacl /dev/kvm /dev/vhost-net /dev/net/tun
    sudo setfacl -m u:${USER}:rw /dev/kvm /dev/vhost-net /dev/net/tun
