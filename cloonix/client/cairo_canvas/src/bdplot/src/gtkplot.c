/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include <cairo.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "io_clownix.h"
#include "bdplot.h"
#include "gtkplot.h"

static const int default_precision_graph = 1024;
static const float defaultscaleX = 64;
static const float maxrange = 5000;
static const float rayon = 2;

static float ColorBallsR[] = { 0, 0.8,0,0,0,0,0,0,0,0};
static float ColorBallsG[] = { 0, 0,0.8,0,0,0,0,0,0,0};
static float ColorBallsB[] = { 0.8, 0,0,0,0,0,0,0,0,0};
static float ColorLinkR[] = { 0, 0.5,0,0,0,0,0,0,0,0};
static float ColorLinkG[] = { 0, 0,0.5,0,0,0,0,0,0,0};
static float ColorLinkB[] = { 0.5, 0,0,0,0,0,0,0,0,0};


typedef struct t_Dot
{
  float x;
  float y;
  struct t_Dot *prevdot;
  float r;
  float g;
  float b;
} t_Dot;

typedef struct t_gtk_plot_ctx
{
  char name[MAX_NAME_LEN];
  int  num;
  char title[MAX_NAME_LEN];
  GtkWidget *window;
  t_Dot *bal;
  GArray *dots[NCURVES];
  float zoom_force;
  int precision_graph;
  float scaleX;
  float scaleXMIN;
  float scaleXMAX;
  float marginX;
  float marginY;
  float mousex;
  float mousey;
  float initmousey;
  bool dragging;
  bool manual;
  float oldScaleY;
  float newScaleYrelative;
  float last_date_s;
} t_gtk_plot_ctx;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void addot(t_gtk_plot_ctx *gp, float x, float y, int ind)
{
  t_Dot *first;
  gp->bal = malloc(sizeof(t_Dot));
  memset(gp->bal,0,sizeof(t_Dot));
  gp->bal->x = x;
  gp->bal->y = y;
  gp->bal->r = ColorBallsR[ind];
  gp->bal->g = ColorBallsG[ind];
  gp->bal->b = ColorBallsB[ind];
  if(gp->dots[ind]->len > 0)
    gp->bal->prevdot=g_array_index(gp->dots[ind], t_Dot *,
                                   (gp->dots[ind]->len)-1);
  else
    gp->bal->prevdot=NULL;
  g_array_append_val(gp->dots[ind], gp->bal);
  first = g_array_index(gp->dots[ind], t_Dot *, 0);
  while(gp->dots[ind]->len>0 && first->x*gp->scaleXMIN < 
        gp->bal->x*gp->scaleXMIN - maxrange)
    {
    g_array_remove_index(gp->dots[ind], 0);
    first = g_array_index(gp->dots[ind], t_Dot *, 0);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void do_drawing(t_gtk_plot_ctx *gp, cairo_t *cr)
{
  int i, ind, mint, maxt;
  unsigned int multiple;
  int w = gtk_widget_get_allocated_width (gp->window);
  int h = gtk_widget_get_allocated_height (gp->window);
  float decalagex = 0; //How much to offset the drawing
  float decalagey = 0;
  float maxheight = 0;
  float basedecalx = gp->marginX;
  float basedecaly = h-gp->marginY;
  float tempdx, scaleY, mintime, maxtime, minkb, maxkb;
  t_Dot *bali;
  char output[400];
  int64_t j, precisionkb, mink, maxk;

  if(w<50)
    w=50;
  if(h<50)
    h=50;

  for(ind=0; ind<NCURVES; ind++)
    {
    tempdx = 0;
    if(gp->dots[ind]->len > 0)
      {
      tempdx = w - gp->marginX -20 -
      (g_array_index(gp->dots[ind],t_Dot *,
                     (gp->dots[ind]->len)-1)->x) * gp->scaleX;
      }
    if(tempdx < decalagex)
      decalagex = tempdx;
    }
  decalagey += basedecaly; //To center on the y axis, with the bottom margin
  decalagex += basedecalx; //for the left margin
  for(ind=0; ind<NCURVES; ind++)
    {
    for(i = 0; i<gp->dots[ind]->len; i++)
      {
      bali = g_array_index(gp->dots[ind], t_Dot *, i);
      if (decalagex+bali->x*gp->scaleX>basedecalx)
        {
        if(-bali->y > maxheight)
          maxheight = -bali->y;
        }
      }
    }
  scaleY = gp->oldScaleY;
  if(!gp->manual)
    {
    scaleY = (h-gp->marginY-h/5)/maxheight;
    gp->oldScaleY = scaleY;
    } 
  else
    {
    if(gp->dragging)
      {
      gp->newScaleYrelative = -(gp->mousey-gp->initmousey)/h*gp->zoom_force;
      if(gp->newScaleYrelative < 0)
        gp->newScaleYrelative = 1/(1-gp->newScaleYrelative);
      else
        gp->newScaleYrelative = gp->newScaleYrelative + 1;
      scaleY *= gp->newScaleYrelative;
      }
    }

  //ARROWS
  cairo_set_source_rgb(cr,0,0,0);

  cairo_move_to(cr, gp->marginX, h-gp->marginY);
  cairo_line_to(cr, w-30, h-gp->marginY);
  cairo_stroke(cr);

  cairo_move_to(cr, w-30, h-gp->marginY);
  cairo_line_to(cr, w-30-7, h-gp->marginY-7);
  cairo_stroke(cr);

  cairo_move_to(cr, w-30, h-gp->marginY);
  cairo_line_to(cr, w-30-7, h-gp->marginY+7);
  cairo_stroke(cr);

  cairo_move_to(cr, gp->marginX, h-gp->marginY);
  cairo_line_to(cr, gp->marginX, gp->marginY);
  cairo_stroke(cr);

  cairo_move_to(cr, gp->marginX, gp->marginY);
  cairo_line_to(cr, gp->marginX-7, gp->marginY+7);
  cairo_stroke(cr);

  cairo_move_to(cr, gp->marginX, gp->marginY);
  cairo_line_to(cr, gp->marginX+7, gp->marginY+7);
  cairo_stroke(cr);

  cairo_set_font_size(cr, 12);

  cairo_set_source_rgb(cr,ColorLinkR[0],ColorLinkG[0],ColorLinkB[0]);
  cairo_move_to(cr, gp->marginX*0.2 + 100, 20);
  cairo_show_text(cr, "RX");

  cairo_set_source_rgb(cr,ColorLinkR[1],ColorLinkG[1],ColorLinkB[1]);
  cairo_move_to(cr, gp->marginX*0.2+180, 20);
  cairo_show_text(cr, "TX");

  cairo_set_source_rgb(cr,0,0,0);

  cairo_move_to(cr, w-50, h-gp->marginY*0.2);
  cairo_show_text(cr, "time(s)");

  //LABEL TIME AXE
  mintime = (basedecalx-decalagex)/gp->scaleX*1000/gp->precision_graph;
  maxtime = mintime+(w-gp->marginX-30)/gp->scaleX*1000/gp->precision_graph;

  mint = (int)mintime+1;
  maxt = (int)maxtime;

  cairo_set_font_size(cr, 8);
  for(i = mint; i<maxt; i++)
    {
    snprintf(output, 400, "%f", (float)i*gp->precision_graph/1000);
    output[6] = 0;

    cairo_move_to(cr, decalagex + i*gp->scaleX/1000*gp->precision_graph,
                      h-gp->marginY + 3);
    cairo_line_to(cr, decalagex + i*gp->scaleX/1000*gp->precision_graph,
                      h-gp->marginY - 3);
    cairo_stroke(cr);

    cairo_move_to(cr, decalagex + i*gp->scaleX/1000*gp->precision_graph-10,
                      h-gp->marginY * 0.3);
    cairo_show_text(cr, output);
    }

  //LABEL KBPS AXE
  minkb = 0;
  maxkb = (h-gp->marginY-50)/scaleY*1000;
  multiple = 1;
  if (maxkb > 1000000.0)
    multiple=1000;
  if (maxkb > 1000000000.0)
    multiple=1000000;
  if (maxkb > 1000000000000.0)
    multiple=1000000000;
  precisionkb = (int64_t)(maxkb/10);
  maxkb/=precisionkb;
  mink = (int64_t)minkb+1;
  maxk = (int64_t)maxkb;
  cairo_set_font_size(cr, 8);
  for(j = mink; j<maxk; j++)
    {
    snprintf(output, 400, "%f", j*precisionkb/1000/(float)multiple);
    output[6] = 0;
    cairo_move_to(cr, gp->marginX-3, h-gp->marginY-j*scaleY/1000*precisionkb);
    cairo_line_to(cr, gp->marginX+3, h-gp->marginY-j*scaleY/1000*precisionkb);
    cairo_stroke(cr);
    cairo_move_to(cr, 10,  h-gp->marginY-j*scaleY/1000*precisionkb+5);
    cairo_show_text(cr, output);
    }
  cairo_move_to(cr, gp->marginX*0.2, 20);
  cairo_set_font_size(cr, 10);
  if(multiple == 1000000000)
    cairo_show_text(cr, "GBps");
  else if(multiple == 1000000)
    cairo_show_text(cr, "MBps");
  else if(multiple == 1000)
    cairo_show_text(cr, "kBps");
  else
    cairo_show_text(cr, "Bps");

  ///END ARROWS

  for(ind=0; ind<NCURVES; ind++)
    {
    for(i = 0; i<gp->dots[ind]->len; i++)
      {
      bali = g_array_index(gp->dots[ind], t_Dot *, i);
      if (decalagex + bali->x * gp->scaleX > basedecalx)
        {
        //lines
        if(bali->prevdot && decalagex+bali->prevdot->x*gp->scaleX > basedecalx)
          {
          cairo_set_source_rgb(cr, ColorLinkR[ind],
                               ColorLinkG[ind],ColorLinkB[ind]);
          cairo_move_to(cr, decalagex + (gp->scaleX * bali->x),
                            scaleY * bali->y + decalagey);
          cairo_line_to(cr, decalagex + (gp->scaleX * bali->prevdot->x),
                            scaleY * bali->prevdot->y + decalagey);
          cairo_stroke(cr);
          cairo_move_to(cr, 0, 0);
          }
        //dots
        cairo_set_source_rgb(cr, bali->r, bali->g, bali->b);
        cairo_arc(cr, decalagex + (gp->scaleX * bali->x),
                      scaleY * bali->y + decalagey, rayon, 0, 2 * M_PI);
        cairo_fill(cr);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void gtkplot_newdata(void *gtk_plot_ctx, float date_s, float *bd)
{
  t_gtk_plot_ctx *gp = (t_gtk_plot_ctx *) gtk_plot_ctx;
  int i;
  float delta_s = date_s - gp->last_date_s;
  if (gp->last_date_s != 0)
    {
    if (delta_s < 0.001)
      KERR("%f %f", date_s, gp->last_date_s);
    else
      {
      for(i=0;i<NCURVES;i++)
        addot(gp, date_s, -(bd[i]/delta_s), i);
      gtk_widget_queue_draw(gp->window);
      }
    }
  gp->last_date_s = date_s;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void destroy(GtkWidget *widget, gpointer data)
{ 
  t_gtk_plot_ctx *gp = (t_gtk_plot_ctx *) data;
  bdplot_destroy(gp->name, gp->num);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean onmouse(GtkWidget *widget,
                        GdkEventMotion *event,
                        gpointer data)
{
  t_gtk_plot_ctx *gp = (t_gtk_plot_ctx *) data;
  gp->mousex = event->x;
  gp->mousey = event->y;
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean onpress(GtkWidget *widget,
                        GdkEventButton *event,
                        gpointer data)
{
  t_gtk_plot_ctx *gp = (t_gtk_plot_ctx *) data;
  gp->mousex = event->x;
  gp->mousey = event->y; 
  if (event->type == GDK_BUTTON_PRESS  &&  event->button == 3) //RIGHT
    gp->manual = false;
  if (event->type == GDK_BUTTON_PRESS  &&  event->button == 1) //LEFT
    {
    gp->manual = true;
    gp->dragging = true;
    gp->initmousey = gp->mousey;
    }
  return true;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean onrelease(GtkWidget *widget,
                          GdkEventButton *event,
                          gpointer data)
{
  t_gtk_plot_ctx *gp = (t_gtk_plot_ctx *) data;
  gp->mousex = event->x;
  gp->mousey = event->y;
  if (event->button == 1)
    {
    gp->oldScaleY *= gp->newScaleYrelative;
    gp->dragging = false;
    }
  return true;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean on_draw_event(GtkWidget *widget,
                              cairo_t *cr,
                              gpointer data)
{
  t_gtk_plot_ctx *gp = (t_gtk_plot_ctx *) data;
  do_drawing(gp, cr);
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean onscroll(GtkWidget *widget,
                         GdkEventScroll *event,
                         gpointer data)
{
  t_gtk_plot_ctx *gp = (t_gtk_plot_ctx *) data;
  if(event->direction==1)
    {
    gp->scaleX*=0.5;
    if(gp->scaleX <= gp->scaleXMIN)
      gp->scaleX = gp->scaleXMIN;
    gp->precision_graph = default_precision_graph*(defaultscaleX/gp->scaleX);
    }
  else if(event->direction==0)
    {
    gp->scaleX*=2;
    if(gp->scaleX >= gp->scaleXMAX)
      gp->scaleX = gp->scaleXMAX;
    gp->precision_graph = default_precision_graph*(defaultscaleX/gp->scaleX);
    }
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static GtkWidget *gtkplot_create(void *gtk_plot_ctx)
{
  GtkWidget *window;
  GtkWidget *darea;
  int ind;
  t_gtk_plot_ctx *gp = gtk_plot_ctx;
  for(ind = 0;ind<NCURVES;ind++)
   gp->dots[ind] = g_array_new (false, false, sizeof (t_Dot *));
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  darea = gtk_drawing_area_new();
  g_signal_connect(G_OBJECT(window),"destroy", G_CALLBACK(destroy), gp);
  gtk_container_add(GTK_CONTAINER(window), darea);
  gtk_widget_set_events (window, GDK_EXPOSURE_MASK       |
                                 GDK_LEAVE_NOTIFY_MASK   |
                                 GDK_POINTER_MOTION_MASK |
                                 GDK_BUTTON_PRESS_MASK   |
                                 GDK_SCROLL_MASK         |
                                 GDK_BUTTON_RELEASE_MASK);
  g_signal_connect(G_OBJECT(darea),  "draw",
                                     G_CALLBACK(on_draw_event), gp);
  g_signal_connect(G_OBJECT(window), "scroll-event",
                                     G_CALLBACK(onscroll), gp);
  g_signal_connect(G_OBJECT(window), "motion_notify_event",
                                     G_CALLBACK(onmouse), gp);
  g_signal_connect(G_OBJECT(window), "button-press-event",
                                     G_CALLBACK(onpress), gp);
  g_signal_connect(G_OBJECT(window), "button-release-event",
                                     G_CALLBACK(onrelease),gp);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
  gtk_window_set_title(GTK_WINDOW(window), gp->title);
  gtk_widget_show_all(window);
  return window;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void *gtkplot_alloc(char *name, int num)
{
  t_gtk_plot_ctx *gt = (t_gtk_plot_ctx *) malloc(sizeof(t_gtk_plot_ctx));
  memset(gt, 0, sizeof(t_gtk_plot_ctx));
  strncpy(gt->name, name, MAX_NAME_LEN-1);
  gt->num = num;
  snprintf(gt->title, MAX_NAME_LEN-1, "%s_eth%d", name, num);
  gt->zoom_force = 10;
  gt->precision_graph = 1024; //in ms, relative to default scaleX
  gt->scaleX = 32; //=1 for 1px on graph per second; =1000 for 1px per milli
  gt->scaleXMIN = 2;
  gt->scaleXMAX = 512;
  gt->marginX = 80;
  gt->marginY = 30;
  gt->mousex = 0;
  gt->mousey = 0;
  gt->initmousey = 0;
  gt->dragging = false;
  gt->manual = false;
  gt->oldScaleY = 100;
  gt->newScaleYrelative = 1;
  gt->last_date_s = 0;
  gt->window = gtkplot_create(gt);
  return (void *) gt;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void gtkplot_free(void *gtk_plot_ctx)
{
  free(gtk_plot_ctx);
}
/*--------------------------------------------------------------------------*/



