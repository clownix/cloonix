/* cr-item.c 
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

#include <string.h>
#include <time.h>
#include "cr-marshal.h"
#include "cr-item.h"

#define DEBUG_PROFILE 0

/**
 * SECTION:cr-item
 * @title: CrItem
 * @short_description: The base class for all canvas items and groups.
 *
 * The base class for all canvas items. This class can be used as a painting
 * object, or as a grouping object, or as both.
 *
 * To use this as a painting object, connect to or redefine the #CrItem::paint
 * and #CrItem::calculate-bounds signals.
 *
 * To use it as a grouping object, call #cr_item_add and #cr_item_remove.  To
 * move the group around, make changes to its matrix (#cr_item_get_matrix) and
 * call #cr_item_request_update.
 */


enum {
        ARG_0,
        PROP_X,
        PROP_Y,
        PROP_VISIBLE,
        PROP_AVOID_TEST,
        PROP_TEST_RECTANGLE,
        PROP_MATRIX
};

enum {
        ADDED,
        REMOVED,
        PAINT,
        TEST,
        REQUEST_UPDATE,
        EVENT,
        CALCULATE_BOUNDS,
        INVALIDATE,
        LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

#if DEBUG_PROFILE
static int num_paints = 0;
static clock_t paint_ticks = 0;
static int num_updates = 0;
static clock_t update_ticks = 0;

void
cr_item_print_stats(void)
{
        g_print("CrItem %d updates, in %lf secs.\n", num_updates, 
                        (double) update_ticks / CLOCKS_PER_SEC);
        g_print("CrItem %d paints, in %lf secs.\n", num_paints, 
                        (double) paint_ticks / CLOCKS_PER_SEC);
        num_paints = num_updates = 0;
        update_ticks = paint_ticks = 0;
}
#endif

static void
create_matrix(CrItem *item)
{
        if (item->matrix) return;
        item->matrix = g_new(cairo_matrix_t, 1);
        item->matrix_i = g_new(cairo_matrix_t, 1);
        item->matrix_p = g_new(cairo_matrix_t, 1);
        cairo_matrix_init_identity(item->matrix);
        cairo_matrix_init_identity(item->matrix_i);
        cairo_matrix_init_identity(item->matrix_p);
}

static void
destroy_matrix(CrItem *item)
{
        if (item->matrix) g_free(item->matrix);
        if (item->matrix_i) g_free(item->matrix_i);
        if (item->matrix_p) g_free(item->matrix_p);
        item->matrix = item->matrix_i = item->matrix_p = NULL;
}

static void
inverse_matrix(CrItem *item)
{
        *item->matrix_i = *item->matrix;
        cairo_matrix_invert(item->matrix_i);
}

/**
 * CrItem::invalidate:
 * @item:
 * @mask:  Some combination of INVALIDATE_OLD, INVALIDATE_NEW, and 
 * INVALIDATE_UPDATE
 * @x1: Leftmost bound.
 * @y1: Rightmost bound.
 * @x2: Rightmost bound.
 * @y2: Bottommost bound.
 * @device: A structure for specifying device coordinate boundaries (if any).
 *
 * This is an internal method. The invalidate signal is propagated up
 * through the hierarchy so that multiple devices (canvases) may
 * receive this signal in their respective coordinate systems.  The anchor and
 * device width and heights (if any) must be added when the invalidate signal
 * reaches its final destination.
 */
static void
invalidated(CrItem *item, int mask,
                double x1, double y1, double x2, double y2, 
                CrDeviceBounds *device)
{
        cairo_matrix_t *matrix;
        double ax1, ay1, ax2, ay2;
        double bx1, by1, bx2, by2;

        /* INVALIDATE_OLD is the reporting of old bounds. */
        /* INVALIDATE_NEW is the reporting of new bounds. */
        /* IN_REPORT_BOUNDS this parent is waiting for an invalidate. */
        /* INVALIDATE_UPDATE is reporting because some child has changed. */

        if (item->flags & CR_ITEM_NEED_UPDATE)
                mask |= CR_ITEM_INVALIDATE_UPDATE;

        /* The signal stops if it got here as a result of a request from a
         * different parent and no item lower on the tree has changed. */
        if (!(mask & CR_ITEM_INVALIDATE_UPDATE) && 
                        !(item->flags & CR_ITEM_IN_REPORT_BOUNDS)) return;

        /* matrix_p is the previously used matrix.  It is important to keep this
         * around so the device can find out where it previously painted the 
         * item.  This allows the current matrix to be changed at any time
         * without breaking the invalidation scheme for the device.
         */

        /* The first time matrix_p is identity, but that's OK because 
         * no invalidations will be sent. */

        if (item->matrix) {
                matrix = mask & CR_ITEM_INVALIDATE_NEW ? item->matrix : 
                        item->matrix_p;

                /* This factors-out any rotations by determining the largest
                 * enclosing rectangle afterwards. */

                ax1 = bx1 = x1; ay1 = by1 = y1; ax2 = bx2 = x2; ay2 = by2 = y2;
                cairo_matrix_transform_point(matrix, &ax1, &ay1);
                cairo_matrix_transform_point(matrix, &bx2, &by1);
                cairo_matrix_transform_point(matrix, &ax2, &ay2);
                cairo_matrix_transform_point(matrix, &bx1, &by2);
                x1 = MIN(MIN(MIN(ax1, ax2), bx1), bx2);
                y1 = MIN(MIN(MIN(ay1, ay2), by1), by2);
                x2 = MAX(MAX(MAX(ax1, ax2), bx1), bx2);
                y2 = MAX(MAX(MAX(ay1, ay2), by1), by2);

#if 0
                /* For now device extents are exactly as specified, but maybe it
                 * would be better to ignore the scale but pay attention to the
                 * rotation.  The code below figures out a new set of bounds
                 * for a rectangle that is rotated by angle assuming the anchor
                 * point does not change. */
                angle = atan2(matrix.yx, matrix.yy);

                x1 = - h * sin(angle);
                x2 = w * cos(angle);
                y1 = w * sin(angle);
                y2 = h * cos(angle);

                device->x1 = (x1 < 0 ? x1 : 0) + (x2 < 0 ? x2 : 0);
                device->y1 = (y1 < 0 ? y1 : 0) + (y2 < 0 ? y2 : 0);
                device->x2 = (x1 > 0 ? x1 : 0) + (x2 > 0 ? x2 : 0);
                device->y2 = (y1 > 0 ? y1 : 0) + (y2 > 0 ? y2 : 0);
#endif

        }

        g_signal_emit(item, signals[INVALIDATE], 0, mask, x1, y1, x2, y2,
                        device);
}

static void
dispose_child(CrItem *child, CrItem *item)
{
        cr_item_remove(item, child);
}

static void
cr_item_dispose(GObject *object)
{
        CrItem *item;

        item = CR_ITEM(object);

        g_list_foreach(item->items, (GFunc) dispose_child, object);
        g_list_free(item->items);

        parent_class->dispose(object);
}

static void
cr_item_finalize(GObject *object)
{
        CrItem *item;

        item = CR_ITEM(object);

        if (item->bounds) cr_bounds_unref(item->bounds);
        item->bounds = NULL;

        if (item->device) cr_device_bounds_unref(item->device);
        item->device = NULL;

        destroy_matrix(item);
        parent_class->finalize(object);
}

static void
cr_item_set_property(GObject *object, guint property_id,
                const GValue *value, GParamSpec *pspec)
{
        CrItem *item = (CrItem*) object;
        double val;

        switch (property_id) {
                case PROP_X:
                        val = g_value_get_double(value);
                        if (!item->matrix || val != item->matrix->x0) {
                                cr_item_get_matrix(item)->x0 = val;
                                cr_item_request_update(item);
                        }
                        break;
                case PROP_Y:
                        val = g_value_get_double(value);
                        if (!item->matrix || val != item->matrix->y0) {
                                cr_item_get_matrix(item)->y0 = val;
                                cr_item_request_update(item);
                        }
                        break;
                case PROP_VISIBLE:
                        if (g_value_get_boolean(value))
                                cr_item_show(item);
                        else
                                cr_item_hide(item);
                        break;
                case PROP_AVOID_TEST:
                        if (g_value_get_boolean(value))
                                item->flags |= CR_ITEM_AVOID_TEST;
                        else
                                item->flags &= ~CR_ITEM_AVOID_TEST;
                        break;
                case PROP_TEST_RECTANGLE:
                        if (g_value_get_boolean(value))
                                item->flags |= CR_ITEM_TEST_RECTANGLE;
                        else
                                item->flags &= ~CR_ITEM_TEST_RECTANGLE;
                        break;
                case PROP_MATRIX:
                        create_matrix(item);
                        *item->matrix = *((cairo_matrix_t *)
                                        g_value_get_boxed(value));
                        cr_item_request_update(item);
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                property_id, pspec);
        }
}

