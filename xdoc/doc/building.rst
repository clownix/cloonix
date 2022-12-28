.. image:: /png/cloonix128.png 
   :align: right

=============================
Instructions for installation
=============================


How the building is done.
=========================

The control of the source code is of great importance, using software from git
is necessary to have the power to correct small problems by direct patches of
the code.
For this reason, cloonix recompiles most of the open-sources it uses and
sometimes patches them for adjustments.
Libraries that are recompiled are put into a separate tree only for cloonix
use. This allows the use of customized libs with no impact on the official
host machine.

The place where all compiled cloonix bin and libs are installed is:
/usr/local/bin/cloonix
The host machine can have qemu, openvswitch and spice installed, cloonix will
use its own version of these softwares.
This way of working gives a very simple way to uninstall cloonix with a simple
"rm -rf /usr/local/bin/cloonix*"

Sources associated to dpdk/ovs
==============================

The external open sources used in cloonix for qemu, ovs, spice ...
are regularly updated through scripts in tools/update_open_sources,
here are the git sources for what can be found in targz_store:

  * https://github.com/openvswitch/ovs.git
  * https://git.qemu.org/git/qemu.git
  * https://gitlab.freedesktop.org/spice/spice-gtk.git
  * https://gitlab.freedesktop.org/spice/spice-protocol.git
  * https://gitlab.freedesktop.org/spice/spice.git
  * https://gitlab.freedesktop.org/spice/usbredir.git
  * https://ftp.openbsd.org/pub/OpenBSD/OpenSSH/portable/openssh-8.6p1.tar.gz


Install
=======

The installation of cloonix is provided from source only with compilation
on host and install in /usr/local/bin/cloonix of the host. For more info
on the dependancy, look into **install_depends** file.
The podman package is not necessary for cloonix use, if you which, you can
suppress the line installing it before launching the script.

The 2 main steps to have a successfull installation are::

    sudo ./install_depends

which will install all the necessary developement packages on your host.
and::

    ./doitall

Here under is the complete list of commands for this installation::
  
    cd ${HOME}
    wget http://clownix.net/downloads/cloonix-__LAST_BASE__/cloonix-__LAST__.tar.gz
    tar xvf cloonix-__LAST__.tar.gz
    cd cloonix-__LAST__
    wget http://clownix.net/downloads/cloonix-__LAST_BASE__/targz_store.tar.gz
    tar xvf targz_store.tar.gz

    sudo ./install_depends
    ./doitall


To erase all trace of cloonix from the host ::

    rm -rf /usr/local/bin/cloonix*


The cloonix source to compile and install is also on github::

    git clone --depth=1 https://github.com/clownix/cloonix.git


Guest download
==============

Cloonix needs root file-systems to run guests, the above installation
does not populate the guest qcow2 files, the server software expects to
find those guests' file system inside a directory called **bulk**, here
under is the default path of the bulk, it is configured inside the
cloonix_config file::

     ${HOME}/cloonix_data/bulk

Here is the complete list of commands to download our first vm guest,
qcow2 for kvm and img for crun::

    mkdir -p ${HOME}/cloonix_data/bulk
    cd ${HOME}/cloonix_data/bulk
    wget http://clownix.net/downloads/cloonix-__LAST_BASE__/bulk/bookworm.qcow2.gz
    wget http://clownix.net/downloads/cloonix-__LAST_BASE__/bulk/bookworm.img.gz
    gunzip bookworm.qcow2.gz
    gunzip bookworm.img.gz

For the podman or docker container guests, you have to create a podman or docker
image and then this image gets to be visible in the Cnt_conf sub-menu of the
menu obtained upon a right-click when above the canvas. Note that you must click
on the docker or podman container type to get to the list of images through Choice.


Running
=======

In most cases, the first try is done on the same host for the server and
client cloonix, the pre-configured cloonix "nemo" name is the usual
default choice::

    cd ${HOME}/cloonix-__LAST__/quickstart
    ./ping_kvm.sh or ./ping_crun.sh

Here is a typical manual start for a server and a gui client::

    cloonix_net nemo 
    cloonix_gui nemo

The cloonix_gui is the first client to launch as it will show the cloonix
objects in real time as they are created.

The cloonix_gui provides a shell for kvm or crun virtual with a double-click
on a blue virtual machine.


Tested on
=========

The install_depends file has been tested to install the correct dependancies
and the compilation and first run was successfull for the following
distributions::

    * *jammy   (ubuntu 22.04),
    * *bookworm (debian 12),
    * *bullseye (debian 11),
    * *tumbleweed (rolling opensuse),
    * *fedora36
    * *centos9
