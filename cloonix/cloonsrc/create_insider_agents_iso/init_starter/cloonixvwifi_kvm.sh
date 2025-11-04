#!/bin/bash
#----------------------------------------------------------------------
modprobe mac80211_hwsim radios=0
sleep 2
/cloonixmnt/cnf_fs/cloonix-vwifi-add-interfaces $1 $2
sleep 1
nohup /cloonixmnt/cnf_fs/cloonix-vwifi-client &
#----------------------------------------------------------------------