static void
cr_item_get_property(GObject *object, guint property_id,
                GValue *value, GParamSpec *pspec)
{
        CrItem *item = (CrItem*) object;
        switch (property_id) {
                case PROP_X:
                        g_value_set_double (value, item->matrix ?
                                        item->matrix->x0 : 0);
                        break;
                case PROP_Y:
                        g_value_set_double (value, item->matrix ?
                                        item->matrix->y0 : 0);
                        break;
                case PROP_VISIBLE:
                        g_value_set_boolean(value, item->flags & 
                                        CR_ITEM_VISIBLE);
                        break;
                case PROP_AVOID_TEST:
                        g_value_set_boolean(value, item->flags & 
                                        CR_ITEM_AVOID_TEST);
                        break;
                case PROP_TEST_RECTANGLE:
                        g_value_set_boolean(value, 
                                item->flags & CR_ITEM_TEST_RECTANGLE);
                        break;
                case PROP_MATRIX:
                        g_value_set_boxed(value, item->matrix);
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                property_id, pspec);
        }
}

static void
cr_item_init(CrItem *item)
{
        item->flags |= (CR_ITEM_NEED_UPDATE | CR_ITEM_VISIBLE);
}

static inline gboolean
get_device_bounds(CrItem *item, cairo_t *c, 
                double *x1, double *y1, double *x2, double *y2)
{
        double bx1, by1, bx2, by2;

        if (!item->device) return FALSE;

        bx1 = item->bounds->x1;
        by1 = item->bounds->y1;
        bx2 = item->bounds->x2;
        by2 = item->bounds->y2;

        cairo_user_to_device(c, &bx1, &by1);
        cairo_user_to_device(c, &bx2, &by2);

                        *x1 = bx1 + item->device->x1;
                        *x2 = bx1 + item->device->x2;

                        *y1 = by1 + item->device->y1;
                        *y2 = by1 + item->device->y2;
        return TRUE;
}

static inline gboolean
in_bounds(CrItem *item, cairo_t *c, double x, double y)
{
        double x1, y1, x2, y2, bx1, by1, bx2, by2;

        if (!item->bounds) return FALSE;

        bx1 = item->bounds->x1;
        by1 = item->bounds->y1;
        bx2 = item->bounds->x2;
        by2 = item->bounds->y2;

        if (x < bx1 || x > bx2 || y < by1 || y > by2 ) {

                if (get_device_bounds(item, c, &x1, &y1, &x2, &y2)) {
                        cairo_user_to_device(c, &x, &y);

                        return !(x < x1 || x > x2 || y < y1 || y > y2 );
                }
                return FALSE;
        }
        return TRUE;
}

/**
 * cr_item_find_child:
 * @item:
 * @matrix: A cairo_matrix_t that should be set to identity at the start of 
 * the search.
 * @child: The item to be found.  
 *
 * An Internal method.  This will search the item tree for a child item that 
 * was previously found using invoke_test.  When the item is found the 
 * cairo matrix will be set to reflect the transformation between
 * this item and the found item.
 *
 * Returns: TRUE if the item was found.
 */
gboolean
cr_item_find_child(CrItem *item, cairo_matrix_t *matrix, CrItem *child)
{
        CrItem *i;
        GList *list;
        gboolean found;

        if (item->matrix)
                cairo_matrix_multiply(matrix, item->matrix, matrix);

        if (item == child) found = TRUE;
        else {

                for (found = FALSE, list = g_list_last(item->items); 
                                !found && list; list = list->prev) {
                        i = list->data;
                        found = cr_item_find_child(i, matrix, child);
                }
        }

                
        if (!found && item->matrix) {
                inverse_matrix(item);
                cairo_matrix_multiply(matrix, item->matrix_i, matrix);
        }

        return found;
}

