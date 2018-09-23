/* cr-panner.c 
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
#include "cr-marshal.h"
#include "cr-panner.h"

/**
 * SECTION:cr-panner
 * @title: CrPanner
 * @short_description: A object for setting up panning on a #CrCanvas widget.
 *
 * 
 */

static GObjectClass *parent_class = NULL;

enum {
        ARG_0,
        PROP_ACTIVE,
        PROP_CURSOR,
        PROP_CANVAS,
        PROP_BUTTON, 
        PROP_ITEM
};

enum {
        PAN,
        ACTIVATE,
        DEACTIVATE,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
pan(CrPanner *panner, double dx, double dy)
{
        double x1, y1;
        cairo_matrix_t *matrix_i;

        if (dx || dy) {
                matrix_i = cr_item_get_inverse_matrix(panner->canvas->root);
                cairo_matrix_transform_distance(matrix_i, &dx, &dy);

                cr_canvas_get_center(panner->canvas, &x1, &y1);
                cr_canvas_center_on(panner->canvas, x1 - dx, y1 - dy);
                // this line will make it more or less efficient depending on
                // what is on the screen
                cr_canvas_queue_repaint(panner->canvas);
                /* can also do it this way:
                cr_canvas_get_scroll_offsets(panner->canvas, &x1, &y1);
                cr_canvas_scroll_to(panner->canvas, x1 - dx, y1 - dy);
                */
        }
}

static void 
activate(CrPanner *panner)
{
        GdkWindow *window;
        GdkCursor *cursor;
	GdkDisplay *display;

        g_return_if_fail(panner->canvas != NULL);

	window = gtk_widget_get_window(GTK_WIDGET(panner->canvas));
	display = gdk_window_get_display(window);
	cursor = gdk_cursor_new_for_display(display, panner->cursor);
	gdk_window_set_cursor(window, cursor);
        g_object_unref(cursor);
        panner->flags |= CR_PANNER_ACTIVE;
}

static void 
deactivate(CrPanner *panner)
{
        GdkWindow *window;

	window = gtk_widget_get_window(GTK_WIDGET(panner->canvas));
        gdk_window_set_cursor(window, NULL);
        panner->flags &= ~CR_PANNER_ACTIVE;

        if (panner->flags  & CR_PANNER_ROOT_AVOID_TEST) {
                g_object_set(panner->canvas->root, "avoid-test", FALSE, NULL);
                panner->flags &= ~CR_PANNER_ROOT_AVOID_TEST;
        }
}

static gboolean
on_event(CrItem *item, GdkEvent *event, CrMatrix *matrix,  
                CrItem *pick_item, CrPanner *panner)
{
        gboolean state;
        double dx, dy, x, y;

        state = FALSE;

        /* button can implicitly activate the panner */
        if (event->type == GDK_BUTTON_PRESS) {
                if (!(panner->flags & CR_PANNER_ACTIVE))
                       {
                       if (event->button.button == panner->button)
                               cr_panner_activate(panner);
                       else
                               return FALSE;
                       }
                panner->last_x = event->button.x;
                panner->last_y = event->button.y;
                cairo_matrix_transform_point(matrix, 
                                &panner->last_x, &panner->last_y);
                panner->flags |= CR_PANNER_DRAGGING;
                panner->last_msec = event->button.time;
                state = TRUE;
        }
        else if (!(panner->flags & CR_PANNER_ACTIVE)) return FALSE;
        else if (event->type == GDK_MOTION_NOTIFY) {
                if ((panner->flags & CR_PANNER_DRAGGING) &&
                                event->button.time - panner->last_msec >= 100) {
                        x = event->motion.x;
                        y = event->motion.y;
                        cairo_matrix_transform_point(matrix, &x, &y);
                        dx = x - panner->last_x;
                        dy = y - panner->last_y;
                        g_signal_emit(panner, signals[PAN], 0, dx, dy);
                        panner->last_msec = event->button.time;
                        panner->last_x = x;
                        panner->last_y = y;
                        state = TRUE;
                }
        }
        else if (event->type == GDK_BUTTON_RELEASE) {
                cr_panner_deactivate(panner);
                panner->flags &= ~CR_PANNER_DRAGGING;
                state = TRUE;
        }
        return state;
}

static CrItem *
on_test(CrItem *item, CrContext *cr, double x, double y)
{
        return item;
}

static void
set_item(CrPanner *panner, GObject *object)
{
        g_return_if_fail (CR_IS_ITEM(object));

        if (panner->item) {
                g_signal_handlers_disconnect_by_func(object, 
                                G_CALLBACK(on_test), panner);
                g_signal_handlers_disconnect_by_func(object, 
                                G_CALLBACK(on_event), panner);
                g_object_unref(panner->item);
        }
        g_signal_connect(object, "test", G_CALLBACK(on_test), panner);
        g_signal_connect(object, "event", G_CALLBACK(on_event), panner);
        panner->item = CR_ITEM(object);
        g_object_ref(panner->item);
}

static void
set_canvas(CrPanner *panner, GObject *object)
{
        g_return_if_fail (CR_IS_CANVAS(object));

        if (panner->canvas) {
                g_object_unref(panner->canvas);
        }
        panner->canvas = CR_CANVAS(object);
        g_object_ref(panner->canvas);
}

static void
cr_panner_dispose(GObject *object)
{
        CrPanner *panner;

        panner = CR_PANNER(object);
        if (panner->canvas)
                g_object_unref(panner->canvas);
        parent_class->dispose(object);
}

static void
cr_panner_finalize(GObject *object)
{
        parent_class->finalize(object);
}

static void
cr_panner_init(CrPanner *panner)
{
        panner->cursor = GDK_HAND2;
        panner->button = 2;
}

static void
cr_panner_get_property(GObject *object, guint property_id,
                GValue *value, GParamSpec *pspec)
{
        CrPanner *panner = CR_PANNER(object);

        switch (property_id) {
                case PROP_ACTIVE:
                        g_value_set_boolean(value, panner->flags &
                                        CR_PANNER_ACTIVE);
                        break;
                case PROP_BUTTON:
                        g_value_set_int(value, panner->button);
                        break;
                case PROP_CANVAS:
                        g_value_set_object(value, panner->canvas);
                        break;
                case PROP_CURSOR:
                        g_value_set_int(value, panner->cursor);
                        break;
                case PROP_ITEM:
                        g_value_set_object(value, panner->item);
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                property_id, pspec);
        }
}

