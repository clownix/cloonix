#!/bin/bash
# cloonix-asterisk-toy.tar.xz: the toy without kvm, asterisk_trixie merged into
# its root, the asterisk demo of services/ and the pages of the Asterisk
# documentation, with SIPp and Kamailio, of asterisk_demo/.
NAME="asterisk"
IMAGE="asterisk_trixie"
KEEP="services/asterisk.py"
. "$(dirname "$0")/lib/special_toy.sh"
