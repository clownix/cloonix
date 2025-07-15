/* cr-item.h 
 * Copyright (C) 2006 Robert Gibbs <bgibbs@users.sourceforge.net> 
 */
#ifndef _CR_ITEM_H_
#define _CR_ITEM_H_
 
#include <gtk/gtk.h>
#include <cairo.h>
#include "cr-types.h"

G_BEGIN_DECLS

#define CR_TYPE_ITEM  (cr_item_get_type())

#define CR_ITEM(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        CR_TYPE_ITEM, CrItem))

#define CR_ITEM_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST ((klass), \
        CR_TYPE_ITEM, CrItemClass))

#define CR_IS_ITEM(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CR_TYPE_ITEM))

#define CR_IS_ITEM_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), CR_TYPE_ITEM))

#define CR_ITEM_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), \
        CR_TYPE_ITEM, CrItemClass))

typedef struct _CrItem CrItem;
typedef struct _CrItemClass CrItemClass;

struct _CrItem
{
  GObject parent;
  GList *items;
  CrBounds *bounds;
  CrDeviceBounds *device;
  cairo_matrix_t *matrix, *matrix_i, *matrix_p;
  guint32 flags;
};

struct _CrItemClass
{
  GObjectClass parent_class;
  void (*paint)(CrItem *item, cairo_t *c);
  gboolean (*calculate_bounds)(CrItem *item, cairo_t *c, CrBounds *bounds, CrDeviceBounds *device);
  CrItem* (*test)(CrItem *item, cairo_t *c, double x, double y);
  gboolean (*event)(CrItem *item, GdkEvent *event,cairo_matrix_t *matrix, CrItem *pick_item);
  void (*add)(CrItem *item, CrItem *child);
  void (*added)(CrItem *item, CrItem *child);
  void (*remove)(CrItem *item, CrItem *child);
  void (*removed)(CrItem *item, CrItem *child);
  void (*invoke_paint)(CrItem *, cairo_t *c, gboolean all, double x1, double y1, double x2, double y2);
  void (*report_old_bounds)(CrItem *, cairo_t *c, gboolean all);
  void (*report_new_bounds)(CrItem *, cairo_t *c, gboolean all);
  void (*invalidate)(CrItem *item, int mask, gboolean all, double x1, double y1, double x2, double y2, CrDeviceBounds *device);
  CrItem* (*invoke_test)(CrItem *, cairo_t *c, double x, double y);
  void (*invoke_calculate_bounds)(CrItem *item, cairo_t *c);
  void (*request_update)(CrItem *);
};

void cr_item_invoke_paint(CrItem *item, cairo_t *ct, gboolean all, double x1, double y1, double x2, double y2);
CrItem *cr_item_invoke_test(CrItem *item, cairo_t *c, double x, double y);
void cr_item_report_old_bounds(CrItem *item, cairo_t *ct, gboolean all);
void cr_item_report_new_bounds(CrItem *item, cairo_t *ct, gboolean all);
gboolean cr_item_invoke_event(CrItem *item, GdkEvent *event, cairo_matrix_t *matrix, CrItem *pick_item);
gboolean cr_item_find_child(CrItem *item, cairo_matrix_t *matrix, CrItem *child);
gboolean cr_item_get_bounds(CrItem *item, double *x1, double *y1, double *x2, double *y2);
gboolean cr_item_get_device_bounds(CrItem *item, CrDeviceBounds *device);
void cr_item_request_update(CrItem *item);
cairo_matrix_t *cr_item_get_matrix(CrItem *item);
cairo_matrix_t *cr_item_get_inverse_matrix(CrItem *item);
void cr_item_cancel_matrix(CrItem *item);
void cr_item_add(CrItem *item, CrItem *child);
void cr_item_remove(CrItem *item, CrItem *child);
void cr_item_hide(CrItem *item);
void cr_item_show(CrItem *item);
void cr_item_move(CrItem *item, double dx, double dy);
void cr_item_set_z_relative(CrItem *item, CrItem *child, int positions);
void cr_item_set_z(CrItem *item, CrItem *child, int position);
CrContext *cr_item_make_temp_cairo(void);
gboolean cr_item_calculate_bounds(CrItem *item, CrBounds *bounds, CrDeviceBounds *device_bounds);
CrItem *cr_item_construct(CrItem *parent, GType type, const gchar *first_arg_name, va_list args);
CrItem *cr_item_new(CrItem *parent, GType type, const gchar *first_arg_name, ...);
GType cr_item_get_type(void);

enum {
        CR_ITEM_VISIBLE = 1 << 0,
        CR_ITEM_NEED_UPDATE = 1 << 1,
        CR_ITEM_AVOID_TEST = 1 << 2,
        CR_ITEM_TEST_RECTANGLE = 1 << 3,
        CR_ITEM_IN_REPORT_BOUNDS = 1 << 4,
        CR_ITEM_INVALIDATE_UPDATE = 1 << 5,
        CR_ITEM_INVALIDATE_OLD = 1 << 6,
        CR_ITEM_INVALIDATE_NEW = 1 << 7
};

G_END_DECLS

#endif
