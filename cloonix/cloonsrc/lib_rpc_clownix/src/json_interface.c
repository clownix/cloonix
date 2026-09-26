/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "io_clownix.h"
#include "lib_topo.h"
#include "json_interface.h"
#include "rpc_clownix.h"
#include "json_min.h"

#define PARSE_SIMPLE_ARRAY(field, nb_field, type_t, parse_name, parse_endp) \
    if(topo->nb_field){                                                       \
      topo->field=(type_t*)clownix_malloc(topo->nb_field*sizeof(type_t),7);  \
      memset(topo->field,0,topo->nb_field*sizeof(type_t));                   \
      p=strstr(buf,"\""#field"\""); if(!p) KOUT("%s",buf);                   \
      p=strchr(p,'[');              if(!p) KOUT("%s",buf);                   \
      for(i=0;i<topo->nb_field;i++){                                          \
        char *obj=strchr(p,'{'); if(!obj) KOUT("%s",buf);                    \
        json_get_str(obj,"name",topo->field[i].parse_name,MAX_NAME_LEN);     \
        json_get_int(obj,#parse_endp,&topo->field[i].parse_endp);            \
        p=strchr(obj,'}'); if(!p) KOUT("%s",buf);                            \
      }                                                                       \
    } 

static char        *sndbuf       = NULL;
static t_llid_tx    g_llid_tx    = NULL;
static t_llid_tx    g_llid_tx_init = NULL;
static t_json_trace g_json_trace = NULL;

enum {
  bnd_min = 0,
  bnd_snf_add,
  bnd_pcap_open, bnd_pcap_close, bnd_pcap_traffic,
  bnd_c2c_add, bnd_c2c_peer_create, bnd_c2c_peer_conf, bnd_c2c_peer_ping,
  bnd_item_pos, bnd_item_poseth,
  bnd_status_ok, bnd_status_ko,
  bnd_add_vm, bnd_sav_vm, bnd_del_name,
  bnd_add_lan_endp, bnd_del_lan_endp,
  bnd_kill_uml_clownix, bnd_del_all,
  bnd_list_pid_req, bnd_list_pid_resp,
  bnd_list_commands_req, bnd_list_commands_resp,
  bnd_topo_small_event_sub, bnd_topo_small_event,
  bnd_event_topo_sub, bnd_event_topo,
  bnd_work_dir_req, bnd_work_dir_resp,
  bnd_vmcmd,
  bnd_eventfull_sub, bnd_eventfull,
  bnd_slowperiodic_sub,
  bnd_slowperiodic_qcow2, bnd_slowperiodic_cnt,
  bnd_nat_add, bnd_phy_add, bnd_cnt_add, bnd_tap_add, bnd_a2b_add,
  bnd_a2b_cnf, bnd_lan_cnf, bnd_c2c_cnf, bnd_nat_cnf,
  bnd_sync_wireshark_req, bnd_sync_wireshark_resp,
  bnd_ovs_cmd_req, bnd_ovs_cmd_resp,
  /* Appended, never moved: from 76 on, a wire value may only be added. */
  bnd_trace_sub, bnd_trace,
  bnd_cnt_cnf, bnd_images_req, bnd_images_resp,
  bnd_max,
};

static char bound_list[bnd_max][MAX_CLOWNIX_BOUND_LEN];

static void my_msg_mngt_tx(int llid, int len, char *buf)
{
  static char *wrap = NULL;
  int wlen;
  if (!g_llid_tx) KOUT(" ");
  if (!wrap) wrap = (char *) clownix_malloc(MAX_SIZE_BIGGEST_MSG + 8, 1);
  wrap[0] = 0xCA; wrap[1] = 0xFE; wrap[2] = 0xDE; wrap[3] = 0xCA;
  wrap[4] = 0xCA; wrap[5] = 0xFE; wrap[6] = 0xDE; wrap[7] = 0xCA;
  memcpy(wrap + 8, buf, len);
  wlen = len + 8;
  g_llid_tx(llid, wlen, wrap);
  /* After the tx: the trace reuses wrap/sndbuf and goes to the transmitter of init. */
  if (g_json_trace)
    {
    t_llid_tx tx = g_llid_tx;
    g_llid_tx = g_llid_tx_init;
    g_json_trace(llid, 1, buf, len);
    g_llid_tx = tx;
    }
}

/* Write src as a JSON-safe string into dst (max dstlen bytes incl. */

static int get_bnd_event(char *bound)
{
  int i;
  for (i = bnd_min; i < bnd_max; i++)
    if (!strcmp(bound, bound_list[i])) return i;
  return 0;
}

/* Find "key": at the top level, skipping string values. */

static int emit_eth_table(char *dst, t_eth_table *eth, int nb)
{
  int i, len = 0;
  len += sprintf(dst+len, "\"eth_table\":[");
  for (i = 0; i < nb; i++) {
    char *ifn = eth[i].vhost_ifname;
    if (!strlen(ifn)) ifn = "noname";
    len += sprintf(dst+len,
      "%s{\"endp_type\":%d,\"randmac\":%d,\"ifname\":\"%s\","
      "\"mac\":[%d,%d,%d,%d,%d,%d]}",
      i ? "," : "",
      eth[i].endp_type, eth[i].randmac, ifn,
      eth[i].mac_addr[0], eth[i].mac_addr[1], eth[i].mac_addr[2],
      eth[i].mac_addr[3], eth[i].mac_addr[4], eth[i].mac_addr[5]);
  }
  len += sprintf(dst+len, "]");
  return len;
}

static void parse_eth_table(char *pp, int nb, t_eth_table *eth)
{
  int i, j;
  char *p = strchr(pp, '[');
  if (!p) KOUT("%s", pp);
  /* The count comes from the wire and the table has MAX_ETH_VM slots: the
     caller keeps and refuses the count asked, the parse stays in bounds. */
  if (nb > MAX_ETH_VM)
    {
    KERR("WARNING %d eth asked, the table holds %d", nb, MAX_ETH_VM);
    nb = MAX_ETH_VM;
    }
  for (i = 0; i < nb; i++) {
    char *obj = strchr(p, '{');
    if (!obj) KOUT("eth_table parse error nb=%d i=%d", nb, i);
    json_get_int(obj, "endp_type", &eth[i].endp_type);
    json_get_int(obj, "randmac",   &eth[i].randmac);
    json_get_str(obj, "ifname", eth[i].vhost_ifname, MAX_NAME_LEN);
    if (!strcmp(eth[i].vhost_ifname, "noname"))
      memset(eth[i].vhost_ifname, 0, MAX_NAME_LEN);
    char *mac_p = strstr(obj, "\"mac\"");
    if (mac_p) {
      mac_p = strchr(mac_p, '[');
      if (mac_p) {
        int vals[6] = {0};
        char *cur = mac_p + 1;
        for (j = 0; j < 6; j++) {
          while (*cur == ' ' || *cur == ',') cur++;
          vals[j] = (int)strtol(cur, &cur, 10);
        }
        for (j = 0; j < 6; j++)
          eth[i].mac_addr[j] = (unsigned char)(vals[j] & 0xFF);
      }
    }
    p = strchr(obj, '}');
    if (!p) KOUT("eth_table close brace missing");
  }
}

void doors_io_basic_json_init(t_llid_tx llid_tx)
{
  sndbuf    = (char *) clownix_malloc(MAX_SIZE_BIGGEST_MSG, 1);
  g_llid_tx = llid_tx;
  g_llid_tx_init = llid_tx;
  memset(bound_list, 0, sizeof(bound_list));
#define SB(idx, str) strncpy(bound_list[idx], str, MAX_CLOWNIX_BOUND_LEN-1)
  SB(bnd_snf_add,              "snf_add");
  SB(bnd_pcap_open,            "pcap_open");
  SB(bnd_pcap_close,           "pcap_close");
  SB(bnd_pcap_traffic,         "pcap_traffic");
  SB(bnd_c2c_add,              "c2c_add");
  SB(bnd_c2c_peer_create,      "c2c_create");
  SB(bnd_c2c_peer_conf,        "c2c_conf");
  SB(bnd_c2c_peer_ping,        "c2c_ping");
  SB(bnd_item_pos,             "item_pos");
  SB(bnd_item_poseth,          "item_poseth");
  SB(bnd_status_ok,            "status_ok");
  SB(bnd_status_ko,            "status_ko");
  SB(bnd_add_vm,               "add_vm");
  SB(bnd_sav_vm,               "sav_vm");
  SB(bnd_del_name,             "del_name");
  SB(bnd_add_lan_endp,         "add_lan_endp");
  SB(bnd_del_lan_endp,         "del_lan_endp");
  SB(bnd_kill_uml_clownix,     "kill_uml_clownix");
  SB(bnd_del_all,              "del_all");
  SB(bnd_list_pid_req,         "list_pid_req");
  SB(bnd_list_pid_resp,        "list_pid_resp");
  SB(bnd_list_commands_req,    "list_commands_req");
  SB(bnd_list_commands_resp,   "list_commands_resp");
  SB(bnd_topo_small_event_sub,    "topo_small_event_sub");
  SB(bnd_topo_small_event,        "topo_small_event");
  SB(bnd_event_topo_sub,       "event_topo_sub");
  SB(bnd_event_topo,           "event_topo");
  SB(bnd_work_dir_req,         "work_dir_req");
  SB(bnd_work_dir_resp,        "work_dir_resp");
  SB(bnd_vmcmd,                "vmcmd");
  SB(bnd_eventfull_sub,        "eventfull_sub");
  SB(bnd_eventfull,            "eventfull");
  SB(bnd_slowperiodic_sub,     "slowperiodic_sub");
  SB(bnd_slowperiodic_qcow2,   "slowperiodic_qcow2");
  SB(bnd_slowperiodic_cnt,     "slowperiodic_cnt");
  SB(bnd_ovs_cmd_req,          "ovs_cmd_req");
  SB(bnd_ovs_cmd_resp,         "ovs_cmd_resp");
  SB(bnd_nat_add,              "nat_add");
  SB(bnd_phy_add,              "phy_add");
  SB(bnd_cnt_add,              "cnt_add");
  SB(bnd_tap_add,              "tap_add");
  SB(bnd_a2b_add,              "a2b_add");
  SB(bnd_a2b_cnf,              "a2b_cnf");
  SB(bnd_lan_cnf,              "lan_cnf");
  SB(bnd_c2c_cnf,              "c2c_cnf");
  SB(bnd_nat_cnf,              "nat_cnf");
  SB(bnd_sync_wireshark_req,   "sync_wireshark_req");
  SB(bnd_sync_wireshark_resp,  "sync_wireshark_resp");
  SB(bnd_trace_sub,            "trace_sub");
  SB(bnd_trace,                "trace");
  SB(bnd_cnt_cnf,              "cnt_cnf");
  SB(bnd_images_req,           "images_req");
  SB(bnd_images_resp,          "images_resp");
#undef SB
}

void doors_io_basic_tx_set(t_llid_tx llid_tx) { g_llid_tx = llid_tx; }

void doors_io_basic_json_trace_set(t_json_trace json_trace)
{ g_json_trace = json_trace; }

char *llid_trace_lib(int type)
{ static char b[32]; snprintf(b,sizeof(b),"llid_type_%d",type); return b; }

char *prop_flags_ascii_get(int prop_flags)
{ static char b[32]; snprintf(b,sizeof(b),"0x%x",prop_flags); return b; }

#define SEND(fmt,...) do{ int _l=sprintf(sndbuf,fmt,##__VA_ARGS__); my_msg_mngt_tx(llid,_l,sndbuf); }while(0)

void send_nat_add(int llid,int tid,char *name,char *ports)
  {SEND("{\"fctident\":\"nat_add\",\"tid\":%d,\"name\":\"%s\","
        "\"ports\":\"%s\"}",tid,name,ports);}
void send_phy_add(int llid,int tid,char *name,int type)
  {SEND("{\"fctident\":\"phy_add\",\"tid\":%d,\"name\":\"%s\",\"type\":%d}",tid,name,type);}
void send_tap_add(int llid,int tid,char *name)
  {SEND("{\"fctident\":\"tap_add\",\"tid\":%d,\"name\":\"%s\"}",tid,name);}
void send_a2b_add(int llid,int tid,char *name)
  {SEND("{\"fctident\":\"a2b_add\",\"tid\":%d,\"name\":\"%s\"}",tid,name);}
void send_a2b_cnf(int llid,int tid,char *name,char *cmd)
  {SEND("{\"fctident\":\"a2b_cnf\",\"tid\":%d,\"name\":\"%s\",\"cmd\":\"%s\"}",tid,name,cmd);}
void send_c2c_cnf(int llid,int tid,char *name,char *cmd)
  {SEND("{\"fctident\":\"c2c_cnf\",\"tid\":%d,\"name\":\"%s\",\"cmd\":\"%s\"}",tid,name,cmd);}
void send_nat_cnf(int llid,int tid,char *name,char *cmd)
  {SEND("{\"fctident\":\"nat_cnf\",\"tid\":%d,\"name\":\"%s\",\"cmd\":\"%s\"}",tid,name,cmd);}
void send_cnt_cnf(int llid,int tid,char *name,char *cmd)
  {SEND("{\"fctident\":\"cnt_cnf\",\"tid\":%d,\"name\":\"%s\",\"cmd\":\"%s\"}",tid,name,cmd);}
void send_lan_cnf(int llid,int tid,char *name,char *cmd)
  {SEND("{\"fctident\":\"lan_cnf\",\"tid\":%d,\"name\":\"%s\",\"cmd\":\"%s\"}",tid,name,cmd);}
void send_snf_add(int llid,int tid,char *name,int num,int val)
  {SEND("{\"fctident\":\"snf_add\",\"tid\":%d,\"name\":\"%s\",\"num\":%d,\"val\":%d}",tid,name,num,val);}
void send_item_pos(int llid,int tid,char *name,int x,int y)
  {SEND("{\"fctident\":\"item_pos\",\"tid\":%d,\"name\":\"%s\",\"x\":%d,\"y\":%d}",tid,name,x,y);}
void send_item_poseth(int llid,int tid,char *name,int num,int deg)
  {SEND("{\"fctident\":\"item_poseth\",\"tid\":%d,\"name\":\"%s\",\"num\":%d,\"deg\":%d}",tid,name,num,deg);}
void send_status_ok(int llid,int tid,char *txt)
{
  int len=sprintf(sndbuf,"{\"fctident\":\"status_ok\",\"tid\":%d,\"txt\":\"",tid);
  len+=json_escape(sndbuf+len,txt?txt:"",MAX_SIZE_BIGGEST_MSG-len);
  len+=sprintf(sndbuf+len,"\"}");
  my_msg_mngt_tx(llid,len,sndbuf);
}
void send_status_ko(int llid,int tid,char *reason)
{
  int len=sprintf(sndbuf,"{\"fctident\":\"status_ko\",\"tid\":%d,\"reason\":\"",tid);
  len+=json_escape(sndbuf+len,reason?reason:"",MAX_SIZE_BIGGEST_MSG-len);
  len+=sprintf(sndbuf+len,"\"}");
  my_msg_mngt_tx(llid,len,sndbuf);
}
void send_del_name(int llid,int tid,char *name)
  {SEND("{\"fctident\":\"del_name\",\"tid\":%d,\"name\":\"%s\"}",tid,name);}
void send_add_lan_endp(int llid,int tid,char *name,int num,char *lan)
  {SEND("{\"fctident\":\"add_lan_endp\",\"tid\":%d,\"name\":\"%s\",\"num\":%d,\"lan\":\"%s\"}",tid,name,num,lan);}
void send_del_lan_endp(int llid,int tid,char *name,int num,char *lan)
  {SEND("{\"fctident\":\"del_lan_endp\",\"tid\":%d,\"name\":\"%s\",\"num\":%d,\"lan\":\"%s\"}",tid,name,num,lan);}
void send_kill_uml_clownix(int llid,int tid)
  {SEND("{\"fctident\":\"kill_uml_clownix\",\"tid\":%d}",tid);}
void send_del_all(int llid,int tid)
  {SEND("{\"fctident\":\"del_all\",\"tid\":%d}",tid);}
void send_work_dir_req(int llid,int tid)
  {SEND("{\"fctident\":\"work_dir_req\",\"tid\":%d}",tid);}
void send_eventfull_sub(int llid,int tid)
  {SEND("{\"fctident\":\"eventfull_sub\",\"tid\":%d}",tid);}
void send_images_req(int llid,int tid)
  {SEND("{\"fctident\":\"images_req\",\"tid\":%d}",tid);}
void send_slowperiodic_sub(int llid,int tid)
  {SEND("{\"fctident\":\"slowperiodic_sub\",\"tid\":%d}",tid);}
void send_list_pid_req(int llid,int tid)
  {SEND("{\"fctident\":\"list_pid_req\",\"tid\":%d}",tid);}
void send_list_commands_req(int llid,int tid,int is_layout)
  {SEND("{\"fctident\":\"list_commands_req\",\"tid\":%d,\"is_layout\":%d}",tid,is_layout);}
void send_topo_small_event_sub(int llid,int tid)
  {SEND("{\"fctident\":\"topo_small_event_sub\",\"tid\":%d}",tid);}
void send_event_topo_sub(int llid,int tid)
  {SEND("{\"fctident\":\"event_topo_sub\",\"tid\":%d}",tid);}
void send_trace_sub(int llid,int tid,int kind,int sub_on)
  {SEND("{\"fctident\":\"trace_sub\",\"tid\":%d,\"kind\":%d,\"sub_on\":%d}",
        tid,kind,sub_on);}
static const char g_b64[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_encode(char *dst, const uint8_t *src, int len)
{
  int i, o = 0;
  for (i = 0; i + 2 < len; i += 3)
    {
    dst[o++] = g_b64[(src[i] >> 2) & 0x3f];
    dst[o++] = g_b64[((src[i] & 0x03) << 4) | ((src[i+1] >> 4) & 0x0f)];
    dst[o++] = g_b64[((src[i+1] & 0x0f) << 2) | ((src[i+2] >> 6) & 0x03)];
    dst[o++] = g_b64[src[i+2] & 0x3f];
    }
  if (i < len)
    {
    dst[o++] = g_b64[(src[i] >> 2) & 0x3f];
    if (i + 1 < len)
      {
      dst[o++] = g_b64[((src[i] & 0x03) << 4) | ((src[i+1] >> 4) & 0x0f)];
      dst[o++] = g_b64[(src[i+1] & 0x0f) << 2];
      }
    else
      dst[o++] = g_b64[(src[i] & 0x03) << 4];
    while (o & 3)
      dst[o++] = '=';
    }
  dst[o] = 0;
  return o;
}

static int b64_val(int c)
{
  if ((c >= 'A') && (c <= 'Z')) return c - 'A';
  if ((c >= 'a') && (c <= 'z')) return c - 'a' + 26;
  if ((c >= '0') && (c <= '9')) return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

static int b64_decode(uint8_t *dst, const char *src, int dstmax)
{
  int acc = 0, bits = 0, v, o = 0;
  while (*src)
    {
    v = b64_val((unsigned char) *src++);
    if (v < 0)
      continue;
    acc = (acc << 6) | v;
    bits += 6;
    if (bits >= 8)
      {
      bits -= 8;
      if (o >= dstmax)
        return -1;
      dst[o++] = (uint8_t) ((acc >> bits) & 0xff);
      }
    }
  return o;
}

void send_pcap_open(int llid,int tid,char *name,int num,char *filter)
  {
  /* The filter is free text from a person: escaped like any other. */
  int len=sprintf(sndbuf,"{\"fctident\":\"pcap_open\",\"tid\":%d,\"name\":\"%s\",\"num\":%d,\"filter\":\"",tid,name?name:"",num);
  len+=json_escape(sndbuf+len,filter?filter:"",MAX_SIZE_BIGGEST_MSG-len);
  len+=sprintf(sndbuf+len,"\"}");
  my_msg_mngt_tx(llid,len,sndbuf);
  }
void send_pcap_close(int llid,int tid,char *name,int num)
  {SEND("{\"fctident\":\"pcap_close\",\"tid\":%d,\"name\":\"%s\",\"num\":%d}",tid,name?name:"",num);}

void send_pcap_traffic(int llid,int tid,char *name,int num,int type,
                       uint32_t pkt_nb,uint32_t sec,uint32_t usec,
                       int caplen,int len,uint8_t *data)
{
  int ln;
  if ((caplen < 0) || (caplen > MAX_PCAP_PKT_LEN))
    {
    KERR("ERROR pcap_traffic caplen %d refused", caplen);
    return;
    }
  ln = sprintf(sndbuf,
       "{\"fctident\":\"pcap_traffic\",\"tid\":%d,\"name\":\"%s\",\"num\":%d,"
       "\"type\":%d,\"pkt_nb\":%u,\"sec\":%u,\"usec\":%u,"
       "\"caplen\":%d,\"len\":%d,\"data\":\"",
       tid,name?name:"",num,type,pkt_nb,sec,usec,caplen,len);
  ln += b64_encode(sndbuf+ln, data?data:(uint8_t *)"", caplen);
  ln += sprintf(sndbuf+ln, "\"}");
  my_msg_mngt_tx(llid,ln,sndbuf);
}

void send_trace(int llid,int tid,int kind,char *name,char *txt)
{
  int len=sprintf(sndbuf,"{\"fctident\":\"trace\",\"tid\":%d,\"kind\":%d,"
                  "\"name\":\"%s\",\"txt\":\"",tid,kind,name?name:"");
  len+=json_escape(sndbuf+len,txt?txt:"",MAX_SIZE_BIGGEST_MSG-len);
  len+=sprintf(sndbuf+len,"\"}");
  my_msg_mngt_tx(llid,len,sndbuf);
}
void send_vmcmd(int llid,int tid,char *name,int vmcmd,int param)
  {SEND("{\"fctident\":\"vmcmd\",\"tid\":%d,\"name\":\"%s\",\"cmd\":%d,\"param\":%d}",tid,name?name:"",vmcmd,param);}
void send_ovs_cmd_req(int llid,int tid,char *tool,char *args)
  {SEND("{\"fctident\":\"ovs_cmd_req\",\"tid\":%d,\"tool\":\"%s\",\"args\":\"%s\"}",tid,tool?tool:"",args?args:"");}
void send_sav_vm(int llid,int tid,char *name,char *path)
  {SEND("{\"fctident\":\"sav_vm\",\"tid\":%d,\"name\":\"%s\",\"path\":\"%s\"}",tid,name,path);}
void send_sync_wireshark_req(int llid,int tid,char *name,int num,int cmd)
  {SEND("{\"fctident\":\"sync_wireshark_req\",\"tid\":%d,\"name\":\"%s\",\"num\":%d,\"cmd\":%d}",tid,name?name:"",num,cmd);}
void send_sync_wireshark_resp(int llid,int tid,char *name,int num,int status)
  {SEND("{\"fctident\":\"sync_wireshark_resp\",\"tid\":%d,\"name\":\"%s\",\"num\":%d,\"status\":%d}",tid,name?name:"",num,status);}
void send_topo_small_event(int llid,int tid,char *name,
                           char *p1,char *p2,int vm_evt)
  {SEND("{\"fctident\":\"topo_small_event\",\"tid\":%d,\"name\":\"%s\","
        "\"param1\":\"%s\",\"param2\":\"%s\",\"evt\":%d}",
        tid,name?name:"",p1?p1:"",p2?p2:"",vm_evt);}

#undef SEND

void send_list_pid_resp(int llid,int tid,int qty,t_pid_lst *pid)
{
  int i, len;
  len=sprintf(sndbuf,"{\"fctident\":\"list_pid_resp\",\"tid\":%d,\"qty\":%d,\"items\":[",tid,qty);
  for(i=0;i<qty;i++)
    len+=sprintf(sndbuf+len,
                 "%s{\"name\":\"%s\",\"pid\":%d,\"cpu\":%d,\"rss\":%d}",
                 i?",":"",pid?pid[i].name:"",pid?pid[i].pid:0,
                 pid?pid[i].cpu_tenth:0,pid?pid[i].rss_kb:0);
  len+=sprintf(sndbuf+len,"]}");
  my_msg_mngt_tx(llid,len,sndbuf);
}

void send_list_commands_resp(int llid,int tid,int qty,t_list_commands *list)
{
  int i, len;
  len=sprintf(sndbuf,"{\"fctident\":\"list_commands_resp\",\"tid\":%d,\"qty\":%d,\"items\":[",tid,qty);
  /* Escaped and bounded, as the ovs reply below: a line may carry quotes. */
  for(i=0;i<qty;i++)
    {
    if (len > MAX_SIZE_BIGGEST_MSG - 2*MAX_LIST_COMMANDS_LEN - 16)
      {
      KERR("WARNING list_commands_resp cut at %d of %d lines", i, qty);
      break;
      }
    len+=sprintf(sndbuf+len,"%s\"",i?",":"");
    len+=json_escape(sndbuf+len,list?list[i].cmd:"",MAX_SIZE_BIGGEST_MSG-len);
    len+=sprintf(sndbuf+len,"\"");
    }
  len+=sprintf(sndbuf+len,"]}");
  my_msg_mngt_tx(llid,len,sndbuf);
}

void send_ovs_cmd_resp(int llid,int tid,char *tool,int status,int qty,
                       t_list_commands *list)
{
  int i, len;
  len=sprintf(sndbuf,"{\"fctident\":\"ovs_cmd_resp\",\"tid\":%d,"
                     "\"tool\":\"%s\",\"status\":%d,\"qty\":%d,\"items\":[",
              tid,tool?tool:"",status,qty);
  /* Escaped (ovs output holds quotes and braces) and bounded (5000 lines of 256). */
  for(i=0;i<qty;i++)
    {
    if (len > MAX_SIZE_BIGGEST_MSG - 2*MAX_LIST_COMMANDS_LEN - 16)
      {
      KERR("WARNING ovs_cmd_resp for %s cut at %d of %d lines", tool, i, qty);
      break;
      }
    len+=sprintf(sndbuf+len,"%s\"",i?",":"");
    len+=json_escape(sndbuf+len,list?list[i].cmd:"",MAX_SIZE_BIGGEST_MSG-len);
    len+=sprintf(sndbuf+len,"\"");
    }
  len+=sprintf(sndbuf+len,"]}");
  my_msg_mngt_tx(llid,len,sndbuf);
}

void send_eventfull(int llid,int tid,int nb_endp,t_eventfull_endp *endp)
{
  int i, len;
  len=sprintf(sndbuf,"{\"fctident\":\"eventfull\",\"tid\":%d,\"nb_endp\":%d,\"items\":[",tid,nb_endp);
  for(i=0;i<nb_endp;i++)
    len+=sprintf(sndbuf+len,
      "%s{\"name\":\"%s\",\"num\":%d,\"type\":%d,\"ram\":%d,\"cpu\":%d,"
      "\"ok\":%d,\"ptx\":%d,\"prx\":%d,\"btx\":%d,\"brx\":%d,\"ms\":%d}",
      i?",":"",
      endp[i].name,endp[i].num,endp[i].type,endp[i].ram,endp[i].cpu,
      endp[i].ok,endp[i].ptx,endp[i].prx,endp[i].btx,endp[i].brx,endp[i].ms);
  len+=sprintf(sndbuf+len,"]}");
  my_msg_mngt_tx(llid,len,sndbuf);
}

static void send_slowp(int llid,int tid,int nb,t_slowperiodic *s,const char *msg)
{
  int i, len;
  len=sprintf(sndbuf,"{\"fctident\":\"%s\",\"tid\":%d,\"nb\":%d,\"items\":[",msg,tid,nb);
  for(i=0;i<nb;i++)
    len+=sprintf(sndbuf+len,"%s\"%s\"",i?",":"",s?s[i].name:"");
  len+=sprintf(sndbuf+len,"]}");
  my_msg_mngt_tx(llid,len,sndbuf);
}
void send_images_resp(int llid,int tid,char *runner,int qty,t_image_item *it)
{
  int i, len;
  len=sprintf(sndbuf,"{\"fctident\":\"images_resp\",\"tid\":%d,\"runner\":\"%s\",\"qty\":%d,\"items\":[",tid,runner,qty);
  for(i=0;i<qty;i++)
    len+=sprintf(sndbuf+len,"%s{\"kind\":\"%s\",\"name\":\"%s\",\"owner\":\"%s\"}",
                 i?",":"",it[i].kind,it[i].name,it[i].owner);
  len+=sprintf(sndbuf+len,"]}");
  my_msg_mngt_tx(llid,len,sndbuf);
}
void send_slowperiodic_qcow2(int llid,int tid,int nb,t_slowperiodic *s)
  {send_slowp(llid,tid,nb,s,"slowperiodic_qcow2");}
void send_slowperiodic_cnt(int llid,int tid,int nb,t_slowperiodic *s)
  {send_slowp(llid,tid,nb,s,"slowperiodic_cnt");}

void send_add_vm(int llid,int tid,t_topo_kvm *kvm)
{
  int len;
  if (!kvm) { len=sprintf(sndbuf,"{\"fctident\":\"add_vm\",\"tid\":%d}",tid);
              my_msg_mngt_tx(llid,len,sndbuf); return; }
  len=sprintf(sndbuf,
    "{\"fctident\":\"add_vm\",\"tid\":%d,"
    "\"name\":\"%s\",\"vm_id\":%d,\"color\":%d,"
    "\"vm_config_flags\":%d,"
    "\"mem\":%d,\"cpu\":%d,\"nb_tot_nb_vwif\":%d,\"nb_tot_eth\":%d,",
    tid,kvm->name,kvm->vm_id,kvm->color,
    kvm->vm_config_flags,
    kvm->mem,kvm->cpu,kvm->nb_tot_nb_vwif,kvm->nb_tot_eth);
  len+=emit_eth_table(sndbuf+len,kvm->eth_table,kvm->nb_tot_eth);
  len+=sprintf(sndbuf+len,
    ",\"rootfs_input\":\"%s\","
    "\"install_cdrom\":\"%s\",\"added_cdrom\":\"%s\","
    "\"added_disk\":\"%s\",\"rootfs_used\":\"%s\","
    "\"rootfs_backing\":\"%s\"}",
    kvm->rootfs_input,
    kvm->install_cdrom,kvm->added_cdrom,kvm->added_disk,
    kvm->rootfs_used,kvm->rootfs_backing);
  my_msg_mngt_tx(llid,len,sndbuf);
}

/* the airs of the radios of a cnt or a kvm, "a,b": the empty name is none */
static int emit_vwif_air(char *dst, int nb, char airs[MAX_VWIF][MAX_NAME_LEN])
{
  int i, len = 0;
  len += sprintf(dst+len, ",\"vwif_air\":\"");
  for (i = 0; (i < nb) && (i < MAX_VWIF); i++)
    len += sprintf(dst+len, "%s%s", i ? "," : "", airs[i]);
  len += sprintf(dst+len, "\"");
  return len;
}

static void parse_vwif_air(char *obj, char *name, int nb,
                           char airs[MAX_VWIF][MAX_NAME_LEN])
{
  char all[MAX_VWIF * MAX_NAME_LEN];
  char *p, *q;
  int i;
  memset(airs, 0, MAX_VWIF * MAX_NAME_LEN);
  memset(all, 0, sizeof(all));
  json_get_str(obj, "vwif_air", all, sizeof(all));
  p = all;
  for (i = 0; (i < nb) && (i < MAX_VWIF) && p; i++)
    {
    q = strchr(p, ',');
    if (q)
      *q = 0;
    strncpy(airs[i], p, MAX_NAME_LEN-1);
    p = q ? q + 1 : NULL;
    }
  if ((nb == 0) ? (all[0] != 0) : ((p != NULL) || (i < nb)))
    KERR("WARNING %s: %d radios and the airs \"%s\" do not match",
         name, nb, all);
}

void send_cnt_add(int llid,int tid,t_topo_cnt *cnt)
{
  int len;
  if (!cnt) { len=sprintf(sndbuf,"{\"fctident\":\"cnt_add\",\"tid\":%d}",tid);
              my_msg_mngt_tx(llid,len,sndbuf); return; }
  len=sprintf(sndbuf,
    "{\"fctident\":\"cnt_add\",\"tid\":%d,"
    "\"brandtype\":\"%s\",\"name\":\"%s\","
    "\"is_persistent\":%d,\"is_privileged\":%d,"
    "\"vm_id\":%d,\"color\":%d,\"image\":\"%s\","
    "\"ping_ok\":%d,\"nb_tot_nb_vwif\":%d,\"nb_tot_eth\":%d,",
    tid,cnt->brandtype,cnt->name,
    cnt->is_persistent,cnt->is_privileged,
    cnt->vm_id,cnt->color,cnt->image,
    cnt->ping_ok,cnt->nb_tot_nb_vwif,cnt->nb_tot_eth);
  len+=emit_eth_table(sndbuf+len,cnt->eth_table,cnt->nb_tot_eth);
  len+=sprintf(sndbuf+len,
    ",\"startup_env\":\"%s\",\"vmount\":\"%s\"}",
    cnt->startup_env,cnt->vmount);
  my_msg_mngt_tx(llid,len,sndbuf);
}

void send_work_dir_resp(int llid,int tid,t_topo_clc *clc)
{
  int len;
  if (!clc) { len=sprintf(sndbuf,"{\"fctident\":\"work_dir_resp\",\"tid\":%d}",tid);
              my_msg_mngt_tx(llid,len,sndbuf); return; }
  len=sprintf(sndbuf,
    "{\"fctident\":\"work_dir_resp\",\"tid\":%d,"
    "\"version\":\"%s\",\"network\":\"%s\","
    "\"username\":\"%s\",\"bin_dir\":\"%s\","
    "\"server_port\":%d,\"web_port\":%d}",
    tid,clc->version,clc->network,clc->username,clc->bin_dir,
    clc->server_port,clc->web_port);
  my_msg_mngt_tx(llid,len,sndbuf);
}

void send_event_topo(int llid,int tid,t_topo_info *topo)
{
  int i, j, len;
  t_topo_clc *cf;
  if (!topo) {
    len=sprintf(sndbuf,"{\"fctident\":\"event_topo\",\"tid\":%d}",tid);
    my_msg_mngt_tx(llid,len,sndbuf); return;
  }
  cf=&topo->clc;
  len=sprintf(sndbuf,
    "{\"fctident\":\"event_topo\",\"tid\":%d,\"conf_rank\":%d,"
    "\"version\":\"%s\",\"network\":\"%s\","
    "\"username\":\"%s\",\"bin_dir\":\"%s\","
    "\"server_port\":%d,"
    "\"nb_cnt\":%d,\"nb_kvm\":%d,"
    "\"nb_c2c\":%d,\"nb_tap\":%d,\"nb_phy\":%d,"
    "\"nb_a2b\":%d,\"nb_nat\":%d,\"nb_endp\":%d,"
    "\"nb_info_phy\":%d,\"nb_bridges\":%d,",
    tid,topo->conf_rank,
    cf->version,cf->network,cf->username,cf->bin_dir,
    cf->server_port,
    topo->nb_cnt,topo->nb_kvm,
    topo->nb_c2c,topo->nb_tap,topo->nb_phy,
    topo->nb_a2b,topo->nb_nat,topo->nb_endp,
    topo->nb_info_phy,topo->nb_bridges);

  len+=sprintf(sndbuf+len,"\"cnt\":[");
  for(i=0;i<topo->nb_cnt;i++){
    t_topo_cnt *c=&topo->cnt[i];
    len+=sprintf(sndbuf+len,
      "%s{\"brandtype\":\"%s\",\"name\":\"%s\","
      "\"is_persistent\":%d,\"is_privileged\":%d,"
      "\"vm_id\":%d,\"color\":%d,\"image\":\"%s\","
      "\"ping_ok\":%d,\"nb_tot_nb_vwif\":%d,\"nb_tot_eth\":%d,",
      i?",":"",c->brandtype,c->name,
      c->is_persistent,c->is_privileged,
      c->vm_id,c->color,c->image,
      c->ping_ok,c->nb_tot_nb_vwif,c->nb_tot_eth);
    len+=emit_eth_table(sndbuf+len,c->eth_table,c->nb_tot_eth);
    len+=emit_vwif_air(sndbuf+len,c->nb_tot_nb_vwif,c->vwif_air);
    len+=sprintf(sndbuf+len,",\"startup_env\":\"%s\",\"vmount\":\"%s\"}",
                 c->startup_env,c->vmount);
  }
  len+=sprintf(sndbuf+len,"],");

  len+=sprintf(sndbuf+len,"\"kvm\":[");
  for(i=0;i<topo->nb_kvm;i++){
    t_topo_kvm *k=&topo->kvm[i];
    len+=sprintf(sndbuf+len,
      "%s{\"name\":\"%s\",\"vm_id\":%d,\"color\":%d,"
      "\"install_cdrom\":\"%s\",\"added_cdrom\":\"%s\","
      "\"added_disk\":\"%s\","
      "\"rootfs_used\":\"%s\",\"rootfs_backing\":\"%s\","
      "\"vm_config_flags\":%d,"
      "\"mem\":%d,\"cpu\":%d,\"ping_ok\":%d,\"nb_tot_nb_vwif\":%d,"
      "\"nb_tot_eth\":%d,",
      i?",":"",k->name,k->vm_id,k->color,
      k->install_cdrom,k->added_cdrom,k->added_disk,
      k->rootfs_used,k->rootfs_backing,
      k->vm_config_flags,k->mem,k->cpu,
      k->ping_ok,k->nb_tot_nb_vwif,k->nb_tot_eth);
    len+=emit_eth_table(sndbuf+len,k->eth_table,k->nb_tot_eth);
    len+=emit_vwif_air(sndbuf+len,k->nb_tot_nb_vwif,k->vwif_air);
    len+=sprintf(sndbuf+len,"}");
  }
  len+=sprintf(sndbuf+len,"],");

  len+=sprintf(sndbuf+len,"\"c2c\":[");
  for(i=0;i<topo->nb_c2c;i++){
    t_topo_c2c *c=&topo->c2c[i];
    len+=sprintf(sndbuf+len,
      "%s{\"name\":\"%s\",\"endp_type\":%d,\"dist_cloon\":\"%s\","
      "\"attlan\":\"%s\",\"local_is_master\":%d,"
      "\"dist_tcp_ip\":%u,\"dist_tcp_port\":%u,"
      "\"loc_udp_ip\":%u,\"dist_udp_ip\":%u,"
      "\"loc_udp_port\":%u,\"dist_udp_port\":%u,"
      "\"tcp_connection\":%d,\"udp_connection\":%d}",
      i?",":"",c->name,c->endp_type,c->dist_cloon,
      c->attlan,c->local_is_master,
      c->dist_tcp_ip,c->dist_tcp_port,
      c->loc_udp_ip,c->dist_udp_ip,
      c->loc_udp_port,c->dist_udp_port,
      c->tcp_connection_peered,c->udp_connection_peered);
  }
  len+=sprintf(sndbuf+len,"],");

  len+=sprintf(sndbuf+len,"\"tap\":[");
  for(i=0;i<topo->nb_tap;i++)
    len+=sprintf(sndbuf+len,"%s{\"name\":\"%s\",\"endp_type\":%d}",
                 i?",":"",topo->tap[i].name,topo->tap[i].endp_type);
  len+=sprintf(sndbuf+len,"],");

  len+=sprintf(sndbuf+len,"\"phy\":[");
  for(i=0;i<topo->nb_phy;i++)
    len+=sprintf(sndbuf+len,"%s{\"name\":\"%s\",\"endp_type\":%d}",
                 i?",":"",topo->phy[i].name,topo->phy[i].endp_type);
  len+=sprintf(sndbuf+len,"],");

  len+=sprintf(sndbuf+len,"\"a2b\":[");
  for(i=0;i<topo->nb_a2b;i++)
    len+=sprintf(sndbuf+len,"%s{\"name\":\"%s\",\"endp_type0\":%d,\"endp_type1\":%d}",
                 i?",":"",topo->a2b[i].name,topo->a2b[i].endp_type0,topo->a2b[i].endp_type1);
  len+=sprintf(sndbuf+len,"],");

  len+=sprintf(sndbuf+len,"\"nat\":[");
  for(i=0;i<topo->nb_nat;i++)
    len+=sprintf(sndbuf+len,"%s{\"name\":\"%s\",\"endp_type\":%d}",
                 i?",":"",topo->nat[i].name,topo->nat[i].endp_type);
  len+=sprintf(sndbuf+len,"],");

  len+=sprintf(sndbuf+len,"\"endp\":[");
  for(i=0;i<topo->nb_endp;i++){
    t_topo_endp *e=&topo->endp[i];
    len+=sprintf(sndbuf+len,
      "%s{\"name\":\"%s\",\"num\":%d,\"type\":%d,\"nb_lan\":%d,\"lans\":[",
      i?",":"",e->name,e->num,e->type,e->lan.nb_lan);
    for(j=0;j<e->lan.nb_lan;j++)
      len+=sprintf(sndbuf+len,"%s\"%s\"",j?",":"",e->lan.lan[j].lan);
    len+=sprintf(sndbuf+len,"]}");
  }
  len+=sprintf(sndbuf+len,"],");

  len+=sprintf(sndbuf+len,"\"info_phy\":[");
  for(i=0;i<topo->nb_info_phy;i++)
    len+=sprintf(sndbuf+len,"%s\"%s\"",i?",":"",topo->info_phy[i].name);
  len+=sprintf(sndbuf+len,"],");

  len+=sprintf(sndbuf+len,"\"bridges\":[");
  for(i=0;i<topo->nb_bridges;i++){
    t_topo_bridges *b=&topo->bridges[i];
    len+=sprintf(sndbuf+len,"%s{\"br\":\"%s\",\"nb_ports\":%d,\"ports\":[",
                 i?",":"",b->br,b->nb_ports);
    for(j=0;j<b->nb_ports;j++)
      len+=sprintf(sndbuf+len,"%s\"%s\"",j?",":"",b->ports[j]);
    len+=sprintf(sndbuf+len,"]}");
  }
  len+=sprintf(sndbuf+len,"]}");
  my_msg_mngt_tx(llid,len,sndbuf);
}

void send_c2c_add(int llid,int tid,char *c2c_name,
                  uint32_t local_udp_ip,char *slave_cloon,
                  uint32_t ip,uint16_t port,char *passwd,
                  uint32_t udp_ip,uint16_t c2c_udp_port_low)
{
  int len=sprintf(sndbuf,
    "{\"fctident\":\"c2c_add\",\"tid\":%d,\"name\":\"%s\","
    "\"local_udp_ip\":%u,\"slave_net\":\"%s\","
    "\"ip\":%u,\"port\":%u,\"passwd\":\"%s\","
    "\"udp_ip\":%u,\"c2c_udp_port_low\":%u}",
    tid,c2c_name?c2c_name:"",local_udp_ip,slave_cloon?slave_cloon:"",
    ip,(unsigned)port,passwd?passwd:"",udp_ip,(unsigned)c2c_udp_port_low);
  my_msg_mngt_tx(llid,len,sndbuf);
}

void send_c2c_peer_create(int llid,int tid,char *c2c_name,int is_ack,
                          char *local_cloon,char *distant_cloon)
{
  int len=sprintf(sndbuf,
    "{\"fctident\":\"c2c_create\",\"tid\":%d,\"name\":\"%s\","
    "\"is_ack\":%d,\"mnet\":\"%s\",\"snet\":\"%s\"}",
    tid,c2c_name?c2c_name:"",is_ack,
    local_cloon?local_cloon:"",distant_cloon?distant_cloon:"");
  my_msg_mngt_tx(llid,len,sndbuf);
}

void send_c2c_peer_conf(int llid,int tid,char *c2c_name,int is_ack,
                        char *local_cloon,char *distant_cloon,
                        uint32_t local_udp_ip,uint32_t distant_udp_ip,
                        uint16_t local_udp_port,uint16_t distant_udp_port)
{
  int len=sprintf(sndbuf,
    "{\"fctident\":\"c2c_conf\",\"tid\":%d,\"name\":\"%s\","
    "\"is_ack\":%d,\"mnet\":\"%s\",\"snet\":\"%s\","
    "\"mip\":%u,\"sip\":%u,\"mport\":%u,\"sport\":%u}",
    tid,c2c_name?c2c_name:"",is_ack,
    local_cloon?local_cloon:"",distant_cloon?distant_cloon:"",
    local_udp_ip,distant_udp_ip,
    (unsigned)local_udp_port,(unsigned)distant_udp_port);
  my_msg_mngt_tx(llid,len,sndbuf);
}

void send_c2c_peer_ping(int llid,int tid,char *c2c_name,int status)
{
  int len=sprintf(sndbuf,
    "{\"fctident\":\"c2c_ping\",\"tid\":%d,\"name\":\"%s\",\"status\":%d}",
    tid,c2c_name?c2c_name:"",status);
  my_msg_mngt_tx(llid,len,sndbuf);
}

/* A packet does not fit the stack buffers the other decoders use: 64k of
   payload is 88k of base64. */
static void decode_pcap_traffic(int llid, int tid, char *buf)
{
  static char    b64[((MAX_PCAP_PKT_LEN/3)+2)*4 + 16];
  static uint8_t raw[MAX_PCAP_PKT_LEN];
  char name[MAX_NAME_LEN];
  int num=0, type=0, caplen=0, len=0, got;
  int pkt_nb=0, sec=0, usec=0;

  json_get_str(buf,"name",name,MAX_NAME_LEN);
  json_get_int(buf,"num",&num);
  json_get_int(buf,"type",&type);
  json_get_int(buf,"pkt_nb",&pkt_nb);
  json_get_int(buf,"sec",&sec);
  json_get_int(buf,"usec",&usec);
  json_get_int(buf,"caplen",&caplen);
  json_get_int(buf,"len",&len);
  json_get_str(buf,"data",b64,sizeof(b64));
  got = b64_decode(raw, b64, sizeof(raw));
  if (got < 0)
    {
    KERR("ERROR pcap_traffic %s %d payload too big", name, num);
    return;
    }
  if (got != caplen)
    {
    KERR("ERROR pcap_traffic %s %d caplen %d but %d bytes", name, num,
         caplen, got);
    caplen = got;
    }
  recv_pcap_traffic(llid, tid, name, num, type, (uint32_t) pkt_nb,
                    (uint32_t) sec, (uint32_t) usec, caplen, len, raw);
}

/* A whole qemu command line: too big for the decode_simple_msgs buffers. */
static void decode_trace(int llid, int tid, char *buf)
{
  static char txt[MAX_TRACE_LEN];
  char name[MAX_NAME_LEN];
  int kind=0;
  json_get_int(buf,"kind",&kind);
  json_get_str(buf,"name",name,MAX_NAME_LEN);
  json_get_str(buf,"txt",txt,sizeof(txt));
  recv_trace(llid,tid,kind,name,txt);
}

static void decode_simple_msgs(int llid, int tid, int bnd, char *buf)
{
  char name[MAX_NAME_LEN];
  char txt[MAX_PRINT_LEN];
  char str1[MAX_PATH_LEN];

  switch(bnd) {
  case bnd_work_dir_req:           recv_work_dir_req(llid,tid);           break;
  case bnd_kill_uml_clownix:       recv_kill_uml_clownix(llid,tid);       break;
  case bnd_del_all:                recv_del_all(llid,tid);                break;
  case bnd_list_pid_req:           recv_list_pid_req(llid,tid);           break;
  case bnd_images_req:             recv_images_req(llid,tid);             break;
  case bnd_eventfull_sub:          recv_eventfull_sub(llid,tid);          break;
  case bnd_slowperiodic_sub:       recv_slowperiodic_sub(llid,tid);       break;
  case bnd_topo_small_event_sub:   recv_topo_small_event_sub(llid,tid);   break;
  case bnd_event_topo_sub:         recv_event_topo_sub(llid,tid);         break;

  case bnd_del_name:
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    recv_del_name(llid,tid,name); break;
  case bnd_nat_add:
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    memset(txt, 0, MAX_PATH_LEN);
    json_get_str(buf,"ports",txt,MAX_PATH_LEN);
    recv_nat_add(llid,tid,name,txt); break;
  case bnd_tap_add:
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    recv_tap_add(llid,tid,name); break;
  case bnd_a2b_add:
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    recv_a2b_add(llid,tid,name); break;
  case bnd_phy_add: {
    int type=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"type",&type);
    recv_phy_add(llid,tid,name,type); break;
  }
  case bnd_snf_add: {
    int num=0,val=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"num",&num); json_get_int(buf,"val",&val);
    recv_snf_add(llid,tid,name,num,val); break;
  }
  case bnd_pcap_open: case bnd_pcap_close: {
    int num=0;
    static char filter[MAX_PCAP_FILTER_LEN];
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"num",&num);
    if (bnd == bnd_pcap_open)
      {
      filter[0]=0;
      json_get_str(buf,"filter",filter,MAX_PCAP_FILTER_LEN);
      recv_pcap_open(llid,tid,name,num,filter);
      }
    else
      recv_pcap_close(llid,tid,name,num);
    break;
  }
  case bnd_item_pos: {
    int x=0, y=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"x",&x);
    json_get_int(buf,"y",&y);
    recv_item_pos(llid,tid,name,x,y); break;
  }
  case bnd_item_poseth: {
    int num=0, deg=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"num",&num);
    json_get_int(buf,"deg",&deg);
    recv_item_poseth(llid,tid,name,num,deg); break;
  }
  case bnd_a2b_cnf:
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_str(buf,"cmd",txt,MAX_PRINT_LEN);
    recv_a2b_cnf(llid,tid,name,txt); break;
  case bnd_nat_cnf:
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_str(buf,"cmd",txt,MAX_PRINT_LEN);
    recv_nat_cnf(llid,tid,name,txt); break;
  case bnd_cnt_cnf:
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_str(buf,"cmd",txt,MAX_PRINT_LEN);
    recv_cnt_cnf(llid,tid,name,txt); break;
  case bnd_lan_cnf:
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_str(buf,"cmd",txt,MAX_PRINT_LEN);
    recv_lan_cnf(llid,tid,name,txt); break;
  case bnd_c2c_cnf:
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_str(buf,"cmd",txt,MAX_PRINT_LEN);
    recv_c2c_cnf(llid,tid,name,txt); break;
  case bnd_add_lan_endp: {
    int num=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"num",&num);
    json_get_str(buf,"lan",str1,MAX_NAME_LEN);
    recv_add_lan_endp(llid,tid,name,num,str1); break;
  }
  case bnd_del_lan_endp: {
    int num=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"num",&num);
    json_get_str(buf,"lan",str1,MAX_NAME_LEN);
    recv_del_lan_endp(llid,tid,name,num,str1); break;
  }
  case bnd_status_ok:
    json_get_str(buf,"txt",txt,MAX_PRINT_LEN);
    recv_status_ok(llid,tid,txt); break;
  case bnd_status_ko:
    json_get_str(buf,"reason",txt,MAX_PRINT_LEN);
    recv_status_ko(llid,tid,txt); break;
  case bnd_sav_vm:
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_str(buf,"path",str1,MAX_PATH_LEN);
    recv_sav_vm(llid,tid,name,str1); break;
  case bnd_ovs_cmd_req:
    json_get_str(buf,"tool",name,MAX_NAME_LEN);
    json_get_str(buf,"args",str1,MAX_PATH_LEN);
    recv_ovs_cmd_req(llid,tid,name,str1); break;
  case bnd_vmcmd: {
    int cmd=0,param=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"cmd",&cmd); json_get_int(buf,"param",&param);
    recv_vmcmd(llid,tid,name,cmd,param); break;
  }
  case bnd_list_commands_req: {
    int is_layout=0;
    json_get_int(buf,"is_layout",&is_layout);
    recv_list_commands_req(llid,tid,is_layout); break;
  }
  case bnd_trace_sub: {
    int kind=0,sub_on=0;
    json_get_int(buf,"kind",&kind); json_get_int(buf,"sub_on",&sub_on);
    recv_trace_sub(llid,tid,kind,sub_on); break;
  }
  case bnd_topo_small_event: {
    char p1[MAX_PATH_LEN], p2[MAX_PATH_LEN]; int evt=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_str(buf,"param1",p1,MAX_PATH_LEN);
    json_get_str(buf,"param2",p2,MAX_PATH_LEN);
    json_get_int(buf,"evt",&evt);
    recv_topo_small_event(llid,tid,name,p1,p2,evt); break;
  }
  case bnd_sync_wireshark_req: {
    int num=0,cmd=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"num",&num); json_get_int(buf,"cmd",&cmd);
    recv_sync_wireshark_req(llid,tid,name,num,cmd); break;
  }
  case bnd_sync_wireshark_resp: {
    int num=0,status=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"num",&num); json_get_int(buf,"status",&status);
    recv_sync_wireshark_resp(llid,tid,name,num,status); break;
  }
  default:
    KOUT("decode_simple_msgs: unexpected bnd=%d", bnd);
  }
}

