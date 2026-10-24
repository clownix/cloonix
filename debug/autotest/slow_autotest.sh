#!/bin/bash
# slow_autotest.sh [<distro>] - the whole non-regression in two kvm of nemo,
# about 3 hours: top_testitall.sh (bundle, kvm1), top_testitalltoy.sh (toy,
# kvm2), then endless tst_max.sh and tst_maxtoy.sh. The machine:
# kvm_autotest_<distro>.qcow2, trixie by default, from ./kvm_autotest_<distro>.sh.
#   ./slow_autotest.sh                kvm_autotest_trixie.qcow2
#   ./slow_autotest.sh rocky          kvm_autotest_rocky.qcow2
#   https://10.9.9.1:4444  https://10.9.9.2:4444     nemo in each, 4454 mito
# Needs in ${HOME} cloonix-bundle.tar.gz and cloonix-toy.tar.xz, the images in
# bulk, nested kvm, and sudo for autotap.
HERE=$(cd "$(dirname "$0")" && pwd)
. "${HERE}/autotest_common.sh"
IMAGES=${AUTOTEST_IMAGES:-"trixie.qcow2 trixie perf_trixie flow_trixie trixie_systemd"}
# Demo images pushed to the guest bulk, each optional (missing: its demo
# skips). Left out: ebpf (needs the initial userns), python_trixie (a third
# kvm level), freebsd, windows and cisco.
DEMO_IMAGES=${AUTOTEST_DEMO_IMAGES:-"wg_trixie batman_trixie ovn_trixie \
openvpn_trixie tinc_trixie strongswan_trixie libreswan_trixie nft_trixie \
arp_trixie frr_trixie bird_trixie babel_trixie dns_trixie unbound_trixie \
knot_trixie dhcp_trixie ntp_trixie proxy_trixie squid_trixie smtp_trixie \
mqtt_trixie asterisk_alpine suricata_trixie radius_trixie vrrp_trixie \
cluster_trixie"}
# The tests run first in each kvm, the latest work: a failure there is
# known in minutes instead of hours; the whole suite runs after them.
AUTOTEST_SUBS_FIRST=${AUTOTEST_SUBS_FIRST-"sub_cntwifi.py sub_kvmwifi.py sub_webshark_wifi.py sub_web_items.py"}
AUTOTEST_DEMOS_FIRST=${AUTOTEST_DEMOS_FIRST-"wifi"}
FIRST="TESTITALL_SUBS_FIRST=\"${AUTOTEST_SUBS_FIRST}\" TESTITALL_DEMOS_FIRST=\"${AUTOTEST_DEMOS_FIRST}\""

autotest_prepare "$@"
# The expected totals for the time stamps are the durations last seen, the
# suite 2h53, the toy suite 2h49; update them when they drift.
runner_write ${KVM1} top_testitall.sh "${FIRST} TESTITALL_NO_ENDURANCE=1 ./top_testitall.sh" 10400
runner_endurance tst_max.sh
runner_start ${KVM1}
runner_write ${KVM2} top_testitalltoy.sh "${FIRST} ./top_testitalltoy.sh" 10100
runner_endurance tst_maxtoy.sh
runner_start ${KVM2}
autotest_banner
echo " after a suite that passed, the endurance runs with no end:"
echo "cloonix cli ${NET} ssh ${KVM1} \"tail -f /home/user/tst_max_output.log\""
echo "cloonix cli ${NET} ssh ${KVM2} \"tail -f /home/user/tst_max_output.log\""
echo "======================================================================"
