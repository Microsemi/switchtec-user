AC_INIT(VERSION-GEN)

AC_PROG_CC
AC_CHECK_TOOL([WINDRES], [windres])

AC_ARG_WITH([curses],
             [AS_HELP_STRING([--with-curses],
	                   [support fancy gui @<:@default=check@:>@])],
             [],
	     [with_curses=check])

AC_ARG_WITH([crypto],
             [AS_HELP_STRING([--with-crypto],
	                   [support recovery commands @<:@default=check@:>@])],
             [],
	     [with_crypto=check])

AS_IF([test "x$with_curses" != xno],
       [AC_CHECK_LIB([ncurses], [initscr], [], [], [])])

AS_IF([test "x$with_curses" != xno &&
	   test "x$ac_cv_lib_ncurses_initscr" != xyes ],
       [AC_CHECK_LIB([tinfo], [tigetstr], [], [], [])
        AC_CHECK_LIB([curses], [initscr], [], [], [-ltinfo])])

AC_CHECK_HEADERS([curses.h ncurses/curses.h])

AS_IF([test "x$with_ssl" != xno],
       [AC_CHECK_LIB([crypto], [RSA_free], [], [], [])])

AC_CONFIG_FILES([Makefile])

AC_CONFIG_HEADER([config.h])

AC_OUTPUT()
