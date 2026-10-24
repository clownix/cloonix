#!/bin/bash
# cloonix-suricata-toy.tar.xz: the toy without kvm, suricata_trixie merged
# into its root, the suricata demo of security/ and the examples of the
# Suricata User Guide of suricata_demo/.
NAME="suricata"
IMAGE="suricata_trixie"
KEEP="security/suricata.py"
. "$(dirname "$0")/lib/special_toy.sh"
