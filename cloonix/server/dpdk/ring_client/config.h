/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* If the C compiler is GCC 4.7 or later, define to the return value of
   __atomic_always_lock_free(1, 0). If the C compiler is not GCC or is an
   older version of GCC, the value does not matter. */
#define ATOMIC_ALWAYS_LOCK_FREE_1B 1

/* If the C compiler is GCC 4.7 or later, define to the return value of
   __atomic_always_lock_free(2, 0). If the C compiler is not GCC or is an
   older version of GCC, the value does not matter. */
#define ATOMIC_ALWAYS_LOCK_FREE_2B 1

/* If the C compiler is GCC 4.7 or later, define to the return value of
   __atomic_always_lock_free(4, 0). If the C compiler is not GCC or is an
   older version of GCC, the value does not matter. */
#define ATOMIC_ALWAYS_LOCK_FREE_4B 1

/* If the C compiler is GCC 4.7 or later, define to the return value of
   __atomic_always_lock_free(8, 0). If the C compiler is not GCC or is an
   older version of GCC, the value does not matter. */
#define ATOMIC_ALWAYS_LOCK_FREE_8B 1

/* System uses the DPDK module. */
#define DPDK_NETDEV 1

/* DPDK pdump enabled in OVS. */
/* #undef DPDK_PDUMP */

/* Define to 1 if building on ESX. */
/* #undef ESX */

/* Define to 1 if you have the <atomic> header file. */
/* #undef HAVE_ATOMIC */

/* Define to 1 if you have backtrace(3). */
#define HAVE_BACKTRACE 1

/* Define to 1 if you have the <bits/floatn-common.h> header file. */
#define HAVE_BITS_FLOATN_COMMON_H 1

/* Define to 1 if you have the `clock_gettime' function. */
#define HAVE_CLOCK_GETTIME 1

/* define if the compiler supports basic C++11 syntax */
#define HAVE_CXX11 1

/* Define to 1 if you have the declaration of `strerror_r', and to 0 if you
   don't. */
#define HAVE_DECL_STRERROR_R 1

/* Define to 1 if you have the declaration of `sys_siglist', and to 0 if you
   don't. */
#define HAVE_DECL_SYS_SIGLIST 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if the C compiler and linker supports the GCC 4.0+ atomic
   built-ins. */
#define HAVE_GCC4_ATOMICS 1

/* Define to 1 if you have the `getloadavg' function. */
#define HAVE_GETLOADAVG 1

/* Define to 1 if you have the `getmntent_r' function. */
#define HAVE_GETMNTENT_R 1

/* Define to 1 if pthread_setname_np() is available and takes 2 parameters
   (like glibc). */
#define HAVE_GLIBC_PTHREAD_SETNAME_NP 1

/* Define to 1 if net/if_dl.h is available. */
/* #undef HAVE_IF_DL */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if libcap-ng is available. */
/* #undef HAVE_LIBCAPNG */

/* Define to 1 if you have the `socket' library (-lsocket). */
/* #undef HAVE_LIBSOCKET */

/* Define to 1 if you have the <linux/if_ether.h> header file. */
#define HAVE_LINUX_IF_ETHER_H 1

/* Define to 1 if you have the <linux/net_namespace.h> header file. */
#define HAVE_LINUX_NET_NAMESPACE_H 1

/* Define to 1 if you have the <linux/perf_event.h> header file. */
#define HAVE_LINUX_PERF_EVENT_H 1

/* Define to 1 if you have the <linux/types.h> header file. */
#define HAVE_LINUX_TYPES_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mlockall' function. */
#define HAVE_MLOCKALL 1

/* Define to 1 if you have the <mntent.h> header file. */
#define HAVE_MNTENT_H 1

/* Define to 1 if pthread_setname_np() is available and takes 3 parameters
   (like NetBSD). */
/* #undef HAVE_NETBSD_PTHREAD_SETNAME_NP */

