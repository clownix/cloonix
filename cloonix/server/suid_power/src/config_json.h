#define CONFIG_JSON_CAPA "\"CAP_CHOWN\",\n"\
"                         \"CAP_DAC_OVERRIDE\",\n"\
"                         \"CAP_DAC_READ_SEARCH\",\n"\
"                         \"CAP_FOWNER\",\n"\
"                         \"CAP_FSETID\",\n"\
"                         \"CAP_KILL\",\n"\
"                         \"CAP_SETGID\",\n"\
"                         \"CAP_SETUID\",\n"\
"                         \"CAP_SETPCAP\",\n"\
"                         \"CAP_LINUX_IMMUTABLE\",\n"\
"                         \"CAP_NET_BIND_SERVICE\",\n"\
"                         \"CAP_NET_BROADCAST\",\n"\
"                         \"CAP_NET_ADMIN\",\n"\
"                         \"CAP_NET_RAW\",\n"\
"                         \"CAP_IPC_LOCK\",\n"\
"                         \"CAP_IPC_OWNER\",\n"\
"                         \"CAP_SYS_MODULE\",\n"\
"                         \"CAP_SYS_RAWIO\",\n"\
"                         \"CAP_SYS_CHROOT\",\n"\
"                         \"CAP_SYS_PTRACE\",\n"\
"                         \"CAP_SYS_PACCT\",\n"\
"                         \"CAP_SYS_ADMIN\",\n"\
"                         \"CAP_SYS_BOOT\",\n"\
"                         \"CAP_SYS_NICE\",\n"\
"                         \"CAP_SYS_RESOURCE\",\n"\
"                         \"CAP_SYS_TIME\",\n"\
"                         \"CAP_SYS_TTY_CONFIG\",\n"\
"                         \"CAP_MKNOD\",\n"\
"                         \"CAP_LEASE\",\n"\
"                         \"CAP_AUDIT_WRITE\",\n"\
"                         \"CAP_AUDIT_CONTROL\",\n"\
"                         \"CAP_SETFCAP\",\n"\
"                         \"CAP_MAC_OVERRIDE\",\n"\
"                         \"CAP_MAC_ADMIN\",\n"\
"                         \"CAP_SYSLOG\",\n"\
"                         \"CAP_WAKE_ALARM\",\n"\
"                         \"CAP_BLOCK_SUSPEND\",\n"\
"                         \"CAP_AUDIT_READ\",\n"\
"                         \"CAP_PERFMON\",\n"\
"                         \"CAP_BPF\",\n"\
"                         \"CAP_CHECKPOINT_RESTORE\"\n"


