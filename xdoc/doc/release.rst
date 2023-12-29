.. image:: /png/cloonix128.png 
   :align: right

============================
Software Release Information
============================

v34-00
======

In this version, the zip containers can be made by service instead
of by distro. See at quickstart/cnt_create the way to create some
of the service-oriented zip containers.
For the startup of the containers, I use the --startup_env="NODE_ID=x" to
differentiate nodes coming from the same container, inside the container
I usualy add a monit service that uses the NODE_ID env variable to
configure the node.


v33-00
======

For this version ZIP files for the crun replace EXT4 files for the
file-systems.

The phy ethernet interface of the host can be used in cloonix
without total disapearance from the host, thanks to the macvlan
type.

The client gui is locked in a non-root envirronment, if you need
the spice usb absorption in the vm desktop, you must use spice
through cloonix_ice and not with a right click on the cloonix_gui.

Docker is not supported anymore, it was redondant with podman
which I personaly prefer..


v30-00 and v30-01
=================

Note that there is no v29, the v30 has a very big change in its delivery
method. Earlier version were given source only with the constraint of
compilation by the user.
The compilation was a drawback to use cloonix because the target host had
to be a developer distribution.
This version has binary delivery, the binaries included are all self-contained,
these binaries do not open any file comming from the host, all is included
within the bundle. This makes the binary compatible with any distribution.

Also, the locations of the binaries and qcow data have changed, it is now
*/usr/libexec/cloonix* and */var/lib/cloonix* and of course, the handle
scripts at */usr/bin/cloonix_xxx*.

If you have an old version of cloonix, then you must erase it with:
*rm -rf /usr/local/bin/cloonix*. If you do not, the old version will
probably take precedence on the new version because the path has the
/usr/local/bin before /usr/bin. 

v28-00
======

The **phy** item has been added, this permits to act on the real physical 
interface of the host.
When you add a phy interface to the canvas, this interface disapears from
the list of interfaces of the host and appears in the namespace of the
cloonix network.

For example on my host I have an unsused physical interface named enp6s0,
if I use the phy item to use this interface within the cloonix net nemo with
the following command::

    cloonix_cli nemo add phy enp6s0

Then the enp6s0 interface disapears from the host ifconfig list, but it is not
lost for every namespace, it can be visualised in the cloonix_nemo
namespace through the following command::

    ip netns exec cloonix_nemo ip address

Then you can link this new interface to a lan of cloonix and through this
lan to amy other item of cloonix.

Beware that cloonix does not check that the interface is not used, if it is
then you will lose the connectivity from this interface.




v27-02
======

Docker and Podman have integrated into their images ENTRYPOINT and CMD
which are predefined startup commands that must be taken into account
when cloonix starts the container.

In order to achieve this goal, cloonix requests the ENTRYPOINT and CMD
through the commands::

    docker/podman inspect -f '{{.Config.Entrypoint}}' <image_id>
    docker/podman inspect -f '{{.Config.Cmd}}' <image_id>

And if lines returned show an empty startup command, then cloonix puts::

    sleep 7777d

as new entrypoint command to have a remanent container even if no input command
is setup.

The version v27-02 also adds the possibility to give some environment variables
at container startup, this is done with the option::

    --startup_env="<env_name=env_val env2_name=env2_val...>"

Tor the help in docker/podman container creation, do ::

  cloonix_cli nemo add doc
