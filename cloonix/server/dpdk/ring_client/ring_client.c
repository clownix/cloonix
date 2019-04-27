
#include "rte_config.h"
#include <getopt.h>
#include <rte_net.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_flow.h>

#include <rte_config.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_string_fns.h>
#include <rte_ip.h>
#include <rte_byteorder.h>
#include <unistd.h>


#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1
#define MP_CLIENT_RXQ_NAME "dpdkr%u_tx"
#define PKT_READ_SIZE  ((uint16_t)1)

#define RTE_BE_TO_CPU_16(be_16_v)  rte_be_to_cpu_16((be_16_v))
#define RTE_CPU_TO_BE_16(cpu_16_v) rte_cpu_to_be_16((cpu_16_v))


static unsigned int client_id;

static int str_to_llong_with_tail(const char *s, char **tail, int base, long long *x)
{
    int save_errno = errno;
    errno = 0;
    *x = strtoll(s, tail, base);
    if (errno == EINVAL || errno == ERANGE || *tail == s) {
        errno = save_errno;
        *x = 0;
        return 0;
    } else {
        errno = save_errno;
        return 1;
    }
}

static int str_to_llong(const char *s, int base, long long *x)
{
    char *tail;
    int ok = str_to_llong_with_tail(s, &tail, base, x);
    if (*tail != '\0') {
        *x = 0;
        return 0;
    }
    return ok;
}

static int str_to_uint(const char *s, int base, unsigned int *u)
{
    long long ll;
    int ok = str_to_llong(s, base, &ll);
    if (!ok || ll < 0 || ll > UINT_MAX) {
        *u = 0;
        return 0;
    } else {
        *u = ll;
        return 1;
    }
}


static inline const char * get_rx_queue_name(unsigned int id)
{
    static char buffer[RTE_RING_NAMESIZE];
    snprintf(buffer, sizeof(buffer), MP_CLIENT_RXQ_NAME, id);
    return buffer;
}

static void usage(const char *progname)
{
    printf("\nUsage: %s [EAL args] -- -n <client_id>\n", progname);
}

static int parse_client_num(const char *client)
{
    if (str_to_uint(client, 10, &client_id)) {
        return 0;
    } else {
        return -1;
    }
}

static int parse_app_args(int argc, char *argv[])
{
    int option_index = 0, opt = 0;
    char **argvopt = argv;
    const char *progname = NULL;
    static struct option lgopts[] = {
        {NULL, 0, NULL, 0 }
    };
    progname = argv[0];

    while ((opt = getopt_long(argc, argvopt, "n:", lgopts,
        &option_index)) != EOF) {
        switch (opt) {
            case 'n':
                if (parse_client_num(optarg) != 0) {
                    usage(progname);
                    return -1;
                }
                break;
            default:
                usage(progname);
                return -1;
        }
    }

    return 0;
}


static inline void
print_ether_addr(const char *what, struct ether_addr *eth_addr)
{
        char buf[ETHER_ADDR_FMT_SIZE];
        ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
        printf("%s%s", what, buf);
}


static void my_process_bulk(struct rte_mbuf *pkts[], uint16_t nb_pkts)
{
        struct rte_mbuf  *mb;
        struct ether_hdr *eth_hdr;
        uint16_t i, eth_type;
        struct rte_net_hdr_lens;

        if (!nb_pkts)
                return;
        printf("%u packets\n", (unsigned int) nb_pkts);
        for (i = 0; i < nb_pkts; i++) {
                mb = pkts[i];
                eth_hdr = rte_pktmbuf_mtod(mb, struct ether_hdr *);
                eth_type = RTE_BE_TO_CPU_16(eth_hdr->ether_type);
                print_ether_addr("  src=", &eth_hdr->s_addr);
                print_ether_addr(" - dst=", &eth_hdr->d_addr);
                printf(" - type=0x%04x - length=%u - nb_segs=%d",
                       eth_type, (unsigned int) mb->pkt_len,
                       (int)mb->nb_segs);
                }
        printf("\n");
}




int main(int argc, char *argv[])
{
  struct rte_ring *rx_ring = NULL;
  unsigned avail;
  int retval = 0;
  unsigned rx_pkts = PKT_READ_SIZE;
  struct rte_mbuf *pkts[PKT_READ_SIZE];
  if ((retval = rte_eal_init(argc, argv)) < 0)
    return -1;
  argc -= retval;
  argv += retval;
  if (parse_app_args(argc, argv) < 0)
    rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
  rx_ring = rte_ring_lookup(get_rx_queue_name(client_id));
  if (rx_ring == NULL)
    rte_exit(EXIT_FAILURE, "Cannot get RX ring - is server process running?\n");
  printf("\nClient process %u handling packets\n", client_id);
  printf("[Press Ctrl-C to quit ...]\n");
  for (;;)
    {
    rx_pkts = (uint16_t)RTE_MIN(rte_ring_count(rx_ring), PKT_READ_SIZE);
    retval = rte_ring_dequeue_bulk(rx_ring, (void *) pkts, rx_pkts, &avail);
    if (retval)
      my_process_bulk(pkts, retval);
    else
      usleep(10);
     }
}
