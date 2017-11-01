AC_INIT(VERSION-GEN)

AC_PROG_CC

AC_ARG_WITH([curses],
             [AS_HELP_STRING([--with-curses],
	                   [support fancy gui @<:@default=check@:>@])],
             [],
	     [with_curses=check])

AS_IF([test "x$with_curses" != xno],
       [AC_CHECK_LIB([ncurses], [initscr], [], [], [])])

AS_IF([test "x$with_curses" != xno &&
	   test "x$ac_cv_lib_ncurses_initscr" != xyes ],
       [AC_CHECK_LIB([tinfo], [tigetstr], [], [], [])
        AC_CHECK_LIB([curses], [initscr], [], [], [-ltinfo])])

AC_CONFIG_FILES([Makefile])

AC_CONFIG_HEADER([config.h])

AC_OUTPUT()
