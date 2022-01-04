#define CONFIG_JSON "  {\n"\
"	\"ociVersion\": \"1.0.0\",\n"\
"	\"process\": {\n"\
"		\"terminal\": \"false\",\n"\
"		\"user\": {\n"\
"			\"uid\": 0,\n"\
"			\"gid\": 0\n"\
"		},\n"\
"               \"args\": [\n"\
"                       \"/cloonix_parrot_srv\"\n"\
"               ],\n"\
"		\"env\": [\n"\
"			\"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin\",\n"\
"			\"TERM=xterm\"\n"\
"		],\n"\
"		\"cwd\": \"/\",\n"\
"		\"capabilities\": {\n"\
"			\"bounding\": [\n"\
"				\"CAP_AUDIT_WRITE\",\n"\
"				\"CAP_KILL\",\n"\
"				\"CAP_NET_ADMIN\",\n"\
"				\"CAP_NET_RAW\",\n"\
"				\"CAP_NET_BIND_SERVICE\"\n"\
"			],\n"\
"			\"effective\": [\n"\
"				\"CAP_AUDIT_WRITE\",\n"\
"				\"CAP_KILL\",\n"\
"				\"CAP_NET_ADMIN\",\n"\
"				\"CAP_NET_RAW\",\n"\
"				\"CAP_NET_BIND_SERVICE\"\n"\
"			],\n"\
"			\"inheritable\": [\n"\
"				\"CAP_AUDIT_WRITE\",\n"\
"				\"CAP_KILL\",\n"\
"				\"CAP_NET_ADMIN\",\n"\
"				\"CAP_NET_RAW\",\n"\
"				\"CAP_NET_BIND_SERVICE\"\n"\
"			],\n"\
"			\"permitted\": [\n"\
"				\"CAP_AUDIT_WRITE\",\n"\
"				\"CAP_KILL\",\n"\
"				\"CAP_NET_ADMIN\",\n"\
"				\"CAP_NET_RAW\",\n"\
"				\"CAP_NET_BIND_SERVICE\"\n"\
"			],\n"\
"			\"ambient\": [\n"\
"				\"CAP_AUDIT_WRITE\",\n"\
"				\"CAP_KILL\",\n"\
"				\"CAP_NET_ADMIN\",\n"\
"				\"CAP_NET_RAW\",\n"\
"				\"CAP_NET_BIND_SERVICE\"\n"\
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
"		\"resources\": {\n"\
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
"		\"readonlyPaths\": [\n"\
"			\"/proc/bus\",\n"\
"			\"/proc/fs\",\n"\
"			\"/proc/irq\",\n"\
"			\"/proc/sys\",\n"\
"			\"/proc/sysrq-trigger\"\n"\
"		]\n"\
"	}\n"\
"}"
