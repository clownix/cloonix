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

Two ways to fill it: download, the section *Guest images* of the chapter
*Instructions for installation*, or build with *vm_create*, this chapter. A
downloaded tar.gz is used as it is; unpacked by hand, with *tar xvf*, the
directory comes first and takes *--persistent*. Either way a container sees
the image with its owner's ids translated into those of the user who started
the net.


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
                             the kvm images a demo needs beyond the distributions;
                             27 of the 29 cnt_<n>_trixie.sh share demo_common.sh
    vm_create/cisco          the Catalyst 8000V image, from a cisco iso

Nothing in them uses sudo except *check_www_links.sh*, which *allbulk.sh*
runs, and *mydistros.sh*, *allbulk.sh* and the two *doitall.sh* when they
create their log in */var/log*. Every builder runs
inside a user namespace and writes into bulk only.


A local mirror first
====================

.. image:: /svg/mirror.svg
   :align: center
   :width: 100%

The builders install from a web server on your own machine, never from the
internet. freebsd has no mirror, so *doitall.sh* does not build it:
*kvm_freebsd.sh* is run by hand. One script per distribution in
*get_distro*: *get_debian.sh*, *get_ubuntu.sh*, *get_fedora.sh*,
*get_rocky.sh*, *get_alpine.sh*, *get_arch.sh*, *get_opensuse.sh*,
*get_openwrt.sh*; and two for what no distribution carries, pinned by their
sha256: *get_pola.sh*, the Pola release of the frr_trixie container, and
*get_asterisk.sh*, the Asterisk tarball and the pjproject it bundles, which
*asterisk_build.sh* compiles for asterisk_trixie, trixie having no asterisk.
They write into the store named at the top of *distro_common.sh*, the
author's disk::

    STORE=/media/perrier/T7Shield2/distros

A distribution of fixed releases keeps the list of its releases at the top
of its script, mirrors each into a directory of its own and refreshes only
the releases given as arguments. *get_debian.sh* and *get_ubuntu.sh* write a
tree per release, named by its codename, holding the release and its
-updates; the -security of ubuntu goes into that tree too, the one of debian,
served by another host, into a tree of its own. *get_fedora.sh* and
*get_rocky.sh* write one tree each, *fedora* and *rocky*, the releases inside
laid out as upstream::

    ./get_fedora.sh 45
    trixie/dists/trixie-updates/Release
    trixie-security/dists/trixie-security/Release
    fedora/releases/45/Everything/x86_64/os/repodata/repomd.xml
    rocky/10/BaseOS/x86_64/os/repodata/repomd.xml

The arch, tumbleweed, alpine and openwrt trees are updated in place, the
packages of a new release replacing those of the old one. The debian and
ubuntu trees hold the amd64 binaries, every Translation and the Contents,
no source package. The debian and ubuntu builders install from the release,
its -updates and its -security, so an image carries the fixes published up to
the day it is built, and its sources.list names the three.

*mydistros.sh* runs every *get_<distro>.sh* at once, one process per
distribution in a detached session, and writes all their output, one
prefixed line each, into */var/log/mydistros*, emptied at each launch. It
runs them from a copy of *get_distro* made at launch, so the tree can
change while the downloads last. Its own lines, tagged *[mydistros]*,
report the disk absent at launch (nothing starts) or gone during the run
(every download stops), the first network error of a distribution
(*UNREACHABLE*), a script ending in error, which runs once more (*RETRY*),
and at the end, for each one failing twice, the lines of its output that
state the problem, then the list of the ok and failed ones::

    ./mydistros.sh
    tail -f /var/log/mydistros
    grep '\[mydistros\]' /var/log/mydistros

A launch made while one runs is refused and leaves the log of the running
one alone; the refusal prints the command that stops it. *--stop* sends a
TERM to every download, a KILL after 30 s to what is left, and lists the
processes still holding the launch when a KILL cannot end them, such as a
download stuck on the disk::

    ./mydistros.sh --stop

