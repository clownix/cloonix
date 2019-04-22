
#include "rte_config.h"
#include <getopt.h>
#include <rte_config.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_string_fns.h>
#include <rte_ip.h>
#include <rte_byteorder.h>

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1
#define MP_CLIENT_RXQ_NAME "dpdkr%u_tx"
#define PKT_READ_SIZE  ((uint16_t)32)

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

static void my_process_bulk(void *pkts)
{
  printf("TODO\n");
}

int main(int argc, char *argv[])
{
  struct rte_ring *rx_ring = NULL;
  int avail, free_count, retval = 0;
  unsigned rx_pkts = PKT_READ_SIZE;
  void *pkts[PKT_READ_SIZE];
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
    retval = rte_ring_dequeue_bulk(rx_ring, pkts, rx_pkts, &avail);
    if (retval)
      {
      if (retval == PKT_READ_SIZE)
        usleep(10000);
      else
        usleep(1000);
      my_process_bulk(pkts);
      free_count = rte_ring_free_count(rx_ring);
      printf("ret: %d %d\n", retval, free_count);
      }
    else
      usleep(500);
     }
}
