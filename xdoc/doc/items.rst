.. image:: /png/cloonix128.png 
   :align: right



=====================
Cloonix items managed
=====================


Crossroad item
==============

* **lan** switch used to connect all other cloonix objects together.

This is the heart of cloonix, a lan is an openvswitch bridge, it connects
all the endpoints of items together.
The lan can be connected to **spyable** or not spyable **vhost** interface.



Endpoint items
==============

* **kvm** guest virtual machine launched through qemu-kvm.
* **cnt** guest container (crun, docker or podman).
* **nat** used to reach the internet dhcp, routing and dns server.
* **tap** used to reach a vhost tap of the host machine.
* **c2c** used to reach another cloonix server through udp messages.
* **a2b** used to control the traffic flow (loss, delay and shaping).
* **phy** used to bring a host physical interface into cloonix.


cnt
---
This is a running instance of a container, in the case of crun, it is mainly
a running process inside a private file-system with a private namespace ip
stack. For the docker and podman cases, the docker or podman software is used
for the running of the containers but cloonix manages their interfaces
endpoints.


kvm
---
This is a running instance of a qemu virtual machine, most of the launching
options are fixed within cloonix.
There are a few cloonix configuration parameters that are necessary, such
as cpu number, ram quantity, eth types spyable or not.


tap
---
The real host's ip stack can be reached through a **tap**. A tap is created
with a veth pair built the following way::

   ip link add name nemotap1 type veth peer _k_2_0 
   ip link set _k_2_0 netns cloonix_nemo

The first line creates a veth pair, the main name nemotap1 stays inside the
main namespace, this way ifconfig shows the new nemotap1 interface. Then the
second command puts the peer port _k_2_0 inside the cloonix_nemo namespace
where the openvswitch runs.


nat
---
Your cloonix network is completely isolated in your host's user space,
to have a connection to the internet outside, **nat** is the way out.

Create a **nat**, connect it to a **lan** with a double-click on the
lan then a simple-click on the nat, connect this lan to the interface
eth0 of the vm the same way.

Double-click on the vm to get a shell on it, then do *dhclient eth0*

The nat respond to the dhcp request for an ip address. The nat also looks
at the /etc/resolv.conf file of the host during its init and transmits the
dns requests made to it through the 172.17.0.3 to the dns of the host and
transmits back the answer from the outside.
The nat is also the gateway for the guests through the 172.17.0.2 address.
UDP, TCP and ICMP transfers from the nat to the host are supported in nat.
The nat is a way to translate the "cooked" (layer 3 or 4) udp tcp icmp from
the host user world to "raw" packets (lowest layer with mac, syn, synack...)
that can enter the vm or container ip stack. (And inversly from vm to host).

Thus the nat permits web browsing from the guest desktop, and you should be
able to ping google.com if your host has an internet connection.


c2c
---
A cloonix server can be considered as a logical group or a cluster of
virtual guest machines.

To have big networks, links cloonix to cloonix must be used.
The object to use for this link is called c2c.
A c2c go in pairs master/slave, the master is given the address
of the distant cloonix.

The master initialises and monitors the connection to the slave.
As long as the master exists, it tries to connect to the slave.


a2b
---
Configurable to create shaping or delay or loss in the packets going
from A to B and same for packets going from B to A.

phy
---
Used to bring a host physical ethernet interface into the cloonix
namespace, the item when within the cloonix namespace can be connected
to a lan and through it to another cloonix item.
Beware: The phy item in cloonix represents real interfaces of the host
machine, the command to use this phy item in cloonix makes an interface dive
into the cloonix net namespace.
As a consequence this command takes the interface out from the list of
host interfaces.

For example, in my host there is an interface named enp6s0, the command:
*"cloonix_cli nemo add phy enp6s0"* takes out that interface from the host
default namespace and puts it in the "cloonix_nemo" namespace.
This interface is visible in its new namespace with the command:
*"sudo ip netns exec cloonix_nemo ip link"*.