Then the symlinks under */var/www/html*, one per tree and named as the
tree, and the check, through the web server, of every file the builders
read::

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

Before launching, both check: not root, known names, bulk writable,
*unshare*, *flock*, *mkfs.ext4* and *qemu-img* installed. The run then goes
on without the terminal, from a copy of *vm_create* made at launch, and
tries every build in turn, containers first, desktops last. A script
missing or not executable, one calling sudo, a desktop whose base image is
absent, a builder ending in error: each is a failure of that build alone,
and the run goes on. Its whole output, then a report, go into
*/var/log/doitall_cnt_kvm.log* or */var/log/doitall_cnt_kvm_demo.log*: each
failure with its problem lines and its last ones, the warnings the ok
builds went on with, the images built. What a failed build leaves is
erased, a container tree or tar.gz, a qcow2 written by it; an older qcow2
it did not touch stays. *--wait* follows the run here and
returns its status. A second launch of the same driver is refused; the
other driver waits for the first to end, the two sharing bulk::

    ./doitall.sh --cnt --kvm
    tail -f /var/log/doitall_cnt_kvm.log
    grep '\[doitall\]' /var/log/doitall_cnt_kvm.log

Every container builder ends by calling *cnt_common_config*
from *cnt_kvm/cnt_common.sh*: syslog configuration, *locale.gen*,
*locale.conf*, */etc/hosts*, root present in *passwd* and *group*.

Everything at once: allbulk.sh
==============================

*vm_create/allbulk.sh* is the complete renewal. It publishes the mirrors
with *check_www_links.sh* and erases nothing unless every file the builders
read is served and the machine can build. It then erases every image the two drivers list,
in bulk and in four directories of the home, runs both
drivers with *--all* to their end, one after the other, delivers what they
built into those four directories, ready to be published or copied to
another machine, and goes on without the terminal::

    ./allbulk.sh                 the help and the names, builds nothing
    ./allbulk.sh --go            hours: erase, build, deliver, report
    ./allbulk.sh --deliver       no build: deliver what bulk holds, here
    tail -f /var/log/allbulk.log

    cnt_kvm/doitall.sh --all        ~/cnt_bulk/<image>.tar.gz
                                    ~/kvm_bulk/<image>.qcow2.gz
    cnt_kvm_demo/doitall.sh --all   ~/cnt_demo_bulk/<image>.tar.gz
                                    ~/kvm_demo_bulk/<image>.qcow2.gz

A container image is copied as it is, a qcow2 is gzipped, with *pigz* when
the machine has it; the images stay in bulk. A build that fails leaves no
image, in bulk or in the home, so what the renewal leaves is valid.
*/var/log/allbulk.log* ends with the reports of both driver logs, the
failed builds and what was delivered. A failed builder, once corrected, is
run alone, ``./doitall.sh --desktop alpine --wait``, and *--deliver*
finishes the work without building anything again. What goes to each
directory is what the *--list* of its driver names, the desktops included,
not what bulk happens to hold. A
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

The openwrt image is made otherwise: *kvm_openwrt.sh* keeps the master boot
record, grub and kernel of the combined image OpenWrt publishes, rebuilds
its root partition from the rootfs tarball with *qemu-ga* added, and checks
the ext4 at partition 2. Its kernel has virtio-scsi, the cd-rom and iso9660
built in, so it boots without an initramfs and mounts the agent disc.

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
    kvm_resolute_gnome.sh     kvm_rocky_gnome.sh       kvm_fedora45_gnome.sh
    kvm_arch_plasma.sh        kvm_tumbleweed_plasma.sh

The eight share *desk_common.sh*, where nothing fails quietly. What the
desktop cannot do without is fatal, says what failed and removes the image:
the mirror out of reach, the desktop install, no session command, no display
manager enabled, a guest that does not power off. What it can do without is
a *WARNING*, printed when it happens and again as the last lines of the
builder; *doitall.sh* repeats the warnings of every ok build in the report
at the end of its log.

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
