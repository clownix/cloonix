.. image:: /png/smile_cloonix_title.png 
   :align: right

=====================
Cloonix Documentation
=====================

:Version: |version|
:Date: |today|

This is the documentation for the cloonix open source hosted at

    * http://clownix.net

Cloonix is a AGPLv3 set of C software elements having the purpose of gluing
together several big names in open source softwares making a single coherent
tool that helps in the creation of virtual networks.

The external big name software used are:**qemu-kvm**, **openvswitch**,
**spice**, **crun**, **podman**, **docker**, **wireshark**, and **openssh**.

Its first goal is an easy usage of the emulated network linking virtual
machines and containers. The network links are based on an openvswitch
instance running inside a private net namespace.

The items of the emulated network are handled either with a command line
interface or with the help of a graphical canvas upon which software elements
are represented through schematic visual items.

Developper view for cloonix
===========================

The cloonix software is a part server and a part client(s).

The server is launched with the **cloonix_net** command with a network name
as parameter, the network name being one of the predefined names found in
cloonix_config. The main process of the server listens to clients and obeys
their commands, it also sends back the topology description.
The connection to the server is a tcp stream protected by password.

All cloonix clients are named **cloonix_xxx** xxx can be one of the
following: gui, cli, ssh, scp, ice, osh, ocp, ovs.
The main one **cloonix_gui** displays the topologie on a canvas also giving
a possible interaction between the user and items on the canvas to send
commands.


User view for cloonix
=====================

As a user, the cloonix software is a set of manageable plugable **items**,
each having a simple graphical representation in the **cloonix_gui** canvas.
Items are plugged together by connection **lines** created by the user.

The **items** can be divided into two categories: those that include
**endpoints**, an endpoint being the terminal point of one single **line**
and the **crossroads** (the only crossroad item is the **lan**) which can
support any number of lines to any number of endpoints.

An item that is in the **endpoints** categorie contains either a single
endpoint for example in the case of a **tap** item or, contains multiple
endpoints for example in the case of a virtual machine **kvm** within which
there are several ethernet interfaces, in that case, each of those interfaces
is one endpoint.

In the **crossroads** category there is only the **lan** item. This is realy
an ovs bridge (also named a switch) and the **endpoints** connected to it
by **lines** are really ports that are attached to the ovs bridge represented
by the lan.

The user creates the items crossroads and endpoints then creates lines between
the endpoints of the items that plug to lans, he can in this fashion make a
network of virtual machines and visualize the resulting topology on the canvas.

From the user view, the most important item in cloonix is the machine
representation, it is implemented either through a **kvm** virtual machine
or a **cnt** (crun, podman or docker) container.
The representation of a machine is a big circle that has satellite smaller
circles with numbers in them. The small circles are the endpoints, they are
graphical representations of the ethernet interfaces for the machine.

These interfaces are the plugable parts from which a line to a **lan** can be
connected. When a line is connected to a lan, the endpoint becomes a **port**
in the lan bridge and the packets tx from this interface goes into the ovs
bridge an accross the crossroad to other endpoints.

From the user view, the second most important item is the **lan** which
should have been called a switch instead because it is the representation
of an openvswitch bridge, it connects all the lines that are attached to it.
in the lan bridge, it acts exactly like an harware switch.

Then there are several secondary items that can be put on the canvas:
**nat**, **tap**, **c2c**, **a2b**.
All those items have a specific graphical representation and are described
latter within this documentation.

To describe the user interaction with the canvas items of cloonix_gui,
therafter are 4 examples of gui interaction:

A double-click on a lan followed by a simple-click on an endpoint creates
the line between endpoint and lan.

A double-click on a blue virtual machine kvm or container cnt creates a
root shell.

A double-click on a virtual machine interface (small circle bordering the vm)
opens a **wireshark** spy that displays the packets running through this
interface.

A right-click when above the **kvm** virtual machine launches a **spice**
graphical desktops.


Usages for cloonix
==================

As all cloonix topologies can be created through commands put inside a
bash script, you can save and reproduce cloonix topologies. Thus the
use of cloonix can be for:

  * **replayable demontration** of network-related softwares,
  * **anti-regression** tests of network-related softwares,
  * **experiments** on network-related softwares.


You can grab a single file html of this doc to print it:

   * `Single html file cloonix doc <../singlehtml/index.html>`_


Content
=======
  
.. toctree::
     :numbered:
     :maxdepth: 1
  
     doc/system.rst
     doc/building.rst
     doc/configure.rst
     doc/clients.rst
     doc/items.rst
     doc/cisco.rst
     doc/ovs.rst
