AC_INIT
AC_CONFIG_SRCDIR([VERSION-GEN])

AC_PROG_CC
AC_CHECK_TOOL([WINDRES], [windres])

AC_ARG_WITH([curses],
             [AS_HELP_STRING([--with-curses],
	                   [support fancy gui @<:@default=check@:>@])],
             [],
	     [with_curses=check])

AC_ARG_WITH([openssl],
             [AS_HELP_STRING([--with-openssl],
	                   [support OpenSSL library @<:@default=check@:>@])],
             [],
	     [with_openssl=check])

AS_IF([test "x$with_curses" != xno],
       [AC_CHECK_LIB([ncurses], [initscr], [], [], [])])

AS_IF([test "x$with_curses" != xno &&
	   test "x$ac_cv_lib_ncurses_initscr" != xyes ],
       [AC_CHECK_LIB([tinfo], [tigetstr], [], [], [])
        AC_CHECK_LIB([curses], [initscr], [], [], [-ltinfo])])

AC_CHECK_HEADERS([curses.h ncurses/curses.h])

AS_IF([test "x$with_openssl" != xno],
       [AC_CHECK_HEADERS([openssl/pem.h])
        AC_CHECK_LIB([crypto], [RSA_free], [], [], [])
        AC_CHECK_DECLS([RSA_get0_key], [], [], [#include <openssl/pem.h>])])

AC_CONFIG_FILES([Makefile])

AC_CONFIG_HEADERS([config.h])

AC_OUTPUT
