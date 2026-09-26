.. image:: /svg/title.svg
   :align: center
   :width: 100%

===============================
Cloonix Developer Documentation
===============================

:Version: |version|
:Date: |today|

This book is for the person who changes cloonix: the processes, the sockets
between them, and what each binary of the delivery is for. The companion
book, **Cloonix User Documentation**, is for the person who runs it.


The shape of it
===============

.. image:: /svg/process_tree.svg
   :align: center
   :width: 100%

One net is one process tree rooted at **cloonix-main-server**, the only
process that knows the topology. Four properties hold everywhere:

  * **One event loop, no threads.** Every daemon is built on the loop of
    *lib_io_clownix*; a callback runs to completion; nothing is locked. The
    one exception, a thread per browser connection in the gui server, is
    stated in its chapter.
  * **Every peer is a unix socket** in */var/lib/cloonix/<net>*, except the
    doors port and the web port, which are tcp so that a client on another
    machine reaches them.
  * **One text rpc**, json lines with a *fctident*, spoken by every process.
  * **One privilege boundary**, *cloonix-suid-power*, the single setuid
    binary of the delivery.


How a net starts
================

::

    cloonix start nemo
      -> cloonix-main-server            reads cloonix.cfg, forks
           -> cloonix-suid-power        the privileged half
           -> cloonix-ovs-drv           creates the namespace cloonix_nemo, starts in it
                -> cloonix-ovsdb-server
                -> cloonix-ovs-vswitchd
           -> cloonix-doorways          the hmac and the legs, behind proxy_pmain.sock
           -> cloonix-proxymous         the two tcp ports, tls, passt, the c2c udp relay
           -> cloonix-gui-server        the canvas

A net that is up has all of them; when one dies the main server tears the
net down. A guest adds one **cloonix-qemu-system** or one **cloonix-crun**, started
by suid-power; its interfaces are ports that ovs-drv plugs into the switch.


Where the code is
=================

.. image:: /svg/source_map.svg
   :align: center
   :width: 100%

Under *cloonix/cloonsrc*, one directory per binary, plus::

    lib_io_clownix     the event loop, the sockets, the timers, KERR
    lib_rpc_clownix    the rpc, one pair of halves per message
    glob_include       structures, paths, the wire and cxp definitions

A directory holding a *Makefile* is cloonix's own code; one holding a *cmd*
wraps an external project: unpacks, patches, builds. Three greps orient in
any of them::

    grep -n "int main" src/*.c
    grep -n "rpct_recv" src/*.c
    grep -n "string_server_unix" src/*.c

The *main* of every daemon ends with *daemon(0,0)* then *msg_mngt_loop()*.


Content
=======

.. toctree::
     :numbered:
     :maxdepth: 1

     doc/client_paths.rst
     doc/interface_logic.rst
     doc/flows.rst
     doc/rpc.rst
     doc/suid_power.rst
     doc/bin_core.rst
     doc/bin_switch.rst
     doc/bin_guest.rst
     doc/bin_build.rst
     doc/gui_server.rst
     doc/conventions.rst
     doc/demo_rules.rst
     doc/autotest.rst
     doc/starved_proxymous.rst
     doc/todo.rst
     doc/todo_maybe.rst
