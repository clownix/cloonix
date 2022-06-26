.. image:: /png/cloonix128.png 
   :align: right

=============================
Details on ovs ports creation
=============================

VM and containers have connections made with the help of the ovs software
(openvswitch) a VM or container interface can be **s**, **d** or **v**,
s is for spyable with wireshark, d is for dpdk and v is for vhost.
The container only accepts vhost interfaces.

For a VM, a spyable interface can launch a wireshark on this interface
with a double-click on the interface.

The guitems a2b, d2d, tap and phy all have spyable type interfaces pre-configured,
a double-click on those should call the wireshark.

If you want the equivalent of a tap but much faster, the vhost configuration
creates a vhost interface on your host, this interface can be used as tap.
The naming of the vhost interface is for a qemu-kvm:
**vho_<cloonix_rank>_<vm_idx>_<eth_num>**
vho_1_1_0 for the first vm created in nemo for eth0 of the vm.

and for a container:
**vgt_<cloonix_rank>_<vm_idx>_<eth_num>**
vgt_1_2_0 for the second creation in nemo for eth0.

The vm_idx is the rank of creation in cloonix of either a vm or a container.

When a lan is created, its name is prefixed with _b_, for example for lan01
this gives in the host bridge _b_lan01.

