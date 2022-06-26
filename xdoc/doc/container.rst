.. image:: /png/cloonix128.png 
   :align: right

==============================
Details on containers creation
==============================

The containers are made from scratch with a linux system and with the
help of the **crun** https://github.com/containers/crun software.
Following are the technical bases to have crun containers within
cloonix.


Container image create
======================

The first step is the building of the image file that will be used in cloonix.
For this, the debootstrap-1.0.126+nmu1 has been downloaded.
The following commands does the building of the image::

    dd if=/dev/zero of=bookworm.img bs=100M count=10
    mkfs.ext4 bookworm.img
    losetup -fP bookworm.img
    mkdir -p /root/tmp_mnt
    DEVLOOP=$(losetup -l | grep bookworm.img | awk '{print $1}')
    echo $DEVLOOP
    mount -o loop $DEVLOOP /root/tmp_mnt
    export DEBOOTSTRAP_DIR=/root/debootstrap-1.0.126+nmu1
    INCLUDES="openssh-client,vim,bash-completion,net-tools,tcpdump,tini"
    ${DEBOOTSTRAP_DIR}/debootstrap  \
                   --no-check-certificate \
                   --no-check-gpg \
                   --arch amd64 \
                   --include=${INCLUDES} \
                   bookworm \
                   /root/tmp_mnt \
                   http://deb.debian.org/debian
    umount /root/tmp_mnt
    losetup -d $DEVLOOP
    rmdir tmp_mnt


Container copy-on-write rootfs
==============================

With the image file created above, a particular way to mount it can be used
to have "Copy On Write" feature, that means the original file is never
writen upon but all the file-system deltas are saved in sub-directories, the
whole thing giving the impression of a writable root file-system.
The following commands are a way to test this rootfs creation::

    losetup -fP bookworm.img
    DEVLOOP=$(losetup -l | grep bookworm.img | awk '{print $1}')
    mkdir -p /root/overlay/{lower,upper,workdir,rootfs}
    mount -o loop $DEVLOOP /root/overlay/lower
    LW="/root/overlay/lower"
    UP="/root/overlay/upper"
    WK="/root/overlay/workdir"
    RT="/root/overlay/rootfs"
    mount none -t overlay -o lowerdir=${LW},upperdir=${UP},workdir=${WK} ${RT}

Note that lower is not writen, workdir is used before atomic copy to upper, and
upper is written with delta of rootfs from template image mounted on lower.


Container private network create
================================

Now that the file-system has been taken care of, we have to have an isolated
ip stack for our container, for this, the namespaces are used, the following
commands create a namespace cloonix_1_1, creates a veth pair (vgt_1_1_0,eth0)
and sends the eth0 one in the cloonix_1_1 namespace. This namespace will be
put in the config.json file of the crun and that will give the eth0 interface
to the new container machine. Do the following commands to have the namespace::

    ip netns ls
    ip netns add cloonix_1_1
    ip link add name vgt_1_1_0 type veth peer name eth0
    ip link set eth0 netns cloonix_1_1
    ip netns exec cloonix_1_1 ip link set lo up
    ip netns exec cloonix_1_1 ip link set eth0 up


Container config creation
=========================

The crun software takes a config.json file as input, to help having a template
for this config, the command "crun spec" creates the config.
In cloonix, the config_json.h file contains the template.
In your test case, the adaptation of the config can be done then by copy-paste
of the following commands::

    crun spec
    sed -i s"%\"path\": \(.*\)%\"path\": \"/root/overlay/rootfs\",%" config.json
    sed -i s"/\"readonly\": \(.*\)/\"readonly\": \"false\"/" config.json
    sed -i s"%\"network\"%\"network\",\n\t\t\t\t\"path\": \"/var/run/netns/cloonix_1_1\"%" config.json
    sed -i s"/\"terminal\": \(.*\)/\"terminal\": \"false\",/" config.json
    sed -i s"%\"CAP_KILL\",%\"CAP_KILL\",\n\t\t\t\t\"CAP_NET_RAW\",%" config.json
    sed -i s"%\"CAP_KILL\",%\"CAP_KILL\",\n\t\t\t\t\"CAP_NET_ADMIN\",%" config.json

Container run
=============

Run your test container if all the above steps have been done, it should work::

    crun list
    crun create --config=/root/config.json Cloon1
    crun list
    crun exec Cloon1 bash
    crun list

Clean your experiment
=====================

After your tests of the container, you can clean it with the following::

    crun list
    kill -9 <pid>
    crun list
    crun delete Cloon1
    crun list



All those techniques are used within cloonix to run an image file, the probleme now is to
run the services you wish to have in your containers, for the moment it is a work in progress.

