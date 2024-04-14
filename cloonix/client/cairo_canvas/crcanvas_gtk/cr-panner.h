/* cr-panner.h 
 * Copyright (C) 2006 Robert Gibbs <bgibbs@users.sourceforge.net> 
 */
#ifndef _CR_PANNER_H_
#define _CR_PANNER_H_
 
#include <gdk/gdk.h>
#include <cr-canvas.h>

G_BEGIN_DECLS

#define CR_TYPE_PANNER  (cr_panner_get_type())

#define CR_PANNER(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        CR_TYPE_PANNER, CrPanner))

#define CR_PANNER_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST ((klass), \
        CR_TYPE_PANNER, CrPannerClass))

#define CR_IS_PANNER(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CR_TYPE_PANNER))

#define CR_IS_PANNER_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), CR_TYPE_PANNER))

#define CR_PANNER_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), \
        CR_TYPE_PANNER, CrPannerClass))

typedef struct _CrPanner CrPanner;
typedef struct _CrPannerClass CrPannerClass;

struct _CrPanner
{
        GObject parent;
        CrCanvas *canvas;
        CrItem *item;
        GdkCursorType cursor;
        double last_msec, last_x, last_y;
        gint button;
        guint flags;
};

struct _CrPannerClass
{
        GObjectClass parent_class;
        void (*pan)(CrPanner *, double dx, double dy);
        void (*activate)(CrPanner *panner);
        void (*deactivate)(CrPanner *);
};

GType cr_panner_get_type(void);

void cr_panner_activate(CrPanner *panner);
void cr_panner_deactivate(CrPanner *panner);

CrPanner *cr_panner_new(CrCanvas *canvas, const gchar *first_arg_name, ...);

enum {
        CR_PANNER_DRAGGING = 1 << 0,
        CR_PANNER_ACTIVE = 1 << 1,
        CR_PANNER_ROOT_AVOID_TEST = 1 << 2
};


G_END_DECLS

#endif
