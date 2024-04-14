#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
#----------------------------------------------------------------------#
NET=nemo
#----------------------------------------------------------------------#

#----------------------------------------------------------------------#
BIN_STORE="/media/perrier/Samsung_T5/bin_store_vlc"
VMNAME=vlc
QCOW2=mediamtx.qcow2
REPO="http://1.1.1.1/debian/bookworm"
REPO="http://172.17.0.2/debian/bookworm"
#######################################################################
set +e
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
cloonix_net ${NET}
sleep 2
set -e
cp -vf /var/lib/cloonix/bulk/bookworm_fr.qcow2 /var/lib/cloonix/bulk/${QCOW2}
sync
sleep 1
sync
#######################################################################
PARAMS="ram=3000 cpu=2 eth=v"
cloonix_cli ${NET} add kvm ${VMNAME} ${PARAMS} ${QCOW2} --persistent
#######################################################################
while ! cloonix_ssh ${NET} ${VMNAME} "echo"; do
  echo ${VMNAME} not ready, waiting 3 sec
  sleep 3
done

#-----------------------------------------------------------------------#
cloonix_scp ${NET} ${BIN_STORE}/nexium-theatre.mp4 ${VMNAME}:/home/user
cloonix_scp ${NET} ${BIN_STORE}/mediamtx_v1.6.0_linux_amd64.tar.gz ${VMNAME}:/home/user
cloonix_scp ${NET} ${BIN_STORE}/live555.tar.gz ${VMNAME}:/root
cloonix_scp ${NET} ${BIN_STORE}/vlc.tar.gz ${VMNAME}:/root
cloonix_ssh ${NET} ${VMNAME} "tar xvf live555.tar.gz"
cloonix_ssh ${NET} ${VMNAME} "tar xvf vlc.tar.gz"
cloonix_ssh ${NET} ${VMNAME} "rm /root/live555.tar.gz"
cloonix_ssh ${NET} ${VMNAME} "rm /root/vlc.tar.gz"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "cat > /etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${REPO} bookworm main contrib non-free
EOF"
#-----------------------------------------------------------------------#
cloonix_cli ${NET} add nat nat
cloonix_cli ${NET} add lan nat 0 lan_nat
cloonix_cli ${NET} add lan ${VMNAME} 0 lan_nat
cloonix_ssh ${NET} ${VMNAME} "dhclient eth0"
#######################################################################
cloonix_ssh ${NET} ${VMNAME} "DEBIAN_FRONTEND=noninteractive \
                        DEBCONF_NONINTERACTIVE_SEEN=true \
                        apt-get -o Dpkg::Options::=--force-confdef \
                        -o Acquire::Check-Valid-Until=false \
                        --allow-unauthenticated --assume-yes update"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "apt-get -o Dpkg::Options::=--force-confdef \
                         -o Acquire::Check-Valid-Until=false \
                         --allow-unauthenticated --assume-yes install \
                         rxvt-unicode chromium openssh-client openssh-server \
                         binutils iperf3 rsyslog lxde spice-vdagent \
                         pulseaudio pipewire ffmpeg"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "apt-get -o Dpkg::Options::=--force-confdef \
                           -o Acquire::Check-Valid-Until=false \
                           --allow-unauthenticated --assume-yes \
                           remove xscreensaver"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "cat >> /etc/lightdm/lightdm.conf << EOF