static void decode_list_pid_resp(int llid, int tid, char *buf)
{
  int qty=0, i; char *p; t_pid_lst *pid;
  json_get_int(buf,"qty",&qty);
  if(qty<0||qty>65536) KOUT("%d",qty);
  pid=(t_pid_lst*)clownix_malloc(qty*sizeof(t_pid_lst),7);
  memset(pid,0,qty*sizeof(t_pid_lst));
  p=strstr(buf,"\"items\""); if(!p) KOUT("%s",buf);
  p=strchr(p,'[');           if(!p) KOUT("%s",buf);
  for(i=0;i<qty;i++){
    char *obj=strchr(p,'{'); if(!obj) KOUT("%s",buf);
    json_get_str(obj,"name",pid[i].name,MAX_NAME_LEN);
    json_get_int(obj,"pid",&pid[i].pid);
    json_get_int(obj,"cpu",&pid[i].cpu_tenth);
    json_get_int(obj,"rss",&pid[i].rss_kb);
    p=strchr(obj,'}'); if(!p) KOUT("%s",buf);
  }
  recv_list_pid_resp(llid,tid,qty,pid);
  clownix_free(pid,__FUNCTION__);
}

static void decode_images_resp(int llid, int tid, char *buf)
{
  int qty=0, i; char *p; t_image_item *it; char runner[MAX_NAME_LEN];
  json_get_int(buf,"qty",&qty);
  if(qty<0||qty>65536) KOUT("%d",qty);
  json_get_str(buf,"runner",runner,MAX_NAME_LEN);
  it=(t_image_item*)clownix_malloc((qty+1)*sizeof(t_image_item),7);
  memset(it,0,(qty+1)*sizeof(t_image_item));
  p=strstr(buf,"\"items\""); if(!p) KOUT("%s",buf);
  p=strchr(p,'[');           if(!p) KOUT("%s",buf);
  for(i=0;i<qty;i++){
    char *obj=strchr(p,'{'); if(!obj) KOUT("%s",buf);
    json_get_str(obj,"kind",it[i].kind,MAX_NAME_LEN);
    json_get_str(obj,"name",it[i].name,MAX_NAME_LEN);
    json_get_str(obj,"owner",it[i].owner,MAX_NAME_LEN);
    p=strchr(obj,'}'); if(!p) KOUT("%s",buf);
  }
  recv_images_resp(llid,tid,runner,qty,it);
  clownix_free(it,__FUNCTION__);
}

