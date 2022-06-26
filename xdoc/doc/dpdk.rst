.. image:: /png/cloonix128.png 
   :align: right

============
DPDK/OVS LAN
============

  The lan in cloonix is based on dpdk and openvswitch, there are 3 possible
  types of lan available in cloonix: the green one (s) is spyable, the grey
  one (d) is dpdk not spyable but faster and the cyan one (v) is vhost.

  On the following image, lan0 is the s type, lan1 the d type and lan2 is
  the v type. In the picture, the 3 lans are configured as seen in the white
  terminal: the green eth0 has ip 1.1.1.1, the grey has ip 2.1.1.1 and the
  cyan has 3.1.1.1, the iperf shows that the spyable lan is slower than
  both d and v interfaces.

  .. image:: /png/vm_iperf_v19.png

  At the quemu level, for the guest, the driver seen for the inside guest kernel
  is the virtio-net driver on the frontend for all 3 types s,d or v.
  The backend inside quemu is a vhost-user in the d or s cases and a vhost for
  the v type.
  The spyable type has a cloonix component added, the xyx_dpdk that is used
  for the packet sensor blinking of the interfaces ans the wireshark spy::
 
          spyable                     dpdk                           vhost
      +---------------+           +---------------+            +---------------+
      |    iperf3 TX  |           |    iperf3 TX  |            |    iperf3 TX  |
      +---------------+           +---------------+            +---------------+
      | Guest Kernel  |           | Guest Kernel  |            | Guest Kernel  |
      +---------------+           +---------------+            +---------------+
      |   virtio net  |           |    virtio net |            |   virtio net  |
      |   qemu_kvm    |           |    qemu_kvm   |            |   qemu_kvm    |
      | dpdkvhostuser |           | dpdkvhostuser |            |     vhost     |
      +-------+-------+           +-------+-------+            +-------+-------+ 
              |                           |                            | 
              v                           v                            v
      +-------+-------+           +-------+-------+            +-------+-------+
      |   OVS BRIDGE  |           |   OVS BRIDGE  |            |   OVS BRIDGE  |
      +-------+-------+           +-------+-------+            +-------+-------+
              |                           |                            | 
              v                           v                            v
      +-------+-------+           +-------+-------+            +-------+-------+
      |net_virtio_user|           | dpdkvhostuser |            |     vhost     |
      +---------------|           |   qemu_kvm    |            |   qemu_kvm    |
      |  xyx_dpdk spy |           |   virtio net  |            |   virtio net  |
      +---------------+           +---------------+            +---------------+
      |net_virtio_user|           | Guest Kernel  |            | Guest Kernel  |
      +-------+-------+           +---------------+            +---------------+
              |                   |    iperf3 RX  |            |    iperf3 RX  |
              v                   +---------------+            +---------------+
      +-------+-------+
      |   OVS BRIDGE  |                                 Note that with the vhost
      +-------+-------+                                 comes a tap named:
              |                                                
              v                                         vho_<idxnet>_<idxvm>_<ifnum>
      +-------+-------+
      |net_virtio_user|                                 To have a fast tap, use it.
      +---------------|
      |  xyx_dpdk spy |
      +---------------+
      |net_virtio_user|
      +-------+-------+
              |
              v
      +-------+-------+
      | dpdkvhostuser |
      |   qemu_kvm    |
      |   virtio net  |
      +---------------+
      | Guest Kernel  |
      +---------------+
      |    iperf3 RX  |
      +---------------+

