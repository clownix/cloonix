#!/bin/bash
# cloonix-bind-toy.tar.xz: the toy without kvm, dns_trixie merged into its
# root, the dns demo of address/ and the examples of the BIND 9
# Administrator Reference Manual of bind_demo/.
NAME="bind"
IMAGE="dns_trixie"
KEEP="address/dns.py"
. "$(dirname "$0")/lib/special_toy.sh"
