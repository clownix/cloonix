.. image:: /png/cloonix128.png 
   :align: right

=============================
Instructions for installation
=============================


Installation of binaries
========================

The cloonix installed binaries are provided by cloonix-bundle-<version>-amd64,
the *install_cloonix* script installs files in the folowing locations::

  /usr/libexec/cloonix/etc/cloonix.cfg                       config
  /usr/libexec/cloonix/common                                binaries
  /usr/libexec/cloonix/server                                binaries
  /usr/bin/cloonix_net                                       server script
  /usr/bin/cloonix_(cli,gui,ice,ssh,scp,wsk,ovs,osh,ocp)     client scripts

The software needs a working directory to provide its services, this working
directory depends on the name of the launched cloonix net::

  /var/lib/cloonix/<net-name>                                work directory


Here under is the complete list of commands for the binary installation::
  
  wget http://clownix.net/downloads/cloonix-__LAST_BASE__/cloonix-bundle-__LAST__-amd64.tar.gz
  tar xvf cloonix-bundle-__LAST__-amd64.tar.gz
  cd cloonix-bundle-__LAST__-amd64
  sudo ./install_cloonix


Guest download
==============

Cloonix needs root file-systems to run guests, the above installation
does not populate the guest qcow2 files, the server software expects to
find those guest file-systems inside a directory called **bulk**::

  /var/lib/cloonix/bulk/*             kvm qcow2 and crun zip file-systems

Here under is the complete list of commands to populate the bulk with qcow2::

  mkdir -p /var/lib/cloonix/bulk
  cd /var/lib/cloonix/bulk
  wget http://clownix.net/downloads/cloonix-__LAST_BASE__/bulk/bookworm.qcow2.gz
  gunzip bookworm.qcow2.gz

For the crun use, commands to populate the bulk with zip file-systems::

    wget http://clownix.net/downloads/cloonix-__LAST_BASE__/bulk/busybox.zip


Howto erase cloonix from host
=============================

A simple way to uninstall cloonix is by erasing all the installed files
The binaries and config file::

  rm -rf /usr/libexec/cloonix

The scripts that call the binaries::

  rm -f /usr/bin/cloonix_*

The storage of all the virtual machine and working files::

  rm -f /var/lib/cloonix


Running the test scripts
=========================

In the cloonix-bundle, scripts are added for an easy test::

  cd cloonix-bundle-__LAST__-amd64

  ./ping_demo.sh

Here is a typical manual start for a server and a gui client::

    cloonix_net nemo 
    cloonix_gui nemo

The cloonix_gui is the first client to launch as it will show the cloonix
objects in real time as they are created.

The cloonix_gui provides a shell for kvm or crun virtual with a double-click
on a blue virtual machine.


