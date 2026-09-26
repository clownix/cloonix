=================
The insider agent
=================

Every guest runs an agent that cloonix talks to. It is what turns a node
blue on the canvas and carries a shell or a file into a machine that has no
usable address; the two gauges of a node are read from its qemu process on
the host, not from the agent.

The agent is not in the guest images. It comes on a cd-rom that cloonix
builds, *cmd_mk_iso* in *cloonsrc/insider_agents*, attached to every guest.
A guest image contains no cloonix at all: no agent, no library, no binary.
An image built for an older release picks up the current agent at its next
boot.

.. image:: /svg/agent_iso.svg
   :align: center
   :width: 100%

Three discs, one per kind of guest::

    insider_agent_x86_64.iso        linux kvm and cnt: cloonix-agent, cloonix-cxp-srv, cloonixsbininit
    insider_agent_win_x86_64.iso    windows: cloonix-agent.exe, cloonix-cxp-srv.exe
    insider_agent_bsd_x86_64.iso    freebsd: the same sources, cross compiled static

*cloonix cli <net> trace qemu* shows which disc a kvm was given.

The agent is built in the same pass as the server and shipped in the same
cloonfs, so it is never of another version. The doors port compares the
wire version of a separately installed client with its own and logs a
difference once per version; the leg goes through in every case.


How it is started
=================

.. image:: /svg/agent_start.svg
   :align: center
   :width: 100%

**A kvm** boots first and is equipped afterwards. Its image runs
*qemu-guest-agent* on a virtio port of its own; cloonix uses it to write
*/agent_cloonix_launcher* into the guest and run it. The script mounts the
cd on */cloonixmnt/cnf_fs* and starts the agent and the exec server. A guest
without a working *qemu-guest-agent.service* never turns blue: create it
with *--no_qemu_ga* and reach it by the Console entry of its menu. A windows
guest gets *C:\agent_cloonix_launcher.bat* through the same channel.

**A cnt** has no boot: crun starts one process as init, and cloonix points
it at the disc, */cloonixmnt/cnf_fs/cloonixsbininit*. It starts the agent,
the exec server and the syslog daemon of the image, waits for the interfaces,
then hands over to the init of the image only when asked::

    cloonix cli nemo add cnt c1 eth=1 trixie_systemd startup_env=CLOONIX_WITH_SYSTEMD=1

The rootfs directory in bulk is never modified.


What it serves
==============

::

    liveness       a ping answered continuously: blue means the agent answers
    control        reboot and halt, behind the Reboot entry and a clean delete
    byte pipes     a stream joined to the unix socket of cloonix-cxp-srv
    radios         the mac80211_hwsim radios of a kvm, wlan=N: the agent makes
                   them in the guest and relays their frames on the backdoor


The exec server
===============

*cloonix-cxp-srv* is at the far end of a pipe and provides what a remote
shell needs without being an ssh server: a command with its exit status, a
login shell on a pty with *HOME*, *USER*, *LOGNAME*, *SHELL* and a *PATH*
set, a file or a directory copied in or out. On windows it listens on
127.0.0.1:4441 inside the guest; the windows chapter has the session details.
