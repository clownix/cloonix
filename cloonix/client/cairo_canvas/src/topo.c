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
#include <gtk/gtk.h>
#include <string.h>
#include <libcrcanvas.h>
#include <math.h>
#include <stdlib.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_consts.h"
#include "interface.h"
#include "bank.h"
#include "move.h"
#include "cloonix.h"
#include "menus.h"
#include "main_timer_loop.h"
#include "eventfull_eth.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "canvas_ctx.h"
#include "layout_rpc.h"
#include "make_layout.h"
#include "layout_topo.h"
#include "bdplot.h"
#include "menu_utils.h"


int get_endp_type(t_bank_item *bitem);
void canvas_ctx_menu(gdouble x, gdouble y);

typedef struct
{
  t_bank_item *bank_item;
} ItemProperties;


/****************************************************************************/
typedef struct t_ccol
{
  double r;
  double g;
  double b;
} t_ccol;
/*--------------------------------------------------------------------------*/
static t_ccol orange, black, blue, green, cyan, red, magenta, brown, 
              lightgrey, darkgrey, lightblue, lightgreen, lightcyan, 
              lightred, lightmagenta, yellow, white;

CrCanvas *glob_canvas = NULL;
GtkWidget *gtkwidget_canvas;
GtkWidget *get_main_window(void);
static CrPanner *glob_panner;


