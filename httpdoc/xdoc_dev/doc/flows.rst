===========================
Signalling and data flows
===========================

Two planes run through a net. The **control plane** carries requests and
answers: text rpc, one json line per message, every hop a socket. The
**data plane** carries frames: it lives in the namespace *cloonix_<net>*,
and no frame of a guest crosses the main server.

.. image:: /svg/flow_planes.svg
   :align: center
   :width: 100%

Three rules hold on every path of this chapter:

  * **One writer of the topology.** *cloonix-main-server* alone changes
    it. A browser, a client, a demo reach it by the same rpc.
  * **An answer states a result.** A command is answered *ok* when the
    state it asked for is true, never when the request is accepted; where
    the result takes time, *ack_pending.c* tests it on a timer, and answers
    *ok*, *ko* or *TIMEOUT* with the reason.
  * **A silence is measured.** A peer judged by its silence is timed on
    *CLOCK_MONOTONIC* with the credit of *stall_detect.h*, never by
    counting beats of the loop.


The sockets
===========

Every socket of a net, by who listens on it::

    tcp  cloonix_port (45211)            proxymous      doors frames, tls or plain, hmac
    tcp  4444                            proxymous      https of the canvas
    unix proxy_pmain.sock                doorways       what arrived on cloonix_port
    unix proxy_pjsweb.sock               gui-server     what arrived on 4444, decrypted
    unix proxymous_demo.sock             proxymous      a demo to run, from the gui server
    unix dgram_c2c_<port>_tx.sock        c2c, proxymous one pair per c2c, datagrams
         dgram_c2c_<port>_rx.sock
    unix snf/<n>_<num>_pcap_sock         cloonix-dumpcap one per sniffed eth
    unix ovsdb_server.sock               ovsdb-server   read by cloonix-ovs-vsctl
    unix /cloonixmnt/tmp/unix_cloonix_cxp_sock
                                         cxp-srv        inside a guest, the agent's peer

The paths not absolute are under */var/lib/cloonix/<net>* or under the
proxyshare directory of the net. *cloonix cli <net> dump pid* lists every
process behind them.


A command from the client
=========================

.. image:: /svg/flow_cli_ack.svg
   :align: center
   :width: 100%

::

    cloonix cli nemo add cnt c1 eth=1 trixie

1. The client writes a doors frame: a 48 byte header, the json line, an
   hmac-sha256 keyed by *cloonix_passwd*. On *ip:port* the frame travels in
   tls, which proxymous ends.
2. proxymous relays the bytes unread to *proxy_pmain.sock*. doorways checks
   the hmac; a mismatch closes the connection without an answer.
3. The main server checks the request: the name is free, the image is in
   bulk, the eth list is valid. A refusal is answered at once, *ko* with
   its reason.
4. suid-power makes the container, one request per step, each answered:
   *create_alloc*, *create_net_eth*, *create_config_json*, *create_overlay*,
   *create_crun_start*. A failed step stops the chain at that step.
5. ovs-drv puts each eth in the switch: the port brought up in the
   namespace, then plugged into the bridge of its lan.
6. *ack_pending.c* tests the result on a timer. The client gets *ok* when
   the container runs with every eth in place, with the *tid* it sent.

The exit code of the client says which side ended the command
(*EXIT_OK*, *EXIT_REFUSED*, *EXIT_LOCAL*, *EXIT_BUG* of *cloonix/cloonix*)::

    0   the server said ok
    1   the server said no, its reason printed
    2   refused on this side: the line, the config, the connection
    3   the client itself failed

The same path serves *ssh*, *scp* and *files*, in a leg of type
*doors_type_dbssh*: see *The ways in*.


A lan
=====

::

    cloonix cli nemo plug c1 eth0 lan1