/**
 * CrItem::invoke_test:
 * @item:
 * @c: The cairo context with its transformation matrix set to reflect all
 * transformations higher in the item tree.
 * @x: The coordinate before applying the item's transformation matrix
 * @y: The coordinate before applying the item's transformation matrix
 *
 * An Internal method.  If an item is found the matrix will be 
 * modified to reflect the multiplication of all matrices from the 
 * root to the found item. 
 */
static CrItem *
invoke_test(CrItem *item, cairo_t *ct, double x, double y)
{
        CrItem *i;
        GList *list;
        CrItem *found;

        if (!(item->flags & CR_ITEM_VISIBLE)) return NULL;
        if (item->flags & CR_ITEM_AVOID_TEST) return NULL;

        if (item->matrix)
                cairo_transform(ct, item->matrix);

        for (found = NULL, list = g_list_last(item->items); 
                        !found && list; list = list->prev) {
                i = list->data;
                found = cr_item_invoke_test(i, ct, x, y);
        }

        if (!found) {
                cairo_device_to_user(ct, &x, &y);
                if (!item->bounds || in_bounds(item, ct, x, y)) {

                        if (item->flags & CR_ITEM_TEST_RECTANGLE) found = item;
                        else {
                                g_signal_emit(item, signals[TEST], 0, 
                                                ct, x, y, &found);
                        }
                }
        }

        if (!found && item->matrix) {
                inverse_matrix(item);
                cairo_transform(ct, item->matrix_i);
        }

        return found;
}

void set_event_coords(GdkEvent *event, double x, double y)
{
        if (event->type == GDK_ENTER_NOTIFY || event->type ==
                        GDK_LEAVE_NOTIFY) {
                event->crossing.x = x;
                event->crossing.y = y;
        }
        else {
                event->motion.x = x;
                event->motion.y = y;
        }
}

static gboolean
item_event(CrItem *item, GdkEvent *event, cairo_matrix_t *matrix, 
                CrItem *pick_item)
{
        gboolean found;

        found = FALSE;

        g_signal_emit(item, signals[EVENT], 0, event, matrix, pick_item, 
                        &found);

        return found;
}

static gboolean
invoke_event(CrItem *item, GdkEvent *event, cairo_matrix_t *matrix,
                CrItem *pick_item)
{
        GdkEvent event_copy;
        cairo_matrix_t matrix_copy;
        double x, y;

        /* The structs are copied because they will be modified.*/
        event_copy = *event;
        matrix_copy = *matrix;

        /* The event chain is started up in device coordinates with this
         * one-time call.  It is converted to item coordinates here.
         * As the 'event' signal gets propagated up the item tree item
         * transformation components are removed. */
        if (gdk_event_get_coords(&event_copy, &x, &y)) {
                cairo_matrix_invert(&matrix_copy);
                cairo_matrix_transform_point( &matrix_copy, &x, &y);
                cairo_matrix_invert(&matrix_copy);
                set_event_coords(&event_copy, x, y);
        }

        return item_event(item, &event_copy, &matrix_copy, pick_item);
}

/**
 * event:
 * @item:
 * @event: The GdkEvent struct with x,y in local item coordinate space.
 * @matrix: The cumulative matrix used to go between item and device space.
 *
 * Item implementations should chain up to this if they intend to return FALSE
 * to the event call.  This allows items further up the tree to get the correct
 * cumulative matrix. Do not chain-up if return value is true. Signal connected
 * implementations need not call this as it will be handled.
 *
 * Returns: True if this item consumed the event. False sends the event up the
 * tree.
 **/
static gboolean
event(CrItem *item, GdkEvent *event, cairo_matrix_t *matrix, CrItem *pick_item)
{
        double x, y;


        if (item->matrix) {

                /* this removes the effect of this local item so the proper
                 * coordinates can be propagated up the item tree.*/
                if (gdk_event_get_coords(event, &x, &y)) {
                        cairo_matrix_transform_point(item->matrix, &x, &y);
                        set_event_coords(event, x, y);
                }
                inverse_matrix(item);
                cairo_matrix_multiply(matrix, item->matrix_i, matrix);
        }

        return FALSE;
}

static inline gboolean
test_inside(double x1, double y1, double x2, double y2,
                double bx1, double by1, double bx2, double by2)
{
        /* If the rectangle intersection is not a rectangle, then it is
         * outside. */
        if (MAX(bx1, x1) > MIN(bx2, x2)) return FALSE;
        if (MAX(by1, y1) > MIN(by2, y2)) return FALSE;

        return TRUE;
}

static inline gboolean
is_inside(CrItem *item, cairo_t *c, double x1, double y1, double x2, double y2)
{
        double bx1, by1, bx2, by2, tmp;
        gboolean inside;

        if (!item->bounds) return FALSE;

        bx1 = item->bounds->x1;
        by1 = item->bounds->y1;
        bx2 = item->bounds->x2;
        by2 = item->bounds->y2;

        if (!item->bounds) return FALSE;

        inside = test_inside(x1, y1, x2, y2, bx1, by1, bx2, by2);

        if (!inside) {

                if (get_device_bounds(item, c, &bx1, &by1, &bx2, &by2)) {

                        cairo_device_to_user(c, &bx1, &by1);
                        cairo_device_to_user(c, &bx2, &by2);
                        if (bx1 > bx2) {
                                tmp = bx1;
                                bx1 = bx2;
                                bx2 = tmp;
                        }
                        if (by1 > by2) {
                                tmp = by1;
                                by1 = by2;
                                by2 = tmp;
                        }

                        return test_inside(x1, y1, x2, y2, bx1, by1, bx2, by2);
                }
        }
        return inside;
}

