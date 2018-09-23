/* cr-text.h 
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
#ifndef _CR_TEXT_H_
#define _CR_TEXT_H_
 
#include <gtk/gtk.h>
#include <cr-item.h>

G_BEGIN_DECLS

#define CR_TYPE_TEXT  (cr_text_get_type())

#define CR_TEXT(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        CR_TYPE_TEXT, CrText))

#define CR_TEXT_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST ((klass), \
        CR_TYPE_TEXT, CrTextClass))

#define CR_IS_TEXT(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CR_TYPE_TEXT))

#define CR_IS_TEXT_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), CR_TYPE_TEXT))

#define CR_TEXT_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), \
        CR_TYPE_TEXT, CrTextClass))

typedef struct _CrText CrText;
typedef struct _CrTextClass CrTextClass;

struct _CrText
{
        CrItem parent;
        char *text;
        PangoFontDescription *font_desc;
        PangoLayout *layout;
        gint32 flags;
        double width, x_offset, y_offset;
//        GtkAnchorType anchor;
        guint fill_color_rgba;
};

struct _CrTextClass
{
        CrItemClass parent_class;
};

enum {
        CR_TEXT_SCALEABLE = 1 << 0,
        CR_TEXT_FILL_COLOR_RGBA = 1 << 1,
        CR_TEXT_USE_MARKUP = 1 << 2
};

GType cr_text_get_type(void);

CrItem *cr_text_new(CrItem *parent, double x, double y,
                const char *text, const gchar *first_arg_name, ...);


G_END_DECLS

#endif /* _CR_TEXT_H_ */
