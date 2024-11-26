.. image:: /png/cloonix128.png 
   :align: right


=======================
Cloonix client binaries
=======================

You have seen in the above chapter some of the cloonix clients to use
to control the cloonix server, here is a full list the clients:

  * *cloonix_gui*  Graphical interface.
  * *cloonix_cli*  Command line interface.
  * *cloonix_ssh*  Pseudo-ssh for vm/container using qemu_guest_agent.
  * *cloonix_scp*  Pseudo-scp for vm/container using qemu_guest_agent.
  * *cloonix_osh*  Pseudo-ssh for vm without qemu_guest_agent (for cisco c8000).
  * *cloonix_shk*  Console for kvm to use if cloonix_ssh fails.
  * *cloonix_shc*  Console for crun to use if cloonix_ssh fails.
  * *cloonix_ocp*  Pseudo-ssh for vm without qemu_guest_agent (for cisco c8000).
  * *cloonix_ice*  Spice desktop command line launcher.
  * *cloonix_ovs*  Ovs command line for the embedded openvswitch.
  * *cloonix_wsk*  Command line to launch wireshark on a cloonix endpoint.
  * *cloonix_dsh*  Distant bash on server with particular env.


cloonix_gui
===========

This is a gtk3 gui software managing a single cloonix network.
The menus for graph are all contextual, triggered by a right-click while on
the cloonix object. The main menu is on the canvas.

The choice of a cloonix item triggers its creation on the server and its
representation appears on the canvas.

In the particular case of a **lan**, a double-click on it creates its
selection to create a line connection with another item. The selected lan
has a purple color as long as it is selected, a single click on an endpoint
finishes the connection by creating the line between the 2 elements.

A complete connection connects two or more endpoints through a lan crossroad.
The endpoints can be interfaces of vm/container or an interface of an a2b item
or a tap, a nat or c2c.

Note the redirection of usb does not work in the spice desktop availlable
with the cloonix_gui.

Use the cloonix_ice to have the usb redirection possible.


cloonix_cli
===========

Hit *cloonix_cli* and return, you can see the servers that can be joined and
all the possible server names in config, notice that you must have launched
nemo to have the menu under (do *cloonix_net nemo* to launch nemo)::

    sources$ cloonix_cli nemo help
    
    |-------------------------------------------------------------------------|
    | cloonix_cli nemo Version:33-00                                          |
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
    | cloonix_cli nemo Version:33-00add                                       |
    |-------------------------------------------------------------------------|
    |    lan             : Add lan (emulated cable)                           |
    |    kvm             : Add kvm (virtualized machine)                      |
    |    cru             : Add container crun                                 |
    |    pod             : Add container podman                               |
    |    tap             : Add tap (host network interface)                   |
    |    phy             : Add phy (real host network interface)              |
    |    nat             : Add nat (access host ip)                           |
    |    a2b             : Add a2b (traffic delay shaping)                    |
    |    c2c             : Add c2c (cloonix 2 cloonix cable)                  |
    |-------------------------------------------------------------------------|


cloonix_ssh
===========

The kvm/container(s) launched with cloonix permit the use of a "pseudo-ssh"
that does not use the guest ip interfaces to enter but a virtual serial port
in case of kvm and a mount in case of container. In both cases, a unix socket
and a software derived from dropbear (a small ssh client/server) are used
to emulate an ssh without using any of the guests' interfaces.
Here is the command::
  
    cloonix_ssh <net_name> <vm_name>
  
This special access to guest has root priviledges on the guest and no
password.

Such access is very usefull to test network solutions on guests without
any unwanted network debug access on the device under test.
Example of use::

    cloonix_ssh nemo vm_name
    or
    cloonix_ssh nemo vm_name "shell command"

The first command gives a shell, the second executes the shell command
inside the kvm.
cloonix_ssh carries the graphical X applications without added option.

Notes:

For kvm virtual machine, in the guest there has to be an active service:
qemu-guest-agent.service to have this service on qemu kvm.

The color blue arrives when the cloonix-agent is operational and that
color indicates that the cloonix_ssh will work.


For crun container, if cloonix_ssh fails, you can have a console with::

    cloonix_shc <net_name> <vm_name>

For kvm virtual machine, if cloonix_ssh fails, you can have a console with::

    cloonix_shk <net_name> <vm_name>



cloonix_scp
===========

This is based on the same binaries as the cloonix_ssh, it is the equivalent of
scp, example of use::

    cloonix_scp <net_name>  <vm_name>:/root/* $HOME
    cloonix_scp nemo -r dir vm_name:/root
    cloonix_scp nemo vm_name:/root/file /home/user

Notes:

Same as for cloonix_ssh.


cloonix_osh
===========

The cisco and the mikrotik virtual machines cannot run an agent to have the
cloonix_ssh backdoor, for these type of machines, if you add at vm creation
the options::

    --no_qemu_ga --natplug=0

Then you can use the commands that replace cloonix_ssh and cloonix_scp::

    cloonix_osh nemo ciscovm
    cloonix_ocp nemo config.cfg ciscovm:running-config

Look at the cisco directory for example of use.

Notes: 

The user "admin" must exist in the guest for this to work.
look for "admin" inside cloonix_osh and change it if you which.

The kvm machines where cloonix_osh is used will keep the red color indicating
that the cloonix-agent is not operationnal.


cloonix_ocp
===========

This is based on the same binaries as the cloonix_osh, it is the equivalent
of scp, example of use::

    cloonix_ocp nemo <file> cisco1:running-config
    cloonix_ocp nemo cisco1:running-config <dir>


cloonix_ice
===========

This gives access to the spice desktop for the vm.
The spice desktop is compiled with the cloonix tool and can be launched
either from gui (right click when above the vm and select spice) or with
cmd line::
  
    cloonix_ice <net_name> <vm_name>
  
With this call for the spice desktop, the **usb redirection is possible**.


cloonix_ovs
===========
   
This is associated to ovs. This is a wrapper around ovs-vsctl to dump
openvswitch data.

Cloonix uses an embedded version of openvswitch, you can have access to
this openvswitch through the cloonix_ovs command, for example, you can
test::

    cloonix_ovs nemo --help
    cloonix_ovs nemo show


cloonix_wsk
===========

Cloonix uses an embedded version of wireshark, you can have access to
this wireshark through the cloonix_wsk command, for example, if you
have nemo running with a vm named Cloon1 in it, you can test::

    cloonix_wsk nemo Cloon1 0