static void
cr_panner_set_property(GObject *object, guint property_id,
                const GValue *value, GParamSpec *pspec)
{
        CrPanner *panner = CR_PANNER(object);
        gboolean bval;

        switch (property_id) {
                case PROP_ACTIVE:
                        bval = g_value_get_boolean(value);
                        if (bval && !(panner->flags & CR_PANNER_ACTIVE))
                                cr_panner_activate(panner);
                        else if (!bval && (panner->flags & CR_PANNER_ACTIVE))
                                cr_panner_deactivate(panner);
                        break;
                case PROP_BUTTON:
                        panner->button = g_value_get_int(value);
                        break;
                case PROP_CANVAS:
                        set_canvas(panner, g_value_get_object (value));
                        break;
                case PROP_CURSOR:
                        panner->cursor = g_value_get_int(value);
                        break;
                case PROP_ITEM:
                        set_item(panner, g_value_get_object (value));
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                property_id, pspec);
        }
}

static void
cr_panner_class_init(CrPannerClass *klass)
{
        GObjectClass *object_class;

        object_class = (GObjectClass *) klass;

        parent_class = g_type_class_peek_parent (klass);
        object_class->dispose = cr_panner_dispose;
        object_class->finalize = cr_panner_finalize;
        object_class->get_property = cr_panner_get_property;
        object_class->set_property = cr_panner_set_property;
        klass->pan = pan;
        klass->activate = activate;
        klass->deactivate = deactivate;

        signals[PAN] = g_signal_new ("pan", 
                        CR_TYPE_PANNER,
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET(CrPannerClass, pan),
                        NULL, NULL,
                        cr_marshal_VOID__DOUBLE_DOUBLE,
                        G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);

        signals[ACTIVATE] = g_signal_new ("activate", 
                        CR_TYPE_PANNER,
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET(CrPannerClass, activate),
                        NULL, NULL,
                        cr_marshal_VOID__VOID,
                        G_TYPE_NONE, 0);

        signals[DEACTIVATE] = g_signal_new ("deactivate", 
                        CR_TYPE_PANNER,
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET(CrPannerClass, deactivate),
                        NULL, NULL,
                        cr_marshal_VOID__VOID,
                        G_TYPE_NONE, 0);

        g_object_class_install_property (object_class, PROP_ACTIVE,
                        g_param_spec_boolean("active",
                                "Active",
                                "Active/Deactivate the panner object or "
                                "check the activation status.",
                                FALSE,
                                G_PARAM_READWRITE));
        g_object_class_install_property(object_class,
                        PROP_CANVAS,
                        g_param_spec_object("canvas", "CrCanvas",
                                "Reference to CrCanvas widget",
                                CR_TYPE_CANVAS, 
                                G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                        PROP_CURSOR,
                        g_param_spec_int("cursor",
                                "Panner Cursor",
                                "GDK Cursor to use when panner is selected",
                                0, GDK_LAST_CURSOR, GDK_HAND2,
                                G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                        PROP_BUTTON,
                        g_param_spec_int("button",
                                "Panner button",
                                "Mouse button implicitly activates the panner",
                                0, 10, 2,
                                G_PARAM_READWRITE));
        g_object_class_install_property(object_class,
                        PROP_ITEM,
                        g_param_spec_object("item", "CrItem",
                                "Reference to CrItem from which to collect the "
                                "button events. Usually this is the canvas root"
                                " item.",
                                CR_TYPE_ITEM, 
                                G_PARAM_READWRITE));
}

GType
cr_panner_get_type(void)
{
        static GType type = 0;
        static const GTypeInfo info = {
                sizeof(CrPannerClass),
                NULL, /*base_init*/
                NULL, /*base_finalize*/
                (GClassInitFunc) cr_panner_class_init,
                (GClassFinalizeFunc) NULL,
                NULL,
                sizeof(CrPanner),
                0,
                (GInstanceInitFunc) cr_panner_init,
                NULL
        };
        if (!type) {
                type = g_type_register_static(G_TYPE_OBJECT,
                        "CrPanner", &info, 0);
        }
        return type;
}

/**
 * cr_panner_new:
 * @canvas: The canvas device that this panner will be used with.
 * @first_arg_name: A list of object argument name/value pairs, NULL-terminated,
 * used to configure the item.
 * @varargs:
 *
 * A factory method to create a new CrPanner and connect it to a canvas in one
 * step.
 *
 * Returns: The newly created CrPanner object.  Unlike with the constructors for
 * CrItem implementations, you own the returned reference.  You should call 
 * g_object_unref when you are finished with this object.
 */
CrPanner *
cr_panner_new(CrCanvas *canvas, const gchar *first_arg_name, ...)
{
        CrPanner *panner;
        va_list args;

        g_return_val_if_fail (CR_IS_CANVAS(canvas), NULL);

        panner = g_object_new(CR_TYPE_PANNER, NULL);
        set_canvas(panner, G_OBJECT(canvas));
        set_item(panner, G_OBJECT(canvas->root));

        va_start (args, first_arg_name);
        g_object_set_valist(G_OBJECT(panner), first_arg_name, args);
        va_end (args);

        return panner;
}

void 
cr_panner_activate(CrPanner *panner)
{
        g_signal_emit(panner, signals[ACTIVATE], 0);
}

void 
cr_panner_deactivate(CrPanner *panner)
{
        g_signal_emit(panner, signals[DEACTIVATE], 0);
}

