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

The most important external software components include **qemu-kvm**,
**openvswitch**, **spice**, **crun** and **wireshark**.

For the transfer of the gui to a web browser, other software used are:
**nginx**, **noVNC**, **websockify-js**, **xorg**.

For the self-extracting component of cloonix, (file extract_nemo.sh)
makeself.sh is used and good old tmux also.


Cloonix's first goal is an easy usage of the emulated network linking virtual
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
The main process **cloonix-main-server** of the server listens to clients and
obeys their commands, it also sends back the topology description.
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

As a user, the cloonix software is a set of manageable plugable items,
each having a simple graphical representation in the **cloonix_gui** canvas.
An item owns one or more **ports** which are endpoints of the network.

Each port of an item can have one (and one only) connection represented
with a line to a crossroad between **ports**.
The crossroad is called **lan** within cloonix, this naming is not good
since it is really a bridge : Each packet coming from a **port**
goes to all the other ports connected by lines to the crossroad **lan**.

The real world equivalent of a **port** is an hardware interface, for the
line between the **port** and the **lan**, the hardware equivalent is an
ethernet cable and the hardware equivalent of the **lan** is a switch or
a hub, that connects the layer 2 ethernet of the **ports**.
The **lan** in cloonix is implemented with an openvswitch bridge.

Here is the list of items, connectable to each-other through a **lan** ::

  **kvm**: Qemu-kvm driven virtual machine (several **ports**).
  **cnt**: Crun driven container (several **ports**).
  **nat**: This nats the packets to reach the outside ethernet. 
  **tap**: This is a tap interface giving cloonix an interface in the host.
  **c2c**: This connects one cloonix net to another cloonix net.
  **a2b**: This can act on packet transit, drops, shaping or delay.
  **phy**: This item is to insert one of the host's physical interface in cloonix.


The user creates items, lans and lines between them, he can in this fashion
make a network of virtual machines. On the canvas he can visualize the
resulting topology.

The representation of a machine is a big circle for KVM, smaller for CNT,
both have satellites on the periphery that are smaller circles with numbers.

The small circles are the ports, they are green when spyable with wireshark
and light blue when not spyable. They represent the ethernet interfaces.

To describe the user interaction with the canvas items of cloonix_gui,
therafter are 4 examples of gui interaction:

A double-click on a lan followed by a simple-click on an port creates
the line between port and lan.

A double-click on a blue virtual machine kvm or container cnt creates a
graphical xterm root shell.

A double-click on a green port (small circle bordering the kvm or cnt)
opens a **wireshark** spy that displays the packets running through this interface.
Note that the wireshark can only visualize or save a .pcap file, it is not
the complete version of wireshark.

A right-click when above the **kvm** virtual machine launches a **spice**
graphical desktops. No desktop for crun containers.


Directories in which cloonix is installed for classic use
=========================================================

*/usr/libexec/cloonix/* for binaries, libraries and configuration files.
*/var/lib/cloonix/bulk* for the cloonix storage of virtual files .qcow2 and .zip.
*/var/lib/cloonix/<net-name>* for run-time storage of temporary files.


Directory in which cloonix is installed in the self_extracting case
===================================================================

In this case, after the call to **extract_nemo.sh**, a new
directory is present in the current directory, called
**extractnemo**. For this installation you do not need the
root or admin privileges.



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
     doc/ovs.rst
     doc/extracting.rst
