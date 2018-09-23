/* cr-item.h 
 * Copyright (C) 2006 Robert Gibbs <bgibbs@users.sourceforge.net> 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
        /*< public >*/
        /*
         * The list of child items may be accessed for reading directly.
         * It should only be modified via the cr_item_add/remove methods.
         */
        GList *items;
        CrBounds *bounds;
        CrDeviceBounds *device;
        cairo_matrix_t *matrix, *matrix_i, *matrix_p;
        guint32 flags;
};

struct _CrItemClass
{
        GObjectClass parent_class;

        /* START:
         * Item implementations should override or connect to the following
         * signals or methods: */

        /**
         * CrItem::paint:
         * @item:
         * @c: The cairo context used to draw the item.
         *
         * Item implementations should override this and use it to draw to 
         * the cairo surface.
         */
        void (*paint)(CrItem *item, cairo_t *c);

        /** 
         * CrItem::calculate-bounds:
         * @item:
         * @c: Cairo context with transformation set to match the current item.
         * @bounds: Bounding box in item coordinates.
         * @device: A structure for specifying device coordinate
         * boundaries (if any)
         *
         * Defining this method is optional but strongly recommended.  If it is
         * not provided, then the device (canvas widget) must be run in repaint
         * mode or else this item won't be painted. In most cases the
         * #device can be ignored.  If some portion of the item is tied
         * to device space then the anchor should be filled to reflect what part
         * of the item bounding box the device bounding box should be measured
         * from.  An anchor to a corner means the device space is measured from
         * that corner.  An anchor to a center means the device space is
         * measured to either side of the item unit bounding box.
         *
         * Returns: %TRUE if the the bounds are defined even if the are 0, 0, 0,
         * 0.  Any implementation of this method should always return %TRUE.
         */
        gboolean (*calculate_bounds)(CrItem *item, cairo_t *c,
                        CrBounds *bounds, CrDeviceBounds *device);
        /** 
         * CrItem::test:
         * @item:
         * @c: Cairo context with tranformation set to match the current item.
         * @x: The coordinate in the item's user space.
         * @y: The coordinate in the item's user space.
         *
         * This signal is optional.  If you don't need interaction
         * with the pointing device, then there is no need to provide it.
         */
        CrItem* (*test)(CrItem *item, cairo_t *c, double x, double y);

        /**
         * CrItem::event:
         * @item:
         * @event: Gdk Event structure with coordinates in item units.
         * @matrix: Use this only if you want to get back to device coordinates.
         * @pick_item: Actual item the cursor is on.  The can sometimes be
         * different from item if the event is caught on a non-leaf node.
         *
         * In most cases applications will connect to this signal. 
         * This method need only be defined if your canvas item has some type of
         * pointer behavior.  If test is not defined, this method will never be
         * called.
         *
         * Since release 0.11, the coordinates returned in the event structure
         * represent the current user coordinates for the item being signaled.
         * The typical way to use the event signal is to connect to the item
         * which is to be manipulated.  During the button press save initial
         * event coordinates.  During the motion event manipulate the item by
         * comparing the event structure coordinates against the saved off
         * initial coordinates. 
         *
         * Returns: %TRUE to stop the event.  %FALSE to allow the event to
         * propagate to other items higher in the tree.
         */
        gboolean (*event)(CrItem *item, GdkEvent *event,cairo_matrix_t *matrix,
                        CrItem *pick_item);
        
        /* END: Custom item methods. 
         */


        /* These remaining methods and signals are primarily used for
         * interaction between items and devices.  There is normally no need to
         * override or connect to them when creating custom canvas items.
         */
        void (*add)(CrItem *item, CrItem *child);
        void (*added)(CrItem *item, CrItem *child);
        void (*remove)(CrItem *item, CrItem *child);
        void (*removed)(CrItem *item, CrItem *child);
        void (*invoke_paint)(CrItem *, cairo_t *c, gboolean all,
                        double x1, double y1, double x2, double y2);
        void (*report_old_bounds)(CrItem *, cairo_t *c, gboolean all);
        void (*report_new_bounds)(CrItem *, cairo_t *c, gboolean all);
        void (*invalidate)(CrItem *item, int mask, gboolean all,
                        double x1, double y1, double x2, double y2,
                        CrDeviceBounds *device);
        CrItem* (*invoke_test)(CrItem *, cairo_t *c, double x, double y);
        void (*invoke_calculate_bounds)(CrItem *item, cairo_t *c);


        void (*request_update)(CrItem *);
};

/* These functions are used for interaction between items and devices. */
void cr_item_invoke_paint(CrItem *item, cairo_t *ct, gboolean all,
                double x1, double y1, double x2, double y2);
CrItem *cr_item_invoke_test(CrItem *item, cairo_t *c, double x, double y);
void cr_item_report_old_bounds(CrItem *item, cairo_t *ct, gboolean all);
void cr_item_report_new_bounds(CrItem *item, cairo_t *ct, gboolean all);
gboolean cr_item_invoke_event(CrItem *item, GdkEvent *event, 
                cairo_matrix_t *matrix, CrItem *pick_item);
gboolean cr_item_find_child(CrItem *item, cairo_matrix_t *matrix, 
                CrItem *child);


/* These functions are for general use: */
gboolean cr_item_get_bounds(CrItem *item, double *x1, double *y1, double *x2, 
                double *y2);
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

gboolean cr_item_calculate_bounds(CrItem *item, CrBounds *bounds, 
                CrDeviceBounds *device_bounds);

CrItem *cr_item_construct(CrItem *parent, GType type, 
                const gchar *first_arg_name, va_list args);
CrItem *cr_item_new(CrItem *parent, GType type, 
                const gchar *first_arg_name, ...);

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

#endif /* _CR_ITEM_H_ */