[SeatDefaults]
autologin-in-background=true
autologin-user=user
autologin-user-timeout=2
EOF"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "cat > /etc/xdg/lxsession/LXDE/autostart << EOF
@lxpanel --profile LXDE
@pcmanfm --desktop --profile LXDE
@xset s noblank
@xset s off
@xset -dpms
EOF"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "apt-get -o Dpkg::Options::=--force-confdef \
                         -o Acquire::Check-Valid-Until=false \
                         --allow-unauthenticated --assume-yes install \
                         autoconf build-essential pkg-config flex bison \
                         libssl-dev libtool gettext liblua5.2-dev lua5.2 \
                         libarchive-dev libdc1394-dev libraw1394-dev \
                         libavc1394-dev libzvbi-dev libogg-dev \
                         libdvdnav-dev libavutil-dev libavcodec-dev \
                         libavformat-dev libswscale-dev \
                         libflac-dev libmpeg2-4-dev libvorbis-dev \
                         libspeex-dev libtheora-dev libx11-xcb-dev \
                         libxcb-composite0-dev libxcb-randr0-dev \
                         libxcb-xkb-dev libxcb-shm0-dev libfreetype-dev \
                         libcaca-dev librtaudio-dev git \
                         libsoxr-dev libebur128-dev \
                         libchromaprint-dev libjack-jackd2-dev"

cloonix_ssh ${NET} ${VMNAME} "apt-get -o Dpkg::Options::=--force-confdef \
                         -o Acquire::Check-Valid-Until=false \
                         --allow-unauthenticated --assume-yes install \
                         libtag1-dev libdca-dev libmad0-dev libmpeg2-4-dev \
                         libavc1394-dev libraw1394-dev libdc1394-dev \
                         libdvdread-dev libdvdnav-dev libfaad-dev \
                         libtwolame-dev liba52-0.7.4-dev libvcdinfo-dev \
                         libiso9660-dev libcddb2-dev libflac-dev \
                         liba52-dev libogg-dev libvorbis-dev \
                         liblua5.1-0-dev libtag1-dev liblightdm-qt5-3-dev \
                         libqt5svg5-dev qtquickcontrols2-5-dev \
                         qtdeclarative5-dev libfreetype-dev libfreetype6-dev \
                         libxcursor-dev libxpm-dev libxext-dev libxinerama-dev \
                         qml-module-qtquick2 qml-module-qtquick-controls \
                         qml-module-qtquick-controls2 libshine-dev \
                         libaom-dev librav1e-dev libdav1d-dev libvpx-dev \
                         libfdk-aac-dev libspeexdsp-dev libpostproc-dev \
                         libopus-dev libx265-dev libxml2-dev libshout-dev \
                         libmpg123-dev libfontconfig-dev libgstreamer1.0-dev \
                         gstreamer1.0-plugins-base-apps gstreamer1.0-plugins-bad-apps \
                         libmediastreamer-dev libgstreamer-plugins-base1.0-dev \
                         libgstreamerd-3-dev libspatialaudio-dev libva-dev \
                         libresample1-dev libsamplerate0-dev libnotify-dev \
                         libsystemd-dev"

cloonix_ssh ${NET} ${VMNAME} "apt-get -o Dpkg::Options::=--force-confdef \
                         -o Acquire::Check-Valid-Until=false \
                         --allow-unauthenticated --assume-yes install \
                         baresip-core baresip-gstreamer baresip baresip-gtk \
                         baresip-x11 baresip-ffmpeg mediainfo-gui \
                         libmediainfo-dev"

cloonix_ssh ${NET} ${VMNAME} "mv /root/live555 /usr/lib/live"
cloonix_ssh ${NET} ${VMNAME} "cd /usr/lib/live; ./genMakefiles linux-with-shared-libraries"
cloonix_ssh ${NET} ${VMNAME} "cd /usr/lib/live; make -j4"
cloonix_ssh ${NET} ${VMNAME} "cd /usr/lib/live; make install"
for i in "BasicUsageEnvironment" "groupsock" "liveMedia" "UsageEnvironment"; do
  cloonix_ssh ${NET} ${VMNAME} "cp /usr/local/include/${i}/* /usr/local/include"
done
cloonix_ssh ${NET} ${VMNAME} "cp -r /usr/local/lib/* /usr/lib"

cloonix_ssh ${NET} ${VMNAME} "cd /root/vlc; ./bootstrap"

