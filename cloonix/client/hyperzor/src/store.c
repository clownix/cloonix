/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include <string.h>
#include <gtk/gtk.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "connect_cloonix.h"
#include "cloonix_conf_info.h"
#include "gui.h"
#include "store.h"

/*--------------------------------------------------------------------------*/
typedef struct t_iter
{
  char sid[MAX_NAME_LEN];
  GtkTreeIter iter;
} t_iter;
/*--------------------------------------------------------------------------*/
typedef struct t_store_vm
{
  char name[MAX_NAME_LEN];
  int vm_id;
  t_iter top_iter;
  t_iter spice_iter;
  t_iter ssh_iter;
  t_iter sys_iter;
  t_iter kil_iter;
  int nb_eth;
  struct t_store_vm *prev;
  struct t_store_vm *next;
} t_store_vm;
/*--------------------------------------------------------------------------*/
typedef struct t_store_sat
{
  char name[MAX_NAME_LEN];
  t_iter top_iter;
  int type;
  struct t_store_sat *prev;
  struct t_store_sat *next;
} t_store_sat;
/*--------------------------------------------------------------------------*/
typedef struct t_store_lan
{
  char name[MAX_NAME_LEN];
  t_iter top_iter;
  struct t_store_lan *prev;
  struct t_store_lan *next;
} t_store_lan;
/*--------------------------------------------------------------------------*/
typedef struct t_store_net
{
  char net_name[MAX_NAME_LEN];
  t_iter top_iter;
  t_iter graph_iter;
  t_iter kill_iter;
  t_iter vm_iter;
  t_iter sat_iter;
  t_iter lan_iter;
  int vm_nb;
  int sat_nb;
  int lan_nb;
  t_store_vm *head_vm;
  t_store_sat *head_sat;
  t_store_lan *head_lan;
  struct t_store_net *prev;
  struct t_store_net *next;
} t_store_net;
/*--------------------------------------------------------------------------*/
static GtkTreeStore *g_store;
static t_store_net *g_head_net;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void reset_treeview(void)
{
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int find_vm_with_str_iter(t_store_net *net, char *str_iter, char *name)
{
  int result = -1;
  t_store_vm *cur = net->head_vm;
  while(cur)
    {
    if (!strncmp(cur->top_iter.sid, str_iter, strlen(cur->top_iter.sid)))
      {
      strcpy(name, cur->name);
      if (!strcmp(cur->spice_iter.sid, str_iter))
        result = type_vm_spice;
      else if (!strcmp(cur->ssh_iter.sid, str_iter))
        result = type_vm_ssh;
      else if (!strcmp(cur->sys_iter.sid, str_iter))
        result = type_vm_sys;
      else if (!strcmp(cur->kil_iter.sid, str_iter))
        result = type_vm_kil;
      else 
        result = type_vm;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int find_sat_with_str_iter(t_store_net *net, char *str_iter, char *name)
{
  int result = -1;
  t_store_sat *cur = net->head_sat;
  while(cur)
    {
    if (!strncmp(cur->top_iter.sid, str_iter, strlen(cur->top_iter.sid)))
      {
      strcpy(name, cur->name);
      result = type_sat;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int find_lan_with_str_iter(t_store_net *net, char *str_iter, char *name)
{
  int result = -1;
  t_store_lan *cur = net->head_lan;
  while(cur)
    {
    if (!strncmp(cur->top_iter.sid, str_iter, strlen(cur->top_iter.sid)))
      {
      strcpy(name, cur->name);
      result = type_lan;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int find_iter_net(t_store_net *net, char *str_iter, char *name)
{
  int result = -1;
  if (strlen(str_iter) == strlen(net->top_iter.sid))
    {
    result = type_network;
    }
  else
    {
    if (!strcmp(net->graph_iter.sid, str_iter))
      {
      strcpy(name, "graph");
      result = type_graph;
      }
    else if (!strcmp(net->kill_iter.sid,  str_iter))
      {
      strcpy(name, "kill");
      result = type_kill;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int find_iter_vm(t_store_net *net, char *str_iter, char *name)
{
  int result = -1;
  if ((net->vm_nb) &&
      (!strncmp(net->vm_iter.sid, str_iter, strlen(net->vm_iter.sid))))
    {
    if (strlen(net->vm_iter.sid) == strlen(str_iter))
      result = type_vm_title;
    else 
      result = find_vm_with_str_iter(net, str_iter, name);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int find_iter_sat(t_store_net *net, char *str_iter, char *name)
{
  int result = -1;
  if ((net->sat_nb) &&
      (!strncmp(net->sat_iter.sid, str_iter, strlen(net->sat_iter.sid))))
    {
    if (strlen(net->sat_iter.sid) == strlen(str_iter))
      result = type_sat_title;
    else 
      result = find_sat_with_str_iter(net, str_iter, name);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int find_iter_lan(t_store_net *net, char *str_iter, char *name)
{
  int result = -1;
  if ((net->lan_nb) &&
      (!strncmp(net->lan_iter.sid, str_iter, strlen(net->lan_iter.sid))))
    {
    if (strlen(net->lan_iter.sid) == strlen(str_iter))
      result = type_lan_title;
    else 
      result = find_lan_with_str_iter(net, str_iter, name);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int find_names_with_str_iter(char *str_iter, char *net_name, char *name)
{
  int result = -1;
  t_store_net *net = g_head_net;
  memset(net_name, 0, MAX_NAME_LEN);
  memset(name, 0, MAX_NAME_LEN);
  while(net)
    {
    if (!strncmp(net->top_iter.sid, str_iter, strlen(net->top_iter.sid)))
      break;
    net = net->next;
    }
  if (net) 
    {
    strcpy(net_name, net->net_name);
    result = find_iter_net(net, str_iter, name); 
    if (result == -1)
      result = find_iter_vm(net, str_iter, name);
    if (result == -1)
      result = find_iter_sat(net, str_iter, name);
    if (result == -1)
      result = find_iter_lan(net, str_iter, name);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_store_net *find_net_with_name(char *net_name)
{
  t_store_net *cur = g_head_net;
  while(cur)
    {
    if (!strcmp(cur->net_name, net_name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_store_vm *find_vm_with_name(t_store_net *net, char *name)
{
  t_store_vm *cur = net->head_vm;
  while(cur)
    {
    if (!strcmp(name, cur->name))
      break;
    cur = cur->next;
    }
  return(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int store_get_vm_id(char *net_name, char *name)
{
  int vm_id = 0;
  t_store_vm *svm;
  t_store_net *net = find_net_with_name(net_name);
  if (net)
    {
    svm = find_vm_with_name(net, name);
    if (svm)
      vm_id = svm->vm_id;
    }
  return vm_id;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_store_sat *find_sat_with_name(t_store_net *net, char *name)
{
  t_store_sat *cur = net->head_sat;
  while(cur)
    {
    if (!strcmp(name, cur->name))
      break;
    cur = cur->next;
    }
  return(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_store_lan *find_lan_with_name(t_store_net *net, char *name)
{
  t_store_lan *cur = net->head_lan;
  while(cur)
    {
    if (!strcmp(name, cur->name))
      break;
    cur = cur->next;
    }
  return(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void setup_sid_from_iter(t_iter *elem)
{
  char *str =
  gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(g_store), &elem->iter);
  memset(elem->sid, 0, MAX_NAME_LEN);
  strncpy(elem->sid, str, MAX_NAME_LEN-1);
  g_free(str);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void recompute_vm_item_sid(t_store_vm *head)
{
  t_store_vm *cur = head;
  while(cur)
    {
    setup_sid_from_iter(&(cur->top_iter));
    setup_sid_from_iter(&(cur->spice_iter));
    setup_sid_from_iter(&(cur->ssh_iter));
    setup_sid_from_iter(&(cur->sys_iter));
    setup_sid_from_iter(&(cur->kil_iter));
    cur = cur->next;
    }
  reset_treeview();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void recompute_sat_item_sid(t_store_sat *head)
{
  t_store_sat *cur = head;
  while(cur)
    {
    setup_sid_from_iter(&(cur->top_iter));
    cur = cur->next;
    }
  reset_treeview();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void recompute_lan_item_sid(t_store_lan *head)
{
  t_store_lan *cur = head;
  while(cur)
    {
    setup_sid_from_iter(&(cur->top_iter));
    cur = cur->next;
    }
  reset_treeview();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void recompute_net_item_sid(t_store_net *head)
{
  t_store_net *cur = head;
  while(cur)
    {
    setup_sid_from_iter(&(cur->top_iter));
    setup_sid_from_iter(&(cur->graph_iter));
    setup_sid_from_iter(&(cur->kill_iter));
    if (cur->vm_nb)
      setup_sid_from_iter(&(cur->vm_iter));
    if (cur->sat_nb)
      setup_sid_from_iter(&(cur->sat_iter));
    if (cur->lan_nb)
      setup_sid_from_iter(&(cur->lan_iter));
    recompute_vm_item_sid(cur->head_vm);
    recompute_sat_item_sid(cur->head_sat);
    recompute_lan_item_sid(cur->head_lan);
    cur = cur->next;
    }
  reset_treeview();
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void store_add_leaf(t_iter *parent, t_iter *child, char *name)
{
  char *str;
  if (parent)
    gtk_tree_store_append(g_store, &child->iter, &parent->iter);
  else
    gtk_tree_store_append(g_store, &child->iter, NULL);
  gtk_tree_store_set(g_store, &child->iter, 0, name, -1);
  str = 
  gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(g_store), &child->iter);
  strncpy(child->sid, str, MAX_NAME_LEN-1);
  g_free(str);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void store_del_leaf(t_iter *child)
{
  gtk_tree_store_remove(g_store, &child->iter);
  memset(child->sid, 0, MAX_NAME_LEN);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_store_vm(char *net_name, char *name, int nb_eth, int vm_id)
{
  t_store_vm *cur;
  t_store_net *net = find_net_with_name(net_name);
  if (!net)
    KOUT("%s %s", net_name, name);
  cur = (t_store_vm *) clownix_malloc(sizeof(t_store_vm), 5);
  memset(cur, 0, sizeof(t_store_vm));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->nb_eth = nb_eth;
  cur->vm_id = vm_id;
  cur->next = net->head_vm;
  if (net->head_vm)
    net->head_vm->prev = cur;
  net->head_vm = cur;
  if (net->vm_nb == 0)
    store_add_leaf(&net->top_iter, &net->vm_iter, "vm");
  net->vm_nb += 1;
  store_add_leaf(&net->vm_iter, &cur->top_iter, name);
  store_add_leaf(&cur->top_iter, &cur->spice_iter, "spice_desktop");
  store_add_leaf(&cur->top_iter, &cur->ssh_iter, "cloonix_ssh");
  store_add_leaf(&cur->top_iter, &cur->sys_iter, "sysinfo");
  store_add_leaf(&cur->top_iter, &cur->kil_iter, "kill");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_store_sat(char *net_name, char *name, int type)
{
  t_store_sat *cur;
  t_store_net *net = find_net_with_name(net_name);
  if (!net)
    KOUT("%s %s", net_name, name);
  cur = (t_store_sat *) clownix_malloc(sizeof(t_store_sat), 5);
  memset(cur, 0, sizeof(t_store_sat));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->type = type;
  cur->next = net->head_sat;
  if (net->head_sat)
    net->head_sat->prev = cur;
  net->head_sat = cur;
  if (net->sat_nb == 0)
    store_add_leaf(&net->top_iter, &net->sat_iter, "sat");
  net->sat_nb += 1;
  store_add_leaf(&net->sat_iter, &cur->top_iter, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_store_lan(char *net_name, char *name)
{
  t_store_lan *cur;
  t_store_net *net = find_net_with_name(net_name);
  if (!net)
    KOUT("%s %s", net_name, name);
  cur = (t_store_lan *) clownix_malloc(sizeof(t_store_lan), 5);
  memset(cur, 0, sizeof(t_store_lan));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->next = net->head_lan;
  if (net->head_lan)
    net->head_lan->prev = cur;
  net->head_lan = cur;
  if (net->lan_nb == 0)
    store_add_leaf(&net->top_iter, &net->lan_iter, "lan");
  net->lan_nb += 1;
  store_add_leaf(&net->lan_iter, &cur->top_iter, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_store_vm(char *net_name, char *name)
{
  t_store_vm *vm;
  t_store_net *net = find_net_with_name(net_name);
  if (!net)
    KOUT("%s %s", net_name, name);
  vm = find_vm_with_name(net, name);
  if (!vm)
    KOUT("%s %s", net_name, name);
  if (vm->prev)
    vm->prev->next = vm->next;
  if (vm->next)
    vm->next->prev = vm->prev;
  if (vm == net->head_vm)
    net->head_vm = vm->next;
  store_del_leaf(&vm->spice_iter);
  store_del_leaf(&vm->ssh_iter);
  store_del_leaf(&vm->sys_iter);
  store_del_leaf(&vm->kil_iter);
  store_del_leaf(&vm->top_iter);
  if (net->vm_nb == 1)
    store_del_leaf(&net->vm_iter);
  net->vm_nb -= 1;
  clownix_free(vm, __FUNCTION__);
  recompute_vm_item_sid(net->head_vm);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_store_sat(char *net_name, char *name)
{
  t_store_sat *sat;
  t_store_net *net = find_net_with_name(net_name);
  if (!net)
    KOUT("%s %s", net_name, name);
  sat = find_sat_with_name(net, name);
  if (!sat)
    KOUT("%s %s", net_name, name);
  if (sat->prev)
    sat->prev->next = sat->next;
  if (sat->next)
    sat->next->prev = sat->prev;
  if (sat == net->head_sat)
    net->head_sat = sat->next;
  store_del_leaf(&sat->top_iter);
  if (net->sat_nb == 1)
    store_del_leaf(&net->sat_iter);
  net->sat_nb -= 1;
  clownix_free(sat, __FUNCTION__);
  recompute_sat_item_sid(net->head_sat);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_store_lan(char *net_name, char *name)
{
  t_store_lan *lan;
  t_store_net *net = find_net_with_name(net_name);
  if (!net)
    KOUT("%s %s", net_name, name);
  lan = find_lan_with_name(net, name);
  if (!lan)
    KOUT("%s %s", net_name, name);
  if (lan->prev)
    lan->prev->next = lan->next;
  if (lan->next)
    lan->next->prev = lan->prev;
  if (lan == net->head_lan)
    net->head_lan = lan->next;
  store_del_leaf(&lan->top_iter);
  if (net->lan_nb == 1)
    store_del_leaf(&net->lan_iter);
  net->lan_nb -= 1;
  clownix_free(lan, __FUNCTION__);
  recompute_lan_item_sid(net->head_lan);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_store_net *alloc_store_net(char *net_name)
{
  t_store_net *cur = find_net_with_name(net_name);
  if (cur)
    KOUT("%s", net_name);
  cur = (t_store_net *) clownix_malloc(sizeof(t_store_net), 5);
  memset(cur, 0, sizeof(t_store_net));
  strncpy(cur->net_name, net_name, MAX_NAME_LEN-1);
  cur->next = g_head_net;
  if (g_head_net)
    g_head_net->prev = cur;
  g_head_net = cur;
  store_add_leaf(NULL, &cur->top_iter, net_name);
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_store_net(t_store_net *cur)
{
  while(cur->head_vm)
    free_store_vm(cur->net_name, cur->head_vm->name);
  while(cur->head_sat)
    free_store_sat(cur->net_name, cur->head_sat->name);
  while(cur->head_lan)
    free_store_lan(cur->net_name, cur->head_lan->name);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_net)
    g_head_net = cur->next;
  store_del_leaf(&cur->top_iter);
  clownix_free(cur, __FUNCTION__);
  recompute_net_item_sid(g_head_net);
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
void store_net_alloc(char *net_name)
{
  t_store_net *cur = alloc_store_net(net_name);
  store_add_leaf(&cur->top_iter, &cur->graph_iter, "graph");
  store_add_leaf(&cur->top_iter, &cur->kill_iter, "kill");
  reset_treeview();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void store_vm_alloc(char *net_name, char *name, int nb_eth, int vm_id)
{
  alloc_store_vm(net_name, name, nb_eth, vm_id);
  reset_treeview();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void store_sat_alloc(char *net_name, char *name, int type)
{
  alloc_store_sat(net_name, name, type);
  reset_treeview();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void store_lan_alloc(char *net_name, char *name)
{
  alloc_store_lan(net_name, name);
  reset_treeview();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void store_net_free(char *net_name)
{
  t_store_net *net = find_net_with_name(net_name);
  if (!net)
    KOUT("%s", net_name);
  store_del_leaf(&net->graph_iter);
  store_del_leaf(&net->kill_iter);
  free_store_net(net);
  reset_treeview();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void store_vm_free(char *net_name, char *name)
{
  free_store_vm(net_name, name);
  reset_treeview();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void store_sat_free(char *net_name, char *name)
{
  free_store_sat(net_name, name);
  reset_treeview();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void store_lan_free(char *net_name, char *name)
{
  free_store_lan(net_name, name);
  reset_treeview();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
GtkTreeStore *store_get(void)
{
  return g_store;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void store_init(void)
{
  g_store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
}
/*---------------------------------------------------------------------------*/
                                                  
