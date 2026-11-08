#!/bin/bash
# Mirror OpenWrt x86/64 (images, target packages, kmods) and the x86_64 feeds
# of one release branch into the openwrt tree, updated in place: the newest
# point release of the branch replaces the old one.
. "$(dirname "$0")/distro_common.sh"
VER=25.12
# rsync.openwrt.org serves tier-1 mirrors only since 2026-01; tetaneutral is
# a public tier-2.
UP=rsync://openwrt.tetaneutral.net/openwrt/releases
if ! LIST=$(rsync --list-only --contimeout=30 --timeout=600 "${UP}/"); then
  echo "FATAL: ${UP}/ cannot be listed; the module is seen with:"
  echo "rsync --list-only ${UP%/releases}/"
  exit 1
fi
# Newest ${VER}.<N>, never an -rc.
POINT=$(echo "${LIST}" \
        | awk -v b="${VER}." '{n=$NF} index(n,b)==1 && substr(n,length(b)+1) ~ /^[0-9]+$/ {print n}' \
        | sort -V | tail -1)
[ -n "${POINT}" ] || { echo "FATAL: no ${VER}.<N> release in ${UP}/"; exit 1; }
# Build and debug material, used neither to make nor to run a guest.
mirror_rsync "${UP}/${POINT}/targets/x86/64/" openwrt/targets/x86/64 \
             --exclude='openwrt-sdk-*' --exclude='openwrt-toolchain-*' \
             --exclude='llvm-bpf-*' --exclude='kernel-debug.tar.zst'
mirror_rsync "${UP}/packages-${VER}/x86_64/" openwrt/packages/x86_64
need openwrt/targets/x86/64/packages/packages.adb
need openwrt/packages/x86_64/base/packages.adb
need "openwrt/targets/x86/64/openwrt-${POINT}-x86-64-rootfs.tar.gz"
finish "openwrt ${POINT}"
