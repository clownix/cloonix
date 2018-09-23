
#ifndef __cr_marshal_MARSHAL_H__
#define __cr_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:VOID (cr-marshal.list:1) */
#define cr_marshal_VOID__VOID	g_cclosure_marshal_VOID__VOID

/* VOID:POINTER (cr-marshal.list:2) */
#define cr_marshal_VOID__POINTER	g_cclosure_marshal_VOID__POINTER

/* VOID:DOUBLE,DOUBLE (cr-marshal.list:3) */
extern void cr_marshal_VOID__DOUBLE_DOUBLE (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);

/* VOID:BOXED,BOOLEAN (cr-marshal.list:4) */
extern void cr_marshal_VOID__BOXED_BOOLEAN (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);

/* OBJECT:BOXED,DOUBLE,DOUBLE (cr-marshal.list:5) */
extern void cr_marshal_OBJECT__BOXED_DOUBLE_DOUBLE (GClosure     *closure,
                                                    GValue       *return_value,
                                                    guint         n_param_values,
                                                    const GValue *param_values,
                                                    gpointer      invocation_hint,
                                                    gpointer      marshal_data);

/* BOOLEAN:BOXED,BOXED (cr-marshal.list:6) */
extern void cr_marshal_BOOLEAN__BOXED_BOXED (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* BOOLEAN:BOXED,BOXED,OBJECT (cr-marshal.list:7) */
extern void cr_marshal_BOOLEAN__BOXED_BOXED_OBJECT (GClosure     *closure,
                                                    GValue       *return_value,
                                                    guint         n_param_values,
                                                    const GValue *param_values,
                                                    gpointer      invocation_hint,
                                                    gpointer      marshal_data);

/* VOID:OBJECT,OBJECT (cr-marshal.list:8) */
extern void cr_marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);

/* BOOLEAN:BOXED,BOXED,BOXED (cr-marshal.list:9) */
extern void cr_marshal_BOOLEAN__BOXED_BOXED_BOXED (GClosure     *closure,
                                                   GValue       *return_value,
                                                   guint         n_param_values,
                                                   const GValue *param_values,
                                                   gpointer      invocation_hint,
                                                   gpointer      marshal_data);

/* BOOLEAN:BOXED (cr-marshal.list:10) */
extern void cr_marshal_BOOLEAN__BOXED (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data);

/* VOID:INT,DOUBLE,DOUBLE,DOUBLE,DOUBLE,BOXED (cr-marshal.list:11) */
extern void cr_marshal_VOID__INT_DOUBLE_DOUBLE_DOUBLE_DOUBLE_BOXED (GClosure     *closure,
                                                                    GValue       *return_value,
                                                                    guint         n_param_values,
                                                                    const GValue *param_values,
                                                                    gpointer      invocation_hint,
                                                                    gpointer      marshal_data);

/* BOOLEAN:DOUBLE,DOUBLE,DOUBLE,DOUBLE (cr-marshal.list:12) */
extern void cr_marshal_BOOLEAN__DOUBLE_DOUBLE_DOUBLE_DOUBLE (GClosure     *closure,
                                                             GValue       *return_value,
                                                             guint         n_param_values,
                                                             const GValue *param_values,
                                                             gpointer      invocation_hint,
                                                             gpointer      marshal_data);

G_END_DECLS

#endif /* __cr_marshal_MARSHAL_H__ */

