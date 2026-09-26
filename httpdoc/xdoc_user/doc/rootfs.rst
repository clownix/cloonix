===============================
Building the guest file-systems
===============================

Cloonix ships no guest. The server looks for them in **bulk** and refuses a
machine whose root file-system is not there; the refusal names the script
of *vm_create* that builds it, *cnt_kvm/kvm_<n>.sh* or *cnt_kvm_demo/kvm_<n>.sh* for *<n>.qcow2*,
*cnt_kvm_demo/cnt_<n>.sh* or *cnt_kvm/cnt_<n>.sh* for a directory *<n>*, and
the download at clownix.net.

.. image:: /svg/bulk.svg
   :align: center
   :width: 100%

::

    /var/lib/cloonix/bulk/<name>.qcow2      a kvm guest
    /var/lib/cloonix/bulk/<name>/           a cnt guest, a directory holding /sbin/init
    /var/lib/cloonix/bulk/<name>.tar.gz     a cnt guest too, when the directory is absent
    /var/lib/cloonix/bulk/<name>_targz/     what cloonix extracted from it, never to be modified

Two ways to fill it: download, or build with *vm_create*.


Downloading
===========

::

    mkdir -p /var/lib/cloonix/bulk
    cd /var/lib/cloonix/bulk
    wget https://clownix.net/downloads/kvm_bulk/trixie.qcow2.gz
    gunzip trixie.qcow2.gz
    wget https://clownix.net/downloads/cnt_bulk/trixie.tar.gz

The tar.gz is used as it is: with no *trixie* directory beside it, the
first *add cnt* on *trixie* extracts it into *trixie_targz* for every net of
the machine. Unpacked by hand, with *tar xvf*, the directory comes first and
takes *--persistent*. Either way a container sees the image with its owner's
ids translated into those of the user who started the net.


The _targz trees
================

A *<name>_targz* tree is the content of *<name>.tar.gz* and stays it::

    trixie.tar.gz        the image, downloaded or built
    trixie_targz/        its extraction, made by cloonix, the same for every
                         net and every user of the machine

* Cloonix never writes in it. Each container gets it mounted read only,
  under an overlay: the writes of the guest go to a tmpfs that is dropped
  with the container, and a write through the mount is answered *Read-only
  file system* by the kernel.
* *--persistent* is refused on the tar.gz and on any name ending in
  *_targz*.
* The modes inside are those of the archive, write bits included: they are
  part of the image, the guest sees them through the overlay, and a daemon
  that leaves root, *named -u bind* for one, needs its directories writable.
  So the modes do not protect the tree on the host: its owner is asked not
  to edit it. A change made there reaches every container started afterwards,
  on every net, and the tree no longer matches its tar.gz.
* *rm -rf trixie_targz* is the repair: the next *add cnt* extracts the
  archive again.

An image to modify, or to run with *--persistent*, is a directory of another
name, made from the archive or copied from the tree::

    cd /var/lib/cloonix/bulk
    mkdir mytrixie && tar xpf trixie.tar.gz -C mytrixie
    cloonix cli nemo add cnt cnt1 eth=1 mytrixie --persistent

or::

    cp -a trixie_targz mytrixie

A directory named *trixie*, without the suffix, comes before *trixie.tar.gz*
for every *add cnt ... trixie* of the machine.


What vm_create contains
=======================

::

    vm_create/get_distro     mirrors a distribution onto a local disk
    vm_create/cnt_kvm        the distributions: one cnt and one kvm per distro, the desktops
    vm_create/cnt_kvm_demo   the demo images: trixie plus one piece of software, and
                             the kvm images a demo needs beyond the distributions
    vm_create/cisco          the Catalyst 8000V image, from a cisco iso

Nothing in them uses sudo except *check_www_links.sh*. Every builder runs
inside a user namespace and writes into bulk only.


A local mirror first
====================

.. image:: /svg/mirror.svg
   :align: center
   :width: 100%

The builders install from a web server on your own machine, never from the
internet. freebsd has no mirror, so *doitall.sh* does not build it:
*kvm_freebsd.sh* is run by hand. One
script per distribution in *get_distro*: *get_trixie.sh*, *get_rocky.sh*,
*get_fedora.sh*, *get_alpine.sh*, *get_arch.sh*, *get_opensuse.sh*,
*get_resolute.sh*, *get_noble.sh*. The paths at their top point at the
author's disk::

    hote="ftp.fr.debian.org"
    cible="/media/perrier/T7Shield2/distros/trixie"

Then the symlinks under */var/www/html*::

    sudo ./check_www_links.sh
    curl http://127.0.0.1/trixie/

Two addresses are written into the images: *http://127.0.0.1/<distro>*
used while building, and *http://10.117.0.1/<distro>* inside the finished
image, which a guest cabled to a **nat** reaches, so *apt install* works
from inside a topology.


Building without root
=====================

