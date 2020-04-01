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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/un.h>

#include "ioc.h"
#include "clo_tcp.h"
#include "sock_fd.h"
#include "main.h"
#include "machine.h"
#include "utils.h"
#include "bootp_input.h"

#define MAX_NOT_ACKED_PUSH_ACK 50

static t_machine *head_machine;
static t_machine_sock *llid_to_socket[CLOWNIX_MAX_CHANNELS];



/*****************************************************************************/
static int get_hash(int type, int lip, int fip, short lport, short fport)
{
  int result;
  int hash4 = 0;
  short hash2 = 0;
  char hash1 = 0;
  hash4 ^= (lip+type); 
  hash4 ^= fip; 
  hash2 ^= hash4 & 0xFFFF;
  hash2 ^= (hash4 >> 16) & 0xFFFF;
  hash2 ^= lport;
  hash2 ^= fport;
  hash1 ^= hash2 & 0xFF;
  hash1 ^= (hash2 >> 8) & 0xFF;
  result = hash1 & 0xFF;
  result += 1;
  if ((result <= 0) || (result >= MAX_HASH_IDX))
    KOUT("%d", result);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_machine_sock *get_fast_sock(t_machine *machine, int type, 
                                     int lip_int, int fip_int, 
                                     short lport, short fport)
{
  int idx = get_hash(type, lip_int, fip_int, lport, fport);
  t_machine_sock *cur = machine->hash_head[idx]; 
  while (cur && ( !( (cur->is_type_sock == type)     &&
                     (cur->lip_int      == lip_int)  &&
                     (cur->fip_int      == fip_int)  &&
                     (cur->lport        == lport)    &&
                     (cur->fport        == fport))))
    cur = cur->fast_sock_next;
  return cur; 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void create_fast_sock(t_machine *machine, t_machine_sock *ms)
{
  int idx = get_hash(ms->is_type_sock, ms->lip_int, ms->fip_int,
                     ms->lport, ms->fport);
  if (machine->hash_head[idx])
    machine->hash_head[idx]->fast_sock_prev = ms;
  ms->fast_sock_next = machine->hash_head[idx];
  machine->hash_head[idx] = ms;    
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void delete_fast_sock(t_machine *machine, t_machine_sock *ms)
{
  int idx = get_hash(ms->is_type_sock, ms->lip_int, ms->fip_int,
                     ms->lport, ms->fport);
  t_machine_sock *cur = machine->hash_head[idx];
  while (cur && ( !( (cur->is_type_sock == ms->is_type_sock) &&
                     (cur->lip_int      == ms->lip_int)      &&
                     (cur->fip_int      == ms->fip_int)      &&
                     (cur->lport        == ms->lport)        &&
                     (cur->fport        == ms->fport))))
    cur = cur->fast_sock_next;
  if (!cur)
    KOUT(" ");
  if (cur->fast_sock_prev)
    cur->fast_sock_prev->fast_sock_next = cur->fast_sock_next;
  if (cur->fast_sock_next)
    cur->fast_sock_next->fast_sock_prev = cur->fast_sock_prev;
  if (cur == machine->hash_head[idx])
    machine->hash_head[idx] = cur = cur->fast_sock_next;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
t_machine *get_head_machine(void)
{
  return head_machine;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void set_llid_to_socket(int llid, t_machine_sock *so)
{
  if (llid_to_socket[llid])
    KOUT(" ");
  if (!so)
    KOUT(" ");
  llid_to_socket[llid] = so;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void reset_llid_to_socket(int llid, t_machine_sock *so)
{
  if (!llid_to_socket[llid])
    KOUT(" ");
  if (llid_to_socket[llid] != so)
    KOUT(" ");
  llid_to_socket[llid] = NULL;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void delete_machine_sock_llid(t_machine_sock *ms)
{
  int is_blkd;
  if (ms->llid > 0)
    {
    reset_llid_to_socket(ms->llid, ms);
    if (msg_exist_channel(get_all_ctx(), ms->llid, &is_blkd, __FUNCTION__))
      msg_delete_channel(get_all_ctx(), ms->llid);
    }
  ms->llid = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_machine_sock *get_llid_to_socket(int llid)
{
  return (llid_to_socket[llid]);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_machine_sock *find_machine_sock(t_machine *machine, int is_type_sock, 
                                         char *lip, short lport,
                                         char *fip, short fport)
{
  t_machine_sock *result;
  uint32_t lip_int, fip_int;
  if (ip_string_to_int(&lip_int, lip))
    KOUT("%s", lip);
  if (ip_string_to_int(&fip_int, fip))
    KOUT("%s", fip);
  result = get_fast_sock(machine,is_type_sock,lip_int,fip_int,lport,fport); 
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_machine_sock *alloc_machine_sock(t_machine *machine, int is_type_sock, 
                                          char *lip, short lport,
                                          char *fip, short fport,
                                          int llid, int fd)
{
  t_machine_sock *ms;
  uint32_t lip_int, fip_int;
  if (ip_string_to_int(&lip_int, lip))
    KOUT("%s", lip);
  if (ip_string_to_int(&fip_int, fip))
    KOUT("%s", fip);

  ms = (t_machine_sock *) malloc(sizeof(t_machine_sock));
  memset(ms, 0, sizeof(t_machine_sock));
  ms->state = 0;
  ms->is_type_sock = is_type_sock;
  strncpy(ms->fip, fip, MAX_NAME_LEN-1);
  strncpy(ms->lip, lip, MAX_NAME_LEN-1);
  ms->fport = fport;
  ms->lport = lport;
  ms->lip_int = lip_int;
  ms->fip_int = fip_int;
  ms->llid = llid;
  ms->fd   = fd;
  ms->machine = machine;
  create_fast_sock(machine, ms);
  if (is_type_sock == is_udp_sock)
    {
    ms->next = machine->head_udp;
    if (machine->head_udp)
      machine->head_udp->prev = ms;
    machine->head_udp = ms;
    }
  else
    KOUT(" ");
  return ms;  
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void free_machine_sock(t_machine_sock *ms)
{
  t_machine *machine = ms->machine;

  free(ms->data_waiting);
  if (ms->msg_rx)
    {
    clownix_timeout_del(get_all_ctx(), 
                        ms->msg_rx->timer_repeat_try_connect_abs_beat,
                        ms->msg_rx->timer_repeat_try_connect_ref, 
                        __FILE__, __LINE__);
    free(ms->msg_rx);
    ms->msg_rx = NULL;
    }
  clownix_timeout_del(get_all_ctx(), 
                      ms->timer_repeat_abs_beat, ms->timer_repeat_ref,
                      __FILE__, __LINE__);
  clownix_timeout_del(get_all_ctx(), 
                      ms->timer_differed_abs_beat, ms->timer_differed_ref,
                      __FILE__, __LINE__);
  clownix_timeout_del(get_all_ctx(), 
                      ms->timer_killer_abs_beat, ms->timer_killer_ref,
                      __FILE__, __LINE__);
  delete_machine_sock_llid(ms);
  delete_fast_sock(machine, ms);
  if (ms->prev)
    ms->prev->next = ms->next;
  if (ms->next)
    ms->next->prev = ms->prev;
  if (ms->is_type_sock == is_udp_sock)
    {
    if (machine->head_udp == ms)
      machine->head_udp = ms->next;
    } 
  else
    KOUT("%d", ms->is_type_sock);
  free(ms);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void free_machine_whole_sock_chain(t_machine_sock *head)
{
  t_machine_sock *next, *cur = head;
  while (cur)
    {
    next = cur->next;
    free_machine_sock(cur); 
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
static int udp_attach(int *ret_fd)
{
  int result = -1;
  struct sockaddr_in addr;
  int fd = socket(AF_INET,SOCK_DGRAM,0);
  if(fd > 0)
    {
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr))<0)
      close(fd);
    else
      {
      if ((fd < 0) || (fd >= MAX_SELECT_CHANNELS-1))
        KOUT("%d", fd);
      result = msg_watch_fd(get_all_ctx(), fd, rx_udp_so, err_udp_so);
      if (result <= 0)
        KOUT(" ");
      nonblock_fd(fd);
      *ret_fd = fd;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_machine *look_for_machine_with_name(char *name)
{
  t_machine *cur = head_machine;
  while(cur && strcmp(cur->name, name))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_machine *look_for_machine_with_mac(char *mac)
{
  t_machine *cur = head_machine;
  while(cur && strcmp(cur->mac, mac))
    cur = cur->next;
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
t_machine *look_for_machine_with_ip(char *ip)
{
  t_machine *cur = head_machine;
  while(cur && strcmp(cur->ip, ip))
    cur = cur->next;
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int add_machine(char *mac)
{
  int  result = -1;
  t_machine *ma = look_for_machine_with_mac(mac);
  if (!ma)
    {
    ma = (t_machine *) malloc(sizeof(t_machine));
    memset(ma, 0, sizeof(t_machine));
    strncpy(ma->mac, mac, MAX_NAME_LEN-1);
    if (head_machine)
      head_machine->prev = ma;
    ma->next = head_machine;
    head_machine = ma;    
    req_for_name_with_mac(mac);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int del_machine(char *mac)
{
  int result = -1;
  t_machine *ma = look_for_machine_with_mac(mac);
  if (ma)
    {
    clownix_timeout_del(get_all_ctx(), 
                        ma->timer_icmp_abs_beat, ma->timer_icmp_ref,
                        __FILE__, __LINE__);
    clownix_timeout_del(get_all_ctx(), 
                        ma->timer_monitor_abs_beat, ma->timer_monitor_ref,
                        __FILE__, __LINE__);
    if (ma->prev)
      ma->prev->next = ma->next;
    if (ma->next)
      ma->next->prev = ma->prev;
    if (ma == head_machine)
      head_machine = ma->next;
    free_machine_whole_sock_chain(ma->head_udp);
    free(ma);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void machine_boop_alloc(t_machine *machine, char *alloc_ip)
{
  t_machine *cur = head_machine;
  while(cur)
    {
    if (machine != cur)
      if (!strcmp(alloc_ip, cur->ip))
        KOUT("%s %s %s", alloc_ip, cur->ip, machine->mac);
    cur = cur->next;
    }
  if (strcmp(alloc_ip, machine->ip))
    {
    strncpy(machine->ip, alloc_ip, MAX_NAME_LEN-1); 
    }
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
static void timer_killer_machine_sock(t_all_ctx *all_ctx, void *data)
{
  t_machine_sock *so = (t_machine_sock *) data;
  so->timer_killer_abs_beat = 0;
  so->timer_killer_ref = 0;
  free_machine_sock(so);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
t_machine_sock *machine_get_udp_sock(t_machine *machine, 
                                     char *lip, short lport,
                                     char *fip, short fport)
{
  int llid, fd;
  t_machine_sock *ms = find_machine_sock(machine, is_udp_sock, lip, lport, 
                                                               fip, fport);
  if (!ms)
    {
    llid = udp_attach(&fd);
    if (llid > 0)
      {
      ms = alloc_machine_sock(machine, is_udp_sock, lip, lport, 
                                                    fip, fport, llid, fd);
      set_llid_to_socket(llid, ms);
      }
    }
  if (ms)
    {
    clownix_timeout_del(get_all_ctx(), 
                        ms->timer_killer_abs_beat, ms->timer_killer_ref,
                        __FILE__, __LINE__);
    clownix_timeout_add(get_all_ctx(), 
                        10, timer_killer_machine_sock, (void *) ms,
                        &(ms->timer_killer_abs_beat), &(ms->timer_killer_ref));
    }
  return ms;
}
/*--------------------------------------------------------------------------*/




/*****************************************************************************/
void machine_init(void)
{
  head_machine = NULL;
}
/*--------------------------------------------------------------------------*/



