=============================
Instructions for installation
=============================

This chapter is **way 1**, the bundle: root once at install time, cloonix in
*/usr/libexec/cloonix/cloonfs*, run as ordinary processes of the user. **Way
2**, the toy, needs no root at all: the chapter *Cloonix toy*. Neither needs
a package installed first.


Installing the bundle
=====================

::

    wget https://clownix.net/downloads/cloonix-bundle.tar.gz
    tar xvf cloonix-bundle.tar.gz
    cd cloonix-bundle
    sudo ./install_cloonfs

What *install_cloonfs* does::

    nets of the installed cloonix running        refused, with the cloonix stop line
                                                 of each: they would keep the old binaries
    rm -f /usr/bin/cloonix*                      no name of an older release survives
    /usr/libexec/cloonix/cloonfs                 binaries, libraries, etc/cloonix.cfg
    /usr/libexec/cloonix/insider_agents          the three agent discs
    /usr/libexec/cloonix/startup_bin             the nine shell tools the init of the toy uses
    /usr/bin/cloonix  /usr/bin/cloonix_*.py      the client and its helpers
    /usr/bin/cloonix_config.py                   a link to cloonfs/etc/cloonix.cfg
    /usr/share/cloonix/demos                     sticky, shipped demos owned by root
    /usr/share/cloonix/pcap                      sticky
    /usr/share/cloonix/cert                      certificate copied from the tarball
    /var/lib/cloonix                             created, sticky
    /var/lib/cloonix/bulk                        created, mode 777
    chmod u+s cloonix-suid-power                 the one setuid binary

The helpers beside the client: *cloonix_files.py* the two panes,
*cloonix_demo.py* the demo player, *cloonix_atop.py* the ascii topology,
*cloonix_completion.py* the words a tab offers, *cloonix_config.py* the nets,
the server's *cloonix.cfg* itself.

Every account of the machine can then start a net; the passwords and the
certificate key are readable by all. What can be closed, and how, is the
section *Hardening after the install* of the chapter *Safety and attack
surface*, each step optional. An existing group named *cloonix* is reported
by the install: that group is the switch of the gate on
*cloonix-suid-power*.

A reinstall keeps the installed certificate, since a browser refuses a
second certificate for a host and port it already trusts. To take the one
of the tarball anyway::

    sudo CLOONIX_REPLACE_CERT=1 ./install_cloonfs


Guest images
============

The installation brings no guest. The server takes them from **bulk**::

    /var/lib/cloonix/bulk/<name>.qcow2       kvm root file-systems
    /var/lib/cloonix/bulk/<name>.tar.gz      cnt root file-systems, as downloaded
    /var/lib/cloonix/bulk/<name>/            cnt root file-systems, extracted

A kvm image, a container image, and the image of the frr demos::

    mkdir -p /var/lib/cloonix/bulk
    cd /var/lib/cloonix/bulk
    wget https://clownix.net/downloads/kvm_bulk/trixie.qcow2.gz
    gunzip trixie.qcow2.gz
    wget https://clownix.net/downloads/cnt_bulk/trixie.tar.gz
    wget https://clownix.net/downloads/cnt_demo_bulk/frr_trixie.tar.gz

https://clownix.net/downloads.html lists every image, in the four
directories that *vm_create/allbulk.sh* of the source tree fills::

    downloads/cnt_bulk/<image>.tar.gz         the distributions, as containers
    downloads/kvm_bulk/<image>.qcow2.gz       the distributions, as machines, and the desktops
    downloads/cnt_demo_bulk/<image>.tar.gz    one container image per subject of the demos
    downloads/kvm_demo_bulk/<image>.qcow2.gz  the machine images of the demos

