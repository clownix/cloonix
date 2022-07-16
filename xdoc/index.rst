.. image:: /png/smile_cloonix_title.png 
   :align: right

=====================
Cloonix Documentation
=====================

:Version: |version|
:Date: |today|

This is the documentation for the cloonix open source hosted at

    * http://clownix.net

Cloonix is a tool that helps in the creation of emulated networks made
with virtual machines and emulated connections.

Its first goal is an easy usage of complex open sources such as
qemu-kvm and openvswitch through visual items that will be called
**guitems** throughout this doc.

Its second goal is to have people share network scripts of topology
demonstrations based on cloonix, it makes reproduction of complex
network scenarios easy.

In a few words, cloonix is a AGPLv3 set of C software elements working
together aiming at the coherent usage of multiple well known open
source softwares, the main ones being: **qemu-kvm**, **openvswitch**,
**spice**, **wireshark** and **openssh**.

Cloonix manages clusters of virtual machines, the server is controled
by **clients** that act from a distant host or on the same host.
All cloonix clients are named **cloonix_xxx** xxx can be one of the
following: gui, cli, ssh, scp, dta, ice, mon, osh, ocp, ovs, xwy.

The cloonix server is launched on the server and is named **cloonix_net**,
it creates and controls **guitems** that run on the server and are
pictured on the gui canvas launched with **cloonix_gui**.

The main guitem is a machine representation, it is either a **kvm**
virtual machine or a **cnt** crun container. Around this guitem there
are 4 other: **lan**, **nat**, **tap**, **c2c**,
those guitems are described within this documentation.

The **kvm** or **cnt** machines can be connected by cloonix commands, the
emulated connections between virtual machines are based on **openvswitch**
and pictured by the **lan** guitem.

Emulated desktops for the **kvm** virtual machines are provided through
**spice** launched with a right-click when above the **kvm** on the gui.

A double-click on a blue virtual machine kvm or cnt creates a shell. 

A double-click on a virtual machine interface (small circle bordering the vm)
opens a **wireshark** spy that displays the packets running through this
interface.

You can grab a single file html of this doc to print it:

   * `Single html file cloonix doc <../singlehtml/index.html>`_

The cloonix source to compile and install is on github or clownix.net::

    git clone --depth=1 https://github.com/clownix/cloonix.git
    wget http://clownix.net/downloads/cloonix-__LAST__/cloonix-__LAST__.tar.gz

All cloonix compiled open sources softwares are installed with cloonix
software at **/usr/local/bin/cloonix** and can be removed with a simple rm.

These necessary softwares are associated to a cloonix version, download it::

    wget http://clownix.net/downloads/cloonix-__LAST__/targz_store.tar.gz

The virtual machines have file-systems that are .qcow2 files, you
can download qcow2 guest virtual machines here:

    * `Qcow2 guests <http://clownix.net/downloads/cloonix-__LAST__/bulk>`_

The containers have file-systems that are .img files, you can download
guest containers here:

    * `Img guests <http://clownix.net/downloads/cloonix-__LAST__/bulk>`_


Content
-------
  
.. toctree::
     :numbered:
     :maxdepth: 1
  
     doc/system.rst
     doc/building.rst
     doc/configure.rst
     doc/clients.rst
     doc/guitems.rst
     doc/cisco.rst
     doc/mikrotik.rst
     doc/container.rst
     doc/ovs.rst
