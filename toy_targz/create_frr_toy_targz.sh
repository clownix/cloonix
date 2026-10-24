#!/bin/bash
# cloonix-frr-toy.tar.xz: the toy without kvm, frr_trixie merged into its
# root, the frr demos of routing/ and the examples of the FRR User Guide of
# frr_demo/.
NAME="frr"
IMAGE="frr_trixie"
KEEP="routing/frrspider.py routing/frrspider_bgp.py routing/frrmix.py"
. "$(dirname "$0")/lib/special_toy.sh"
