.. image:: /png/cloonix128.png 
   :align: right


===============
Cloonix clients
===============

You have seen in the above chapter some of the cloonix clients to use
to control the cloonix server, here is a full list the clients:

  * *cloonix_gui*  Graphical interface.
  * *cloonix_cli*  Command line interface.
  * *cloonix_ssh*  Pseudo-ssh for virtual machine using backdoor.
  * *cloonix_scp*  Pseudo-scp for virtual machine using backdoor.
  * *cloonix_dta*  Pseudo-console ttyS0 for virtual machine.
  * *cloonix_ice*  Spice desktop command line launcher.
  * *cloonix_mon*  Qemu monitor terminal.
  * *cloonix_osh*  Pseudo-ssh for vm with no backdoor, uses the nat.
  * *cloonix_ocp*  Pseudo-scp for vm with no backdoor, uses the nat.
  * *cloonix_ovs*  Ovs command line for the embedded openvswitch.
  * *cloonix_xwy*  Distant server cloonix command, supports X11.


cloonix_gui
===========

This is a gtk3 gui software managing a single cloonix network.
The menus for graph are all contextual, triggered by a right-click while on
the cloonix object. The main menu is on the canvas.

The choice of a cloonix guitem triggers its creation on the server and its
representation appears on the canvas.

In the particular case of a **lan**, a double-click on it creates its
selection to create a connection with another guitem. The selected lan has
a purple color as long as it is selected, a single click on an interface
of another guitem finishes the connection.


cloonix_cli
===========

Hit *cloonix_cli* and return, you can see the servers that can be joined and
all the possible server names in config, notice that you must have launched
nemo to have the menu under (do *cloonix_net nemo* to launch nemo)::

    sources$ cloonix_cli nemo
    
    |-------------------------------------------------------------------------|
    | cloonix_cli nemo Version:24-00                                          |
    |-------------------------------------------------------------------------|
    |    snf             : Add wireshark capability on item                   |
    |    kil             : Destroys all objects, cleans and kills switch      |
    |    rma             : Destroys all cloon objects and graphs              |
    |    dcf             : Dump config                                        |
    |    dmp             : Dump topo                                          |
    |    lst             : List commands to replay topo                       |
    |    lay             : List topo layout                                   |
    |    add             : Add one cloon object to topo                       |
    |    del             : Del one cloon object from topo                     |
    |    sav             : Save in qcow2                                      |
    |    cnf             : Configure a cloon object                           |
    |    sub             : Subscribe to stats counters                        |
    |    hop             : dump 1 hop debug                                   |
    |    pid             : dump pids of processes                             |
    |    evt             : prints events                                      |
    |    sys             : prints system stats                                |
    |-------------------------------------------------------------------------|

Then hit *cloonix_cli nemo add* to have the next possible choices::

    sources$ cloonix_cli nemo add
    
    |-------------------------------------------------------------------------|
    | cloonix_cli nemo Version:24-00add                                       |
    |-------------------------------------------------------------------------|
    |    lan             : Add lan (emulated cable)                           |
    |    kvm             : Add kvm (virtualized machine)                      |
    |    phy             : Add phy (host network interface)                   |
    |    cnt             : Add container                                      |
    |    tap             : Add tap (host network interface)                   |
    |    nat             : Add nat (access host ip)                           |
    |    c2c             : Add c2c (cloon 2 cloon cable)                      |
    |-------------------------------------------------------------------------|

  Then hit *cloonix_cli nemo add kvm*, you get the help for this command, the
  add kvm is the most complex command for the cloonix_cli.



cloonix_ssh
===========

The vms launched with cloonix permit the use of a "pseudo-ssh" that does not
use the guest ip interfaces to enter but a virtual serial port and a software
derived from dropbear which is a small ssh server/client used in openwrt here
is the command::
  
    cloonix_ssh <net_name> <vm_name>
  
This special access to guest has root priviledges on the guest and no
password.

Such access is very usefull to test network solutions on guests without
any unwanted network debug access on the device under test.

This tool is a way to launch root commands into a kvm or crun guest.
The way to use it is similar to the ssh::
   
    cloonix_ssh nemo vm_name
    or
    cloonix_ssh nemo vm_name "shell command"

The first command gives a shell, the second executes the shell command
inside the kvm.
cloonix_ssh carries the graphical X applications.


cloonix_scp
===========

This is based on the same binaries as the cloonix_ssh, it is the equivalent of
scp, example of use::

    cloonix_scp <net_name>  <vm_name>:/root/* $HOME
    cloonix_scp nemo -r dir vm_name:/root
    cloonix_scp nemo vm_name:/root/file /home/user



cloonix_osh
===========

The cisco and the mikrotik virtual machines cannot run an agent to have the
cloonix_ssh backdoor, for these type of machines, if you add at vm creation
the options::

    --nobackdoor --natplug=0

Then you can use the commands that replace cloonix_ssh and cloonix_scp::

    cloonix_osh nemo ciscovm
    cloonix_ocp nemo config.cfg ciscovm:running-config

Look at the cisco or the mikrotik for example of use.

Note that the user "admin" must exist in the guest for this to work.
look for "admin" inside cloonix_osh and change it if you which.


cloonix_ocp
===========

This is based on the same binaries as the cloonix_osh, it is the equivalent
of scp, example of use::

    cloonix_ocp nemo <file> cisco1:running-config
    cloonix_ocp nemo cisco1:running-config <dir>


cloonix_mon
===========

This gives access to qemu monitor console to send qemu commands, avoid
using it since quit from this monitor kills the vm, Ctrl-C and return
to quit.

The qemu monitor cmd line is also accessible from gui with the
qemu monitor choice or with::
  
    cloonix_mon <net_name> <vm_name>
  
Use Ctrl-C followed by return to exit the monitor.


cloonix_dta
===========

This gives access to the serial interface, this gives a terminal to the
pseudo ttyS0 of the virtual machine.
The emulated ttyS0 for the vm are accessible either from gui with the
*dtach* label or with::
  
    cloonix_dta <net_name> <vm_name>
  
BEWARE that a Ctrl-C from this console kills the vm! To get out of
this console, do: Ctrl-\ you can also close the terminal window to
get out.


cloonix_ice
===========

This gives access to the spice desktop for the vm.
The spice desktop is compiled with the cloonix tool and can be launched
either from gui (right click when above the vm and select spice) or with
cmd line::
  
    cloonix_ice <net_name> <vm_name>
  

cloonix_xwy
===========

This gives a console to the distant cloonix server like ssh does, it
carries the X11 if you wish to launch a distant X software.

Cloonix has a direct command path to the server that can launch X11-based
applications at a distance, cloonix_xwy is a for distant access shell::

    cloonix_xwy nemo




cloonix_ovs
===========
   
This is associated to dpdk/ovs. This is a wrapper around ovs-vsctl or
ovs-appctl to dump openvswitch data.

Cloonix uses an embedded version of openvswitch, you can have access to
this openvswitch through the cloonix_ovs command, for example, you can
test::

    cloonix_ovs nemo vsctl show
    cloonix_ovs nemo appctl vlog/list