static void
invoke_calculate_bounds(CrItem *item, cairo_t *c)
{
        gboolean have_bounds;
        CrDeviceBounds *device;
        CrBounds *bounds;

        if (item->bounds) {
                bounds = cr_bounds_ref(item->bounds);
                memset(bounds, 0, sizeof(double)*4);
        }
        else
                bounds = cr_bounds_new();

        if (item->device)
                device = cr_device_bounds_ref(item->device);
        else 
                device = cr_device_bounds_new();

        have_bounds = FALSE;

#if DEBUG_PROFILE
        num_updates++;
        clock_t ticks;
        ticks = clock();
#endif

        /* Asks the derived item to report its bounds. 
         * The derived item may do an update at this time.*/
        g_signal_emit(item, signals[CALCULATE_BOUNDS], 0, 
                        c, bounds, device, &have_bounds);
#if DEBUG_PROFILE
        update_ticks += clock() - ticks;
#endif

        if (have_bounds) {
                if (!item->bounds)
                        item->bounds = cr_bounds_ref(bounds);

                if (device->x2 - device->x1 != 0 ||
                                device->y2 - device->y1 != 0) {

                        if (!item->device) 
                                item->device = cr_device_bounds_ref(device);
                }
                else if (item->device) {
                        cr_device_bounds_unref(item->device);
                        item->device = NULL;
                }
        }
        else if (item->bounds) {
                cr_bounds_unref(item->bounds);
                item->bounds = NULL;
        }

        cr_bounds_unref(bounds);
        cr_device_bounds_unref(device);
}

static void
invoke_paint(CrItem *item, cairo_t *ct, gboolean all,
                double x1, double y1, double x2, double y2)
{
        CrItem *i;
        GList *list;
        double ax1, ay1, ax2, ay2, bx1, by1, bx2, by2;

        if (!(item->flags & CR_ITEM_VISIBLE)) {
                item->flags &= ~CR_ITEM_NEED_UPDATE;
                return;
        }

        if (item->flags & CR_ITEM_NEED_UPDATE) {
                /* It is possible to get to this point if the device (canvas)
                 * has shut off its update loop. Items may require the
                 * #calculate_bounds routine as a necessary step prior to
                 * painting. Note, there is no short-cut way to cut this out,
                 * because if 'update' was called on a canvas item without
                 * calling calculate bounds, then there would be no way to
                 * calculate the bounds later as it has already been updated.
                 * The only way would be to have separate 'update' and 'bounds'
                 * method.*/
                (*CR_ITEM_GET_CLASS(item)->invoke_calculate_bounds)(item, ct);

                item->flags &= ~CR_ITEM_NEED_UPDATE;
        }

        if (item->matrix) {
                inverse_matrix(item);

                /* This finds the largest enclosing un-rotated rectangle in 
                 * case the transformed rectangle has been rotated. */

                ax1 = bx1 = x1; ay1 = by1 = y1; ax2 = bx2 = x2; ay2 = by2 = y2;
                cairo_matrix_transform_point(item->matrix_i, &ax1, &ay1);
                cairo_matrix_transform_point(item->matrix_i, &bx2, &by1);
                cairo_matrix_transform_point(item->matrix_i, &ax2, &ay2);
                cairo_matrix_transform_point(item->matrix_i, &bx1, &by2);
                x1 = MIN(MIN(MIN(ax1, ax2), bx1), bx2);
                y1 = MIN(MIN(MIN(ay1, ay2), by1), by2);
                x2 = MAX(MAX(MAX(ax1, ax2), bx1), bx2);
                y2 = MAX(MAX(MAX(ay1, ay2), by1), by2);
        }

        cairo_save(ct);

        if (item->matrix) cairo_transform(ct, item->matrix);

        if (all || is_inside(item, ct, x1, y1, x2, y2)) {
#if DEBUG_PROFILE
        num_paints++;
        clock_t ticks = clock();
#endif
                g_signal_emit(item, signals[PAINT], 0, ct);
#if DEBUG_PROFILE
        paint_ticks += clock() - ticks;
#endif
        }

        for (list = item->items; list; list = list->next) {
                i = list->data;
                cr_item_invoke_paint(i, ct, all, x1, y1, x2, y2);
        }

        cairo_restore(ct);
}

static void
added(CrItem *item, CrItem *child)
{
        /* This used to propagate the signal through the item tree.*/
        g_signal_emit(item, signals[ADDED], 0, child);
}

static void
removed(CrItem *item, CrItem *child)
{
        g_signal_emit(item, signals[REMOVED], 0, child);
}

static void
add(CrItem *item, CrItem *child)
{
        g_object_ref(child);

        item->items = g_list_append(item->items, child);

        /* This signal will propagate the event upwards through the group
         * hierarchy only if user signals return false.  This way a parent item
         * may be able to consume the event when a child tests true. */
        g_signal_connect_data(child, "event", (GCallback) item_event,
                        item, NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);

        /* The device needs to know whenever an item is added or removed.
         * This signal will also propagate up.*/
        g_signal_connect_swapped(child, "added", (GCallback) added, item);
        g_signal_connect_swapped(child, "removed", (GCallback) removed, item);

        /* The invalidate signal is propagate up through the hierarchy so that
         * multiple devices (canvases) may receive this signal in their 
         * respective coordinate systems. */
        g_signal_connect_swapped(child, "invalidate", (GCallback) invalidated,
                        item);

        added(item, child);
        cr_item_request_update(child);
}

static void
_remove(CrItem *item, CrItem *child)
{
        GList *list, *link;

        link = g_list_find(item->items, child);
        if (link) {
                item->flags |= CR_ITEM_NEED_UPDATE;
                cr_item_report_old_bounds(child, NULL, 1);
                item->items = g_list_delete_link(item->items, link);
                g_signal_handlers_disconnect_by_func(child,
                                (GCallback)item_event, item);
                g_signal_handlers_disconnect_by_func(child,
                                (GCallback)added, item);
                g_signal_handlers_disconnect_by_func(child,
                                (GCallback)removed, item);
                g_signal_handlers_disconnect_by_func(child,
                                (GCallback)invalidated, item);
                removed(item, child);
                g_object_unref(child);
        }
        else {
                for (list = item->items; list; list = list->next) {
                        cr_item_remove(CR_ITEM(list->data), child);
                }
        }
}

