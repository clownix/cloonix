#!/bin/bash
releasever=44
basearch=x86_64
MIRROR=rsync://ftp.fau.de/fedora/linux
FEDORA_MAIN=${MIRROR}/releases/${releasever}/Everything/${basearch}
FEDORA_MAIN_UPDATES=${MIRROR}/updates/${releasever}/Everything/${basearch}
DISKREPO=/media/perrier/T7Shield2/distros/fedora${releasever}
RSYNC_OPTS="-avH --delete-delay --no-perms --no-owner --no-group --chmod=ugo=rwX"

mkdir -p ${DISKREPO}/releases/${releasever}/Everything/${basearch}
mkdir -p ${DISKREPO}/updates/${releasever}/Everything

# releases: only os/ (siblings debug/ and iso/ are not wanted).
rsync ${RSYNC_OPTS} ${FEDORA_MAIN}/os ${DISKREPO}/releases/${releasever}/Everything/${basearch}/
# updates: debug/ excluded; an excluded path is never deleted locally.
rsync ${RSYNC_OPTS} --exclude=debug/ ${FEDORA_MAIN_UPDATES} ${DISKREPO}/updates/${releasever}/Everything/
