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
#include <gtk/gtk.h>
#include <string.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cloonix.h"
#define MAX_WARNING 1000
#define MAX_OFFSET  70
#define MAX_STOPPED  15
#define NB_LINES_IN_SCROLL MAX_WARNING

void put_top_left_icon(GtkWidget *mainwin);

static GtkWidget *glob_trace_textview_win;
static GtkWidget *glob_trace_textview;

static GtkWidget *glob_demo_textview_win;
static GtkWidget *glob_demo_textview;

static char *st_queue[MAX_WARNING];
static int st_queue_write = 0;
static char *queue[MAX_WARNING];
static int must_popup_flag_queue[MAX_WARNING];
static int queue_read = 0;
static int queue_write = 0;
static GtkWidget *popup = NULL;
static GtkWidget *vbox;
static gint x, y, offsety, stopped;
static gint popup_state = 0;
static char *txt_format = "\n<span font_desc=\"Times New Roman italic 12\" "
                          "foreground=\"#000000\"> %s </span>";
static char caption[1000];  

void get_main_window_coords(gint *x, gint *y, gint *width, gint *heigh);


/*****************************************************************************/
/*                   set_police                                              */
/*---------------------------------------------------------------------------*/
static void set_police(GtkWidget *trace_textview)
{
/*
  GtkStyle *style;
  style = gtk_widget_get_style(trace_textview);
  pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_NORMAL);
  pango_font_description_set_family(style->font_desc, "Monospace");
  gtk_widget_modify_font(trace_textview, style->font_desc);
*/
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/* Function                   trace_textview_destroy                         */
/*---------------------------------------------------------------------------*/
static void trace_textview_destroy(GtkWidget *object, gpointer  user_data)
{
  if (glob_trace_textview_win != object)
    KOUT(" ");
  if (user_data)
    KOUT(" ");
  gtk_widget_destroy(glob_trace_textview);
  glob_trace_textview_win = NULL;
  glob_trace_textview = NULL;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                   setup_scroll                                            */
/*---------------------------------------------------------------------------*/
static void setup_scroll (GtkTextView *txtview)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  buffer = gtk_text_view_get_buffer (txtview);
  gtk_text_buffer_get_end_iter (buffer, &iter);
  gtk_text_buffer_create_mark (buffer, "scroll", &iter, TRUE);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                   trace_textview_scroll_to_bottom                         */
/*---------------------------------------------------------------------------*/
static void trace_textview_scroll_to_bottom (char *txt)
{
  int nb_line;
  GtkTextBuffer *buffer;
  GtkTextIter start_iter;
  GtkTextIter iter;
  GtkTextIter end_iter;
  GtkTextMark *mark;
  if (glob_trace_textview)
    {
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW(glob_trace_textview));
    gtk_text_buffer_get_end_iter (buffer, &end_iter);
    gtk_text_buffer_insert (buffer, &end_iter, txt, -1);
    gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
    mark = gtk_text_buffer_get_mark (buffer, "scroll");
    gtk_text_iter_set_line_offset(&end_iter, 0);
    gtk_text_buffer_move_mark (buffer, mark, &end_iter);

    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(glob_trace_textview),mark);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(glob_trace_textview),
                                 mark, 0.0, TRUE, 0.0, 0.0);
    nb_line = gtk_text_buffer_get_line_count(buffer);
    if (nb_line > NB_LINES_IN_SCROLL)
      {
      gtk_text_buffer_get_iter_at_line(buffer,&iter,nb_line-NB_LINES_IN_SCROLL);
      gtk_text_buffer_delete(buffer, &start_iter, &iter);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/* Function             trace_textview_fill_it_up                            */
/*---------------------------------------------------------------------------*/
static void trace_textview_fill_it_up(void)
{
  queue_read = st_queue_write;
  do
    {
    if (st_queue[queue_read])
      {
      trace_textview_scroll_to_bottom (st_queue[queue_read]);
      }
    queue_read++;
    if (queue_read == MAX_WARNING)
      queue_read = 0;
    } while (queue_read != st_queue_write);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void stored_warning_trace_textview_create (void)
{
  GtkWidget *text_view_window,  *swindow, *frame, *trace_textview;
  char title[MAX_NAME_LEN];
  if (glob_trace_textview_win == NULL)
    {
    sprintf(title, "STORED WARNINGS");
    text_view_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    glob_trace_textview_win = text_view_window;
    gtk_window_set_modal (GTK_WINDOW(text_view_window), FALSE);
    gtk_window_set_title (GTK_WINDOW (text_view_window), title);
    gtk_window_set_default_size(GTK_WINDOW(text_view_window), 300, 200);
    put_top_left_icon(text_view_window);
    g_signal_connect (G_OBJECT (text_view_window), "destroy",
                      (GCallback) trace_textview_destroy, NULL);
    swindow = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swindow), 
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    frame = gtk_frame_new ("Warnings");
    gtk_container_add (GTK_CONTAINER (frame), swindow);
    trace_textview = gtk_text_view_new ();
    gtk_container_add (GTK_CONTAINER (swindow), trace_textview);
    setup_scroll (GTK_TEXT_VIEW (trace_textview));
    gtk_container_add (GTK_CONTAINER (text_view_window), frame);
    gtk_widget_show_all(text_view_window);
    set_police(trace_textview);
    glob_trace_textview = trace_textview;
    trace_textview_fill_it_up();
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/*               set_popup_coords                                            */
/*---------------------------------------------------------------------------*/
static void set_popup_coords(void)
{
  gint x_win, y_win, width_win, height_win;
  get_main_window_coords(&x_win, &y_win, &width_win, &height_win);
  x = x_win + 100;
  y = y_win + height_win - 100;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/*                popup_warning                                              */
/*---------------------------------------------------------------------------*/
static void popup_warning(char *input_txt)
{
  GtkWidget *label;
  if (popup_state)
    KOUT(" ");
  set_popup_coords();
  offsety = 0;
  sprintf(caption, txt_format, input_txt);
  popup = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_move(GTK_WINDOW(popup), x, y);
  gtk_window_set_decorated(GTK_WINDOW(popup), FALSE);
//  gtk_container_border_width(GTK_CONTAINER(popup),3);
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  label=gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label), caption);
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 10);
  gtk_container_add (GTK_CONTAINER (popup), vbox);
  gtk_widget_show_all(popup);
  popup_state = 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                extract_next_warning                                       */
/*---------------------------------------------------------------------------*/
static char *extract_next_warning(void)
{
  char *result = NULL;
  while ( queue_read != queue_write )
    {
    if (must_popup_flag_queue[queue_read])
      result = queue[queue_read];
    queue_read += 1;
    if (queue_read == MAX_WARNING)
      queue_read = 0; 
    if (result)
      break;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                popup_warning                                              */
/*---------------------------------------------------------------------------*/
void popup_timeout(void)
{
  char *txt;
  if (popup_state == 0)
    {
    txt = extract_next_warning();
    if (txt)
      {
      popup_warning(txt);
      clownix_free(txt, __FUNCTION__);
      }
    }
  else
    {
    if (popup_state == 1)
      {
      offsety += 10;
      if (offsety > MAX_OFFSET)
        {
        popup_state = 2;
        stopped = 0;
        }
      gtk_window_move (GTK_WINDOW(popup), x, y - offsety);
      gtk_window_set_keep_above(GTK_WINDOW(popup), TRUE);
      }
    else if (popup_state == 2)
      {
      stopped++;
      if (stopped > MAX_STOPPED)
        popup_state = 3;
      }
    else if (popup_state == 3)
      {
      offsety -= 10;
      if (offsety == 0)
        {
        gtk_widget_destroy(popup);
        popup_state = 0;
        popup = NULL;
        }
      else
        {
        gtk_window_move (GTK_WINDOW(popup), x, y - offsety);
        gtk_window_set_keep_above(GTK_WINDOW(popup), TRUE);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*                set_popup_window_coords                                    */
/*---------------------------------------------------------------------------*/
void set_popup_window_coords(void)
{
  set_popup_coords();
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/*                insert_next_warning                                        */
/*---------------------------------------------------------------------------*/
void insert_next_warning(char *input_txt, int must_popup_flag)
{
  queue[queue_write] = (char *) clownix_malloc(strlen(input_txt) + 1, 18);
  strcpy(queue[queue_write], input_txt);
  must_popup_flag_queue[queue_write] = must_popup_flag;
  queue_write++;
  if (queue_write == MAX_WARNING)
    queue_write = 0; 
  if (queue_read == queue_write)
    KOUT(" ");
  if (st_queue[st_queue_write])
    clownix_free(st_queue[st_queue_write], __FUNCTION__);
  st_queue[st_queue_write]=(char *)clownix_malloc(strlen(input_txt) + 2, 18);
  strcpy(st_queue[st_queue_write], input_txt);
  if (st_queue[st_queue_write][strlen(input_txt)] != '\n')
    strcat(st_queue[st_queue_write], "\n");
  if (glob_trace_textview_win)
    trace_textview_scroll_to_bottom(st_queue[st_queue_write]);
  st_queue_write++;
  if (st_queue_write == MAX_WARNING)
    st_queue_write = 0;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
/*                   popup_init                                              */
/*---------------------------------------------------------------------------*/
void popup_init(void)
{
  memset(st_queue, 0, MAX_WARNING * sizeof(char *));
  glob_trace_textview_win = NULL;
  glob_trace_textview = NULL;
  glob_demo_textview_win = NULL;
  glob_demo_textview = NULL;
}
/*---------------------------------------------------------------------------*/