static void 
report_old_bounds(CrItem *item, cairo_t *ct, gboolean all)
{
        GList *list;
        int mask;

        /* 1. Only the specific items that requested updates will send
         * invalidate signals. 
         * 2. The item may have been shown, then hidden.  In this case we need
         * to send an invalidate even if it has no bounds.
         * 3. The item may never have been shown before.  In this case no need
         * to invalidate here.
         * 4. The item may have been hidden, then shown.  In this case no need
         * to invalidate here.
         */

        all |= (item->flags & CR_ITEM_NEED_UPDATE);

        /* parent is expecting to be called */
        item->flags |= CR_ITEM_IN_REPORT_BOUNDS;
         
        if (all && item->bounds) {

                mask = CR_ITEM_INVALIDATE_OLD;
                if (item->flags & CR_ITEM_NEED_UPDATE)
                        mask |= CR_ITEM_INVALIDATE_UPDATE;

                /* sends the old bounds to the through the item tree to
                 * the device for invalidation */
                invalidated(item, mask,
                                item->bounds->x1, item->bounds->y1, 
                                item->bounds->x2, item->bounds->y2,
                                item->device);
        }

        /* do the same for the children. */
        for (list = item->items; list; list = list->next) {
                cr_item_report_old_bounds(CR_ITEM(list->data), ct, all);
        }

        item->flags &= ~CR_ITEM_IN_REPORT_BOUNDS;
}

static void
report_new_bounds(CrItem *item, cairo_t *ct, gboolean all)
{
        GList *list;
        int mask;

        if (!(item->flags & CR_ITEM_VISIBLE)) {
                item->flags &= ~CR_ITEM_NEED_UPDATE;
                return;
        }

        cairo_save(ct);

        if (item->matrix) {
                cairo_transform(ct, item->matrix);
                /* save this for later to report the old bounds. */
                *item->matrix_p = *item->matrix;
        }

        all |= (item->flags & CR_ITEM_NEED_UPDATE);

        item->flags |= CR_ITEM_IN_REPORT_BOUNDS;

        for (list = item->items; list; list = list->next) {
                /* If a group needs to be updated, then all children also
                 * will need to re-report bounds */
                cr_item_report_new_bounds(CR_ITEM(list->data), ct, all);
        }

        if (item->flags & CR_ITEM_NEED_UPDATE) 
                (*CR_ITEM_GET_CLASS(item)->invoke_calculate_bounds)(item, ct);

        if ((item->flags & CR_ITEM_NEED_UPDATE) || all ) {

                if (item->bounds) {

                        mask = CR_ITEM_INVALIDATE_NEW;
                        if (item->flags & CR_ITEM_NEED_UPDATE)
                                mask |= CR_ITEM_INVALIDATE_UPDATE;

                        /* The invalidate signal reports to the item or device
                         * immediately above it. It needs to report in item
                         * units except for the device paramters (if any). */
                        invalidated(item, mask,
                                        item->bounds->x1, item->bounds->y1,
                                        item->bounds->x2, item->bounds->y2,
                                        item->device);

                }
        }
        item->flags &= ~CR_ITEM_NEED_UPDATE;
        item->flags &= ~CR_ITEM_IN_REPORT_BOUNDS;

        cairo_restore(ct);
}

static void
request_update(CrItem *item)
{
        if (item->flags & CR_ITEM_VISIBLE)
                item->flags |= CR_ITEM_NEED_UPDATE;

        /* A signal emission lets the device know that an update has been
         * requested.*/
}
 
static gboolean
object_handled_accumulator (GSignalInvocationHint *ihint,
                             GValue                *return_accu,
                             const GValue          *handler_return,
                             gpointer               dummy)
{
        GObject *object;

        object = g_value_get_object (handler_return);
        if (object) {
                g_value_set_object (return_accu, object);
                return FALSE;
        }

        return TRUE;
}

static gboolean
boolean_handled_accumulator (GSignalInvocationHint *ihint,
                             GValue                *return_accu,
                             const GValue          *handler_return,
                             gpointer               dummy)
{
        gboolean continue_emission;
        gboolean signal_handled;

        signal_handled = g_value_get_boolean (handler_return);
        g_value_set_boolean (return_accu, signal_handled);
        continue_emission = !signal_handled;

        return continue_emission;
}

static gboolean
boolean_accumulator (GSignalInvocationHint *ihint,
                             GValue                *return_accu,
                             const GValue          *handler_return,
                             gpointer               dummy)
{
        gboolean result, current;

        result = g_value_get_boolean (handler_return);
        current = g_value_get_boolean (return_accu);
        g_value_set_boolean (return_accu, result || current);

        return TRUE;
}