static void decode_list_commands_resp(int llid, int tid, char *buf)
{
  int qty=0, i; char *p; t_list_commands *list;
  json_get_int(buf,"qty",&qty);
  if(qty<0||qty>65536) KOUT("%d",qty);
  list=(t_list_commands*)clownix_malloc(qty*sizeof(t_list_commands),7);
  memset(list,0,qty*sizeof(t_list_commands));
  p=strstr(buf,"\"items\""); if(!p) KOUT("%s",buf);
  p=strchr(p,'[');           if(!p) KOUT("%s",buf);
  for(i=0;i<qty;i++){
    char *s=strchr(p,'"'); if(!s) KOUT("%s",buf); s++;
    char *e=strchr(s,'"'); if(!e) KOUT("%s",buf);
    int sl=(int)(e-s);
    if(sl>=MAX_LIST_COMMANDS_LEN) sl=MAX_LIST_COMMANDS_LEN-1;
    memcpy(list[i].cmd,s,sl); list[i].cmd[sl]=0;
    p=e+1;
  }
  recv_list_commands_resp(llid,tid,qty,list);
  clownix_free(list,__FUNCTION__);
}

static void decode_ovs_cmd_resp(int llid, int tid, char *buf)
{
  int qty=0, status=0, i; char *p; char tool[MAX_NAME_LEN];
  t_list_commands *list;
  memset(tool, 0, MAX_NAME_LEN);
  json_get_str(buf,"tool",tool,MAX_NAME_LEN);
  json_get_int(buf,"status",&status);
  json_get_int(buf,"qty",&qty);
  if(qty<0||qty>65536) KOUT("%d",qty);
  list=(t_list_commands*)clownix_malloc(qty*sizeof(t_list_commands),7);
  memset(list,0,qty*sizeof(t_list_commands));
  p=strstr(buf,"\"items\""); if(!p) KOUT("%s",buf);
  p=strchr(p,'[');           if(!p) KOUT("%s",buf);
  for(i=0;i<qty;i++){
    char *s=strchr(p,'"'); if(!s) KOUT("%s",buf); s++;
    char *e=strchr(s,'"'); if(!e) KOUT("%s",buf);
    int sl=(int)(e-s);
    if(sl>=MAX_LIST_COMMANDS_LEN) sl=MAX_LIST_COMMANDS_LEN-1;
    memcpy(list[i].cmd,s,sl); list[i].cmd[sl]=0;
    p=e+1;
  }
  recv_ovs_cmd_resp(llid,tid,tool,status,qty,list);
  clownix_free(list,__FUNCTION__);
}

