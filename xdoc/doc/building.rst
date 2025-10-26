.. image:: /png/cloonix128.png 
   :align: right

=========================
Instructions for building
=========================


How the building is done.
=========================

Cloonix recompiles most of the open-sources it uses and sometimes patches
them for adjustments.
Binaries and libraries are put into a separate zone only for cloonix use ::

    /usr/libexec/cloonix/cloonfs

This allows the use of customized libs with no impact on the official host
machine.

The host machine can have qemu, openvswitch and spice installed, cloonix will
use its own version of these softwares.

Wireshark and crun are not required in the host, those software are embedded
in the bundle delivered with cloonix binaries.


Sources associated to cloonix
=============================

The external open sources used in cloonix for qemu, ovs, spice ...
are updated using the following links for the main software:

  * https://github.com/containers/crun.git
  * https://git.qemu.org/git/qemu.git
  * https://github.com/openvswitch/ovs.git
  * https://github.com/wireshark/wireshark.git
  * https://gitlab.freedesktop.org/spice/spice-gtk.git
  * https://gitlab.freedesktop.org/spice/spice-protocol.git
  * https://gitlab.freedesktop.org/spice/spice.git
  * https://gitlab.freedesktop.org/spice/usbredir.git
  * https://github.com/openssh/openssh-portable.git
  * https://github.com/NixOS/patchelf.git


For the function that creates the web interface, other open sources are used:

  * https://gitlab.freedesktop.org/xorg/xserver.git
  * https://github.com/novnc/noVNC
  * https://github.com/nginx/nginx.git

Get the cloonix sources
=======================

The cloonix source to compile is on github::

    git clone --depth=1 https://github.com/clownix/cloonix.git

You can get it also by download::

    wget http://clownix.net/downloads/cloonix-__LAST__/cloonix-__LAST__.tar.gz
    tar xvf cloonix-__LAST__.tar.gz


Howto create the bundle
=======================

To build the cloonix binaries, you have to have a debian repository, mine is
local at 127.0.0.1, then go to cloonix-__LAST__/build_tools/vmbuild and do::

    sudo ./make_trixie

These commands should create in bulk the qcow of debian virtual machines::

    /var/lib/cloonix/bulk/trixie.qcow2

Then you have to build the virtual machines debian_builder.qcow2
the bundle will be created from inside these::

    ./vmbuild_create

The cloonix-__LAST__ directory is not sufficient to compile all of cloonix,
you need some external open sources that are in the targz_store.tar.gz provided.
You must untar the targz_store.tar.gz in the cloonix-__LAST__ directory first.
And last you have the compilation virtual machines ready in bulk and do::

    wget http://clownix.net/downloads/cloonix-__LAST__/targz_store.tar.gz
    tar xvf targz_store.tar.gz
    ./doitall

This creates in build_tools the directory::

    ./build_tools/cloonix-bundle-__LAST__

For the i386, it is not availlable now.
