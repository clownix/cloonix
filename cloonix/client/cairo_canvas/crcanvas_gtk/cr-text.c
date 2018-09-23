/* cr-text.c
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
#include <math.h>
#include <gtk/gtk.h>
#include <syslog.h>
#include "cr-text.h"

/**
 * SECTION:cr-text
 * @title: CrText
 * @short_description: A text canvas item that uses #PangoLayout.
 *
 * This canvas item can render #PangoLayout.
 */

static GObjectClass *parent_class = NULL;

enum {
        ARG_0,
        PROP_X_OFFSET,
        PROP_Y_OFFSET,
        PROP_WIDTH,
        PROP_SCALEABLE,
        PROP_FONT,
        PROP_TEXT,
        PROP_FILL_COLOR_RGBA,
        PROP_FILL_COLOR,
        PROP_USE_MARKUP
};

static void
cr_text_dispose(GObject *object)
{
        parent_class->dispose(object);
}

static void
cr_text_finalize(GObject *object)
{
        CrText *text;

        text = CR_TEXT(object);

        if (text->layout) g_object_unref(text->layout);
        if (text->text) g_free(text->text);
        if (text->font_desc) pango_font_description_free(text->font_desc);

        parent_class->finalize(object);
}

static void
cr_text_set_property(GObject *object, guint property_id,
                const GValue *value, GParamSpec *pspec)
{
//        const char *str;
        GdkColor color = { 0, 0, 0, 0, };
        CrText *text = (CrText*) object;

        switch (property_id) {
                case PROP_X_OFFSET:
                        text->x_offset = g_value_get_double (value);
                        cr_item_request_update(CR_ITEM(text));
                        break;
                case PROP_Y_OFFSET:
                        text->y_offset = g_value_get_double (value);
                        cr_item_request_update(CR_ITEM(text));
                        break;
                case PROP_WIDTH:
                        text->width = g_value_get_double (value);
                        cr_item_request_update(CR_ITEM(text));
                        break;
                case PROP_SCALEABLE:
                        if (g_value_get_boolean(value))
                                text->flags |= CR_TEXT_SCALEABLE;
                        else
                                text->flags &= ~CR_TEXT_SCALEABLE;
                        cr_item_request_update(CR_ITEM(text));
                        break;
                case PROP_FONT:
                        if (text->font_desc)
                                pango_font_description_free(text->font_desc);
                        text->font_desc = pango_font_description_from_string(
                                        g_value_get_string (value));
                        cr_item_request_update(CR_ITEM(text));
                        break;
                case PROP_FILL_COLOR_RGBA:
                        text->fill_color_rgba = g_value_get_uint(value);
                        text->flags |= CR_TEXT_FILL_COLOR_RGBA;
                        cr_item_request_update(CR_ITEM(text));
                        break;
                case PROP_FILL_COLOR:
                        text->fill_color_rgba = ((color.red & 0xff00) << 16 |
                                        (color.green & 0xff00) << 8 |
                                        (color.blue & 0xff00) |
                                        0xff);
                        text->flags |= CR_TEXT_FILL_COLOR_RGBA;
                        cr_item_request_update(CR_ITEM(text));
                        break;
                case PROP_TEXT:
                        if (text->text) g_free(text->text);
                        text->text = g_value_dup_string (value);
                        cr_item_request_update(CR_ITEM(text));
                        break;
                case PROP_USE_MARKUP:
                        if (g_value_get_boolean(value)) 
                                text->flags |= CR_TEXT_USE_MARKUP;
                        else
                                text->flags &= ~CR_TEXT_USE_MARKUP;
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                property_id, pspec);
        }
}

static void
cr_text_get_property(GObject *object, guint property_id,
                GValue *value, GParamSpec *pspec)
{
        CrText *text = (CrText*) object;
        char *str;
        char color_string[8];

        switch (property_id) {
                case PROP_X_OFFSET:
                        g_value_set_double (value, text->x_offset);
                        break;
                case PROP_Y_OFFSET:
                        g_value_set_double (value, text->y_offset);
                        break;
                case PROP_WIDTH:
                        g_value_set_double (value, text->width);
                        break;
                case PROP_SCALEABLE:
                        g_value_set_boolean(value, 
                                        text->flags & CR_TEXT_SCALEABLE);
                        break;
                case PROP_FONT:
                        if (text->font_desc) {
                                str = pango_font_description_to_string(
                                                text->font_desc);
                                g_value_set_string (value, str);
                                g_free(str);
                        }
                        else
                                g_value_set_string(value, NULL);
                        break;
                case PROP_FILL_COLOR_RGBA:
                        g_value_set_uint(value, text->fill_color_rgba);
                        break;
                case PROP_FILL_COLOR:
                        sprintf(color_string, "#%.6x", 
                                        text->fill_color_rgba >> 8);
                        g_value_set_string(value, color_string);
                        break;
                case PROP_TEXT:
                        g_value_set_string (value, text->text);
                        break;
                case PROP_USE_MARKUP:
                        g_value_set_boolean(value, 
                                        text->flags & CR_TEXT_USE_MARKUP);
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                property_id, pspec);
        }
}

