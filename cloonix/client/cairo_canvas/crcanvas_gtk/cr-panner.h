/* cr-panner.h 
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
        /**
         * CrPanner::pan:
         * @panner:
         * @dx: Device units moved in the X direction.
         * @dy: Device units moved in the Y direction.
         *
         * This signal is emitted each time the canvas is panned.
         */
        void (*pan)(CrPanner *, double dx, double dy);
        /**
         * CrPanner::activate:
         * @panner:
         *
         * This signal is emitted whenever the panner is first activated.
         */
        void (*activate)(CrPanner *panner);
        /**
         * CrPanner::deactivate:
         * @panner:
         *
         * This signal is emitted whenever the panner is deactivated. 
         * It can be used to get to a callback from a panning selection.
         */
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

#endif /* _CR_PANNER_H_ */
