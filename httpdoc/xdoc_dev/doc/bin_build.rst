====================================
The build chain and cloonix-patchelf
====================================

Nothing here runs when a net is up.

.. image:: /svg/build_chain.svg
   :align: center
   :width: 100%


cloonix-patchelf
================

*cloonsrc/patchelf* is a *cmd* around NixOS's patchelf, the one binary
never shipped: it runs once in
*pack_cloonfs* over every other binary and library and rewrites the
interpreter to the loader inside *cloonfs* and the rpath to the library
directories inside *cloonfs*.


The scripts that make a delivery
================================

::

    rootfs_targz_store.sh          mmdebstrap into a tree, then a curated copy into the delivery;
                                   compiles copy_libs from build_tools/toolbox/fill_common.c inside
                                   the chroot, hence gcc and libc6-dev in the list
    copy_libs                      ldd every executable of bin, sbin, the plug-in directories, and
                                   each sub-directory named in create_libs_txt; copies the libraries
    build_cloonfs                  checks the tools, compiles everything
    pack_cloonfs                   copies each binary in, runs copy_libs, runs patchelf, installs
                                   the demos, makes the certificate, keeps make_cloonix_cert in bin
    create_toy_targz.sh            the rootless tarball from an installed bundle: the crun that
                                   wraps cloonix, the init starter, the <net>cmd script
    create_<name>_toy_targz.sh     a specialised toy, through lib/special_toy.sh: the toy without
                                   kvm, the image of its project merged into its root,
                                   <name>_trixie, dns_trixie for bind
    lib/special_needs.py           what of that image the demos use
    lib/pack_toy.sh                the .tar.xz of a toy
    toy_drop                       the demos the toy leaves out, read by demo_test.py too

A package in the *LIST* of *rootfs_targz_store.sh* is not shipped unless a
copy list names it.


The specialised toys
====================

*lib/special_toy.sh* unpacks the toy, takes kvm and qemu out, and merges
into its root what *lib/special_needs.py* selects of the image::

    the commands the demo strings run       a word in command position that is an executable
                                            of the image
    the packages of <name>_demo/needs       whole: dpkg names or globs
    the scripts and libraries they need     transitively

The lines of a *needs* file::

    frr                         a package, taken whole
    !import                     a word of a configuration that is not a command
    ?usr/lib/frr/sharpd         a file of those packages, taken only when a demo names it
    @python                     of the python of the image, the modules the scripts written
                                into the containers import
    @python:<module>            one more module, imported at run time only

Python test files (*test_\*.py*, *tests/*) are never taken. Then every elf
added must load in the root of the toy; with *@python*, the interpreter
must start silently and every selected module import, and a
*sitecustomize.py* writes a module missing at run time as a KERR line.
*lib/pack_toy.sh* writes the .tar.xz: directories first, then the files
sorted by name, so that a library and its twin sit side by side for xz,
*-9e* on one thread.


Reading a build failure
=======================

Three shapes, in the order they bite::

    a package taken out of LIST whose files a copy list names    the copy fails
    a file taken out whose path another script rewrites          a sed fails
    a library no longer harvested, its binary in a directory     nothing fails at build;
    copy_libs does not scan                                      the binary does not start on the target

After any change to the rootfs::

    for f in cloonfs/bin/*; do ldd $f | grep "not found"; done


What the delivery is not
========================

frr is a guest image, *vm_create/cnt_kvm_demo/cnt_frr_trixie.sh*. The gtk
client, the Qt capture viewer, x11vnc, dropbear and qemu-img are not in it.
A directory of the rootfs that nothing fills is from one of those eras.