A lan is an openvswitch bridge in the namespace, made by ovs-drv on the
first eth plugged into it and removed with the last one::

    main-server                       ovs-drv                      ovsdb, vswitchd
    add_lan lan1        ----------->  ovs_cmd_add_lan      ---->   add-br
    add_lan_endp        ----------->  ovs_cmd_add_lan_endp ---->   add-port
                        <-----------  ovs_resp_ok
    status ok to the client, topology event to the gui server

*cloonix-ovs-drv* is one process per net: it creates the namespace, starts
*cloonix-ovsdb-server* and *cloonix-ovs-vswitchd* inside it, and runs every
change of the switch that follows. It reads nothing but the requests of the
main server.


The canvas
==========

.. image:: /svg/flow_web.svg
   :align: center
   :width: 100%

The browser speaks https to proxymous on 4444. The gui server receives
plain http on *proxy_pjsweb.sock*, checks the basic credentials (the net
name and *cloonix_passwd*) and the *Origin* against the *Host*, then turns
a */ctl/<verb>* into the rpc a client would send. The answer to the browser
is the topology event: the main server tells its subscribers, the gui
server pushes it on every */topo* websocket. Two browsers on one net see
the same canvas.

A shell of the canvas is */sh-<net>/ssh/<vm>/ws*: the gui server runs
*cloonix-cxp-cli* under *forkpty* and relays the pty on the websocket.


Into a guest
============

::

    cloonix cli nemo ssh c1 "ip addr"

::

    client ==doors, dbssh leg==> proxymous -> doorways -> agent socket of c1
      kvm: a qemu chardev, /dev/virtio-ports/net.cloon.0 in the guest
      cnt: a unix socket bind-mounted into the container
    cloonix-agent -> /cloonixmnt/tmp/unix_cloonix_cxp_sock -> cloonix-cxp-srv
      EXEC PTY PUT GET PUTTREE GETTREE LIST, one operation per connection

No network interface of the guest is used: the path works on a guest with
no eth at all, and a guest whose network is broken stays reachable.

Every queue of the path has a bound, and a byte pipe is paced by
*header_val_xoff* and *header_val_xon* on its own number, either way::

    doorways -> agent       xoff while the client of the pipe is past
                            msg_tx_queue_high() (8 MB), xon under 1 MB
    agent -> doorways       xoff while more than 1 MB waits for the exec
                            server, xon under 256 kB; past 32 MB the pipe is
                            closed with a WARNING
    doorways                the client of a pipe is not read after an xoff of
                            the agent, nor while the backdoor of the guest is
                            past the mark
    agent                   no pipe is read while more than 8 MB waits for the
                            virtio port; no queue passes 64 MB
    agent                   30 pipes at most: the next is refused, a WARNING

A pipe held 10 s with its queues not shrinking is a WARNING in doorways,
a virtio port that took nothing for 10 s one in the agent.

A KERR of an agent is written in the guest, then sent on the port as
*header_type_kerr*; the linux exec server gives its own to the agent on the
datagram socket *UNIX_KERR_SOCK* (*cxp_wire.h*). Doorways passes at most
ten lines per ten seconds per guest, control bytes replaced, as the event
*agent_kerr*, and the main server writes each in the syslog of the net::

    KERR: nemo guest k1: action.c line:280 WARNING byte pipe 41 refused: ...

A line the port cannot take is counted and the next line says how many
were lost. The windows agent sends its faults the same way; the windows
exec server keeps its own in its log.


A capture
=========

.. image:: /svg/flow_sniff.svg
   :align: center
   :width: 100%

::

    cloonix cli nemo cnf c1 eth0 sniff=on
    cloonix cli nemo sniff c1 eth0 --dump icmp

