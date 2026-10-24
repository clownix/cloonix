/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
//http://wking.github.io/opencontainer-runtime-spec/
//https://github.com/opencontainers/runtime-spec/blob/main/config.md
//https://github.com/opencontainers/runtime-spec/blob/main/config-linux.md

#define CONFIG_JSON_MOUNT_ITEM "\n"\
"{\n"\
"  \"destination\": \"%s\",\n"\
"  \"type\": \"none\",\n"\
"  \"source\": \"%s\",\n"\
"  \"options\": [\"rbind\",\"rw\"]\n"\
"},\n"

#define CONFIG_JSON_SELF_ROOTFS "\n"\
"{\n"\
"  \"destination\": \"/var\",\n"\
"  \"type\": \"tmpfs\",\n"\
"  \"source\": \"tmpfs\",\n"\
"  \"options\": [\"rw\",\"mode=0755\"]\n"\
"},\n"\
"{\n"\
"  \"destination\": \"/etc/cloonix.cfg\",\n"\
"  \"type\": \"none\",\n"\
"  \"source\": \"/dev/null\",\n"\
"  \"options\": [\"bind\",\"ro\"]\n"\
"},\n"

#define CONFIG_JSON_MOUNT "\n"\
" \n %s %s %s %s %s %s %s %s\n"\
"{\n"\
"  \"destination\": \"/cloonixmnt\",\n"\
"  \"type\": \"none\",\n"\
"  \"source\": \"%s\",\n"\
"  \"options\": [\"rbind\",\"rw\"]\n"\
"},\n"\
"{\n"\
"  \"destination\": \"/run\",\n"\
"  \"type\": \"tmpfs\",\n"\
"  \"source\": \"tmpfs\",\n"\
"  \"options\": [\"rw\",\"nosuid\",\"nodev\",\"mode=777\"]\n"\
"},\n"\
"{\n"\
"  \"destination\": \"/run/lock\",\n"\
"  \"type\": \"tmpfs\",\n"\
"  \"source\": \"tmpfs\",\n"\
"  \"options\": [\"rw\",\"nosuid\",\"nodev\",\"mode=777\"]\n"\
"},\n"\
"%s \n"\
"{\n"\
"  \"destination\": \"/tmp\",\n"\
"  \"type\": \"tmpfs\",\n"\
"  \"source\": \"tmpfs\",\n"\
"  \"options\": [\"rw\",\"nosuid\",\"nodev\",\"mode=777\"]\n"\
"},\n"\
"{\n"\
"  \"destination\": \"/dev\",\n"\
"  \"type\": \"tmpfs\",\n"\
"  \"source\": \"tmpfs\",\n"\
"  \"options\": [\n"\
"          \"nosuid\",\n"\
"          \"strictatime\",\n"\
"          \"mode=755\"\n"\
"  ]\n"\
"},\n"\
"{\n"\
"  \"destination\": \"/dev/net\",\n"\
"  \"type\": \"tmpfs\",\n"\
"  \"source\": \"tmpfs\",\n"\
"  \"options\": [\n"\
"          \"nosuid\",\n"\
"          \"strictatime\",\n"\
"          \"mode=755\"\n"\
"  ]\n"\
"},\n"\
"{\n"\
"  \"destination\": \"/dev/shm\",\n"\
"  \"type\": \"tmpfs\",\n"\
"  \"source\": \"shm\",\n"\
"  \"options\": [\n"\
"          \"nosuid\",\n"\
"          \"noexec\",\n"\
"          \"nodev\",\n"\
"          \"mode=1777\"\n"\
"  ]\n"\
"},\n"\
"{\n"\
"  \"destination\": \"/dev/pts\",\n"\
"  \"type\": \"devpts\",\n"\
"  \"source\": \"devpts\",\n"\
"  \"options\": [\n"\
"          \"nosuid\",\n"\
"          \"noexec\",\n"\
"          \"newinstance\",\n"\
"          \"ptmxmode=0666\",\n"\
"          \"mode=0666\"\n"\
"  ]\n"\
"},\n"\
"{\n"\
"  \"destination\": \"/dev/mqueue\",\n"\
"  \"type\": \"mqueue\",\n"\
"  \"source\": \"mqueue\",\n"\
"  \"options\": [\"nosuid\",\"noexec\",\"nodev\"]\n"\
"},\n"\
"{\n"\
"  \"destination\": \"/sys\",\n"\
"  \"type\": \"sysfs\",\n"\
"  \"source\": \"sysfs\",\n"\
"  \"options\": [\"nosuid\",\"noexec\",\"nodev\"]\n"\
"},\n"\
"{\n"\
"  \"destination\": \"/sys/fs/cgroup\",\n"\
"  \"type\": \"cgroup2\",\n"\
"  \"source\": \"cgroup2\",\n"\
"  \"options\": [\"nosuid\",\"nodev\",\"noexec\"]\n"\
"},\n"\
"{\n"\
"  \"destination\": \"/proc\",\n"\
"  \"type\": \"proc\",\n"\
"  \"source\": \"proc\",\n"\
"  \"options\": [\"nosuid\",\"noexec\",\"nodev\"]\n"\
"},\n"\
"{\n"\
"  \"destination\": \"/cloonixmnt/tmp\",\n"\
"  \"type\": \"none\",\n"\
"  \"source\": \"%s\",\n"\
"  \"options\": [\"rbind\",\"mode=777\",\"rw\"]\n"\
"}\n"

