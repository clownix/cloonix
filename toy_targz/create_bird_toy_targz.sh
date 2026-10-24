#!/bin/bash
# cloonix-bird-toy.tar.xz: the toy without kvm, bird_trixie merged into its
# root, the bird demo of routing/ and the examples of the BIRD User's Guide
# of bird_demo/.
NAME="bird"
IMAGE="bird_trixie"
KEEP="routing/bird.py"
. "$(dirname "$0")/lib/special_toy.sh"