static void
cr_item_class_init(CrItemClass *klass)
{
        GObjectClass *object_class;

        object_class = (GObjectClass *) klass;

        parent_class = g_type_class_peek_parent (klass);
        object_class->get_property = cr_item_get_property;
        object_class->set_property = cr_item_set_property;
        object_class->dispose = cr_item_dispose;
        object_class->finalize = cr_item_finalize;
        klass->add = add;
        klass->remove = _remove;
        klass->request_update = request_update;
        klass->invoke_paint = invoke_paint;
        klass->invoke_test = invoke_test;
        klass->invoke_calculate_bounds = invoke_calculate_bounds;
        klass->report_old_bounds = report_old_bounds;
        klass->report_new_bounds = report_new_bounds;
        klass->event = event;

        g_object_class_install_property
                (object_class,
                 PROP_X,
                 g_param_spec_double ("x", "x", "The matrix translation in the"
                                " x direction in item units.",
                                -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                G_PARAM_READWRITE));
        g_object_class_install_property
                (object_class,
                 PROP_Y,
                 g_param_spec_double ("y", "y", "The matrix translation in the"
                                " y direction in item units.",
                                -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                G_PARAM_READWRITE));
        g_object_class_install_property
                (object_class,
                 PROP_VISIBLE,
                 g_param_spec_boolean ("visible", NULL, "Whether the item is "
                                "visible",
                                TRUE,
                                G_PARAM_READWRITE));
        g_object_class_install_property
                (object_class,
                 PROP_AVOID_TEST,
                 g_param_spec_boolean ("avoid_test", NULL, 
                        "Prevents item and children from being cursor tested.",
                        FALSE,
                        G_PARAM_READWRITE));
        g_object_class_install_property
                (object_class,
                 PROP_TEST_RECTANGLE,
                 g_param_spec_boolean ("test-rectangle", "test-rectangle",
                                "If the point test routine should ignore "
                                "any details about the item and just "
                                "test the enclosing rectangle.  For CrPath "
                                "based items the test-fill property may be "
                                "more appropriate.",
                                FALSE,
                                G_PARAM_READWRITE));
        g_object_class_install_property
                (object_class,
                 PROP_MATRIX,
                 g_param_spec_boxed("matrix", "matrix",
                         "Cairo matrix used to transform this item.",
                         CR_TYPE_MATRIX,
                         G_PARAM_READWRITE));

        signals[ADDED] = g_signal_new ("added",
                        CR_TYPE_ITEM,
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET(CrItemClass, added),
                        NULL, NULL,
                        g_cclosure_marshal_VOID__OBJECT,
                        G_TYPE_NONE, 1, CR_TYPE_ITEM);
        signals[REMOVED] = g_signal_new ("removed",
                        CR_TYPE_ITEM,
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET(CrItemClass, removed),
                        NULL, NULL,
                        g_cclosure_marshal_VOID__OBJECT,
                        G_TYPE_NONE, 1, CR_TYPE_ITEM);
        signals[PAINT] = g_signal_new ("paint",
                        CR_TYPE_ITEM,
                        G_SIGNAL_RUN_LAST,
                        G_STRUCT_OFFSET(CrItemClass, paint),
                        NULL, NULL,
                        g_cclosure_marshal_VOID__BOXED,
                        G_TYPE_NONE, 1, CR_TYPE_CONTEXT);
        signals[TEST] = g_signal_new ("test",
                        CR_TYPE_ITEM,
                        G_SIGNAL_RUN_LAST,
                        G_STRUCT_OFFSET(CrItemClass, test),
                        object_handled_accumulator,
                        NULL,
                        cr_marshal_OBJECT__BOXED_DOUBLE_DOUBLE,
                        CR_TYPE_ITEM, 3, CR_TYPE_CONTEXT,
                        G_TYPE_DOUBLE, G_TYPE_DOUBLE);
        signals[REQUEST_UPDATE] = g_signal_new ("request-update",
                        CR_TYPE_ITEM,
                        G_SIGNAL_RUN_LAST,
                        G_STRUCT_OFFSET(CrItemClass, request_update),
                        NULL, NULL,
                        g_cclosure_marshal_VOID__VOID,
                        G_TYPE_NONE, 0);
        signals[EVENT] = g_signal_new ("event",
                        CR_TYPE_ITEM,
                        G_SIGNAL_RUN_LAST,
                        G_STRUCT_OFFSET(CrItemClass, event),
                        boolean_handled_accumulator, NULL,
                        cr_marshal_BOOLEAN__BOXED_BOXED_OBJECT,
                        G_TYPE_BOOLEAN, 3,
                        GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE,
                        CR_TYPE_MATRIX, CR_TYPE_ITEM);
        signals[CALCULATE_BOUNDS] = g_signal_new ("calculate_bounds",
                        CR_TYPE_ITEM,
                        G_SIGNAL_RUN_LAST,
                        G_STRUCT_OFFSET(CrItemClass, calculate_bounds),
                        boolean_accumulator, NULL,
                        cr_marshal_BOOLEAN__BOXED_BOXED_BOXED,
                        G_TYPE_BOOLEAN, 3, CR_TYPE_CONTEXT, CR_TYPE_BOUNDS,
                        CR_TYPE_DEVICE_BOUNDS);
        signals[INVALIDATE] = g_signal_new ("invalidate",
                        CR_TYPE_ITEM,
                        G_SIGNAL_RUN_LAST,
                        G_STRUCT_OFFSET(CrItemClass, invalidate),
                        NULL, NULL,
                        cr_marshal_VOID__INT_DOUBLE_DOUBLE_DOUBLE_DOUBLE_BOXED,
                        G_TYPE_NONE, 6, G_TYPE_INT,
                        G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE, 
                        G_TYPE_DOUBLE, CR_TYPE_DEVICE_BOUNDS);
}

GType
cr_item_get_type(void)
{
        static GType type = 0;
        static const GTypeInfo info = {
                sizeof(CrItemClass),
                NULL, /*base_init*/
                NULL, /*base_finalize*/
                (GClassInitFunc) cr_item_class_init,
                (GClassFinalizeFunc) NULL,
                NULL,
                sizeof(CrItem),
                0,
                (GInstanceInitFunc) cr_item_init,
                NULL
        };
        if (!type) {
                type = g_type_register_static(G_TYPE_OBJECT,
                        "CrItem", &info, 0);
        }
        return type;
}

/**
 * cr_item_invoke_paint:
 * @item:
 * @ct: cairo context to paint with.
 * @x1: Left coord of the rectangle to be painted in pre-item units.
 * @y1: Top coord of the rectangle to be painted in pre-item units.
 * @x2: Right coord of the rectangle to be painted in pre-item units.
 * @y2: Bottom coordinate of the rectangle to be painted in pre-item units.
 *
 * This is normally called only by CrCanvas or CrItem.
 */
void 
cr_item_invoke_paint(CrItem *item, cairo_t *ct, gboolean all,
                double x1, double y1, double x2, double y2)
{
        (*CR_ITEM_GET_CLASS(item)->invoke_paint)(item, ct, all, x1, y1, x2, y2);
}

/**
 * cr_item_invoke_test:
 * @item:
 * @c: The cairo context with its transformation matrix set to reflect all
 * transformations higher in the item tree.
 * @x: The point to be tested in device coordinates.
 * @y: The point to be tested in device coordinates.
 *
 * This is normally called only by CrCanvas or CrItem.  It is used to run the
 * test method or emit a test signal on the derived canvas items.
 *
 * Returns: The first canvas item in the z-order to test TRUE.
 */
CrItem * 
cr_item_invoke_test(CrItem *item, cairo_t *c, double x, double y)
{
        return (*CR_ITEM_GET_CLASS(item)->invoke_test)(item, c, x, y);
}

