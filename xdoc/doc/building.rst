.. image:: /png/cloonix128.png 
   :align: right

=============================
Instructions for installation
=============================


How the building is done.
=========================

Cloonix recompiles most of the open-sources it uses and sometimes patches
them for adjustments.
Libraries are put into a separate zone only for cloonix use.
This allows the use of customized libs with no impact on the official host machine.

The host machine can have qemu, openvswitch and spice installed, cloonix will
use its own version of these softwares. None of the host's binary are called
except for podman and docker.

Wireshark and crun are not required in the host, those software are embedded in
the bundle delivered with cloonix binaries.


Sources associated to dpdk/ovs
==============================

The external open sources used in cloonix for qemu, ovs, spice ...
are updated using the following links:

  * https://github.com/openvswitch/ovs.git
  * https://git.qemu.org/git/qemu.git
  * https://gitlab.freedesktop.org/spice/spice-gtk.git
  * https://gitlab.freedesktop.org/spice/spice-protocol.git
  * https://gitlab.freedesktop.org/spice/spice.git
  * https://gitlab.freedesktop.org/spice/usbredir.git
  * https://ftp.openbsd.org/pub/OpenBSD/OpenSSH/portable/openssh-8.6p1.tar.gz
  * https://github.com/containers/crun.git
  * https://github.com/wireshark/wireshark.git


Installation of binaries
========================

The cloonix installed binaries are provided by cloonix-bundle-<version>,
the *install_cloonix* script installs files in the folowing locations::

  /usr/libexec/cloonix/etc/cloonix.cfg                       config
  /usr/libexec/cloonix/common                                binaries
  /usr/libexec/cloonix/client                                binaries
  /usr/libexec/cloonix/server                                binaries
  /usr/bin/cloonix_net                                       server handle script
  /usr/bin/cloonix_(cli, gui, scp, ssh, ovs, ice, ocp, osh)  client handle scripts

The software needs a working directory to provide its services, this working
directory depends on the name of the launched cloonix net::

  /var/lib/cloonix/<net-name>                                work directory


Here under is the complete list of commands for the binary installation::
  
  wget http://clownix.net/downloads/cloonix-__LAST_BASE__/cloonix-bundle-__LAST__.tar.gz
  tar xvf cloonix-bundle-__LAST__.tar.gz
  cd cloonix-bundle-__LAST__
  sudo ./install_cloonix



Guest download
==============

Cloonix needs root file-systems to run guests, the above installation
does not populate the guest qcow2 files, the server software expects to
find those guest file-systems inside a directory called **bulk**::

  /var/lib/cloonix/bulk/*                                    vm file-systems


Here under is the complete list of commands to populate the bulk with qcow2::

  mkdir -p /var/lib/cloonix/bulk
  cd /var/lib/cloonix/bulk
  wget http://clownix.net/downloads/cloonix-__LAST_BASE__/bulk/bookworm.qcow2.gz
  gunzip bookworm.qcow2.gz

For the crun use, commands to populate the bulk with ext4 image files::

    wget http://clownix.net/downloads/cloonix-__LAST_BASE__/bulk/bookworm.img.gz
    gunzip bookworm.img.gz

For the podman or docker container guests, you have to create a podman or docker
image in your host outside of cloonix. This image gets to be visible in the Cnt_conf
sub-menu of the menu obtained upon a right-click when above the canvas.
Note that you must click on the docker or podman container type to get to the list
of images through Choice.


Erase cloonix from host
=======================

A simple way to uninstall cloonix is by erasing all the installed files
The binaries and config file::

  rm -rf /usr/libexec/cloonix

The scripts that call the binaries::

  rm -f /usr/bin/cloonix_*

The storage of all the virtual machine and working files::

  rm -f /var/lib/cloonix


Cloonix sources
===============

The cloonix source to compile is on github::

    git clone --depth=1 https://github.com/clownix/cloonix.git


Running the test scripts
=========================

In the cloonix-bundle, scripts are added for an easy test::

  cd cloonix-bundle-__LAST__

  ./ping_kvm.sh
  or
  ./ping_crun.sh

Here is a typical manual start for a server and a gui client::

    cloonix_net nemo 
    cloonix_gui nemo

The cloonix_gui is the first client to launch as it will show the cloonix
objects in real time as they are created.

The cloonix_gui provides a shell for kvm or crun virtual with a double-click
on a blue virtual machine.

