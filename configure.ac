AC_INIT([zim-text-extractor], [1.0.0])

AC_CONFIG_SRCDIR([src/main.cpp])

AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

AC_PROG_CXX

dnl PKG_CHECK_MODULES([XML], [libxml-2.0 >= 2])
PKG_CHECK_MODULES([TIDY], [tidy >= 5])
PKG_CHECK_MODULES([ZIM], [libzim >= 7])

LIBS="$TIDY_LIBS $ZIM_LIBS"
CFLAGS="$TIDY_CFLAGS $ZIM_CFLAGS"

AC_SUBST(LIBS)
AC_SUBST(CFLAGS)

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