*cnf <name> eth<i> sniff=on* asks ovs-drv for a mirror of the port (*add_snf_lan*) and
starts one *cloonix-ovs-snf* on it; *cloonix-dumpcap* beside it serves
*snf/<n>_<num>_pcap_sock*, one connection per subscriber:

  * **the client**: *pcap_relay.c* of the main server reads the stream,
    parses each record header, applies the filter of *pcap_filter.c* (a
    tcpdump-like grammar, no libpcap) and sends *pcap_traffic* messages on
    the doors connection. Past *msg_tx_queue_high()* (8 MB, *msg_layer.c*)
    waiting towards one client, its packets are dropped and counted, so a
    client that does not read slows nothing but itself.
  * **the canvas**: the */rec* websocket of the gui server starts a thread
    per eth that copies the stream into a *.pcap* file; webshark reads it
    in the browser.

The capture is a copy of the port: a frame not read by anyone costs the
bridge the mirror only.

An snf that goes on its own comes back by itself while its eth is still
capturable:

.. image:: /svg/flow_snf_restart.svg
   :align: center
   :width: 100%

The dumpcap reads the end of its fifo and ends with status 0; its snf pid
being gone, that is written in the history and nothing more. The heartbeat
of *ovs_snf.c* (500 ms) sees the channel closed: *free_snf()* writes the
history of the capture once, one WARNING, and *timer_snf_defer()* starts a
new snf 200 ms later on the same eth, mirrored again on the same lan. The
new one finds the socket of the old one, erased, and its fifo, kept for a
reader that may be waiting on it (*ctrl_link.c*). A dumpcap that dies with
its snf alive is started again 300 ms later, five times at most.


The nat
=======

.. image:: /svg/flow_nat.svg
   :align: center
   :width: 100%

The nat item gives the guests of a lan the network of the host with no
change on the host: no bridge, no iptables rule, no route. A frame of a
guest crosses the bridge, the tap of *cloonix-ovs-nat*, a pipe to its
parent (*passt_shuttle.c*), then the socket of a *cloonix-passt* running on
the host as the user, as a 32 bit big-endian length and the frame. passt
turns the frames into ordinary sockets of the host. proxymous starts that
passt (*passt_spawn.c*), with *-t* and *-u* for the ports published to the
guests.

The backlog towards passt is bounded; past its high mark the tap is no
longer read, so the control link of the satellite is always served and its
watchdog never starves.


The c2c
=======

A c2c carries one lan of net A into net B, on the same host or another.
First the signalling, over the doors port of B:

.. image:: /svg/flow_c2c_sig.svg
   :align: center
   :width: 100%

The master is the net where the c2c was created. It connects to the
*cloonix_port* of the peer with the password of the peer, the peer creates
its half, then each main server asks its proxymous for a udp port at or
above *c2c_udp_port_low* and sends it to the other. The states are those
*cloonix cli <net> dump topo* shows, *state_master_try_connect_to_peer* to
*state_udp_connection_tx_configured*.

Then the frames, which never cross a main server:

.. image:: /svg/flow_c2c.svg
   :align: center
   :width: 100%

*cloonix-ovs-c2c* reads the bridge through its tap, rewrites the macs of
arp and ip where the far lan needs it (*packet_arp_mangle.c*), and passes
each frame on its datagram socket pair to proxymous, one udp datagram per
frame. Five c2c relay at once at most (*MAX_UDP_PROXY*). A cut link is
rebuilt by the master.


The life of a satellite
=======================

.. image:: /svg/flow_watchdog.svg
   :align: center
   :width: 100%

snf, a2b, c2c and nat share *ctrl_link.c* of *lib_io_clownix*: one listening
socket for the main server, one live connection, one watchdog. The first
*pid_req* must arrive within 5 s (*CTRL_LINK_GRACE_MS*), then one within
every 20 s (*CTRL_LINK_BEAT_MS*). Every stop, watchdog, closed connection
or *kil_req*, is the same *ctrl_link_stop*: the paths it registered
unlinked, its tap children killed, one KERR line, exit 0.