static void decode_eventfull(int llid, int tid, char *buf)
{
  int nb=0, i; char *p; t_eventfull_endp *endp;
  json_get_int(buf,"nb_endp",&nb);
  if(nb<0||nb>65536) KOUT("%d",nb);
  endp=(t_eventfull_endp*)clownix_malloc(nb*sizeof(t_eventfull_endp),7);
  memset(endp,0,nb*sizeof(t_eventfull_endp));
  p=strstr(buf,"\"items\""); if(!p) KOUT("%s",buf);
  p=strchr(p,'[');           if(!p) KOUT("%s",buf);
  for(i=0;i<nb;i++){
    char *obj=strchr(p,'{'); if(!obj) KOUT("%s",buf);
    json_get_str(obj,"name",endp[i].name,MAX_NAME_LEN);
    json_get_int(obj,"num",&endp[i].num);
    json_get_int(obj,"type",&endp[i].type);
    json_get_int(obj,"ram",&endp[i].ram);
    json_get_int(obj,"cpu",&endp[i].cpu);
    json_get_int(obj,"ok",&endp[i].ok);
    json_get_int(obj,"ptx",&endp[i].ptx);
    json_get_int(obj,"prx",&endp[i].prx);
    json_get_int(obj,"btx",&endp[i].btx);
    json_get_int(obj,"brx",&endp[i].brx);
    json_get_int(obj,"ms",&endp[i].ms);
    p=strchr(obj,'}'); if(!p) KOUT("%s",buf);
  }
  recv_eventfull(llid,tid,nb,endp);
  clownix_free(endp,__FUNCTION__);
}