/** 
 * cr_item_invoke_event:
 * @item:
 * @event: The event with coordinates in device units
 * @matrix: The cumulative matrix from device to this item.
 * @pick_item: The item the cursor is on.  This may be different from item if
 * the event is caught on a non-leaf node.
 *
 * This is normally called only by the device. It is used to trigger the
 * derived canvas item 'event' signal.
 *
 * Returns: TRUE to stop propagation of the event.  FALSE to continue.
 */
gboolean 
cr_item_invoke_event(CrItem *item, GdkEvent *event, cairo_matrix_t *matrix,
                CrItem *pick_item)
{
        return invoke_event(item, event, matrix, pick_item);
}

/**
 * cr_item_report_old_bounds:
 * @item:
 * @ct: The cairo context transformed to be relative to the device coordinates
 * for this item.
 * @all: Used to tell children to re-report theit bounds.
 *
 * This is normally called only by the canvas widget or canvas item.  It is used
 * to report back invalidated regions to the canvas or device.
 */
void 
cr_item_report_old_bounds(CrItem *item, cairo_t *ct, gboolean all)
{
        (*CR_ITEM_GET_CLASS(item)->report_old_bounds)(item, ct, all);
}

/**
 * cr_item_report_new_bounds:
 * @item:
 * @ct: The cairo context transformed to be relative to the device coordinates
 * for this item.
 * @all: Used to tell children to re-report theit bounds.
 *
 * This is normally called only by the canvas widget or canvas item.  It is used
 * to trigger calculate_bounds in derived canvas items and to report back
 * invalidated regions to the canvas or device.
 */
void 
cr_item_report_new_bounds(CrItem *item, cairo_t *ct, gboolean all)
{
        (*CR_ITEM_GET_CLASS(item)->report_new_bounds)(item, ct, all);
}

/**
 * cr_item_request_update:
 * @item:
 *
 * Propagates request up through the canvas item tree, to let the canvas know
 * that a screen refresh is required.
 **/
void
cr_item_request_update(CrItem *item)
{
        g_signal_emit(item, signals[REQUEST_UPDATE], 0);
}

/**
 * cr_item_get_matrix:
 * @item:
 *
 * Creates an identity matrix if one does not exist.  
 * Otherwise returns the current matrix. If you modify the matrix in any way,
 * you must call request_update in order to see the results.
 *
 * Returns: The canvas item transformation matrix.
 **/
cairo_matrix_t *
cr_item_get_matrix(CrItem *item)
{
        create_matrix(item);
        return item->matrix;
}

/**
 * cr_item_get_inverse_matrix:
 * @item:
 *
 * Creates an identity matrix if one does not exist. Otherwise returns the
 * current inverse matrix. If you modify the matrix in any way, you must call
 * request_update in order to see the results.
 *
 * Returns: The canvas item inverse transformation matrix.
 **/
cairo_matrix_t *
cr_item_get_inverse_matrix(CrItem *item)
{
        create_matrix(item);
        inverse_matrix(item);
        return item->matrix_i;
}

/**
 * cr_item_cancel_matrix:
 * @item:
 *
 * Removes any transformations from this canvas item and request an update if
 * necessary.
 **/
void
cr_item_cancel_matrix(CrItem *item)
{
        gboolean update;

        update = (item->matrix != NULL);
        destroy_matrix(item);
        if (update) cr_item_request_update(item);
}

/**
 * cr_item_add:
 * @item: The parent item.
 * @child: The child item.
 *
 * The child item is added under management of the parent item such that the
 * rendering (z-order) of the child will be more forward than the parent.  The
 * parent will add a reference to the child.  The child can be unref'd by the 
 * user after adding.
 **/
void
cr_item_add(CrItem *item, CrItem *child)
{
        (*CR_ITEM_GET_CLASS(item)->add)(item, child);
}

/**
 * cr_item_remove:
 * @item: The parent canvas item.
 * @child: The child canvas item.
 *
 * Removes and unref's child from the item's list or and sublist under item. The
 * child need not be a direct descended of the parent item.  This means you can
 * call this function with the canvas root item if necessary.  The tree will be
 * searched until the child is found.
 **/
void
cr_item_remove(CrItem *item, CrItem *child)
{
        (*CR_ITEM_GET_CLASS(item)->remove)(item, child);
}

/**
 * cr_item_get_bounds:
 * @item:
 * @x1: Leftmost edge of the bounding box (return value).
 * @y1: Upper edge of the bounding box (return value).
 * @x2: Rightmost edge of the bounding box (return value).
 * @y2: Lower edge of the bounding box (return value).
 *
 * The bounds are returned in the world coordinate system of item.
 * Note that this routine will return the last set of bounds reported by the
 * item during a canvas update.  If no updates have occurred then %FALSE is
 * returned.  Use #cr_item_calculate_bounds if you need to know the bounds
 * immediately.
 *
 * Returns: %FALSE if the bounds are not available for this item.
 **/
gboolean
cr_item_get_bounds(CrItem *item, double *x1, double *y1, double *x2, double *y2)
{
        if (item->bounds) {
                *x1 = item->bounds->x1;
                *y1 = item->bounds->y1;
                *x2 = item->bounds->x2;
                *y2 = item->bounds->y2;
        }
        return (item->bounds != NULL);
}

/**
 * cr_item_get_device_bounds:
 * @item:
 * @device: A structure representing the items device-based boundaries (if any).
 * If this return returns %TRUE, the structure will be filled with the item's
 * device based boundaries.
 *
 * The device bounds report any space the item uses that is device based.
 * This space is anchored to the item bounds reported by #cr_item_get_bounds.
 * Note that this routine will return the last set of bounds reported by the
 * item during a canvas update.  If no updates have occurred then %FALSE is
 * returned.  Use #cr_item_calculate_bounds if you need to know the bounds
 * immediately.
 *
 * Returns: %TRUE if this item has any device based space.  %FALSE if not.
 */
gboolean
cr_item_get_device_bounds(CrItem *item, CrDeviceBounds *device)
{
        if (item->device)
                *device = *item->device;

        return (item->device != NULL);
}

/**
 * cr_item_hide:
 * @item:
 *
 * Hides the item.  If the item is a parent, then all children will
 * be hidden as well.
 **/
void
cr_item_hide(CrItem *item)
{
        if (!(item->flags & CR_ITEM_VISIBLE)) return;

        g_signal_emit(item, signals[REQUEST_UPDATE], 0);

        item->flags &= ~CR_ITEM_VISIBLE;
}

