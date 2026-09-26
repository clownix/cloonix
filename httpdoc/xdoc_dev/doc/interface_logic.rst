===========================
The logic behind the verbs
===========================

The command line of the user book is written in *cloonix/cloonix*; what it
sends is one json message of *json_interface.c* per request, the same the
canvas sends. This chapter is where each rule of the command line lives.

.. image:: /svg/interface_logic.svg
   :align: center
   :width: 100%


cnf: the kind read from the keys
================================

*build_cnf()* turns *cnf <name> [<interface>|A2B|B2A] key=value* into one
message. No two kinds of object share a key (*CNF_KEYS*), so the name does
not need its kind::

    sniff=           snf_add            any interface, any air
    x= y=            item_pos           any item of the canvas
    angle=           item_poseth        an interface of any item
    A2B|B2A ...      a2b_cnf            dir=<0|1> cmd=<name> val=<n>
    publish=         nat_cnf            publish=<ports>
    cpu= ram=        cnt_cnf            the keys given only
    rstp=            lan_cnf            rstp=on|off
    mac_mangle=      c2c_cnf            mac_mangle=<mac>

A cap not named keeps its value: *cnt_limit()* starts from the caps in
force (*lim_cpu*, *lim_ram* of *t_cnt*) and overwrites the ones named.


plug, unplug: the cable read first
==================================

*cable_first()* asks *list_commands_req* on the connection of the request,
answered at once, and reads the lan of the interface in the *plug* lines
(*lan_of()*); a wlan without a line is on the air of its kind, *cntwifi* or
*kvmwifi*:

* an *unplug* without a lan gets the lan it is on;
* a *plug* onto the lan it is on answers 0 and says so;
* a *plug* of an eth cabled elsewhere is an unplug then a plug; a plug
  refused there puts the eth back where it was. A wlan changes air in one
  move of *wifi_air.c*.

*cable()* asks again, every half second within the ack budget, a plug or
an unplug refused because the same interface *is being* cabled or
uncabled, or has a plug *already waiting* for its endpoint.

A message given with *rpc* is sent as it is, without this reading.
*cfg_name_is_iface()* refuses a lan or an air named *eth<i>*, *wlan<i>*,
*A* or *B*, so the word after a name is read one way only.


sniff: armed before it opens
============================

*cnf <guest> wlan<i> sniff=* is sent for the air of that radio, read from
*dump commands* the same way, and says so on stderr.

*do_sniff()* reads *dump commands* first: an interface without its
*cnf <name> <itf> sniff=on* line is armed with *snf_add* before the
*pcap_open*, so a nominal sniff is never refused by the server. A wlan is
replaced by its air. The refusal texts of *pcap_relay.c* stay the fallback
of a request from elsewhere.


dump commands: every setting replayed
=====================================

*list_commands.c* writes the adds, the plugs, the sniffs, the settings and
the places of the canvas, in an order where each line finds what it needs:

A c2c is written by the net whose *local_is_master* is set; the far net
writes the same line behind a *#*, so one replay makes one cable.

======================  ===================================================
setting                 recorded in
======================  ===================================================
nat ports               *ports* of *t_ovs_nat_main*, on the add line
a2b settings            *cnf_set*, *cnf_val*, *cnf_repsrc*, *cnf_repdst* of
                        *t_ovs_a2b*, on the ok of the a2b process
cnt caps                *lim_cpu*, *lim_ram* of *t_cnt*, on the ok of
                        suid_power
rstp                    *rstp* of the lan in *lan_to_name.c*, on the ok of
                        ovs_drv
mac_mangle              *c2c_chainlan.c*, *c2c_chainlan_mac_mangle()*
sniff of an item        its *endp_type*, *sniffed()*
capture of an air       *wifi_air.c*, gone with the air
places                  *layout_store.c*
======================  ===================================================

A setting is recorded when the process that holds it answers ok, never on
the request: *dump commands* says what is in force.


The requests of the canvas
==========================

The http threads of *cloonix-gui-server* never touch doorways. Each
request is queued (*job_post()*) and sent by the main loop (*job_rx()*,
woken by a pipe): the hmac key and digest, the rpc buffers and the tx
queues of doorways belong to one thread.