static void decode_slowperiodic(int llid, int tid, int bnd, char *buf)
{
  int nb=0, i; char *p; t_slowperiodic *spic;
  json_get_int(buf,"nb",&nb);
  if(nb<0||nb>65536) KOUT("%d",nb);
  spic=(t_slowperiodic*)clownix_malloc(nb*sizeof(t_slowperiodic),7);
  memset(spic,0,nb*sizeof(t_slowperiodic));
  p=strstr(buf,"\"items\""); if(!p) KOUT("%s",buf);
  p=strchr(p,'[');           if(!p) KOUT("%s",buf);
  for(i=0;i<nb;i++){
    char *s=strchr(p,'"'); if(!s) KOUT("%s",buf); s++;
    char *e=strchr(s,'"'); if(!e) KOUT("%s",buf);
    int sl=(int)(e-s);
    if(sl>=MAX_NAME_LEN) sl=MAX_NAME_LEN-1;
    memcpy(spic[i].name,s,sl); spic[i].name[sl]=0;
    p=e+1;
  }
  if(bnd==bnd_slowperiodic_qcow2)
    recv_slowperiodic_qcow2(llid,tid,nb,spic);
  else
    recv_slowperiodic_cnt(llid,tid,nb,spic);
  clownix_free(spic,__FUNCTION__);
}

