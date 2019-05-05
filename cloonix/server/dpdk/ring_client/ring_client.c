#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <unistd.h>

#define PKT_READ_SIZE  ((uint16_t)32)

#define MEMPOOL_ELT_SIZE 2048
#define MAX_KEEP 16
#define MEMPOOL_SIZE ((rte_lcore_count()*(MAX_KEEP+RTE_MEMPOOL_CACHE_MAX_SIZE))-1)


static int g_ring1;



/*****************************************************************************/
static inline const char *get_rx_queue_name(unsigned int id)
{
  static char buffer[RTE_RING_NAMESIZE];
  snprintf(buffer, sizeof(buffer), "dpdkr%u_tx", id);
  return buffer;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void parse_app_args(int argc, char *argv[])
{
  char *endptr;
  if (argc != 2)
    {
    printf("\nUsage: [EAL args] -- <num1>\n");
    exit(1);
    }
  else
    {
    g_ring1 = (int) strtol(argv[1], &endptr, 10);
    if (endptr[0] != 0)
      {
      printf("\nUsage: [EAL args] -- <num1>\n");
      exit(1);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void my_process_bulk(struct rte_mbuf *pkts[PKT_READ_SIZE],
                            uint16_t nb_pkts)
{
  struct rte_mbuf  *mb;
  struct ether_hdr *eth_hdr;
  uint16_t i, eth_type;
  struct rte_net_hdr_lens;
  char buf[ETHER_ADDR_FMT_SIZE];
struct rte_mempool_ops *ops;

  printf("%u packets\n", (unsigned int) nb_pkts);
  for (i = 0; i < nb_pkts; i++)
    {
    mb = pkts[i];
    eth_hdr = rte_pktmbuf_mtod(mb, struct ether_hdr *);
    eth_type = rte_be_to_cpu_16(eth_hdr->ether_type);
    ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, &(eth_hdr->s_addr));
    printf("  src=%s", buf);
    ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, &(eth_hdr->d_addr));
    printf(" - dst=%s", buf);
    printf(" - type=0x%04x - length=%u - nb_segs=%d\n",
    eth_type, (unsigned int) mb->pkt_len, (int)mb->nb_segs);
ops = rte_mempool_get_ops(mb->pool->ops_index);

if ((mb->pool->flags & MEMPOOL_F_POOL_CREATED) == 0) 
printf("NOT CREATED %X\n", mb->pool->flags);
else
printf("CREATED %X %p %d\n", mb->pool->flags, (void *) ops,  mb->pool->ops_index);

    rte_pktmbuf_free(mb);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int main(int argc, char *argv[])
{
  struct rte_ring *rx_ring1 = NULL;
  unsigned avail;
  int retval = 0;
  unsigned rx_pkts = PKT_READ_SIZE;
  struct rte_mbuf *pkts[PKT_READ_SIZE];

  setenv("XDG_RUNTIME_DIR", "/home/perrier/cloonix_data/nemo/dpdk", 1);

  if ((retval = rte_eal_init(argc, argv)) < 0)
    return -1;
  argc -= retval;
  argv += retval;
  parse_app_args(argc, argv);
  printf("looking up rings\n");
  rx_ring1 = rte_ring_lookup(get_rx_queue_name(g_ring1));
  if (rx_ring1 == NULL) 
    rte_exit(EXIT_FAILURE, "Cannot get RX ring - is server process running?\n");


  printf("\nClient process %u handling packets\n", g_ring1);
  printf("[Press Ctrl-C to quit ...]\n");
  for (;;)
    {
    rx_pkts = (uint16_t)RTE_MIN(rte_ring_count(rx_ring1), PKT_READ_SIZE);
    retval = rte_ring_dequeue_bulk(rx_ring1, (void *) pkts, rx_pkts, &avail);
    if (retval > 0)
      my_process_bulk(pkts, retval);
    }
}
/*---------------------------------------------------------------------------*/