static void
create_layout(CrText *text, cairo_t *c)
{
        if (!text->layout)
                text->layout = pango_cairo_create_layout(c);

        if (text->font_desc)
                pango_layout_set_font_description(text->layout, 
                                text->font_desc);

        if (text->width > 0)
                pango_layout_set_width (text->layout, 
                                text->width * PANGO_SCALE);
        if (text->flags & CR_TEXT_USE_MARKUP)
                pango_layout_set_markup(text->layout, text->text, -1);
        else
                pango_layout_set_text(text->layout, text->text, -1);
}

static void
get_device_extents(CrText *text, cairo_t *c, CrDeviceBounds *device)
{
        int width, height;
        double w, h;

        cairo_save(c);
        cairo_identity_matrix(c);

        pango_cairo_update_layout(c, text->layout);

        pango_layout_get_size (text->layout, &width, &height);
        w = (double)width / PANGO_SCALE;
        h = (double)height / PANGO_SCALE;

        cairo_restore(c);


                        device->x1 = 0 + text->x_offset;
                        device->x2 = w + text->x_offset;

                        device->y1 = 0 + text->y_offset;
                        device->y2 = h + text->y_offset;

}

static void
get_item_extents(CrText *text, cairo_t *c, 
                double *x, double *y, double *w, double *h)
{
        int width, height;

        *x = *y = 0;

        pango_cairo_update_layout(c, text->layout);
        pango_layout_get_size (text->layout, &width, &height);
        *w = (double)width / PANGO_SCALE;
        *h = (double)height / PANGO_SCALE;
                        *y += *h;

}

static void
report_new_bounds(CrItem *item, cairo_t *ct, gboolean all)
{
        double w, h;
        CrText *text;

        text = CR_TEXT(item);

        /* Pango layout can change item coordinate size depending on the scale
         * and rotation of the items higher up in the tree.  This is because the
         * fonts do not scale linearly.  So each time the bounds get requested,
         * I re-calculate the bounds just in case the higher up scale has
         * changed.  This is inefficient, but the alternative is setting up
         * separate recalculate-bounds and update-item cycles.*/

        if (text->layout && item->bounds && (text->flags & CR_TEXT_SCALEABLE)) {
                get_item_extents(text, ct, 
                                &item->bounds->x1, &item->bounds->y2, 
                                &w, &h);
                item->bounds->x2 = item->bounds->x1 + w;
                item->bounds->y1 = item->bounds->y2 - h;
        }
        CR_ITEM_CLASS(parent_class)->report_new_bounds(item, ct, all);
}

static void
paint(CrItem *item, cairo_t *c)
{
        CrText *text;
        double r, g, b, a;
        double x, y;

        text = CR_TEXT(item);

        if (!text->text) return;

        if (text->flags & CR_TEXT_FILL_COLOR_RGBA) {
                r = ((text->fill_color_rgba & 0xff000000) >> 24) / 255.;
                g = ((text->fill_color_rgba & 0x00ff0000) >> 16) / 255.;
                b = ((text->fill_color_rgba & 0x0000ff00) >> 8) / 255.;
                a = (text->fill_color_rgba & 0xff) / 255.;
                if (a == 1)
                        cairo_set_source_rgb(c, r, g, b);
                else
                        cairo_set_source_rgba(c, r, g, b, a);
        }
        x = item->bounds->x1;
        y = item->bounds->y1;
        /*
        cairo_move_to(c, 0, 0);
        cairo_line_to(c, 0, -10);
        cairo_move_to(c, 0, 0);
        cairo_line_to(c, 10, 0);
        cairo_stroke(c);
        */

        if (!(text->flags & CR_TEXT_SCALEABLE)) {
                cairo_user_to_device(c, &x, &y);
                cairo_identity_matrix(c);
                cairo_move_to(c, x + item->device->x1, y + item->device->y1);
        }
        else
                cairo_move_to(c, x, y);

        pango_cairo_update_layout(c, text->layout);
        pango_cairo_show_layout(c, text->layout);
}

static gboolean
calculate_bounds(CrItem *item, cairo_t *c, CrBounds *bounds,
                CrDeviceBounds *device)
{
        CrText *text;
        double x, y, w, h;

        text = CR_TEXT(item);

        if (text->layout) 
                g_object_unref(text->layout);
        text->layout = NULL;

        if (text->text) {
                create_layout(text, c);


                if (text->flags & CR_TEXT_SCALEABLE) {
                        get_item_extents(text, c, &x, &y, &w, &h);
                        bounds->x1 = x;
                        bounds->y1 = y - h;
                        bounds->x2 = x + w;
                        bounds->y2 = y;
                }
                else {
                        get_device_extents(text, c, device);
                }

        }

        return TRUE;
}