static void decode_add_vm(int llid, int tid, char *buf)
{
  t_topo_kvm kvm; int nb_eth=0; char *p;
  memset(&kvm,0,sizeof(kvm));
  json_get_str(buf,"name",kvm.name,MAX_NAME_LEN);
  json_get_int(buf,"vm_id",&kvm.vm_id);
  json_get_int(buf,"color",&kvm.color);
  json_get_int(buf,"vm_config_flags",&kvm.vm_config_flags);
  json_get_int(buf,"mem",&kvm.mem);
  json_get_int(buf,"cpu",&kvm.cpu);
  json_get_int(buf,"nb_tot_nb_vwif",&kvm.nb_tot_nb_vwif);
  json_get_int(buf,"nb_tot_eth",&nb_eth);
  kvm.nb_tot_eth=nb_eth;
  p=strstr(buf,"\"eth_table\"");
  if(p) parse_eth_table(p,nb_eth,kvm.eth_table);
  json_get_str(buf,"rootfs_input",kvm.rootfs_input,MAX_PATH_LEN);
  json_get_str(buf,"install_cdrom",kvm.install_cdrom,MAX_PATH_LEN);
  json_get_str(buf,"added_cdrom",kvm.added_cdrom,MAX_PATH_LEN);
  json_get_str(buf,"added_disk",kvm.added_disk,MAX_PATH_LEN);
  json_get_str(buf,"rootfs_used",kvm.rootfs_used,MAX_PATH_LEN);
  json_get_str(buf,"rootfs_backing",kvm.rootfs_backing,MAX_PATH_LEN);
  recv_add_vm(llid,tid,&kvm);
}

static void decode_cnt_add(int llid, int tid, char *buf)
{
  t_topo_cnt cnt; int nb_eth=0; char *p;
  memset(&cnt,0,sizeof(cnt));
  json_get_str(buf,"brandtype",cnt.brandtype,MAX_NAME_LEN);
  json_get_str(buf,"name",cnt.name,MAX_NAME_LEN);
  json_get_int(buf,"is_persistent",&cnt.is_persistent);
  json_get_int(buf,"is_privileged",&cnt.is_privileged);
  json_get_int(buf,"vm_id",&cnt.vm_id);
  json_get_int(buf,"color",&cnt.color);
  json_get_str(buf,"image",cnt.image,MAX_PATH_LEN);
  json_get_int(buf,"ping_ok",&cnt.ping_ok);
  json_get_int(buf,"nb_tot_nb_vwif",&cnt.nb_tot_nb_vwif);
  json_get_int(buf,"nb_tot_eth",&nb_eth);
  cnt.nb_tot_eth=nb_eth;
  p=strstr(buf,"\"eth_table\"");
  if(p) parse_eth_table(p,nb_eth,cnt.eth_table);
  json_get_str(buf,"startup_env",cnt.startup_env,MAX_PATH_LEN);
  json_get_str(buf,"vmount",cnt.vmount,MAX_SIZE_VMOUNT);
  recv_cnt_add(llid,tid,&cnt);
}

static void decode_work_dir_resp(int llid, int tid, char *buf)
{
  t_topo_clc clc;
  memset(&clc,0,sizeof(clc));
  json_get_str(buf,"version",clc.version,MAX_NAME_LEN);
  json_get_str(buf,"network",clc.network,MAX_NAME_LEN);
  json_get_str(buf,"username",clc.username,MAX_NAME_LEN);
  json_get_str(buf,"bin_dir",clc.bin_dir,MAX_PATH_LEN);
  json_get_int(buf,"server_port",&clc.server_port);
  json_get_int(buf,"web_port",&clc.web_port);
  recv_work_dir_resp(llid,tid,&clc);
}

