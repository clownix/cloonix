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

The build runs inside a debian trixie virtual machine, the builder, so the
result does not depend on the workstation. With the *targz_store* extracted
at the root of the tree, it goes in three levels from *build_tools/vmbuild*::

    tar xvf ../cloonix-targz_store-__LAST__.tar.gz
    cd build_tools/vmbuild
    ./seedbuilder.sh
    ./builder.sh
    ./doitall.sh

*seedbuilder.sh* makes *seedbuilder.qcow2*, the first small machine;
*builder.sh* makes *builder.qcow2* from it, with every package the build
needs and the image of every specialised toy in its bulk. These two take
from a local debian repository at 127.0.0.1 and are run again when the
packages or the toy images change. *doitall.sh* takes from no repository:
it copies the tree into the builder, runs *./doitall* there, and brings the
bundle, the toy and every specialised toy of *toy_targz* back into
*${HOME}*; when *builder.qcow2* is missing or lacks the image of a toy, a
toy added after it was made, it stops at once and names *builder.sh*. On a
debian trixie machine, the same build without the builder, at the root of
the tree::

    ./doitall

*doitall* calls *allclean*, copies the tree into the build zone, compiles
patchelf, runs *build_tools/build_cloonfs*, which checks every tool the tree
needs before compiling anything (*gcc-mingw-w64-x86-64* for the windows
agent, *clang* for the freebsd one among them) and compiles everything,
then *build_tools/pack_cloonfs*, which collects the binaries, adds the base
root file-system, makes the certificate, and rewrites every rpath and
interpreter with patchelf. The result::

    ${HOME}/cloonix-bundle.tar.gz

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
certificate (the user of the toy puts the images in */var/lib/cloonix/bulk*
before running that *setup.sh*, see the toy chapter), and leaves::

    ${HOME}/cloonix-toy.tar.xz

A toy for one project makes its own classic toy first, then needs the image
of the project as */var/lib/cloonix/bulk/<image>.tar.gz*::

    ./create_frr_toy_targz.sh         ${HOME}/cloonix-frr-toy.tar.xz, from frr_trixie.tar.gz
    ./create_bird_toy_targz.sh        ${HOME}/cloonix-bird-toy.tar.xz, from bird_trixie.tar.gz
    ./create_suricata_toy_targz.sh    ${HOME}/cloonix-suricata-toy.tar.xz, from suricata_trixie.tar.gz
    ./create_bind_toy_targz.sh        ${HOME}/cloonix-bind-toy.tar.xz, from dns_trixie.tar.gz


Building this documentation
===========================

Two books, *httpdoc/xdoc_user* and *httpdoc/xdoc_dev*, one *cmd* for both::

    cd httpdoc && ./cmd

*cmd* reads the version from *cloonix/cloonix_config.py*, writes it into the
two pages that name it, *index.html* and *downloads.html*, copies the
sources of each book, substitutes *__LAST__* and runs sphinx into
*httpdoc/build_user* and *httpdoc/build_dev*. *httpdoc/xdoc_theme* gives the pages the header, the
footer and the stylesheet of the site. *httpdoc/index.html* is the one index:
its nav links to *build_user/user.html* and to *build_dev/dev.html*. *httpdoc/allclean*
removes both builds.

The checkers of *debug/* that read the docs and the tree::

    debug/check_docs.sh          every cloonix-* name is in the delivery, no version by hand
    debug/check_site.py          every link of the pages, every figure of the books
    debug/check_doc_words.sh     every line carries a fact and no judgement
    debug/check_cli.py           every command line is spelled as the client reads it
    debug/check_doc_cli.py       every cli line of the books parses in the client
    debug/check_demos.py         the demos tree

*debug/README.md* has the others, *check_wire.sh*, *check_cloonfs_bins.sh*
and *deadcode.sh*.
