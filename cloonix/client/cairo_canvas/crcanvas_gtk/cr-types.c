#include "cr-types.h"

/**
 * SECTION:cr-types
 * @title: cr-types
 * @short_description: #Gobject Boxed wrappers for passing data in signals and
 * language wrapping.
 *
 * 
 */

CrPoints *
cr_points_ref(CrPoints *points)
{
        points->ref_count++;
        return points;
}
void
cr_points_unref(CrPoints *points)
{
        points->ref_count--;
        if (points->ref_count <= 0) {
                if (points->array) g_array_free(points->array, TRUE);
                g_free(points);
        }
}
CrPoints *
cr_points_new(void)
{
        CrPoints *points;

        points = g_new(CrPoints, 1);
        points->array = g_array_new(0, 0, sizeof(double));
        points->ref_count = 1;
        return points;
}

GType
cr_points_get_type(void)
{
        static GType type = 0;

        if (!type)
                type = g_boxed_type_register_static ("CrPoints",
                                (GBoxedCopyFunc)cr_points_ref, 
                                (GBoxedFreeFunc)cr_points_unref);
        return type;
}

CrBounds *
cr_bounds_ref(CrBounds *bounds)
{
        bounds->ref_count++;
        return bounds;
}
void
cr_bounds_unref(CrBounds *bounds)
{
        bounds->ref_count--;
        if (bounds->ref_count <= 0) {
                g_free(bounds);
        }
}
CrBounds *
cr_bounds_new(void)
{
        CrBounds *bounds;

        bounds = g_new0(CrBounds, 1);
        bounds->ref_count = 1;
        return bounds;
}
GType
cr_bounds_get_type(void)
{
        static GType type = 0;

        if (!type)
                type = g_boxed_type_register_static ("CrBounds",
                                (GBoxedCopyFunc)cr_bounds_ref, 
                                (GBoxedFreeFunc)cr_bounds_unref);
        return type;
}

CrDeviceBounds *
cr_device_bounds_ref(CrDeviceBounds *device)
{
        device->ref_count++;
        return device;
}
void
cr_device_bounds_unref(CrDeviceBounds *device)
{
        device->ref_count--;
        if (device->ref_count <= 0) {
                g_free(device);
        }
}
CrDeviceBounds *
cr_device_bounds_new(void)
{
        CrDeviceBounds *device;

        device = g_new0(CrDeviceBounds, 1);
//        device->anchor = GTK_ANCHOR_NW;
        device->ref_count = 1;
        return device;
}

GType
cr_device_bounds_get_type(void)
{
        static GType type = 0;

        if (!type)
                type = g_boxed_type_register_static ("CrDeviceBounds",
                                (GBoxedCopyFunc)cr_device_bounds_ref, 
                                (GBoxedFreeFunc)cr_device_bounds_unref);
        return type;
}

CrDash *
cr_dash_ref(CrDash *dash)
{
        dash->ref_count++;
        return dash;
}
void
cr_dash_unref(CrDash *dash)
{
        dash->ref_count--;
        if (dash->ref_count <= 0) {
                if (dash->array) g_array_free(dash->array, TRUE);
                g_free(dash);
        }
}
CrDash *
cr_dash_new(void)
{
        CrDash *dash;

        dash = g_new0(CrDash, 1);
        dash->array = g_array_new(0, 0, sizeof(double));
        dash->ref_count = 1;
        return dash;
}

GType
cr_dash_get_type(void)
{
        static GType type = 0;

        if (!type)
                type = g_boxed_type_register_static ("CrDash",
                                (GBoxedCopyFunc)cr_dash_ref, 
                                (GBoxedFreeFunc)cr_dash_unref);
        return type;
}

GType
cr_pattern_get_type(void)
{
        static GType type = 0;

        if (!type)
                type = g_boxed_type_register_static ("CrPattern",
                                (GBoxedCopyFunc)cairo_pattern_reference, 
                                (GBoxedFreeFunc)cairo_pattern_destroy);
        return type;
}

GType
cr_surface_get_type(void)
{
        static GType type = 0;

        if (!type)
                type = g_boxed_type_register_static ("CrSurface",
                                (GBoxedCopyFunc)cairo_surface_reference, 
                                (GBoxedFreeFunc)cairo_surface_destroy);
        return type;
}

static gpointer
cr_matrix_copy(gpointer p)
{
        CrMatrix *newp = g_new(CrMatrix, 1);
        *newp = *((CrMatrix*) p);

        return newp;
}

GType
cr_matrix_get_type(void)
{
        static GType type = 0;

        if (!type)
                type = g_boxed_type_register_static ("CrMatrix",
                                cr_matrix_copy, g_free);
        return type;
}

GType
cr_context_get_type(void)
{
        static GType type = 0;

        if (!type)
                type = g_boxed_type_register_static ("CrContext",
                                (GBoxedCopyFunc)cairo_reference, 
                                (GBoxedFreeFunc)cairo_destroy);
        return type;
}

GType
cr_cap_get_type(void)
{
        static GType type = 0;
        static const GEnumValue values[] = {
                { CAIRO_LINE_CAP_BUTT, "CAIRO_LINE_CAP_BUTT", "butt" },
                { CAIRO_LINE_CAP_ROUND, "CAIRO_LINE_CAP_ROUND", "round" },
                { CAIRO_LINE_CAP_SQUARE, "CAIRO_LINE_CAP_SQUARE", "square" },
                { 0, NULL, NULL }
        };

        if (!type)
                type = g_enum_register_static("CrCap", values);
        return type;
}

GType
cr_join_get_type(void)
{
        static GType type = 0;
        static const GEnumValue values[] = {
                { CAIRO_LINE_JOIN_MITER, "CAIRO_LINE_JOIN_MITER", "miter" },
                { CAIRO_LINE_JOIN_ROUND, "CAIRO_LINE_JOIN_ROUND", "round" },
                { CAIRO_LINE_JOIN_BEVEL, "CAIRO_LINE_JOIN_BEVEL", "bevel" },
                { 0, NULL, NULL }
        };

        if (!type)
                type = g_enum_register_static("CrJoin", values);
        return type;
}
GType
cr_fill_rule_get_type(void)
{
        static GType type = 0;
        static const GEnumValue values[] = {
                { CAIRO_FILL_RULE_WINDING, "CAIRO_FILL_RULE_WINDING", 
                        "winding" },
                { CAIRO_FILL_RULE_EVEN_ODD, "CAIRO_FILL_RULE_EVEN_ODD", 
                        "even-odd" },
                { 0, NULL, NULL }
        };

        if (!type)
                type = g_enum_register_static("CrFillRule", values);
        return type;
}