/****************************************************************************/
static void sat_lozange(cairo_t *c, double x0, double y0, double d0)
{
  cairo_move_to (c, x0,      y0 - d0);
  cairo_line_to (c, x0 + d0, y0);
  cairo_line_to (c, x0,      y0 + d0);
  cairo_line_to (c, x0 - d0, y0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_layout_center_scale(const char *from)
{
  double dcx, dcy, dcw, dch;
  int cx, cy, cw, ch;
  cr_canvas_get_center_scale(CR_CANVAS(gtkwidget_canvas),&dcx,&dcy,&dcw,&dch);
  cx = (int) dcx;
  cy = (int) dcy;
  cw = (int) dcw;
  ch = (int) dch;
  if (layout_get_ready_for_send())
    {
    send_layout_center_scale(get_clownix_main_llid(), 8888, cx, cy, cw, ch);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void paint_color_of_d2d(t_bank_item *bitem, cairo_t *c)
{
  if (is_a_d2d(bitem))
    {
    if ((bitem->pbi.pbi_sat->topo_d2d.tcp_connection_peered) &&
        (bitem->pbi.pbi_sat->topo_d2d.udp_connection_peered) &&
        (bitem->pbi.pbi_sat->topo_d2d.ovs_lan_attach_ready))
      cairo_set_source_rgba (c, lightgreen.r, lightgreen.g, lightgreen.b, 1.0);
    else if ((bitem->pbi.pbi_sat->topo_d2d.tcp_connection_peered) &&
             (bitem->pbi.pbi_sat->topo_d2d.udp_connection_peered))
      cairo_set_source_rgba (c, orange.r, orange.g, orange.b, 0.8);
    else if (bitem->pbi.pbi_sat->topo_d2d.tcp_connection_peered)
      cairo_set_source_rgba (c, lightred.r, lightred.g, lightred.b, 0.8);
    else
      cairo_set_source_rgba (c, red.r, red.g, red.b, 0.8);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void topo_zoom_in_out_canvas(int in, int val)
{
  int i;
  for (i=0; i<val; i++)
    {
    if (in)
      cr_canvas_zoom(glob_canvas, 1.4, 0);
    else
      cr_canvas_zoom(glob_canvas, 0.9, 0);
    }
  update_layout_center_scale(__FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void topo_repaint_request(void)
{
  cr_canvas_queue_repaint (glob_canvas);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_bank_item *from_critem_to_bank_item(CrItem *item)
{
  ItemProperties *properties;
  properties = g_object_get_data(G_OBJECT(item), "properties");
  return (properties->bank_item);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
CrItem *get_glob_canvas_root(void)
{
  if (!glob_canvas)
    KOUT(" ");
  return (glob_canvas->root);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void topo_get_matrix_inv_transform_point(double *x, double *y)
{
  CrItem *root = get_glob_canvas_root();
  cairo_matrix_transform_point(cr_item_get_inverse_matrix(root), x, y);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void init_colors(void)
{
  orange.r       = 0.99;  orange.g       = 0.33;  orange.b       = 0.00;
  black.r        = 0.00;  black.g        = 0.00;  black.b        = 0.00;
  blue.r         = 0.00;  blue.g         = 0.00;  blue.b         = 0.66;
  green.r        = 0.00;  green.g        = 0.66;  green.b        = 0.00;
  cyan.r         = 0.00;  cyan.g         = 0.66;  cyan.b         = 0.66;
  red.r          = 0.66;  red.g          = 0.00;  red.b          = 0.00;
  magenta.r      = 0.66;  magenta.g      = 0.00;  magenta.b      = 0.66;
  brown.r        = 0.66;  brown.g        = 0.33;  brown.b        = 0.00;
  lightgrey.r    = 0.66;  lightgrey.g    = 0.66;  lightgrey.b    = 0.66;
  darkgrey.r     = 0.33;  darkgrey.g     = 0.33;  darkgrey.b     = 0.33;
  lightblue.r    = 0.50;  lightblue.g    = 0.50;  lightblue.b    = 1.00;
  lightgreen.r   = 0.33;  lightgreen.g   = 1.00;  lightgreen.b   = 0.33;
  lightcyan.r    = 0.33;  lightcyan.g    = 1.00;  lightcyan.b    = 1.00;
  lightred.r     = 1.00;  lightred.g     = 0.33;  lightred.b     = 0.33;
  lightmagenta.r = 1.00;  lightmagenta.g = 0.33;  lightmagenta.b = 1.00;
  yellow.r       = 1.00;  yellow.g       = 1.00;  yellow.b       = 0.33;
  white.r        = 1.00;  white.g        = 1.00;  white.b        = 1.00;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void modif_position_layout(t_bank_item *bitem, double xi, double yi)
{
  double x=xi, y=yi;
  cairo_matrix_translate(cr_item_get_matrix(CR_ITEM(bitem->cr_item)), x, y);
  attached_edge_update_all(bitem);
  move_manager_update_item(bitem);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void modif_position_generic(t_bank_item *bitem, double xi, double yi)
{
  double x=xi, y=yi, tx0, ty0;
  tx0 = x - bitem->pbi.x;
  ty0 = y - bitem->pbi.y;
  cairo_matrix_translate(cr_item_get_matrix(CR_ITEM(bitem->cr_item)),tx0,ty0);
  attached_edge_update_all(bitem);
  move_manager_update_item(bitem);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void modif_position_eth(t_bank_item *bitem, double xi, double yi)
{
  cairo_matrix_t *mat;
  double vx, vy, dist, x0, y0, tx, ty, x1=xi, y1=yi;
  x0 = bitem->pbi.x0;
  y0 = bitem->pbi.y0;
  tx = bitem->pbi.tx;
  ty = bitem->pbi.ty;
  if ((!bitem) || (!bitem->att_node))
    KOUT(" ");
  mat = cr_item_get_matrix(CR_ITEM(bitem->att_node->cr_item));
  cairo_matrix_transform_point(mat, &x0, &y0);
  vx = x1 - x0;
  vy = y1 - y0;
  dist = sqrt(vx*vx + vy*vy);
  mat = cr_item_get_matrix(CR_ITEM(bitem->cr_item));
  cairo_matrix_translate(mat,-tx,-ty);
  if (dist == 0)
    {
    tx = 0;
    ty = 0;
    }
  else
    {
    if (bitem->att_node->pbi.endp_type == endp_type_a2b)
      {
      tx = (vx * (A2B_DIA*VAL_INTF_POS_A2B))/dist;
      ty = (vy * (A2B_DIA*VAL_INTF_POS_A2B))/dist;
      }
    else
      {
      tx = (vx * (NODE_DIA*VAL_INTF_POS_NODE))/dist;
      ty = (vy * (NODE_DIA*VAL_INTF_POS_NODE))/dist;
      }
    }
  mat = cr_item_get_matrix(CR_ITEM(bitem->cr_item));
  cairo_matrix_translate(mat, tx, ty);
  bitem->pbi.tx = tx;
  bitem->pbi.ty = ty;
  attached_edge_update_all(bitem);
  move_manager_update_item(bitem);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void move_manager_translate(t_bank_item *bitem, double x1, double y1)
{
  double dx = x1;
  double dy = y1;
  cairo_matrix_translate(cr_item_get_matrix(CR_ITEM(bitem->cr_item)),dx,dy);
  attached_edge_update_all(bitem);
  move_manager_update_item(bitem);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void move_manager_rotate(t_bank_item *bitem, double x1, double y1)
{
  double y = y1;
  double x = x1;
  topo_get_matrix_inv_transform_point(&x, &y);
  modif_position_eth(bitem, x, y);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void process_mouse_double_click(t_bank_item *bitem)
{
  int config_flags;
  switch(bitem->bank_type)
    {

    case bank_type_lan:
      selectioned_flip_flop(bitem);
      break;

    case bank_type_node:
      config_flags = bitem->pbi.pbi_node->node_vm_config_flags;
      launch_xterm_double_click(bitem->name, config_flags);
      break;

    case bank_type_eth:
      bitem->pbi.flag = flag_normal;
      if (get_endp_type(bitem) == endp_type_eths)
        wireshark_launch(bitem->name, bitem->num);
      break;

    case bank_type_sat:
      bitem->pbi.flag = flag_normal;
      if ((bitem->pbi.endp_type == endp_type_phy) ||
          (bitem->pbi.endp_type == endp_type_tap))
        wireshark_launch(bitem->name, 0);
      break;

    default:
      break;
  }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void process_mouse_motion(GdkEvent *event, 
                                 cairo_matrix_t *matrix,
                                 t_bank_item *bitem)
{
  double x,y;
  x = event->motion.x;
  y = event->motion.y;
  if ((bitem->bank_type == bank_type_node) ||
      (bitem->bank_type == bank_type_lan)  ||
      (bitem->bank_type == bank_type_sat))
    {
    modif_position_generic(bitem, x, y);
    }
  else if (bitem->bank_type ==  bank_type_eth)
    {
    cairo_matrix_transform_point(matrix, &x, &y);
    topo_get_matrix_inv_transform_point(&x, &y);
    modif_position_eth(bitem, x, y);
    }
  else if (bitem->bank_type == bank_type_edge)
    {
    }
  else
    KOUT("%d", bitem->bank_type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void menu_caller(t_bank_item *bitem)
{ 
  switch (bitem->bank_type)
    {
    case bank_type_node:
      node_ctx_menu(bitem);
      bitem->pbi.menu_on = 1;
      break;
    case bank_type_lan:
      if (!is_selectable(bitem))
        {
        lan_ctx_menu(bitem);
        bitem->pbi.menu_on = 1;
        }
      break;
    case bank_type_eth:
      intf_ctx_menu(bitem);
      bitem->pbi.menu_on = 1;
      break;

    case bank_type_sat:
        {
        sat_ctx_menu(bitem);
        bitem->pbi.menu_on = 1;
        }
      break;

    case bank_type_edge:
      edge_ctx_menu(bitem);
      bitem->pbi.menu_on = 1;
      break;
    default:
      KOUT("%d", bitem->bank_type);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fct_button_1_pressed(t_bank_item *bitem, int pressed)
{
  if ((bitem->bank_type == bank_type_node) ||
      (bitem->bank_type == bank_type_eth) ||
      (bitem->bank_type == bank_type_lan) ||
      (bitem->bank_type == bank_type_sat))
    bitem->button_1_pressed = pressed;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fct_button_1_moved(t_bank_item *bitem, int moved)
{
  if ((bitem->bank_type == bank_type_node) ||
      (bitem->bank_type == bank_type_lan) ||
      (bitem->bank_type == bank_type_eth) ||
      (bitem->bank_type == bank_type_sat))
    bitem->button_1_moved = moved;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fct_bitem_moved_by_operator(t_bank_item *bitem)
{
  t_layout_node *layout_node;
  t_layout_sat *layout_sat;
  t_layout_lan *layout_lan;
  if (bitem->bank_type == bank_type_node) 
    {
    layout_node = make_layout_node(bitem);
    layout_send_layout_node(layout_node);
    }
  else if (bitem->bank_type == bank_type_eth)
    {
    if (bitem->pbi.endp_type == endp_type_a2b)
      {
      layout_sat = make_layout_sat(bitem->att_node);
      layout_send_layout_sat(layout_sat);
      }
    else
      {
      layout_node = make_layout_node(bitem->att_node);
      layout_send_layout_node(layout_node);
      }
    }
  else if (bitem->bank_type == bank_type_lan)
    {
    layout_lan = make_layout_lan(bitem);
    layout_send_layout_lan(layout_lan);
    }
  else if (bitem->bank_type == bank_type_sat)
    {
    layout_sat = make_layout_sat(bitem);
    layout_send_layout_sat(layout_sat);
    }
  else
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fct_button_1_double_click(t_bank_item *bitem, int hit)
{
  if (bitem->bank_type == bank_type_node) 
    bitem->button_1_double_click = hit;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void print_event_item(GdkEventType type, char *from)
{
  switch (type)
    {
    case GDK_MOTION_NOTIFY:
    break;
    case GDK_ENTER_NOTIFY:
      //printf("%s %d GDK_ENTER_NOTIFY\n", from, type);
    break;
    case GDK_LEAVE_NOTIFY:
      //printf("%s %d GDK_LEAVE_NOTIFY\n", from, type);
    break;
    case GDK_BUTTON_PRESS:
      //printf("%s %d GDK_BUTTON_PRESS\n", from, type);
    break;
    case GDK_BUTTON_RELEASE:
      //printf("%s %d GDK_BUTTON_RELEASE\n", from, type);
    break;
    case GDK_2BUTTON_PRESS:
      //printf("%s %d GDK_2BUTTON_PRESS\n", from, type);
    break;
    case GDK_FOCUS_CHANGE:
      //printf("%s %d GDK_FOCUS_CHANGE\n", from, type);
    break;
    case GDK_KEY_PRESS:
      //printf("%s %d GDK_KEY_PRESS\n", from, type);
    break;
    case GDK_KEY_RELEASE:
      //printf("%s %d GDK_KEY_RELEASE\n", from, type);
    break;
    case GDK_GRAB_BROKEN:
      //printf("%s %d GDK_GRAB_BROKEN\n", from, type);
    break;
    case GDK_VISIBILITY_NOTIFY:
      //printf("%s %d GDK_VISIBILITY_NOTIFY\n", from, type);
    break;
    case GDK_MAP:
      //printf("%s %d GDK_MAP\n", from, type);
    break;
    case GDK_3BUTTON_PRESS:
    break;
    default:
      KERR("%s MUSTADD %d", from, type);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean on_item_event(CrItem *item, GdkEvent *event, 
                              cairo_matrix_t *matrix, CrItem *pick_item) 
{
  gboolean result = FALSE;
  t_bank_item *bitem = from_critem_to_bank_item(item);
  if (!bitem)
    KOUT(" ");
  if (!pick_item)
    KOUT(" ");
  print_event_item(event->type, "ON_ITEM");
  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      if (event->button.button == 1)
        {
        fct_button_1_pressed(bitem, 1);
        fct_button_1_moved(bitem, 0);
        fct_button_1_double_click(bitem, 0);
        selectioned_mouse_button_1_press(bitem,event->button.x,event->button.y); 
        result = TRUE;
        }
      else if (event->button.button == 3)
        {
        menu_caller(bitem);
        result = TRUE;
        }
      break;
    case GDK_2BUTTON_PRESS:
      if (event->button.button == 1) 
        {
        fct_button_1_double_click(bitem, 1);
        process_mouse_double_click(bitem);
        result = TRUE;
        }
      break;
    case GDK_3BUTTON_PRESS:
      break;
    case GDK_MOTION_NOTIFY:
      if (bitem->button_1_pressed)
        {
        process_mouse_motion(event, matrix, bitem);
        fct_button_1_moved(bitem, 1);
        result = TRUE;
        }
      break;
    case GDK_ENTER_NOTIFY:
        enter_item_surface(bitem);
        result = TRUE;
      break;
    case GDK_LEAVE_NOTIFY:
        leave_item_surface();
        result = TRUE;
      break;
    case GDK_BUTTON_RELEASE:
      if (event->button.button == 1)
        {
        fct_button_1_pressed(bitem, 0);
        if (bitem->button_1_moved)
          fct_bitem_moved_by_operator(bitem);
        fct_button_1_moved(bitem, 0);
        result = TRUE;
        }
      else if (event->button.button == 3)
        {
        printf("UNHANDLED 3 GDK_BUTTON_RELEASE\n");
        }
      break;
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
    break;
    default:
      printf("UNHANDLED: %d\n", event->type);
      break;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void paint_yellow(cairo_t *c)
{
  cairo_set_source_rgba (c, 1.0, 1.0, 0.33, 1.0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void paint_select(cairo_t *c, int flag, int flag_trace, 
                         t_ccol *norm, t_ccol *sel, t_ccol *att)
{
  if (flag_trace)
    paint_yellow(c);
  else
    {
    switch (flag)
      {
      case flag_select:
        cairo_set_source_rgba (c, sel->r, sel->g, sel->b, 1.0);
        break;
      case flag_attach:
        cairo_set_source_rgba (c, att->r, att->g, att->b, 1.0);
        break;
      default:
        cairo_set_source_rgba (c, norm->r, norm->g, norm->b, 1.0);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void paint_select_source_color(t_bank_item *bitem, cairo_t *c)
{
  int flag, flag_trace;
  flag = bitem->pbi.flag;
  flag_trace = bitem->pbi.flag_trace;
  switch (bitem->bank_type)
    {
    case bank_type_sat:
      paint_color_of_d2d(bitem, c);
      break;
    case bank_type_lan:
      paint_select(c,flag,flag_trace,&lightgrey,&red,&lightmagenta);
      break;
    case bank_type_edge:
      paint_select(c,flag,flag_trace,&lightgreen,&red,&lightmagenta);
      break;
    default:
      KOUT("%d", bitem->bank_type);
    }
  cairo_fill_preserve(c);
  if (bitem->pbi.grabbed)
    cairo_set_source_rgba (c, white.r, white.g, white.b, 1.0);
  else
    cairo_set_source_rgba (c, black.r, black.g, black.b, 1.0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cairo_elem(cairo_t *c, int bank_type, double x0, double y0,
                                                  double x1, double y1)
{
  switch (bank_type)
    {
    case bank_type_edge:
      cairo_new_path(c);
      cairo_move_to (c, x0, y0);
      cairo_line_to (c, x1, y1);
      break;
    case bank_type_lan:
      cairo_arc (c, x0, y0, INTF_DIA/2, 0, 2*M_PI);
      break;

    default: 
      KOUT("%d", bank_type);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_item_paint_nat(CrItem *item, cairo_t *c)
{
  t_bank_item *bitem = from_critem_to_bank_item(item);
  double x0,y0, d0;
  if (!bitem)
    KOUT(" ");
  x0 = bitem->pbi.x0;
  y0 = bitem->pbi.y0;
  cairo_new_path(c);

  if (bitem->pbi.blink_tx)
    {
    d0 = NAT_RAD + NAT_RAD/3;
    sat_lozange(c, x0, y0, d0);
    cairo_set_source_rgba (c, orange.r, orange.g, orange.b, 0.8);
    cairo_fill(c);
    }
  d0 = NAT_RAD;
  sat_lozange(c, x0, y0, d0);
  cairo_close_path (c);
  if (bitem->pbi.grabbed)
    cairo_set_source_rgba (c, white.r, white.g, white.b, 1.0);
  else
    cairo_set_source_rgba (c, black.r, black.g, black.b, 1.0);
  cairo_stroke_preserve(c);

  cairo_set_source_rgba (c, lightcyan.r, lightcyan.g, lightcyan.b, 0.8);


  cairo_fill(c);
  if (bitem->pbi.blink_rx)
    {
    cairo_arc (c, bitem->pbi.x0, bitem->pbi.y0, INTF_DIA/6, 0, 2*M_PI);
    cairo_set_source_rgba (c, orange.r, orange.g, orange.b, 0.8);
    cairo_fill(c);
    }
  if (bitem->pbi.grabbed)
    cairo_set_source_rgba (c, white.r, white.g, white.b, 1.0);
  else
    cairo_set_source_rgba (c, black.r, black.g, black.b, 1.0);
  cairo_set_line_width(c, bitem->pbi.line_width);
  cairo_stroke(c);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_item_paint_eth(CrItem *item, cairo_t *c)
{
  int endp_type, flag, flag_trace, flag_grabbed;
  t_bank_item *bitem = from_critem_to_bank_item(item);
  flag = bitem->pbi.flag;
  flag_trace = bitem->pbi.flag_trace;
  flag_grabbed = bitem->pbi.grabbed;
  if (!bitem)
    KOUT(" ");

  cairo_new_path(c);

  if (bitem->pbi.blink_tx)
    {
    cairo_arc (c, bitem->pbi.x0, bitem->pbi.y0, 
               INTF_DIA/2+INTF_DIA/8, 0, 2*M_PI);
    cairo_set_source_rgba (c, orange.r, orange.g, orange.b, 0.8);
    cairo_fill(c);
    }

  cairo_arc (c, bitem->pbi.x0, bitem->pbi.y0, INTF_DIA/2, 0, 2*M_PI);
  if (bitem->bank_type == bank_type_eth)
    {
    endp_type = get_endp_type(bitem);
    if (endp_type == endp_type_ethd)
      paint_select(c,flag,flag_trace,&lightgrey,&red,&lightmagenta);
    else if (endp_type == endp_type_eths)
      paint_select(c,flag,flag_trace,&lightgreen,&red,&lightmagenta);
    else if (endp_type == endp_type_ethv)
      paint_select(c,flag,flag_trace,&lightcyan,&red,&lightmagenta);
    }
  else
    KOUT("%d", bitem->bank_type);
  cairo_fill(c);

  if (bitem->pbi.blink_rx)
    {
    cairo_arc (c, bitem->pbi.x0, bitem->pbi.y0, INTF_DIA/6, 0, 2*M_PI);
    cairo_set_source_rgba (c, orange.r, orange.g, orange.b, 0.8);
    cairo_fill(c);
    }

  cairo_arc (c, bitem->pbi.x0, bitem->pbi.y0, INTF_DIA/2, 0, 2*M_PI);
  if (flag_grabbed)
    cairo_set_source_rgba (c, white.r, white.g, white.b, 1.0);
  else
    cairo_set_source_rgba (c, black.r, black.g, black.b, 1.0);
  cairo_set_line_width(c, bitem->pbi.line_width);
  cairo_stroke(c);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void paint_rectangle(t_bank_item *bitem, cairo_t *c, int y, int val)
{
  cairo_move_to (c, bitem->pbi.x0-15, bitem->pbi.y0 + y);
  cairo_line_to (c, bitem->pbi.x0-15, bitem->pbi.y0 + y + 7);
  cairo_line_to (c, bitem->pbi.x0+15, bitem->pbi.y0 + y + 7);
  cairo_line_to (c, bitem->pbi.x0+15, bitem->pbi.y0 + y);
  cairo_close_path (c);
  cairo_set_source_rgba (c, black.r, black.g, black.b, 1.0);
  cairo_set_line_width(c, bitem->pbi.line_width);
  cairo_stroke_preserve(c);
  cairo_set_source_rgba (c, white.r, white.g, white.b, 0.6);
  cairo_fill(c);
  cairo_move_to (c, bitem->pbi.x0-15, bitem->pbi.y0 + y);
  cairo_line_to (c, bitem->pbi.x0-15, bitem->pbi.y0 + y + 7);
  cairo_line_to (c, bitem->pbi.x0-15+val, bitem->pbi.y0 + y + 7);
  cairo_line_to (c, bitem->pbi.x0-15+val, bitem->pbi.y0 + y);
  cairo_set_source_rgba (c, orange.r, orange.g, orange.b, 0.8);
  cairo_fill(c);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_item_paint_node_ram_cpu(t_bank_item *bitem, cairo_t *c)
{
  int cpu = bitem->pbi.pbi_node->node_cpu;
  int ram = bitem->pbi.pbi_node->node_ram;
  paint_rectangle(bitem, c, -20, ram/20);
  paint_rectangle(bitem, c, 10, cpu/2);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_item_paint_node(CrItem *item, cairo_t *c)
{
  t_bank_item *bitem = from_critem_to_bank_item(item);
  if (!bitem)
    KOUT(" ");
  cairo_new_path(c);
  cairo_arc (c, bitem->pbi.x0, bitem->pbi.y0, NODE_DIA/2, 0, 2*M_PI);
  if (bitem->pbi.flag_trace)
    paint_yellow(c);
  else if ((bitem->pbi.flag == flag_qmp_conn_ko) || 
           (bitem->pbi.flag == flag_normal))
    cairo_set_source_rgba (c, 0.50, 0.30, 0.30, 1.0);
  else
    {
    if (bitem->pbi.color_choice == 1)
        cairo_set_source_rgba (c, cyan.r, cyan.g, cyan.b, 1.0);
    else if (bitem->pbi.color_choice == 2)
        cairo_set_source_rgba (c, brown.r, brown.g, brown.b, 1.0);
    else
      {
      if (bitem->pbi.flag == flag_qmp_conn_ok)
        cairo_set_source_rgba (c, 0.90, 0.10, 0.10, 1.0);
      else if (bitem->pbi.flag == flag_ping_ok)
        {
        bitem->pbi.pbi_node->node_vm_config_flags |= 
                                              VM_FLAG_CLOONIX_AGENT_PING_OK;
        cairo_set_source_rgba (c, 0.50, 0.50, 1.0, 1.0);
        }
      else
        {
        KERR("%d", bitem->pbi.flag);
        cairo_set_source_rgba (c, 0.60, 0.20, 0.20, 1.0);
        }
      }
    }
  cairo_fill(c);

  on_item_paint_node_ram_cpu(bitem, c);

  cairo_arc (c, bitem->pbi.x0, bitem->pbi.y0, NODE_DIA/2, 0, 2*M_PI);
  if (bitem->pbi.grabbed)
    cairo_set_source_rgba (c, white.r, white.g, white.b, 1.0);
  else
    cairo_set_source_rgba (c, black.r, black.g, black.b, 1.0);
  cairo_set_line_width(c, bitem->pbi.line_width);
  cairo_stroke(c);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void sat_rectangle(cairo_t *c, double x0, double y0, double d0)
{
  cairo_move_to (c, x0 - d0, y0 - d0);
  cairo_line_to (c, x0 + d0, y0 - d0);
  cairo_line_to (c, x0 + d0, y0 + d0);
  cairo_line_to (c, x0 - d0, y0 + d0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_item_paint_sat(CrItem *item, cairo_t *c)
{
  t_bank_item *bitem = from_critem_to_bank_item(item);
  double x0,y0, d0;
  if (!bitem)
    KOUT(" ");
  x0 = bitem->pbi.x0;
  y0 = bitem->pbi.y0;
  cairo_new_path(c);
  if (bitem->pbi.blink_tx)
    {
    d0 = TAP_DIA/2+TAP_DIA/8;
    sat_rectangle(c, x0, y0, d0);
    cairo_set_source_rgba (c, orange.r, orange.g, orange.b, 0.8);
    cairo_fill(c);
    }
  d0 = TAP_DIA/2;
  sat_rectangle(c, x0, y0, d0);
  cairo_close_path (c);
  if (bitem->pbi.grabbed)
    cairo_set_source_rgba (c, white.r, white.g, white.b, 1.0);
  else
    cairo_set_source_rgba (c, black.r, black.g, black.b, 1.0);
  cairo_stroke_preserve(c);
  cairo_set_source_rgba (c, lightcyan.r, lightcyan.g, lightcyan.b, 1.0);
  cairo_fill(c);
  if (bitem->pbi.blink_rx)
    {
    cairo_arc (c, bitem->pbi.x0, bitem->pbi.y0, INTF_DIA/6, 0, 2*M_PI);
    cairo_set_source_rgba (c, orange.r, orange.g, orange.b, 0.8);
    cairo_fill(c);
    }
  if (bitem->pbi.grabbed)
    cairo_set_source_rgba (c, white.r, white.g, white.b, 1.0);
  else
    cairo_set_source_rgba (c, black.r, black.g, black.b, 1.0);
  cairo_set_line_width(c, bitem->pbi.line_width);
  cairo_stroke(c);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_item_paint_a2b(CrItem *item, cairo_t *c)
{
  t_bank_item *bitem = from_critem_to_bank_item(item);
  double x0,y0, d0;
  if (!bitem)
    KOUT(" ");
  x0 = bitem->pbi.x0;
  y0 = bitem->pbi.y0;
  cairo_new_path(c);
  if (bitem->pbi.blink_tx)
    {
    }
  d0 = A2B_DIA/2;
  cairo_arc (c, x0, y0, d0, 0, 2*M_PI);
  cairo_close_path (c);
  if (bitem->pbi.grabbed)
    cairo_set_source_rgba (c, white.r, white.g, white.b, 1.0);
  else
    cairo_set_source_rgba (c, black.r, black.g, black.b, 1.0);
  cairo_stroke_preserve(c);
  cairo_set_source_rgba (c, lightcyan.r, lightcyan.g, lightcyan.b, 1.0);
  cairo_fill(c);
  if (bitem->pbi.blink_rx)
    {
    }
  if (bitem->pbi.grabbed)
    cairo_set_source_rgba (c, white.r, white.g, white.b, 1.0);
  else
    cairo_set_source_rgba (c, black.r, black.g, black.b, 1.0);
  cairo_set_line_width(c, bitem->pbi.line_width);
  cairo_stroke(c);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_item_paint_d2d(CrItem *item, cairo_t *c)
{
  t_bank_item *bitem = from_critem_to_bank_item(item);
  double x0,y0, d0;
  if (!bitem)
    KOUT(" ");
  x0 = bitem->pbi.x0;
  y0 = bitem->pbi.y0;
  cairo_new_path(c);

  if (bitem->pbi.blink_tx)
    {
    d0 = D2D_DIA/2+D2D_DIA/6; 
    sat_lozange(c, x0, y0, d0);
    cairo_set_source_rgba (c, orange.r, orange.g, orange.b, 0.8);
    cairo_fill(c);
    }
  d0 = D2D_DIA/2; 
  sat_lozange(c, x0, y0, d0);
  cairo_close_path (c);
  if (bitem->pbi.grabbed)
    cairo_set_source_rgba (c, white.r, white.g, white.b, 1.0);
  else
    cairo_set_source_rgba (c, black.r, black.g, black.b, 1.0);
  cairo_stroke_preserve(c);
  paint_color_of_d2d(bitem, c);
  cairo_fill(c);
  if (bitem->pbi.blink_rx)
    {
    cairo_arc (c, bitem->pbi.x0, bitem->pbi.y0, INTF_DIA/6, 0, 2*M_PI);
    cairo_set_source_rgba (c, orange.r, orange.g, orange.b, 0.8);
    cairo_fill(c);
    }
  if (bitem->pbi.grabbed)
    cairo_set_source_rgba (c, white.r, white.g, white.b, 1.0);
  else
    cairo_set_source_rgba (c, black.r, black.g, black.b, 1.0);
  cairo_set_line_width(c, bitem->pbi.line_width);
  cairo_stroke(c);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_item_paint(CrItem *item, cairo_t *c)
{
  t_bank_item *bitem = from_critem_to_bank_item(item);
  if (!bitem)
    KOUT(" ");
  cairo_new_path(c);
  cairo_elem(c, bitem->bank_type, bitem->pbi.x0, bitem->pbi.y0, 
             bitem->pbi.x1, bitem->pbi.y1);
  paint_select_source_color(bitem, c);
  cairo_set_line_width(c, bitem->pbi.line_width);
  cairo_stroke(c);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void edge_sensitivity_surface(cairo_t *c, double x0, double y0,
                                                 double x1, double y1, 
                                                 double dist)
{
  double xa, ya, xm, ym;
  xm = x0 + (x1-x0)/2;
  ym = y0 + (y1-y0)/2;
  xa = (-(y1-y0)/dist)*10;
  ya = ((x1-x0)/dist)*10;
  cairo_new_path(c);
  cairo_move_to (c, x0, y0);
  cairo_line_to (c, xm+xa, ym+ya);
  cairo_line_to (c, x1, y1);
  cairo_line_to (c, xm-xa, ym-ya);
  cairo_close_path (c);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static CrItem *on_item_test(CrItem *item, cairo_t *c, double x, double y)
{
  double x0, y0, d0, x1, y1, dist;
  gboolean inside;
  t_bank_item *bitem = from_critem_to_bank_item(item);
  if (!bitem)
    KOUT(" ");
  cairo_save(c);
  cairo_new_path(c);
  x0 = bitem->pbi.x0;
  x1 = bitem->pbi.x1;
  y0 = bitem->pbi.y0;
  y1 = bitem->pbi.y1;
  dist = bitem->pbi.dist;
  if(bitem->bank_type == bank_type_edge)
    edge_sensitivity_surface(c, x0, y0, x1, y1, dist); 
  else
    {
    if (bitem->bank_type == bank_type_eth) 
      cairo_arc (c, x0, y0, INTF_DIA/2, 0, 2*M_PI);
    else if (bitem->bank_type == bank_type_node) 
      cairo_arc (c, x0, y0, NODE_DIA/2, 0, 2*M_PI);
    else if (bitem->bank_type == bank_type_sat)
      {
      if (is_a_nat(bitem))
        cairo_arc (c, x0, y0, NAT_RAD, 0, 2*M_PI);
      else if (is_a_d2d(bitem))
        {
        d0 = D2D_DIA/2;
        sat_lozange(c, x0, y0, d0);
        cairo_close_path (c);
        }
      else if (is_a_a2b(bitem))
        {
        d0 = A2B_DIA/2;
        cairo_arc (c, x0, y0, d0, 0, 2*M_PI);
        }
      else
        {
        d0 = TAP_DIA/2;
        sat_rectangle(c, x0, y0, d0);
        }
      cairo_close_path (c);
      }
    else
      cairo_elem(c, bitem->bank_type, x0, y0, x1, y1); 
    }
  inside = cairo_in_fill(c, x, y);
  cairo_restore(c);
  return inside ? item : NULL;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean on_item_calcb(CrItem *item, cairo_t *c, 
                       CrBounds *bounds, CrDeviceBounds *device)
{
  t_bank_item *bitem = from_critem_to_bank_item(item);
  if (!bitem)
    KOUT(" ");
  if (!device)
    KOUT(" ");
  cairo_save(c);
  cairo_set_line_width(c, bitem->pbi.line_width);
  cairo_new_path(c);
  if (bitem->bank_type == bank_type_edge)
    edge_sensitivity_surface(c, bitem->pbi.x0, bitem->pbi.y0, 
                             bitem->pbi.x1, bitem->pbi.y1, 
                             bitem->pbi.dist); 
  else
    {
    if (bitem->bank_type == bank_type_eth) 
      cairo_arc (c, bitem->pbi.x0, bitem->pbi.y0, INTF_DIA/2, 0, 2*M_PI);
    else if (bitem->bank_type == bank_type_node)
      cairo_arc (c, bitem->pbi.x0, bitem->pbi.y0, NODE_DIA/2, 0, 2*M_PI);
    else if (bitem->bank_type == bank_type_sat)
      {
      if (is_a_nat(bitem))
        cairo_arc (c, bitem->pbi.x0, bitem->pbi.y0, SNIFF_RAD, 0, 2*M_PI);
      else
        {
        cairo_move_to (c, bitem->pbi.x0-TAP_DIA/2, bitem->pbi.y0-TAP_DIA/2);
        cairo_line_to (c, bitem->pbi.x0+TAP_DIA/2, bitem->pbi.y0-TAP_DIA/2);
        cairo_line_to (c, bitem->pbi.x0+TAP_DIA/2, bitem->pbi.y0+TAP_DIA/2);
        cairo_line_to (c, bitem->pbi.x0-TAP_DIA/2, bitem->pbi.y0+TAP_DIA/2);
        }
      cairo_close_path (c);
      }
    else
      cairo_elem(c, bitem->bank_type, bitem->pbi.x0, bitem->pbi.y0,
                                      bitem->pbi.x1, bitem->pbi.y1); 
    }
  cairo_stroke_extents(c, &bounds->x1, &bounds->y1,
                          &bounds->x2, &bounds->y2);
  cairo_restore(c);
  return TRUE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean on_canvas_event(GtkWidget* widget,
                                GdkEvent *event, 
                                gpointer data)
{
  gboolean result = FALSE;
  if (widget != gtkwidget_canvas)
    KOUT(" ");
  (void) data;
  print_event_item(event->type, "ON_CANVAS");
  if (event->type == GDK_BUTTON_PRESS)
    {
    if (!(get_currently_in_item_surface()))
      {
      if (event->button.button == 1)
        {
        result = TRUE;
        }
      else if (event->button.button == 3)
        {
        canvas_ctx_menu(event->button.x, event->button.y);
        result = TRUE;
        }
      }
    }
  if (event->type == GDK_BUTTON_RELEASE)
    {
    if ((!(get_currently_in_item_surface())) && (event->button.button == 1))
      {
      update_layout_center_scale(__FUNCTION__);
      result = TRUE;
      }
    }
  if (event->type == GDK_LEAVE_NOTIFY)
    {
    cr_glob_focus_out(widget, (GdkEventFocus *)event);
    result = TRUE;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean topo_configure_event (GtkWidget       *widget,
                                      GdkEventConfigure *event,
                                      gpointer         data)
{
  if (widget != get_main_window())
    KOUT(" ");
  if (data)
    KOUT(" ");
  set_main_window_coords(event->x, event->y, event->width, event->height);
  if (layout_get_ready_for_send())
    {
    send_layout_width_height(get_clownix_main_llid(), 8888, 
                             event->width, event->height);
    update_layout_center_scale(__FUNCTION__);
    }
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean topo_mouse_wheel_scroll (GtkWidget* widget,
                                  GdkEventScroll* event,
                                  gpointer data)
{
  if (widget != get_main_window())
    KOUT(" ");
  if (data)
    KOUT(" ");
  if (event->direction == GDK_SCROLL_DOWN)
    topo_zoom_in_out_canvas(1, 1);
  else
    topo_zoom_in_out_canvas(0, 1);
  return TRUE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
GtkWidget *topo_canvas(void)
{
  GtkWidget *vbox, *canvas;
  init_colors();
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  canvas = cr_canvas_new ( "maintain_aspect", TRUE,
                           "auto_scale",      FALSE,
                           "maintain_center", TRUE,
                            NULL );
  gtkwidget_canvas = canvas;

  g_signal_connect(canvas,"event",(GCallback)on_canvas_event, NULL);

  g_object_ref(canvas);
  glob_canvas = CR_CANVAS(canvas);
  cr_canvas_set_scroll_factor(glob_canvas, 3, 3);
  gtk_widget_set_size_request(canvas, WIDTH, HEIGH);
  gtk_box_pack_start (GTK_BOX (vbox), canvas, TRUE, TRUE, 0);
  glob_panner = cr_panner_new(glob_canvas, NULL);
  cr_canvas_center_scale(CR_CANVAS(gtkwidget_canvas), WIDTH/2, HEIGH/2, WIDTH/3, HEIGH/3);
  cr_canvas_set_repaint_mode(glob_canvas, TRUE);
  cr_canvas_set_min_scale_factor (glob_canvas,0.2,0.2);
  cr_canvas_set_max_scale_factor (glob_canvas,5,5);
  return vbox;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void properties_free(ItemProperties *properties)
{
  g_free(properties);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void topo_add_cr_item_to_canvas(t_bank_item *bitem, t_bank_item *bnode)
{
  ItemProperties *properties;
  CrItem *item = g_object_new(CR_TYPE_ITEM, NULL);
  CrItem *root = get_glob_canvas_root();
  properties = g_new0(ItemProperties, 1);
  g_object_set_data_full(G_OBJECT(item), "properties", properties, 
                         (GDestroyNotify) properties_free);             
  g_signal_connect(item, "event", (GCallback) on_item_event, NULL);
  if (bitem->bank_type == bank_type_eth) 
    g_signal_connect(item, "paint", (GCallback) on_item_paint_eth, NULL);
  else if (bitem->bank_type == bank_type_node) 
    g_signal_connect(item, "paint", (GCallback) on_item_paint_node, NULL);
  else if (bitem->bank_type == bank_type_sat)  
    {
    if (is_a_nat(bitem))
      g_signal_connect(item, "paint", (GCallback) on_item_paint_nat, NULL);
    else if (is_a_d2d(bitem))
      g_signal_connect(item, "paint", (GCallback) on_item_paint_d2d, NULL);
    else if (is_a_a2b(bitem))
      g_signal_connect(item, "paint", (GCallback) on_item_paint_a2b, NULL);
    else
      g_signal_connect(item, "paint", (GCallback) on_item_paint_sat, NULL);
    }
  else
    g_signal_connect(item, "paint", (GCallback) on_item_paint, NULL);
  g_signal_connect(item, "test",  (GCallback) on_item_test,  NULL);
  g_signal_connect(item, "calculate_bounds", (GCallback) on_item_calcb, NULL);
  if (bitem->bank_type == bank_type_eth)  
    {
    if (!bnode)
      KOUT(" ");
    cr_item_add(CR_ITEM(bnode->cr_item), item);
    }
  else
    cr_item_add(root, item);
  g_object_unref(G_OBJECT(item));
  properties = g_object_get_data(G_OBJECT(item), "properties");
  properties->bank_item = bitem;
  bitem->root = (void *) root;
  bitem->cr_item = (void *) item;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void topo_set_signals(GtkWidget *window)
{
  g_signal_connect ( G_OBJECT(window), "delete-event",
                     G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(G_OBJECT (window), "scroll-event",
                   G_CALLBACK(topo_mouse_wheel_scroll), NULL);
  g_signal_connect(G_OBJECT (window), "configure_event",
                   (GCallback)topo_configure_event, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void topo_bitem_hide(t_bank_item *bitem)
{
  cr_item_hide(CR_ITEM(bitem->cr_item));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void topo_bitem_show(t_bank_item *bitem)
{
  cr_item_show(CR_ITEM(bitem->cr_item));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void topo_remove_cr_item(t_bank_item *bitem)
{
  cr_item_hide(CR_ITEM(bitem->cr_item));
  if (bitem->bank_type == bank_type_eth) 
    {
    if (!bitem->att_node)
      KOUT(" ");
    cr_item_remove(CR_ITEM(bitem->att_node->cr_item),CR_ITEM(bitem->cr_item));
    }
  else
    cr_item_remove(CR_ITEM(bitem->root),CR_ITEM(bitem->cr_item));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void topo_cr_item_text(t_bank_item *bitem, double x, double y, char *name)
{
  if (bitem->bank_type == bank_type_lan)
    cr_text_new (CR_ITEM(bitem->cr_item), x, y, name, "font", "Sans 6", NULL);
  else if (bitem->bank_type == bank_type_node) 
    {
    cr_text_new (CR_ITEM(bitem->cr_item), x, y, name,  "font", "Sans 10", NULL);
    cr_text_new (CR_ITEM(bitem->cr_item), bitem->pbi.x0 - 8, 
                 bitem->pbi.y0 - 22, "ram", "font", "Sans 6", NULL);
    cr_text_new (CR_ITEM(bitem->cr_item), bitem->pbi.x0 - 8, 
                 bitem->pbi.y0 + 8, "cpu", "font", "Sans 6", NULL);
    }
  else
    cr_text_new (CR_ITEM(bitem->cr_item), x, y, name, "font", "Sans 7", NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*                       topo_get_matrix_transform_point                    */
/*--------------------------------------------------------------------------*/
void topo_get_matrix_transform_point(t_bank_item *bitem, double *x, double *y)
{
  cairo_matrix_t *mat;
  mat = cr_item_get_matrix(CR_ITEM(bitem->cr_item));
  cairo_matrix_transform_point(mat, x, y);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void topo_cr_item_set_z(t_bank_item *bitem)
{
   if (bitem->bank_type == bank_type_eth) 
      {
      if (!bitem->att_node)
        KOUT(" ");
      cr_item_set_z(CR_ITEM(bitem->att_node->root), 
                    CR_ITEM(bitem->att_node->cr_item), -1);
      cr_item_set_z(CR_ITEM(bitem->att_node->cr_item), 
                    CR_ITEM(bitem->cr_item), -1);
      }
  else
    cr_item_set_z(CR_ITEM(bitem->root), CR_ITEM(bitem->cr_item), -1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void topo_get_absolute_coords(t_bank_item *bitem)
{
  double x = bitem->pbi.x0;
  double y = bitem->pbi.y0;
  cairo_matrix_t *mat;
  switch(bitem->bank_type)
    {
    case bank_type_eth:
      if (!bitem->att_node)
        KOUT(" ");
      mat = cr_item_get_matrix(CR_ITEM(bitem->att_node->cr_item));
      cairo_matrix_transform_point(mat, &x, &y);
      mat = cr_item_get_matrix(CR_ITEM(bitem->cr_item));
      cairo_matrix_transform_point(mat, &x, &y);
      bitem->pbi.position_x = x;
      bitem->pbi.position_y = y;
    break;

    case bank_type_lan:
    case bank_type_node:
    case bank_type_sat:
      mat = cr_item_get_matrix(CR_ITEM(bitem->cr_item));
      cairo_matrix_transform_point(mat, &x, &y);
      bitem->pbi.position_x = x;
      bitem->pbi.position_y = y;
    break;

    case bank_type_edge:
    break;
    default:
      KOUT("%d", bitem->bank_type);
    break;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void topo_get_save_params(int *on, int *width, int *height, 
                          int *cx, int *cy, int *cw, int *ch)
{
  double dcx, dcy, dcw, dch;
  *on = !(get_move_freeze());
  gtk_window_get_size(GTK_WINDOW(get_main_window()), width, height);
  cr_canvas_get_center_scale(CR_CANVAS(gtkwidget_canvas),&dcx,&dcy,&dcw,&dch);
  *cx = (int) dcx;
  *cy = (int) dcy;
  *cw = (int) dcw;
  *ch = (int) dch;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void topo_set_window_resize(int width, int height)
{
  gtk_window_resize(GTK_WINDOW(get_main_window()), width, height);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void topo_set_center_scale(int x, int y, int w, int h)
{
  double dcx, dcy, dcw, dch;
  double nx, ny, nw, nh;
  nx = (double) x;
  ny = (double) y;
  nw = (double) w;
  nh = (double) h;
  cr_canvas_center_scale(CR_CANVAS(gtkwidget_canvas), nx, ny, nw, nh);
  cr_canvas_get_center_scale(CR_CANVAS(gtkwidget_canvas),&dcx,&dcy,&dcw,&dch);
}
/*---------------------------------------------------------------------------*/



