#!/bin/bash
HERE=`pwd`

BUNDLE="/root/cloonix_bundle"
ROOTFS="${BUNDLE}/cloonfs"
TARGZ="${BUNDLE}/targz_store"

tar --directory=${ROOTFS} -xf ${TARGZ}/trixie_base_rootfs.tar.gz

#----------------------------------------------------------------------
for i in "log" "spool/rsyslog" "lib/cloonix/cache" \
         "lib/cloonix/bulk" "lib/cloonix/cache/fontconfig" ; do
  mkdir -p ${ROOTFS}/var/${i}
done

for i in "fonts/truetype" "terminfo" "mime" "misc" "dconf" \
         "locale" "i18n/charmaps" "i18n/locales" \
         "X11/locale" "vim/vim91" ; do 
  mkdir -p ${ROOTFS}/share/${i}
done

for i in "dev/net" "lib/modules" "bin/ovsschema" \
         "lib/x86_64-linux-gnu/qt6/plugins" ; do
  mkdir -p ${ROOTFS}/${i}
done
#----------------------------------------------------------------------
cp -Lrf /etc/fonts ${ROOTFS}/etc
rm -f ${ROOTFS}/etc/fonts/fonts.conf

cat > ${ROOTFS}/etc/fonts/fonts.conf << EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
    <dir>/usr/libexec/cloonix/cloonfs/share</dir>
    <cachedir>/var/lib/cloonix/cache/fontconfig</cachedir>
    <cachedir prefix="xdg">fontconfig</cachedir>
</fontconfig>
EOF
#----------------------------------------------------------------------
touch ${ROOTFS}/etc/adduser.conf
touch ${ROOTFS}/share/vim/vim91/defaults.vim
#----------------------------------------------------------------------
cp -Lrf /usr/lib/x86_64-linux-gnu/perl-base ${ROOTFS}/lib/x86_64-linux-gnu
cp -Lrf /usr/lib/x86_64-linux-gnu/perl5 ${ROOTFS}/lib/x86_64-linux-gnu
cp -Lrf /usr/share/perl5  ${ROOTFS}/share
cp -Lrf /usr/lib/locale/* ${ROOTFS}/lib/locale
cp -Lrf /usr/share/terminfo/l ${ROOTFS}/share/terminfo
cp -Lrf /usr/share/terminfo/t ${ROOTFS}/share/terminfo
cp -Lrf /usr/share/terminfo/x ${ROOTFS}/share/terminfo
cp -Lrf /usr/share/fonts/truetype/dejavu ${ROOTFS}/share/fonts/truetype
cp -Lrf /usr/share/fontconfig ${ROOTFS}/share
cp -Lrf /usr/share/X11/locale/en_US.UTF-8 ${ROOTFS}/share/X11/locale
cp -Lrf /usr/share/X11/locale/C ${ROOTFS}/share/X11/locale
cp -Lrf /usr/share/X11/locale/locale.dir ${ROOTFS}/share/X11/locale
cp -Lrf /usr/share/X11/xkb ${ROOTFS}/share/X11
cp  -f  /usr/share/X11/rgb.txt ${ROOTFS}/share/X11
cp -rfn /usr/lib/x86_64-linux-gnu/qt6/plugins/platforms ${ROOTFS}/lib/x86_64-linux-gnu/qt6/plugins
for i in "en_GB" "en" "locale.alias" ; do
  cp -Lrf /usr/share/locale/${i} ${ROOTFS}/share/locale
done
cp -f /usr/share/i18n/charmaps/UTF-8.gz ${ROOTFS}/share/i18n/charmaps
for i in "en_US" "en_GB" "i18n" "i18n_ctype" "iso14651_t1" \
         "iso14651_t1_common" "translit_*" ; do
  cp -f /usr/share/i18n/locales/${i} ${ROOTFS}/share/i18n/locales
done
cd ${ROOTFS}/share/i18n/charmaps
gunzip -f UTF-8.gz
#----------------------------------------------------------------------
cd ${WORK}
openssl req -x509 -nodes -newkey rsa:3072 -keyout novnc.pem -subj \
            "/CN=cloonix.net" -out ${ROOTFS}/bin/novnc.pem -days 3650
rm -f novnc.pem
#----------------------------------------------------------------------
cp -f /usr/bin/node ${ROOTFS}/bin
cp -Lrf /usr/share/nodejs ${ROOTFS}/share
cp -Lrf /usr/share/node_modules ${ROOTFS}/share
cp -Lrf /usr/lib/x86_64-linux-gnu/nodejs ${ROOTFS}/lib/x86_64-linux-gnu
tar --directory=${ROOTFS}/share -xf ${TARGZ}/noVNC_*.tar.gz
for i in "share/glib-2.0/schemas" \
         "lib/x86_64-linux-gnu/gstreamer-1.0" \
         "lib/x86_64-linux-gnu/gconv" ; do
  mkdir -p ${ROOTFS}/${i}
done
#----------------------------------------------------------------------
cp -Lrf /usr/lib/x86_64-linux-gnu/gtk-3.0 ${ROOTFS}/lib/x86_64-linux-gnu/
cp -Lrf /usr/share/themes ${ROOTFS}/share
cp -f /usr/share/mime/mime.cache ${ROOTFS}/share/mime
cp  /usr/share/glib-2.0/schemas/gschemas.compiled ${ROOTFS}/share/glib-2.0/schemas
cp -Lrf /usr/share/gstreamer-1.0 ${ROOTFS}/share
#----------------------------------------------------------------------
SRCD="/usr/lib/x86_64-linux-gnu"
DSTD="${ROOTFS}/lib/x86_64-linux-gnu"
for i in "gstreamer1.0" "pipewire-0.3" ; do
  cp -rf ${SRCD}/${i} ${DSTD}
done
for i in "libgstpulseaudio.so" \
         "libgstautodetect.so" \
         "libgstaudioresample.so" \
         "libgstaudioconvert.so" \
         "libgstcoreelements.so" \
         "libgstapp.so" ; do
  cp -rf ${SRCD}/gstreamer-1.0/${i} ${DSTD}/gstreamer-1.0
done
#----------------------------------------------------------------------
cp -f ${SRCD}/gconv/gconv-modules.cache ${DSTD}/gconv
#----------------------------------------------------------------------



