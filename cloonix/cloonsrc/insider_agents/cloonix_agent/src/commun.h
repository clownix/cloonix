/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define MAX_ASCII_LEN 200
#ifdef __FreeBSD__
#define VIRTIOPORT "/dev/vtcon/net.cloon.0"
#else
#define VIRTIOPORT "/dev/virtio-ports/net.cloon.0"
#endif
#define UNIX_CXP_SOCK      "/cloonixmnt/tmp/unix_cloonix_cxp_sock"

/* One open byte pipe: the llid the host knows it by, and the socket to the
   exec server that it is joined to. */
typedef struct t_pipe_ctx
{
  int dido_llid;
  int fd;
  /* the host said xoff: the pipe is not read */
  int host_xoff;
  /* the host was told xoff: too much queued for the exec server */
  int told_xoff;
  struct t_pipe_ctx *prev;
  struct t_pipe_ctx *next;
} t_pipe_ctx;

void send_to_virtio(int dido_llid, int vwifi_base, int vwifi_cid,
                    int type, int val, int len, char *msg);
int virtio_tx_high(void);

void action_rx_virtio(int dido_llid, int vwifi_base, int vwifi_cid,
                      int type, int val, int len, char *rx);
void action_heartbeat(int cur_sec);
void action_init(void);

void action_prepare_fd_set(fd_set *infd, fd_set *outfd, int virtio_full);
void action_events(fd_set *infd);
int  action_get_biggest_fd(void);


void wifi_rx_host_ctrl(int val, int len, char *rx);
void wifi_rx_host_frame(int len, char *rx);
void wifi_events(fd_set *infd);
int  wifi_get_fd(void);
