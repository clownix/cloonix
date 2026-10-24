==========================================
cloonix-suid-power, the privilege boundary
==========================================

One binary of the delivery is setuid root, and every operation the kernel
refuses to an ordinary user happens here. Source: *cloonsrc/suid_power*, a
*Makefile*. One process per net, started by the main server, dying with it.

.. image:: /svg/suid_power.svg
   :align: center
   :width: 100%

One long-lived process reachable through one unix socket in the work
directory of the net, rather than a setuid helper per operation callable by
anyone with any argument. Its whole interface is the vocabulary below.


The vocabulary
==============

Requests, one text line each on the rpc::

    cloonsuid_req_launch      name=%s vm_id=%d dtach=%s argc=%d: ...   start a qemu
    cloonsuid_req_vm_kill
    cloonsuid_req_pid_kill
    cloonsuid_req_phy
    cloonsuid_req_ovs         tool=ovs-vsctl args: a read of the ovs database, run as root
    cloonsuid_req_ns_pid      name=%s pid=%d: one process of the net, for the namespaces
    cloonsuid_req_ns_go       read /proc/<pid>/ns of the pids received, answer as ovs lines
    cloonsuid_req_spawn       start an ovs satellite: drv, snf, a2b, c2c, nat
    cloonsuid_req_suidroot
    cloonsuid_air_cnt         name=%s idx=%d air=%s: a cnt radio onto an air, "-" none
    cloonsuid_air_kvm         name=%s idx=%d mac=%s air=%s: a kvm radio, made the first time
    cloonsuid_air_kvm_del     name=%s: the radios of a kvm that died
    cloonsuid_air_snf         air=%s on=%d: the capture of an air, snf/<air>_0
    cloonsuid_crun_update     name=%s cpu=%d ram=%d: the caps of a running container

Answers::

    cloonsuid_resp_launch_vm_ok   cloonsuid_resp_launch_vm_ko
    cloonsuid_resp_pid_ok         cloonsuid_resp_pid_ko
    cloonsuid_resp_phy
    cloonsuid_resp_ovs_line       cloonsuid_resp_ovs_end     the lines of an ovs read or of dump namespaces
    cloonsuid_resp_suidroot_ok    cloonsuid_resp_suidroot_ko
    cloonsuid_air_cnt_resp_ok     cloonsuid_air_cnt_resp_ko
    cloonsuid_air_kvm_resp_ok     cloonsuid_air_kvm_resp_ko
    cloonsuid_crun_update_resp_ok cloonsuid_crun_update_resp_ko
    cloonsuid_crun_killed         a container whose crun died, unasked
    cloonsuid_trace               a line for trace crun or trace nspace

Containers are driven by a second set on the same socket, one line per
step, *cloonsuid_crun_<step>*, handled in *crun.c*: *create_tar_img* (a tar.gz extracted into
*<n>_targz*), *create_net*, *create_eth*, *create_config_json*,
*create_overlay*, *create_crun_start*, *delete*.
The main server waits for each answer, so a failure stops at its step.


Making a container
==================

.. image:: /svg/crun_steps.svg
   :align: center
   :width: 100%

Six steps, in *crun.c* and *crun_utils.c*:

1. *crun_utils_create_overlay*: overlayfs, the image of bulk read-only
   below, a writable layer above. Twenty containers share one image.
2. *create_config_json*: the OCI spec from the template in
   *config_json.h*, mounts, capabilities, uid and gid maps, namespaces,
   and rp_filter 0 in *all* and *default* (*linux.sysctl*), which crun
   writes inside the namespace of the container: a new namespace takes the
   ipv4 settings of the host, whose rp_filter, 2 on systemd distributions,
   drops what enters through an interface without address, a VRF device.
   suid_power itself cannot write them in a toy, where its /proc/sys is
   read-only.
3. *crun create*: the container exists, paused; *read_crun_create_pid*
   reads the pid, the handle on every namespace.
4. *crun_utils_create_veth*: one end in the switch namespace, the other
   moved into the container by that pid, before anything runs inside.
5. *crun start*.
6. Teardown in reverse, in three finalizations because a mount can be busy
   for a moment after its last process dies; a retry is preferred to a
   failure.


The console socket
==================

Entering a container's namespaces needs root. The gui server asks on
*CONSOLE_SOCK* in the work directory, one line, *console_crun
name=<guest>*; what is forked is a dtach session running *crun exec -ti
<guest> /bin/sh*, and the reply names the socket to attach to. The socket is
chowned to the caller, 0600; the attach is an ordinary *dtach -a* as the
user. A second request for the same container gets the existing socket.
*cloonix-xwy-srv* and *cloonix-xwycli*, which once did this job, are gone.


The uid map
===========

