=========================
Instructions for building
=========================

.. image:: /svg/build_chain.svg
   :align: center
   :width: 100%

Cloonix recompiles the open sources it uses and patches some of them. The
result lives in */usr/libexec/cloonix/cloonfs*: a customised library has no
effect on the host, and a qemu, openvswitch or crun of the host has no
effect on cloonix.


The external sources
====================

Five projects, refreshed by *update_targz_store.sh* at the root of the tree,
each cloned at depth 1 and named after its commit in *targz_store*::

    https://git.qemu.org/git/qemu.git
    https://github.com/openvswitch/ovs.git
    https://github.com/containers/crun.git
    https://passt.top/passt
    https://github.com/NixOS/patchelf.git

::

    ./update_targz_store.sh              all of them
    ./update_targz_store.sh ovs          one
    ./update_targz_store.sh --list       the names

Two more scripts fill the store: *rootfs_targz_store.sh* builds the base
root file-system of cloonix with mmdebstrap, and
*rootfs_targz_store_freebsd.sh* takes the freebsd base once and keeps the
headers and the static archives the freebsd agents are cross compiled
against, a few megabytes. *doitall* then reads the store and downloads
nothing. The dissection sources of webshark, noVNC and xterm.js are vendored
under *cloonsrc/jscloonix/jscloonix*.


Getting the cloonix sources
===========================

::

    git clone --depth=1 https://github.com/clownix/cloonix.git

or::

    wget https://clownix.net/downloads/cloonix-source-__LAST__.tar.gz
    wget https://clownix.net/downloads/cloonix-targz_store-__LAST__.tar.gz
    tar xvf cloonix-source-__LAST__.tar.gz

The second file goes with the first: it is the *targz_store* directory, the
external sources above at the commits of this version and the base root
file-system. It is extracted at the root of the tree before the build, below;
a git clone takes the same file, or fills the store itself with the three
scripts above.


Building the bundle
===================

The build runs inside a debian trixie virtual machine, so the result does
not depend on the workstation. First the builder machine, from
*build_tools/vmbuild*::

    ./doitall.sh

which needs a local debian repository at 127.0.0.1 and produces
*/var/lib/cloonix/bulk/trixie.qcow2* and the builder. Then, at the root of
the tree::

    tar xvf ../cloonix-targz_store-__LAST__.tar.gz
    ./doitall

*doitall* calls *allclean*, copies the tree into the build zone, compiles
patchelf, runs *build_tools/build_cloonfs*, which checks every tool the tree
needs before compiling anything (*gcc-mingw-w64-x86-64* for the windows
agent, *clang* for the freebsd one among them) and compiles everything,
then *build_tools/pack_cloonfs*, which collects the binaries, adds the base
root file-system, makes the certificate, and rewrites every rpath and
interpreter with patchelf. The result::

    ${HOME}/cloonix-bundle-__LAST__.tar.gz

::

    ./allclean                    wipes the objects and the build zone

The i386 architecture is not available.


Building the toy
================

From an installed bundle::

    cd toy_targz
    ./create_toy_targz.sh

It copies */usr/libexec/cloonix* into a container root, adds crun, passt,
proxymous, the loader they need, the client, the demos, *setup.sh* and the
certificate, and leaves::

    ${HOME}/cloonix-toy-__LAST__.tar.gz


Building this documentation
===========================

Two books, *httpdoc/xdoc_user* and *httpdoc/xdoc_dev*, one *cmd* for both::

    cd httpdoc && ./cmd

*cmd* reads the version from *cloonix/cloonix_config.py*, copies the sources
of each book, substitutes *__LAST__* and runs sphinx into *httpdoc/build_user*
and *httpdoc/build_dev*. *httpdoc/xdoc_theme* gives the pages the header, the
footer and the stylesheet of the site. *httpdoc/index.html* is the one index:
its nav links to *build_user/user.html* and to *build_dev/dev.html*. *httpdoc/allclean*
removes both builds.

The checkers of *debug/* that read the docs and the tree::

    debug/check_docs.sh          every cloonix-* name is in the delivery, no version by hand
    debug/check_doc_words.sh     every line carries a fact and no judgement
    debug/check_cli.py           every command line is spelled as the client reads it
    debug/check_doc_cli.py       every cli line of the books parses in the client
    debug/check_demos.py         the demos tree

*debug/README.md* has the others, *check_wire.sh*, *check_cloonfs_bins.sh*
and *deadcode.sh*.