static void decode_event_topo(int llid, int tid, char *buf)
{
  int i, j; char *p; t_topo_info *topo;
  topo=(t_topo_info*)clownix_malloc(sizeof(t_topo_info),7);
  memset(topo,0,sizeof(t_topo_info));
  json_get_int(buf,"conf_rank",&topo->conf_rank);
  json_get_str(buf,"version",topo->clc.version,MAX_NAME_LEN);
  json_get_str(buf,"network",topo->clc.network,MAX_NAME_LEN);
  json_get_str(buf,"username",topo->clc.username,MAX_NAME_LEN);
  json_get_str(buf,"bin_dir",topo->clc.bin_dir,MAX_PATH_LEN);
  json_get_int(buf,"server_port",&topo->clc.server_port);
  json_get_int(buf,"nb_cnt",&topo->nb_cnt);
  json_get_int(buf,"nb_kvm",&topo->nb_kvm);
  json_get_int(buf,"nb_c2c",&topo->nb_c2c);
  json_get_int(buf,"nb_tap",&topo->nb_tap);
  json_get_int(buf,"nb_phy",&topo->nb_phy);
  json_get_int(buf,"nb_a2b",&topo->nb_a2b);
  json_get_int(buf,"nb_nat",&topo->nb_nat);
  json_get_int(buf,"nb_endp",&topo->nb_endp);
  json_get_int(buf,"nb_info_phy",&topo->nb_info_phy);
  json_get_int(buf,"nb_bridges",&topo->nb_bridges);

  if(topo->nb_cnt){
    topo->cnt=(t_topo_cnt*)clownix_malloc(topo->nb_cnt*sizeof(t_topo_cnt),7);
    memset(topo->cnt,0,topo->nb_cnt*sizeof(t_topo_cnt));
    p=strstr(buf,"\"cnt\""); if(!p) KOUT("%s",buf);
    p=strchr(p,'[');         if(!p) KOUT("%s",buf);
    for(i=0;i<topo->nb_cnt;i++){
      char *obj=strchr(p,'{'); if(!obj) KOUT("%s",buf);
      t_topo_cnt *c=&topo->cnt[i]; int nb_eth=0; char *ep;
      json_get_str(obj,"brandtype",c->brandtype,MAX_NAME_LEN);
      json_get_str(obj,"name",c->name,MAX_NAME_LEN);
      json_get_int(obj,"is_persistent",&c->is_persistent);
      json_get_int(obj,"is_privileged",&c->is_privileged);
      json_get_int(obj,"vm_id",&c->vm_id);
      json_get_int(obj,"color",&c->color);
      json_get_str(obj,"image",c->image,MAX_PATH_LEN);
      json_get_int(obj,"ping_ok",&c->ping_ok);
      json_get_int(obj,"nb_tot_nb_vwif",&c->nb_tot_nb_vwif);
      parse_vwif_air(obj,c->name,c->nb_tot_nb_vwif,c->vwif_air);
      json_get_int(obj,"nb_tot_eth",&nb_eth);
      c->nb_tot_eth=nb_eth;
      ep=strstr(obj,"\"eth_table\"");
      if(ep) parse_eth_table(ep,nb_eth,c->eth_table);
      json_get_str(obj,"startup_env",c->startup_env,MAX_PATH_LEN);
      json_get_str(obj,"vmount",c->vmount,MAX_SIZE_VMOUNT);
      char *ep2=strstr(obj,"\"vmount\"");
      if(ep2){p=strchr(ep2,'}');if(!p)KOUT("%s",buf);}
      else{p=strchr(obj,'}');if(!p)KOUT("%s",buf);}
    }
  }

  if(topo->nb_kvm){
    topo->kvm=(t_topo_kvm*)clownix_malloc(topo->nb_kvm*sizeof(t_topo_kvm),7);
    memset(topo->kvm,0,topo->nb_kvm*sizeof(t_topo_kvm));
    p=strstr(buf,"\"kvm\""); if(!p) KOUT("%s",buf);
    p=strchr(p,'[');         if(!p) KOUT("%s",buf);
    for(i=0;i<topo->nb_kvm;i++){
      char *obj=strchr(p,'{'); if(!obj) KOUT("%s",buf);
      t_topo_kvm *k=&topo->kvm[i]; int nb_eth=0; char *ep;
      json_get_str(obj,"name",k->name,MAX_NAME_LEN);
      json_get_int(obj,"vm_id",&k->vm_id);
      json_get_int(obj,"color",&k->color);
      json_get_str(obj,"install_cdrom",k->install_cdrom,MAX_PATH_LEN);
      json_get_str(obj,"added_cdrom",k->added_cdrom,MAX_PATH_LEN);
      json_get_str(obj,"added_disk",k->added_disk,MAX_PATH_LEN);
      json_get_str(obj,"rootfs_used",k->rootfs_used,MAX_PATH_LEN);
      json_get_str(obj,"rootfs_backing",k->rootfs_backing,MAX_PATH_LEN);
      json_get_int(obj,"vm_config_flags",&k->vm_config_flags);
      json_get_int(obj,"mem",&k->mem);
      json_get_int(obj,"cpu",&k->cpu);
      json_get_int(obj,"ping_ok",&k->ping_ok);
      json_get_int(obj,"nb_tot_nb_vwif",&k->nb_tot_nb_vwif);
      json_get_int(obj,"nb_tot_eth",&nb_eth);
      k->nb_tot_eth=nb_eth;
      ep=strstr(obj,"\"eth_table\"");
      if(ep) parse_eth_table(ep,nb_eth,k->eth_table);
      parse_vwif_air(obj,k->name,k->nb_tot_nb_vwif,k->vwif_air);
      if(ep){char*eq=strchr(ep,']');p=eq?eq:obj;}
      else p=obj;
      char *nx=strchr(p,'}'); if(!nx) KOUT("%s",buf); p=nx;
    }
  }

  if(topo->nb_c2c){
    topo->c2c=(t_topo_c2c*)clownix_malloc(topo->nb_c2c*sizeof(t_topo_c2c),7);
    memset(topo->c2c,0,topo->nb_c2c*sizeof(t_topo_c2c));
    p=strstr(buf,"\"c2c\""); if(!p) KOUT("%s",buf);
    p=strchr(p,'[');         if(!p) KOUT("%s",buf);
    for(i=0;i<topo->nb_c2c;i++){
      char *obj=strchr(p,'{'); if(!obj) KOUT("%s",buf);
      t_topo_c2c *c=&topo->c2c[i];
      json_get_str(obj,"name",c->name,MAX_NAME_LEN);
      json_get_int(obj,"endp_type",&c->endp_type);
      json_get_str(obj,"dist_cloon",c->dist_cloon,MAX_NAME_LEN);
      json_get_str(obj,"attlan",c->attlan,MAX_NAME_LEN);
      json_get_int(obj,"local_is_master",&c->local_is_master);
      { unsigned int v=0;
        json_get_uint(obj,"dist_tcp_ip",&v); c->dist_tcp_ip=v;
        json_get_uint(obj,"dist_tcp_port",&v); c->dist_tcp_port=v;
        json_get_uint(obj,"loc_udp_ip",&v); c->loc_udp_ip=v;
        json_get_uint(obj,"dist_udp_ip",&v); c->dist_udp_ip=v;
        json_get_uint(obj,"loc_udp_port",&v); c->loc_udp_port=v;
        json_get_uint(obj,"dist_udp_port",&v); c->dist_udp_port=v; }
      json_get_int(obj,"tcp_connection",&c->tcp_connection_peered);
      json_get_int(obj,"udp_connection",&c->udp_connection_peered);
      p=strchr(obj,'}'); if(!p) KOUT("%s",buf);
    }
  }

  PARSE_SIMPLE_ARRAY(tap,nb_tap,t_topo_tap,name,endp_type)
  PARSE_SIMPLE_ARRAY(phy,nb_phy,t_topo_phy,name,endp_type)
  PARSE_SIMPLE_ARRAY(nat,nb_nat,t_topo_nat,name,endp_type)

  if(topo->nb_a2b){
    topo->a2b=(t_topo_a2b*)clownix_malloc(topo->nb_a2b*sizeof(t_topo_a2b),7);
    memset(topo->a2b,0,topo->nb_a2b*sizeof(t_topo_a2b));
    p=strstr(buf,"\"a2b\""); if(!p) KOUT("%s",buf);
    p=strchr(p,'[');         if(!p) KOUT("%s",buf);
    for(i=0;i<topo->nb_a2b;i++){
      char *obj=strchr(p,'{'); if(!obj) KOUT("%s",buf);
      json_get_str(obj,"name",topo->a2b[i].name,MAX_NAME_LEN);
      json_get_int(obj,"endp_type0",&topo->a2b[i].endp_type0);
      json_get_int(obj,"endp_type1",&topo->a2b[i].endp_type1);
      p=strchr(obj,'}'); if(!p) KOUT("%s",buf);
    }
  }

  if(topo->nb_endp){
    topo->endp=(t_topo_endp*)clownix_malloc(topo->nb_endp*sizeof(t_topo_endp),7);
    memset(topo->endp,0,topo->nb_endp*sizeof(t_topo_endp));
    p=strstr(buf,"\"endp\""); if(!p) KOUT("%s",buf);
    p=strchr(p,'[');          if(!p) KOUT("%s",buf);
    for(i=0;i<topo->nb_endp;i++){
      char *obj=strchr(p,'{'); if(!obj) KOUT("%s",buf);
      t_topo_endp *e=&topo->endp[i]; int nb_lan=0;
      json_get_str(obj,"name",e->name,MAX_NAME_LEN);
      json_get_int(obj,"num",&e->num);
      json_get_int(obj,"type",&e->type);
      json_get_int(obj,"nb_lan",&nb_lan);
      e->lan.nb_lan=nb_lan;
      if(nb_lan>0){
        int len2=nb_lan*sizeof(t_lan_group_item);
        e->lan.lan=(t_lan_group_item*)clownix_malloc(len2,3);
        memset(e->lan.lan,0,len2);
        char *lp=strstr(obj,"\"lans\""); if(!lp) KOUT("%s",buf);
        lp=strchr(lp,'[');              if(!lp) KOUT("%s",buf);
        for(j=0;j<nb_lan;j++){
          char *s=strchr(lp,'"'); if(!s) KOUT("%s",buf); s++;
          char *eq=strchr(s,'"'); if(!eq) KOUT("%s",buf);
          int sl=(int)(eq-s);
          if(sl>=MAX_NAME_LEN) sl=MAX_NAME_LEN-1;
          memcpy(e->lan.lan[j].lan,s,sl); e->lan.lan[j].lan[sl]=0;
          lp=eq+1;
        }
      }
      char *ep2=strstr(obj,"\"lans\"");
      if(ep2){char *eq=strchr(ep2,']');p=eq?eq:obj;}
      else p=obj;
      char *nx=strchr(p,'}');if(!nx)KOUT("%s",buf);p=nx;
    }
  }

  if(topo->nb_info_phy){
    topo->info_phy=(t_topo_info_phy*)clownix_malloc(
        topo->nb_info_phy*sizeof(t_topo_info_phy),7);
    memset(topo->info_phy,0,topo->nb_info_phy*sizeof(t_topo_info_phy));
    p=strstr(buf,"\"info_phy\""); if(!p) KOUT("%s",buf);
    p=strchr(p,'[');              if(!p) KOUT("%s",buf);
    for(i=0;i<topo->nb_info_phy;i++){
      char *s=strchr(p,'"'); if(!s) KOUT("%s",buf); s++;
      char *e=strchr(s,'"'); if(!e) KOUT("%s",buf);
      int sl=(int)(e-s);
      if(sl>=MAX_NAME_LEN) sl=MAX_NAME_LEN-1;
      memcpy(topo->info_phy[i].name,s,sl);
      topo->info_phy[i].name[sl]=0; p=e+1;
    }
  }

  if(topo->nb_bridges){
    topo->bridges=(t_topo_bridges*)clownix_malloc(
        topo->nb_bridges*sizeof(t_topo_bridges),7);
    memset(topo->bridges,0,topo->nb_bridges*sizeof(t_topo_bridges));
    p=strstr(buf,"\"bridges\""); if(!p) KOUT("%s",buf);
    p=strchr(p,'[');             if(!p) KOUT("%s",buf);
    for(i=0;i<topo->nb_bridges;i++){
      char *obj=strchr(p,'{'); if(!obj) KOUT("%s",buf);
      t_topo_bridges *b=&topo->bridges[i];
      json_get_str(obj,"br",b->br,MAX_NAME_LEN);
      json_get_int(obj,"nb_ports",&b->nb_ports);
      char *pp=strstr(obj,"\"ports\""); if(!pp) KOUT("%s",buf);
      pp=strchr(pp,'[');               if(!pp) KOUT("%s",buf);
      for(j=0;j<b->nb_ports;j++){
        char *s=strchr(pp,'"'); if(!s) KOUT("%s",buf); s++;
        char *e=strchr(s,'"'); if(!e) KOUT("%s",buf);
        int sl=(int)(e-s);
        if(sl>=MAX_NAME_LEN) sl=MAX_NAME_LEN-1;
        memcpy(b->ports[j],s,sl); b->ports[j][sl]=0; pp=e+1;
      }
      char *eq=strchr(obj,'}'); p=eq?eq:obj;
    }
  }

  recv_event_topo(llid,tid,topo);
  topo_free_topo(topo);
}

static void decode_c2c_msgs(int llid, int tid, int bnd, char *buf)
{
  char name[MAX_NAME_LEN], str1[MAX_PATH_LEN], str2[MAX_PATH_LEN];

  switch(bnd) {
  case bnd_c2c_add: {
    unsigned int local_udp_ip=0,ip=0,udp_ip=0,port=0,c2c_low=0;
    char passwd[MAX_NAME_LEN];
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_uint(buf,"local_udp_ip",&local_udp_ip);
    json_get_str(buf,"slave_net",str1,MAX_NAME_LEN);
    json_get_uint(buf,"ip",&ip);
    json_get_uint(buf,"port",&port);
    json_get_str(buf,"passwd",passwd,MAX_NAME_LEN);
    json_get_uint(buf,"udp_ip",&udp_ip);
    json_get_uint(buf,"c2c_udp_port_low",&c2c_low);
    recv_c2c_add(llid,tid,name,local_udp_ip,str1,ip,
                 (uint16_t)port,passwd,udp_ip,(uint16_t)c2c_low); break;
  }
  case bnd_c2c_peer_create: {
    int is_ack=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"is_ack",&is_ack);
    json_get_str(buf,"mnet",str1,MAX_NAME_LEN);
    json_get_str(buf,"snet",str2,MAX_NAME_LEN);
    recv_c2c_peer_create(llid,tid,name,is_ack,str1,str2); break;
  }
  case bnd_c2c_peer_conf: {
    int is_ack=0; unsigned int mip=0,sip=0,mport=0,sport=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"is_ack",&is_ack);
    json_get_str(buf,"mnet",str1,MAX_NAME_LEN);
    json_get_str(buf,"snet",str2,MAX_NAME_LEN);
    json_get_uint(buf,"mip",&mip); json_get_uint(buf,"sip",&sip);
    json_get_uint(buf,"mport",&mport); json_get_uint(buf,"sport",&sport);
    recv_c2c_peer_conf(llid,tid,name,is_ack,str1,str2,
                       mip,sip,(uint16_t)mport,(uint16_t)sport); break;
  }
  case bnd_c2c_peer_ping: {
    int status=0;
    json_get_str(buf,"name",name,MAX_NAME_LEN);
    json_get_int(buf,"status",&status);
    recv_c2c_peer_ping(llid,tid,name,status); break;
  }
  default:
    KOUT("decode_c2c_msgs: unexpected bnd=%d", bnd);
  }
}

