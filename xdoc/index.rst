.. image:: /png/smile_cloonix_title.png 
   :align: right

=====================
Cloonix Documentation
=====================

:Version: |version|
:Date: |today|

This is the documentation for the cloonix open source hosted at

    * http://clownix.net

Cloonix is an AGPLv3 suite of C software components designed to seamlessly 
integrate various well-known open-source software, creating a unified and 
cohesive tool that facilitates the creation of virtual networks.

The external software components include **qemu-kvm**, **openvswitch**,
**spice**, **crun**, **wireshark** and **podman**.

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
*/usr/libexec/cloonix/common/etc/cloonix.cfg*.
The */usr/bin/cloonix_net* is a bash script that calls binaries and libraries
located in */usr/libexec/cloonix/common* and */usr/libexec/cloonix/server*.
The main process of the server listens to clients and obeys their commands,
it also sends back the topology description.
The connection to the server is a tcp stream protected by password.

All cloonix clients are named **cloonix_xxx** xxx can be one of the
following: cli, gui, ice, ssh, scp, wsk, ovs, osh, ocp.
The */usr/bin/cloonix_xxx* are bash scripts that call binaries and libraries
located in */usr/libexec/cloonix/common*.

The main one **cloonix_gui** displays the topologie on a canvas also giving
a possible interaction between the user and items on the canvas to send
commands.

The working directories for the software is located at:
*/var/lib/cloonix/<net-name>*

The storage directory for the virtual machines is located at:
*/var/lib/cloonix/bulk*


User view for cloonix
=====================

As a user, the cloonix software is a set of manageable plugable **items**,
each having a simple graphical representation in the **cloonix_gui** canvas.
Items are plugged together by connection **lines** created by the user.
Those lines emulate ethernet wires.

The **items** can be divided into two categories: those that include
**endpoints**, an endpoint being the terminal point of one single **line**
and the **crossroads** (the only crossroad item is the **lan**) which can
support any number of lines to any number of endpoints.
Those crossroads emulate ethernet hubs, those are realy ovs bridges.

An item that is in the **endpoints** categorie contains either a single
endpoint for example in the case of a **tap** item or, contains multiple
endpoints for example in the case of a virtual machine **kvm** within which
there are several ethernet interfaces, in that case, each of those interfaces
is one endpoint.

The user creates lines, crossroads and endpoints, he can in this fashion make a
network of virtual machines and visualize the resulting topology on the canvas.

From the user view, the most important item in cloonix is the machine
representation, it is implemented either through a **kvm** virtual machine
or a **cnt** (crun or podman) container.
The representation of a machine is a big circle that has satellite smaller
circles with numbers in them. The small circles are the endpoints, they are
graphical representations of the ethernet interfaces for the machine.

To describe the user interaction with the canvas items of cloonix_gui,
therafter are 4 examples of gui interaction:

A double-click on a lan followed by a simple-click on an endpoint creates
the line between endpoint and lan.

A double-click on a blue virtual machine kvm or container cnt creates a root
shell in a urxvt screen.

A double-click on a virtual machine interface (small circle bordering the vm)
opens a **wireshark** spy that displays the packets running through this interface.

A right-click when above the **kvm** virtual machine permits launching a
**spice** graphical desktops.


Directories cloonix uses
========================

*/usr/libexec/cloonix/* for binaries, libraries and configuration files.
*/var/lib/cloonix/bulk* for the cloonix storage of virtual files .qcow2 and .zip.
*/var/lib/cloonix/<net-name>* for run-time storage of temporary files.

You can grab a single file html of this doc to print it:

   * `Single html file cloonix doc <../singlehtml/index.html>`_


Content
=======
  
.. toctree::
     :numbered:
     :maxdepth: 1
  
     doc/release.rst
     doc/system.rst
     doc/installation.rst
     doc/building.rst
     doc/configure.rst
     doc/clients.rst
     doc/items.rst
     doc/cisco.rst
     doc/podman.rst
     doc/ovs.rst
