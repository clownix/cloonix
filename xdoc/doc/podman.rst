.. image:: /png/cloonix128.png 
   :align: right

=================
Cloonix in Podman
=================

To create the podman, first we create a zip container by launching the
file:
**cloonix/quickstart/cnt_create/custom/cloonix/cnt_cloonix.sh**

This script should create cnt_cloonix.qcow2 and cnt_cloonix.zip inside
the /var/lib/cloonix/bulk/ directory.

Then use the script at:
**cloonix/quickstart/cnt_create/podman_transform.sh**

./podman_transform.sh cnt_cloonix.zip

This creates the podman containing the cloonix software.

For the cloonix to be able to use /dev/kvm, /dev/vhost-net and /dev/net/tun,
some commands must be done as root, here is the way to check that your user
can read/write those devices and the way to authorise their use:

getfacl /dev/kvm
getfacl /dev/vhost-net
getfacl /dev/net/tun

sudo setfacl -R -m u:${USER}:rw /dev/kvm
sudo setfacl -R -m u:${USER}:rw /dev/vhost-net
sudo setfacl -R -m u:${USER}:rw /dev/net/tun

Then try the podman, note that the directory /var/lib/cloonix/bulk
of your host is made visible inside the podman and must contain
some qcow2 and zip file-systems::

  ID_IMAGE=$(podman images |grep cnt_cloonix | awk "{print \$3}")

  podman run -it --privileged \
           -e DISPLAY=:0 -v /tmp/.X11-unix:/tmp/.X11-unix \
           -v /var/lib/cloonix/bulk:/var/lib/cloonix/bulk \
           -v /lib/modules:/lib/modules $ID_IMAGE /bin/bash

Then try cloonix from within the podman::

  cloonix_net nemo
  cloonix_gui nemo