.. image:: /svg/userns.svg
   :align: center
   :width: 100%

Every builder re-executes itself through::

    unshare --map-root-user --user --map-auto --mount -- ...

Inside, the builder is uid 0, so *mmdebstrap*, *dnf*, *pacman*,
*mkfs.ext4* and *chroot* work. Seen from the host the files belong to the
subuid range of your account, from */etc/subuid* and */etc/subgid*. A tree
built that way is erased from the same kind of namespace::

    unshare --map-root-user --user --map-auto -- rm -rf /var/lib/cloonix/bulk/trixie

A guest runs in a user namespace too: container uid 0 is the user running
cloonix, container 1 upward is the subuid window. Files whose owner or group
land outside that map show as *nobody:nogroup* in the guest and cannot be
written, chowned or listed by container root, whatever its capabilities.
*setgroups* is refused for the life of the namespace, and *chown* to an id
outside the map answers EINVAL. A package that creates a user, frr with
*/etc/frr*, is handed back to root at the end of its builder, and the demos
tell such daemons to stay root. Check an image with::

    find /var/lib/cloonix/bulk/<image> ! -user $(id -u)


The driver: doitall.sh
======================

*cnt_kvm/doitall.sh*::

    ./doitall.sh                 the help, builds nothing
    ./doitall.sh --cnt           every container image
    ./doitall.sh --kvm           every base machine image, no desktop
    ./doitall.sh --cnt --kvm     both
    ./doitall.sh --cnt --kvm trixie rocky
    ./doitall.sh --desktop       the desktop machines
    ./doitall.sh --all           everything
    ./doitall.sh --list          what would be built

*cnt_kvm_demo/doitall.sh*, keyed by the demo name, *frr* for the three frr
demos; *realnet* is its one kvm image, *python_trixie.qcow2*, built by
*kvm_python_trixie.sh* under a running cloonix::

    ./doitall.sh                 the help
    ./doitall.sh --all           every image every demo needs
    ./doitall.sh frr             frr_trixie
    ./doitall.sh ipsec           both swans
    ./doitall.sh realnet         python_trixie.qcow2
    ./doitall.sh dns dhcp
    ./doitall.sh --list

Before building, both check: not root, known names, scripts present and
executable, none calling sudo, bulk writable, *unshare*, *flock*,
*mkfs.ext4* and *qemu-img* installed. Containers are built first. Each
build writes a log::

    /tmp/doitall-logs/cnt_trixie.log
    /tmp/doitall-logs/kvm_trixie.log

A failure stops the run and names the log and the retry, *./doitall.sh
--cnt trixie*. Every container builder ends by calling *cnt_common_config*
from *cnt_kvm/cnt_common.sh*: syslog configuration, *locale.gen*,
*locale.conf*, */etc/hosts*, root present in *passwd* and *group*.

Everything at once: allbulk.sh
==============================

*vm_create/allbulk.sh* runs both drivers with *--all*, one after the other,
and delivers what they built into four directories of the home, ready to be
published or copied to another machine::

    ./allbulk.sh                 the help and the names, builds nothing
    ./allbulk.sh --go            hours: every image of both directories
    ./allbulk.sh --deliver       no build: deliver what bulk holds

    cnt_kvm/doitall.sh --all        ~/cnt_bulk/<image>.tar.gz
                                    ~/kvm_bulk/<image>.qcow2.gz
    cnt_kvm_demo/doitall.sh --all   ~/cnt_demo_bulk/<image>.tar.gz
                                    ~/kvm_demo_bulk/<image>.qcow2.gz

A container image is copied as it is, a qcow2 is gzipped, with *pigz* when
the machine has it; the images stay in bulk. It stops at the first error: a
builder that fails, a listed image that is not in bulk, or one that this run
did not build. After such a stop the failed builder is corrected and run
alone, ``./doitall.sh --desktop alpine`` as the failure says, the rest is
built with the two drivers, and *--deliver* finishes the work without
building anything again. What goes to each directory is what the *--list* of its
driver names, the desktops included, not what bulk happens to hold. A
delivered machine image is put back in use with
``gunzip -c trixie.qcow2.gz > /var/lib/cloonix/bulk/trixie.qcow2``.



Building a container image
==========================

*cnt_<distro>.sh* builds a directory and delivers it as *<distro>.tar.gz* in
bulk, the directory erased, by *cnt_common_pack* of *cnt_common.sh*: the
tree is packed from the user namespace it was built in, so its owners read
0 upward inside the archive. The package list of the debian one:
*adduser*, *iproute2*, *iputils-ping*, *net-tools*, *procps*, *rsyslog*,
*tcpdump*, *strace*, *vim*, *kmod*, and for the wifi demo *iw*, *hostapd*,
*wpasupplicant*. No *dbus*: nothing starts it in a container without an
init, and a package that needs the library pulls it in on its own.