static int doors_io_basic_decoder_one(int llid, int len, char *buf)
{
  char bound[MAX_CLOWNIX_BOUND_LEN];
  int  bnd, tid=0;

  if (len < 2)
    KOUT("ERROR %d %s", len, buf);
  if (buf[0] != '{')
    {
    KERR("TOLOOK: %s", buf);
    return -1;
    }
  if (buf[len-1] != '}')
    KOUT("ERROR JSONbad len=%d last=%c: %.80s\n", len, buf[len-1], buf);
  json_fctident(buf, bound);
  bnd = get_bnd_event(bound);
  json_get_int(buf, "tid", &tid);
  /* Before the dispatch, so a message and the messages it causes come out in
     that order. */
  if (bnd && g_json_trace)
    g_json_trace(llid, 0, buf, len);

  switch(bnd) {
  case bnd_work_dir_req: case bnd_kill_uml_clownix: case bnd_del_all:
  case bnd_list_pid_req: case bnd_images_req:
  case bnd_eventfull_sub: case bnd_slowperiodic_sub:
  case bnd_topo_small_event_sub:
  case bnd_event_topo_sub:
  case bnd_del_name: case bnd_nat_add: case bnd_tap_add: case bnd_a2b_add:
  case bnd_phy_add: case bnd_snf_add:
  case bnd_item_pos: case bnd_item_poseth:
  case bnd_pcap_open: case bnd_pcap_close:
  case bnd_a2b_cnf: case bnd_nat_cnf: case bnd_lan_cnf: case bnd_c2c_cnf:
  case bnd_cnt_cnf:
  case bnd_add_lan_endp: case bnd_del_lan_endp:
  case bnd_status_ok: case bnd_status_ko:
  case bnd_sav_vm: case bnd_vmcmd: case bnd_list_commands_req:
  case bnd_ovs_cmd_req:
  case bnd_topo_small_event:
  case bnd_sync_wireshark_req: case bnd_sync_wireshark_resp:
  case bnd_trace_sub:
    decode_simple_msgs(llid, tid, bnd, buf); break;

  case bnd_trace:
    decode_trace(llid, tid, buf); break;

  case bnd_pcap_traffic:
    decode_pcap_traffic(llid, tid, buf); break;

  case bnd_list_pid_resp:
    decode_list_pid_resp(llid, tid, buf); break;
  case bnd_images_resp:
    decode_images_resp(llid, tid, buf); break;
  case bnd_list_commands_resp:
    decode_list_commands_resp(llid, tid, buf); break;
  case bnd_ovs_cmd_resp:
    decode_ovs_cmd_resp(llid, tid, buf); break;

  case bnd_eventfull:
    decode_eventfull(llid, tid, buf); break;
  case bnd_slowperiodic_qcow2: case bnd_slowperiodic_cnt:
    decode_slowperiodic(llid, tid, bnd, buf); break;

  case bnd_add_vm:
    decode_add_vm(llid, tid, buf); break;
  case bnd_cnt_add:
    decode_cnt_add(llid, tid, buf); break;
  case bnd_work_dir_resp:
    decode_work_dir_resp(llid, tid, buf); break;
  case bnd_event_topo:
    decode_event_topo(llid, tid, buf); break;

  case bnd_c2c_add: case bnd_c2c_peer_create:
  case bnd_c2c_peer_conf: case bnd_c2c_peer_ping:
    decode_c2c_msgs(llid, tid, bnd, buf); break;

  default:
    return -1;
  }
  return 0;
}

int doors_io_basic_decoder(int llid, int len, char *buf)
{
  int i, depth = 0, start = -1, in_str = 0, esc = 0;
  int rc, result = 0, decoded = 0;

  for (i = 0; i < len; i++)
    {
    char c = buf[i];
    if (in_str)
      {
      if (esc)
        esc = 0;
      else if (c == '\\')
        esc = 1;
      else if (c == '"')
        in_str = 0;
      continue;
      }
    if (c == '"')
      {
      in_str = 1;
      continue;
      }
    if (c == '{')
      {
      if (depth == 0)
        start = i;
      depth++;
      }
    else if (c == '}')
      {
      depth--;
      if ((depth == 0) && (start >= 0))
        {
        int obj_len = i - start + 1;
        char *obj = buf + start;
        if (start + obj_len < len)
          {
          char saved = obj[obj_len];
          obj[obj_len] = 0;
          rc = doors_io_basic_decoder_one(llid, obj_len, obj);
          obj[obj_len] = saved;
          }
        else
          {
          rc = doors_io_basic_decoder_one(llid, obj_len, obj);
          }
        decoded++;
        if (rc == -1)
          result = -1;
        start = -1;
        }
      }
    }
  if (decoded == 0)
    return doors_io_basic_decoder_one(llid, len, buf);
  return result;
}

/* Weak default for a receiver the linking process does not define.        */
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define RECV_DEFAULT(name, args) \
__attribute__((weak)) void name args \
{ \
  static int count = 0; \
  if ((count % 1000) == 0) \
    KERR("WARNING %s reached a process that does not handle it", #name); \
  count += 1; \
}
RECV_DEFAULT(recv_a2b_add, (int llid, int tid, char *name))
RECV_DEFAULT(recv_a2b_cnf, (int llid, int tid, char *name, char *cmd))
RECV_DEFAULT(recv_add_lan_endp, (int llid, int tid, char *name, int num, char *lan))
RECV_DEFAULT(recv_add_vm, (int llid, int tid, t_topo_kvm *kvm))
RECV_DEFAULT(recv_c2c_add, (int llid, int tid, char *c2c_name, uint32_t local_udp_ip, char *slave_cloon, uint32_t ip, uint16_t port, char *passwd, uint32_t udp_ip, uint16_t c2c_udp_port_low))
RECV_DEFAULT(recv_c2c_cnf, (int llid, int tid, char *name, char *cmd))
RECV_DEFAULT(recv_c2c_peer_conf, (int llid, int tid, char *c2c_name, int is_ack, char *distant_cloon, char *local_cloon, uint32_t distant_udp_ip, uint32_t local_udp_ip, uint16_t distant_udp_port, uint16_t local_udp_port))
RECV_DEFAULT(recv_c2c_peer_create, (int llid, int tid, char *c2c_name, int is_ack, char *distant_cloon, char *local_cloon))
RECV_DEFAULT(recv_c2c_peer_ping, (int llid, int tid, char *c2c_name, int status))
RECV_DEFAULT(recv_cnt_add, (int llid, int tid, t_topo_cnt *cnt))
RECV_DEFAULT(recv_del_all, (int llid, int tid))
RECV_DEFAULT(recv_del_lan_endp, (int llid, int tid, char *name, int num, char *lan))
RECV_DEFAULT(recv_del_name, (int llid, int tid, char *name))
RECV_DEFAULT(recv_event_topo, (int llid, int tid, t_topo_info *topo))
RECV_DEFAULT(recv_event_topo_sub, (int llid, int tid))
RECV_DEFAULT(recv_eventfull, (int llid, int tid, int nb_endp, t_eventfull_endp *endp))
RECV_DEFAULT(recv_eventfull_sub, (int llid, int tid))
RECV_DEFAULT(recv_item_pos, (int llid, int tid, char *name, int x, int y))
RECV_DEFAULT(recv_item_poseth, (int llid, int tid, char *name, int num, int deg))
RECV_DEFAULT(recv_kill_uml_clownix, (int llid, int tid))
RECV_DEFAULT(recv_cnt_cnf, (int llid, int tid, char *name, char *cmd))
RECV_DEFAULT(recv_images_req, (int llid, int tid))
RECV_DEFAULT(recv_images_resp, (int llid, int tid, char *runner, int qty, t_image_item *items))
RECV_DEFAULT(recv_lan_cnf, (int llid, int tid, char *name, char *cmd))
RECV_DEFAULT(recv_list_commands_req, (int llid, int tid, int is_layout))
RECV_DEFAULT(recv_list_commands_resp, (int llid, int tid, int qty, t_list_commands *list))
RECV_DEFAULT(recv_list_pid_req, (int llid, int tid))
RECV_DEFAULT(recv_list_pid_resp, (int llid, int tid, int qty, t_pid_lst *pid))
RECV_DEFAULT(recv_nat_add, (int llid, int tid, char *name, char *ports))
RECV_DEFAULT(recv_nat_cnf, (int llid, int tid, char *name, char *cmd))
RECV_DEFAULT(recv_ovs_cmd_req, (int llid, int tid, char *tool, char *args))
RECV_DEFAULT(recv_ovs_cmd_resp, (int llid, int tid, char *tool, int status, int qty, t_list_commands *list))
RECV_DEFAULT(recv_pcap_close, (int llid, int tid, char *name, int num))
RECV_DEFAULT(recv_pcap_open, (int llid, int tid, char *name, int num, char *filter))
RECV_DEFAULT(recv_pcap_traffic, (int llid, int tid, char *name, int num, int type, uint32_t pkt_nb, uint32_t sec, uint32_t usec, int caplen, int len, uint8_t *data))
RECV_DEFAULT(recv_phy_add, (int llid, int tid, char *name, int type))
RECV_DEFAULT(recv_sav_vm, (int llid, int tid, char *name, char *sav_vm_path))
RECV_DEFAULT(recv_slowperiodic_cnt, (int llid, int tid, int nb, t_slowperiodic *spic))
RECV_DEFAULT(recv_slowperiodic_qcow2, (int llid, int tid, int nb, t_slowperiodic *spic))
RECV_DEFAULT(recv_slowperiodic_sub, (int llid, int tid))
RECV_DEFAULT(recv_snf_add, (int llid, int tid, char *name, int num, int val))
RECV_DEFAULT(recv_status_ko, (int llid, int tid, char *reason))
RECV_DEFAULT(recv_status_ok, (int llid, int tid, char *txt))
RECV_DEFAULT(recv_sync_wireshark_req, (int llid, int tid, char *name, int num, int cmd))
RECV_DEFAULT(recv_sync_wireshark_resp, (int llid,int tid,char *name,int num,int status))
RECV_DEFAULT(recv_tap_add, (int llid, int tid, char *name))
RECV_DEFAULT(recv_topo_small_event, (int llid, int tid, char *name, char *param1, char *param2, int vm_evt))
RECV_DEFAULT(recv_topo_small_event_sub, (int llid, int tid))
RECV_DEFAULT(recv_trace, (int llid, int tid, int kind, char *name, char *txt))
RECV_DEFAULT(recv_trace_sub, (int llid, int tid, int kind, int sub_on))
RECV_DEFAULT(recv_vmcmd, (int llid, int tid, char *name, int vmcmd, int param))
RECV_DEFAULT(recv_work_dir_req, (int llid, int tid))
RECV_DEFAULT(recv_work_dir_resp, (int llid, int tid, t_topo_clc *clc))
