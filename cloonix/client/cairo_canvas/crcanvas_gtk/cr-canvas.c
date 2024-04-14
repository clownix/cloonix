#include <math.h>
#include <gtk/gtk.h>
#include <syslog.h>
#include "cr-marshal.h"
#include "cr-canvas.h"

static GObjectClass *parent_class = NULL;

enum {
  ARG_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_MAINTAIN_CENTER,
  PROP_MAINTAIN_ASPECT,
  PROP_AUTO_SCALE,
  PROP_SHOW_LESS,
  PROP_REPAINT_MODE,
  PROP_ROOT,
  PROP_PICK_ITEM,
  PROP_REPAINT_ON_SCROLL
};

enum {
  SCROLL_REGION_CHANGED,
  BEFORE_PAINT,
  EVENT_ROOT,
  LAST_SIGNAL
};

static guint cr_canvas_signals[LAST_SIGNAL] = { 0 };

/****************************************************************************/
static void update_adjustment_factor(CrCanvas *canvas,
                                     GtkAdjustment *adjustment,
                                     gdouble scroll_factor,
                                     gdouble viewport_length)
{
  gdouble region;
  region = MAX(1, viewport_length * scroll_factor);
  if (gtk_adjustment_get_upper(adjustment) != region)
    {
    gtk_adjustment_set_lower(adjustment, 0);
    gtk_adjustment_set_upper(adjustment, region);
    gtk_adjustment_set_page_size(adjustment, viewport_length);
    gtk_adjustment_set_page_increment(adjustment, viewport_length);
    gtk_adjustment_set_step_increment(adjustment, viewport_length/10);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_adjustment_world(CrCanvas *canvas,
                                    GtkAdjustment *adjustment,
                                    gdouble value, gdouble upper,
                                    gdouble viewport_length)
{
  double length = MIN(viewport_length, upper);
  if ((length != gtk_adjustment_get_page_size(adjustment)) || 
      (upper != gtk_adjustment_get_upper(adjustment)))
    {
    gtk_adjustment_set_page_size(adjustment, length);
    gtk_adjustment_set_page_increment(adjustment, length);
    gtk_adjustment_set_step_increment(adjustment, length/10);
    gtk_adjustment_set_upper(adjustment, upper);
    gtk_adjustment_set_lower(adjustment, 0);
    }
  value = CLAMP(value, 0, gtk_adjustment_get_upper(adjustment) - length);
  if (value != gtk_adjustment_get_value(adjustment))
    {
    gtk_adjustment_set_value(adjustment, value);
    g_signal_handlers_block_matched(adjustment, G_SIGNAL_MATCH_DATA,
                                    0, 0, NULL, NULL, canvas);
    g_signal_handlers_unblock_matched(adjustment, G_SIGNAL_MATCH_DATA,
                                      0, 0, NULL, NULL, canvas);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_adjustments(CrCanvas *canvas)
{
  gdouble width, height, x1, y1, x2, y2, z, vx, vy, dx, dy;
  gdouble x, y, old_x, old_y;
  cairo_matrix_t *matrix, *matrix_i;
  CrItem *item;
  GtkAllocation *allocation = g_new0 (GtkAllocation, 1);
  gtk_widget_get_allocation(GTK_WIDGET(canvas), allocation);
  width = allocation->width;
  height = allocation->height;
  g_free (allocation);
  item = canvas->root;
  if (canvas->flags & CR_CANVAS_SCROLL_WORLD)
    {
    x1 = canvas->scroll_x1;
    y1 = canvas->scroll_y1;
    x2 = canvas->scroll_x2;
    y2 = canvas->scroll_y2;
    matrix = cr_item_get_matrix(item);
    matrix_i = cr_item_get_inverse_matrix(item);
    z = 0;
    cairo_matrix_transform_point(matrix, &x1, &z);
    z = 0;
    cairo_matrix_transform_point(matrix, &x2, &z);
    z = 0;
    cairo_matrix_transform_point(matrix, &z, &y1);
    z = 0;
    cairo_matrix_transform_point(matrix, &z, &y2);
    dx = fabs(x2 - x1);
    dy = fabs(y2 - y1);
    vx = fabs(MIN(x1,x2));
    vy = fabs(MIN(y1,y2));
    canvas->value_x = vx;
    canvas->value_y = vy;
    update_adjustment_world(canvas, canvas->hadjustment, vx, dx, width);
    update_adjustment_world(canvas, canvas->vadjustment, vy, dy, height);
    if ((width > dx+1) || (height > dy+1))
      {
      x = (canvas->scroll_x2 - canvas->scroll_x1) / 2 + canvas->scroll_x1;
      y = (canvas->scroll_y2 - canvas->scroll_y1) / 2 + canvas->scroll_y1;
      old_x = width/2;
      old_y = height/2;
      cairo_matrix_transform_point(matrix_i, &old_x, &old_y);
      if (old_x - x != 0 || old_y - y != 0)
        cairo_matrix_translate(matrix, old_x - x, old_y - y);
      }
    }
  else
    {
    update_adjustment_factor(canvas, canvas->hadjustment,
                             canvas->scroll_factor_x, width);
    update_adjustment_factor(canvas, canvas->vadjustment,
                             canvas->scroll_factor_y, height);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void recenter_adjustment(GtkAdjustment *adjustment,
                                gdouble *newval_ref)
{
  *newval_ref = (gtk_adjustment_get_upper(adjustment) - 
                 gtk_adjustment_get_page_size(adjustment) -
                 gtk_adjustment_get_lower(adjustment)) / 2;
  if (*newval_ref != gtk_adjustment_get_value(adjustment))
    gtk_adjustment_set_value(adjustment, *newval_ref);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void recenter_adjustments(CrCanvas *canvas)
{
  if (!(canvas->flags & CR_CANVAS_SCROLL_WORLD))
    {
    recenter_adjustment(canvas->hadjustment, &canvas->value_x);
    recenter_adjustment(canvas->vadjustment, &canvas->value_y);
    }
  g_signal_emit(canvas, cr_canvas_signals[SCROLL_REGION_CHANGED], 0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean on_update_idle(CrCanvas *canvas)
{
  (*CR_CANVAS_GET_CLASS(canvas)->update)(canvas);
  canvas->update_idle_id = 0;
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean repaint_revert(CrCanvas *canvas) 
{
  if (canvas->flags & CR_CANVAS_REPAINT_REVERT)
    {
    (*CR_CANVAS_GET_CLASS(canvas)->quick_update)(canvas);
    canvas->flags &= ~(CR_CANVAS_REPAINT_MODE | CR_CANVAS_REPAINT_REVERT);
    }
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean on_repaint_revert_idle(CrCanvas *canvas)
{
  cairo_t *cr;
  GdkDrawingContext *context;
  GdkWindow *window;
  cairo_region_t *cairo_region;
  canvas->update_idle_id = g_idle_add_full(G_PRIORITY_LOW,
                                           (GSourceFunc) repaint_revert,
                                           canvas, NULL);
  window = gtk_widget_get_window(GTK_WIDGET(canvas));
  cairo_region = gdk_window_get_clip_region(window);
  context = gdk_window_begin_draw_frame(window, cairo_region);
  cr = gdk_drawing_context_get_cairo_context(context);
  g_signal_emit(canvas, cr_canvas_signals[BEFORE_PAINT], 0, cr,
                  (canvas->flags & CR_CANVAS_VIEWPORT_CHANGED) != 0);
  canvas->flags &= ~CR_CANVAS_VIEWPORT_CHANGED;
  gdk_window_end_draw_frame(window, context);
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update(CrCanvas *canvas)
{
  cairo_t *cr;
  GdkDrawingContext *context;
  GdkWindow *window;
  cairo_region_t *cairo_region;
  if (!gtk_widget_get_mapped (GTK_WIDGET(canvas)))
    return;
  window = gtk_widget_get_window(GTK_WIDGET(canvas));
  cairo_region = gdk_window_get_clip_region(window);
  context = gdk_window_begin_draw_frame(window, cairo_region);
  cr = gdk_drawing_context_get_cairo_context(context);
  if (canvas->update_idle_id)
    {
    g_source_remove(canvas->update_idle_id);
    canvas->update_idle_id = 0;
    }
  canvas->flags &= ~CR_CANVAS_NEED_UPDATE;
  cairo_identity_matrix(cr);
  cr_item_report_old_bounds(canvas->root, cr, FALSE);
  cairo_identity_matrix(cr);
  cr_item_report_new_bounds(canvas->root, cr, FALSE);
  if (canvas->flags & CR_CANVAS_NEED_UPDATE)
    {
    g_warning("Canvas item requested update during update loop.");
    (*CR_CANVAS_GET_CLASS(canvas)->update)(canvas);
    }
  g_signal_emit(canvas, cr_canvas_signals[BEFORE_PAINT], 0, cr,
                (canvas->flags & CR_CANVAS_VIEWPORT_CHANGED) != 0);
  canvas->flags &= ~CR_CANVAS_VIEWPORT_CHANGED;
  gdk_window_end_draw_frame(window, context);
  if (!(canvas->flags & CR_CANVAS_REPAINT_MODE) &&
       (canvas->flags & CR_CANVAS_NEED_UPDATE))
    {
    if (canvas->flags & CR_CANVAS_INSIDE_UPDATE)
      {
      g_warning("before-paint signal triggers update twice"
                " further recursion has been stopped.");
      }
    else
      {
      canvas->flags |= CR_CANVAS_INSIDE_UPDATE;
      (*CR_CANVAS_GET_CLASS(canvas)->update)(canvas);
      canvas->flags &= ~CR_CANVAS_INSIDE_UPDATE;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void quick_update(CrCanvas *canvas) 
{
  cairo_t *cr;
  GdkDrawingContext *context;
  GdkWindow *window;
  cairo_region_t *cairo_region;
  if (!gtk_widget_get_mapped(GTK_WIDGET(canvas)))
    return;
  window = gtk_widget_get_window(GTK_WIDGET(canvas));
  cairo_region = gdk_window_get_clip_region(window);
  context = gdk_window_begin_draw_frame(window, cairo_region);
  cr = gdk_drawing_context_get_cairo_context(context);
  if (canvas->update_idle_id)
    {
    g_source_remove(canvas->update_idle_id);
    canvas->update_idle_id = 0;
    }
  canvas->flags |= CR_CANVAS_NEED_UPDATE;
  canvas->flags |= CR_CANVAS_IGNORE_INVALIDATE;
  cairo_identity_matrix(cr);
  cr_item_report_new_bounds(canvas->root, cr, FALSE);
  canvas->flags &= ~CR_CANVAS_NEED_UPDATE;
  canvas->flags &= ~CR_CANVAS_IGNORE_INVALIDATE;
  gdk_window_end_draw_frame(window, context);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_item_request_update(CrCanvas *canvas)
{
  if (gtk_widget_get_realized(GTK_WIDGET(canvas)) && !(canvas->flags & 
                             (CR_CANVAS_IN_EXPOSE | CR_CANVAS_NEED_UPDATE)))
    {
    if (canvas->update_idle_id)
      canvas->update_idle_id = 0;
    if (canvas->flags & CR_CANVAS_REPAINT_MODE)
      {
      canvas->update_idle_id = g_idle_add_full(GDK_PRIORITY_REDRAW - 20,
                                               (GSourceFunc) on_repaint_revert_idle,
                                               canvas, NULL);
      gtk_widget_queue_draw(GTK_WIDGET(canvas));
      }
    else
      {  
      canvas->update_idle_id = g_idle_add_full(GDK_PRIORITY_REDRAW - 20,
                                               (GSourceFunc) on_update_idle,
                                               canvas, NULL);
      }
    }
  canvas->flags |= CR_CANVAS_NEED_UPDATE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_item_invalidated(CrCanvas *canvas, int mask,
                                double x1, double y1, double x2, double y2,
                                CrDeviceBounds *device)
{
  GdkRectangle rect;
  if (canvas->flags & CR_CANVAS_IGNORE_INVALIDATE)
    return;
  if (device)
    {
    rect.x = x2 + device->x1 - 1;
    rect.width = device->x2 - device->x1 + 2;
    rect.x = x2 + device->x1 - 1;
    rect.width = device->x2 - device->x1 + 2;
    gdk_window_invalidate_rect(gtk_widget_get_window(GTK_WIDGET(canvas)), &rect, FALSE);
    }
  if (x2 - x1 <= 0 || y2 - y1 <= 0)
    return;
  rect.x = (int) x1;
  rect.y = (int) y1;
  rect.width = (int) (x2 - x1 + 2);
  rect.height = (int) (y2 - y1 + 2);
  gdk_window_invalidate_rect(gtk_widget_get_window(GTK_WIDGET(canvas)), &rect, FALSE);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void do_item_added(CrItem *newitem, CrCanvas *canvas)
{
  g_signal_connect_swapped(newitem, "request_update",
                           (GCallback)on_item_request_update, canvas);

  g_list_foreach(newitem->items, (GFunc) do_item_added, canvas);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_item_added(CrCanvas *canvas, CrItem *newitem, CrItem *root)
{
  do_item_added(newitem, canvas);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void do_item_removed(CrItem *olditem, CrCanvas *canvas)
{
  g_signal_handlers_disconnect_by_func(olditem, (GCallback)on_item_request_update, canvas);
  g_list_foreach(olditem->items, (GFunc) do_item_removed, canvas);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_item_removed(CrCanvas *canvas, CrItem *olditem, CrItem *root)
{
  do_item_removed(olditem, canvas);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_root(CrCanvas *canvas, CrItem *item)
{
  if (canvas->root)
    {
    g_signal_handlers_disconnect_by_func(canvas->root, (GCallback)on_item_added, canvas);
    g_signal_handlers_disconnect_by_func(canvas->root, (GCallback)on_item_removed, canvas);
    g_signal_handlers_disconnect_by_func(canvas->root, (GCallback)on_item_invalidated, canvas);
    g_object_unref(canvas->root);
    canvas->root = NULL;
    }
  if (item)
    {
    canvas->root = item;
    g_object_ref(item);
    g_signal_connect_swapped(canvas->root, "added", (GCallback)on_item_added, canvas);
    g_signal_connect_swapped(canvas->root, "removed", (GCallback)on_item_removed, canvas);
    g_signal_connect_swapped(canvas->root, "invalidate", (GCallback)on_item_invalidated, canvas);
    on_item_added(canvas, canvas->root, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cr_canvas_dispose(GObject *object)
{
  CrCanvas *canvas;
  canvas = CR_CANVAS(object);
  if (canvas->update_idle_id)
    canvas->update_idle_id = 0;
  (*CR_CANVAS_GET_CLASS(canvas)->set_root)(canvas, NULL);
  if (canvas->hadjustment)
    g_object_unref(canvas->hadjustment);
  if (canvas->vadjustment)
    g_object_unref(canvas->vadjustment);
  canvas->hadjustment = canvas->vadjustment = NULL;
  parent_class->dispose(object);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cr_canvas_finalize(GObject *object)
{
  parent_class->finalize(object);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cr_canvas_set_property(GObject *object, guint property_id,
                                   const GValue *value, GParamSpec *pspec)
{
  CrCanvas *canvas = (CrCanvas*) object;
  gboolean bval;
  guint32 flag = 0;
  switch (property_id)
    {
    case PROP_HADJUSTMENT:
      cr_canvas_set_hadjustment (canvas, (GtkAdjustment*) g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      cr_canvas_set_vadjustment (canvas, (GtkAdjustment*) g_value_get_object (value));
      break;
    case PROP_MAINTAIN_CENTER:
      flag = CR_CANVAS_MAINTAIN_CENTER;
      break;
    case PROP_MAINTAIN_ASPECT:
      flag = CR_CANVAS_MAINTAIN_ASPECT;
      break;
    case PROP_AUTO_SCALE:
      flag = CR_CANVAS_AUTO_SCALE;
      break;
    case PROP_SHOW_LESS:
      flag = CR_CANVAS_SHOW_LESS;
      break;
    case PROP_REPAINT_MODE:
      cr_canvas_set_repaint_mode(canvas, g_value_get_boolean(value));
      break;
    case PROP_ROOT:
      (*CR_CANVAS_GET_CLASS(canvas)->set_root)(canvas, g_value_get_object(value));
      break;
    case PROP_REPAINT_ON_SCROLL:
      flag = CR_CANVAS_REPAINT_ON_SCROLL;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
  if (flag)
    {
    bval = g_value_get_boolean(value);
    if (bval)
      canvas->flags |= flag;
    else
      canvas->flags &= ~flag;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cr_canvas_get_property(GObject *object, guint property_id,
                                   GValue *value, GParamSpec *pspec)
{
  CrCanvas *canvas = (CrCanvas*) object;
  switch (property_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, canvas->hadjustment);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, canvas->vadjustment);
      break;
    case PROP_MAINTAIN_CENTER:
      g_value_set_boolean(value, canvas->flags & CR_CANVAS_MAINTAIN_CENTER);
      break;
    case PROP_MAINTAIN_ASPECT:
      g_value_set_boolean(value, canvas->flags & CR_CANVAS_MAINTAIN_ASPECT);
      break;
    case PROP_AUTO_SCALE:
      g_value_set_boolean(value, canvas->flags & CR_CANVAS_AUTO_SCALE);
      break;
    case PROP_SHOW_LESS:
      g_value_set_boolean(value, canvas->flags & CR_CANVAS_SHOW_LESS);
      break;
    case PROP_REPAINT_MODE:
      g_value_set_boolean(value, canvas->flags & CR_CANVAS_REPAINT_MODE);
      break;
    case PROP_ROOT:
      g_value_set_object (value, canvas->root);
      break;
    case PROP_PICK_ITEM:
      g_value_set_object (value, canvas->pick_item);
      break;
    case PROP_REPAINT_ON_SCROLL:
      g_value_set_boolean(value, canvas->flags & CR_CANVAS_REPAINT_ON_SCROLL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cr_canvas_init(CrCanvas *canvas)
{
  CrItem *item;
  gtk_widget_set_can_focus(GTK_WIDGET(canvas), TRUE);
  item = g_object_new(CR_TYPE_ITEM, NULL);
  (*CR_CANVAS_GET_CLASS(canvas)->set_root)(canvas, item);
  g_object_unref(item);
  gtk_widget_set_has_window (GTK_WIDGET(canvas), TRUE);
  canvas->flags |= CR_CANVAS_MAINTAIN_ASPECT;
  canvas->scroll_factor_x = canvas->scroll_factor_y = 1.0;
  cr_canvas_set_vadjustment(canvas, g_object_new(GTK_TYPE_ADJUSTMENT, NULL));
  cr_canvas_set_hadjustment(canvas, g_object_new(GTK_TYPE_ADJUSTMENT, NULL));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gint on_expose(GtkWidget *widget, cairo_t *cr)
{
  CrCanvas *canvas;
  CrItem *item;
  if (GTK_WIDGET_CLASS(parent_class)->draw)
    GTK_WIDGET_CLASS(parent_class)->draw(widget, cr);
  canvas = CR_CANVAS(widget);
  item = CR_ITEM(canvas->root);
  canvas->flags |= CR_CANVAS_IN_EXPOSE;
  update_adjustments(canvas);
  cr_item_invoke_paint(item, cr, (canvas->flags & CR_CANVAS_REPAINT_MODE),0,0,0,0);
  if (canvas->flags & CR_CANVAS_REPAINT_MODE)
    canvas->flags &= ~CR_CANVAS_NEED_UPDATE;
  canvas->flags &= ~CR_CANVAS_IN_EXPOSE;
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void maintain_center(CrCanvas *canvas, gdouble neww, gdouble newh,
                            gdouble oldw, gdouble oldh)
{
  CrItem *item;
  gdouble dx, dy;
  int wx, wy;
  item = CR_ITEM(canvas->root);
  dx = (neww - oldw)/2;
  dy = (newh - oldh)/2;
  wx = (int) dx;
  wy = (int) dy;
  cairo_matrix_transform_distance(cr_item_get_inverse_matrix(item), &dx, &dy);
  cairo_matrix_translate(cr_item_get_matrix(item), dx, dy);
  *item->matrix_p = *item->matrix;
  if (gtk_widget_get_window(GTK_WIDGET(canvas)))
    gdk_window_scroll(gtk_widget_get_window(GTK_WIDGET(canvas)), wx, wy);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void calculate_scale_factors(CrCanvas *canvas, gdouble width,
                                    gdouble height, gdouble *factor_x,
                                    gdouble *factor_y)
{
  *factor_x = width / canvas->init_w;
  *factor_y = height / canvas->init_h;
  gboolean show_less, less_than;
  if (canvas->flags & CR_CANVAS_MAINTAIN_ASPECT)
    {
    less_than = (fabs(1. - *factor_x) < fabs(1. - *factor_y));
    show_less = ((canvas->flags & CR_CANVAS_SHOW_LESS) != 0);
    if (less_than ^ show_less)
      *factor_y = *factor_x;
    else
      *factor_x = *factor_y;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void auto_scale(CrCanvas *canvas, gdouble neww, gdouble newh,
                       gdouble oldw, gdouble oldh)
{
  CrItem *item;
  gdouble cx, cy, ncx, ncy;
  gdouble old_fx, old_fy, new_fx, new_fy, factor_x, factor_y;
  cairo_matrix_t *matrix, *matrix_i;
  item = canvas->root;
  matrix = cr_item_get_matrix(item);
  matrix_i = cr_item_get_inverse_matrix(item);
  calculate_scale_factors(canvas, neww, newh, &new_fx, &new_fy);
  calculate_scale_factors(canvas, oldw, oldh, &old_fx, &old_fy);
  factor_x = new_fx / old_fx;
  factor_y = new_fy / old_fy;
  if (canvas->flags & CR_CANVAS_MAINTAIN_CENTER)
    {
    cx = oldw/2;
    cy = oldh/2;
    }
  else
    cx = cy = 0;
  cairo_matrix_transform_point(matrix_i, &cx, &cy);
  cairo_matrix_scale(matrix, factor_x, factor_y);
  matrix_i = cr_item_get_inverse_matrix(item);
  if (canvas->flags & CR_CANVAS_MAINTAIN_CENTER)
    {
    ncx = neww/2;
    ncy = newh/2;
    }
  else
    ncx = ncy = 0;
  cairo_matrix_transform_point(matrix_i, &ncx, &ncy);
  cairo_matrix_translate(matrix, ncx - cx, ncy - cy);
  cr_canvas_queue_repaint(canvas);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void apply_min_scale_factor(CrCanvas *canvas, double width, double height)
{
  double x_factor, y_factor;
  x_factor = width / (canvas->scroll_x2-canvas->scroll_x1);
  y_factor = height / (canvas->scroll_y2-canvas->scroll_y1);
  if (canvas->flags & CR_CANVAS_MAINTAIN_ASPECT)
    x_factor = y_factor = MAX(x_factor, y_factor);
  cr_canvas_set_min_scale_factor(canvas, x_factor, y_factor);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void size_allocate(GtkWidget *widget, GtkAllocation *alocation)
{
  CrCanvas *canvas;
  gdouble neww, newh, oldw, oldh;
  neww = alocation->width;
  newh = alocation->height;
  if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
    (* GTK_WIDGET_CLASS (parent_class)->size_allocate)(widget, alocation);
  canvas = CR_CANVAS(widget);
  oldw = canvas->last_w;
  oldh = canvas->last_h;
  if (oldw > 1 && neww > 1)
    {
    if (canvas->flags & CR_CANVAS_SCROLL_WORLD)
      apply_min_scale_factor(canvas, neww, newh);
    if (canvas->flags & CR_CANVAS_AUTO_SCALE)
      auto_scale(canvas, neww, newh, oldw, oldh);
    else if (canvas->flags & CR_CANVAS_MAINTAIN_CENTER)
      maintain_center(canvas, neww, newh, oldw, oldh);
    }
  else
    {
    canvas->init_w = neww;
    canvas->init_h = newh;
    }
  update_adjustments(canvas);
  canvas->last_w = neww;
  canvas->last_h = newh;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_scroll_adjustments(CrCanvas *canvas,
                                   GtkAdjustment *hadjustment,
                                   GtkAdjustment *vadjustment)
{
  if (hadjustment)
    cr_canvas_set_hadjustment(canvas, hadjustment);
  if (vadjustment)
    cr_canvas_set_vadjustment(canvas, vadjustment);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void realize(GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint      attributes_mask;
  CrCanvas *canvas;
  GtkAllocation *allocation = g_new0 (GtkAllocation, 1);
  canvas = CR_CANVAS(widget);
  if (canvas->pick_item)
    {
    g_object_remove_weak_pointer(G_OBJECT(canvas->pick_item), (gpointer)&canvas->pick_item);
    canvas->pick_item = NULL;
    }
  canvas->pick_button = 0;
  gtk_widget_set_realized(widget, TRUE);
  gtk_widget_get_allocation(GTK_WIDGET(widget), allocation);
  attributes.x = allocation->x;
  attributes.y = allocation->y;
  attributes.width = allocation->width;
  attributes.height = allocation->height;
  g_free (allocation);
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = GDK_EXPOSURE_MASK | gtk_widget_get_events (widget);
  attributes.visual = gtk_widget_get_visual (widget);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL ;
  gtk_widget_set_window(widget, 
                        gdk_window_new(gtk_widget_get_window(gtk_widget_get_parent(widget)),
                        &attributes, attributes_mask));
  gdk_window_set_events(gtk_widget_get_window(widget), 
                        (gdk_window_get_events (gtk_widget_get_window(widget))
                         | GDK_EXPOSURE_MASK
                         | GDK_BUTTON_PRESS_MASK
                         | GDK_BUTTON_RELEASE_MASK
                         | GDK_POINTER_MOTION_MASK
                         | GDK_KEY_PRESS_MASK
                         | GDK_KEY_RELEASE_MASK
                         | GDK_ENTER_NOTIFY_MASK
                         | GDK_LEAVE_NOTIFY_MASK
                         | GDK_FOCUS_CHANGE_MASK));
  gdk_window_set_user_data (gtk_widget_get_window(widget), widget);
  recenter_adjustments(CR_CANVAS(widget));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void map(GtkWidget *widget)
{
  CrCanvas *canvas;
  if (GTK_WIDGET_CLASS(parent_class)->map)
    GTK_WIDGET_CLASS(parent_class)->map(widget);
  canvas = CR_CANVAS(widget);
  if (canvas->flags & CR_CANVAS_NEED_UPDATE)
    (*CR_CANVAS_GET_CLASS(canvas)->update)(canvas);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean motion_event(GtkWidget *widget, GdkEventMotion *event)
{
  CrItem *root_item, *new_item;
  CrCanvas *canvas;
  cairo_t *cr;
  GdkEvent event_copy;
  gboolean state;
  cairo_matrix_t pick_matrix;
  GdkDrawingContext *context;
  GdkWindow *window;
  cairo_region_t *cairo_region;
  canvas = CR_CANVAS(widget);
  root_item = CR_ITEM(canvas->root);
  state = FALSE;
  event_copy = *((GdkEvent *) event);
  new_item = NULL;
  window = gtk_widget_get_window(widget);
  cairo_region = gdk_window_get_clip_region(window);
  context = gdk_window_begin_draw_frame(window, cairo_region);
  cr = gdk_drawing_context_get_cairo_context(context);
  cairo_matrix_init_identity(&pick_matrix);
  if (canvas->pick_item)
    cr_item_find_child(root_item, &pick_matrix, canvas->pick_item);
  if (canvas->pick_button && canvas->pick_item)
    {
    state = cr_item_invoke_event(canvas->pick_item,
                                (GdkEvent*) event, &pick_matrix,
                                canvas->pick_item);
    }
  else
    {
    canvas->pick_button = 0;
    cairo_identity_matrix(cr);
    new_item = cr_item_invoke_test(root_item, cr, event->x, event->y);
    if (new_item == canvas->pick_item)
      {
      gdk_window_end_draw_frame(window, context);
      return state;
      }
    event_copy.crossing.x = event->x;
    event_copy.crossing.y = event->y;
    event_copy.crossing.x_root = event->x_root;
    event_copy.crossing.y_root = event->y_root;
    if (canvas->pick_item)
      {
      event_copy.type = GDK_LEAVE_NOTIFY;
      state |= cr_item_invoke_event(canvas->pick_item, &event_copy,
                                    &pick_matrix, canvas->pick_item);
      }
    if (new_item)
      {
      cairo_get_matrix(cr, &pick_matrix);
      event_copy.type = GDK_ENTER_NOTIFY;
      canvas->pick_item = new_item;
      g_object_add_weak_pointer(G_OBJECT(new_item),
                                (gpointer)&canvas->pick_item);
      state |= cr_item_invoke_event(new_item, &event_copy,
                                    &pick_matrix, canvas->pick_item);
      }
    else
      {
      g_object_remove_weak_pointer(G_OBJECT(canvas->pick_item),
                                   (gpointer)&canvas->pick_item);
      canvas->pick_item = NULL;
      }
    }
  gdk_window_end_draw_frame(window, context);
  return state;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean button_event(GtkWidget *widget, GdkEventButton *event)
{
  CrCanvas *canvas;
  CrItem *root_item;
  gboolean state;
  cairo_matrix_t pick_matrix;
  canvas = CR_CANVAS(widget);
  root_item = CR_ITEM(canvas->root);
  state = FALSE;
  cairo_matrix_init_identity(&pick_matrix);
  if (canvas->pick_item)
    cr_item_find_child(root_item, &pick_matrix, canvas->pick_item);
  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
      canvas->pick_button = event->button;
      if (canvas->pick_item)
        {
        state = cr_item_invoke_event(canvas->pick_item,
                                     (GdkEvent *)event, &pick_matrix,
                                     canvas->pick_item);
        }
      break;
    case GDK_BUTTON_RELEASE:
      canvas->pick_button = 0;
      if (canvas->pick_item)
        {
        state = cr_item_invoke_event(canvas->pick_item,
                                     (GdkEvent*) event, &pick_matrix,
                                     canvas->pick_item);
        }
        break;
    default:
      syslog(LOG_ERR, "%s %d", __FILE__, __LINE__);
      break;
    }
  return state;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gboolean key_event(GtkWidget *widget, GdkEventKey *event)
{
  CrCanvas *canvas;
  CrItem *root_item;
  gboolean state;
  cairo_matrix_t pick_matrix;
  canvas = CR_CANVAS(widget);
  root_item = CR_ITEM(canvas->root);
  state = FALSE;
  if (canvas->pick_item)
    {
    cairo_matrix_init_identity(&pick_matrix);
    cr_item_find_child(root_item, &pick_matrix, canvas->pick_item);
    state = cr_item_invoke_event(canvas->pick_item,
                                 (GdkEvent*) event, &pick_matrix,
                                 canvas->pick_item);
    }
  return state;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gint focus_in(GtkWidget *widget, GdkEventFocus *event)
{
  gtk_widget_grab_focus(widget);
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
gint cr_glob_focus_out(GtkWidget *widget, GdkEventFocus *event)
{
  CrCanvas *canvas;
  CrItem *root_item;
  GdkEvent event_copy;
  cairo_matrix_t pick_matrix;
  canvas = CR_CANVAS(widget);
  root_item = CR_ITEM(canvas->root);
  gtk_widget_set_can_focus(widget, FALSE);
  if (canvas->pick_item)
    {
    cairo_matrix_init_identity(&pick_matrix);
    cr_item_find_child(root_item, &pick_matrix, canvas->pick_item);
    event_copy = *((GdkEvent *) event);
    event_copy.type = GDK_LEAVE_NOTIFY;
    cr_item_invoke_event(canvas->pick_item, &event_copy,
                         &pick_matrix, canvas->pick_item);
    g_object_remove_weak_pointer(G_OBJECT(canvas->pick_item),
                                 (gpointer)&canvas->pick_item);
    canvas->pick_item = NULL;
  }
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static gint enter_notify(GtkWidget *widget, GdkEventCrossing *event)
{
  gtk_widget_grab_focus(widget);
  return FALSE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cr_canvas_class_init(CrCanvasClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  object_class = (GObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  parent_class = g_type_class_peek_parent (klass);
  object_class->get_property = cr_canvas_get_property;
  object_class->set_property = cr_canvas_set_property;
  object_class->dispose = cr_canvas_dispose;
  object_class->finalize = cr_canvas_finalize;
  widget_class->draw = on_expose;
  klass->set_scroll_adjustments = set_scroll_adjustments;
  klass->set_root = set_root;
  klass->update = update;
  klass->quick_update = quick_update;
  widget_class->size_allocate = size_allocate;
  widget_class->realize = realize;
  widget_class->map = map;
  widget_class->button_press_event = button_event;
  widget_class->button_release_event = button_event;
  widget_class->motion_notify_event = motion_event;
  widget_class->key_press_event = key_event;
  widget_class->key_release_event = key_event;
  widget_class->focus_in_event = focus_in;
  widget_class->focus_out_event = cr_glob_focus_out;
  widget_class->enter_notify_event = enter_notify;
  cr_canvas_signals[SCROLL_REGION_CHANGED] =
  g_signal_new("scroll_region_changed", CR_TYPE_CANVAS, G_SIGNAL_RUN_FIRST,
               G_STRUCT_OFFSET(CrCanvasClass, scroll_region_changed),
               NULL, NULL, cr_marshal_VOID__VOID, G_TYPE_NONE, 0);
  cr_canvas_signals[BEFORE_PAINT] =
  g_signal_new("before_paint", CR_TYPE_CANVAS, G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET(CrCanvasClass, before_paint),
                  NULL, NULL, cr_marshal_VOID__BOXED_BOOLEAN,
                  G_TYPE_NONE, 2, CR_TYPE_CONTEXT, G_TYPE_BOOLEAN);
  g_object_class_install_property(object_class, PROP_HADJUSTMENT,
                                  g_param_spec_object("hadjustment", 
                                  "Horizontal adjustment",
                                  "The GtkAdjustment for the horizontal position",
                                  GTK_TYPE_ADJUSTMENT, G_PARAM_READWRITE));
  g_object_class_install_property(object_class, PROP_VADJUSTMENT,
                                  g_param_spec_object ("vadjustment", 
                                  "Vertical adjustment",
                                  "The GtkAdjustment for the vertical position",
                                  GTK_TYPE_ADJUSTMENT, G_PARAM_READWRITE));
  g_object_class_install_property(object_class, PROP_MAINTAIN_CENTER,
                                  g_param_spec_boolean("maintain_center",
                                  "Maintain Center",
                                  "World center point remains on resize.",
                                  FALSE, G_PARAM_READWRITE));
  g_object_class_install_property(object_class, PROP_MAINTAIN_ASPECT,
                                  g_param_spec_boolean("maintain_aspect",
                                  "Maintain Aspect",
                                  "Aspect ratio is maintained on resize and"
                                  "zooming events.", TRUE, G_PARAM_READWRITE));
  g_object_class_install_property(object_class, PROP_AUTO_SCALE,
                                  g_param_spec_boolean("auto_scale",
                                  "Auto Scale",
                                  "Present viewport contents are retained on"
                                  " resize. This means items will be zoomed"
                                  " in or out depending on how the window "
                                  "changes.", FALSE, G_PARAM_READWRITE));
  g_object_class_install_property(object_class, PROP_SHOW_LESS,
                                  g_param_spec_boolean("show_less",
                                  "Show Less",
                                  "Whether to show more area or less area on "
                                  "a viewport resize when the aspect ratio "
                                  "changes.  This only has an effect when both "
                                  "auto-scale and maintain-aspect are set.",
                                  FALSE, G_PARAM_READWRITE));
  g_object_class_install_property(object_class, PROP_REPAINT_MODE,
                                  g_param_spec_boolean("repaint_mode",
                                  "Repaint Mode",
                                  "Canvas repaints all items on each update.",
                                  FALSE, G_PARAM_READWRITE));
  g_object_class_install_property(object_class, PROP_ROOT,
                                  g_param_spec_object ("root", 
                                  "Root Canvas Item",
                                  "The root canvas item for this widget",
                                  CR_TYPE_ITEM, G_PARAM_READWRITE));
  g_object_class_install_property(object_class, PROP_PICK_ITEM,
                                  g_param_spec_object ("pick-item", 
                                  "Cursor Picked Item",
                                  "The lowest item in the tree presently "
                                  "receiving cursor events.",
                                  CR_TYPE_ITEM, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_REPAINT_ON_SCROLL,
                                  g_param_spec_boolean("repaint-on-scroll",
                                  "Repaint on Scroll",
                                  "Repaint the whole canvas when the scrollbar is moved."
                                  " Default behavior is to move the previously painted "
                                  "area,  This changes the default behavior to repaint "
                                  " the whole canvas instead. This is needed for the "
                                  "use case where something on the canvas will be changed"
                                  " immediately as a result of the scroll action.",
                                  FALSE, G_PARAM_READWRITE));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
GType cr_canvas_get_type(void)
{
  static GType type = 0;
  static const GTypeInfo info =
    {
    sizeof(CrCanvasClass),
    NULL,
    NULL,
    (GClassInitFunc) cr_canvas_class_init,
    (GClassFinalizeFunc) NULL,
    NULL,
    sizeof(CrCanvas),
    0,
    (GInstanceInitFunc) cr_canvas_init,
    NULL
    };
  if (!type)
    type = g_type_register_static(GTK_TYPE_WIDGET, "CrCanvas", &info, 0);
  return type;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void get_size(CrCanvas *canvas, double *width, double *height)
{
GtkAllocation *allocation = g_new0 (GtkAllocation, 1);
  gtk_widget_get_allocation(GTK_WIDGET(canvas), allocation);
  if ((allocation->width <= 1) || (allocation->height <= 1))
    {
    canvas->init_w = 100;
    canvas->init_h = 100;
    canvas->last_w = 100;
    canvas->last_h = 100;
    allocation->width = 100;
    allocation->height = 100;
    }
  *width = (double) allocation->width;
  *height = (double) allocation->height;
  g_free(allocation);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void limit_scale(CrCanvas *canvas, double *x_scale, double *y_scale)
{
  cairo_matrix_t matrix;
  CrItem *item;
  double current_x_scale, current_y_scale;
  item = CR_ITEM(canvas->root);
  matrix = *(cr_item_get_matrix(item));
  cairo_matrix_rotate(&matrix, -atan2(matrix.yx, matrix.yy));
  current_x_scale = current_y_scale = 1;
  cairo_matrix_transform_distance(&matrix, &current_x_scale, &current_y_scale);
  if (canvas->flags & CR_CANVAS_MAX_SCALE_FACTOR)
    {
    if (*x_scale * current_x_scale > canvas->max_x_scale_factor)
      *x_scale = canvas->max_x_scale_factor / current_x_scale;
    if (*y_scale * current_y_scale > canvas->max_y_scale_factor)
      *y_scale = canvas->max_y_scale_factor / current_y_scale;
    }
  if (canvas->flags & CR_CANVAS_MIN_SCALE_FACTOR)
    {
    if (*x_scale * current_x_scale < canvas->min_x_scale_factor)
      *x_scale = canvas->min_x_scale_factor / current_x_scale;
    if (*y_scale * current_y_scale < canvas->min_y_scale_factor)
      *y_scale = canvas->min_y_scale_factor / current_y_scale;
   }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void on_adjustment_value_changed(GtkAdjustment *adjustment,
                                        CrCanvas *canvas)
{
  CrItem *item;
  double dx, dy;
  int wx, wy;
  gdouble hadv_value, vadv_value;
  hadv_value = gtk_adjustment_get_value(canvas->hadjustment); 
  vadv_value = gtk_adjustment_get_value(canvas->vadjustment); 
  dx = wx = -(hadv_value - canvas->value_x);
  dy = wy = -(vadv_value - canvas->value_y);
  item = canvas->root;
  cairo_matrix_transform_distance(cr_item_get_inverse_matrix(item), &dx, &dy);
  canvas->value_x = gtk_adjustment_get_value(canvas->hadjustment);
  canvas->value_y = gtk_adjustment_get_value(canvas->vadjustment);
  if (dx != 0 || dy != 0)
    {
    cairo_matrix_translate(cr_item_get_matrix(item), dx, dy);
    *item->matrix_p = *item->matrix;
    canvas->flags |= CR_CANVAS_VIEWPORT_CHANGED;
    if (canvas->flags & CR_CANVAS_REPAINT_ON_SCROLL)
      cr_canvas_queue_repaint(canvas);
    else
      gdk_window_scroll(gtk_widget_get_window(GTK_WIDGET(canvas)), wx, wy);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void setup_adjustment(CrCanvas *canvas, GtkAdjustment *old,
                             GtkAdjustment *new)
{
  if (old)
    {
    gtk_adjustment_set_upper(new, gtk_adjustment_get_upper(old));
    gtk_adjustment_set_lower(new, gtk_adjustment_get_lower(old));
    gtk_adjustment_set_page_size(new, gtk_adjustment_get_page_size(old));
    gtk_adjustment_set_page_increment(new, gtk_adjustment_get_page_increment(old));
    gtk_adjustment_set_step_increment(new, gtk_adjustment_get_step_increment(old));
    gtk_adjustment_set_value(new, gtk_adjustment_get_value(old));
    g_signal_handlers_disconnect_by_func(old, on_adjustment_value_changed, canvas);
    g_object_unref(old);
    }
  g_object_ref (new);
  g_object_ref_sink (G_OBJECT(new));
  g_signal_connect (new, "value_changed", G_CALLBACK (on_adjustment_value_changed), canvas);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_set_hadjustment(CrCanvas *canvas, GtkAdjustment *adjustment)
{
  setup_adjustment(canvas, canvas->hadjustment, adjustment);
  canvas->value_x = gtk_adjustment_get_value(adjustment);
  canvas->hadjustment = adjustment;
  g_object_notify (G_OBJECT (canvas), "hadjustment");
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
void cr_canvas_set_vadjustment(CrCanvas *canvas, GtkAdjustment *adjustment)
{
  setup_adjustment(canvas, canvas->vadjustment, adjustment);
  canvas->value_y = gtk_adjustment_get_value(adjustment);
  canvas->vadjustment = adjustment;
  g_object_notify (G_OBJECT (canvas), "vadjustment");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_set_scroll_region(CrCanvas *canvas,
                                 gdouble scroll_x1, gdouble scroll_y1,
                                 gdouble scroll_x2, gdouble scroll_y2)
{
  double width, height;
  canvas->scroll_x1 = scroll_x1;
  canvas->scroll_y1 = scroll_y1;
  canvas->scroll_x2 = scroll_x2;
  canvas->scroll_y2 = scroll_y2;
  canvas->flags |= CR_CANVAS_SCROLL_WORLD;
  get_size(canvas, &width, &height);
  apply_min_scale_factor(canvas, width, height);
  update_adjustments(canvas);
  g_signal_emit(canvas, cr_canvas_signals[SCROLL_REGION_CHANGED], 0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_get_scroll_region(CrCanvas *canvas,
                                 gdouble *scroll_x1, gdouble *scroll_y1,
                                 gdouble *scroll_x2, gdouble *scroll_y2)
{
  CrItem *item;
  if (canvas->flags & CR_CANVAS_SCROLL_WORLD)
    {
    *scroll_x1 = canvas->scroll_x1;
    *scroll_y1 = canvas->scroll_y1;
    *scroll_x2 = canvas->scroll_x2;
    *scroll_y2 = canvas->scroll_y2;
    }
  else
    {
    *scroll_x1 = gtk_adjustment_get_lower(canvas->hadjustment) - 
                 gtk_adjustment_get_value(canvas->hadjustment);
    *scroll_y1 = gtk_adjustment_get_lower(canvas->vadjustment) - 
                 gtk_adjustment_get_value(canvas->vadjustment);
    *scroll_x2 = gtk_adjustment_get_upper(canvas->hadjustment) - 
                 gtk_adjustment_get_value(canvas->hadjustment);
    *scroll_y2 = gtk_adjustment_get_upper(canvas->vadjustment) - 
                 gtk_adjustment_get_value(canvas->vadjustment);
    item = CR_ITEM(canvas->root);
    cairo_matrix_transform_point(cr_item_get_inverse_matrix(item), scroll_x1, scroll_y1);
    cairo_matrix_transform_point(cr_item_get_inverse_matrix(item), scroll_x2, scroll_y2);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_set_scroll_factor(CrCanvas *canvas,
                                 double scroll_factor_x,
                                 double scroll_factor_y)
{
  canvas->scroll_factor_x = scroll_factor_x;
  canvas->scroll_factor_y = scroll_factor_y;
  canvas->flags &= ~CR_CANVAS_SCROLL_WORLD;
  update_adjustments(canvas);
  recenter_adjustments(canvas);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_scroll_to(CrCanvas *canvas, gdouble x, gdouble y)
{
  CrItem *item;
  double sx, sy, w, h, dx, dy;
  item = CR_ITEM(canvas->root);
  if (canvas->flags & CR_CANVAS_SCROLL_WORLD)
    {
    get_size(canvas, &w, &h);
    cairo_matrix_transform_distance(cr_item_get_inverse_matrix( item), &w, &h);
    x = CLAMP(x, canvas->scroll_x1, canvas->scroll_x2 - w);
    y = CLAMP(y, canvas->scroll_y1, canvas->scroll_y2 - h);
    }
  sx = sy = 0;
  cairo_matrix_transform_point(cr_item_get_inverse_matrix(item), &sx, &sy);
  dx = sx - x;
  dy = sy - y;
  cairo_matrix_translate(cr_item_get_matrix(item), dx, dy);
  cairo_matrix_transform_distance(cr_item_get_matrix(item), &dx, &dy);
  cr_item_request_update(item);
  canvas->flags |= CR_CANVAS_VIEWPORT_CHANGED;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_get_scroll_offsets(CrCanvas *canvas, gdouble *x, gdouble *y)
{
  CrItem *item;
  item = CR_ITEM(canvas->root);
  *x = *y = 0;
  cairo_matrix_transform_point(cr_item_get_inverse_matrix(item), x, y);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_center_on(CrCanvas *canvas, gdouble x, gdouble y)
{
  gdouble w, h,  old_x, old_y, half_w, half_h, dx, dy;
  CrItem *item;
  item = CR_ITEM(canvas->root);
  get_size(canvas, &w, &h);
  old_x = half_w = w / 2.;
  old_y = half_h = h / 2.;
  cairo_matrix_transform_point(cr_item_get_inverse_matrix(item), &old_x, &old_y);
  if (canvas->flags & CR_CANVAS_SCROLL_WORLD)
    {
    cairo_matrix_transform_distance(cr_item_get_inverse_matrix(item), &half_w, &half_h);
    x = CLAMP(x, canvas->scroll_x1 + half_w, canvas->scroll_x2 - half_w);
    y = CLAMP(y, canvas->scroll_y1 + half_h, canvas->scroll_y2 - half_h);
    }
  dx = old_x - x;
  dy = old_y - y;
  cairo_matrix_translate(cr_item_get_matrix(item), dx, dy);
  recenter_adjustments(canvas);
  cairo_matrix_transform_distance(cr_item_get_matrix(item), &dx, &dy);
  cr_item_request_update(item);
  canvas->flags |= CR_CANVAS_VIEWPORT_CHANGED;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_get_center(CrCanvas *canvas, gdouble *x, gdouble *y)
{
  CrItem *item;
  double w, h;
  item = CR_ITEM(canvas->root);
  get_size(canvas, &w, &h);
  *x = w / 2;
  *y = h / 2;
  cairo_matrix_transform_point(cr_item_get_inverse_matrix(item), x, y);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_center_scale(CrCanvas *canvas,
                            gdouble x, gdouble y,
                            gdouble w, gdouble h)
{
  gdouble width, height, cx, cy, sx, sy, z;
  CrItem *item;
  cairo_matrix_t *matrix, *matrix_i;
  item = CR_ITEM(canvas->root);
  matrix = cr_item_get_matrix(item);
  matrix_i = cr_item_get_inverse_matrix(item);
  get_size(canvas, &width, &height);
  cx = width/2;
  cy = height/2;
  z = 0;
  cairo_matrix_transform_distance(matrix_i, &width, &z);
  z = 0;
  cairo_matrix_transform_distance(matrix_i, &z, &height);
  sx = width/w;
  sy = height/h;
  if (canvas->flags & CR_CANVAS_MAINTAIN_ASPECT)
    {
    if (fabs(1. - sx) < fabs(1. - sy))
      sy = sx;
    else
      sx = sy;
    }
  limit_scale(canvas, &sx, &sy);
  cairo_matrix_scale(matrix, sx, sy);
  matrix_i = cr_item_get_inverse_matrix(item);
  cairo_matrix_transform_point(matrix_i, &cx, &cy);
  cairo_matrix_translate(matrix, cx - x, cy - y); 
  get_size(canvas, &canvas->init_w, &canvas->init_h);
  canvas->flags |= CR_CANVAS_VIEWPORT_CHANGED;
  cr_item_request_update(item);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_get_center_scale(CrCanvas *canvas,
                                gdouble *cx, gdouble *cy,
                                gdouble *w, gdouble *h)
{
  CrItem *item;
  cairo_matrix_t *matrix_i;
  get_size(canvas, w, h);
  *cx = *w/2;
  *cy = *h/2;
  item = CR_ITEM(canvas->root);
  matrix_i = cr_item_get_inverse_matrix(item);
  cairo_matrix_transform_distance(matrix_i, w, h);
  cairo_matrix_transform_point(matrix_i, cx, cy);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_set_viewport(CrCanvas *canvas,
                            gdouble x1, gdouble y1,
                            gdouble x2, gdouble y2)
{
  cr_canvas_center_scale(canvas, x1 + (x2 - x1) / 2, y1 + (y2 - y1) / 2, x2 - x1, y2 - y1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_get_viewport(CrCanvas *canvas,
                            gdouble *x1, gdouble *y1,
                            gdouble *x2, gdouble *y2)
{
  CrItem *item;
  cairo_matrix_t *matrix_i;
  item = CR_ITEM(canvas->root);
  matrix_i = cr_item_get_inverse_matrix(item);
  *x1 = *y1 = 0;
  get_size(canvas, x2, y2);
  cairo_matrix_transform_point(matrix_i, x1, y1);
  cairo_matrix_transform_point(matrix_i, x2, y2);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_zoom(CrCanvas *canvas, gdouble x_factor, gdouble y_factor)
{
  gdouble cx, cy, ncx, ncy;
  CrItem *item;
  cairo_matrix_t *matrix, *matrix_i;
  item = CR_ITEM(canvas->root);
  matrix = cr_item_get_matrix(item);
  matrix_i = cr_item_get_inverse_matrix(item);
  if (canvas->flags & CR_CANVAS_MAINTAIN_ASPECT)
    y_factor = x_factor;
  if (canvas->flags & CR_CANVAS_MAINTAIN_CENTER)
    {
    get_size(canvas, &cx, &cy);
    ncx = cx = cx/2;
    ncy = cy = cy/2;
    cairo_matrix_transform_point(matrix_i, &cx, &cy);
    }
  limit_scale(canvas, &x_factor, &y_factor);
  cairo_matrix_scale(matrix, x_factor, y_factor);
  matrix_i = cr_item_get_inverse_matrix(item);
  if (canvas->flags & CR_CANVAS_MAINTAIN_CENTER)
    {
    cairo_matrix_transform_point(matrix_i, &ncx, &ncy);
    cairo_matrix_translate(matrix, ncx - cx, ncy - cy);
    }
  canvas->flags |= CR_CANVAS_VIEWPORT_CHANGED;
  cr_item_request_update(item);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_zoom_world(CrCanvas *canvas)
{
  double x_scale, y_scale;
  CrItem *item;
  cairo_matrix_t *matrix;
  if (canvas->flags & CR_CANVAS_SCROLL_WORLD)
    cr_canvas_set_viewport(canvas, canvas->scroll_x1, canvas->scroll_y1,
                           canvas->scroll_x2, canvas->scroll_y2);
  else if (canvas->flags & CR_CANVAS_MIN_SCALE_FACTOR)
    {
    item = CR_ITEM(canvas->root);
    matrix = cr_item_get_matrix(item);
    x_scale = y_scale = 1;
    cairo_matrix_transform_distance(matrix, &x_scale, &y_scale);
    x_scale = canvas->min_x_scale_factor / x_scale;
    y_scale = canvas->min_y_scale_factor / x_scale;
    cr_canvas_zoom(canvas, x_scale, y_scale);
    }
  else
    cr_canvas_zoom(canvas, 1/canvas->scroll_factor_x, 1/canvas->scroll_factor_y);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_set_max_scale_factor(CrCanvas *canvas, double x_factor, double y_factor)
{
  if ((canvas->flags & CR_CANVAS_MAINTAIN_ASPECT) &&
      (x_factor != y_factor))
    g_warning("Setting unequal max scale factor for maintain aspect canvas.");
  canvas->flags |= CR_CANVAS_MAX_SCALE_FACTOR;
  canvas->max_x_scale_factor = x_factor;
  canvas->max_y_scale_factor = y_factor;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_set_min_scale_factor(CrCanvas *canvas, double x_factor, double y_factor)
{
  if ((canvas->flags & CR_CANVAS_MAINTAIN_ASPECT) &&
      (x_factor != y_factor))
    g_warning("Setting unequal min scale factor for maintain aspect canvas.");
  canvas->flags |= CR_CANVAS_MIN_SCALE_FACTOR;
  canvas->min_x_scale_factor = x_factor;
  canvas->min_y_scale_factor = y_factor;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_set_repaint_mode(CrCanvas *canvas, gboolean on)
{
  if (on)
    canvas->flags |= CR_CANVAS_REPAINT_MODE;
  else
    canvas->flags &= ~CR_CANVAS_REPAINT_MODE;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_canvas_queue_repaint(CrCanvas *canvas)
{
  if (!(canvas->flags & CR_CANVAS_REPAINT_MODE))
    canvas->flags |= (CR_CANVAS_REPAINT_MODE | CR_CANVAS_REPAINT_REVERT);
  if (canvas->update_idle_id)
    canvas->update_idle_id = 0;
  canvas->update_idle_id = g_idle_add_full(GDK_PRIORITY_REDRAW - 20,
                                           (GSourceFunc) on_repaint_revert_idle,
                                           canvas, NULL);
  gtk_widget_queue_draw(GTK_WIDGET(canvas));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
GtkWidget * cr_canvas_new(const gchar *first_arg_name, ...)
{
  GtkWidget *canvas;
  va_list args;
  va_start (args, first_arg_name);
  canvas=GTK_WIDGET(g_object_new_valist(CR_TYPE_CANVAS,first_arg_name,args));
  va_end (args);
  return canvas;
}
/*--------------------------------------------------------------------------*/

