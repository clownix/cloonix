.. image:: /png/cloonix128.png 
   :align: right

=============================
Details on ovs ports creation
=============================

VM and containers have connections made with the help of the ovs software
(openvswitch). A VM or container interface is vhost based and can be
**s** or **v** type, s is for spyable with wireshark, and v is not spyable.
The total number of spyable interfaces is limited, v is advised when you
do not wish to inspect transiting packets.

For any item except the lan, a spyable interface can launch a wireshark on
this interface with a double-click on the item or interface of the item.


Ovs port naming convention
==========================

All endpoint items are ports as viewed by openvswitch, the first part of the
naming convention is that kvm virtual machine, cru, doc and pod containers,
tap, nat, a2b and c2c all have a unique number obj_id given at creation in
cloonix. Number given by cfg_alloc_obj_id() function in the code.

For an interface of a vm or container, the number of the interface is appended
to this identifier and to simplify, for tap, nat or c2c, a zero is appended.

The resulted name of a port is: **_k_<obj_id>_<num_eth>**

The openvswitch process runs in the net namespace given as parameter to the
cloonix_name command, while cloonix is running, if you type::

   ip netns list

you should see cloonix_nemo dumped.

Then if you type::

  sudo ip netns exec cloonix_nemo ip addr

Then you shoud see all the interfaces of the cloonix nemo namespace, and
since this network is private, the naming of the items inside this network
is unique even if 2 different cloonix networks are running on the same
server host.

The result of the following command on the host server::

  cloonix_ovs nemo show

should give the same ports as the command above.

The vm_idx is the rank of creation in cloonix of either a vm or a container.

When a lan is created in cloonix, its name is prefixed with _b_, for example
for lan01 this gives in the host bridge _b_lan01.
This bridge is then created within the openvswitch instance attached to the
cloonix network chosen.


As an example, the following lines create a topology with 2 virtual machines
kvm and a lan connected to the interfaces eth0 of both kvm::

    cloonix_cli nemo add kvm Cloon1 ram=2000 cpu=2 eth=sss bookworm.qcow2
    cloonix_cli nemo add kvm Cloon2 ram=2000 cpu=2 eth=sss bookworm.qcow2
    cloonix_cli nemo add lan Cloon2 0 lan01
    cloonix_cli nemo add lan Cloon1 0 lan01


The result of the following command on the host server::

    ip netns list
    cloonix_ovs nemo show

are::

    ~$ ip netns list
    cloonix_nemo

    ~$cloonix_ovs nemo show
    099c0660-6111-4b0f-b3fc-6dae7138bf19
        Bridge _b_lan01
            Port s_k_2_0
                Interface s_k_2_0
            Port _b_lan01
                Interface _b_lan01
                    type: internal
            Port _k_2_0
                Interface _k_2_0
            Port s_k_1_0
                Interface s_k_1_0
            Port _k_1_0
                Interface _k_1_0

Note that in the above dumps, the s_k_2_0 and s_k_1_0 ports are mirrors
to spy on the _k_2_0 and _k_1_0 ports that are the eth0 of Cloon2 and eth0
of Cloon1.
