#include <glib-object.h>

#define g_marshal_value_peek_boolean(v)  (v)->data[0].v_int
#define g_marshal_value_peek_char(v)     (v)->data[0].v_int
#define g_marshal_value_peek_uchar(v)    (v)->data[0].v_uint
#define g_marshal_value_peek_int(v)      (v)->data[0].v_int
#define g_marshal_value_peek_uint(v)     (v)->data[0].v_uint
#define g_marshal_value_peek_long(v)     (v)->data[0].v_long
#define g_marshal_value_peek_ulong(v)    (v)->data[0].v_ulong
#define g_marshal_value_peek_int64(v)    (v)->data[0].v_int64
#define g_marshal_value_peek_uint64(v)   (v)->data[0].v_uint64
#define g_marshal_value_peek_enum(v)     (v)->data[0].v_long
#define g_marshal_value_peek_flags(v)    (v)->data[0].v_ulong
#define g_marshal_value_peek_float(v)    (v)->data[0].v_float
#define g_marshal_value_peek_double(v)   (v)->data[0].v_double
#define g_marshal_value_peek_string(v)   (v)->data[0].v_pointer
#define g_marshal_value_peek_param(v)    (v)->data[0].v_pointer
#define g_marshal_value_peek_boxed(v)    (v)->data[0].v_pointer
#define g_marshal_value_peek_pointer(v)  (v)->data[0].v_pointer
#define g_marshal_value_peek_object(v)   (v)->data[0].v_pointer