/* Define to 1 if Netlink protocol is available. */
#define HAVE_NETLINK 1

/* Define to 1 if you have the <net/if_mib.h> header file. */
/* #undef HAVE_NET_IF_MIB_H */

/* Define to 1 if OpenSSL is installed. */
/* #undef HAVE_OPENSSL */

/* Define to 1 if `posix_memalign' works. */
#define HAVE_POSIX_MEMALIGN 1

/* Define if compiler supports #pragma message directive */
#define HAVE_PRAGMA_MESSAGE 1

/* Define to 1 if you have the `pthread_set_name_np' function. */
/* #undef HAVE_PTHREAD_SET_NAME_NP */

/* Define to 1 if you have the `sendmmsg' function. */
#define HAVE_SENDMMSG 1

/* Define to 1 if you have the `statvfs' function. */
#define HAVE_STATVFS 1

/* Define to 1 if you have the <stdatomic.h> header file. */
#define HAVE_STDATOMIC_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strerror_r' function. */
#define HAVE_STRERROR_R 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strnlen' function. */
#define HAVE_STRNLEN 1

/* Define if strtok_r macro segfaults on some inputs */
/* #undef HAVE_STRTOK_R_BUG */

/* Define to 1 if `ifr_flagshigh' is a member of `struct ifreq'. */
/* #undef HAVE_STRUCT_IFREQ_IFR_FLAGSHIGH */

/* Define to 1 if `msg_len' is a member of `struct mmsghdr'. */
#define HAVE_STRUCT_MMSGHDR_MSG_LEN 1

/* Define to 1 if `sin6_scope_id' is a member of `struct sockaddr_in6'. */
#define HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID 1

/* Define to 1 if `st_mtimensec' is a member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_MTIMENSEC */

/* Define to 1 if `st_mtim.tv_nsec' is a member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC 1

/* Define to 1 if the system has the type `struct timespec'. */
/* #undef HAVE_STRUCT_TIMESPEC */

/* Define to 1 if you have the <sys/statvfs.h> header file. */
#define HAVE_SYS_STATVFS_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if TCA_FLOWER_KEY_ENC_IP_TTL_MASK is available. */
#define HAVE_TCA_FLOWER_KEY_ENC_IP_TTL_MASK 1

/* Define to 1 if TCA_PEDIT_KEY_EX_HDR_TYPE_UDP is available. */
#define HAVE_TCA_PEDIT_KEY_EX_HDR_TYPE_UDP 1

/* Define to 1 if TCA_TUNNEL_KEY_ENC_TTL is available. */
#define HAVE_TCA_TUNNEL_KEY_ENC_TTL 1

/* Define to 1 if TCA_VLAN_PUSH_VLAN_PRIORITY is available. */
#define HAVE_TCA_VLAN_PUSH_VLAN_PRIORITY 1

/* Define to 1 if the C compiler and linker supports the C11 thread_local
   matcro defined in <threads.h>. */
#define HAVE_THREAD_LOCAL 1

/* Define to 1 if unbound is detected. */
/* #undef HAVE_UNBOUND */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <valgrind/valgrind.h> header file. */
/* #undef HAVE_VALGRIND_VALGRIND_H */

/* Define to 1 if the C compiler and linker supports the GCC __thread
   extenions. */
/* #undef HAVE___THREAD */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "openvswitch"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "bugs@openvswitch.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "openvswitch"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "openvswitch 2.11.1"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "openvswitch"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.11.1"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if strerror_r returns char *. */
#define STRERROR_R_CHAR_P 1

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Version number of package */
#define VERSION "2.11.1"

/* NUMA Aware vHost support detected in DPDK. */
#define VHOST_NUMA 1

/* System uses the Visual Studio build target. */
#define VSTUDIO_DDK 1

/* Define to 1 if building on WIN32. */
/* #undef WIN32 */

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

#ifdef WIN32
#include "include/windows/windefs.h"
#endif
