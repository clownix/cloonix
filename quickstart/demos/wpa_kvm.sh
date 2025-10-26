#!/bin/bash

#----------------------------------------------------------------------
cloonix_net nemo
#----------------------------------------------------------------------
cloonix_gui nemo
#----------------------------------------------------------------------
PARAMS="ram=1000 cpu=2"
cloonix_cli nemo add kvm kvm1 ${PARAMS} trixie.qcow2 --vwifi=1 &
cloonix_cli nemo add kvm kvm2 ${PARAMS} trixie.qcow2 --vwifi=1 &
cloonix_cli nemo add kvm kvm3 ${PARAMS} trixie.qcow2 --vwifi=1 &
#----------------------------------------------------------------------
set +e
for i in "kvm1" "kvm2" "kvm3" ; do
  while ! cloonix_ssh nemo ${i} "echo" 2>/dev/null; do
    echo ${i} not ready yet, waiting 1 sec
    sleep 1
  done
done
set -e
#----------------------------------------------------------------------
cloonix_ssh nemo kvm1 "cat > /etc/hostapd.conf << EOF
interface=wlan0
driver=nl80211
hw_mode=g
channel=1
ssid=mac80211_wpa
wpa=2
wpa_key_mgmt=WPA-PSK
wpa_pairwise=CCMP
wpa_passphrase=12345678
EOF"
#----------------------------------------------------------------------
cloonix_ssh nemo kvm2 "cat > /etc/wpa_supplicant.conf << EOF
ctrl_interface=/var/run/wpa_supplicant
network={
        ssid=\"mac80211_wpa\"
        psk=\"12345678\"
        key_mgmt=WPA-PSK
        proto=WPA2
        pairwise=CCMP
        group=CCMP
#dtim_period=1
#beacon_int=500
}
EOF"
#----------------------------------------------------------------------
cloonix_ssh nemo kvm3 "cat > /etc/wpa_supplicant.conf << EOF
ctrl_interface=/var/run/wpa_supplicant
network={
        ssid=\"mac80211_wpa\"
        psk=\"12345678\"
        key_mgmt=WPA-PSK
        proto=WPA2
        pairwise=CCMP
        group=CCMP
#dtim_period=1
#beacon_int=500
}
EOF"
#----------------------------------------------------------------------
cloonix_ssh nemo kvm2 "wpa_supplicant -B -Dnl80211 -iwlan0 -c /etc/wpa_supplicant.conf"
cloonix_ssh nemo kvm3 "wpa_supplicant -B -Dnl80211 -iwlan0 -c /etc/wpa_supplicant.conf"
cloonix_ssh nemo kvm1 "hostapd -B /etc/hostapd.conf"
#----------------------------------------------------------------------
sleep 10
#----------------------------------------------------------------------
cloonix_ssh nemo kvm1 "ip a a 10.0.0.1/8 dev wlan0"
cloonix_ssh nemo kvm2 "ip a a 10.0.0.2/8 dev wlan0"
cloonix_ssh nemo kvm3 "ip a a 10.0.0.3/8 dev wlan0"
#----------------------------------------------------------------------
cloonix_ssh nemo kvm2 "ping 10.0.0.3"
#----------------------------------------------------------------------



