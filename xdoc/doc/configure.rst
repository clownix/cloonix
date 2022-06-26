.. image:: /png/cloonix128.png 
   :align: right

===================
Cloonix config file
===================

A physical machine can launch several cloonix servers identified by
name, ip and port.

The server and the client share the same configuration file called
cloonix_config, this file gives the necessary info to both clients
and servers.

The config file is installed at: **/usr/local/bin/cloonix/cloonix_config**.
In this file, there are informations on cloonix installation version and
paths::

    CLOONIX_VERSION=__LAST__
    CLOONIX_TREE=/usr/local/bin/cloonix
    CLOONIX_WORK=${HOME}/cloonix_data
    CLOONIX_BULK=${HOME}/cloonix_data/bulk

The cloonix_config contains after the paths, all the cloonix server and
client net name, ports and password::

    CLOONIX_NET: nemo {
      cloonix_ip       127.0.0.1
      cloonix_port     43211
      cloonix_passwd   nemoclown
      d2d_udp_ip       127.0.0.1
    }

* nemo: name used to reference the server.
* cloonix_ip: used by the client to access the server.
* cloonix_port: the server listens to it and the client connects to it.
* cloonix_passwd: used by both client and server to authenticate each other.
  The password is used to produce a sha256 hmac 32 bytes long based on the
  data payload. A connection with a wrong hmac digest is closed.
* d2d_udp_ip: for the d2d connection, udp ports are negociated on the
  tcp connection between cloonix, ip address used by distant cloonix
  to reach local nemo cloonix will be d2d_udp_ip.

The user must follow the syntax in this template to add or change configured
server name and addresses::

    CLOONIX_NET: <name> {
      cloonix_ip       <ip_number>
      cloonix_port     <port_number>
      cloonix_passwd   <password>
      d2d_udp_ip       <ip_number>
    }


Working dir
===========

CLOONIX_WORK=${HOME}/cloonix_data is the place where the server will write
all that is needed to run.
It contains all the unix socket paths and the qcow2 files derived from the
bulk qcow2 images.
All the paths used by dpdk to share the hudge pages memory is here too.


Usages for cloonix
==================

  * **replayable demontration** of network-related softwares,
  * **anti-regression** tests of network-related softwares,
  * **experiments** on network-related softwares.


