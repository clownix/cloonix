==============
The wifi demos
==============

The frames these demos show, beacon to encrypted data, are explained in the
chapter *Wifi: from the radio to the ip stack*; the airs and the radios are
in the chapter on the items, section *air*.

wifi/cnt_wpa2.py
================

.. image:: /svg/demo_wifi.svg
   :align: center
   :width: 100%

Three containers, no lan, any image carrying *iw*, *hostapd* and
*wpa_supplicant*::

    sudo modprobe mac80211_hwsim radios=0
    cloonix cli nemo demo wifi/cnt_wpa2.py

Six questions in order: did the radios arrive, does hostapd start, do the
stations hear it, can they join and ping, does the capture of the air carry
the beacons of the access point, and does a station put on a second air
lose the access point, then join it again back on *cntwifi*::

    cloonix cli nemo cnf cntwifi sniff=on
    cloonix cli nemo sniff cntwifi
    cloonix cli nemo plug sta2 wlan0 air2

In webshark, a double-click on the air, the 4-way handshake
shows as EAPOL Message 1 of 4 to Message 4 of 4, filter *eapol*. The pings
are *QoS Data ... (encrypted)*, filter *wlan.fc.type == 2*: WPA2 leaves no
IP readable, button *Data only*; *wifi/cnt_open.py* shows the same frames
decoded.


wifi/cnt_open.py
================

An access point and a station, no key::

    sudo modprobe mac80211_hwsim radios=0
    cloonix cli nemo demo wifi/cnt_open.py

In 1997 the IEEE published 802.11, a lan carried by radio; the first
networks were open and anyone on the channel reads every frame. hostapd and
wpa_supplicant are Jouni Malinen's, since 2002. The ping is captured on the
air and counted in clear::

    cloonix cli nemo cnf cntwifi sniff=on
    cloonix cli nemo sniff cntwifi


wifi/cnt_wpa3.py
================

The same network with WPA3-Personal. SAE, the Dragonfly exchange of Dan
Harkins adopted by the Wi-Fi Alliance in 2018, makes a password guess cost
one exchange with the access point. The capture of the join counts the SAE
commit and confirm and the four EAPOL messages; the ping that follows is
only encrypted data frames::

    cloonix cli nemo demo wifi/cnt_wpa3.py
    cloonix cli nemo cnf cntwifi sniff=on
    cloonix cli nemo sniff cntwifi


wifi/cnt_bridge.py
==================

A home box: its radio and its ethernet are one Linux bridge (Lennert
Buytenhek, 1999), hostapd putting *wlan0* in *br0* with *bridge=*. The ping
of the station is counted on the cable of the server::

    cloonix cli nemo demo wifi/cnt_bridge.py
    cloonix cli nemo sniff srv eth0 --dump icmp


wifi/cnt_roaming.py
===================

Two access points, one SSID and one key, each on an air of its own, both
bridged to the lan of a server. The station joins *ap1* on *cell1*; moved
to *cell2*, it finds *ap2* and keeps its address::

    cloonix cli nemo demo wifi/cnt_roaming.py
    cloonix cli nemo plug sta wlan0 cell2


wifi/cnt_mesh.py
================

Three radios, no access point: 802.11s (2011), open80211s by Javier Cardona
and Luis Carlos Cobo at Cozybit, in mac80211 since 2008. Each radio joins
the mesh *demomesh*, peers with the others, and the ping crosses the
mesh::

    cloonix cli nemo demo wifi/cnt_mesh.py
    cloonix cli nemo cnf cntwifi sniff=on
    cloonix cli nemo sniff cntwifi


wifi/kvm_wpa2.py, kvm_open.py, kvm_wpa3.py, kvm_bridge.py, kvm_roaming.py and kvm_mesh.py
=========================================================================================

Each of the six demos above has a twin played by kvm guests, image
*trixie.qcow2*, the radios made in each guest by the cloonix agent and
started on the air *kvmwifi* in place of *cntwifi*::

    cloonix cli nemo demo wifi/kvm_wpa2.py
    cloonix cli nemo demo wifi/kvm_open.py
    cloonix cli nemo demo wifi/kvm_wpa3.py
    cloonix cli nemo demo wifi/kvm_bridge.py
    cloonix cli nemo demo wifi/kvm_roaming.py
    cloonix cli nemo demo wifi/kvm_mesh.py
    cloonix cli nemo cnf kvmwifi sniff=on
    cloonix cli nemo sniff kvmwifi

The frames on the air are the frames of the container version.


wifi/cntkvm_home.py
===================

The box of a home: a kvm access point bridges its radio to lan1, where a
container runs kea, the DHCP server. A container station and a kvm station,
their radios on *cntwifi*, join with WPA2 and have no address: sta1 asks
with dhcpcd, sta2 with dhclient, the answer comes from the cable through
the bridge. The DHCP is in clear on the cable, encrypted on the air: WPA2
protects the air, not the lan behind the box. Images *dhcp_trixie* and
*trixie.qcow2*::

    cloonix cli nemo demo wifi/cntkvm_home.py
    cloonix cli nemo sniff srv eth0 --dump udp port 67 or udp port 68


wifi/cntkvm_enterprise.py
=========================

WPA2-Enterprise and the three roles of 802.1X: each station is a
supplicant, the kvm access point the authenticator, freeradius in a
container on the cable the authentication server. *alice* (container) and
*bob* (kvm) log in with PEAP and MSCHAPv2 and ping each other; *mallory*
(container) gives a wrong password and gets Access-Reject. The access point
never sees a password. Images *radius_trixie* and *trixie.qcow2*::

    cloonix cli nemo demo wifi/cntkvm_enterprise.py
    cloonix cli nemo sniff radius eth0 --dump udp port 1812


wifi/cntkvm_roaming.py
======================

One SSID on two cells, a container access point on *cell1*, a kvm access
point on *cell2*, both bridged to lan1. The kvm station pings the server
five times a second while it is moved to *cell2*: it finds *ap2*, keeps its
address, and the demo counts the pings lost in the change. Images *trixie*
and *trixie.qcow2*::

    cloonix cli nemo demo wifi/cntkvm_roaming.py
    cloonix cli nemo plug sta wlan0 cell2
    cloonix cli nemo cnf cell2 sniff=on
    cloonix cli nemo sniff cell2


wifi/cntkvm_monitor.py
======================

Two containers run a network, a kvm listens with a monitor interface,
*mon0*, beside its *wlan0*, tcpdump inside the guest: the view of an
eavesdropper, where *sniff cntwifi* is the view of the medium. On the
open network *spy* reads the ping; on WPA2 it sees the beacons, the 4-way
handshake, the addresses and sizes, and no IP. Images *trixie* and
*trixie.qcow2*::

    cloonix cli nemo demo wifi/cntkvm_monitor.py
    cloonix cli nemo ssh spy "tcpdump -n -i mon0 -c 20"