*add cnt ... trixie* takes the directory *trixie* of bulk when there is one,
*trixie.tar.gz* otherwise: the first container that asks extracts it, once,
into *trixie_targz*, shared by every net and user of the machine; the wait
is the extraction. The topology names the image *trixie_targz*. That tree is
mounted read only for each container and stays what *trixie.tar.gz* holds:
never edit it. A tar.gz image is never persistent: *--persistent* needs the
directory form, ``tar xvf trixie.tar.gz`` in bulk, or a copy under another
name, ``cp -a trixie_targz mytrixie``. Whoever unpacked it, sudo or not, a
container sees the image with its owner's ids translated into those of the
user who started the net. The chapter on the guest root file-systems has
the builders.

A guest on an image absent from bulk is refused; the refusal names the
builder of *vm_create*, in *cnt_kvm/* or *cnt_kvm_demo/*, and the download::

    REFUSED by the cloonix server at 127.0.0.1:45211
      asked : add cnt c1 eth=1 wg_trixie
      reason: wg_trixie is not in /var/lib/cloonix/bulk, as a directory or as
              wg_trixie.tar.gz: build it with cnt_wg_trixie.sh of vm_create,
              or take wg_trixie.tar.gz from https://clownix.net/downloads.html
              and put it there, cloonix extracts it

A demo prints that line first and stops.


First run
=========

.. image:: /svg/first_net.svg
   :align: center
   :width: 100%

Start the server::

    cloonix start nemo

Open the canvas, on this machine or from another one by its address; the
browser warns once about the self-signed certificate::

    https://127.0.0.1:4444
    https://<ip>:4444

The page asks for a user and a password, not secret: they are shipped in the
configuration, where they can be changed::

    https://127.0.0.1:4444      user  nemo      password  nemoclown

    the file, bundle      /usr/libexec/cloonix/cloonfs/etc/cloonix.cfg
    the file, toy         <toy directory>/rootfs/cloonix/cloonfs/etc/cloonix.cfg

The user is the name of the net, the password its *cloonix_passwd* in that
file, the net name followed by *clown*: user *nemo*, password *nemoclown*.

Two machines and a ping::

    cloonix cli nemo add kvm kvm1 ram=1000 cpu=2 eth=1 trixie.qcow2
    cloonix cli nemo add kvm kvm2 ram=1000 cpu=2 eth=1 trixie.qcow2
    cloonix cli nemo plug kvm1 eth0 lan1
    cloonix cli nemo plug kvm2 eth0 lan1
    cloonix cli nemo ssh kvm1 "ip addr add dev eth0 1.1.1.1/24"
    cloonix cli nemo ssh kvm1 "ip link set dev eth0 up"
    cloonix cli nemo ssh kvm2 "ip addr add dev eth0 1.1.1.2/24"
    cloonix cli nemo ssh kvm2 "ip link set dev eth0 up"
    cloonix cli nemo ssh kvm1 "ping -c 3 1.1.1.2"

These nine lines, run by a program that waits for the guests and prints
each line, are the demo *cloonix/kvm.py*; *cloonix/cnt.py* is its twin with
two containers (chapter *The first network*). A guest answers *ssh* a few
seconds after its creation; a script waits like this::

    while ! cloonix cli nemo ssh kvm1 "echo" 2>/dev/null; do sleep 1; done

Two containers on a second lan:

.. image:: /svg/cnt_kvm_topo.svg
   :align: center
   :width: 100%

::

    cloonix cli nemo add cnt cnt1 eth=1 trixie
    cloonix cli nemo add cnt cnt2 eth=1 trixie
    cloonix cli nemo plug cnt1 eth0 lan2
    cloonix cli nemo plug cnt2 eth0 lan2

Erase every object, the server staying, then stop the server::

    cloonix cli nemo del all
    cloonix stop nemo


Erasing cloonix from the host
=============================

::

    sudo rm -rf /usr/libexec/cloonix
    sudo rm -f /usr/bin/cloonix*
    sudo rm -rf /usr/share/cloonix
    sudo rm -rf /var/lib/cloonix
