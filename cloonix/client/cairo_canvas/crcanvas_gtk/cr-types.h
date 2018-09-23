#ifndef _CR_TYPES_H_
#define _CR_TYPES_H_

#include <gtk/gtk.h>
#include <cairo.h>

G_BEGIN_DECLS

/* CrPoints */
#define CR_TYPE_POINTS (cr_points_get_type())
typedef struct _CrPoints CrPoints;

struct _CrPoints 
{
        GArray *array;
        int ref_count;
};
GType cr_points_get_type(void);
CrPoints *cr_points_ref(CrPoints *);
void cr_points_unref(CrPoints *);
CrPoints *cr_points_new(void);

/*CrBounds */
#define CR_TYPE_BOUNDS (cr_bounds_get_type())
typedef struct _CrBounds CrBounds;
struct _CrBounds
{
    double x1, y1, x2, y2;
    int ref_count;
};
GType cr_bounds_get_type(void);
CrBounds *cr_bounds_new(void);
void cr_bounds_unref(CrBounds *);
CrBounds *cr_bounds_ref(CrBounds *);

/* CrDeviceBounds */
#define CR_TYPE_DEVICE_BOUNDS (cr_device_bounds_get_type())
typedef struct _CrDeviceBounds CrDeviceBounds;

struct _CrDeviceBounds
{
        double x1, y1, x2, y2;
//        GtkAnchorType anchor;
        int ref_count;
};
GType cr_device_bounds_get_type(void);
CrDeviceBounds *cr_device_bounds_new(void);
void cr_device_bounds_unref(CrDeviceBounds *);
CrDeviceBounds *cr_device_bounds_ref(CrDeviceBounds *);

/* CrPattern */
#define CR_TYPE_PATTERN (cr_pattern_get_type())
GType cr_pattern_get_type(void);
typedef cairo_pattern_t CrPattern;

/* CrSurface */
#define CR_TYPE_SURFACE (cr_surface_get_type())
GType cr_surface_get_type(void);
typedef cairo_surface_t CrSurface;

/* CrMatrix */
/* box for test method passing cairo_matrix_t */
#define CR_TYPE_MATRIX (cr_matrix_get_type())
typedef cairo_matrix_t CrMatrix;
GType cr_matrix_get_type(void);

/* CrContext */
#define CR_TYPE_CONTEXT (cr_context_get_type())
typedef cairo_t CrContext;
GType cr_context_get_type(void);

/* CrDeviceBounds */
#define CR_TYPE_DASH (cr_dash_get_type())
typedef struct _CrDash CrDash;

/* Dashed Lines -- see Cairo ref manual */
struct _CrDash
{
        GArray *array;
        double offset;
        int ref_count;
};
GType cr_dash_get_type(void);
CrDash *cr_dash_new(void);
void cr_dash_unref(CrDash *);
CrDash *cr_dash_ref(CrDash *);

/*Enumeration Wrappers*/
#define CR_TYPE_CAP (cr_cap_get_type())
#define CR_TYPE_FILL_RULE (cr_fill_rule_get_type())
#define CR_TYPE_JOIN (cr_join_get_type())

GType cr_cap_get_type(void);
GType cr_fill_rule_get_type(void);
GType cr_join_get_type(void);

G_END_DECLS

#endif /* _CR_TYPES_H_ */