cloonix_ssh ${NET} ${VMNAME} "cd /root/vlc; \
     export LIVE555_CFLAGS=-I/usr/local/include; \
     export LIVE555_LIBS='-L/usr/local/lib -lBasicUsageEnvironment '; \
     export LIVE555_LIBS+='-lUsageEnvironment -lliveMedia -lgroupsock'; \
     ./configure --prefix=/usr --enable-mad --enable-a52 \
     --enable-dca --enable-libmpeg2 --enable-dvdnav --enable-faad \
     --enable-vorbis --enable-ogg --enable-theora \
     --enable-freetype --enable-fribidi --enable-speex \
     --enable-flac --enable-caca --enable-live555 \
     --disable-skins2 --enable-alsa --enable-ncurses \
     --enable-pulse --enable-qt --enable-qt-qml-cache \
     --enable-qt-qml-debug  --enable-skins2  --enable-chromaprint \
     --enable-shine --enable-aom --enable-rav1e --enable-vpx \
     --enable-twolame --enable-fdkaac --enable-vorbis \
     --enable-x265"


#./configure  '--build=x86_64-linux-gnu' '--prefix=/usr' '--includedir=${prefix}/include' '--mandir=${prefix}/share/man' '--infodir=${prefix}/share/info' '--sysconfdir=/etc' '--localstatedir=/var' '--disable-option-checking' '--disable-silent-rules' '--libdir=${prefix}/lib/x86_64-linux-gnu' '--runstatedir=/run' '--disable-maintainer-mode' '--disable-dependency-tracking' '--disable-debug' '--config-cache' '--disable-update-check' '--enable-fast-install' '--docdir=/usr/share/doc/vlc' '--with-binary-version=3.0.18-2' '--enable-a52' '--enable-aa' '--enable-aribsub' '--enable-avahi' '--enable-bluray' '--enable-caca' '--enable-chromaprint' '--enable-chromecast' '--enable-dav1d' '--enable-dbus' '--enable-dca' '--enable-dvbpsi' '--enable-dvdnav' '--enable-faad' '--enable-flac' '--enable-fluidsynth' '--enable-freetype' '--enable-fribidi' '--enable-gles2' '--enable-gnutls' '--enable-harfbuzz' '--enable-jack' '--enable-kate' '--enable-libass' '--enable-libmpeg2' '--enable-libxml2' '--enable-lirc' '--enable-mad' '--enable-matroska' '--enable-mod' '--enable-mpc' '--enable-mpg123' '--enable-mtp' '--enable-ncurses' '--enable-notify' '--enable-ogg' '--enable-opus' '--enable-pulse' '--enable-qt' '--enable-realrtsp' '--enable-samplerate' '--enable-sdl-image' '--enable-sftp' '--enable-shine' '--enable-shout' '--enable-skins2' '--enable-soxr' '--enable-spatialaudio' '--enable-speex' '--enable-srt' '--enable-svg' '--enable-svgdec' '--enable-taglib' '--enable-theora' '--enable-twolame' '--enable-upnp' '--enable-vdpau' '--enable-vnc' '--enable-vorbis' '--enable-x264' '--enable-x265' '--enable-zvbi' '--with-kde-solid=/usr/share/solid/actions/' '--disable-aom' '--disable-crystalhd' '--disable-d3d11va' '--disable-decklink' '--disable-directx' '--disable-dsm' '--disable-dxva2' '--disable-fdkaac' '--disable-fluidlite' '--disable-freerdp' '--disable-goom' '--disable-gst-decode' '--disable-libtar' '--disable-live555' '--disable-macosx' '--disable-macosx-avfoundation' '--disable-macosx-qtkit' '--disable-mfx' '--disable-microdns' '--disable-opencv' '--disable-projectm' '--disable-schroedinger' '--disable-sndio' '--disable-sparkle' '--disable-telx' '--disable-vpx' '--disable-vsxu' '--disable-wasapi' '--enable-alsa' '--enable-dc1394' '--enable-dv1394' '--enable-libplacebo' '--enable-linsys' '--enable-nfs' '--enable-udev' '--enable-v4l2' '--enable-wayland' '--enable-vcd' '--enable-smbclient' '--disable-oss' '--enable-mmx' '--enable-sse' '--disable-neon' '--disable-altivec' '--disable-omxil' 'build_alias=x86_64-linux-gnu' 'CFLAGS=-g -O2 -ffile-prefix-map=/build/vlc-WcXCHF/vlc-3.0.18=. -fstack-protector-strong -Wformat -Werror=format-security ' 'LDFLAGS=-Wl,-z,relro -Wl,-z,now' 'CPPFLAGS=-Wdate-time -D_FORTIFY_SOURCE=2' 'CXXFLAGS=-g -O2 -ffile-prefix-map=/build/vlc-WcXCHF/vlc-3.0.18=. -fstack-protector-strong -Wformat -Werror=format-security ' 'OBJCFLAGS=-g -O2 -ffile-prefix-map=/build/vlc-WcXCHF/vlc-3.0.18=. -fstack-protector-strong -Wformat -Werror=format-security'