#define CONFIG_JSON_CAPA "\"CAP_CHOWN\",\"CAP_DAC_OVERRIDE\",\n"\
"\"CAP_DAC_READ_SEARCH\",\"CAP_FOWNER\",\"CAP_FSETID\",\"CAP_KILL\",\n"\
"\"CAP_SETGID\",\"CAP_SETUID\",\"CAP_SETPCAP\",\"CAP_LINUX_IMMUTABLE\",\n"\
"\"CAP_NET_BIND_SERVICE\",\"CAP_NET_BROADCAST\",\"CAP_NET_ADMIN\",\n"\
"\"CAP_NET_RAW\",\"CAP_IPC_LOCK\",\"CAP_IPC_OWNER\",\"CAP_SYS_MODULE\",\n"\
"\"CAP_SYS_RAWIO\",\"CAP_SYS_CHROOT\",\"CAP_SYS_PTRACE\",\"CAP_SYS_PACCT\",\n"\
"\"CAP_SYS_ADMIN\",\"CAP_SYS_BOOT\",\"CAP_SYS_NICE\",\"CAP_SYS_RESOURCE\",\n"\
"\"CAP_SYS_TIME\",\"CAP_SYS_TTY_CONFIG\",\"CAP_MKNOD\",\"CAP_LEASE\",\n"\
"\"CAP_AUDIT_WRITE\",\"CAP_AUDIT_CONTROL\",\"CAP_SETFCAP\",\n"\
"\"CAP_MAC_OVERRIDE\",\"CAP_MAC_ADMIN\",\"CAP_SYSLOG\",\"CAP_WAKE_ALARM\",\n"\
"\"CAP_BLOCK_SUSPEND\",\"CAP_AUDIT_READ\",\"CAP_PERFMON\",\"CAP_BPF\",\n"\
"\"CAP_CHECKPOINT_RESTORE\"\n"

#define UID_GID_MAPPING "\"uidMappings\": [ \n"\
"     { \n"\
"         \"containerID\": %d, \n"\
"         \"hostID\": %d, \n"\
"         \"size\": %d \n"\
"     }, \n"\
"     { \n"\
"         \"containerID\": %d, \n"\
"         \"hostID\": %d, \n"\
"         \"size\": %d \n"\
"     } \n"\
" ], \n"\
" \"gidMappings\": [ \n"\
"     { \n"\
"         \"containerID\": %d, \n"\
"         \"hostID\": %d, \n"\
"         \"size\": %d \n"\
"     }, \n"\
"     { \n"\
"         \"containerID\": %d, \n"\
"         \"hostID\": %d, \n"\
"         \"size\": %d \n"\
"     } \n"\
" ], \n"

