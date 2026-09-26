===============================
Things that could be added next
===============================

A list to choose from, each entry naming what exists in the tree so nothing
is built twice. What was given up on purpose, with the architecture that
would bring it back, has its own chapter, *Things set aside*.


Small, groundwork done
======================

  * **publish=<addr>:<hostport>:<guestport>**: passt's *-t* takes an
    address; *ports_to_passt* in *proxymous_passt/src/passt_spawn.c* builds
    it.
  * **An instantaneous cpu rate**: *dump pid* reports the average over the
    life of the process; a rate needs two samples and the half-second beat
    of the switch could hold the first.


From the windows test on a work machine
=======================================

The syslog of a failed *add win* was readable only with the source open
and the guest at hand. Each entry below removes one trip.

  * **why <guest>**: the startup state *qga_where* prints, the ping status
    of doorways, the exit text of the agent launcher and the qemu stderr
    tail the death KERR carries, answered to the client as one message instead of scattered over
    a syslog the user has to grep on the server.
  * **A windows launcher that returns at once**: *QGA_WIN_SCRIPT_LAUNCH*
    uses *start /b*, and cmd.exe came back 33s later on the test machine.
    The switch now waits for windows instead of failing, but the cause
    (Defender scanning the exes from the cd, or inherited pipe handles) is
    unmeasured. *powershell Start-Process* or *wmic process call create*
    detaches without inheriting the qemu-ga pipes; *C:\\cloonix-agent.log*
    timestamps say which of the two it was.
  * **Second family of demo helpers**: twelve demos keep an *ssh()* with a
    *log=* argument and a longer error text, thirteen a *put()* that traces,
    twenty-seven a *place()* built on *cli()*. Converging each pair on one form
    moves about 700 more lines into *demos/lib/demolib.py*; the error
    texts change, so it goes with a run of *debug/demo_test.py*.
