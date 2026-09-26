/* stub pcap/pcap.h for webshark/dumpcap build — only struct pcap_pkthdr is needed */
#ifndef __PCAP_PCAP_H__
#define __PCAP_PCAP_H__
#include <stdint.h>
#include <sys/time.h>
typedef uint32_t bpf_u_int32;
struct pcap_pkthdr {
    struct timeval ts;
    bpf_u_int32    caplen;
    bpf_u_int32    len;
};
#endif

/* DLT constants needed by dumpcap */
#define DLT_NULL    0
#define DLT_FDDI    10
#define PCAP_ERRBUF_SIZE 256
