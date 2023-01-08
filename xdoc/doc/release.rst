.. image:: /png/cloonix128.png 
   :align: right

============================
Software Release Information
============================

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