/****************************************************************************/
void cr_marshal_VOID__DOUBLE_DOUBLE(GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__DOUBLE_DOUBLE)(gpointer data1,
          gdouble arg_1, gdouble arg_2, gpointer data2);
  register GMarshalFunc_VOID__DOUBLE_DOUBLE callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  g_return_if_fail (n_param_values == 3);
  if (G_CCLOSURE_SWAP_DATA (closure))
    {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__DOUBLE_DOUBLE) (marshal_data ? marshal_data : cc->callback);
  callback (data1, g_marshal_value_peek_double (param_values + 1),
            g_marshal_value_peek_double (param_values + 2), data2);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_marshal_VOID__BOXED_BOOLEAN(GClosure *closure, GValue *return_value G_GNUC_UNUSED,
                                    guint n_param_values, const GValue *param_values,
                                    gpointer invocation_hint G_GNUC_UNUSED,
                                    gpointer marshal_data)
{
  typedef void (*GMarshalFunc_VOID__BOXED_BOOLEAN)(gpointer data1,
               gpointer arg_1, gboolean arg_2, gpointer data2);
  register GMarshalFunc_VOID__BOXED_BOOLEAN callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  g_return_if_fail (n_param_values == 3);
  if (G_CCLOSURE_SWAP_DATA (closure))
    {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__BOXED_BOOLEAN) (marshal_data ? marshal_data : cc->callback);
  callback (data1, g_marshal_value_peek_boxed (param_values + 1),
            g_marshal_value_peek_boolean (param_values + 2), data2);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_marshal_OBJECT__BOXED_DOUBLE_DOUBLE (GClosure *closure, GValue *return_value G_GNUC_UNUSED,
                                            guint n_param_values, const GValue *param_values,
                                            gpointer invocation_hint G_GNUC_UNUSED, gpointer marshal_data)
{
  typedef GObject* (*GMarshalFunc_OBJECT__BOXED_DOUBLE_DOUBLE) (gpointer data1,
                    gpointer arg_1, gdouble arg_2, gdouble arg_3, gpointer data2);
  register GMarshalFunc_OBJECT__BOXED_DOUBLE_DOUBLE callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  GObject* v_return;
  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 4);
  if (G_CCLOSURE_SWAP_DATA (closure))
    {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
    }
  callback = (GMarshalFunc_OBJECT__BOXED_DOUBLE_DOUBLE) (marshal_data ? marshal_data : cc->callback);
  v_return = callback (data1, g_marshal_value_peek_boxed (param_values + 1),
                       g_marshal_value_peek_double (param_values + 2),
                       g_marshal_value_peek_double (param_values + 3), data2);
  g_value_take_object (return_value, v_return);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_marshal_BOOLEAN__BOXED_BOXED (GClosure *closure, GValue *return_value G_GNUC_UNUSED,
                                      guint n_param_values, const GValue *param_values,
                                      gpointer invocation_hint G_GNUC_UNUSED, gpointer marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__BOXED_BOXED) (gpointer data1,
                    gpointer arg_1, gpointer arg_2, gpointer data2);
  register GMarshalFunc_BOOLEAN__BOXED_BOXED callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gboolean v_return;
  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 3);
  if (G_CCLOSURE_SWAP_DATA (closure))
    {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__BOXED_BOXED) (marshal_data ? marshal_data : cc->callback);
  v_return = callback (data1, g_marshal_value_peek_boxed (param_values + 1),
                       g_marshal_value_peek_boxed (param_values + 2), data2);
  g_value_set_boolean (return_value, v_return);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_marshal_BOOLEAN__BOXED_BOXED_OBJECT(GClosure *closure, GValue *return_value G_GNUC_UNUSED,
                                            guint n_param_values, const GValue *param_values,
                                            gpointer invocation_hint G_GNUC_UNUSED, gpointer marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__BOXED_BOXED_OBJECT)(gpointer data1,
                    gpointer arg_1, gpointer arg_2, gpointer arg_3, gpointer data2);
  register GMarshalFunc_BOOLEAN__BOXED_BOXED_OBJECT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gboolean v_return;
  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 4);
  if (G_CCLOSURE_SWAP_DATA (closure))
    {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__BOXED_BOXED_OBJECT) (marshal_data ? marshal_data : cc->callback);
  v_return = callback (data1, g_marshal_value_peek_boxed (param_values + 1),
                       g_marshal_value_peek_boxed (param_values + 2),
                       g_marshal_value_peek_object (param_values + 3), data2);
  g_value_set_boolean (return_value, v_return);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_marshal_VOID__OBJECT_OBJECT(GClosure *closure, GValue *return_value G_GNUC_UNUSED,
     guint n_param_values, const GValue *param_values, gpointer invocation_hint G_GNUC_UNUSED, gpointer marshal_data)
{
  typedef void (*GMarshalFunc_VOID__OBJECT_OBJECT) (gpointer data1, gpointer arg_1, gpointer arg_2, gpointer data2);
  register GMarshalFunc_VOID__OBJECT_OBJECT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  g_return_if_fail (n_param_values == 3);
  if (G_CCLOSURE_SWAP_DATA (closure))
    {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__OBJECT_OBJECT) (marshal_data ? marshal_data : cc->callback);
  callback (data1, g_marshal_value_peek_object (param_values + 1),
            g_marshal_value_peek_object (param_values + 2), data2);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_marshal_BOOLEAN__BOXED_BOXED_BOXED (GClosure *closure, GValue *return_value G_GNUC_UNUSED,
     guint n_param_values, const GValue *param_values, gpointer invocation_hint G_GNUC_UNUSED, gpointer marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__BOXED_BOXED_BOXED)(gpointer data1, gpointer arg_1, gpointer arg_2,
                                                              gpointer arg_3, gpointer data2);
  register GMarshalFunc_BOOLEAN__BOXED_BOXED_BOXED callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gboolean v_return;
  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 4);
  if (G_CCLOSURE_SWAP_DATA (closure))
    {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__BOXED_BOXED_BOXED) (marshal_data ? marshal_data : cc->callback);
  v_return = callback (data1, g_marshal_value_peek_boxed (param_values + 1),
                       g_marshal_value_peek_boxed (param_values + 2),
                       g_marshal_value_peek_boxed (param_values + 3), data2);
  g_value_set_boolean (return_value, v_return);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_marshal_BOOLEAN__BOXED(GClosure *closure, GValue *return_value G_GNUC_UNUSED,
                               guint n_param_values, const GValue *param_values,
                               gpointer invocation_hint G_GNUC_UNUSED, gpointer marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__BOXED)(gpointer data1, gpointer arg_1, gpointer data2);
  register GMarshalFunc_BOOLEAN__BOXED callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gboolean v_return;
  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 2);
  if (G_CCLOSURE_SWAP_DATA (closure))
    {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__BOXED) (marshal_data ? marshal_data : cc->callback);
  v_return = callback (data1, g_marshal_value_peek_boxed (param_values + 1), data2);
  g_value_set_boolean (return_value, v_return);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_marshal_VOID__INT_DOUBLE_DOUBLE_DOUBLE_DOUBLE_BOXED(GClosure *closure, GValue *return_value G_GNUC_UNUSED,
                                                            guint n_param_values, const GValue *param_values,
                                                            gpointer invocation_hint G_GNUC_UNUSED, gpointer marshal_data)
{
  typedef void (*GMarshalFunc_VOID__INT_DOUBLE_DOUBLE_DOUBLE_DOUBLE_BOXED)(gpointer data1, gint arg_1, gdouble arg_2,
                                                                           gdouble arg_3, gdouble arg_4, gdouble arg_5,
                                                                           gpointer arg_6, gpointer data2);
  register GMarshalFunc_VOID__INT_DOUBLE_DOUBLE_DOUBLE_DOUBLE_BOXED callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  g_return_if_fail (n_param_values == 7);
  if (G_CCLOSURE_SWAP_DATA (closure))
    {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__INT_DOUBLE_DOUBLE_DOUBLE_DOUBLE_BOXED) (marshal_data ? marshal_data : cc->callback);
  callback(data1, g_marshal_value_peek_int (param_values + 1), g_marshal_value_peek_double (param_values + 2),
           g_marshal_value_peek_double (param_values + 3), g_marshal_value_peek_double (param_values + 4),
           g_marshal_value_peek_double (param_values + 5), g_marshal_value_peek_boxed (param_values + 6), data2);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cr_marshal_BOOLEAN__DOUBLE_DOUBLE_DOUBLE_DOUBLE(GClosure *closure, GValue *return_value G_GNUC_UNUSED,
                                                     guint n_param_values, const GValue *param_values,
                                                     gpointer invocation_hint G_GNUC_UNUSED, gpointer marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__DOUBLE_DOUBLE_DOUBLE_DOUBLE)(gpointer data1, gdouble arg_1, gdouble arg_2,
                                                                        gdouble arg_3, gdouble arg_4, gpointer data2);
  register GMarshalFunc_BOOLEAN__DOUBLE_DOUBLE_DOUBLE_DOUBLE callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gboolean v_return;
  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 5);
  if (G_CCLOSURE_SWAP_DATA (closure))
    {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__DOUBLE_DOUBLE_DOUBLE_DOUBLE) (marshal_data ? marshal_data : cc->callback);
  v_return = callback (data1, g_marshal_value_peek_double (param_values + 1),
                       g_marshal_value_peek_double (param_values + 2),
                       g_marshal_value_peek_double (param_values + 3),
                       g_marshal_value_peek_double (param_values + 4), data2);
  g_value_set_boolean (return_value, v_return);
}
/*--------------------------------------------------------------------------*/