cloonix_ssh ${NET} ${VMNAME} "git config --global --add safe.directory /root/vlc"

cloonix_ssh ${NET} ${VMNAME} "cd /root/vlc; make -j4"

cloonix_ssh ${NET} ${VMNAME} "cd /root/vlc; make install"

#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "systemctl disable networking.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable bluetooth.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable connman-wait-online.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable connman.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable dundee.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable ofono.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable e2scrub_reap.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable wpa_supplicant.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable remote-fs.target"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable apt-daily.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable apt-daily-upgrade.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable avahi-daemon.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable systemd-network-generator.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable systemd-networkd-wait-online.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable systemd-pstore.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable avahi-daemon.socket"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable dpkg-db-backup.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable e2scrub_all.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable fstrim.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable logrotate.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable man-db.timer"
cloonix_ssh ${NET} ${VMNAME} "rm -f /etc/resolv.conf"
cloonix_ssh ${NET} ${VMNAME} "systemctl enable systemd-networkd.service"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "cat > /home/user/vlc_rtsp_publish.sh << EOF
#!/bin/bash
export VLC_VERBOSE=2; cvlc file:///home/user/nexium-theatre.mp4 \
--h264-fps=10 \
--live-caching=0 \
--sout-transcode-high-priority \
--sout-all \
--sout-keep \
--sout=\"#transcode{vcodec=h264,vb=100,width=640,height=360}:rtp{sdp=rtsp://:8555/helico.mp4}\"
EOF"

cloonix_ssh ${NET} ${VMNAME} "cat > /home/user/vlc_rtsp_publish.sh << EOF
#!/bin/bash
export VLC_VERBOSE=2
cvlc file:///home/user/nexium-theatre.mp4 --mtu 500 --sout=\"#transcode{vcodec=h264,vb=40,width=320,height=180}:rtp{sdp=rtsp://:8555/helico.mp4}\"
EOF"


cloonix_ssh ${NET} ${VMNAME} "cat > /home/user/ffmpeg_rtsp_publish.sh << EOF
#!/bin/bash
ffmpeg -re -stream_loop -1 -i /home/user/nexium-theatre.mp4 -c copy -f rtsp rtsp://localhost:8554/mystream
EOF"

cloonix_ssh ${NET} ${VMNAME} "cat > /home/user/vlc_rtsp_read.sh << EOF
#!/bin/bash
vlc --network-caching=50 rtsp://localhost:8554/mystream
EOF"

for i in "vlc_rtsp_publish.sh" "ffmpeg_rtsp_publish.sh" "vlc_rtsp_read.sh"; do 
  cloonix_ssh ${NET} ${VMNAME} "chown user:user /home/user/${i}"
  cloonix_ssh ${NET} ${VMNAME} "chmod +x /home/user/${i}"
done

sleep 5
cloonix_ssh ${NET} ${VMNAME} "sync"
sleep 1
cloonix_ssh ${NET} ${VMNAME} "sync"
sleep 1
cloonix_ssh ${NET} ${VMNAME} "poweroff"
exit
sleep 10
set +e
cloonix_cli ${NET} kil
sleep 1
echo END ################################################"
#-----------------------------------------------------------------------#