#define NAMESPACES_PRIVILEGED "\"namespaces\": [\n"\
"            {\"type\": \"network\"},\n"\
"            {\"type\": \"ipc\"},\n"\
"            {\"type\": \"uts\"},\n"\
"            {\"type\": \"pid\"},\n"\
"            {\"type\": \"cgroup\"},\n"\
"            {\"type\": \"mount\"}\n"\
"        ],\n"

#define NAMESPACES_NOT_PRIVILEGED "\"namespaces\": [\n"\
"            {\"type\": \"network\"},\n"\
"            {\"type\": \"ipc\"},\n"\
"            {\"type\": \"uts\"},\n"\
"            {\"type\": \"user\"},\n"\
"            {\"type\": \"pid\"},\n"\
"            {\"type\": \"cgroup\"},\n"\
"            {\"type\": \"mount\"}\n"\
"        ],\n"

#define MEMORY_NOT_LIMITED "\"resources\": {\n"\
"            \"memory\": {\n"\
"                \"limit\": -1,\n"\
"                \"reservation\": -1\n"\
"            }, \n"\
"            \"devices\": [\n"\
"                {\n"\
"                \"allow\": false,\n"\
"                \"access\": \"rwm\"\n"\
"                } \n"\
"            ]\n"\
"        },\n"

/* Order of the %s: uid, gid, args, env, the five capability vectors, rootfs
   path, uid/gid mappings, namespaces, resources, mounts. rp_filter 0: a new
   network namespace takes the host's, which drops what enters through an
   interface without address, a VRF device. */
#define CONFIG_MAIN_JSON "{\"ociVersion\": \"1.0.2\",\n"\
"    \"process\": {\n"\
"        \"terminal\": false,\n"\
"        \"user\": {\n"\
"            \"uid\": %d,\n"\
"            \"gid\": %d,\n"\
"            \"umask\": 0\n"\
"        },\n"\
"        \"args\": [%s],\n"\
"        \"env\": [\n"\
"        \"HOME=/root\",\n"\
"        \"TERM=xterm\",\n"\
"        \"PATH=/usr/sbin:/usr/bin:/sbin:/bin\"\n"\
"        %s\n"\
"        ],\n"\
"        \"cwd\": \"/\",\n"\
"        \"capabilities\": {\n"\
"            \"inheritable\": [%s],\n"\
"            \"bounding\": [%s],\n"\
"            \"effective\": [%s],\n"\
"            \"permitted\": [%s],\n"\
"            \"ambient\": [%s]\n"\
"        },\n"\
"        \"rlimits\": [\n"\
"            {\n"\
"            \"type\": \"RLIMIT_NOFILE\",\n"\
"            \"hard\": 1024,\n"\
"            \"soft\": 1024\n"\
"            }\n"\
"        ],\n"\
"        \"noNewPrivileges\": true\n"\
"    },\n"\
"    \"root\": {\n"\
"        \"path\": \"%s\"\n"\
"        },\n"\
"    \"hostname\": \"crun\",\n"\
"    \"linux\": {\n"\
"        %s \n"\
"        \"sysctl\": {\n"\
"            \"net.ipv4.conf.all.rp_filter\": \"0\",\n"\
"            \"net.ipv4.conf.default.rp_filter\": \"0\"\n"\
"        },\n"\
"        \"rootfsPropagation\": \"private\",\n"\
"        %s    \n"\
"        %s\n"\
"       \"maskedPaths\": [\n"\
"           \"/proc/acpi\",\n"\
"           \"/proc/asound\",\n"\
"           \"/proc/kcore\",\n"\
"           \"/proc/keys\",\n"\
"           \"/proc/latency_stats\",\n"\
"           \"/proc/timer_list\",\n"\
"           \"/proc/timer_stats\",\n"\
"           \"/proc/sched_debug\",\n"\
"           \"/sys/firmware\",\n"\
"           \"/proc/scsi\"\n"\
"       ],\n"\
"           \"readonlyPaths\": [\n"\
"           \"/proc/bus\",\n"\
"           \"/proc/fs\",\n"\
"           \"/proc/irq\",\n"\
"           \"/proc/sysrq-trigger\"\n"\
"       ]\n"\
"    },\n"\
"    \"mounts\": [ %s ]\n"\
"}"
