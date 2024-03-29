.. image:: /png/cloonix128.png 
   :align: right

=====================
Cloonix configuration
=====================

A physical machine can launch several cloonix servers identified by
name, ip and port.

The server and the client share the same configuration file called
cloonix.cfg this file gives the necessary info to both clients and servers.

The cloonix config file is installed at::

    **/usr/libexec/cloonix/etc/cloonix.cfg**.

In this file, there are informations on cloonix installation version and
paths::

    CLOONIX_VERSION=__LAST__

The cloonix_config contains after the paths, all the cloonix server and
client net name, ports and password::

    CLOONIX_NET: nemo {
      cloonix_ip       127.0.0.1
      cloonix_port     43211
      cloonix_passwd   nemoclown
      c2c_udp_ip       127.0.0.1
    }

* nemo: name used to reference the server.
* cloonix_ip: used by the client to access the server.
* cloonix_port: the server listens to it and the client connects to it.
* cloonix_passwd: used by both client and server to authenticate each other.
  The password is used to produce a sha256 hmac 32 bytes long based on the
  data payload. A connection with a wrong hmac digest is closed.
* c2c_udp_ip: for the d2d connection, udp ports are negociated on the
  tcp connection between cloonix, ip address used by distant cloonix
  to reach local nemo cloonix will be c2c_udp_ip.

The user must follow the syntax in this template to add or change configured
server name and addresses::

    CLOONIX_NET: <name> {
      cloonix_ip       <ip_number>
      cloonix_port     <port_number>
      cloonix_passwd   <password>
      c2c_udp_ip       <ip_number>
    }