/**
 * cr_item_show:
 * @item:
 *
 * Shows the item.  If the item is a parent, then the children may or may not 
 * be shown depending on their visible property.
 **/
void
cr_item_show(CrItem *item)
{
        if (item->flags & CR_ITEM_VISIBLE) return;

        item->flags |= CR_ITEM_VISIBLE;

        g_signal_emit(item, signals[REQUEST_UPDATE], 0);
}

/**
 * cr_item_construct:
 * @parent: The parent item.
 * @type: The type of item to create.
 * @first_arg_name: The name of the first argument.
 * @args: The list of arguments used to configure the item.
 *
 * A constructor for use by CrItem implementations.
 *
 * Returns: A newly created reference.  You must call g_object_ref if you intend
 * to keep it around.
 */
CrItem *
cr_item_construct(CrItem *parent, GType type, 
                const gchar *first_arg_name, va_list args) 
{
        CrItem *item;

        g_return_val_if_fail (CR_IS_ITEM (parent), NULL);
        g_return_val_if_fail (g_type_is_a (type, CR_TYPE_ITEM), NULL);

        item = CR_ITEM(g_object_new_valist(type, first_arg_name, args));

        cr_item_add(parent, item);

        g_object_unref(item);
        return item;
}

/**
 * cr_item_new:
 * @parent: The parent item.
 * @type: The type of item to create.
 * @first_arg_name: A list of object argument name/value pairs, NULL-terminated,
 * used to configure the item.
 * @varargs:
 *
 * A factory method to create a new CrItem and add it to a group in one step.
 *
 * Returns: The newly created item. You must call g_object_ref if you intend to
 * use this reference outside the local scope.
 */
CrItem *
cr_item_new(CrItem *parent, GType type, const gchar *first_arg_name, ...)
{
        CrItem *item;
        va_list args;

        va_start (args, first_arg_name);
        item = cr_item_construct(parent, type, first_arg_name, args);
        va_end (args);

        return item;
}

/**
 * cr_item_move:
 * @item:
 * @dx: The distance in item units to move in the x direction.
 * @dy: The distance in item units to move in the y direction.
 *
 * Moves the item to a position relative to where it is now.
 */
void 
cr_item_move(CrItem *item, double dx, double dy)
{
        cairo_matrix_t *matrix;

        if (dx == 0 && dy == 0) return;

        matrix = cr_item_get_matrix(item);
        matrix->x0 += dx;
        matrix->y0 += dy;
        cr_item_request_update(item);
}

/**
 * cr_item_set_z_relative:
 * @item: The group holding the item.
 * @child: The item to move.
 * @positions: Number positions to move by. Positive goes up.  Negative goes
 * down.

 * Moves the child item in height relative to its siblings by the number of
 * places given in @positions.
 */
void 
cr_item_set_z_relative(CrItem *item, CrItem *child, int positions)
{
        int index;

        if (positions == 0) return;
        index = g_list_index(item->items, child);
        g_return_if_fail(index >= 0);
        index += positions;
        if (index < 0) index = 0;
        item->items = g_list_remove(item->items, child);
        item->items = g_list_insert(item->items, child, index);
        cr_item_request_update(child);
}

/**
 * cr_item_set_z:
 * @item: The group holding the item.
 * @child: The item to move.
 * @position: Absolute position. Positive is places from the bottom.  
 * Negative is places from the top.
 * 
 * Sets the child item height relative to its siblings to be the absolute
 * position given by @position. Zero is the bottom, -1 is the top.
 */
void 
cr_item_set_z(CrItem *item, CrItem *child, int position)
{
        int index, length;

        index = g_list_index(item->items, child);
        g_return_if_fail(index >= 0);
        if (index == position) return;
        if (position < -1) {
                length = g_list_length(item->items);
                position = length - position;
                if (position < 0) position = 0;
        }
        item->items = g_list_remove(item->items, child);
        item->items = g_list_insert(item->items, child, position);
        cr_item_request_update(child);
}

/**
 * cr_item_make_temp_cairo:
 *
 * A convenience procedure used by some implementations to create a path. In
 * order to use this, the scale must set the scale to be roughly "ball-park" to
 * the scale where the canvas item will be painted.
 *
 * Returns: A cairo context. Call cairo_destroy on this reference when finished.
 */
CrContext *
cr_item_make_temp_cairo()
{
        cairo_surface_t *surface;
        cairo_t *cairo;

        surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 0, 0);
        cairo = cairo_create(surface);
        cairo_surface_destroy(surface);
        return cairo;
}

/**
 * cr_item_calculate_bounds:
 * @item:
 * @bounds: Bounding box in item coordinates.
 * @device_bounds: A structure for specifying device coordinate boundaries (if
 * any)
 *
 * Run the #CrItem::calculate_bounds bounds signal and virtual method on the
 * item using a temporary cairo context and report the results.  The results are
 * not retained by this object. This function
 * is provided as a user convenience and is not run as part of normal canvas
 * operation. The reason a user might want to call this is to know the bounds in
 * advance of the first canvas update.  The #cr_item_get_bounds and
 * #cr_item_get_device_bounds methods will return %FALSE unless the canvas has
 * been updated at least once. The disadvantage to using this is that in some
 * extreme cases the bounds reported by a temporary cairo context may differ
 * from the bounds reported by the cairo context being used by the canvas
 * device. So this routine really will give only an approximation of what the
 * bounds will be when the item is rendered to the screen.
 *
 * Returns: %TRUE if the the bounds are defined even if the are 0, 0, 0, 0.
 */
gboolean
cr_item_calculate_bounds(CrItem *item, CrBounds *bounds, CrDeviceBounds 
                *device_bounds)
{
        gboolean have_bounds;
        cairo_t *c;

        have_bounds = FALSE;

        c = cr_item_make_temp_cairo();

        /* Asks the derived item to report its bounds. 
         * The derived item may do an update at this time.*/
        g_signal_emit(item, signals[CALCULATE_BOUNDS], 0, 
                        c, bounds, device_bounds, &have_bounds);

        cairo_destroy(c);

        return have_bounds;
}