Every silence judged between the main server and a helper is 20 s, so a
host slowed for seconds by an extreme load breaks nothing::

    CTRL_LINK_BEAT_MS     20000   ctrl_link.c       snf, a2b, c2c, nat: the main server
    PROXY_WATCHDOG_MS     20000   ovs_nat_passt.c   nat: its proxymous leg
    HEARTBEAT_LIMIT_MS    20000   ovs_drv.c         ovs-drv: the main server, 500 ms beats
    C2C_PEER_SILENCE_MS   20000   ovs_c2c.c         the peer net of a c2c


The airs
========

.. image:: /svg/flow_air.svg
   :align: center
   :width: 100%

suid-power is the medium of every air (*vwifi.c*), cnt and kvm radios
alike, in a process of its own, suid-power-air in ps, started by
suid-power (*air_proc.c*): the commands suid-power runs for the containers
block it, and a medium stopped for a second loses the beacons of its airs.
A radio is created on a pipe that suid-power waits on; a delete, an air
and a capture go on a second pipe, whose answers come back on it. A cnt radio of *wlan=* is a mac80211_hwsim radio made by suid-power
from inside *cloonix_<net>*, so the radios of a net are a netgroup of their
own, then moved into its container as *wlan<n>*. suid-power registers as
the medium of that netgroup (*HWSIM_CMD_REGISTER*)::

    radio of sta1 --HWSIM_CMD_FRAME--> suid-power (vwifi.c)
       for each radio on the same air:  HWSIM_CMD_FRAME to a cnt radio,
                                        the air link to a kvm radio
       to the sender:                   HWSIM_CMD_TX_INFO_FRAME, acked when
                                        the frame is multicast or reached
                                        its receiver
       the frame, radiotap pcap:        snf/<air>_0
    cloonix-dumpcap -i snf/<air>_0      started by the main server (wifi_air.c)
       <air>_0_pcap_sock, _pcap_fifo    sniff <air>, webshark

A frame or an address change from a radio deleted less than 10 s ago was
queued before the delete and is dropped without a word; from any other radio
unknown to the net it is a WARNING. A container being deleted holds no air:
the main server stops counting its radios when the delete starts, as
suid-power does.

The capture of an air is asked by *cnf <air> sniff=on* and lives as long as
the air: an air that loses its last radio is gone with its capture, and
comes back without one. suid-power writes the frames of a captured air only,
and writes a pcap header each time it opens the fifo. A second *sniff=on*
leaves a running dumpcap alone. A dumpcap that dies is started again one
beat later, suid-power told first to close its writer, so the new dumpcap
reads a fresh header; five deaths and the air is left uncaptured, with a
WARNING.

A kvm radio is made by the agent in its guest. At each declaration of the
backdoor of a kvm of *wlan=N* (*wifi_air_kvm_radio*), the main server
tells suid-power each radio and its air, then doorways the radios::

    cloonsuid_air_kvm name=<k> idx=<i> mac=<m+i> air=<air>    to suid-power,
                                    "-" for no air; the radio made the first
                                    time, answered cloonsuid_air_kvm_resp_ok
    kvmwifi_on nb=<N> mac=<m>       to doorways, radio i takes m, last byte
                                    plus i
    cloonsuid_air_kvm_del name=<k>  to suid-power when the kvm dies

Doorways asks the agent on the answered pings until it says on or ko. The
agent (*cloonix_agent/src/wifi.c*, raw generic netlink) loads mac80211_hwsim
in the guest, gives the macs to udev with *ID_NET_MANAGED_BY* (in
*/run/udev/rules.d*) and to an older networkd with *Unmanaged=yes* (in
*/run/systemd/network*), makes the radios, *wlan0* to *wlan<N-1>* in that
order, and registers as the medium of the guest. Doorways relays the frames
on the **air link**, a seqpacket socket *<net dir>/air_link* suid-power
listens on and doorways connects to (*glob_include/air_link.h*); a message
is its type, the kvm name, then the payload of the agent::

    radio i of k1 --HWSIM_CMD_FRAME--> cloonix-agent of k1
       to the host:          header_type_wifi_frame: freq, rate, i, frame
       to the radio:         HWSIM_CMD_TX_INFO_FRAME, acked when the frame
                             is multicast or its receiver is a peer of
                             radio i
    doorways (kvmwifi.c)     AIR_LINK_FRAME k1, the payload as it is
    suid-power (vwifi.c)     to every other radio on the air of radio i,
                             cnt or kvm, a kvm one with the index of the
                             radio to give it to
                             the frame, radiotap pcap: snf/<air>_0
    doorways (kvmwifi.c)     AIR_LINK_FRAME k2 j, to the agent of k2
    cloonix-agent of k2 --HWSIM_CMD_FRAME--> radio j of k2

