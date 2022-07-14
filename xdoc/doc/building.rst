.. image:: /png/cloonix128.png 
   :align: right

=====================
Instructions for use
=====================


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
The host machine can have qemu, openvswitch and dpdk installed, cloonix will
use its own version of these softwares.
This way of working gives a very simple way to uninstall cloonix with a simple
"rm -rf /usr/local/bin/cloonix*"

Sources associated to dpdk/ovs
==============================

The external open sources used in cloonix for qemu, dpdk, ovs ...
are regularly updated through scripts in tools/update_open_sources,
here are the git sources for what can be found in targz_store:

  * https://github.com/openvswitch/ovs.git
  * https://github.com/mesonbuild/meson.git
  * https://github.com/ninja-build/ninja.git
  * https://git.qemu.org/git/qemu.git
  * https://gitlab.freedesktop.org/spice/spice-gtk.git
  * https://ftp.openbsd.org/pub/OpenBSD/OpenSSH/portable/openssh-8.6p1.tar.gz


Install
=======

The installation of cloonix is provided from source only with compilation
on host and install in /usr/local/bin/cloonix of the host. For more info
on the dependancy, look into **install_depends** file.
The 2 main steps to have a successfull installation are::

    sudo ./install_depends

which will install all the necessary developement packages on your host.
and::

    ./doitall

Here under is the complete list of commands for this installation::
  
    cd ${HOME}
    wget http://clownix.net/downloads/cloonix-__LAST__/cloonix-__LAST__.tar.gz
    tar xvf cloonix-__LAST__.tar.gz
    cd cloonix-__LAST__
    wget http://clownix.net/downloads/cloonix-__LAST__/targz_store.tar.gz
    tar xvf targz_store.tar.gz

    sudo ./install_depends
    ./doitall


To erase all trace of cloonix from the host ::

    rm -rf /usr/local/bin/cloonix*

Guest download
==============

Cloonix needs root file-systems to run guests, the above installation
does not populate the guest qcow2 files, the server software expects to
find those guests' file system inside a directory called **bulk**, here
under is the default path of the bulk, it is configured inside the
cloonix_config file::

     ${HOME}/cloonix_data/bulk

Here is the complete list of commands to download our first vm guest::

    mkdir -p ${HOME}/cloonix_data/bulk
    cd ${HOME}/cloonix_data/bulk
    wget http://clownix.net/downloads/cloonix-__LAST__/bulk/bookworm.qcow2.gz
    gunzip bookworm.qcow2.gz


Running
=======

In most cases, the first try is done on the same host for the server and
client cloonix, the pre-configured cloonix "nemo" name is the usual
default choice. Before run, have a look at the Host Customisation chapter.
For the first run, use the ping.sh script with the sock option as follows
to avoid any dpdk host misconfiguration problems::

    cd ${HOME}/cloonix-__LAST__/quickstart
    ./ping_kvm.sh

Here is a typical manual start for a server and a gui client::

    cloonix_net nemo 
    cloonix_gui nemo

The cloonix_gui is the first client to launch as it will show the cloonix
objects in real time as they are created.

The cloonix_gui provides cloonix_ssh by a double-click on a blue virtual
machine, cloonix_ssh natively provides an x11 path.


Saving
======

The saving of a vm is done with the following commands::
    
    cloonix_cli nemo sav <name> <complete_file_path>

The save creates an autonomous qcow2 file merging the derived and the
backing files into one.


Tested on
=========

The install_depends file has been tested to install the correct dependancies
and the compilation and first run was successfull for the following
distributions:

    * *jammy   (ubuntu 22.04),
    * *impish  (ubuntu 21.10),
    * *hirsute (ubuntu 21.05),
    * *bookworm (debian 12),
    * *bullseye (debian 11),
    * *tumbleweed (rolling opensuse),
    * *fedora35