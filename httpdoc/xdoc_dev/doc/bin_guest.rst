=======================
The guest-side binaries
=======================


cloonix-qemu-system
===================

*cloonsrc/qemu* is a *cmd* around the vendored qemu with
*--without-default-features --without-default-devices
--target-list=x86_64-softmmu* and an allow-list of devices, so six firmware
files are shipped rather than the *pc-bios* tree, and the virtio-net device
is emitted with *romfile=* so no option ROM ships. One process per kvm,
started by suid-power. The main server drives it on two unix sockets in the
guest's work directory: **qmp**, *qmp.c* and *qmp_dialog.c*, and **qga**,
the guest agent inside the guest, *qga_dialog.c*, which is how cloonix knows
a guest has booted and how it writes the launcher in. The qcow2 overlay is
written and its backing file read in *qcow2_hdr.c*; no qemu-img is shipped.
A qemu that dies without a guest shutdown is reported with the last line of
its *dtach/<vm>.stderr* in the KERR (*stderr_tail()* of *qemu_automaton.c*).


cloonix-crun
============

*cloonsrc/crun*, a *cmd* around the vendored crun, built static and
stripped. One process per cnt; the use is in the suid-power chapter.


cloonix-dtach
=============

*cloonsrc/dtach*, a *cmd*. Keeps a program attached to a pty alive when
nobody watches: a console survives its browser tab. The socket per session
is in the work directory, *utils_get_dtach_sock_path* for a kvm, *cnt_<n>*
made by suid-power for a cnt. *web_pty.c* runs *cloonix-dtach -a <sock>*
under *forkpty*, not setuid, as the user.


The insider agents
==================

*cloonsrc/insider_agents* holds the sources and *cmd_mk_iso*::

    cloonix_agent          linux, also cross compiled with clang for freebsd
    cloonix_cxp_srv        linux and freebsd
    cloonix_agent_win      mingw
    cloonix_cxp_srv_win    mingw
    init_starter           cloonixsbininit, pid 1 of a cnt

Three discs, one per kind of guest, each checked to carry its own set and
not the others'::

    insider_agent_x86_64.iso
    insider_agent_win_x86_64.iso
    insider_agent_bsd_x86_64.iso

*pthexec.c* has a function per disc and *utils_get_cdrom_path_name* picks
on the vm flags. *build_cloonfs* checks every tool the tree needs before
compiling, *gcc-mingw-w64-x86-64* and *clang* among them, and reports every
miss in one pass; *cmd_mk_iso* refuses to produce a disc with a missing
agent. What is checked is the binary, the package column says what to
install, and *build_tools/vmbuild/builder.sh* installs the same list.

The agent is built in the same pass and shipped in the same cloonfs as the
server, and a guest image carries no cloonix: only two empty mount points
under */var/lib/cloonix* and, for the toy, *cloonix_server.service*, which
does nothing unless the cloonfs is mounted at */cloonix*. So there is no
version check on the agent channel; the doors port logs a version
difference for a separately installed client and lets it through.