static CrItem *
test(CrItem *item, cairo_t *c, double x, double y)
{
        /* anything in bounds will match. */
        return item;
}

static void
cr_text_init(CrText *text)
{
        text->flags |= CR_TEXT_SCALEABLE;
}

static void
cr_text_class_init(CrTextClass *klass)
{
        GObjectClass *object_class;
        CrItemClass *item_class;

        object_class = (GObjectClass *) klass;
        item_class = (CrItemClass *) klass;

        parent_class = g_type_class_peek_parent (klass);
        object_class->get_property = cr_text_get_property;
        object_class->set_property = cr_text_set_property;
        object_class->dispose = cr_text_dispose;
        object_class->finalize = cr_text_finalize;
        item_class->calculate_bounds = calculate_bounds;
        item_class->report_new_bounds = report_new_bounds;
        item_class->test = test;
        item_class->paint = paint;

        g_object_class_install_property (object_class, 
                        PROP_TEXT,
                        g_param_spec_string ("text", NULL, 
                                "The text to be drawn.",
                                NULL, G_PARAM_READWRITE));
        g_object_class_install_property (object_class, 
                        PROP_FONT,
                        g_param_spec_string ("font", "Font", 
                                "A pango font description string of the form "
                                "[FAMILY-LIST] [STYLE-OPTIONS] [SIZE]",
                                        NULL, G_PARAM_READWRITE));
        g_object_class_install_property
                (object_class,
                 PROP_WIDTH,
                 g_param_spec_double ("width", "Width", 
                        "Width before line-wrap",
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      G_PARAM_READWRITE));
        g_object_class_install_property
                (object_class,
                 PROP_SCALEABLE,
                 g_param_spec_boolean ("scaleable", NULL, "When True the size "
                         " of the text is in item units.  When False, the "
                         "size is in device/pixel units.  This effects "
                         "x/y_offset as well.  See also @CrInverse for "
                         "another way to achieve the same effect.",
				       TRUE,
				       G_PARAM_READWRITE));
        g_object_class_install_property
                (object_class,
                 PROP_X_OFFSET,
                 g_param_spec_double ("x_offset", NULL, "A device offset "
                         "from the item's anchor position.  Only used when "
                         "scaleable=FALSE.",
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      G_PARAM_READWRITE));
        g_object_class_install_property
                (object_class,
                 PROP_Y_OFFSET,
                 g_param_spec_double ("y_offset", NULL, "A device offset "
                         "from the item's anchor position.  Only used when "
                         "scaleable=FALSE.",
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      G_PARAM_READWRITE));

        g_object_class_install_property
                (object_class,
                 PROP_FILL_COLOR_RGBA,
                 g_param_spec_uint ("fill_color_rgba", NULL, 
                                    "Text color, red,grn,blue,alpha",
				    0, G_MAXUINT, 0,
				    G_PARAM_READWRITE));
        g_object_class_install_property
                (object_class,
                 PROP_FILL_COLOR,
                 g_param_spec_string ("fill_color", "Fill Color", 
                         "A string color such as 'red', or '#123456'",
                         NULL, G_PARAM_READWRITE));
        g_object_class_install_property
                (object_class,
                 PROP_USE_MARKUP,
                 g_param_spec_boolean ("use_markup", NULL, "If html style "
                         "markup language should be used.",
				       FALSE,
				       G_PARAM_READWRITE));
}

GType
cr_text_get_type(void)
{
        static GType type = 0;
        static const GTypeInfo info = {
                sizeof(CrTextClass),
                NULL, /*base_init*/
                NULL, /*base_finalize*/
                (GClassInitFunc) cr_text_class_init,
                (GClassFinalizeFunc) NULL,
                NULL,
                sizeof(CrText),
                0,
                (GInstanceInitFunc) cr_text_init,
                NULL
        };
        if (!type) {
                type = g_type_register_static(CR_TYPE_ITEM,
                        "CrText", &info, 0);
        }
        return type;
}

/**
 * cr_text_new:
 * @parent: The parent canvas item.
 * @x: X position of the text.
 * @y: Y position of the text.
 * @text: The text string.
 *
 * A convenience constructor for creating an text string and adding it to 
 * an item group in one step.
 *
 * Returns: A reference to a new CrItem.  You must call g_object_ref if you
 * intend to use this reference outside the local scope.
 */
CrItem *
cr_text_new(CrItem *parent, double x, double y,
                const char *text, const gchar *first_arg_name, ...)
{
        CrItem *item;
        va_list args;

        va_start (args, first_arg_name);
        item = cr_item_construct(parent, CR_TYPE_TEXT, first_arg_name, 
                        args);
        va_end (args);

        if (item) {
                g_object_set(item, "x", x, "y", y, "text", text, NULL);
        }

        return item;
}

