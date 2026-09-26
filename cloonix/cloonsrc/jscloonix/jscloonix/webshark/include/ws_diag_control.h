/* ws_diag_control.h
 * Turn compiler diagnostic messages on and off.
 *
 * From FreeRADIUS build.h.
 *
 * @copyright 2013 The FreeRADIUS server project
 *
 * That project is covered by the GPLv2, so:
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __WS_DIAG_CONTROL_H__
#define __WS_DIAG_CONTROL_H__


#ifdef __cplusplus
extern "C" {
#endif

#define XSTRINGIFY(x) #x
#define DIAG_JOINSTR(x,y) XSTRINGIFY(x ## y)
#define DIAG_DO_PRAGMA(x) _Pragma (#x)

    #define DIAG_PRAGMA(x) DIAG_DO_PRAGMA(GCC diagnostic x)
    #define DIAG_OFF(x) DIAG_PRAGMA(push) DIAG_PRAGMA(ignored DIAG_JOINSTR(-W,x))
    #define DIAG_WARN(x) DIAG_PRAGMA(push) DIAG_PRAGMA(warning DIAG_JOINSTR(-W,x))
    #define DIAG_ON(x) DIAG_PRAGMA(pop)
    #define DIAG_OFF_PEDANTIC DIAG_OFF(pedantic)
    #define DIAG_ON_PEDANTIC DIAG_ON(pedantic)
    #define DIAG_OFF_INIT_TWICE DIAG_OFF(override-init)
    #define DIAG_ON_INIT_TWICE DIAG_ON(override-init)

#  define DIAG_OFF_CLANG(x)
#  define DIAG_ON_CLANG(x)

    #define DIAG_OFF_FLEX() \
      DIAG_OFF(sign-compare) \
      DIAG_OFF(unused-parameter)
    #define DIAG_ON_FLEX() \
      DIAG_ON(unused-parameter) \
      DIAG_ON(sign-compare)

  #define DIAG_OFF_LEMON() \
    DIAG_OFF_CLANG(unreachable-code)
  #define DIAG_ON_LEMON() \
    DIAG_ON_CLANG(unreachable-code)

#if defined(__GNUC__) || defined(__clang__)
  #define DIAG_OFF_CAST_AWAY_CONST DIAG_OFF(cast-qual)
  #define DIAG_ON_CAST_AWAY_CONST DIAG_OFF(cast-qual)
#endif

  #define DIAG_OFF_STRINGOP_OVERFLOW()
  #define DIAG_ON_STRINGOP_OVERFLOW()

#  define USES_APPLE_DEPRECATED_API
#  define USES_APPLE_RST

#ifdef __cplusplus
}
#endif
#endif /* __WS_DIAG_CONTROL_H__ */
