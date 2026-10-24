#!/bin/bash -x
DISTRO=trixie

hote="ftp.fr.debian.org"
racine="debian/"
methode="http"
distribution="${DISTRO}"
sections="main,contrib,non-free,non-free-firmware"
cible="/media/perrier/T7Shield2/distros/${DISTRO}"
architecture="amd64"
/usr/bin/debmirror --passive --no-check-gpg \
                   --ignore-small-errors \
                   --method=$methode --root=$racine \
                   --dist=$distribution --host=$hote \
                   --section=$sections --arch=$architecture $cible \
                   --getcontents --ignore-release-gpg --progress --i18n

