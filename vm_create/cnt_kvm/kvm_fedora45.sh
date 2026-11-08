#!/bin/bash
# Fedora 45 qcow2 image, built by kvm_fedora.sh.
exec "$(dirname "$0")/kvm_fedora.sh" 45 "$@"