Container uid 0 maps to the user running cloonix -- the caller uid of
*cloonix-suid-power*, not the owner of the image -- and container 1 upward
to that user's subuid window, added to */etc/subuid* and */etc/subgid* by
*tar_img.c* when it is missing. The image is not chowned: *idmap.c* mounts
it on *<cnt>/lower* through a user namespace that maps its owner to the
caller and the owner's window to the caller's (*open_tree*,
*mount_setattr* with *MOUNT_ATTR_IDMAP*, *move_mount*; kernel 5.19 or later
for the overlay on it). Non persistent, the overlay's lower is that mount,
made read only in the same call (*idmap_mount_ro*, *MOUNT_ATTR_RDONLY*): the
modes of an image are the guest's and cannot protect it, the mount does,
which is what keeps a *_targz* tree equal to its tar.gz;
persistent, that mount is the rootfs (*idmap_mount*) and writes reach bulk
translated back. A file whose owner or group is outside the map
shows as *nobody:nogroup*, and container root can neither write, chown nor
list it whatever *CapEff* says; *setgroups* is denied for the life of the
namespace; *chown* to an unmapped id answers EINVAL.

A privileged container is the unprivileged list minus *{"type": "user"}*,
*NAMESPACES_PRIVILEGED* in *config_json.h*. Container root is then host
root, with *CAP_SYS_MODULE* and *CAP_MKNOD* meaning what they say; crun
runs with *--cgroup-manager=disabled*, so there is no cgroup and no limit.
The namespace report of *trace nspace* has no *user* line for one. No
client flag asks for it any more: it exists for the toy alone, where the
promotion below makes every container one, and *crun.c* refuses the bit
outside the toy. The *is_privileged* field stays on the wire, at 0.


The owner of an image
=====================

*get_path_uid_image* in *tar_img.c* takes the uid of the caller, never the
owner of the image: *idmap.c* reads the owner off the tree (*tree_owner*)
and builds the user namespace that maps that owner to the caller, so an
image unpacked with sudo, by another user, or copied from another machine
runs as it is and is never chowned. uid 0 as caller means the toy, where
the crun of the toy maps one uid::

    "uidMappings": [ { "containerID": 0, "hostID": __USER_UID__, "size": 1 } ]

and the promotion to privileged in *crun.c* is that path.

A missing image is refused in the switch, *image_missing* in
*recv_from_client.c*, before *cloonsuid_crun_create_net* is sent: the
client gets the reason at once and suid-power is not asked. The refusal
names the builder of *vm_create* by the naming rule of the tree.


The guards
==========

Everything that reaches this process comes from the net owner, an account
that is not root, and *guard.c* is where each root action is bounded to
what that account could do alone. One function per shape of argument:

============================  ===============================================
guard                         what it decides
============================  ===============================================
*guard_group_allows*          at start: the caller is root or a member of
                              that group, read from */etc/group* with
                              *getgrouplist*, not from the process
                              credentials, so a *usermod* is effective
                              without a new login; no group, no gate
*guard_path_in_root*          absolute, no *..*, under
                              */var/lib/cloonix/<net>*: the dtach socket,
                              the launch script, the stderr log
*guard_caller_can_access*     *setfsuid* to the caller, *access()*, back to
                              root: can the caller open this file; an
                              absent file is judged by its directory
*guard_launch_line*           the five words of a launch: pinned dtach,
                              *-n*, socket in root, pinned bash, script in
                              root
*guard_launch_script*         the script: owned by the caller, two lines,
                              the shebang of the cloonfs bash, then *exec
                              nsenter --net=<ns> cloonix-qemu-system -L
                              <dir> ...*, no shell character, one trailing
                              *2><log>* in root, every path of every
                              *key=value* item openable by the caller
*guard_qemu_token*            one token of the qemu line: the value after
                              the last *=* of a *key=value* item, or the
                              whole token; a token holding a slash is a
                              path and the caller must be able to open it
*guard_pid_is_callers*        the pid's real uid is the caller, or its
                              command line names the net directory
============================  ===============================================

Outside *guard.c*: *ovs_word_allowed* refuses any option token, since
*--log-file* or *--db* would make the tool write or read as root wherever
told; *spawn_bin_allowed* pins the five satellites against
*pthexec_usr_libexec_cloonix()*, and *main* refuses an *argv[3]* that
differs from it and an *argv[2]* that is not */var/lib/cloonix/<net>*;
*erase_workdir* removes a directory the caller owns; *crun.c* refuses a
privileged container outside the toy, whoever asks.

qemu itself gets *-run-with user=<uid>:<gid>* from *qemu_automaton.c*, so
it is root only while opening its devices. The four satellites call
*cloonix_drop_root()* from *glob_include/name_sane.c* right after their
tap is open: *setgroups*, *setresgid*, *setresuid* to the caller; their
*suidroot* answer is a handshake and takes nothing.

The directory the scripts live in, */var/lib/cloonix/<net>*, is 0700 of the
owner, so what root runs cannot be swapped by another local user between
the write and the exec.


Names that become paths
=======================

*name_is_sane()* and *path_is_sane()* in *glob_include/name_sane.c*,
compiled into every binary: a name is letters, digits, underscore, dash,
dot, no leading dash or dot, no *..*; a path is absolute with no *..*. Every
argument that becomes a path is checked before it does, in suid-power and
in the five ovs satellites alike. *erase_workdir*, a verb of this binary
that removes */var/lib/cloonix/<net>*, checks its argument the same way.


In the rootless installation
============================

.. image:: /svg/namespaces.svg
   :align: center
   :width: 100%

The binary is not setuid there: it is root inside the crun that wraps
cloonix, and root inside that namespace is your uid on the disk. Same code,
same verbs.