#define CONFIG_JSON "  {\n"\
"	\"ociVersion\": \"1.0.0\",\n"\
"	\"process\": {\n"\
"		\"terminal\": \"false\",\n"\
"		\"user\": {\n"\
"			\"uid\": 0,\n"\
"			\"gid\": 0\n"\
"		},\n"\
"               \"args\": [\n"\
"                       %s\n"\
"               ],\n"\
"		\"env\": [\n"\
"			\"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin\",\n"\
"			\"TERM=xterm\"\n"\
"		],\n"\
"		\"cwd\": \"/\",\n"\
"		\"capabilities\": {\n"\
"			\"bounding\": [\n"\
"				%s\n"\
"			],\n"\
"			\"effective\": [\n"\
"				%s\n"\
"			],\n"\
"			\"inheritable\": [\n"\
"				%s\n"\
"			],\n"\
"			\"permitted\": [\n"\
"				%s\n"\
"			],\n"\
"			\"ambient\": [\n"\
"				%s\n"\
"			]\n"\
"		},\n"\
"		\"rlimits\": [\n"\
"			{\n"\
"				\"type\": \"RLIMIT_NOFILE\",\n"\
"				\"hard\": 1024,\n"\
"				\"soft\": 1024\n"\
"			}\n"\
"		],\n"\
"		\"noNewPrivileges\": true\n"\
"	},\n"\
"	\"root\": {\n"\
"		\"path\": \"%s\",\n"\
"		\"readonly\": \"false\"\n"\
"	},\n"\
"	\"hostname\": \"crun\",\n"\
"	\"mounts\": [\n"\
"               {\n"\
"                       \"destination\": \"/mnt\",\n"\
"                       \"type\": \"none\",\n"\
"                       \"source\": \"%s\",\n"\
"                       \"options\": [\"rbind\",\"rw\"]\n"\
"               },\n"\
"               {\n"\
"                       \"destination\": \"/lib/modules\",\n"\
"                       \"type\": \"none\",\n"\
"                       \"source\": \"/lib/modules\",\n"\
"                       \"options\": [\"rbind\",\"ro\"]\n"\
"               },\n"\
"		{\n"\
"			\"destination\": \"/proc\",\n"\
"			\"type\": \"proc\",\n"\
"			\"source\": \"proc\"\n"\
"		},\n"\
"		{\n"\
"			\"destination\": \"/dev\",\n"\
"			\"type\": \"tmpfs\",\n"\
"			\"source\": \"tmpfs\",\n"\
"			\"options\": [\n"\
"				\"nosuid\",\n"\
"				\"strictatime\",\n"\
"				\"mode=755\",\n"\
"				\"size=65536k\"\n"\
"			]\n"\
"		},\n"\
"		{\n"\
"			\"destination\": \"/dev/pts\",\n"\
"			\"type\": \"devpts\",\n"\
"			\"source\": \"devpts\",\n"\
"			\"options\": [\n"\
"				\"nosuid\",\n"\
"				\"noexec\",\n"\
"				\"newinstance\",\n"\
"				\"ptmxmode=0666\",\n"\
"				\"mode=0620\",\n"\
"				\"gid=5\"\n"\
"			]\n"\
"		},\n"\
"		{\n"\
"			\"destination\": \"/dev/shm\",\n"\
"			\"type\": \"tmpfs\",\n"\
"			\"source\": \"shm\",\n"\
"			\"options\": [\n"\
"				\"nosuid\",\n"\
"				\"noexec\",\n"\
"				\"nodev\",\n"\
"				\"mode=1777\",\n"\
"				\"size=65536k\"\n"\
"			]\n"\
"		},\n"\
"		{\n"\
"			\"destination\": \"/dev/mqueue\",\n"\
"			\"type\": \"mqueue\",\n"\
"			\"source\": \"mqueue\",\n"\
"			\"options\": [\n"\
"				\"nosuid\",\n"\
"				\"noexec\",\n"\
"				\"nodev\"\n"\
"			]\n"\
"		},\n"\
"		{\n"\
"			\"destination\": \"/sys\",\n"\
"			\"type\": \"sysfs\",\n"\
"			\"source\": \"sysfs\",\n"\
"			\"options\": [\n"\
"				\"nosuid\",\n"\
"				\"noexec\",\n"\
"				\"nodev\",\n"\
"				\"ro\"\n"\
"			]\n"\
"		},\n"\
"		{\n"\
"			\"destination\": \"/sys/fs/cgroup\",\n"\
"			\"type\": \"cgroup\",\n"\
"			\"source\": \"cgroup\",\n"\
"			\"options\": [\n"\
"				\"nosuid\",\n"\
"				\"noexec\",\n"\
"				\"nodev\",\n"\
"				\"relatime\",\n"\
"				\"ro\"\n"\
"			]\n"\
"		}\n"\
"	],\n"\
"	\"linux\": {\n"\
"		\"device\": [\n"\
"			{\n"\
"				\"path\": \"/dev/net/tun\",\n"\
"				\"type\": \"c\",\n"\
"				\"major\": 10,\n"\
"				\"minor\": 200,\n"\
"				\"fileMode\": 438,\n"\
"				\"uid\": 0,\n"\
"				\"gid\": 0\n"\
"			}\n"\
"		],\n"\
"		\"resources\": {\n"\
"				\"allow\": true,\n"\
"				\"access\": \"rwm\"\n"\
"		},\n"\
"		\"namespaces\": [\n"\
"			{\n"\
"				\"type\": \"pid\"\n"\
"			},\n"\
"			{\n"\
"				\"type\": \"network\",\n"\
"				\"path\": \"%s\"\n"\
"			},\n"\
"			{\n"\
"				\"type\": \"ipc\"\n"\
"			},\n"\
"			{\n"\
"				\"type\": \"uts\"\n"\
"			},\n"\
"			{\n"\
"				\"type\": \"mount\"\n"\
"			}\n"\
"		],\n"\
"		\"maskedPaths\": [\n"\
"			\"/proc/acpi\",\n"\
"			\"/proc/asound\",\n"\
"			\"/proc/kcore\",\n"\
"			\"/proc/keys\",\n"\
"			\"/proc/latency_stats\",\n"\
"			\"/proc/timer_list\",\n"\
"			\"/proc/timer_stats\",\n"\
"			\"/proc/sched_debug\",\n"\
"			\"/sys/firmware\",\n"\
"			\"/proc/scsi\"\n"\
"		],\n"\
"		\"sysctl\": {\n"\
"                          \"net.ipv4.ip_forward\": \"1\",\n"\
"                          \"net.ipv6.conf.all.disable_ipv6\": \"1\",\n"\
"                          \"net.ipv6.conf.default.disable_ipv6\": \"1\"\n"\
"                          },\n"\
"		\"readonlyPaths\": [\n"\
"			\"/proc/bus\",\n"\
"			\"/proc/fs\",\n"\
"			\"/proc/irq\",\n"\
"			\"/proc/sys\",\n"\
"			\"/proc/sysrq-trigger\"\n"\
"		]\n"\
"	}\n"\
"}"
