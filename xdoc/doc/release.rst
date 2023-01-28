.. image:: /png/cloonix128.png 
   :align: right

============================
Software Release Information
============================

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

The customer launch is now an option::

    --customer_launch="<script with params>"

Tor the help in docker/podman container creation, do ::

  cloonix_cli nemo add doc