A kvm sender is acked by its agent without waiting: a unicast is acked when
its receiver is on the air of the sender. suid-power keeps per radio its
mac and further addresses, of a cnt radio its vifs, of a kvm radio the
senders of its frames, probe requests aside, and tells each kvm radio the
addresses of the other radios of its air (*AIR_LINK_PEERS*, relayed as
*header_val_wifi_peers*) whenever they change, and to a radio moved to
another air in any case. Doorways keeps the last list of each radio and
tells it again to an agent back up. The agent adds to a radio's list the
senders of the frames given to it, which came from its air, until the next
list replaces it: a list late behind a first frame costs no ack. A list
that reaches doorways before the ask of the switch is kept for the ask. A
qmp reconnect drops the kvm in doorways and adds it again; a radio that
had its list counts as gone for ten seconds, and its frames in that window
are dropped without a warning. The air link may also speak of a kvm
before its add, sent by the switch on another socket, reaches doorways:
its peers are kept (*early_note()*) and given to it at its add
(*kvmwifi_born()*); a name still unknown after ten seconds is a WARNING. A radio
moved to another air stops being acked by the radios it left, so a station
loses its access point as it would out of range. An agent that gets frames
with no radio (restarted) answers *wifi_off* and is asked again.

No queue of an air grows without a bound, and the medium is never paused:
one slow guest would stop every air of the net. A frame that finds its way
full is dropped and counted, one WARNING per ten seconds, five in the agent::

    cnt radio -> suid-power       mac80211_hwsim keeps a fixed number of
                                  frames waiting for their tx status, which
                                  suid-power gives after the delivery; a
                                  netlink socket overflowed is lost frames
    suid-power <-> doorways       the air link full, in either direction
    doorways -> agent of k        the backdoor of k past msg_tx_queue_high()
    agent of k -> doorways        the virtio port past 8 MB (VIRTIO_TX_HIGH)
    suid-power -> dumpcap         the fifo of the capture full
    dumpcap -> client, canvas     as for an eth, see A capture

A frame the agent drops is not acked, as on a busy air, and 802.11 sends
it again; the kvm senders behind doorways are acked by their agent, so a
drop there is a frame lost after its ack.

*plug <guest> wlan<i> <air>* is the air of a radio, any air for any
radio: *cloonsuid_air_cnt* for a cnt, *cloonsuid_air_kvm* for a kvm,
answered ok once suid-power has moved it, a ko after 3 s without the answer
for a kvm. *dump json* carries the airs as *vwif_air*, *dump commands* replays
those off the default air of the radio, *cntwifi* or *kvmwifi*.

The interface of *plug*, *unplug*, *cnf* and *sniff* is an
*int num* on the wire. The client writes *eth<i>* as *i* and *wlan<i>* as
*WLAN_NUM_BASE + i* (256, *glob_common.h*); the number of an item passes as
it is. The main server turns *wlan<i>* into the number after the eths of the
guest (*wifi_air_num()*, *wifi_air.c*) and writes an interface back as the
user gives it (*wifi_air_iface()*), in *dump commands* and in its answers. A
plain number past the eths of a guest is still a wlan: a 76 client sends it
so.