The debian images carry no systemd; *trixie_systemd*, built by
*cnt_trixie_systemd.sh*, is the one with an init for the demos that need
one. On rocky, fedora, tumbleweed and arch systemd comes with the base and is
never started unless asked. A syslog daemon is in every image, *rsyslog*
everywhere but arch, where it is *syslog-ng*; *cloonixsbininit* starts
whichever it finds, so a container keeps a */var/log/syslog*, readable from
the host under */var/lib/cloonix/<net>/cnt/<n>/rootfs/var/log/*.

::

    cd vm_create/cnt_kvm
    ./cnt_trixie.sh                                 writes bulk/trixie.tar.gz
    cloonix cli nemo add cnt cnt1 eth=1 trixie      extracts it at this first use

A *cnt_* script refuses to run if its target directory or tar.gz exists;
*doitall.sh* erases both, and the *_targz* tree a net extracted from the
previous archive, before a rebuild.

The demo images::

    cd vm_create/cnt_kvm_demo
    ./cnt_frr_trixie.sh          bulk/frr_trixie.tar.gz: trixie plus frr and frr-pythontools

No configuration is baked into a demo image: the demo writes the daemons
file, *frr.conf*, keys, zones and rulesets at run time over *cloonix cli
ssh*, so one image serves several demos and no key ends in a tarball.


Building a virtual machine image
================================

*kvm_<distro>.sh* assembles a kernel, an initramfs, a bootloader, a
partition table and a file-system without a loop device and without
mounting anything: *mkfs.ext4 -E offset= -d* writes the file-system into the
image at the right offset, *grub-bios-setup* takes the image path, and
*grub.cfg* is written by hand.

.. image:: /svg/disk_layout.svg
   :align: center
   :width: 100%

The build ends with two checks: the boot signature *55 aa* in the first
sector, and a non-zero BIOS boot partition at sector 2048. The previous
image is kept as *<name>.qcow2.prev* until the new one is written. A lock
on */var/lib/cloonix/bulk* refuses two image builds at once, whoever runs
them. The scripts write nothing beside themselves: any user runs them from
anyone's tree.
*KEEP_TREE=1* keeps the intermediate tree and skips the bootstrap on the
next run::

    KEEP_TREE=1 ./kvm_trixie.sh

What the builders put in a machine image::

    qemu-guest-agent enabled          cloonix drives it to start its agent
    virtio modules forced at boot     /etc/modules-load.d/virtio.conf
    wait-online units masked          no 90 s stall when no dhcp answers
    a getty on ttyS0                  the Console panel
    no periodic fsck
    root password root
    a user named user, password user  trixie, resolute, arch; and every desktop,
                                      whose builder creates it when the base has none

*kvm_freebsd.sh* is the exception to the mirror rule, and for that reason
not one of the builders *doitall.sh* runs: it is called by hand. It takes the
disc1 iso of FreeBSD 15.1 from download.freebsd.org, installs the base system
from the packages that disc carries with *bsdinstall* driven by an
*installerconfig*, and fetches one package, qemu-guest-agent. The serial
console is turned on by the rebuilt iso and by the post-install script.

To test an image outside cloonix with the cloonix qemu, console on the
terminal, Ctrl-a then x to leave::

    ./tester_cloonix_qemu.sh


The desktop images
==================

Built by cloonix itself: the plain image is booted persistent, the desktop
is installed through a nat, the guest is powered off::

    ./kvm_trixie_gnome.sh

::

    kvm_alpine_xfce.sh        kvm_trixie_lxde.sh       kvm_trixie_gnome.sh
    kvm_resolute_gnome.sh     kvm_rocky_gnome.sh       kvm_fedora44_gnome.sh
    kvm_arch_plasma.sh        kvm_tumbleweed_plasma.sh

The eight share *desk_common.sh*, where nothing fails quietly. What the
desktop cannot do without is fatal, says what failed and removes the image:
the mirror out of reach, the desktop install, no session command, no display
manager enabled, a guest that does not power off. What it can do without is
a *WARNING*, printed when it happens and again as the last lines of the
builder; *doitall.sh* repeats the warnings of every builder under its final
summary.

Each installs firefox, chromium and epiphany, one engine each, a missing
package costing that browser only, with a warning. Ubuntu ships firefox
and chromium as snaps, which a guest on a local mirror cannot fetch, so
that image installs epiphany only and pins snapd out. Open the result with
*Desktop* in the machine menu.


Using your own image
====================

Any qcow2 that boots on a BIOS machine with a virtio-scsi disk runs as a
kvm; a gpt disk takes *--uefi*; any directory holding an */sbin/init* runs as
a cnt, whoever owns it. For a kvm, install and enable *qemu-guest-agent*,
or create the guest with *--no_qemu_ga* and use the Console panel.

The root file-system of the toy is usable as a container image under the
name *self_rootfs*.
