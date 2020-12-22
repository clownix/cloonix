/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
/* Original copy: https://github.com/quiqfield/flowatcher                    */
/*****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <libwebsockets.h>

#include <rte_compat.h>
#include <rte_bus_pci.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_meter.h>
#include <rte_pci.h>
#include <rte_version.h>
#include <rte_vhost.h>
#include <rte_hash.h>



#include "io_clownix.h"
#include "flow_tab.h"

#define FLOW_TIMEOUT 30
#define EXPORT_FLOW_MAX_V4 50


/*****************************************************************************/
void print_five(int dir, struct fivetuple_v4 *fivetuple)
{
  uint32_t s,d;
  uint16_t sp,dp;
  s = fivetuple->src_ip;
  d = fivetuple->dst_ip;
  sp = fivetuple->src_port;
  dp = fivetuple->dst_port;
  KERR("%d src:%d.%d.%d.%d dst:%d.%d.%d.%d sport:%d dport:%d proto:%d", dir,
  (s >> 24) & 0xff, (s >> 16) & 0xff, (s >> 8) & 0xff, s & 0xff,
  (d >> 24) & 0xff, (d >> 16) & 0xff, (d >> 8) & 0xff, d & 0xff,
  sp & 0xFFFF, dp & 0xFFFF, fivetuple->proto & 0xFF);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void update_export_data_v4(int dir, int flow_table_size)
{
  int j;
  struct flow_entry *entry;
  struct flow_table *flow = get_flow_table_v4(dir);
  for (j = 0; j < flow_table_size; j++)
    {
    entry = &flow->entries[j];
    if (entry && (entry->start_tsc != 0))
      {
      KERR("%d start_tsc:%lu  pkt_cnt:%lu  byte_cnt:%lu",
           dir, entry->start_tsc, entry->pkt_cnt, entry->byte_cnt);
      print_five(dir, &(entry->key));
      }
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int callback_flowexp(struct lws *wsi, enum lws_callback_reasons reason,
                     void *user, void *in, size_t len)
{
//      struct lws_protocols *protocols = get_wss_protocol(PROTOCOL_FLOWEXP);
//      struct lws_context *context = get_wss_context();
//	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 1280];
//	struct user_data *psd = (struct user_data *)user;
//	struct export_format_v4 *p4 = (struct export_format_v4 *)&buf[LWS_SEND_BUFFER_PRE_PADDING];
//	char seeya[6];
//	int n, m;
//	int write_mode = LWS_WRITE_CONTINUATION;
//	uint32_t i;
//
//	switch (reason) {
//
//		case LWS_CALLBACK_ESTABLISHED:
//
//			RTE_LOG(INFO, USER1, "Server sees client connect\n");
//			psd->serial = 0;
//			psd->state = EXPORT_START_V4;
//			break;
//
//		case LWS_CALLBACK_SERVER_WRITEABLE:
//
//			switch (psd->state) {
//
//				case EXPORT_START_V4:
//
//					psd->left = exp_table_v4->num;
//					write_mode = LWS_WRITE_BINARY;
//
//					p4->serial = ++psd->serial;
//					p4->flow_cnt = app[0].flow_table_v4->flow_cnt;
//
//					if (psd->left <= EXPORT_FLOW_MAX_V4) { /* NO FRAGMENT */
//
//						n = sizeof(struct export_format_v4) +
//							sizeof(struct flowexp_data_v4) * psd->left;
//						for (i = 0; i < psd->left; i++) {
//							rte_memcpy(&p4->fdata[i], &exp_table_v4->entries[exp_idx_v4[i]].fdata,
//									sizeof(struct flowexp_data_v4));
//						}
//
//						m = lws_write(wsi, (unsigned char *)p4, n, write_mode);
//						if (m < 0) {
//							RTE_LOG(WARNING, USER1, "Failed to write (%d)\n", m);
//							return -1;
//						}
//						if (m < n) {
//							RTE_LOG(WARNING, USER1, "Partial write (%d/%d)\n", m, n);
//							return -1;
//						}
//
//						psd->state = EXPORT_START_V4;
//						lws_callback_on_writable(wsi);
//
//						break;
//
//					} else { /* FIRST FRAGMENT */
//
//						write_mode |= LWS_WRITE_NO_FIN;
//
//						n = sizeof(struct export_format_v4) +
//							sizeof(struct flowexp_data_v4) * EXPORT_FLOW_MAX_V4;
//						for (i = 0; i < EXPORT_FLOW_MAX_V4; i++) {
//							rte_memcpy(&p4->fdata[i], &exp_table_v4->entries[exp_idx_v4[i]].fdata,
//									sizeof(struct flowexp_data_v4));
//						}
//
//						m = lws_write(wsi, (unsigned char *)p4, n, write_mode);
//						if (m < 0) {
//							RTE_LOG(WARNING, USER1, "Failed to write (%d)\n", m);
//							return -1;
//						}
//						if (m < n) {
//							RTE_LOG(WARNING, USER1, "Partial write (%d/%d)\n", m, n);
//							return -1;
//						}
//
//						psd->left -= EXPORT_FLOW_MAX_V4;
//						psd->state = EXPORT_PAYLOAD_V4;
//						lws_callback_on_writable(wsi);
//
//						break;
//					}
//
//				case EXPORT_PAYLOAD_V4:
//
//					if (psd->left <= EXPORT_FLOW_MAX_V4) {
//						psd->state = EXPORT_PAYLOAD_V4_FIN;
//						lws_callback_on_writable(wsi);
//						break;
//					}
//
//					write_mode |= LWS_WRITE_NO_FIN;
//
//					n = sizeof(struct flowexp_data_v4) * EXPORT_FLOW_MAX_V4;
//					for (i = 0; i < EXPORT_FLOW_MAX_V4; i++) {
//						rte_memcpy(&p4->fdata[i],
//								&exp_table_v4->entries[exp_idx_v4[exp_table_v4->num - psd->left + i]].fdata,
//								sizeof(struct flowexp_data_v4));
//					}
//
//					m = lws_write(wsi, (unsigned char *)p4->fdata, n, write_mode);
//					if (m < 0) {
//						RTE_LOG(WARNING, USER1, "Failed to write (%d)\n", m);
//						return -1;
//					}
//					if (m < n) {
//						RTE_LOG(WARNING, USER1, "Partial write (%d/%d)\n", m, n);
//						return -1;
//					}
//
//					psd->left -= EXPORT_FLOW_MAX_V4;
//					lws_callback_on_writable(wsi);
//
//					break;
//
//				case EXPORT_PAYLOAD_V4_FIN:
//
//					n = sizeof(struct flowexp_data_v4) * psd->left;
//					for (i = 0; i < psd->left; i++) {
//						rte_memcpy(&p4->fdata[i],
//								&exp_table_v4->entries[exp_idx_v4[exp_table_v4->num - psd->left + i]].fdata,
//								sizeof(struct flowexp_data_v4));
//					}
//
//					m = lws_write(wsi, (unsigned char *)p4->fdata, n, write_mode);
//					if (m < 0) {
//						RTE_LOG(WARNING, USER1, "Failed to write (%d)\n", m);
//						return -1;
//					}
//					if (m < n) {
//						RTE_LOG(WARNING, USER1, "Partial write (%d/%d)\n", m, n);
//						return -1;
//					}
//
//					psd->state = EXPORT_START_V4;
//					lws_callback_on_writable(wsi);
//					break;
//			}
//
//
//			break;
//
//		case LWS_CALLBACK_RECEIVE:
//
//			if (len < 6)
//				break;
//
//			if (strcmp((char *)in, "reset\n") == 0)
//				psd->serial = 0;
//
//			if (strcmp((char *)in, "closeme\n") == 0) {
//				RTE_LOG(INFO, USER1, "Closing as requested\n");
//				strcpy(seeya, "seeya");
//				lws_close_reason(wsi, LWS_CLOSE_STATUS_GOINGAWAY, (unsigned char *)seeya, 5);
//				return -1;
//			}
//
//			break;
//
//		default:
//			break;
//	}

	return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void dump_handshake_info(struct lws *wsi)
{
  int n = 0;
  char buf[256];
  unsigned char *c;
  printf("\n");
  do
    {
    c = (unsigned char *) lws_token_to_string(n);
    if (!c)
      {
      n++;
      continue;
      }
    if (!lws_hdr_total_length(wsi, n))
      {
      n++;
      continue;
      }
    lws_hdr_copy(wsi, buf, sizeof(buf), n);
    RTE_LOG(INFO, USER1, "%s = %s\n", c, buf);
    KERR("%s = %s\n", c, buf);
    n++;
    } while (c);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *get_mimetype(char *file)
{
  int n = strlen(file);
  char *result = NULL;
  if (n >= 5)
    {
    if (!strcmp(&file[n - 3], ".js"))
      result = "application/javascript";
    else if (!strcmp(&file[n - 4], ".ico"))
      result = "image/x-icon";
    else if (!strcmp(&file[n - 4], ".png"))
      result = "image/png";
    else if (!strcmp(&file[n - 5], ".html"))
      result = "text/html";
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int callback_http(struct lws *wsi, enum lws_callback_reasons reason,
                  void *user, void *in, size_t len)
{
  char *mimetype;
  char buf[256];
  int n = 0, result = 0;
  switch (reason)
    {
    case LWS_CALLBACK_HTTP:
      dump_handshake_info(wsi);
      if (len < 1)
        {
        lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);
        result = -1;
        }
      else if (strchr((char *)in + 1, '/'))
        {
        lws_return_http_status(wsi, HTTP_STATUS_FORBIDDEN, NULL);
        result = -1;
        }
      else
        {
        strcpy(buf, "client");
        if (strcmp((char *)in, "/"))
          {
          if (*((char *)in) != '/')
            strcat(buf, "/");
          strncat(buf, in, sizeof(buf) - strlen("client"));
          }
        else
          strcat(buf, "/index.html");
        buf[sizeof(buf) - 1] = '\0';
        mimetype = get_mimetype(buf);
        if (!mimetype)
          {
          RTE_LOG(WARNING, USER1, "Unknown mimetype for %s\n", buf);
          lws_return_http_status(wsi, HTTP_STATUS_FORBIDDEN, NULL);
          result = -1;
          }
        n = lws_serve_http_file(wsi, buf, mimetype, NULL, n);
        if (n < 0 || ((n > 0) && lws_http_transaction_completed(wsi)))
                        result = -1;
        }
      break;

      case LWS_CALLBACK_PROTOCOL_INIT:
        KERR("LWS_CALLBACK_PROTOCOL_INIT %d", reason);
      break;

      case LWS_CALLBACK_ADD_POLL_FD:
        KERR("LWS_CALLBACK_ADD_POLL_FD %d", reason);
      break;

      case LWS_CALLBACK_LOCK_POLL:
        KERR("LWS_CALLBACK_LOCK_POLL %d", reason);
      break;

      case LWS_CALLBACK_UNLOCK_POLL:
        KERR("LWS_CALLBACK_UNLOCK_POLL %d", reason);
      break;

      case LWS_CALLBACK_GET_THREAD_ID:
        //KERR("LWS_CALLBACK_GET_THREAD_ID %d", reason);
      break;

      default:
        KERR("case NOT YET SEEN callback_http %d", reason);
      break;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void flow_tab_periodic_dump(int flow_table_size)
{
  struct lws_protocols *protocols = get_wss_protocol(PROTOCOL_FLOWEXP);
  struct lws_context *context = get_wss_context();
//  update_export_data_v4(0, flow_table_size);
//  update_export_data_v4(1, flow_table_size);
  lws_callback_on_writable_all_protocol(context, protocols);
}
/*---------------------------------------------------------------------------*/
