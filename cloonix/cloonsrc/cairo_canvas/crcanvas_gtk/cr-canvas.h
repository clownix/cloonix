/* cr-canvas.h 
 * Copyright (C) 2006 Robert Gibbs <bgibbs@users.sourceforge.net> 
 */

#ifndef _CR_CANVAS_H_
#define _CR_CANVAS_H_
 
#include <gtk/gtk.h>
#include "cr-item.h"

G_BEGIN_DECLS

#define CR_TYPE_CANVAS  (cr_canvas_get_type())

#define CR_CANVAS(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        CR_TYPE_CANVAS, CrCanvas))

#define CR_CANVAS_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST ((klass), \
        CR_TYPE_CANVAS, CrCanvasClass))

#define CR_IS_CANVAS(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CR_TYPE_CANVAS))

#define CR_IS_CANVAS_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), CR_TYPE_CANVAS))

#define CR_CANVAS_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), \
        CR_TYPE_CANVAS, CrCanvasClass))

typedef struct _CrCanvas CrCanvas;
typedef struct _CrCanvasClass CrCanvasClass;

struct _CrCanvas
{
        GtkWidget parent;
        GtkAdjustment *hadjustment, *vadjustment;
        CrItem *root;
        CrItem *pick_item;
        gint32 pick_button;
        gdouble scroll_x1, scroll_y1, scroll_x2, scroll_y2;
        gdouble scroll_factor_x, scroll_factor_y;
        gdouble init_w, init_h, last_w, last_h, value_x, value_y;
        gdouble min_x_scale_factor, min_y_scale_factor; 
        gdouble max_x_scale_factor, max_y_scale_factor;
        gint update_idle_id;
        guint32 flags;
};

struct _CrCanvasClass
{
  GtkWidgetClass parent_class;
  void (*set_scroll_adjustments)(CrCanvas *layout,
                                 GtkAdjustment  *hadjustment,
                                 GtkAdjustment  *vadjustment);
  gboolean (*canvas_event) (CrCanvas *canvas, GdkEvent *event);
  void (*set_root) (CrCanvas *canvas, CrItem *item);
  void (*update) (CrCanvas *canvas);
  void (*quick_update) (CrCanvas *canvas);
  void (*scroll_region_changed)(CrCanvas *canvas);
  void (*before_paint)(CrCanvas *canvas, cairo_t *cr, gboolean viewport_changed); 
};

void cr_canvas_set_vadjustment(CrCanvas *canvas, GtkAdjustment *adjustment);
void cr_canvas_set_hadjustment(CrCanvas *canvas, GtkAdjustment *adjustment);
void cr_canvas_set_scroll_region(CrCanvas *canvas, gdouble scroll_x1,
                gdouble scroll_y1, gdouble scroll_x2, gdouble scroll_y2);
void cr_canvas_get_scroll_region(CrCanvas *canvas, gdouble *scroll_x1,
                gdouble *scroll_y1, gdouble *scroll_x2, gdouble *scroll_y2);
void cr_canvas_set_scroll_factor(CrCanvas *canvas, double scroll_factor_x, double scroll_factor_y);
void cr_canvas_scroll_to(CrCanvas *canvas, gdouble x, gdouble y);
void cr_canvas_get_scroll_offsets(CrCanvas *canvas, gdouble *x, gdouble *y);
void cr_canvas_center_on(CrCanvas *canvas, gdouble x, gdouble y);
void cr_canvas_get_center(CrCanvas *canvas, gdouble *x, gdouble *y);
void cr_canvas_center_scale(CrCanvas *canvas, gdouble x, gdouble y, gdouble w, gdouble h);
void cr_canvas_get_center_scale(CrCanvas *canvas, gdouble *cx, gdouble *cy, gdouble *w, gdouble *h);
void cr_canvas_set_viewport(CrCanvas *canvas, gdouble x1, gdouble y1, gdouble x2, gdouble y2);
void cr_canvas_get_viewport(CrCanvas *canvas, gdouble *x1, gdouble *y1, gdouble *x2, gdouble *y2);
void cr_canvas_zoom(CrCanvas *canvas, gdouble x_factor, gdouble y_factor);
void cr_canvas_zoom_world(CrCanvas *canvas);
void cr_canvas_set_max_scale_factor(CrCanvas *canvas, double x_factor, double y_factor);
void cr_canvas_set_min_scale_factor(CrCanvas *canvas, double x_factor, double y_factor);
void cr_canvas_set_repaint_mode(CrCanvas *canvas, gboolean on);
void cr_canvas_queue_repaint(CrCanvas *canvas);
GType cr_canvas_get_type(void);
GtkWidget *cr_canvas_new(const gchar *first_arg_name, ...);
gint cr_glob_focus_out(GtkWidget *widget, GdkEventFocus *event);

enum {
  CR_CANVAS_NEED_UPDATE = 1 << 0,
  CR_CANVAS_IN_EXPOSE = 1 << 1,
  CR_CANVAS_SCROLL_WORLD = 1 << 2,
  CR_CANVAS_MAINTAIN_CENTER = 1 << 3,
  CR_CANVAS_MAINTAIN_ASPECT = 1 << 4,
  CR_CANVAS_AUTO_SCALE = 1 << 5,
  CR_CANVAS_SHOW_LESS = 1 << 6,
  CR_CANVAS_MAX_SCALE_FACTOR = 1 << 7,
  CR_CANVAS_MIN_SCALE_FACTOR = 1 << 8,
  CR_CANVAS_REPAINT_MODE = 1 << 9,
  CR_CANVAS_REPAINT_REVERT = 1 << 10,
  CR_CANVAS_IGNORE_INVALIDATE = 1 << 11,
  CR_CANVAS_VIEWPORT_CHANGED = 1 << 12,
  CR_CANVAS_REPAINT_ON_SCROLL = 1 << 13,
  CR_CANVAS_INSIDE_UPDATE = 1 << 14,
};

G_END_DECLS

#endif
