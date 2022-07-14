.. image:: /png/cloonix128.png 
   :align: right



===============
Cloonix guitems
===============

Here is the list of all the cloonix objects working for with dpdk:

* **kvm** guest virtual machine launched through qemu-kvm.
* **lan** hub used to connect all other cloonix objects together.
* **nat** used to reach the internet, (slirp is its old name).
* **tap** used to reach a tap of the host machine.
* **c2c** used to reach another cloonix server through udp messages.

New:

* **cnt** uses crun to run a container, (a very light vm).

cnt
===

This is a running instance of a container, it is mainly a running process
inside a private file-system with a private namespace ip stack.
The the connections for the ethernets within the container are possible
only with **vhost** type of openvswitch lan connection.
See container chapter to have info on the technics involved for this
new guitem.

kvm
===

This is a running instance of a qemu virtual machine, most of the launching
options are fixed within cloonix.

There are a few cloonix configuration parameters that are necessary, such
as cpu number, ram quantity, eth types **spyable**, **dpdk**, or **vhost**.

For details on the 3 types of connections, see later in this doc.


lan
===

This is the heart of cloonix, it connects all the other virtual network
guitems together.

The lan is generic, the same lan can be connected to **spyable**, to a
**dpdk** or to a **vhost** interface.


tap
===

The real host's ip stack can be reached through a **tap**. A tap is a
**spyable** interface, there is no choice for this.

In case you want a very high bandwidth between a tap and a vm, create a vhost
interface for this vm and use the vhost interface created for this interface.
The vhost interface has a name begining with "vho".  

This must be plugged to a **spyable** interface on the vm side.

phy
===

This is an ethernet physical interface of the host machine. A phy has a fixed
name given by the host, it is a **spyable** interface.

In the futur, there will probably be a faster non-spyable way to plug a
physical interface to a vhost, this should give higher bandwidth.

This must be plugged to a **spyable** interface on the vm side.

nat
===
 
Your cloonix network is completely isolated in your host's user space,
to have a connection to the internet outside, **nat** is the way out.

Create a **nat**, connect it to a **lan** with a double-click on the
lan then a simple-click on the nat, connect this lan to the interface
eth0 of the vm the same way.

Double-click on the vm to get a shell on it, then do *dhclient eth0*

A nat permits web browsing from the guest desktop, when connected to a
VM, call dhclient from within the guest to the interface where the nat is
connected. Then you should be able to ping google.com if your host has
an internet connection.

The nat is a way to translate the "cooked" (layer 3 or 4) udp tcp icmp from
the user world to "raw" (lowest layer with mac, syn, synack...).

You should be able to wget stuff from the web, or browse it, the nat
translates the udp/tcp protocols from the emulated in-cloonix lan cable
to udp/tcp socket going outside the host.

UDP, TCP and ICMP are supported in nat.

This must be plugged to a **spyable** interface on the vm side.
 

d2d
===

A cloonix server can be considered as a logical group or a cluster of
virtual guest machines.

To have big networks, links cloonix to cloonix must be used.
The object to use for this link is called d2d.
A d2d go in pairs master/slave, the master is given the address
of the distant cloonix.

The master initialises and monitors the connection to the slave.
As long as the master exists, it tries to connect to the slave.

This must be plugged to a **spyable** interface on the vm side.

a2b
===

Configurable to create shaping or delay or loss in the packets going
from A to B and same for packets going from B to A.

This must be plugged to a **spyable** interface on the vm side.

